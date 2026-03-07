# OpenThread Border Router - Traefik Reverse Proxy

Secure HTTPS reverse proxy for [OpenThread Border Router REST API](../src/rest/otbr_rest_specification.openapi.yaml) with IEEE 802.1AR device identity certificates (IDevID/LDevID) and flexible authentication.

## Quick Start

```bash
# System installation (creates systemd services)
sudo ./script/setup-reverseproxy.sh
```

Access: `https://localhost` or `https://$(hostname).local`

## Architecture

```
Client → HTTPS:443 (Traefik) → HTTP:8081 (otbr-agent REST API)
                              → HTTP:8082 (otbr-enrollment for server enrollment and user authentication)
                              → HTTP:8080 (otbr-web UI)
```

**System Layout**:

- Binary: `/opt/traefik/traefik` (with `CAP_NET_BIND_SERVICE`)
- Config: `/var/lib/traefik/`
- Logs: `/var/log/traefik/`
- Service: `systemctl start|stop|restart traefik`

## Authentication Modes

1. **Basic Auth** - Username/password (default: `admin:thread`)
2. **mTLS** - Client certificate validation

See [OpenAPI Specification](../src/rest/otbr_rest_specification.openapi.yaml#L57-L204) for enrollment procedures.

## Testing

```bash
# Check services
systemctl status traefik traefik-cgi

# Basic Auth
curl -k -u admin:thread https://localhost/api/node

# Bruno-based enrollment test suite
cd tests/restjsonapi
# if not yet installed
./install_bruno_cli
./test-secure-enrollment
```

## Management

```bash
# Service control
sudo systemctl start|stop|restart traefik

# View logs
sudo journalctl -u traefik -f
tail -f /var/log/traefik/traefik.log

# Regenerate certificates
sudo ./traefik/createcert.sh
```

## Documentation

- [REST API Spec](../src/rest/openapi.yaml) - Complete enrollment procedures
- [Traefik Documentation](https://doc.traefik.io/traefik/) - Reverse proxy configuration
