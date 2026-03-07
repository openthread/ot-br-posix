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
"""
Authorization Middleware - Validates client certificate authorization

Middleware that:
- Extracts client certificate info from Traefik headers
- Validates against authorization database (client_authz.json)
- Checks issuer, SANs, role, and scopes
- Injects auth info into Flask request context
- Supports HTTP Basic Auth in basicauth mode
- Enforces mTLS-only authentication in mTLS mode
"""

import base64
import json
import logging
import re
from pathlib import Path
from urllib.parse import unquote_plus
from flask import g, request

logger = logging.getLogger(__name__)


def normalize_dn(dn_string):
    """
    Normalize a Distinguished Name by parsing components and sorting them.
    
    Handles different DN orderings from different sources:
    - Traefik: C=CH,O=OpenThread,CN=Demo CA
    - OpenSSL RFC2253: CN=Demo CA,O=OpenThread,C=CH
    
    Returns components sorted alphabetically by attribute for consistent comparison.
    """
    if not dn_string:
        return ""
    
    # Parse DN into components
    components = {}
    for part in dn_string.split(','):
        part = part.strip()
        if '=' in part:
            key, value = part.split('=', 1)
            components[key.strip()] = value.strip()
    
    # Sort by attribute name and reconstruct
    sorted_components = sorted(components.items())
    return ','.join(f"{k}={v}" for k, v in sorted_components)


class AuthorizationMiddleware:
    """
    WSGI middleware for client certificate authorization
    
    Validates all requests by checking client certificates extracted by Traefik
    against the authorization database. Implements role-based access control
    with read/write scopes.
    """
    
    def __init__(self, app, authz_file, htpasswd_file=None):
        """
        Initialize authorization middleware
        
        Args:
            app: Flask application instance
            authz_file: Path to client_authz.json authorization database
            htpasswd_file: Path to htpasswd file for Basic Auth (optional)
        """
        self.app = app
        self.authz_file = Path(authz_file)
        self.htpasswd_file = Path(htpasswd_file) if htpasswd_file else None
        self._ensure_authz_file()
    
    def _ensure_authz_file(self):
        """Ensure authorization file exists with empty clients structure"""
        if not self.authz_file.exists():
            self.authz_file.parent.mkdir(parents=True, exist_ok=True)
            with open(self.authz_file, 'w') as f:
                json.dump({'clients': {}}, f, indent=2)
            logger.info(f"Created empty authorization database: {self.authz_file}")
    
    def _load_authz_db(self):
        """Load authorization database from file"""
        try:
            with open(self.authz_file, 'r') as f:
                db = json.load(f)
                return db.get('clients', {})
        except (FileNotFoundError, json.JSONDecodeError) as e:
            logger.error(f"Failed to load authorization database: {e}")
            return {}
    
    def _parse_cert_info(self, cert_info_header):
        """
        Parse client certificate info from Traefik header
        
        Args:
            cert_info_header: X-Forwarded-Tls-Client-Cert-Info header value
        
        Returns:
            dict with 'issuer' and 'sans' keys
        """
        if not cert_info_header:
            return {'issuer': '', 'sans': []}
        
        # URL decode
        cert_info = unquote_plus(cert_info_header)
        
        # Extract issuer: Issuer="..."
        issuer_match = re.search(r'Issuer="([^"]+)"', cert_info)
        issuer = issuer_match.group(1) if issuer_match else ''
        
        # Extract SANs: SAN="..." (comma-separated)
        san_match = re.search(r'SAN="([^"]+)"', cert_info)
        if san_match:
            san_str = san_match.group(1)
            sans = [s.strip() for s in san_str.split(',')]
        else:
            sans = []
        
        logger.debug(f"Parsed cert: issuer={issuer}, sans={sans}")
        return {'issuer': issuer, 'sans': sans}
    
    def _find_authorized_client(self, issuer, sans, clients_db):
        """
        Find matching client in authorization database
        
        Args:
            issuer: Certificate issuer DN
            sans: List of Subject Alternative Names
            clients_db: Dictionary of authorized clients
        
        Returns:
            tuple: (client_id, client_config) or (None, None)
        """
        # Normalize client certificate issuer for comparison
        issuer_normalized = normalize_dn(issuer)
        
        for client_id, client_data in clients_db.items():
            # Check issuer match - normalize both sides for comparison
            authz_issuer = client_data.get('issuer', '')
            if normalize_dn(authz_issuer) != issuer_normalized:
                continue
            
            # Check SAN match (any client SAN matches any stored SAN)
            stored_sans = client_data.get('sans', [])
            
            # Match found if any SAN matches
            if any(san in stored_sans for san in sans):
                logger.debug(f"Client authorized: id={client_id}, role={client_data.get('role')}")
                return client_id, client_data
        
        return None, None
    
    def _is_mtls_mode(self):
        """
        Check if system is in mTLS mode (vs basicauth mode)
        
        System is in mTLS mode when mtls_required flag is explicitly set to true
        in client-authz.json (set by enrollment endpoint when admin client exists).
        In mTLS mode, Basic Auth should be rejected.
        
        Returns:
            bool: True if in mTLS mode, False if in basicauth mode
        """
        # Use self.authz_file which is already the path to client-authz.json
        if not self.authz_file.exists():
            return False
        
        try:
            with open(self.authz_file, 'r') as f:
                authz = json.load(f)
            
            # Check explicit mtls_required flag (default False if not present)
            mtls_required = authz.get('mtls_required', False)
            return mtls_required
        except Exception as e:
            logger.warning(f"Failed to check client-authz.json: {e}")
        
        return False
    
    def _check_scope(self, method, scopes):
        """
        Check if client has required scope for HTTP method
        
        Args:
            method: HTTP method
            scopes: List of scope strings
        
        Returns:
            tuple: (has_scope: bool, required_scope: str)
        """
        if method in ['GET', 'HEAD', 'OPTIONS']:
            required_scope = 'read'
        elif method in ['POST', 'PUT', 'PATCH', 'DELETE']:
            required_scope = 'write'
        else:
            return False, 'unknown'
        
        has_scope = required_scope in scopes
        return has_scope, required_scope
    
    def __call__(self, environ, start_response):
        """
        WSGI middleware entry point
        
        Validates client certificate or HTTP Basic Auth before passing request
        to Flask application. Injects auth info into environ for Flask to access.
        
        Authentication priority:
        1. Public paths (no auth required)
        2. Mode detection (basicauth vs mTLS)
        3. HTTP Basic Auth (basicauth mode only - rejected in mTLS mode)
        4. Mutual TLS (validates client certificate enrollment)
        
        Mode behaviors:
        - basicauth mode: Basic Auth accepted, client certs ignored for authorization
        - mTLS mode: Basic Auth rejected with 403, only enrolled client certs accepted
        """
        method = environ.get('REQUEST_METHOD', 'GET')
        path = environ.get('PATH_INFO', '/')
        
        # Internal endpoints accessible only to Traefik forwardAuth (not external clients)
        INTERNAL_PATHS = ['/health', '/internal/forwardauth']
        PUBLIC_GET_PATHS = [
            '/.well-known/thread',
            '/.well-known/thread/idevid',
            '/.well-known/thread/ldevid'
        ]
        
        if path in INTERNAL_PATHS:
            return self.app(environ, start_response)
        
        if method == 'GET' and path in PUBLIC_GET_PATHS:
            return self.app(environ, start_response)
        
        # Load authorization database
        clients_db = self._load_authz_db()
        
        # Check if system is in mTLS mode (admin client enrolled)
        is_mtls_mode = self._is_mtls_mode()
        
        # In mTLS mode: reject Basic Auth, enforce certificate-only authentication
        # In basicauth mode: accept Basic Auth, allow cert enrollment
        auth_header = environ.get('HTTP_AUTHORIZATION', '')
        
        if auth_header and auth_header.startswith('Basic ') and not is_mtls_mode:
            
            # basicauth mode - accept Basic Auth
            # If Traefik's basicAuth middleware let this request through,
            # it means the credentials were already validated.
            # We trust Traefik's validation and don't re-check.
            
            try:
                # Extract username for logging/tracking (optional)
                auth_b64 = auth_header[6:]
                credentials = base64.b64decode(auth_b64).decode('utf-8')
                username = credentials.split(':', 1)[0] if ':' in credentials else 'unknown'
            except:
                username = 'unknown'
            
            # Basic Auth successful (validated by Traefik) - grant admin privileges
            # Client cert is ignored in basicauth mode (used for enrollment, not authorization)
            environ['auth.role'] = 'admin'
            environ['auth.scopes'] = ['read', 'write']
            environ['auth.client_id'] = f'basic:{username}'
            environ['auth.method'] = 'basic'
            environ['auth.username'] = username
            
            logger.info(f"Auth passed (Basic/Traefik): {method} {path} - User: {username}")
            
            return self.app(environ, start_response)
        
        # No Basic Auth - try mutual TLS authentication (mTLS mode)
        # In mTLS mode, Traefik enforces client cert (mtls-required)
        cert_info_header = environ.get('HTTP_X_FORWARDED_TLS_CLIENT_CERT_INFO', '')
        
        if is_mtls_mode and cert_info_header:
            # Parse certificate
            cert_data = self._parse_cert_info(cert_info_header)
            
            if not cert_data['issuer'] or not cert_data['sans']:
                logger.warning(f"Auth failed: Invalid certificate info for {method} {path}")
                return self._error_response(
                    start_response,
                    403,
                    "Invalid client certificate information",
                    environ.get('HTTP_ACCEPT', '')
                )
            
            # Find authorized client
            client_id, client_config = self._find_authorized_client(
                cert_data['issuer'],
                cert_data['sans'],
                clients_db
            )
            
            if client_config:
                # mTLS authentication successful
                role = client_config.get('role', 'user')
                scopes = client_config.get('scopes', [])
                
                # Check scope
                has_scope, required_scope = self._check_scope(method, scopes)
                
                if not has_scope:
                    logger.warning(
                        f"Auth failed: Insufficient scope - "
                        f"Required: {required_scope}, Role: {role}, Client: {client_id}"
                    )
                    return self._error_response(
                        start_response,
                        403,
                        f"Insufficient permissions: {required_scope} access required",
                        environ.get('HTTP_ACCEPT', '')
                    )
                
                # Authorization successful - inject into environ
                environ['auth.role'] = role
                environ['auth.scopes'] = scopes
                environ['auth.client_id'] = client_id
                environ['auth.method'] = 'mtls'
                environ['client_cert_info'] = cert_data
                
                logger.info(
                    f"Auth passed (mTLS): {method} {path} - "
                    f"ID: {client_id}, Role: {role}, Scope: {required_scope}"
                )
                
                return self.app(environ, start_response)
            else:
                # Certificate not authorized - reject with 403
                # This happens in mTLS mode when client presents unauthorized cert
                logger.warning(
                    f"Auth failed: Certificate not authorized - "
                    f"Issuer: {cert_data['issuer']}, SANs: {cert_data['sans']}"
                )
                if auth_header and auth_header.startswith('Basic '):
                    logger.warning(
                        f"Note: Basic Auth credentials were provided but rejected in mTLS mode for {method} {path}"
                    )
                    return self._error_response(
                        start_response,
                        403,
                        "Client certificate not authorized. Basic Authentication rejected in mTLS mode.",
                        environ.get('HTTP_ACCEPT', '')
                    )
                else:
                    return self._error_response(
                        start_response,
                        403,
                        "Client certificate not authorized",
                        environ.get('HTTP_ACCEPT', '')
                    )
        
        # No Basic authentication provided (clients without certificates are blocked by traefik in mtls mode)
        logger.warning(f"Auth failed: No credentials provided for {method} {path}")
        return self._error_response(
            start_response,
            401,
            "Authentication required",
            environ.get('HTTP_ACCEPT', ''),
            www_authenticate='Basic realm="OpenThread Border Router"'
        )
        

    
    def _error_response(self, start_response, status, detail, accept_header, www_authenticate=None):
        """
        Return error response with content negotiation
        
        Args:
            start_response: WSGI start_response callable
            status: HTTP status code
            detail: Error detail message
            accept_header: Client's Accept header
            www_authenticate: WWW-Authenticate header value (for 401 responses)
        
        Returns:
            response body as list of bytes
        """
        status_text = {
            401: 'Unauthorized',
            403: 'Forbidden',
            500: 'Internal Server Error'
        }.get(status, 'Error')
        
        # Content negotiation
        if 'application/vnd.api+json' in accept_header:
            content_type = 'application/vnd.api+json'
            body = json.dumps({
                'errors': [{
                    'title': status_text,
                    'status': status,
                    'detail': detail
                }]
            })
        else:
            content_type = 'application/json'
            body = json.dumps({
                'title': status_text,
                'status': status,
                'detail': detail
            })
        
        body_bytes = body.encode('utf-8')
        
        headers = [
            ('Content-Type', content_type),
            ('Content-Length', str(len(body_bytes)))
        ]
        
        # Add WWW-Authenticate header for 401 responses
        if www_authenticate:
            headers.append(('WWW-Authenticate', www_authenticate))
        
        start_response(f'{status} {status_text}', headers)
        
        return [body_bytes]
