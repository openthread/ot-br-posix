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
"""Response handling utilities for enrollment API"""

import json
from flask import Response, jsonify, request


def send_json(data, status=200):
    """
    Send JSON response
    
    Args:
        data: Dictionary to serialize as JSON
        status: HTTP status code
    
    Returns:
        Flask Response
    """
    return jsonify(data), status


def send_json_api(data, status=200, meta=None, resource_type=None):
    """
    Send JSON:API formatted response
    
    Args:
        data: Data object or list
        status: HTTP status code
        meta: Optional metadata dictionary
        resource_type: Optional JSON:API resource type (e.g., 'certificate')
                      If provided, transforms plain dict/list into JSON:API format
    
    Returns:
        Flask Response with application/vnd.api+json
    """
    # Transform to JSON:API format if resource_type specified
    if resource_type and isinstance(data, list):
        formatted_data = []
        for item in data:
            if isinstance(item, dict) and 'id' in item:
                # Extract id, move rest to attributes
                item_copy = item.copy()
                item_id = item_copy.pop('id')
                formatted_item = {
                    'type': resource_type,
                    'id': item_id,
                    'attributes': item_copy
                }
                formatted_data.append(formatted_item)
            else:
                formatted_data.append(item)
        data = formatted_data
    elif resource_type and isinstance(data, dict) and 'id' in data:
        # Single object
        data_copy = data.copy()
        item_id = data_copy.pop('id')
        data = {
            'type': resource_type,
            'id': item_id,
            'attributes': data_copy
        }
    
    response_data = {'data': data}
    if meta:
        response_data['meta'] = meta
    
    return Response(
        response=json.dumps(response_data),
        status=status,
        mimetype='application/vnd.api+json'
    )


def send_text(text, status=200, charset='utf-8'):
    """
    Send plain text response
    
    Args:
        text: String content
        status: HTTP status code
        charset: Character encoding
    
    Returns:
        Flask Response
    """
    return Response(
        text,
        status=status,
        mimetype=f'text/plain; charset={charset}'
    )


def send_pem(pem_data, status=200):
    """
    Send PEM-encoded certificate response
    
    Args:
        pem_data: PEM-formatted string or bytes
        status: HTTP status code
    
    Returns:
        Flask Response
    """
    if isinstance(pem_data, str):
        pem_data = pem_data.encode('ascii')
    
    return Response(
        pem_data,
        status=status,
        mimetype='application/x-pem-file; charset=ASCII'
    )


def send_pkcs7(pkcs7_data, status=200):
    """
    Send PKCS#7 certificate bundle response
    
    Args:
        pkcs7_data: PKCS#7 formatted bytes or string
        status: HTTP status code
    
    Returns:
        Flask Response
    """
    if isinstance(pkcs7_data, str):
        pkcs7_data = pkcs7_data.encode('ascii')
    
    return Response(
        pkcs7_data,
        status=status,
        mimetype='application/pkcs7-mime; smime-type=certs-only'
    )


def send_pkcs10(csr_data, status=200):
    """
    Send PKCS#10 CSR response
    
    Args:
        csr_data: PKCS#10 formatted bytes or string
        status: HTTP status code
    
    Returns:
        Flask Response
    """
    if isinstance(csr_data, str):
        csr_data = csr_data.encode('ascii')
    
    return Response(
        csr_data,
        status=status,
        mimetype='application/pkcs10'
    )


def send_no_content():
    """
    Send 204 No Content response
    
    Returns:
        Flask Response
    """
    return '', 204


def send_created(data=None, location=None):
    """
    Send 201 Created response
    
    Args:
        data: Optional response body
        location: Optional Location header value
    
    Returns:
        Flask Response
    """
    headers = {}
    if location:
        headers['Location'] = location
    
    if data:
        return jsonify(data), 201, headers
    else:
        return '', 201, headers


def error_response(status, title, detail=None, accept_header=None):
    """
    Send RFC 7807 error response with content negotiation
    
    Args:
        status: HTTP status code
        title: Short error title
        detail: Detailed error message
        accept_header: Client Accept header (or None to use request.accept_mimetypes)
    
    Returns:
        Flask Response
    """
    if accept_header is None:
        accept_mimetypes = request.accept_mimetypes
    else:
        # Simple check for JSON:API
        accept_mimetypes = accept_header
    
    # Check if client prefers JSON:API format
    if 'application/vnd.api+json' in str(accept_mimetypes):
        error_data = {
            'errors': [{
                'title': title,
                'status': str(status)
            }]
        }
        if detail:
            error_data['errors'][0]['detail'] = detail
        
        return Response(
            response=json.dumps(error_data),
            status=status,
            mimetype='application/vnd.api+json'
        )
    else:
        # Standard JSON error
        error_data = {
            'title': title,
            'status': str(status)
        }
        if detail:
            error_data['detail'] = detail
        
        return jsonify(error_data), status
