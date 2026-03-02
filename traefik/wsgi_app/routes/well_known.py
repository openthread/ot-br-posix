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
"""Route handlers for /.well-known/thread/* endpoints"""

import hashlib
import logging
import time
from flask import Blueprint, request
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.backends import default_backend

from ..config import (IDEVID_CERT, IDEVID_CHAIN, LDEVID_CERT, LDEVID_KEY,
                      CSR_FILE, CA_BUNDLE, LDEVID_CHAIN_UPLOADED,
                      LDEVID_KEY_UPLOADED, CSR_TEMP_KEY_FILE)
from ..utils.responses import (send_json, send_pkcs7, send_pkcs10, send_pem,
                               send_no_content, error_response)
from ..utils.cert_utils import split_cert_and_key, extract_ca_certs_to_bundle

from ..utils.system_utils import get_hostname

logger = logging.getLogger(__name__)

well_known_bp = Blueprint('well_known', __name__)


@well_known_bp.route('/.well-known/thread', methods=['GET'])
def get_well_known_thread():
    """
    Get API discovery information
    
    Returns API version and entry points for service discovery.
    """
    return send_json({
        'api': {
            'version': '0.3.0',
            'base': '/api/'
        },
        'links': [{
            'href': '/.well-known/thread',
            'rel': 'self',
            'type': ['application/json']
        }, {
            'href': '/.well-known/thread/idevid',
            'rel': 'idevid',
            'type': ['application/pkcs7-mime; smime-type=certs-only']
        }, {
            'href':
                '/.well-known/thread/ldevid',
            'rel':
                'ldevid',
            'type': [
                'application/pkcs7-mime; smime-type=certs-only',
                'application/pem-certificate-chain', 'application/x-pem-file'
            ]
        }, {
            'href': '/.well-known/thread/csr',
            'rel': 'csr',
            'type': ['application/pkcs10']
        }, {
            'href': '/api/auth/password',
            'rel': 'password',
            'type': ['text/plain; charset=UTF-8']
        }, {
            'href': '/api/auth/enroll',
            'rel': 'enroll',
            'type': ['application/json']
        }, {
            'href':
                '/api/auth/certs',
            'rel':
                'certs',
            'type': [
                'application/json',
                'application/pkcs7-mime; smime-type=certs-only'
            ]
        }, {
            'href': '/api/node',
            'rel': 'node',
            'type': ['application/vnd.api+json']
        }, {
            'href': '/api/actions',
            'rel': 'task',
            'type': ['application/vnd.api+json']
        }, {
            'href': '/api/devices',
            'rel': 'device',
            'type': ['application/vnd.api+json']
        }, {
            'href': '/api/diagnostics',
            'rel': 'diagnostic',
            'type': ['application/vnd.api+json']
        }]
    })


@well_known_bp.route('/.well-known/thread/idevid', methods=['GET'])
def get_idevid():
    """
    Get factory-installed IDevID certificate
    
    Returns manufacturer device certificate in PKCS#7 format.
    """
    if not IDEVID_CERT.exists():
        logger.error("IDevID certificate not found")
        return error_response(500, "Internal Server Error",
                              "IDevID certificate not available")

    try:
        with open(IDEVID_CERT, 'rb') as f:
            cert_data = f.read()

        # Return as PKCS#7 bundle
        from ..utils.cert_utils import pem_to_pkcs7
        pkcs7_data = pem_to_pkcs7(cert_data)

        return send_pkcs7(pkcs7_data)

    except Exception as e:
        logger.error(f"Failed to read IDevID certificate: {e}")
        return error_response(500, "Internal Server Error",
                              "Failed to retrieve IDevID certificate")


@well_known_bp.route('/.well-known/thread/ldevid', methods=['GET', 'PUT'])
def handle_ldevid():
    """
    Get or set operational LDevID certificate
    
    GET: Returns generated or uploaded LDevID certificate
    PUT: Store externally-signed LDevID certificate and private key
    """
    if request.method == 'GET':
        # GET returns the uploaded (not yet active) LDevID certificate
        # Client uploads to ldevid-chain.pem, then can retrieve it before activation
        if not LDEVID_CHAIN_UPLOADED.exists():
            return error_response(404, "Not Found",
                                  "LDevID certificate not yet uploaded")

        try:
            with open(LDEVID_CHAIN_UPLOADED, 'rb') as f:
                cert_data = f.read()

            # Return as PKCS#7 bundle
            from ..utils.cert_utils import pem_to_pkcs7
            pkcs7_data = pem_to_pkcs7(cert_data)

            return send_pkcs7(pkcs7_data)

        except Exception as e:
            logger.error(f"Failed to read LDevID certificate: {e}")
            return error_response(500, "Internal Server Error",
                                  "Failed to retrieve LDevID certificate")

    else:  # PUT
        # Store certificate and private key
        try:
            cert_data = request.data

            if not cert_data:
                return error_response(400, "Bad Request",
                                      "Missing certificate data")

            # Split certificate and key
            cert_pem, key_pem = split_cert_and_key(cert_data)

            if not cert_pem:
                return error_response(422, "Unprocessable Content",
                                      "No certificate found in request")

            # Key is optional - if not provided, verify cert matches a stored key
            if not key_pem:
                logger.info(
                    "LDevID uploaded without private key - checking stored keys"
                )

                # Load certificate and get its public key
                try:
                    cert = x509.load_pem_x509_certificate(
                        cert_pem.encode('ascii')
                        if isinstance(cert_pem, str) else cert_pem,
                        default_backend())
                    cert_public_key = cert.public_key()
                    cert_pubkey_bytes = cert_public_key.public_bytes(
                        encoding=serialization.Encoding.PEM,
                        format=serialization.PublicFormat.SubjectPublicKeyInfo)
                    cert_pubkey_hash = hashlib.md5(
                        cert_pubkey_bytes).hexdigest()
                except Exception as e:
                    return error_response(422, "Unprocessable Content",
                                          "Invalid certificate format")

                # Check against stored keys: active key and CSR temp key
                matched_key_file = None
                for key_file in [LDEVID_KEY, CSR_TEMP_KEY_FILE]:
                    if key_file.exists():
                        try:
                            with open(key_file, 'rb') as f:
                                stored_key_pem = f.read()

                            # Load stored key and get its public key
                            stored_private_key = serialization.load_pem_private_key(
                                stored_key_pem,
                                password=None,
                                backend=default_backend())
                            stored_public_key = stored_private_key.public_key()
                            stored_pubkey_bytes = stored_public_key.public_bytes(
                                encoding=serialization.Encoding.PEM,
                                format=serialization.PublicFormat.
                                SubjectPublicKeyInfo)
                            stored_pubkey_hash = hashlib.md5(
                                stored_pubkey_bytes).hexdigest()

                            if cert_pubkey_hash == stored_pubkey_hash:
                                matched_key_file = key_file
                                logger.info(
                                    f"Certificate matches stored key: {key_file}"
                                )
                                break
                        except Exception as e:
                            logger.warning(
                                f"Failed to check key {key_file}: {e}")
                            continue

                if matched_key_file is None:
                    logger.error(
                        "Certificate does not match any stored private key")
                    return error_response(
                        409, "Conflict",
                        "Certificate public key does not match any stored private key"
                    )

                # Use the matched key
                with open(matched_key_file, 'rb') as f:
                    key_pem = f.read().decode('ascii')
            else:
                # Verify provided key matches certificate
                try:
                    # Load certificate and get its public key
                    cert = x509.load_pem_x509_certificate(
                        cert_pem.encode('ascii')
                        if isinstance(cert_pem, str) else cert_pem,
                        default_backend())
                    cert_public_key = cert.public_key()
                    cert_pubkey_bytes = cert_public_key.public_bytes(
                        encoding=serialization.Encoding.PEM,
                        format=serialization.PublicFormat.SubjectPublicKeyInfo)
                    cert_pubkey_hash = hashlib.md5(
                        cert_pubkey_bytes).hexdigest()

                    # Load private key and get its public key
                    key_private_key = serialization.load_pem_private_key(
                        key_pem.encode('ascii')
                        if isinstance(key_pem, str) else key_pem,
                        password=None,
                        backend=default_backend())
                    key_public_key = key_private_key.public_key()
                    key_pubkey_bytes = key_public_key.public_bytes(
                        encoding=serialization.Encoding.PEM,
                        format=serialization.PublicFormat.SubjectPublicKeyInfo)
                    key_pubkey_hash = hashlib.md5(key_pubkey_bytes).hexdigest()

                    if cert_pubkey_hash != key_pubkey_hash:
                        logger.error("Private key does not match certificate")
                        return error_response(
                            400, "Bad Request",
                            "Private key does not match public key in certificate"
                        )
                    logger.info("Verified: Private key matches certificate")
                except Exception as e:
                    logger.error(f"Failed to verify key/certificate match: {e}")
                    return error_response(422, "Unprocessable Content",
                                          "Invalid certificate or key format")

            # Store certificate and key to uploaded location (not yet active)
            LDEVID_CHAIN_UPLOADED.parent.mkdir(parents=True, exist_ok=True)

            with open(LDEVID_CHAIN_UPLOADED, 'w') as f:
                f.write(cert_pem)

            with open(LDEVID_KEY_UPLOADED, 'w') as f:
                f.write(key_pem)

            # Set proper permissions
            LDEVID_CHAIN_UPLOADED.chmod(0o644)
            LDEVID_KEY_UPLOADED.chmod(0o600)

            # Extract CA certificates from chain and update ca-bundle.pem
            try:
                added_count = extract_ca_certs_to_bundle(cert_data, CA_BUNDLE)
                if added_count > 0:
                    logger.info(
                        f"Added {added_count} CA certificate(s) to bundle from LDevID chain"
                    )
            except Exception as e:
                logger.warning(f"Failed to extract CA certificates: {e}")

            logger.info(
                "LDevID certificate and key uploaded successfully (not yet active)"
            )
            logger.info(
                "To activate, POST to /api/auth/enroll with {\"est_mode\":\"push\"}"
            )
            return send_no_content()

        except Exception as e:
            logger.error(f"Failed to store LDevID certificate: {e}")
            return error_response(500, "Internal Server Error",
                                  "Failed to store LDevID certificate")


@well_known_bp.route('/.well-known/thread/csr', methods=['GET'])
def get_csr():
    """
    Get Certificate Signing Request for LDevID
    
    Generates a new CSR on each request.
    Returns PKCS#10 CSR for external CA signing.
    """
    try:
        # Generate new EC key for CSR (secp256r1 / prime256v1)
        private_key = ec.generate_private_key(ec.SECP256R1())

        # Serialize private key to PEM
        private_key_pem = private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.TraditionalOpenSSL,
            encryption_algorithm=serialization.NoEncryption()).decode('utf-8')

        # Save private key for later certificate upload (CSR-based workflow)
        try:
            CSR_TEMP_KEY_FILE.parent.mkdir(parents=True, exist_ok=True)
            with open(CSR_TEMP_KEY_FILE, 'w') as f:
                f.write(private_key_pem)
            CSR_TEMP_KEY_FILE.chmod(0o600)
            logger.info(f"Saved CSR private key to {CSR_TEMP_KEY_FILE}")
        except Exception as e:
            logger.warning(f"Failed to save CSR private key: {e}")

        # Get serial number from IDevID if available
        serial_no = f"SN-{int(time.time())}"
        if IDEVID_CHAIN.exists():
            try:
                with open(IDEVID_CHAIN, 'rb') as f:
                    idevid_cert_data = f.read()
                idevid_cert = x509.load_pem_x509_certificate(
                    idevid_cert_data, default_backend())
                idevid_serial = format(idevid_cert.serial_number, 'X')
                if idevid_serial:
                    serial_no = f"SN-{idevid_serial}"
            except Exception as e:
                logger.warning(f"Failed to extract IDevID serial: {e}")

        # Get hostname
        hostname = get_hostname()

        # Build CSR subject
        subject = x509.Name([
            x509.NameAttribute(NameOID.COUNTRY_NAME, "CH"),
            x509.NameAttribute(NameOID.ORGANIZATION_NAME, "OpenThread"),
            x509.NameAttribute(NameOID.SERIAL_NUMBER, serial_no),
            x509.NameAttribute(NameOID.COMMON_NAME, f"{hostname}.local"),
        ])

        # Add SAN extension — both bare short name and mDNS .local name (RFC 7030 §4.2 requirement)
        san_extension = x509.SubjectAlternativeName([
            x509.DNSName(hostname),
            x509.DNSName(f"{hostname}.local"),
        ])

        # Generate CSR
        csr = x509.CertificateSigningRequestBuilder().subject_name(
            subject).add_extension(san_extension,
                                   critical=False).sign(private_key,
                                                        hashes.SHA256())

        # Serialize CSR to PEM
        csr_pem = csr.public_bytes(serialization.Encoding.PEM).decode('utf-8')

        # Log CSR generation
        csr_subject = csr.subject.rfc4514_string()
        logger.info(f"CSR generated - Subject: {csr_subject}")

        response = send_pkcs10(csr_pem.encode())
        # Add Content-Disposition header for download
        response.headers[
            'Content-Disposition'] = f'attachment; filename="{hostname}-csr.pem"'
        return response

    except Exception as e:
        logger.error(f"Failed to generate CSR: {e}")
        return error_response(500, "Internal Server Error",
                              f"Failed to generate CSR")
