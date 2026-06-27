#!/bin/bash
#
# OpenThread Border Router - Traefik System Installation
# Installs traefik as a system service with proper directory structure.
# Includes WSGI-based enrollment service (otbr-auth.service).
# Runs as dedicated 'traefik' user with CAP_NET_BIND_SERVICE

set -e

# Must run as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root (use sudo)"
    exit 1
fi

TRAEFIK_VERSION="3.6.0"
ARCH="arm64"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Source unified path configuration
PATHS_ENV="${PROJECT_ROOT}/traefik/wsgi_app/paths.env"
if [ -f "$PATHS_ENV" ]; then
    source "$PATHS_ENV"
    echo "Using paths from: $PATHS_ENV"
else
    echo "Warning: paths.env not found, using defaults"
fi

# System directories (can be overridden by paths.env)
TRAEFIK_BIN_DIR="${TRAEFIK_BIN_DIR:-/opt/traefik}"
TRAEFIK_DATA_DIR="${TRAEFIK_DATA_DIR:-/var/lib/traefik}"
TRAEFIK_LOG_DIR="${LOG_DIR:-/var/log/traefik}"
TRAEFIK_USER="traefik"
TRAEFIK_GROUP="traefik"

echo "=========================================="
echo "OpenThread Border Router - Traefik Setup"
echo "=========================================="
echo "Binary:        $TRAEFIK_BIN_DIR"
echo "Config & Data: $TRAEFIK_DATA_DIR"
echo "Logs:          $TRAEFIK_LOG_DIR"
echo "User:          $TRAEFIK_USER"
echo "=========================================="
echo ""

# Shutdown any previously running services
echo "Checking for previously running services..."

if systemctl is-active --quiet traefik 2>/dev/null; then
    echo "Stopping traefik service..."
    systemctl stop traefik
fi

# Kill any running Traefik processes from user-space installations
if pgrep -f "traefik --configFile=traefik.yml" >/dev/null 2>&1; then
    echo "Stopping user-space traefik processes..."
    pkill -f "traefik --configFile=traefik.yml" || true
    sleep 2
fi

# Kill any running WSGI server processes
if pgrep -f "gunicorn.*wsgi_app" >/dev/null 2>&1; then
    echo "Stopping WSGI server processes..."
    pkill -f "gunicorn.*wsgi_app" || true
    sleep 2
fi

echo "Previous services shutdown complete."
echo ""

# Install dependencies
echo "Checking dependencies..."
if ! command -v lsof >/dev/null 2>&1 || ! command -v setcap >/dev/null 2>&1; then
    echo "Installing dependencies..."
    apt-get update
    apt-get install -y lsof libcap2-bin
fi

# Create traefik system user (no login, no home)
if ! id -u "$TRAEFIK_USER" >/dev/null 2>&1; then
    echo "Creating system user: $TRAEFIK_USER"
    useradd --system --no-create-home --shell /usr/sbin/nologin "$TRAEFIK_USER"
else
    echo "User $TRAEFIK_USER already exists"
fi

# Create directory structure
echo "Creating directory structure..."
mkdir -p "$TRAEFIK_BIN_DIR"
mkdir -p "$TRAEFIK_DATA_DIR"/{certs,wsgi_app,auth}
mkdir -p "$TRAEFIK_LOG_DIR"

# Download and install traefik binary
if [ ! -f "$TRAEFIK_BIN_DIR/traefik" ]; then
    echo "Downloading traefik v${TRAEFIK_VERSION}..."
    TEMP_DIR=$(mktemp -d)
    cd "$TEMP_DIR"

    wget -q "https://github.com/traefik/traefik/releases/download/v${TRAEFIK_VERSION}/traefik_v${TRAEFIK_VERSION}_linux_${ARCH}.tar.gz"
    wget -q "https://github.com/traefik/traefik/releases/download/v${TRAEFIK_VERSION}/traefik_v${TRAEFIK_VERSION}_checksums.txt"

    echo "Verifying checksum..."
    sha256sum -c --ignore-missing "traefik_v${TRAEFIK_VERSION}_checksums.txt"

    echo "Extracting traefik..."
    tar -xzf "traefik_v${TRAEFIK_VERSION}_linux_${ARCH}.tar.gz" traefik

    mv traefik "$TRAEFIK_BIN_DIR/"
    chmod 755 "$TRAEFIK_BIN_DIR/traefik"

    cd - >/dev/null
    rm -rf "$TEMP_DIR"

    echo "✓ Traefik binary installed to $TRAEFIK_BIN_DIR/traefik"
else
    echo "✓ Traefik binary already installed"
fi

# Grant CAP_NET_BIND_SERVICE capability (allows binding to ports 80/443 without root)
echo "Setting capabilities for ports 80/443..."
setcap 'cap_net_bind_service=+ep' "$TRAEFIK_BIN_DIR/traefik"

# Create sudoers file for traefik user
echo "Configuring sudo permissions for traefik user..."
cat >/etc/sudoers.d/traefik <<'EOF'
# Allow traefik user to manage traefik service
traefik ALL=(ALL) NOPASSWD: /bin/systemctl restart traefik
traefik ALL=(ALL) NOPASSWD: /bin/systemctl reload traefik
traefik ALL=(ALL) NOPASSWD: /bin/systemctl is-active traefik
traefik ALL=(ALL) NOPASSWD: /usr/bin/systemctl restart traefik
traefik ALL=(ALL) NOPASSWD: /usr/bin/systemctl reload traefik
traefik ALL=(ALL) NOPASSWD: /usr/bin/systemctl is-active traefik
EOF
chmod 440 /etc/sudoers.d/traefik
echo "✓ Sudoers configuration created"

# Validate sudoers syntax
if ! visudo -c -f /etc/sudoers.d/traefik-reload >/dev/null 2>&1; then
    echo "ERROR: Invalid sudoers syntax, removing file"
    rm -f /etc/sudoers.d/traefik-reload
    exit 1
fi

# Configure otbr-web to use port 8080
OTBR_WEB_DEFAULT="/etc/default/otbr-web"
echo "Configuring otbr-web to use port 8080..."
if [ -f "$OTBR_WEB_DEFAULT" ]; then
    cp "$OTBR_WEB_DEFAULT" "${OTBR_WEB_DEFAULT}.bak"
fi

echo 'OTBR_WEB_OPTS="-I wpan0 -p 8080"' >"$OTBR_WEB_DEFAULT"

# Start or restart otbr-web service
if systemctl is-active --quiet otbr-web 2>/dev/null; then
    echo "Restarting otbr-web with new port..."
    systemctl restart otbr-web
elif systemctl is-enabled --quiet otbr-web 2>/dev/null; then
    echo "Starting otbr-web on port 8080..."
    systemctl start otbr-web
else
    echo "⚠ Warning: otbr-web service not found or not enabled"
fi

sleep 2

# Check for port conflicts
echo "Checking for port conflicts..."
if lsof -Pi :80 -sTCP:LISTEN -t >/dev/null 2>&1; then
    echo "⚠ Warning: Port 80 is already in use"
    lsof -Pi :80 -sTCP:LISTEN
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

if lsof -Pi :443 -sTCP:LISTEN -t >/dev/null 2>&1; then
    echo "⚠ Warning: Port 443 is already in use"
    lsof -Pi :443 -sTCP:LISTEN
    exit 1
fi

# Generate certificates
echo ""
echo "Generating certificates..."
"$PROJECT_ROOT/traefik/createcert.sh"

# Install configuration files
echo "Installing configuration files..."

# Use the OpenThread mDNS hostname as the authoritative name for Traefik routes.
# OpenThread sets the kernel hostname via sethostname() to its mDNS name (e.g. ot<extaddr>
# or a custom name configured via 'ot-ctl mdns localhostname'). The certificates include
# both this name and /etc/hostname as SANs, so TLS works from either name.
sleep 2
if command -v ot-ctl >/dev/null 2>&1; then
    HOSTNAME=$(ot-ctl mdns localhostname 2>/dev/null \
        | grep -v "Done" \
        | grep -v "^Error" \
        | tr -d '[:space:]' \
        | grep -oE '^[A-Za-z0-9][A-Za-z0-9-]*$' \
        | head -1)
fi
if [ -z "$HOSTNAME" ]; then
    HOSTNAME=$(cat /etc/hostname 2>/dev/null | tr -d '[:space:]')
fi
if [ -z "$HOSTNAME" ]; then
    HOSTNAME=$(hostname)
fi
echo "Using hostname for Traefik routes: ${HOSTNAME}.local"

HOST_RULE="(Host(\`${HOSTNAME}.local\`) || Host(\`localhost\`))"

# Install traefik.yml (static configuration)
cat >"$TRAEFIK_DATA_DIR/traefik.yml" <<EOF
# Traefik static configuration for OpenThread Border Router Secure Enrollment
# System installation - runs as user 'traefik' with CAP_NET_BIND_SERVICE

entryPoints:
  web:
    address: :80
    http:
      redirections:
        entryPoint:
          to: websecure
          scheme: https
  websecure:
    address: :443
    transport:
      respondingTimeouts:
        idleTimeout: 3m
    http:
      tls:
        options: default

# Global TLS configuration
tls:
  stores:
    default:
      defaultCertificate:
        # Initial IDevID certificate chain (ex-factory state)
        certFile: "$TRAEFIK_DATA_DIR/certs/idevid-chain.pem"
        keyFile: "$TRAEFIK_DATA_DIR/certs/idevid-key.pem"

# Dynamic configuration provider
providers:
  file:
    directory: "$TRAEFIK_DATA_DIR"
    watch: true

# API and Dashboard
api:
  dashboard: true
  insecure: false

# Logging
log:
  level: DEBUG
  filePath: "$TRAEFIK_LOG_DIR/traefik.log"
  format: json
EOF

# Install dynamic.yml template
cat >"$TRAEFIK_DATA_DIR/dynamic.yml" <<EOF
# Dynamic configuration for OpenThread Border Router Secure Enrollment
# This file is watched by Traefik and changes are applied automatically

# IMPORTANT: Traefik limitation - all routers on the same host must use the SAME TLS option.
# Mixing mtls-optional and mtls-required on ${HOSTNAME}.local causes Traefik to fall back
# to default TLS options, breaking mTLS validation. This config uses *mtls-optional* for all.

http:
  routers:
    # Well-Known endpoints - publicly accessible for discovery
    # These are served by WSGI enrollment service
    wellknown-router:
      rule: "${HOST_RULE} && PathPrefix(\`/.well-known/thread\`)"
      entryPoints:
        - websecure
      service: otbr-auth
      tls:
        options: mtls-optional
      priority: 100

    # Security auth endpoints - require authentication
    auth-router:
      rule: "${HOST_RULE} && (Path(\`/api/auth/password\`) || Path(\`/api/auth/token\`) || PathPrefix(\`/api/auth/certs\`) || Path(\`/api/auth/enroll\`))"
      entryPoints:
        - websecure
      service: otbr-auth
      tls:
        options: mtls-optional
      middlewares:
        - auth-chain
        - cert-headers
      priority: 90

    # API endpoints - proxied from otbr-agent
    api-router-operational:
      rule: "${HOST_RULE} && PathPrefix(\`/api\`)"
      entryPoints:
        - websecure
      service: otbr-agent
      tls:
        options: mtls-optional
      middlewares:
        - auth-chain
        - api-headers
      priority: 80

    # Web interface - proxied from otbr-web
    web-router:
      rule: "${HOST_RULE} && PathPrefix(\`/\`)"
      entryPoints:
        - websecure
      service: otbr-web
      tls:
        options: mtls-optional
      middlewares:
        - auth-chain
        - api-headers
      priority: 10

  services:
    otbr-agent:
      loadBalancer:
        servers:
          - url: "http://127.0.0.1:8081"
    
    otbr-auth:
      loadBalancer:
        servers:
          - url: "http://127.0.0.1:8082"
    
    otbr-web:
      loadBalancer:
        servers:
          - url: "http://127.0.0.1:8080"

  middlewares:
    # Authentication chain - Basic Auth or Bearer token
    auth-chain:
      chain:
        middlewares:
          - basic-or-bearer

    # Basic or Bearer token authentication
    basic-or-bearer:
      basicAuth:
        usersFile: "$TRAEFIK_DATA_DIR/auth/htpasswd"
        # Don't remove Authorization header - allows bearer tokens to pass through
        removeHeader: false

    # Pass client certificate information to backend
    # Backend can use these headers for authorization decisions
    cert-headers:
      chain:
        middlewares:
          - cert-pass-tls

    cert-pass-tls:
      passTLSClientCert:
        pem: false
        info:
          notAfter: true
          notBefore: true
          sans: true
          subject:
            commonName: true
            country: true
            organization: true
            serialNumber: true
          issuer:
            commonName: true
            country: true
            organization: true

    # CORS and security headers for API
    api-headers:
      headers:
        customRequestHeaders:
          X-Forwarded-Proto: "https"

# TLS configuration (must be in dynamic config for file provider)
tls:
  certificates:
    # Active certificate chain - starts with IDevID, switches to LDevID after enrollment
    - certFile: "$TRAEFIK_DATA_DIR/certs/active-chain.pem"
      keyFile: "$TRAEFIK_DATA_DIR/certs/active-key.pem"
      stores:
        - default

  options:
    # Optional mTLS - validates client certificate if provided, but not required
    mtls-optional:
      minVersion: VersionTLS13
      cipherSuites:
        - TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
        - TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
        - TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
        - TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
      clientAuth:
        caFiles:
          - "$TRAEFIK_DATA_DIR/certs/ca-bundle.pem"
        clientAuthType: VerifyClientCertIfGiven
EOF

# Install dynamic configuration templates for test scripts and enrollment
echo "Installing dynamic configuration templates..."

# Copy dynamic.yml as dynamic-basicauth.template (used by test scripts)
cp "$TRAEFIK_DATA_DIR/dynamic.yml" "$TRAEFIK_DATA_DIR/dynamic-basicauth.template"

# Generate dynamic-mtls.template (used by enrollment script)
cat >"$TRAEFIK_DATA_DIR/dynamic-mtls.template" <<EOF
# Dynamic configuration for OpenThread Border Router Secure Enrollment
# mTLS-ONLY configuration (no basic auth)
# This file replaces dynamic.yml during enrollment. dynamic.yml is watched by Traefik, so changes are applied automatically.

# IMPORTANT: Traefik limitation - all routers on the same host must use the SAME TLS option.
# Mixing mtls-optional and mtls-required on ${HOSTNAME}.local causes Traefik to fall back
# to default TLS options, breaking mTLS validation. This config uses *mtls-required* for all.

http:
  routers:
    # Well-Known endpoints
    # These are served by WSGI auth service
    # cert-validation middleware is provided by otbr-auth itself to check client cert against authz database
    wellknown-router:
      rule: "${HOST_RULE} && PathPrefix(\`/.well-known/thread\`)"
      entryPoints:
        - websecure
      service: otbr-auth
      tls:
        options: mtls-required
      middlewares:
        - cert-headers
        - api-headers
      priority: 100

    # Security auth endpoints - require mTLS
    # Served by WSGI auth service (password changes, auth, enrollment)
    # cert-validation middleware is provided by otbr-auth itself to check client cert against authz database
    auth-router:
      rule: "${HOST_RULE} && (Path(\`/api/auth/password\`) || Path(\`/api/auth/token\`) || PathPrefix(\`/api/auth/certs\`) || Path(\`/api/auth/enroll\`))"
      entryPoints:
        - websecure
      service: otbr-auth
      tls:
        options: mtls-required
      middlewares:
        - cert-headers
        - api-headers
      priority: 90

    # API endpoints - proxied from otbr-agent
    api-router-operational:
      rule: "${HOST_RULE} && PathPrefix(\`/api\`)"
      entryPoints:
        - websecure
      service: otbr-agent
      tls:
        options: mtls-required
      middlewares:
        - cert-headers
        - cert-validation
        - api-headers
      priority: 80

    # Web interface - proxied from otbr-web
    web-router:
      rule: "${HOST_RULE} && PathPrefix(\`/\`)"
      entryPoints:
        - websecure
      service: otbr-web
      tls:
        options: mtls-required
      middlewares:
        - cert-headers
        - cert-validation
        - api-headers
      priority: 10

  services:
    # otbr-agent REST API server (Thread network operations)
    otbr-agent:
      loadBalancer:
        servers:
          - url: "http://localhost:8081"
        passHostHeader: true

    # WSGI server (secure auth and auth endpoints)
    otbr-auth:
      loadBalancer:
        servers:
          - url: "http://localhost:8082"
        passHostHeader: true

    # otbr-web interface (web UI)
    otbr-web:
      loadBalancer:
        servers:
          - url: "http://localhost:8080"
        passHostHeader: true

  middlewares:
    # Certificate validation - validates client cert against authorization database
    # ForwardAuth calls /internal/forwardauth which returns auth headers
    cert-validation:
      forwardAuth:
        address: "http://localhost:8082/internal/forwardauth"
        trustForwardHeader: true
        authResponseHeaders:
          - "X-Auth-Role"
          - "X-Auth-Scopes"

    # Pass client certificate information to backend
    # Backend can use these headers for authorization decisions
    # Traefik populates X-Forwarded-Tls-Client-Cert-Info
    cert-headers:
      chain:
        middlewares:
          - cert-pass-tls

    cert-pass-tls:
      passTLSClientCert:
        pem: false
        info:
          notAfter: true
          notBefore: true
          sans: true
          subject:
            commonName: true
            country: true
            organization: true
            serialNumber: true
          issuer:
            commonName: true
            country: true
            organization: true

    # Headers to pass to backend
    api-headers:
      headers:
        customRequestHeaders:
          X-Forwarded-Proto: "https"

# TLS configuration - certificates loaded here can be hot-reloaded
tls:
  certificates:
    # Active certificate chain - dynamically updated during enrollment
    - certFile: "$TRAEFIK_DATA_DIR/certs/active-chain.pem"
      keyFile: "$TRAEFIK_DATA_DIR/certs/active-key.pem"
      stores:
        - default

  # TLS options for client authentication
  options:
    # Mutual TLS required - client certificate must be valid
    mtls-required:
      minVersion: VersionTLS13
      cipherSuites:
        - TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
        - TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
        - TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
        - TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
      clientAuth:
        caFiles:
          - "$TRAEFIK_DATA_DIR/certs/ca-bundle.pem"
        clientAuthType: RequireAndVerifyClientCert
EOF

# Set ownership and permissions
echo "Setting ownership and permissions..."

# Static config (readable by traefik user)
# Config now in $TRAEFIK_DATA_DIR (owned by traefik user)

# Dynamic data (writable by traefik user for certificate updates)
chown -R "$TRAEFIK_USER:$TRAEFIK_GROUP" "$TRAEFIK_DATA_DIR"
chmod 775 "$TRAEFIK_DATA_DIR"
chmod 644 "$TRAEFIK_DATA_DIR/dynamic.yml"
chmod 644 "$TRAEFIK_DATA_DIR/dynamic-basicauth.template"
chmod 644 "$TRAEFIK_DATA_DIR/dynamic-mtls.template"
chmod 775 "$TRAEFIK_DATA_DIR/certs"
chmod 775 "$TRAEFIK_DATA_DIR/auth"

# Create initial admin password (only on first install; not overwritten on reinstall)
HTPASSWD_INIT="$TRAEFIK_DATA_DIR/auth/htpasswd"
INITIAL_PASS_FILE="$TRAEFIK_DATA_DIR/auth/initial-password"
if [ ! -f "$HTPASSWD_INIT" ]; then
    echo "Generating initial admin password..."
    INITIAL_PASS=$(python3 -c "
import secrets, string
upper = string.ascii_uppercase
lower = string.ascii_lowercase
digits = string.digits
special = '!@#\$%^&*'
chars = upper + lower + digits + special
p = [secrets.choice(upper), secrets.choice(lower), secrets.choice(digits), secrets.choice(special)]
p += [secrets.choice(chars) for _ in range(16)]
import random; rng = random.SystemRandom(); rng.shuffle(p)
print(''.join(p))
")
    INIT_HASH=$(openssl passwd -apr1 "$INITIAL_PASS")
    echo "admin:${INIT_HASH}" | sudo -u "$TRAEFIK_USER" tee "$HTPASSWD_INIT" >/dev/null
    echo "$INITIAL_PASS" | sudo -u "$TRAEFIK_USER" tee "$INITIAL_PASS_FILE" >/dev/null
    chmod 644 "$INITIAL_PASS_FILE"
    echo "✓ Initial admin password generated"
    echo "  Credentials: admin / ${INITIAL_PASS}"
    echo "  Saved to:    $INITIAL_PASS_FILE"
else
    echo "✓ Admin htpasswd already exists (not overwritten)"
fi

# Logs (writable by traefik user)
chown -R "$TRAEFIK_USER:$TRAEFIK_GROUP" "$TRAEFIK_LOG_DIR"
chmod 755 "$TRAEFIK_LOG_DIR"

# Binary
chown root:root "$TRAEFIK_BIN_DIR/traefik"

# Create systemd service
echo "Installing systemd service..."
cat >/etc/systemd/system/traefik.service <<EOF
[Unit]
Description=Traefik Reverse Proxy for OpenThread Border Router
Documentation=https://doc.traefik.io/traefik/
After=network-online.target otbr-agent.service
Wants=network-online.target

[Service]
Type=simple
User=$TRAEFIK_USER
Group=$TRAEFIK_GROUP
ExecStart=$TRAEFIK_BIN_DIR/traefik --configFile=$TRAEFIK_DATA_DIR/traefik.yml
Restart=on-failure
RestartSec=5s

# Security hardening
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=$TRAEFIK_DATA_DIR $TRAEFIK_LOG_DIR

# Ambient capabilities (required for binding to ports 80/443 as non-root)
AmbientCapabilities=CAP_NET_BIND_SERVICE

# Resource limits
LimitNOFILE=1048576
LimitNPROC=512

[Install]
WantedBy=multi-user.target
EOF

# Install Python dependencies for WSGI auth service
echo "Installing Python dependencies for auth service..."
if ! python3 -c "import flask" 2>/dev/null; then
    echo "Installing Flask..."
    apt-get install -y python3-flask
fi

if ! python3 -c "import gunicorn" 2>/dev/null; then
    echo "Installing Gunicorn..."
    apt-get install -y python3-gunicorn
fi

# Install additional Python dependencies (cryptography and passlib)
if ! python3 -c "import cryptography" 2>/dev/null; then
    echo "Installing cryptography..."
    apt-get install -y python3-cryptography
fi

if ! python3 -c "import passlib" 2>/dev/null; then
    echo "Installing passlib..."
    apt-get install -y python3-passlib
fi

if ! python3 -c "import requests" 2>/dev/null; then
    echo "Installing requests..."
    apt-get install -y python3-requests
fi

echo "Python dependencies installed successfully"

# Copy WSGI auth application
echo "Installing WSGI auth application..."
if [ -d "$PROJECT_ROOT/traefik/wsgi_app" ]; then
    # Remove existing wsgi_app before copying to ensure a clean install
    # (cp -r merges into existing dirs, leaving stale .pyc files and deleted files behind)
    rm -rf "$TRAEFIK_DATA_DIR/wsgi_app"
    cp -r "$PROJECT_ROOT/traefik/wsgi_app" "$TRAEFIK_DATA_DIR/"
    # Remove developer-only artefacts that have no role in production
    rm -rf "$TRAEFIK_DATA_DIR/wsgi_app/tests" "$TRAEFIK_DATA_DIR/wsgi_app/__pycache__"
    chown -R "$TRAEFIK_USER:$TRAEFIK_GROUP" "$TRAEFIK_DATA_DIR/wsgi_app"
    echo "✓ WSGI application installed to $TRAEFIK_DATA_DIR/wsgi_app"
else
    echo "⚠ Warning: WSGI application not found at $PROJECT_ROOT/traefik/wsgi_app"
    exit 1
fi

# Create WSGI auth systemd service
echo "Installing WSGI auth systemd service..."
cat >/etc/systemd/system/otbr-auth.service <<EOF
[Unit]
Description=OpenThread Border Router Auth Service (WSGI)
Documentation=https://github.com/openthread/ot-br-posix
After=network.target
Wants=network-online.target

[Service]
Type=notify
User=$TRAEFIK_USER
Group=$TRAEFIK_GROUP
WorkingDirectory=$TRAEFIK_DATA_DIR
Environment="TRAEFIK_DIR=$TRAEFIK_DATA_DIR"
Environment="WSGI_HOST=127.0.0.1"
Environment="WSGI_PORT=8082"
Environment="PYTHONUNBUFFERED=1"

# Gunicorn command
ExecStart=/usr/bin/python3 -m gunicorn \\
    --config $TRAEFIK_DATA_DIR/wsgi_app/gunicorn.conf.py \\
    --chdir $TRAEFIK_DATA_DIR \\
    wsgi_app.app:app

# Restart policy
Restart=on-failure
RestartSec=5s
StartLimitInterval=60s
StartLimitBurst=3

# Security hardening
NoNewPrivileges=false
PrivateTmp=true
ProtectSystem=full
ProtectHome=true
ReadWritePaths=$TRAEFIK_DATA_DIR

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=otbr-auth

# Process management
KillMode=mixed
KillSignal=SIGTERM
TimeoutStopSec=30

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd
systemctl daemon-reload

# Enable and start services
echo ""
echo "Enabling and starting services..."
systemctl enable traefik
systemctl enable otbr-auth
systemctl start otbr-auth
systemctl start traefik

# Wait for services to start
sleep 3

# Check service status
echo ""
echo "=========================================="
echo "Service Status:"
echo "=========================================="
systemctl status traefik --no-pager -l || true
echo ""
systemctl status otbr-auth --no-pager -l || true

# Verify ports are listening
echo ""
echo "=========================================="
echo "Port Check:"
echo "=========================================="
ss -tlnp | grep -E ':(80|443|8080|8081|8082)\s' || echo "No services found on expected ports"

# Test HTTPS endpoint
echo ""
echo "=========================================="
echo "Testing HTTPS endpoint..."
echo "=========================================="
sleep 2

HTTP_CODE=$(curl -k --connect-timeout 5 -o /dev/null -w "%{http_code}" "https://localhost/.well-known/thread" 2>/dev/null || echo "000")
if [ "$HTTP_CODE" = "200" ]; then
    echo "✓ Traefik is responding on port 443 (HTTP 200)"
    echo ""
    echo "Access via:"
    echo "  https://localhost"
    echo "  https://${HOSTNAME}.local"
    echo ""
    INITIAL_PASS_FILE="$TRAEFIK_DATA_DIR/auth/initial-password"
    if [ -f "$INITIAL_PASS_FILE" ]; then
        echo "Initial credentials: admin / $(cat $INITIAL_PASS_FILE)"
        echo "  (saved at $INITIAL_PASS_FILE)"
    else
        echo "Admin password is in: $TRAEFIK_DATA_DIR/auth/htpasswd"
    fi
else
    echo "⚠ Warning: Unexpected HTTP response code: $HTTP_CODE (expected 200)"
    echo ""
    echo "Troubleshooting:"
    echo "  systemctl status traefik"
    echo "  journalctl -u traefik -f"
    echo "  curl -kv https://localhost/.well-known/thread"
fi

echo ""
echo "=========================================="
echo "Installation Complete!"
echo "=========================================="
echo ""
echo "Configuration locations:"
echo "  Binary:       $TRAEFIK_BIN_DIR/traefik"
echo "  Config:       $TRAEFIK_DATA_DIR/traefik.yml"
echo "  Dynamic:      $TRAEFIK_DATA_DIR/dynamic.yml"
echo "  Templates:    $TRAEFIK_DATA_DIR/dynamic-{basicauth,mtls}.template"
echo "  Certificates: $TRAEFIK_DATA_DIR/certs/"
echo "  Logs:         $TRAEFIK_LOG_DIR/traefik.log"
echo "  Auth:         $TRAEFIK_DATA_DIR/auth/htpasswd"
echo ""
echo "Service management:"
echo "  systemctl start|stop|restart traefik"
echo "  systemctl start|stop|restart otbr-auth"
echo "  journalctl -u traefik -f"
echo "  journalctl -u otbr-auth -f"
echo ""
echo "Certificate transition (IDevID → LDevID):"
echo "  See: $PROJECT_ROOT/traefik/README.md"
echo ""
