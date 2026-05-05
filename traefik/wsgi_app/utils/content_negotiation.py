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
"""Content negotiation utilities for HTTP Accept header handling"""

from flask import request


def prefers_json_api():
    """
    Check if client prefers JSON:API format
    
    Returns:
        bool: True if Accept header includes application/vnd.api+json
    """
    return 'application/vnd.api+json' in request.accept_mimetypes


def prefers_json():
    """
    Check if client prefers JSON format (including JSON:API)
    
    Returns:
        bool: True if Accept header includes application/json or JSON:API
    """
    return ('application/json' in request.accept_mimetypes or
            'application/vnd.api+json' in request.accept_mimetypes)


def prefers_text():
    """
    Check if client prefers plain text
    
    Returns:
        bool: True if Accept header includes text/plain
    """
    return 'text/plain' in request.accept_mimetypes


def prefers_pkcs7():
    """
    Check if client prefers PKCS#7 certificate format
    
    Returns:
        bool: True if Accept header includes application/pkcs7-mime
    """
    return 'application/pkcs7-mime' in request.accept_mimetypes


def get_preferred_response_format(default='pkcs7'):
    """
    Determine preferred response format for certificate endpoints
    
    Args:
        default: Default format if no preference specified
    
    Returns:
        str: One of 'json', 'json_api', 'pkcs7', 'pem', 'text'
    """
    if prefers_json_api():
        return 'json_api'
    elif 'application/json' in request.accept_mimetypes:
        return 'json'
    elif 'application/pkcs7-mime' in request.accept_mimetypes:
        return 'pkcs7'
    elif 'application/x-pem-file' in request.accept_mimetypes:
        return 'pem'
    elif prefers_text():
        return 'text'
    else:
        return default


def validate_content_type(allowed_types):
    """
    Validate request Content-Type against allowed types
    
    Args:
        allowed_types: List of allowed MIME types
    
    Returns:
        bool: True if content type is allowed or empty request
    
    Raises:
        415 error if content type not allowed
    """
    if not request.data:
        # Empty request body is always OK
        return True

    content_type = request.content_type
    if not content_type:
        # No content-type header but has data - check if server is tolerant
        # OpenAPI spec: "tolerant server" accepts missing Content-Type
        return True

    # Extract base content type (ignore charset etc)
    base_type = content_type.split(';')[0].strip()

    return base_type in allowed_types
