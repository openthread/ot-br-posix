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
"""Certificate handling utilities"""

import base64
import logging
import socket
import requests
from urllib.parse import urlparse

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.serialization import pkcs7
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import NameOID, ExtensionOID
from cryptography.hazmat.backends import default_backend

from ..config import EST_TIMEOUT

logger = logging.getLogger(__name__)


def calculate_cert_id(cert_pem):
    """
    Calculate certificate ID from PEM-encoded certificate
    
    Uses first 32 characters of SHA-256 fingerprint of DER-encoded certificate.
    This matches the server's certificate ID calculation method.
    
    Args:
        cert_pem: PEM-encoded certificate as string or bytes
    
    Returns:
        str: Certificate ID (32 hex characters, lowercase)
    """
    if isinstance(cert_pem, str):
        cert_pem = cert_pem.encode('ascii')
    
    try:
        # Load certificate and calculate SHA-256 fingerprint
        cert = x509.load_pem_x509_certificate(cert_pem, default_backend())
        fingerprint = cert.fingerprint(hashes.SHA256())
        
        # Convert to hex string (lowercase)
        cert_id = fingerprint.hex().lower()
        
        # Take first 32 characters
        return cert_id[:32]
        
    except Exception as e:
        logger.error(f"Failed to calculate certificate ID: {e}")
        raise ValueError("Invalid certificate format")


def extract_cert_info(cert_pem):
    """
    Extract certificate information for metadata
    
    Args:
        cert_pem: PEM-encoded certificate as string or bytes
    
    Returns:
        dict with subject, issuer, notBefore, notAfter, sans
    """
    if isinstance(cert_pem, str):
        cert_pem = cert_pem.encode('ascii')
    
    try:
        # Load certificate
        cert = x509.load_pem_x509_certificate(cert_pem, default_backend())
        
        # Get subject - format as comma-separated name=value pairs
        subject_parts = []
        for attr in cert.subject:
            subject_parts.append(f"{attr.rfc4514_attribute_name}={attr.value}")
        subject = ','.join(subject_parts)
        
        # Get issuer - format as comma-separated name=value pairs
        issuer_parts = []
        for attr in cert.issuer:
            issuer_parts.append(f"{attr.rfc4514_attribute_name}={attr.value}")
        issuer = ','.join(issuer_parts)
        
        # Get dates (formatted as string matching OpenSSL output)
        # Note: Cryptography <42.0.0 uses not_valid_before/not_valid_after (naive datetime, UTC)
        #       Cryptography >=42.0.0 adds not_valid_before_utc/not_valid_after_utc (aware datetime)
        if hasattr(cert, 'not_valid_before_utc'):
            not_before = cert.not_valid_before_utc.strftime('%b %d %H:%M:%S %Y GMT')
            not_after = cert.not_valid_after_utc.strftime('%b %d %H:%M:%S %Y GMT')
        else:
            # Fallback for older versions (38.x in Debian Bookworm)
            not_before = cert.not_valid_before.strftime('%b %d %H:%M:%S %Y GMT')
            not_after = cert.not_valid_after.strftime('%b %d %H:%M:%S %Y GMT')
        
        # Get SANs
        sans = []
        try:
            san_ext = cert.extensions.get_extension_for_oid(ExtensionOID.SUBJECT_ALTERNATIVE_NAME)
            for san in san_ext.value:
                if isinstance(san, x509.DNSName):
                    sans.append(san.value)
                elif isinstance(san, x509.IPAddress):
                    sans.append(str(san.value))
        except x509.ExtensionNotFound:
            # Certificate may not have SANs
            pass
        
        return {
            'subject': subject,
            'issuer': issuer,
            'notBefore': not_before,
            'notAfter': not_after,
            'sans': sans
        }
        
    except Exception as e:
        logger.error(f"Failed to extract certificate info: {e}")
        raise ValueError("Invalid certificate format")


def pem_to_pkcs7(pem_data):
    """
    Convert PEM certificates to PKCS#7 bundle in DER format
    
    Args:
        pem_data: Single or multiple PEM certificates as string or bytes
    
    Returns:
        bytes: PKCS#7 DER-encoded certificate bundle
    """
    if isinstance(pem_data, str):
        pem_data = pem_data.encode('ascii')
    
    try:
        # Parse all certificates from PEM data
        certs = []
        cert_data = pem_data.decode('ascii') if isinstance(pem_data, bytes) else pem_data
        
        # Extract all certificate blocks
        current_cert = []
        in_cert = False
        
        for line in cert_data.split('\n'):
            if '-----BEGIN CERTIFICATE-----' in line:
                in_cert = True
                current_cert = [line]
            elif '-----END CERTIFICATE-----' in line:
                current_cert.append(line)
                cert_pem = '\n'.join(current_cert).encode('ascii')
                cert = x509.load_pem_x509_certificate(cert_pem, default_backend())
                certs.append(cert)
                current_cert = []
                in_cert = False
            elif in_cert:
                current_cert.append(line)
        
        if not certs:
            raise ValueError("No certificates found in PEM data")
        
        # Serialize to PKCS7 DER format
        pkcs7_data = pkcs7.serialize_certificates(
            certs,
            encoding=serialization.Encoding.DER
        )
        
        return pkcs7_data
    except Exception as e:
        logger.error(f"Failed to convert PEM to PKCS#7: {e}")
        raise ValueError("Failed to create PKCS#7 bundle")


def validate_cert_chain(cert_pem, ca_bundle_path):
    """
    Validate certificate chain against CA bundle
    
    Args:
        cert_pem: PEM-encoded certificate
        ca_bundle_path: Path to CA bundle file
    
    Returns:
        bool: True if valid, False otherwise
    """
    if isinstance(cert_pem, str):
        cert_pem = cert_pem.encode('ascii')
    
    try:
        # Load the certificate to verify
        cert = x509.load_pem_x509_certificate(cert_pem, default_backend())
        
        # Load CA bundle
        with open(ca_bundle_path, 'rb') as f:
            ca_data = f.read()
        
        # Parse CA certificates
        ca_certs = []
        ca_data_str = ca_data.decode('ascii')
        current_cert = []
        in_cert = False
        
        for line in ca_data_str.split('\n'):
            if '-----BEGIN CERTIFICATE-----' in line:
                in_cert = True
                current_cert = [line]
            elif '-----END CERTIFICATE-----' in line:
                current_cert.append(line)
                ca_cert_pem = '\n'.join(current_cert).encode('ascii')
                ca_cert = x509.load_pem_x509_certificate(ca_cert_pem, default_backend())
                ca_certs.append(ca_cert)
                current_cert = []
                in_cert = False
            elif in_cert:
                current_cert.append(line)
        
        # Simple validation: check if cert's issuer matches any CA's subject
        # This is a simplified validation - full chain validation is complex
        for ca_cert in ca_certs:
            if cert.issuer == ca_cert.subject:
                return True
        
        return False
    except Exception:
        return False


def split_cert_and_key(pem_data):
    """
    Split combined PEM data into certificate chain and private key
    
    Args:
        pem_data: Combined PEM data containing cert(s) and key
    
    Returns:
        tuple: (cert_pem, key_pem) or (cert_pem, None) if no key
        Note: cert_pem may contain multiple certificates (full chain)
    """
    if isinstance(pem_data, bytes):
        pem_data = pem_data.decode('ascii')
    
    cert_pem = None
    key_pem = None
    
    # Extract ALL certificates (maintain full chain)
    if '-----BEGIN CERTIFICATE-----' in pem_data:
        cert_blocks = []
        search_start = 0
        
        while True:
            try:
                start = pem_data.index('-----BEGIN CERTIFICATE-----', search_start)
                end = pem_data.index('-----END CERTIFICATE-----', start) + len('-----END CERTIFICATE-----')
                cert_blocks.append(pem_data[start:end])
                search_start = end
            except ValueError:
                # No more certificates found
                break
        
        if cert_blocks:
            # Join all certificates with newlines to preserve chain
            cert_pem = '\n'.join(cert_blocks)
    
    # Extract private key (multiple formats)
    for key_header in ['-----BEGIN PRIVATE KEY-----', 
                       '-----BEGIN RSA PRIVATE KEY-----',
                       '-----BEGIN EC PRIVATE KEY-----']:
        if key_header in pem_data:
            key_footer = key_header.replace('BEGIN', 'END')
            start = pem_data.index(key_header)
            end = pem_data.index(key_footer) + len(key_footer)
            key_pem = pem_data[start:end]
            break
    
    return cert_pem, key_pem


def extract_ca_certs_to_bundle(cert_data, ca_bundle_path):
    """
    Extract CA certificates from a certificate chain and add them to ca-bundle.pem
    
    This function:
    1. Splits the certificate chain into individual certificates
    2. Identifies CA certificates by checking basicConstraints extension
    3. Calculates SHA-256 fingerprints to detect duplicates
    4. Appends new CA certificates to the bundle
    
    Args:
        cert_data: Certificate chain as bytes or string (may contain multiple certs)
        ca_bundle_path: Path object pointing to ca-bundle.pem
    
    Returns:
        int: Number of CA certificates added
    """
    if isinstance(cert_data, bytes):
        cert_data = cert_data.decode('ascii')
    
    # Split the chain into individual certificates
    certs = []
    current_cert = []
    in_cert = False
    
    for line in cert_data.split('\n'):
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
    
    if not certs:
        logger.warning("No certificates found in provided data")
        return 0
    
    # Load existing CA bundle fingerprints to avoid duplicates
    existing_fingerprints = set()
    if ca_bundle_path.exists():
        try:
            with open(ca_bundle_path, 'r') as f:
                bundle_data = f.read()
            
            # Split existing bundle into certs
            existing_certs = []
            current_cert = []
            in_cert = False
            
            for line in bundle_data.split('\n'):
                if '-----BEGIN CERTIFICATE-----' in line:
                    in_cert = True
                    current_cert = [line]
                elif '-----END CERTIFICATE-----' in line:
                    current_cert.append(line)
                    existing_certs.append('\n'.join(current_cert))
                    current_cert = []
                    in_cert = False
                elif in_cert:
                    current_cert.append(line)
            
            # Get fingerprints of existing certs
            for cert_pem in existing_certs:
                try:
                    cert = x509.load_pem_x509_certificate(cert_pem.encode('ascii'), default_backend())
                    fingerprint = cert.fingerprint(hashes.SHA256())
                    fingerprint_hex = ':'.join(f'{b:02X}' for b in fingerprint)
                    existing_fingerprints.add(fingerprint_hex)
                except Exception:
                    continue
        except Exception as e:
            logger.warning(f"Could not read existing CA bundle: {e}")
    
    # Process each certificate in the chain
    added_count = 0
    for cert_pem in certs:
        try:
            # Load certificate
            cert = x509.load_pem_x509_certificate(cert_pem.encode('ascii'), default_backend())
            
            # Check if this is a CA certificate
            is_ca = False
            try:
                basic_constraints = cert.extensions.get_extension_for_oid(ExtensionOID.BASIC_CONSTRAINTS)
                is_ca = basic_constraints.value.ca
            except x509.ExtensionNotFound:
                is_ca = False
            
            if not is_ca:
                logger.debug("Certificate is not a CA, skipping")
                continue
            
            # Get fingerprint
            fingerprint = cert.fingerprint(hashes.SHA256())
            fingerprint_hex = ':'.join(f'{b:02X}' for b in fingerprint)
            
            # Check if already in bundle
            if fingerprint_hex in existing_fingerprints:
                logger.debug(f"CA certificate with fingerprint {fingerprint_hex} already in bundle")
                continue
            
            # Get CA subject for logging
            subject_parts = []
            for attr in cert.subject:
                subject_parts.append(f"{attr.rfc4514_attribute_name}={attr.value}")
            subject = ','.join(subject_parts)
            
            # Append to CA bundle
            ca_bundle_path.parent.mkdir(parents=True, exist_ok=True)
            with open(ca_bundle_path, 'a') as f:
                if ca_bundle_path.stat().st_size > 0:
                    f.write('\n')
                f.write(cert_pem)
                if not cert_pem.endswith('\n'):
                    f.write('\n')
            
            existing_fingerprints.add(fingerprint_hex)
            added_count += 1
            logger.info(f"Added CA certificate to bundle: {subject}, fingerprint: {fingerprint_hex}")
            
        except Exception as e:
            logger.warning(f"Failed to process certificate: {e}")
            continue
    
    return added_count


def validate_url(url):
    """
    Validate EST server URL
    
    Returns:
        tuple: (is_valid, error_message)
    """
    try:
        parsed = urlparse(url)
        if parsed.scheme != 'https':
            return False, "EST server must use HTTPS"
        if not parsed.netloc:
            return False, "Invalid EST server URL"
        return True, None
    except Exception as e:
        return False, f"Invalid URL: {str(e)}"


def fetch_est_cacerts(est_server, ca_bundle, idevid_cert, idevid_key):
    """
    Fetch CA certificates from EST server
    
    Args:
        est_server: EST server base URL (e.g., https://est.example.com)
        ca_bundle: Path to CA bundle for server verification
        idevid_cert: Path to IDevID certificate for client authentication
        idevid_key: Path to IDevID key for client authentication
    
    Returns:
        str: PEM-formatted CA certificates
    """
    url = f"{est_server}/.well-known/est/cacerts"
    
    try:
        # Use IDevID for client authentication and CA bundle for server verification
        response = requests.get(
            url,
            cert=(str(idevid_cert), str(idevid_key)),
            verify=str(ca_bundle),
            timeout=EST_TIMEOUT
        )
        response.raise_for_status()  # Raises HTTPError for 4xx/5xx responses
        base64_response = response.text
        
    except requests.exceptions.RequestException as e:
        raise Exception(f"Failed to fetch CA certs from EST server: {e}")
    
    # EST /cacerts returns base64-encoded PKCS#7
    # Decode and extract certificates
    try:
        # Decode base64
        pkcs7_der = base64.b64decode(base64_response)
        
        # Load and extract certificates from PKCS#7
        certs = pkcs7.load_der_pkcs7_certificates(pkcs7_der)
        
        # Convert certificates back to PEM
        pem_certs = []
        for cert in certs:
            pem_cert = cert.public_bytes(serialization.Encoding.PEM).decode('ascii')
            pem_certs.append(pem_cert)
        
        return ''.join(pem_certs)
        
    except Exception as e:
        raise Exception(f"Failed to process EST CA certs: {e}")


def submit_csr_to_est(est_server, csr_pem, ca_bundle, idevid_cert, idevid_key):
    """
    Submit CSR to EST server simpleenroll endpoint
    
    Args:
        est_server: EST server base URL
        csr_pem: PEM-formatted CSR
        ca_bundle: Path to CA bundle for server verification
        idevid_cert: Path to IDevID certificate for client authentication
        idevid_key: Path to IDevID key for client authentication
    
    Returns:
        str: PEM-formatted issued certificate
    """
    url = f"{est_server}/.well-known/est/simpleenroll"
    
    # EST requires base64-encoded CSR in request body
    # Extract just the base64 part (without headers)
    csr_b64 = ''.join(
        line for line in csr_pem.split('\n')
        if line and not line.startswith('-----')
    )
    
    try:
        # Submit CSR to EST server
        response = requests.post(
            url,
            data=csr_b64,
            cert=(str(idevid_cert), str(idevid_key)),
            verify=str(ca_bundle),
            headers={
                'Content-Type': 'application/pkcs10',
                'Content-Transfer-Encoding': 'base64'
            },
            timeout=EST_TIMEOUT
        )
        response.raise_for_status()
        base64_response = response.text
        
    except requests.exceptions.RequestException as e:
        raise Exception(f"Failed to submit CSR to EST server: {e}")
    
    # EST /simpleenroll returns base64-encoded PKCS#7 with the issued certificate
    # Decode and extract certificate
    try:
        # Decode base64
        pkcs7_der = base64.b64decode(base64_response)
        
        # Load and extract certificate from PKCS#7
        certs = pkcs7.load_der_pkcs7_certificates(pkcs7_der)
        
        # Convert certificate to PEM
        if certs:
            pem_cert = certs[0].public_bytes(serialization.Encoding.PEM).decode('ascii')
            return pem_cert
        else:
            raise Exception("No certificate found in PKCS#7 response")
        
    except Exception as e:
        raise Exception(f"Failed to process EST certificate: {e}")


def generate_csr_for_est(ldevid_key_path):
    """
    Generate CSR for LDevID with hostname.local as SAN
    
    Args:
        ldevid_key_path: Path where the private key will be stored
    
    Returns:
        str: PEM-formatted CSR
    """
    # Get hostname
    hostname = socket.gethostname()
    san = f"{hostname}.local"
    
    # Generate private key
    private_key = ec.generate_private_key(ec.SECP256R1(), default_backend())
    
    # Save private key
    ldevid_key_path.parent.mkdir(parents=True, exist_ok=True)
    key_pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.TraditionalOpenSSL,
        encryption_algorithm=serialization.NoEncryption()
    )
    with open(ldevid_key_path, 'wb') as f:
        f.write(key_pem)
    ldevid_key_path.chmod(0o600)
    
    # Build CSR with SAN
    subject = x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, hostname)
    ])
    
    # Add SAN extension
    san_extension = x509.SubjectAlternativeName([
        x509.DNSName(san)
    ])
    
    # Generate CSR
    csr = x509.CertificateSigningRequestBuilder().subject_name(
        subject
    ).add_extension(
        san_extension,
        critical=False
    ).sign(private_key, hashes.SHA256(), default_backend())
    
    # Convert to PEM
    csr_pem = csr.public_bytes(serialization.Encoding.PEM).decode('ascii')
    
    return csr_pem
