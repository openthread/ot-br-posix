#!/bin/bash
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
# Create demonstration certificates for OpenThread Border Router Secure Enrollment
# Note that these are demo certificates and should not be used in production.
#
# Uses paths from paths.env for all directory locations.
#
# Certificate strategy:
# - Generates ALL certificates in local test-certs/ directory (includes CA private keys)
# - Installs minimal certificates to system installation:
#   * CA public certificates (root-ca-cert, issuing-ca-cert) - NO private keys
#   * IDevID cert + key (factory default device identity)
#   * Active cert + key (initially = IDevID, updated during enrollment)
#   * CA bundle (for trust validation)
#   * NO client certificates initially (added during enrollment)
#   * NO LDevID initially (created during enrollment)
# - CA private keys remain ONLY in test-certs/ for local testing/signing
#
# This script creates:
# - Root CA (manufacturer root CA)
# - Issuing CA (manufacturer issuing CA)
# - IDevID certificate (factory-installed device identity)
# - LDevID certificate (operational device identity)
# - Client certificates (for mutual TLS testing)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source unified path configuration
PATHS_ENV="${SCRIPT_DIR}/wsgi_app/paths.env"
if [ -f "$PATHS_ENV" ]; then
    source "$PATHS_ENV"
    echo "Using paths from: $PATHS_ENV"
else
    echo "Error: paths.env not found at $PATHS_ENV"
    exit 1
fi

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root (use sudo)"
    echo "Usage: sudo ./createcert.sh"
    exit 1
fi

# Temporary directory for certificate generation (all certs generated here first)
TEST_CERTS_DIR="${SCRIPT_DIR}/test-certs"

# System installation paths (minimal - no CA private keys, no client certs, no ldevid)
SYSTEM_CERTS_DIR="${TRAEFIK_DATA_DIR}/certs"
AUTH_DIR="${TRAEFIK_DATA_DIR}/auth"
TRAEFIK_USER="${TRAEFIK_USER:-traefik}"
TRAEFIK_GROUP="${TRAEFIK_GROUP:-traefik}"

echo "=========================================="
echo "Certificate Generation"
echo "=========================================="
echo "Test certs directory:  $TEST_CERTS_DIR"
echo "System certs directory: $SYSTEM_CERTS_DIR"
echo "Auth directory:        $AUTH_DIR"
echo "=========================================="
echo ""

# Check if certs already exist
if [ -f "${SYSTEM_CERTS_DIR}/root-ca-cert.pem" ]; then
    echo "====================================="
    echo "Certificates already exist in ${SYSTEM_CERTS_DIR}"
    echo "Skipping certificate creation."
    echo "To regenerate certificates, remove the certificate directories first:"
    echo "  sudo rm -rf ${SYSTEM_CERTS_DIR}/* ${TEST_CERTS_DIR}/*"
    echo "====================================="
    exit 0
fi

# Create directories
mkdir -p "${TEST_CERTS_DIR}/clients"
mkdir -p "${SYSTEM_CERTS_DIR}/clients"
mkdir -p "${AUTH_DIR}"

HOSTNAME=$(hostname)
SERIAL_NO="SN-$(openssl rand -hex 16)"

echo "Creating demonstration certificates for secure enrollment..."
echo "Hostname: ${HOSTNAME}"
echo "Serial Number: ${SERIAL_NO}"
echo ""

# 1. Create Root CA (Manufacturer Root CA)
echo "====================================="
echo "1. Creating Root CA..."
echo "====================================="
openssl ecparam -genkey -name secp384r1 -out "${TEST_CERTS_DIR}/root-ca-key.pem"
openssl req -x509 -new -nodes \
    -key "${TEST_CERTS_DIR}/root-ca-key.pem" \
    -sha384 -days 7300 \
    -out "${TEST_CERTS_DIR}/root-ca-cert.pem" \
    -subj "/C=CH/O=OpenThread/CN=Demo Root CA 2026"

# 2. Create Issuing CA (Manufacturer Issuing CA)
echo "====================================="
echo "2. Creating Issuing CA..."
echo "====================================="
openssl ecparam -genkey -name secp256r1 -out "${TEST_CERTS_DIR}/issuing-ca-key.pem"
openssl req -new \
    -key "${TEST_CERTS_DIR}/issuing-ca-key.pem" \
    -out "${TEST_CERTS_DIR}/issuing-ca.csr" \
    -subj "/C=CH/O=OpenThread/CN=Demo Issuing CA 2026"

# Sign Issuing CA with Root CA
openssl x509 -req -in "${TEST_CERTS_DIR}/issuing-ca.csr" \
    -CA "${TEST_CERTS_DIR}/root-ca-cert.pem" \
    -CAkey "${TEST_CERTS_DIR}/root-ca-key.pem" \
    -CAcreateserial -out "${TEST_CERTS_DIR}/issuing-ca-cert.pem" \
    -days 3650 -sha384 \
    -extensions v3_ca -extfile <(
        cat <<EOF
[v3_ca]
basicConstraints = critical,CA:TRUE,pathlen:0
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
EOF
    )

# 3. Create IDevID certificate (Initial Device Identity - ex-factory)
echo "====================================="
echo "3. Creating IDevID certificate..."
echo "====================================="
openssl ecparam -genkey -name secp256r1 -out "${TEST_CERTS_DIR}/idevid-key.pem"
openssl req -new \
    -key "${TEST_CERTS_DIR}/idevid-key.pem" \
    -out "${TEST_CERTS_DIR}/idevid.csr" \
    -subj "/C=CH/O=OpenThread/serialNumber=${SERIAL_NO}/CN=${HOSTNAME}-idevid"

openssl x509 -req -in "${TEST_CERTS_DIR}/idevid.csr" \
    -CA "${TEST_CERTS_DIR}/issuing-ca-cert.pem" \
    -CAkey "${TEST_CERTS_DIR}/issuing-ca-key.pem" \
    -CAcreateserial -out /tmp/idevid-cert-temp.pem \
    -days 7300 -sha256 \
    -extensions v3_idevid -extfile <(
        cat <<EOF
[v3_idevid]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth,clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
subjectAltName = DNS:${HOSTNAME}.local,DNS:${HOSTNAME},DNS:localhost
# IEEE 802.1AR IDevID OID
1.3.6.1.4.1.46384.1.1 = ASN1:UTF8String:IDevID
EOF
    )

# Create chain files (cert + issuing CA + root CA)
cat /tmp/idevid-cert-temp.pem "${TEST_CERTS_DIR}/issuing-ca-cert.pem" "${TEST_CERTS_DIR}/root-ca-cert.pem" >"${TEST_CERTS_DIR}/idevid-chain.pem"
cp /tmp/idevid-cert-temp.pem "${TEST_CERTS_DIR}/idevid-cert.pem"
rm /tmp/idevid-cert-temp.pem

# 4. Create LDevID certificate (Locally significant Device Identity - operational)
echo "====================================="
echo "4. Creating LDevID certificate..."
echo "====================================="
openssl ecparam -genkey -name secp256r1 -out "${TEST_CERTS_DIR}/ldevid-key.pem"
openssl req -new \
    -key "${TEST_CERTS_DIR}/ldevid-key.pem" \
    -out "${TEST_CERTS_DIR}/ldevid.csr" \
    -subj "/C=CH/O=OpenThread/serialNumber=${SERIAL_NO}/CN=${HOSTNAME}-ldevid"

openssl x509 -req -in "${TEST_CERTS_DIR}/ldevid.csr" \
    -CA "${TEST_CERTS_DIR}/issuing-ca-cert.pem" \
    -CAkey "${TEST_CERTS_DIR}/issuing-ca-key.pem" \
    -CAcreateserial -out /tmp/ldevid-cert-temp.pem \
    -days 1095 -sha256 \
    -extensions v3_ldevid -extfile <(
        cat <<EOF
[v3_ldevid]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth,clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
subjectAltName = DNS:${HOSTNAME}.local,DNS:${HOSTNAME},DNS:localhost
# IEEE 802.1AR LDevID OID
1.3.6.1.4.1.46384.1.2 = ASN1:UTF8String:LDevID
EOF
    )

# Create chain files
cat /tmp/ldevid-cert-temp.pem "${TEST_CERTS_DIR}/issuing-ca-cert.pem" "${TEST_CERTS_DIR}/root-ca-cert.pem" >"${TEST_CERTS_DIR}/ldevid-chain.pem"
cp /tmp/ldevid-cert-temp.pem "${TEST_CERTS_DIR}/ldevid-cert.pem"
rm /tmp/ldevid-cert-temp.pem

# 5. Create client certificates for testing
echo "====================================="
echo "5. Creating client certificates..."
echo "====================================="

# Client 1
openssl ecparam -genkey -name secp256r1 -out "${TEST_CERTS_DIR}/clients/client-key.pem"
openssl req -new \
    -key "${TEST_CERTS_DIR}/clients/client-key.pem" \
    -out "${TEST_CERTS_DIR}/clients/client.csr" \
    -subj "/C=CH/O=OpenThread/CN=rest-api-client-demo"

openssl x509 -req -in "${TEST_CERTS_DIR}/clients/client.csr" \
    -CA "${TEST_CERTS_DIR}/issuing-ca-cert.pem" \
    -CAkey "${TEST_CERTS_DIR}/issuing-ca-key.pem" \
    -CAcreateserial -out /tmp/client-cert-temp.pem \
    -days 1095 -sha256 \
    -extensions v3_client -extfile <(
        cat <<EOF
[v3_client]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
subjectAltName = DNS:rest-api-client-demo.local
EOF
    )

cat /tmp/client-cert-temp.pem "${TEST_CERTS_DIR}/issuing-ca-cert.pem" "${TEST_CERTS_DIR}/root-ca-cert.pem" >"${TEST_CERTS_DIR}/clients/client-chain.pem"
rm /tmp/client-cert-temp.pem

# Client 2
openssl ecparam -genkey -name secp256r1 -out "${TEST_CERTS_DIR}/clients/client2-key.pem"
openssl req -new \
    -key "${TEST_CERTS_DIR}/clients/client2-key.pem" \
    -out "${TEST_CERTS_DIR}/clients/client2.csr" \
    -subj "/C=CH/O=OpenThread/CN=rest-api-client2-demo"

openssl x509 -req -in "${TEST_CERTS_DIR}/clients/client2.csr" \
    -CA "${TEST_CERTS_DIR}/issuing-ca-cert.pem" \
    -CAkey "${TEST_CERTS_DIR}/issuing-ca-key.pem" \
    -CAcreateserial -out /tmp/client2-cert-temp.pem \
    -days 1095 -sha256 \
    -extensions v3_client -extfile <(
        cat <<EOF
[v3_client]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
subjectAltName = DNS:rest-api-client2-demo.local
EOF
    )

cat /tmp/client2-cert-temp.pem "${TEST_CERTS_DIR}/issuing-ca-cert.pem" "${TEST_CERTS_DIR}/root-ca-cert.pem" >"${TEST_CERTS_DIR}/clients/client2-chain.pem"
rm /tmp/client2-cert-temp.pem

# Fix ownership of client certificates so non-root user can use them for testing
if [ -n "$SUDO_USER" ] && [ "$SUDO_USER" != "root" ]; then
    chown -R "$SUDO_USER:$SUDO_USER" "${TEST_CERTS_DIR}/clients"
    chmod 644 "${TEST_CERTS_DIR}/clients/"*.pem
    chmod 600 "${TEST_CERTS_DIR}/clients/"*-key.pem
    echo "Client certificates ownership set to $SUDO_USER"
fi

# Client 3 (self-signed, for testing untrusted client)
openssl ecparam -genkey -name secp256r1 -out "${TEST_CERTS_DIR}/clients/client3-key.pem"
openssl req -new \
    -key "${TEST_CERTS_DIR}/clients/client3-key.pem" \
    -out "${TEST_CERTS_DIR}/clients/client3.csr" \
    -subj "/C=CH/O=OpenThread/CN=rest-api-client3-demo"

openssl x509 -req -in "${TEST_CERTS_DIR}/clients/client3.csr" \
    -signkey "${TEST_CERTS_DIR}/clients/client3-key.pem" \
    -out "${TEST_CERTS_DIR}/clients/client3-cert.pem" \
    -days 1095 -sha256 \
    -extensions v3_client -extfile <(
        cat <<EOF
[v3_client]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
subjectAltName = DNS:rest-api-client3-demo.local
EOF
    )

cat "${TEST_CERTS_DIR}/clients/client3-cert.pem" >"${TEST_CERTS_DIR}/clients/client3-chain.pem"
rm "${TEST_CERTS_DIR}/clients/client3-cert.pem"

# Fix ownership of client certificates so non-root user can use them for testing
if [ -n "$SUDO_USER" ] && [ "$SUDO_USER" != "root" ]; then
    chown -R "$SUDO_USER:$SUDO_USER" "${TEST_CERTS_DIR}/clients"
    chmod 644 "${TEST_CERTS_DIR}/clients/"*.pem
    chmod 600 "${TEST_CERTS_DIR}/clients/"*-key.pem
    echo "Client certificates ownership set to $SUDO_USER"
fi

# 6. Create CA bundle
echo "====================================="
echo "6. Creating CA bundle..."
echo "====================================="
# Starts with factory root CA and issuing CA
cat "${TEST_CERTS_DIR}/issuing-ca-cert.pem" "${TEST_CERTS_DIR}/root-ca-cert.pem" >"${TEST_CERTS_DIR}/ca-bundle.pem"
echo "CA bundle created in ${TEST_CERTS_DIR}/ca-bundle.pem"

# 7. Copy minimal certificates to system installation
echo "====================================="
echo "7. Installing minimal certificates to system..."
echo "====================================="
# Copy only public certificates needed for trust (NO private keys for CAs)
cp "${TEST_CERTS_DIR}/root-ca-cert.pem" "${SYSTEM_CERTS_DIR}/"
cp "${TEST_CERTS_DIR}/issuing-ca-cert.pem" "${SYSTEM_CERTS_DIR}/"
cp "${TEST_CERTS_DIR}/ca-bundle.pem" "${SYSTEM_CERTS_DIR}/"

# Copy IDevID (factory default) - this is the only device cert in system initially
cp "${TEST_CERTS_DIR}/idevid-chain.pem" "${SYSTEM_CERTS_DIR}/"
cp "${TEST_CERTS_DIR}/idevid-cert.pem" "${SYSTEM_CERTS_DIR}/"
cp "${TEST_CERTS_DIR}/idevid-key.pem" "${SYSTEM_CERTS_DIR}/"

# Create active-chain and active-key as copies of IDevID (ex-factory state)
cp "${TEST_CERTS_DIR}/idevid-chain.pem" "${SYSTEM_CERTS_DIR}/active-chain.pem"
cp "${TEST_CERTS_DIR}/idevid-key.pem" "${SYSTEM_CERTS_DIR}/active-key.pem"

echo "System installation has:"
echo "  - Root CA cert (public only)"
echo "  - Issuing CA cert (public only)"
echo "  - CA bundle"
echo "  - IDevID cert + key (factory default)"
echo "  - Active cert + key (initially = IDevID)"
echo "  - NO CA private keys"
echo "  - NO client certificates (added during enrollment)"
echo "  - NO LDevID (created during enrollment)"

# 8. Create default htpasswd file for basic authentication
echo "====================================="
echo "8. Creating default authentication file..."
echo "====================================="
# Default password: admin:thread
# Generated with: openssl passwd -apr1 thread
echo 'admin:$apr1$H6uskkkW$C.uE1YX.y.goACkMVBsIK0' >"${AUTH_DIR}/htpasswd"

# 9. Set permissions
echo "====================================="
echo "9. Setting permissions..."
echo "====================================="

# Test certs directory (local, for testing)
chmod 755 "${TEST_CERTS_DIR}"
chmod 644 "${TEST_CERTS_DIR}"/*.pem 2>/dev/null || true
chmod 600 "${TEST_CERTS_DIR}"/*-key.pem 2>/dev/null || true
chmod 755 "${TEST_CERTS_DIR}/clients"
chmod 644 "${TEST_CERTS_DIR}/clients"/*.pem 2>/dev/null || true
chmod 600 "${TEST_CERTS_DIR}/clients"/*-key.pem 2>/dev/null || true

# System certs directory (minimal, runtime)
chown -R "$TRAEFIK_USER:$TRAEFIK_GROUP" "${SYSTEM_CERTS_DIR}"
chmod 755 "${SYSTEM_CERTS_DIR}"
chmod 644 "${SYSTEM_CERTS_DIR}"/*.pem 2>/dev/null || true
chmod 600 "${SYSTEM_CERTS_DIR}"/*-key.pem
chmod 755 "${SYSTEM_CERTS_DIR}/clients"

# Auth directory
chown "$TRAEFIK_USER:$TRAEFIK_GROUP" "${AUTH_DIR}/htpasswd"
chmod 644 "${AUTH_DIR}/htpasswd"

# Test certs directory - owned by user who ran sudo for local testing/signing
if [ -n "$SUDO_USER" ]; then
    chown -R "$SUDO_USER:$SUDO_USER" "${TEST_CERTS_DIR}"
    echo "Test certificates owned by: $SUDO_USER:$SUDO_USER"
fi

echo ""
echo "=========================================="
echo "Certificate Generation Complete!"
echo "=========================================="
echo ""
echo "Test certificates (local - for signing/testing):"
echo "  Location:     ${TEST_CERTS_DIR}/"
echo "  Root CA:      root-ca-cert.pem + root-ca-key.pem"
echo "  Issuing CA:   issuing-ca-cert.pem + issuing-ca-key.pem"
echo "  IDevID:       idevid-cert.pem + idevid-key.pem + idevid-chain.pem"
echo "  LDevID:       ldevid-cert.pem + ldevid-key.pem + ldevid-chain.pem"
echo "  Clients:      clients/client-chain.pem + client-key.pem"
echo "                clients/client2-chain.pem + client2-key.pem"
echo "  CA Bundle:    ca-bundle.pem"
echo ""
echo "System installation (minimal - production-ready):"
echo "  Location:     ${SYSTEM_CERTS_DIR}/"
echo "  Root CA:      root-ca-cert.pem (public cert only)"
echo "  Issuing CA:   issuing-ca-cert.pem (public cert only)"
echo "  IDevID:       idevid-cert.pem + idevid-key.pem + idevid-chain.pem"
echo "  Active:       active-chain.pem + active-key.pem (initially = IDevID)"
echo "  CA Bundle:    ca-bundle.pem"
echo "  Clients:      clients/ (empty - populated during enrollment)"
echo ""
echo "Authentication:"
echo "  Htpasswd:     ${AUTH_DIR}/htpasswd"
echo "  Default:      admin:thread"
echo ""
echo "Security notes:"
echo "  - CA private keys (root-ca-key, issuing-ca-key) are NOT in system"
echo "  - CA keys remain in test-certs/ for local testing only"
echo "  - Client certificates added via enrollment API"
echo "  - LDevID created during enrollment process"
echo ""
echo "Certificate workflow:"
echo "  1. Device ships with IDevID (ex-factory) - active cert"
echo "  2. Enrollment generates/installs LDevID - becomes active cert"
echo "  3. Client certificates added via PUT /api/auth/certs"
echo "  4. EST enrollment can add new CAs to bundle"
echo ""
echo "Ownership:"
echo "  Test certs:  Current user (for local testing)"
echo "  System:      ${TRAEFIK_USER}:${TRAEFIK_GROUP} (runtime updates enabled)"
echo ""
