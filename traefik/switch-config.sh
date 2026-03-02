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
#
# Switch Traefik dynamic configuration between authentication modes
# System installation only - uses systemd services
# Usage: sudo ./switch-config.sh <mode> [default-state]
# Where mode is: mtls, basicauth, or backup
# and "default-state" as second argument will restore defaults after switching configuration
#
# This is a wrapper that calls the Python implementation to ensure
# consistent behavior with the CGI enrollment endpoint.

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Delegate to Python implementation for consistency (only pass first argument - the mode)
python3 "$SCRIPT_DIR/switch_traefik_config.py" "$1"

# Exit unless "default-state" is given as second argument
if [ "$2" != "default-state" ]; then
    exit 0
fi

# =============================================================================
# Restore default states before starting tests
# =============================================================================
# Source unified path configuration from working directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATHS_ENV="${SCRIPT_DIR}/wsgi_app/paths.env"
if [ -f "$PATHS_ENV" ]; then
    source "$PATHS_ENV"
else
    echo "Error: paths.env not found at $PATHS_ENV"
    exit 1
fi

# Build full paths from components
HTPASSWD_PATH="${TRAEFIK_DATA_DIR}/${AUTH_DIR}/${HTPASSWD_FILE}"
CLIENT_AUTHZ_PATH="${TRAEFIK_DATA_DIR}/${CERTS_DIR}/${CLIENT_CERTS_SUBDIR}/${CLIENT_AUTHZ}"
SYSTEM_CERTS_DIR="${TRAEFIK_DATA_DIR}/${CERTS_DIR}"

# Define test log file and colors
LOG_FILE="${SCRIPT_DIR}/test-enrollment.log"
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "\n${YELLOW}Restoring default state for test run...${NC}"
echo "[$(date -Iseconds)] Test Enrollment - Restoring defaults" >>"$LOG_FILE"

# Restore default password using openssl (matches auth.py implementation)
sudo -u traefik mkdir -p "$(dirname "$HTPASSWD_PATH")"

# Generate APR1 (MD5) hash for password "thread" using openssl
# APR1 is the standard htpasswd format and has maximum compatibility with Traefik
DEFAULT_HASH=$(openssl passwd -apr1 'thread')
echo "admin:${DEFAULT_HASH}" | sudo -u traefik tee "$HTPASSWD_PATH" >/dev/null

echo -e "${GREEN}✓ Password reset to default (admin:thread) at $HTPASSWD_PATH${NC}"
echo "[$(date -Iseconds)] Password reset to default (admin:thread)" >>"$LOG_FILE"

# Restore client-authz.json to empty state
sudo -u traefik rm -f "$CLIENT_AUTHZ_PATH"
sudo -u traefik mkdir -p "$(dirname "$CLIENT_AUTHZ_PATH")"
cat <<'EOF' | sudo -u traefik tee "$CLIENT_AUTHZ_PATH" >/dev/null
{"clients": {}}
EOF
echo -e "${GREEN}✓ Client authorization reset to empty state${NC}"
echo "[$(date -Iseconds)] Client authorization reset to empty state" >>"$LOG_FILE"

# Delete client certificate files with 32-char hex filenames (from previous test runs)
SYSTEM_CLIENT_CERTS_PATH="${TRAEFIK_DATA_DIR}/${CERTS_DIR}/${CLIENT_CERTS_SUBDIR}"
if [ -d "${SYSTEM_CLIENT_CERTS_PATH}" ]; then
    DELETED_COUNT=$(sudo -u traefik find "${SYSTEM_CLIENT_CERTS_PATH}" -type f -name '[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f].pem' -delete -print | wc -l)
    if [ "$DELETED_COUNT" -gt 0 ]; then
        echo -e "${GREEN}✓ Deleted $DELETED_COUNT client certificate file(s) with 32-char hex filenames${NC}"
        echo "[$(date -Iseconds)] Deleted $DELETED_COUNT client certificate file(s)" >>"$LOG_FILE"
    fi
fi

# Restore IDevID as active certificate (ex-factory state) on the system
SYSTEM_IDEVID_CHAIN_PATH="${SYSTEM_CERTS_DIR}/${IDEVID_CHAIN}"
SYSTEM_IDEVID_KEY_PATH="${SYSTEM_CERTS_DIR}/${IDEVID_KEY}"
SYSTEM_ACTIVE_CHAIN_PATH="${SYSTEM_CERTS_DIR}/${ACTIVE_CHAIN}"
SYSTEM_ACTIVE_KEY_PATH="${SYSTEM_CERTS_DIR}/${ACTIVE_KEY}"
if [ -f "${SYSTEM_IDEVID_CHAIN_PATH}" ] && [ -f "${SYSTEM_IDEVID_KEY_PATH}" ]; then
    sudo -u traefik cp "${SYSTEM_IDEVID_CHAIN_PATH}" "${SYSTEM_ACTIVE_CHAIN_PATH}"
    sudo -u traefik cp "${SYSTEM_IDEVID_KEY_PATH}" "${SYSTEM_ACTIVE_KEY_PATH}"
    sudo -u traefik chmod 644 "${SYSTEM_ACTIVE_CHAIN_PATH}"
    sudo -u traefik chmod 600 "${SYSTEM_ACTIVE_KEY_PATH}"
    echo -e "${GREEN}✓ Certificate restored to IDevID (ex-factory state)${NC}"
    echo "[$(date -Iseconds)] Certificate restored to IDevID (ex-factory state)" >>"$LOG_FILE"
fi

# Remove any LDevID files if present (from previous test runs)
DELETED_LDEVID_COUNT=$(sudo -u traefik find "${SYSTEM_CERTS_DIR}" -type f -name 'ldevid*' -delete -print | wc -l)
if [ "$DELETED_LDEVID_COUNT" -gt 0 ]; then
    echo -e "${GREEN}✓ Deleted $DELETED_LDEVID_COUNT LDevID file(s)${NC}"
    echo "[$(date -Iseconds)] Deleted $DELETED_LDEVID_COUNT LDevID file(s)" >>"$LOG_FILE"
fi

# Switch to basic auth configuration (from mTLS mode if it was active)
if sudo "${SCRIPT_DIR}/switch-config.sh" basicauth; then
    sleep 5 # Wait a moment for Traefik to restart
    echo -e "${GREEN}✓ Basic auth mode active and services running${NC}"
else
    echo -e "${RED}✗ Failed to switch to basic auth mode${NC}"
    exit 1
fi
