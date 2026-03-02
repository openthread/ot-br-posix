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
"""Gunicorn configuration for OpenThread BR auth service"""

import multiprocessing
import os

# Server socket
bind = os.getenv('WSGI_HOST', '127.0.0.1') + ':' + os.getenv('WSGI_PORT', '8082')
backlog = 2048

# Worker processes
workers = 1
worker_class = 'sync'
threads = 1
worker_connections = 1000
max_requests = 1000
max_requests_jitter = 50
timeout = 30
keepalive = 2

# Application
wsgi_app = 'wsgi_app.app:app'

# Logging
accesslog = '-'  # stdout
errorlog = '-'   # stderr
loglevel = 'info'
access_log_format = '%(h)s %(l)s %(u)s %(t)s "%(r)s" %(s)s %(b)s "%(f)s" "%(a)s" %(D)s'

# Process naming
proc_name = 'gunicorn-otbr-auth'

# Server mechanics
daemon = False
pidfile = None
umask = 0o022
user = 'traefik'
group = 'traefik'
tmp_upload_dir = None

# SSL (handled by Traefik reverse proxy)
# Do not enable SSL here - Traefik terminates TLS and validates client certs
# SSL configuration is intentionally disabled - all settings left at defaults

# Security
limit_request_line = 4094
limit_request_fields = 100
limit_request_field_size = 8190

# Server hooks
def on_starting(server):
    """Called just before the master process is initialized"""
    server.log.info("Starting OpenThread BR auth service")

def on_reload(server):
    """Called to recycle workers during a reload"""
    server.log.info("Reloading OpenThread BR auth service")

def when_ready(server):
    """Called just after the server is started"""
    server.log.info(f"OpenThread BR auth service ready on {bind}")

def on_exit(server):
    """Called just before exiting gunicorn"""
    server.log.info("Shutting down OpenThread BR auth service")
