#!/bin/bash
#
# Source this file to set up environment variables for manual Bruno testing
# Usage: source ./setup-sec-enroll-test-env.sh
#        OR
#        . ./setup-sec-enroll-test-env.sh
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Test certificates directory
export TEST_CERTS_DIR="$(cd "$SCRIPT_DIR/../../traefik/test-certs" && pwd)"

# Check if test certificates exist
if [ ! -d "$TEST_CERTS_DIR" ]; then
    echo "Error: Test certificates directory not found at: $TEST_CERTS_DIR"
    echo "Run: cd ../../traefik && sudo ./createcert.sh"
    exit 1
fi

# Detect mDNS hostname from OpenThread
if command -v ot-ctl >/dev/null 2>&1; then
    MDNS_HOSTNAME=$(ot-ctl mdns localhostname 2>/dev/null | grep -v "Done" | grep -v "Error" | tr -d '[:space:]' | head -1 || true)
    if [ -z "$MDNS_HOSTNAME" ]; then
        MDNS_HOSTNAME=$(hostname)
        echo "Warning: Could not get mDNS hostname, using system hostname: ${MDNS_HOSTNAME}"
    else
        echo "Detected OpenThread mDNS hostname: ${MDNS_HOSTNAME}"
    fi
else
    MDNS_HOSTNAME=$(hostname)
    echo "Warning: ot-ctl not found, using system hostname: ${MDNS_HOSTNAME}"
fi

export MDNS_HOSTNAME

# Read randomized initial admin password (written by switch-config.sh basicauth default-state)
INITIAL_PASS_FILE="/var/lib/traefik/auth/initial-password"
if [ -f "$INITIAL_PASS_FILE" ]; then
    export INITIAL_ADMIN_PASS
    INITIAL_ADMIN_PASS=$(cat "$INITIAL_PASS_FILE")
else
    echo "Error: Initial password file not found at $INITIAL_PASS_FILE"
    echo "  Run: sudo ./traefik/switch-config.sh basicauth default-state"
    exit 1
fi

# Generate a random new password per test run for the password-change tests.
# Satisfies API policy: min 12 chars, requires uppercase, lowercase, digit, special char.
# Guard: if already set (e.g. when sourced by a sub-script), keep the existing value so
# the password in Traefik's htpasswd stays consistent across the whole test session.
if [ -z "${NEW_ADMIN_PASS:-}" ]; then
    export NEW_ADMIN_PASS
    NEW_ADMIN_PASS=$(python3 -c "
import secrets, string
upper = string.ascii_uppercase
lower = string.ascii_lowercase
digits = string.digits
special = '!@#\$%^&*'
chars = upper + lower + digits + special
p = [secrets.choice(upper), secrets.choice(lower), secrets.choice(digits), secrets.choice(special)]
p += [secrets.choice(chars) for _ in range(12)]
import random; rng = random.SystemRandom(); rng.shuffle(p)
print(''.join(p))
")
fi

# Load client certificate for mTLS tests (if exists)
if [ -f "$TEST_CERTS_DIR/clients/client-chain.pem" ]; then
    export CLIENT_CERT_PEM=$(cat "$TEST_CERTS_DIR/clients/client-chain.pem")
    # Calculate certificate ID (first 32 chars of fingerprint)
    export CLIENT_CERT_ID=$(openssl x509 -in "$TEST_CERTS_DIR/clients/client-chain.pem" -noout -fingerprint -sha256 2>/dev/null | cut -d= -f2 | tr -d ':' | tr '[:upper:]' '[:lower:]' | head -c 32)
fi
# Load client2 certificate for mTLS tests (if exists)
if [ -f "$TEST_CERTS_DIR/clients/client2-chain.pem" ]; then
    export CLIENT2_CERT_PEM=$(cat "$TEST_CERTS_DIR/clients/client2-chain.pem")
    # Calculate certificate ID (first 32 chars of fingerprint)
    export CLIENT2_CERT_ID=$(openssl x509 -in "$TEST_CERTS_DIR/clients/client2-chain.pem" -noout -fingerprint -sha256 2>/dev/null | cut -d= -f2 | tr -d ':' | tr '[:upper:]' '[:lower:]' | head -c 32)
fi

# Generate Bruno CLI client certificate configuration files
# (Bruno CLI requires literal paths, not environment variables)
echo "Generating Bruno client certificate configuration files..."

cat >"$SCRIPT_DIR/client-cert-config.json" <<EOF
{
  "enabled": true,
  "certs": [
    {
      "domain": "${MDNS_HOSTNAME}.local",
      "certFilePath": "${TEST_CERTS_DIR}/clients/client-chain.pem",
      "keyFilePath": "${TEST_CERTS_DIR}/clients/client-key.pem"
    }
  ]
}
EOF

cat >"$SCRIPT_DIR/client2-cert-config.json" <<EOF
{
  "enabled": true,
  "certs": [
    {
      "domain": "${MDNS_HOSTNAME}.local",
      "certFilePath": "${TEST_CERTS_DIR}/clients/client2-chain.pem",
      "keyFilePath": "${TEST_CERTS_DIR}/clients/client2-key.pem"
    }
  ]
}
EOF

echo "  ✓ Generated client-cert-config.json"
echo "  ✓ Generated client2-cert-config.json"

echo ""
echo "Environment variables set:"
echo "  TEST_CERTS_DIR=$TEST_CERTS_DIR"
echo "  MDNS_HOSTNAME=$MDNS_HOSTNAME"
echo "  INITIAL_ADMIN_PASS=<set>"
echo "  NEW_ADMIN_PASS=<set>"
echo "  CLIENT_CERT_PEM=${CLIENT_CERT_PEM:+<set>}"
echo "  CLIENT_CERT_ID=${CLIENT_CERT_ID:+$CLIENT_CERT_ID}"
echo "  CLIENT2_CERT_PEM=${CLIENT2_CERT_PEM:+<set>}"
echo "  CLIENT2_CERT_ID=${CLIENT2_CERT_ID:+$CLIENT2_CERT_ID}"
echo ""
echo "You can now run Bruno commands manually:"
echo '  bru run workflows/01-discovery/ --env baseline --cacert ${TEST_CERTS_DIR}/ca-bundle.pem'
echo '  bru run workflows/02-basic-auth/ --env initial-state --cacert ${TEST_CERTS_DIR}/ca-bundle.pem'
echo ""
