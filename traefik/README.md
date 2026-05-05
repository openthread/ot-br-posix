# OpenThread Border Router — Traefik Reverse Proxy

Traefik provides a secure HTTPS front-end for the OTBR REST API. It terminates TLS, enforces authentication, and forwards requests to the three backend services.

```
Client ──HTTPS:443──► Traefik ──► :8081  otbr-agent  (REST API)
                               ──► :8082  otbr-auth   (enrollment / auth service)
                               ──► :8080  otbr-web    (web UI)
```

Two authentication modes are supported:

| Mode | Credential | Default |
| --- | --- | --- |
| Basic Auth | Username + password (randomized at install, stored in `/var/lib/traefik/auth/initial-password`) | ✓ active after install |
| mTLS | Client TLS certificate | activated during enrollment |

The enrollment workflow — generating device identity certificates, switching auth modes, and managing client certificates — is defined in the [OpenAPI specification](../src/rest/openapi.yaml).

---

## Installation

Run the setup script once from the repository root. It downloads the Traefik binary, generates TLS certificates, installs systemd services, and starts everything.

```bash
sudo ./script/setup-reverseproxy.sh
```

After installation the API is available at:

- `https://localhost/api/node`
- `https://<hostname>.local/api/node`

Files installed on the system:

| Path                   | Contents                                 |
| ---------------------- | ---------------------------------------- |
| `/opt/traefik/traefik` | Traefik binary (`CAP_NET_BIND_SERVICE`)  |
| `/var/lib/traefik/`    | Dynamic config, certificates, auth state |
| `/var/log/traefik/`    | Access and error logs                    |

---

## Verify the installation

```bash
# Both services must be active
systemctl status traefik otbr-auth --no-pager

# All four ports must be listening
ss -tlnp | grep -E ':(443|8081|8082|8080)\s'

# Quick API check (uses bundled CA certificate)
PASS=$(cat /var/lib/traefik/auth/initial-password)
curl --cacert tests/restjsonapi/certs/ca-bundle.pem \
     -u "admin:$PASS" \
     https://localhost/api/node
```

---

## Run the enrollment test suite

The test suite exercises the full enrollment and authentication workflow using [Bruno CLI](https://www.usebruno.com/).

**One-time setup** (installs Bruno CLI via fnm / Node.js 24):

```bash
cd tests/restjsonapi
./install_bruno_cli
```

**Reset the device to a known state, then run:**

```bash
sudo ./traefik/switch-config.sh basicauth default-state
cd tests/restjsonapi
./test-secure-enrollment
```

The reset step puts the system back to Basic Auth with the default password and no enrolled certificates — the state the test suite expects.

---

## Troubleshooting

### Services not running

```bash
sudo systemctl restart traefik otbr-auth
sudo journalctl -u traefik -u otbr-auth --no-pager -n 40
```

### Regenerate TLS certificates

```bash
sudo ./traefik/createcert.sh
sudo systemctl restart traefik
```

### Common failures

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `curl: (60) SSL certificate problem` | CA bundle not trusted | Add `--cacert tests/restjsonapi/certs/ca-bundle.pem` |
| `502 Bad Gateway` | `otbr-agent` or `otbr-auth` not running | `sudo systemctl start otbr-agent otbr-auth` |
| `401` on all requests after a test run | Password changed mid-test | `sudo ./traefik/switch-config.sh basicauth default-state` |
| `bru: command not found` | Bruno CLI not installed | `cd tests/restjsonapi && ./install_bruno_cli` |
| Bruno error: Node.js version | Node.js < 24 | `./install_bruno_cli` upgrades via fnm |
| Port 8082 not listening | `otbr-auth` failed to start | `sudo journalctl -u otbr-auth -n 50` |

### View live logs

```bash
sudo journalctl -u traefik -f          # Traefik (access + errors)
sudo journalctl -u otbr-auth -f        # Enrollment service
tail -f /var/log/traefik/traefik.log   # Traefik file log
```

---

## Reference

- [OpenAPI specification](../src/rest/openapi.yaml) — enrollment flow and all endpoints
- [Traefik documentation](https://doc.traefik.io/traefik/)
