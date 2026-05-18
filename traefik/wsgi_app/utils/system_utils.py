#!/usr/bin/env python3
"""
System utilities for Traefik management
"""

import json
import subprocess
import logging
import socket
from pathlib import Path

logger = logging.getLogger(__name__)


def get_etc_hostname():
    """Return the static system hostname from /etc/hostname."""
    try:
        with open('/etc/hostname', 'r') as f:
            hostname = f.read().strip()
        if hostname:
            return hostname
    except (FileNotFoundError, IOError):
        pass
    return socket.gethostname()


def get_hostname():
    """Return the OpenThread mDNS hostname (what OT actually advertises).

    ot-ctl mdns localhostname is the authoritative source — it reflects whatever
    OpenThread has set via sethostname() (e.g. ot<extaddr> or a custom name).
    Falls back to get_etc_hostname().
    """
    import re
    try:
        result = subprocess.run(['ot-ctl', 'mdns', 'localhostname'],
                                capture_output=True,
                                text=True,
                                timeout=5)
        for line in result.stdout.splitlines():
            line = line.strip()
            if line and line != 'Done' and not line.startswith('Error'):
                m = re.match(r'^([A-Za-z0-9][A-Za-z0-9-]*)$', line)
                if m:
                    return m.group(1)
    except Exception:
        pass
    # Fallback: kernel hostname via gethostname() syscall — reflects what OpenThread
    # set via sethostname(), unlike $HOSTNAME which is a stale bash snapshot.
    kernel_hostname = socket.gethostname().strip()
    if kernel_hostname:
        return kernel_hostname
    return get_etc_hostname()


def switch_config_files(mode, traefik_dir):
    """
    Switch Traefik configuration files for specified mode
    
    Args:
        mode: Configuration mode ('mtls' or 'basicauth')
        traefik_dir: Directory containing traefik configuration files (Path object or string)
    """
    traefik_dir = Path(traefik_dir) if not isinstance(traefik_dir,
                                                      Path) else traefik_dir
    logger.info(f"Switching to {mode} configuration")

    try:
        # Get hostname for substitution
        hostname = get_hostname()

        logger.debug(f"Using hostname: {hostname}.local")

        if mode == 'mtls':
            template_name = "dynamic-mtls.template"
        elif mode == 'basicauth':
            template_name = "dynamic-basicauth.template"
        elif mode == 'backup':
            template_name = "dynamic-backup.template"
            hostname = socket.gethostname()

        logger.debug(f"Using hostname: {hostname}.local")

        if mode == 'mtls':
            template_name = "dynamic-mtls.template"
        elif mode == 'basicauth':
            template_name = "dynamic-basicauth.template"
        elif mode == 'backup':
            template_name = "dynamic-backup.template"
        else:
            raise ValueError(f"Unknown configuration mode: {mode}")

        # Substitute hostname in dynamic config
        template_path = traefik_dir / template_name
        with open(template_path, 'r') as f:
            template_content = f.read()

        # Replace placeholder with actual hostname
        config_content = template_content.replace('otbr-hostname.local',
                                                  f'{hostname}.local')

        # Write to dynamic.yml
        with open(traefik_dir / "dynamic.yml", 'w') as f:
            f.write(config_content)

        logger.debug(f"Applied {mode} configuration with hostname substitution")
    except Exception as e:
        logger.error(f"Failed to copy {mode} config files: {e}")
        raise Exception(f"Configuration switch failed: {e}")


def should_switch_to_mtls(client_authz_file):
    """
    Check if we should switch to mTLS mode and set the mtls_required flag
    
    This function checks if there's at least one admin client with valid SANs.
    If yes, it sets mtls_required=true in client-authz.json to enforce mTLS mode.
    
    Args:
        client_authz_file: Path to client-authz.json file
        
    Returns:
        bool: True if should switch to mTLS, False otherwise
    """
    client_authz_file = Path(client_authz_file) if not isinstance(
        client_authz_file, Path) else client_authz_file

    if not client_authz_file.exists():
        logger.debug("No client-authz.json found, staying in basic auth mode")
        return False

    try:
        with open(client_authz_file, 'r') as f:
            authz = json.load(f)

        # Check if there are any registered client certificates with admin role and non-empty SANs
        clients = authz.get('clients', {})
        if not clients:
            logger.debug(
                "No registered clients found, staying in basic auth mode")
            return False

        # Look for at least one client with admin role and at least one SAN entry
        has_admin = False
        for client_id, client_info in clients.items():
            role = client_info.get('role', '')
            sans = client_info.get('sans', [])

            # Check if role is admin and sans list has at least one entry
            if role == 'admin' and isinstance(sans, list) and len(sans) > 0:
                logger.info(
                    f"Found admin client (ID: {client_id[:16]}...) with {len(sans)} SAN(s), will switch to mTLS"
                )
                has_admin = True
                break

        if not has_admin:
            logger.debug(
                "No admin client with valid SANs found, staying in basic auth mode"
            )
            return False

        # Set mtls_required flag to indicate enrollment has triggered mTLS mode
        authz['mtls_required'] = True
        with open(client_authz_file, 'w') as f:
            json.dump(authz, f, indent=2)
        logger.info("Set mtls_required=true in client-authz.json")

        return True
    except Exception as e:
        logger.warning(f"Failed to check authorization: {e}")

    return False


def restart_traefik():
    """
    Restart Traefik service using systemd in detached background process
    
    This requires /etc/sudoers.d/traefik-reload with:
    traefik ALL=(ALL) NOPASSWD: /usr/bin/systemctl restart traefik.service
    
    The restart happens in a fully detached background process to prevent
    blocking the WSGI application.
    
    Returns:
        bool: True if restart was triggered, False on error
    """
    logger.info("Triggering Traefik restart in detached background process...")

    try:
        subprocess.Popen(
            ['sudo', 'systemctl', 'restart', 'traefik.service'],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            stdin=subprocess.DEVNULL,
            start_new_session=True  # Completely detach from parent
        )
        logger.info("Traefik restart triggered successfully")
        return True
    except Exception as e:
        logger.error(f"Failed to trigger Traefik restart: {e}")
        return False
