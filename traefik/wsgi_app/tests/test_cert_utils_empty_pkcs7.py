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
"""Unit tests for _EMPTY_PKCS7_DER constant and pem_to_pkcs7() in cert_utils."""

import subprocess
import pytest

from traefik.wsgi_app.utils.cert_utils import _EMPTY_PKCS7_DER, pem_to_pkcs7

# DER tag / OID constants (RFC 2315 / PKCS#7, will never change)
_DER_SEQUENCE_TAG = b'\x30'
_SIGNED_DATA_OID = b'\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x07\x02'
_PKCS7_DATA_OID = b'\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x07\x01'


class TestEmptyPkcs7Constant:
    """Byte-level and structural tests for _EMPTY_PKCS7_DER."""

    def test_is_bytes(self):
        assert isinstance(_EMPTY_PKCS7_DER, bytes)

    def test_starts_with_sequence_tag(self):
        assert _EMPTY_PKCS7_DER[0:1] == _DER_SEQUENCE_TAG

    def test_contains_signed_data_oid(self):
        assert _SIGNED_DATA_OID in _EMPTY_PKCS7_DER, \
            "signedData OID (1.2.840.113549.1.7.2) not found"

    def test_contains_pkcs7_data_oid(self):
        # encapContentInfo must contain the data OID
        assert _PKCS7_DATA_OID in _EMPTY_PKCS7_DER, \
            "pkcs7-data OID (1.2.840.113549.1.7.1) not found in encapContentInfo"

    def test_length_matches_outer_sequence(self):
        # DER length field at byte 1 gives the content length; total = 2 + content
        content_length = _EMPTY_PKCS7_DER[1]
        assert len(_EMPTY_PKCS7_DER) == 2 + content_length, \
            "DER length field inconsistent with actual byte length"

    def test_openssl_asn1parse_accepts_structure(self):
        """openssl asn1parse -inform DER is an independent validator of DER structure."""
        result = subprocess.run(
            ['openssl', 'asn1parse', '-inform', 'DER'],
            input=_EMPTY_PKCS7_DER,
            capture_output=True,
        )
        assert result.returncode == 0, \
            f"openssl asn1parse rejected the DER:\n{result.stderr.decode()}"
        output = result.stdout.decode()
        assert 'pkcs7-signedData' in output, \
            f"Expected 'pkcs7-signedData' in openssl output:\n{output}"
        assert 'pkcs7-data' in output, \
            f"Expected 'pkcs7-data' (encapContentInfo) in openssl output:\n{output}"

    def test_openssl_reports_no_certificates(self):
        """The bundle must contain no certificate entries."""
        result = subprocess.run(
            ['openssl', 'asn1parse', '-inform', 'DER'],
            input=_EMPTY_PKCS7_DER,
            capture_output=True,
        )
        output = result.stdout.decode()
        # A non-empty bundle would contain SEQUENCE blocks for each certificate after
        # the encapContentInfo (they appear after pkcs7-data in the output).
        # For an empty bundle, only two OID lines appear.
        oid_count = output.count('pkcs7-signedData') + output.count(
            'pkcs7-data')
        assert oid_count == 2, \
            f"Unexpected OID count {oid_count} — bundle may contain certificates:\n{output}"


class TestPemToPkcs7EmptyInput:
    """Tests for pem_to_pkcs7() behaviour on empty / no-cert input."""

    def test_empty_bytes_returns_empty_bundle(self):
        result = pem_to_pkcs7(b'')
        assert result == _EMPTY_PKCS7_DER

    def test_empty_string_returns_empty_bundle(self):
        result = pem_to_pkcs7('')
        assert result == _EMPTY_PKCS7_DER

    def test_whitespace_only_returns_empty_bundle(self):
        result = pem_to_pkcs7(b'   \n  \n  ')
        assert result == _EMPTY_PKCS7_DER

    def test_returns_bytes(self):
        result = pem_to_pkcs7(b'')
        assert isinstance(result, bytes)

    def test_empty_bundle_is_valid_der_structure(self):
        """The bytes returned for empty input must also pass the structural checks."""
        result = pem_to_pkcs7(b'')
        assert result[0:1] == _DER_SEQUENCE_TAG
        assert _SIGNED_DATA_OID in result
