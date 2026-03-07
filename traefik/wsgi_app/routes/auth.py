#!/usr/bin/env python3
#
#  Copyright (c) 2026, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#
"""Route handlers for /api/auth/* endpoints"""

import json
import logging
import re
import secrets
import shutil
import string
from flask import Blueprint, request, g, make_response

from ..config import (
    HTPASSWD_FILE, CLIENT_AUTHZ_FILE, CLIENT_CERTS_DIR, CA_BUNDLE,
    PASSWORD_MIN_LENGTH, PASSWORD_MAX_LENGTH,
    PASSWORD_REQUIRE_DIGIT, PASSWORD_REQUIRE_LOWERCASE,
    PASSWORD_REQUIRE_UPPERCASE, PASSWORD_REQUIRE_SPECIAL,
    PASSWORD_MIN_CHAR_TYPES,
    LDEVID_CERT, LDEVID_KEY, LDEVID_CHAIN_UPLOADED, LDEVID_KEY_UPLOADED,
    IDEVID_CERT, IDEVID_KEY, TRAEFIK_DIR
)
from ..utils.responses import (
    send_json, send_json_api, send_text, send_pem, send_pkcs7,
    send_no_content, send_created, error_response
)
from ..utils.system_utils import restart_traefik, switch_config_files, should_switch_to_mtls
from ..utils.content_negotiation import (
    prefers_json_api, validate_content_type, get_preferred_response_format
)
from ..utils.cert_utils import (
    calculate_cert_id, extract_cert_info, pem_to_pkcs7, validate_cert_chain,
    split_cert_and_key, extract_ca_certs_to_bundle, validate_url,
    fetch_est_cacerts, submit_csr_to_est, generate_csr_for_est
)
from passlib.hash import apr_md5_crypt
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.x509.oid import ExtensionOID

logger = logging.getLogger(__name__)

auth_bp = Blueprint('auth', __name__)


def generate_secure_password():
    """
    Generate a secure random password according to password policy:
    - Length: 16-24 bytes
    - At least 3 of 4 character types:
      * Uppercase alphabetic
      * Lowercase alphabetic
      * Digit
      * Non-alphanumeric (special characters)
    - No control characters (< 0x20)
    
    Returns:
        str: Generated password meeting policy requirements
    """
    # Character pools
    uppercase = string.ascii_uppercase
    lowercase = string.ascii_lowercase
    digits = string.digits
    special = '!@#$%^&*()_+-=[]{}|;:,.<>?'
    
    # Random length between 16-24
    length = 16 + secrets.randbelow(9)
    
    # Ensure at least 3 character types (meet policy requirement)
    # Select 3 or 4 character pools
    pools_to_use = secrets.randbelow(2) + 3  # 3 or 4 pools
    
    # Guarantee at least one char from first 3 pools, and maybe special
    password_chars = []
    password_chars.append(secrets.choice(uppercase))
    password_chars.append(secrets.choice(lowercase))
    password_chars.append(secrets.choice(digits))
    
    if pools_to_use == 4:
        password_chars.append(secrets.choice(special))
        combined_pool = uppercase + lowercase + digits + special
    else:
        combined_pool = uppercase + lowercase + digits
    
    # Fill remaining length
    for _ in range(length - len(password_chars)):
        password_chars.append(secrets.choice(combined_pool))
    
    # Shuffle to randomize position of required characters
    secrets.SystemRandom().shuffle(password_chars)
    
    return ''.join(password_chars)


def validate_password_strength(password):
    """
    Validate password meets policy requirements
    
    Returns (is_valid, error_message) tuple
    """
    if len(password) < PASSWORD_MIN_LENGTH:
        return False, f"Password must be at least {PASSWORD_MIN_LENGTH} characters"
    
    if len(password) > PASSWORD_MAX_LENGTH:
        return False, f"Password must not exceed {PASSWORD_MAX_LENGTH} characters"
    
    checks_passed = 0
    checks_required = PASSWORD_MIN_CHAR_TYPES
    
    if PASSWORD_REQUIRE_LOWERCASE and re.search(r'[a-z]', password):
        checks_passed += 1
    
    if PASSWORD_REQUIRE_UPPERCASE and re.search(r'[A-Z]', password):
        checks_passed += 1
    
    if PASSWORD_REQUIRE_DIGIT and re.search(r'[0-9]', password):
        checks_passed += 1
    
    if PASSWORD_REQUIRE_SPECIAL and re.search(r'[^a-zA-Z0-9]', password):
        checks_passed += 1
    
    if checks_passed < checks_required:
        return False, (
            f"Password must contain at least {checks_required} of: "
            "lowercase, uppercase, digit, special character"
        )
    
    return True, None


@auth_bp.route('/internal/forwardauth', methods=['GET', 'POST'])
def forwardauth():
    """
    Traefik forwardAuth endpoint for client certificate validation
    
    Called by Traefik's forwardAuth middleware to validate client certificates
    before forwarding requests to backend services (e.g., otbr-agent, otbr-web).
    
    Validates the client certificate from X-Forwarded-Tls-Client-Cert-Info header
    against the authorization database.
    
    On success, returns auth headers that Traefik forwards to backend services:
        - X-Auth-Role: Client's role (admin or user)
        - X-Auth-Scopes: Client's scopes (read,write or read)
    
    Backend services can use these headers to adapt responses based on
    authorization level (e.g., redact sensitive data for non-admin users).
    
    Returns:
        200 OK + auth headers: Client certificate is authorized
        403 Forbidden: Client certificate not authorized or missing
    """
    # This endpoint is accessed directly by Traefik, not through the WSGI middleware
    # So we need to validate the certificate here
    from ..middleware.auth import normalize_dn
    
    # Extract client certificate info from Traefik header
    cert_info_header = request.headers.get('X-Forwarded-Tls-Client-Cert-Info', '')
    
    if not cert_info_header:
        logger.warning("ForwardAuth: No client certificate provided")
        return error_response(403, "Forbidden", "Client certificate required")
    
    # Parse certificate info
    try:
        from urllib.parse import unquote_plus
        cert_info = unquote_plus(cert_info_header)
        
        # Extract issuer and SANs
        issuer_match = re.search(r'Issuer="([^"]+)"', cert_info)
        san_match = re.search(r'SAN="([^"]+)"', cert_info)
        
        if not issuer_match or not san_match:
            logger.warning(f"ForwardAuth: Invalid cert info format: {cert_info}")
            return error_response(403, "Forbidden", "Invalid certificate information")
        
        cert_issuer = issuer_match.group(1)
        cert_sans = [s.strip() for s in san_match.group(1).split(',')]
        
        logger.debug(f"ForwardAuth: Validating cert - Issuer: {cert_issuer}, SANs: {cert_sans}")
        
    except Exception as e:
        logger.error(f"ForwardAuth: Failed to parse cert info: {e}")
        return error_response(403, "Forbidden", "Certificate parsing error")
    
    # Load authorization database
    try:
        with open(CLIENT_AUTHZ_FILE, 'r') as f:
            authz_data = json.load(f)
        clients_db = authz_data.get('clients', {})
    except Exception as e:
        logger.error(f"ForwardAuth: Failed to load authz database: {e}")
        return error_response(403, "Forbidden", "Authorization system error")
    
    # Find matching authorized client
    cert_issuer_normalized = normalize_dn(cert_issuer)
    
    for client_id, client_data in clients_db.items():
        authz_issuer = client_data.get('issuer', '')
        if normalize_dn(authz_issuer) != cert_issuer_normalized:
            continue
        
        # Check SAN match
        authz_sans = client_data.get('sans', [])
        
        if any(san in authz_sans for san in cert_sans):
            # Client authorized - return 200 with auth headers for Traefik to forward
            role = client_data.get('role', 'user')
            scopes = client_data.get('scopes', [])
            
            logger.info(f"ForwardAuth: Client authorized - ID: {client_id}, Role: {role}")
            
            # Create response with auth headers that Traefik will forward to backend
            response = make_response('OK', 200)
            response.headers['X-Auth-Role'] = role
            response.headers['X-Auth-Scopes'] = ','.join(scopes)
            
            return response
    
    # Not authorized
    logger.warning(f"ForwardAuth: Certificate not authorized - Issuer: {cert_issuer}, SANs: {cert_sans}")
    return error_response(403, "Forbidden", "Certificate not authorized")


@auth_bp.route('/api/auth/password', methods=['POST', 'PUT', 'DELETE'])
def handle_password():
    """
    Manage HTTP basic authentication password
    
    POST: Generate password proposal (secure random password)
    PUT: Set or update password
    DELETE: Remove password
    """
    if request.method == 'POST':
        # Generate password proposal
        generated_password = generate_secure_password()
        logger.debug(f"Generated password length={len(generated_password)}")
        
        # Determine response format based on Accept header
        accept_header = request.headers.get('Accept', 'text/plain')
        
        if 'application/json' in accept_header:
            return send_json({"password": generated_password})
        else:
            return send_text(generated_password)
    
    elif request.method == 'PUT':
        # Support both JSON and plain text password inputs
        if request.content_type and 'application/json' in request.content_type:
            try:
                data = request.get_json()
                password = data.get('password', '').strip()
            except Exception:
                return error_response(400, "Bad Request", "Invalid JSON")
        else:
            password = request.data.decode('utf-8').strip()
        
        if not password:
            return error_response(400, "Bad Request",
                                "Password cannot be empty")
        
        # Validate password strength
        is_valid, error_msg = validate_password_strength(password)
        if not is_valid:
            return error_response(422, "Unprocessable Content", error_msg)
    
        try:
            # Use passlib to generate APR1 (MD5) hash for htpasswd compatibility
            HTPASSWD_FILE.parent.mkdir(parents=True, exist_ok=True)
            
            # Generate hash using passlib APR1 MD5
            password_hash = apr_md5_crypt.hash(password)
            
            # Write htpasswd file in format: username:hash
            with open(HTPASSWD_FILE, 'w') as f:
                f.write(f'admin:{password_hash}\n')
            
            HTPASSWD_FILE.chmod(0o600)
            logger.info("Password set successfully using passlib APR1 MD5")
            
            # Trigger Traefik restart in detached background process
            if restart_traefik():
                logger.info("Traefik restart triggered after password change")
            else:
                logger.warning("Traefik restart trigger failed after password change")
            
            return send_no_content()
            
        except Exception as e:
            logger.error(f"Password set failed: {e}")
            return error_response(500, "Internal Server Error",
                                "Failed to set password")
    
    else:  # DELETE
        """
        Delete password and auto-switch to mTLS mode if admin client certificates exist.
        
        According to OpenAPI spec, requires at least one other authentication method
        (mutual certificate authentication) to be configured; otherwise returns 409 Conflict.
        
        When the admin password is deleted:
        1. Check if admin client certificates are registered
        2. If no, return 409 Conflict (no authentication method would remain)
        3. If yes, remove htpasswd file and switch to mTLS mode (similar to enrollment push mode)
        4. Restart Traefik with mTLS configuration
        """
        try:
            # First check if admin client certificates exist before allowing deletion
            # This prevents locking out all access if password is the only auth method
            has_admin_cert = False
            if CLIENT_AUTHZ_FILE.exists():
                try:
                    with open(CLIENT_AUTHZ_FILE, 'r') as f:
                        authz = json.load(f)
                    
                    clients = authz.get('clients', {})
                    for client_id, client_info in clients.items():
                        role = client_info.get('role', '')
                        sans = client_info.get('sans', [])
                        
                        if role == 'admin' and isinstance(sans, list) and len(sans) > 0:
                            has_admin_cert = True
                            logger.debug(f"Found admin client certificate: {client_id[:16]}...")
                            break
                except Exception as e:
                    logger.warning(f"Failed to check client certificates: {e}")
            
            # Enforce requirement: at least one authentication method must remain
            if not has_admin_cert:
                logger.warning("Password deletion rejected - no admin client certificates configured")
                return error_response(409, "Conflict",
                                    "Cannot delete password: no other authentication method (mTLS) is configured. "
                                    "Upload and enroll an admin client certificate first.")
            
            # Proceed with deletion since we have admin client certificates
            password_existed = HTPASSWD_FILE.exists()
            
            if password_existed:
                HTPASSWD_FILE.unlink()
                logger.info("Password deleted - switching to mTLS mode")
                
                # Switch to mTLS mode (we already verified admin client exists)
                # This will set mtls_required=true in client-authz.json
                switch_to_mtls = should_switch_to_mtls(CLIENT_AUTHZ_FILE)
                
                if not switch_to_mtls:
                    # This shouldn't happen since we checked above, but handle it defensively
                    logger.error("Unexpected: should_switch_to_mtls returned False after verification")
                    return error_response(500, "Internal Server Error",
                                        "Inconsistent authentication state detected")
                
                # Switch configuration files to mTLS
                try:
                    switch_config_files('mtls', TRAEFIK_DIR)
                    logger.info("Switched to mTLS configuration")
                except Exception as e:
                    logger.error(f"Failed to switch configuration files: {e}")
                    return error_response(500, "Internal Server Error",
                                        f"Failed to update Traefik configuration: {e}")
                
                # Trigger Traefik restart to apply new configuration
                if restart_traefik():
                    logger.info("Traefik restart triggered after password deletion (mTLS mode)")
                else:
                    logger.warning("Traefik restart trigger failed after password deletion")
            else:
                logger.info("Password deletion requested but htpasswd file does not exist")
            
            return send_no_content()
            
        except Exception as e:
            logger.error(f"Password delete failed: {e}")
            return error_response(500, "Internal Server Error",
                                "Failed to delete password")


@auth_bp.route('/api/auth/enroll', methods=['POST'])
def enroll():
    """
    Activate LDevID server certificate (server enrollment)
    
    POST: Activate uploaded LDevID certificate and restart Traefik
    
    Modes:
    - Push mode (default or {"est_mode": "push"}): Activate uploaded LDevID
    - Pull mode ({"est_mode": "pull", "est_server": "..."}): Get cert from EST server (not yet implemented)
    
    Empty POST body defaults to push mode for compatibility.
    """
    logger.info("=== Server enrollment request started ===")
    
    try:
        # Parse request body (empty body or JSON)
        request_data = {}
        content_length = request.content_length or 0
        
        if content_length > 0:
            if request.content_type and 'application/json' in request.content_type:
                request_data = request.get_json() or {}
            else:
                # Accept empty POST without Content-Type
                pass
        
        # Determine enrollment mode
        # - Empty body → defaults to 'push'
        # - Body with content but no est_mode → no default (allows implicit pull via est_server)
        est_mode = request_data.get('est_mode', 'push' if content_length == 0 else None)
        est_server = request_data.get('est_server')
        
        logger.debug(f"Enrollment mode: est_mode={est_mode}, est_server={est_server}")
        
        # Validate mode combinations
        if est_mode and est_server:
            if est_mode != 'pull':
                logger.warning(f"Invalid combination: est_mode='{est_mode}' with est_server specified")
                return error_response(400, "Bad Request",
                                    f"Cannot specify est_server with est_mode='{est_mode}'. Use est_mode='pull' or omit est_mode.")
        
        # Check for missing parameters
        if not est_mode and not est_server:
            logger.warning("Missing required parameters: neither est_mode nor est_server specified")
            return error_response(400, "Bad Request",
                                "Must specify either 'est_mode' or 'est_server' or have empty body for push mode")
        
        # Handle push mode (activate preloaded LDevID)
        if est_mode == 'push':
            logger.info("Processing push mode enrollment")
            
            if est_server:
                logger.warning("Invalid: est_server specified with push mode")
                return error_response(400, "Bad Request",
                                    "Cannot specify est_server with est_mode='push'")
            
            # Check if uploaded LDevID exists
            if not LDEVID_CHAIN_UPLOADED.exists() or not LDEVID_KEY_UPLOADED.exists():
                logger.warning("LDevID certificate not found")
                return error_response(409, "Conflict",
                                    "LDevID certificate not found. Upload certificate to /.well-known/thread/ldevid first")
            
            try:
                logger.info("Activating preloaded LDevID certificate (push mode)")
                
                # Copy uploaded certificates to active location
                shutil.copy(LDEVID_CHAIN_UPLOADED, LDEVID_CERT)
                shutil.copy(LDEVID_KEY_UPLOADED, LDEVID_KEY)
                logger.debug("Certificates copied to active location")
                
                # Set proper permissions on active certificates
                LDEVID_CERT.chmod(0o644)
                LDEVID_KEY.chmod(0o600)
                
                # Check if we should switch to mTLS mode
                # This will set mtls_required=true in client-authz.json if admin client exists
                switch_to_mtls = should_switch_to_mtls(CLIENT_AUTHZ_FILE)
                config_mode = "mtls" if switch_to_mtls else "basicauth"
                
                # Switch configuration files
                try:
                    switch_config_files(config_mode, TRAEFIK_DIR)
                except Exception as e:
                    logger.error(f"Failed to switch configuration files: {e}")
                    return error_response(500, "Internal Server Error",
                                        f"Failed to update Traefik configuration: {e}")
                
                # Restart Traefik to activate new certificate and configuration
                try:
                    if restart_traefik():
                        logger.info(f"Certificate activation complete ({config_mode} mode)")
                    else:
                        logger.warning(f"Certificate activation complete but Traefik restart verification failed")
                except Exception as e:
                    logger.error(f"Failed to restart Traefik: {e}")
                    return error_response(500, "Internal Server Error",
                                        f"Failed to restart Traefik: {e}")
                
                logger.info("LDevID activation successful (push mode)")
                return send_no_content()
                
            except Exception as e:
                logger.error(f"LDevID activation failed: {e}")
                return error_response(500, "Internal Server Error",
                                    f"Failed to activate LDevID: {e}")
        
        # Handle pull mode (EST enrollment) - either explicit or implicit via est_server
        if est_mode == 'pull' or (est_mode is None and est_server):
            if not est_server:
                logger.warning("Missing est_server parameter for pull mode")
                return error_response(400, "Bad Request",
                                    "Missing 'est_server' parameter for pull mode")
            
            logger.info(f"Starting EST pull mode enrollment from {est_server}")
            
            try:
                # Step 1: Validate EST server URL
                is_valid, error_msg = validate_url(est_server)
                if not is_valid:
                    logger.warning(f"Invalid EST server URL: {error_msg}")
                    return error_response(400, "Bad Request", error_msg)
                
                # Step 2: Fetch CA certificates from EST server
                logger.info("Fetching CA certificates from EST server")
                try:
                    ca_certs = fetch_est_cacerts(est_server, CA_BUNDLE, IDEVID_CERT, IDEVID_KEY)
                except Exception as e:
                    logger.error(f"Failed to fetch CA certs: {e}")
                    return error_response(502, "Bad Gateway",
                                        f"Failed to fetch CA certs from EST server: {e}")
                
                # Step 3: Update CA bundle with new certificates
                logger.info("Updating CA bundle with EST server certificates")
                try:
                    added = extract_ca_certs_to_bundle(ca_certs)
                    logger.info(f"Added {added} CA certificate(s) to bundle")
                except Exception as e:
                    logger.warning(f"Failed to update CA bundle: {e}")
                    # Continue anyway - existing bundle may be sufficient
                
                # Step 4: Generate CSR for LDevID
                logger.info("Generating CSR for LDevID")
                try:
                    csr_pem = generate_csr_for_est(LDEVID_KEY_UPLOADED)
                    logger.info("CSR generated successfully")
                except Exception as e:
                    logger.error(f"Failed to generate CSR: {e}")
                    return error_response(500, "Internal Server Error",
                                        f"Failed to generate CSR: {e}")
                
                # Step 5: Submit CSR to EST server and get certificate
                logger.info("Submitting CSR to EST server")
                try:
                    ldevid_cert = submit_csr_to_est(est_server, csr_pem, CA_BUNDLE,
                                                   IDEVID_CERT, IDEVID_KEY)
                    logger.info("Certificate received from EST server")
                except Exception as e:
                    logger.error(f"Failed to submit CSR: {e}")
                    return error_response(502, "Bad Gateway",
                                        f"Failed to submit CSR to EST server: {e}")
                
                # Step 6: Save certificate to uploaded location
                try:
                    LDEVID_CHAIN_UPLOADED.parent.mkdir(parents=True, exist_ok=True)
                    with open(LDEVID_CHAIN_UPLOADED, 'w') as f:
                        f.write(ldevid_cert)
                    LDEVID_CHAIN_UPLOADED.chmod(0o644)
                    logger.info(f"LDevID certificate saved to {LDEVID_CHAIN_UPLOADED}")
                except Exception as e:
                    logger.error(f"Failed to save certificate: {e}")
                    return error_response(500, "Internal Server Error",
                                        f"Failed to save certificate: {e}")
                
                # Step 7: Activate the certificate
                logger.info("Activating new LDevID certificate")
                try:
                    # Copy uploaded files to active locations
                    shutil.copy2(LDEVID_CHAIN_UPLOADED, LDEVID_CERT)
                    shutil.copy2(LDEVID_KEY_UPLOADED, LDEVID_KEY)
                    LDEVID_CERT.chmod(0o644)
                    LDEVID_KEY.chmod(0o600)
                    logger.info(f"Copied certificate and key to active locations")
                    
                    # Restart Traefik to apply new certificate
                    logger.info("Restarting Traefik to activate certificate")
                    if not restart_traefik():
                        logger.warning("Traefik restart trigger failed")
                    
                    logger.info("LDevID activation successful (EST pull mode)")
                    return send_no_content()
                    
                except Exception as e:
                    logger.error(f"Failed to activate certificate: {e}")
                    return error_response(500, "Internal Server Error",
                                        f"Failed to activate certificate: {e}")
            
            except Exception as e:
                logger.error(f"EST pull mode enrollment failed: {e}")
                return error_response(500, "Internal Server Error",
                                    f"EST pull mode enrollment failed: {e}")
        
        # Invalid mode
        if est_mode and est_mode not in ['push', 'pull']:
            logger.warning(f"Invalid est_mode: '{est_mode}'")
            return error_response(400, "Bad Request",
                                f"Invalid est_mode '{est_mode}'. Supported: 'push', 'pull'")
        
    except Exception as e:
        logger.error(f"Enrollment failed: {e}")
        return error_response(500, "Internal Server Error",
                            f"Enrollment failed: {e}")


@auth_bp.route('/api/auth/certs', methods=['GET', 'PUT'])
def handle_certs():
    """
    Manage client certificates for mutual TLS authentication
    
    GET: List client certificates or CA certificates
    PUT: Upload new client certificate (auto-enrolls in authorization database)
    """
    if request.method == 'GET':
        try:
            # Check if requesting CA certificates
            cert_type = request.args.get('type', '')
            
            if cert_type == 'ca':
                # List CA certificates from ca-bundle.pem
                if not CA_BUNDLE.exists():
                    ca_list = []
                else:
                    ca_list = []
                    with open(CA_BUNDLE, 'r') as f:
                        bundle_data = f.read()
                    
                    # Split bundle into individual certs
                    certs = []
                    current_cert = []
                    in_cert = False
                    
                    for line in bundle_data.split('\n'):
                        if '-----BEGIN CERTIFICATE-----' in line:
                            in_cert = True
                            current_cert = [line]
                        elif '-----END CERTIFICATE-----' in line:
                            current_cert.append(line)
                            certs.append('\n'.join(current_cert))
                            current_cert = []
                            in_cert = False
                        elif in_cert:
                            current_cert.append(line)
                    
                    # Extract info from each CA cert
                    for cert_pem in certs:
                        try:
                            cert_data = cert_pem.encode('ascii')
                            cert_id = calculate_cert_id(cert_data)
                            cert_info = extract_cert_info(cert_data)
                            
                            # Determine CA type: root (self-signed) or intermediate
                            subject = cert_info.get('subject', '')
                            issuer = cert_info.get('issuer', '')
                            ca_type = 'root' if subject == issuer else 'intermediate'
                            
                            # Extract Subject Key Identifier (SKI)
                            ski = None
                            try:
                                # Load certificate from PEM
                                cert_obj = x509.load_pem_x509_certificate(cert_data)
                                
                                # Get Subject Key Identifier extension
                                try:
                                    ski_ext = cert_obj.extensions.get_extension_for_oid(
                                        ExtensionOID.SUBJECT_KEY_IDENTIFIER
                                    )
                                    # Convert bytes to hex string (lowercase, no colons)
                                    ski = ski_ext.value.digest.hex().lower()
                                except x509.ExtensionNotFound:
                                    # SKI extension not present in certificate
                                    pass
                            except Exception:
                                pass
                            
                            ca_entry = {
                                'id': cert_id,
                                'subject': subject,
                                'issuer': issuer,
                                'notBefore': cert_info.get('notBefore', ''),
                                'notAfter': cert_info.get('notAfter', ''),
                                'type': ca_type
                            }
                            
                            # Only include subjectKeyId if it exists
                            if ski:
                                ca_entry['subjectKeyId'] = ski
                            
                            ca_list.append(ca_entry)
                        except Exception as e:
                            logger.warning(f"Failed to parse CA cert: {e}")
                            continue
                
                # Return CA list as JSON
                if prefers_json_api():
                    return send_json_api(ca_list, resource_type='ca-certificate')
                else:
                    return send_json(ca_list)
            
            # Otherwise list client certificates
            if not CLIENT_CERTS_DIR.exists():
                certs_list = []
            else:
                # Load authorization database
                authz_data = {'clients': {}}
                if CLIENT_AUTHZ_FILE.exists():
                    try:
                        with open(CLIENT_AUTHZ_FILE, 'r') as f:
                            authz_data = json.load(f)
                    except Exception as e:
                        logger.warning(f"Failed to load client-authz.json: {e}")
                
                certs_list = []
                for cert_file in CLIENT_CERTS_DIR.glob('*.pem'):
                    try:
                        cert_id = cert_file.stem
                        
                        with open(cert_file, 'rb') as f:
                            cert_data = f.read()
                        
                        cert_info = extract_cert_info(cert_data)
                        
                        # Get authorization info from client-authz.json
                        client_authz = authz_data.get('clients', {}).get(cert_id, {})
                        
                        certs_list.append({
                            'id': cert_id,
                            'subject': cert_info.get('subject', ''),
                            'issuer': cert_info.get('issuer', ''),
                            'notBefore': cert_info.get('notBefore', ''),
                            'notAfter': cert_info.get('notAfter', ''),
                            'sans': cert_info.get('sans', []),
                            'role': client_authz.get('role', 'unknown'),
                            'scopes': client_authz.get('scopes', [])
                        })
                        
                    except Exception as e:
                        logger.warning(f"Failed to read cert {cert_file}: {e}")
                        continue
            
            # Return as PKCS#7 bundle if requested
            response_format = get_preferred_response_format(request)
            
            if response_format == 'pkcs7':
                # Combine all client certificates into PKCS#7
                all_certs = b''
                for cert_info in certs_list:
                    cert_file = CLIENT_CERTS_DIR / f"{cert_info['id']}.pem"
                    if cert_file.exists():
                        with open(cert_file, 'rb') as f:
                            all_certs += f.read()
                
                if all_certs:
                    pkcs7_data = pem_to_pkcs7(all_certs)
                    return send_pkcs7(pkcs7_data)
                else:
                    return error_response(404, "Not Found",
                                        "No client certificates found")
            
            # Otherwise return JSON
            if prefers_json_api():
                return send_json_api(certs_list, resource_type='client-certificate')
            else:
                return send_json(certs_list)
            
        except Exception as e:
            logger.error(f"Failed to list certificates: {e}")
            return error_response(500, "Internal Server Error",
                                "Failed to list certificates")
    
    else:  # PUT
        try:
            cert_data = request.data
            
            if not cert_data:
                return error_response(400, "Bad Request",
                                    "Missing certificate data")
            
            # The uploaded data should contain the full certificate chain
            # Split to get just the first (end-entity) certificate
            cert_pem, _ = split_cert_and_key(cert_data)
            
            if not cert_pem:
                return error_response(422, "Unprocessable Content",
                                    "No certificate found in request")
            
            # Calculate certificate ID from end-entity cert
            cert_id = calculate_cert_id(cert_pem.encode())
            
            # Store the full chain (not just end-entity cert)
            CLIENT_CERTS_DIR.mkdir(parents=True, exist_ok=True)
            
            cert_file = CLIENT_CERTS_DIR / f"{cert_id}.pem"
            
            with open(cert_file, 'wb') as f:
                f.write(cert_data if isinstance(cert_data, bytes) else cert_data.encode())
            
            logger.info(f"Client certificate stored: {cert_id}")
            
            # Extract CA certificates from chain and update ca-bundle.pem
            try:
                added_count = extract_ca_certs_to_bundle(cert_data, CA_BUNDLE)
                if added_count > 0:
                    logger.info(f"Added {added_count} CA certificate(s) to bundle from client cert chain")
            except Exception as e:
                logger.warning(f"Failed to extract CA certificates: {e}")
            
            # Auto-enroll in authorization database
            authz_data = {'clients': {}}
            if CLIENT_AUTHZ_FILE.exists():
                try:
                    with open(CLIENT_AUTHZ_FILE, 'r') as f:
                        authz_data = json.load(f)
                except Exception as e:
                    logger.warning(f"Failed to load client-authz.json: {e}")
            
            # Determine role: first client gets admin, subsequent get user
            if 'clients' not in authz_data:
                authz_data['clients'] = {}
            
            existing_count = len(authz_data['clients'])
            
            if existing_count == 0:
                # First client cert: grant admin privileges
                role = 'admin'
                scopes = ['read', 'write']
                logger.info(f"Auto-enrolling first client cert as admin: {cert_id}")
            else:
                # Subsequent client certs: grant user privileges
                role = 'user'
                scopes = ['read']
                logger.info(f"Auto-enrolling client cert as user: {cert_id}")
            
            # Extract cert info for authorization metadata
            cert_info = extract_cert_info(cert_pem.encode())
            
            # Store authorization entry
            authz_data['clients'][cert_id] = {
                'role': role,
                'scopes': scopes,
                'sans': cert_info.get('sans', []),
                'issuer': cert_info.get('issuer', '')
            }
            
            # Save authorization database
            CLIENT_AUTHZ_FILE.parent.mkdir(parents=True, exist_ok=True)
            with open(CLIENT_AUTHZ_FILE, 'w') as f:
                json.dump(authz_data, f, indent=2)
            
            logger.info(f"Client {cert_id} auto-enrolled with role: {role}")
            
            # Return certificate info
            response_data = {
                'id': cert_id,
                'subject': cert_info.get('subject', ''),
                'issuer': cert_info.get('issuer', ''),
                'notBefore': cert_info.get('notBefore', ''),
                'notAfter': cert_info.get('notAfter', ''),
                'sans': cert_info.get('sans', []),
                'role': role,
                'scopes': scopes
            }
            
            return send_created(data=response_data, location=f"/api/auth/certs/{cert_id}")
            
        except Exception as e:
            logger.error(f"Failed to store certificate: {e}")
            return error_response(500, "Internal Server Error",
                                "Failed to store certificate")


@auth_bp.route('/api/auth/certs/<cert_id>', methods=['GET', 'PUT', 'DELETE'])
def handle_cert_by_id(cert_id):
    """
    Manage individual client certificate
    
    GET: Retrieve client certificate by ID
    PUT: Update client certificate by ID (with ID verification)
    DELETE: Delete client certificate by ID (with last-admin protection)
    """
    cert_file = CLIENT_CERTS_DIR / f"{cert_id}.pem"
    
    if request.method == 'GET':
        if not cert_file.exists():
            return error_response(404, "Not Found",
                                f"Client certificate {cert_id} not found")
        
        try:
            with open(cert_file, 'rb') as f:
                cert_data = f.read()
            
            # Return in preferred format
            response_format = get_preferred_response_format(request)
            
            if response_format == 'pkcs7':
                pkcs7_data = pem_to_pkcs7(cert_data)
                return send_pkcs7(pkcs7_data)
            elif response_format == 'pem':
                return send_pem(cert_data)
            else:
                # Return JSON with cert info and authorization
                cert_info = extract_cert_info(cert_data)
                
                # Load authorization info
                authz_data = {'clients': {}}
                if CLIENT_AUTHZ_FILE.exists():
                    try:
                        with open(CLIENT_AUTHZ_FILE, 'r') as f:
                            authz_data = json.load(f)
                    except Exception as e:
                        logger.warning(f"Failed to load client-authz.json: {e}")
                
                client_authz = authz_data.get('clients', {}).get(cert_id, {})
                
                response_data = {
                    'id': cert_id,
                    'subject': cert_info.get('subject', ''),
                    'issuer': cert_info.get('issuer', ''),
                    'notBefore': cert_info.get('notBefore', ''),
                    'notAfter': cert_info.get('notAfter', ''),
                    'sans': cert_info.get('sans', []),
                    'role': client_authz.get('role', 'unknown'),
                    'scopes': client_authz.get('scopes', [])
                }
                
                if prefers_json_api():
                    return send_json_api(response_data, resource_type='certificate')
                else:
                    return send_json(response_data)
            
        except Exception as e:
            logger.error(f"Failed to read certificate {cert_id}: {e}")
            return error_response(500, "Internal Server Error",
                                "Failed to read certificate")
    
    elif request.method == 'PUT':
        try:
            cert_data = request.data
            
            if not cert_data:
                return error_response(400, "Bad Request",
                                    "Missing certificate data")
            
            # Split to get end-entity certificate
            cert_pem, _ = split_cert_and_key(cert_data)
            
            if not cert_pem:
                return error_response(422, "Unprocessable Content",
                                    "No certificate found in request")
            
            # Verify ID matches
            actual_id = calculate_cert_id(cert_pem.encode())
            if actual_id != cert_id:
                return error_response(422, "Unprocessable Content",
                                    f"Certificate ID mismatch: expected {cert_id}, got {actual_id}")
            
            # Store the full chain
            CLIENT_CERTS_DIR.mkdir(parents=True, exist_ok=True)
            
            with open(cert_file, 'wb') as f:
                f.write(cert_data if isinstance(cert_data, bytes) else cert_data.encode())
            
            logger.info(f"Client certificate updated: {cert_id}")
            
            # Extract CA certificates from chain and update ca-bundle.pem
            try:
                added_count = extract_ca_certs_to_bundle(cert_data, CA_BUNDLE)
                if added_count > 0:
                    logger.info(f"Added {added_count} CA certificate(s) to bundle from updated cert chain")
            except Exception as e:
                logger.warning(f"Failed to extract CA certificates: {e}")
            
            # Update authorization metadata
            authz_data = {'clients': {}}
            if CLIENT_AUTHZ_FILE.exists():
                try:
                    with open(CLIENT_AUTHZ_FILE, 'r') as f:
                        authz_data = json.load(f)
                except Exception as e:
                    logger.warning(f"Failed to load client-authz.json: {e}")
            
            # Keep existing role/scopes but update sans/issuer
            if 'clients' not in authz_data:
                authz_data['clients'] = {}
            
            cert_info = extract_cert_info(cert_pem.encode())
            
            if cert_id in authz_data['clients']:
                # Update existing entry
                authz_data['clients'][cert_id]['sans'] = cert_info.get('sans', [])
                authz_data['clients'][cert_id]['issuer'] = cert_info.get('issuer', '')
            else:
                # Create new entry (shouldn't happen in PUT with ID, but be safe)
                authz_data['clients'][cert_id] = {
                    'role': 'user',
                    'scopes': ['read'],
                    'sans': cert_info.get('sans', []),
                    'issuer': cert_info.get('issuer', '')
                }
            
            # Save authorization database
            with open(CLIENT_AUTHZ_FILE, 'w') as f:
                json.dump(authz_data, f, indent=2)
            
            return send_no_content()
            
        except Exception as e:
            logger.error(f"Failed to update certificate {cert_id}: {e}")
            return error_response(500, "Internal Server Error",
                                "Failed to update certificate")
    
    else:  # DELETE
        try:
            if not cert_file.exists():
                return error_response(404, "Not Found",
                                    f"Client certificate {cert_id} not found")
            
            # Load authorization database
            authz_data = {'clients': {}}
            if CLIENT_AUTHZ_FILE.exists():
                try:
                    with open(CLIENT_AUTHZ_FILE, 'r') as f:
                        authz_data = json.load(f)
                except Exception as e:
                    logger.warning(f"Failed to load client-authz.json: {e}")
            
            # Safety check: prevent deleting last admin certificate
            # Only enforce when authenticated via mTLS
            auth_method = g.get('auth', {}).get('method')
            
            if auth_method == 'mtls':
                admin_count = sum(
                    1 for client in authz_data.get('clients', {}).values()
                    if client.get('role') == 'admin'
                )
                
                is_admin = authz_data.get('clients', {}).get(cert_id, {}).get('role') == 'admin'
                
                if is_admin and admin_count <= 1:
                    return error_response(403, "Forbidden",
                                        "Cannot delete the last admin certificate when authenticated via mTLS. "
                                        "This would cause a lockout.")
            
            # Delete certificate file
            cert_file.unlink()
            
            # Remove from authorization database
            if 'clients' in authz_data and cert_id in authz_data['clients']:
                del authz_data['clients'][cert_id]
                
                # Save updated authorization database
                with open(CLIENT_AUTHZ_FILE, 'w') as f:
                    json.dump(authz_data, f, indent=2)
            
            logger.info(f"Client certificate deleted: {cert_id}")
            return send_no_content()
            
        except Exception as e:
            logger.error(f"Failed to delete certificate {cert_id}: {e}")
            return error_response(500, "Internal Server Error",
                                "Failed to delete certificate")


@auth_bp.route('/api/auth/token', methods=['POST'])
def create_token():
    """
    Create JWT bearer token (placeholder for future OAuth2 support)
    
    POST: Generate token for authorized client
    """
    # This is a placeholder for future OAuth2/JWT implementation
    # Currently the system uses mutual TLS exclusively
    return error_response(501, "Not Implemented",
                        "Token authentication not yet implemented. Use mutual TLS.")
