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
"""Configuration for WSGI enrollment application"""

import os
from pathlib import Path


def _load_paths_env():
    """Load filename constants from paths.env"""
    paths = {}
    paths_env_file = Path(__file__).parent / 'paths.env'
    if paths_env_file.exists():
        with open(paths_env_file) as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#') and '=' in line:
                    key, value = line.split('=', 1)
                    paths[key.strip()] = value.strip()
    return paths


# Load filename constants from paths.env
_PATHS = _load_paths_env()

# Base paths
TRAEFIK_DIR = Path(os.environ.get('TRAEFIK_DIR', '/var/lib/traefik'))
CERTS_DIR = TRAEFIK_DIR / _PATHS.get('CERTS_DIR', 'certs')

# Authentication
CLIENT_CERTS_DIR = CERTS_DIR / _PATHS.get('CLIENT_CERTS_SUBDIR', 'clients')
CLIENT_AUTHZ_FILE = CLIENT_CERTS_DIR / _PATHS.get('CLIENT_AUTHZ',
                                                  'client-authz.json')
HTPASSWD_FILE = TRAEFIK_DIR / _PATHS.get('AUTH_DIR', 'auth') / _PATHS.get(
    'HTPASSWD_FILE', 'htpasswd')

# Server certificates (IDevID and LDevID)
IDEVID_CERT = CERTS_DIR / 'idevid-cert.pem'
IDEVID_CHAIN = CERTS_DIR / _PATHS.get('IDEVID_CHAIN', 'idevid-chain.pem')
IDEVID_KEY = CERTS_DIR / _PATHS.get('IDEVID_KEY', 'idevid-key.pem')

# Uploaded LDevID (not yet active)
LDEVID_CHAIN_UPLOADED = CERTS_DIR / _PATHS.get('LDEVID_CHAIN',
                                               'ldevid-chain.pem')
LDEVID_KEY_UPLOADED = CERTS_DIR / _PATHS.get('LDEVID_KEY', 'ldevid-key.pem')

# Active operational certificate (what Traefik uses)
LDEVID_CERT = CERTS_DIR / _PATHS.get('ACTIVE_CHAIN', 'active-chain.pem')
LDEVID_KEY = CERTS_DIR / _PATHS.get('ACTIVE_KEY', 'active-key.pem')
CSR_FILE = CERTS_DIR / 'ldevid.csr'
CSR_TEMP_KEY_FILE = CERTS_DIR / 'csr-temp-key.pem'

# CA certificates
CA_BUNDLE = CERTS_DIR / _PATHS.get('CA_BUNDLE', 'ca-bundle.pem')

# Application settings
WSGI_HOST = os.environ.get('WSGI_HOST', '127.0.0.1')
WSGI_PORT = int(os.environ.get('WSGI_PORT', '8082'))

# EST server settings (for pull mode enrollment)
EST_TIMEOUT = 30  # seconds

# Password policy
PASSWORD_MIN_LENGTH = 12
PASSWORD_MAX_LENGTH = 255
PASSWORD_REQUIRE_DIGIT = True
PASSWORD_REQUIRE_LOWERCASE = True
PASSWORD_REQUIRE_UPPERCASE = True
PASSWORD_REQUIRE_SPECIAL = True
PASSWORD_MIN_CHAR_TYPES = 3  # Out of: uppercase, lowercase, digit, special
