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
"""Main Flask application for OpenThread Border Router enrollment service"""

import logging
import sys
from flask import Flask, g, request
from werkzeug.exceptions import HTTPException

from .middleware.auth import AuthorizationMiddleware
from .routes.well_known import well_known_bp
from .routes.auth import auth_bp
from .utils.responses import error_response
from .config import WSGI_HOST, WSGI_PORT, CLIENT_AUTHZ_FILE, HTPASSWD_FILE

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(name)s: %(message)s',
    stream=sys.stderr
)

logger = logging.getLogger(__name__)


def create_app():
    """
    Application factory pattern
    
    Creates and configures Flask application with middleware and routes.
    """
    app = Flask(__name__)
    
    # Configure Flask
    app.config['JSON_SORT_KEYS'] = False
    app.config['JSONIFY_PRETTYPRINT_REGULAR'] = True
    
    # Register blueprints
    app.register_blueprint(well_known_bp)
    app.register_blueprint(auth_bp)
    
    # Wrap application with authorization middleware
    # Supports both mutual TLS and HTTP Basic Auth
    app.wsgi_app = AuthorizationMiddleware(
        app.wsgi_app,
        authz_file=CLIENT_AUTHZ_FILE,
        htpasswd_file=HTPASSWD_FILE
    )
    
    # Request context processors
    @app.before_request
    def inject_auth_context():
        """
        Make authorization info available via Flask's g object
        
        Middleware sets environ['auth.role'], environ['auth.scopes'], etc.
        """
        if hasattr(request.environ, 'get'):
            g.auth_role = request.environ.get('auth.role')
            g.auth_scopes = request.environ.get('auth.scopes', [])
            g.auth_client_id = request.environ.get('auth.client_id')
            g.client_cert_info = request.environ.get('client_cert_info')
    
    # Error handlers
    @app.errorhandler(HTTPException)
    def handle_http_exception(e):
        """Convert Werkzeug HTTP exceptions to RFC 7807 responses"""
        return error_response(e.code, e.name, e.description)
    
    @app.errorhandler(Exception)
    def handle_generic_exception(e):
        """Handle unexpected exceptions"""
        logger.exception("Unhandled exception")
        return error_response(500, "Internal Server Error",
                            "An unexpected error occurred")
    
    # Health check endpoint (not subject to authorization)
    @app.route('/health', methods=['GET'])
    def health():
        """Simple health check for service monitoring"""
        from .utils.responses import send_json
        return send_json({'status': 'ok'})
    
    logger.info("OpenThread Border Router enrollment service initialized")
    
    return app


# Create application instance
app = create_app()


if __name__ == '__main__':
    # Development server (not for production)
    logger.warning("Running Flask development server - use gunicorn for production")
    app.run(host=WSGI_HOST, port=WSGI_PORT, debug=False)
