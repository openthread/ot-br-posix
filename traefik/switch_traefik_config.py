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
"""
Command-line wrapper for switching Traefik configuration modes
Usage: sudo ./switch_traefik_config.py <mode>
Where mode is: mtls, basicauth, or backup
"""

import sys
import os
import socket
from pathlib import Path

# Add traefik directory to path
sys.path.insert(0, str(Path(__file__).parent))

from wsgi_app.utils.system_utils import switch_config_files, restart_traefik, get_hostname
#from logger import setup_logger

def main():
    if len(sys.argv) != 2:
        print("Usage: sudo ./switch_traefik_config.py <mode>")
        print("  mode: mtls, basicauth, or backup")
        sys.exit(1)
    
    mode = sys.argv[1].lower()
    
    if mode not in ['mtls', 'basicauth', 'backup']:
        print(f"Error: Invalid mode '{mode}'")
        print("Valid modes: mtls, basicauth, backup")
        sys.exit(1)
    
    # Check if running as root
    if os.geteuid() != 0:
        print("Error: This script must be run as root (use sudo)")
        sys.exit(1)
    
    # Setup logger
    # logger = setup_logger('switch-config')
    
    # Determine traefik directory (system installation)
    traefik_dir = Path('/var/lib/traefik')
    
    if not traefik_dir.exists():
        #logger.error(f"Traefik directory not found: {traefik_dir}")
        print(f"Error: Traefik directory not found: {traefik_dir}")
        sys.exit(1)
    
    mode_names = {
        'mtls': 'mTLS-only',
        'basicauth': 'Basic Auth',
        'backup': 'Backup'
    }
    
    print(f"Switching to {mode_names[mode]} mode...")
    
    try:
        print(f"  Hostname: {get_hostname()}.local")
        
        # Backup current config
        print("  Backing up current configuration...")
        import shutil
        try:
            shutil.copy(traefik_dir / "dynamic.yml", traefik_dir / "dynamic.yml.bak")
        except FileNotFoundError:
            pass
        
        # Apply new configuration using shared function
        print(f"  Applying {mode_names[mode]} configuration...")
        switch_config_files(mode, traefik_dir)
        
        # Traefik watches the directory and will automatically reload
        restart_traefik()
        
        print(f"✓ Configuration switched to {mode_names[mode]} mode")
        # logger.info(f"Configuration successfully switched to {mode} mode")
        
    except Exception as e:
        # logger.error(f"Configuration switch failed: {e}")
        print(f"✗ ERROR: Configuration switch failed: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
