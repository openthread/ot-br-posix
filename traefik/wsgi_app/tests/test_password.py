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
"""Unit tests for password generation and validation logic."""

import re
import string

import pytest

from traefik.wsgi_app.utils.password_utils import generate_secure_password, validate_password_strength


class TestGenerateSecurePassword:
    """Tests for generate_secure_password()."""

    def test_length_within_policy_bounds(self):
        for _ in range(50):
            p = generate_secure_password()
            assert 16 <= len(
                p) <= 24, f"Length {len(p)} out of expected range [16,24]"

    def test_passes_own_policy(self):
        """Every generated password must pass validate_password_strength."""
        for _ in range(100):
            p = generate_secure_password()
            valid, msg = validate_password_strength(p)
            assert valid, f"Generated password failed policy: {msg!r}"

    def test_contains_uppercase(self):
        for _ in range(50):
            p = generate_secure_password()
            assert re.search(r'[A-Z]', p), "No uppercase letter found"

    def test_contains_lowercase(self):
        for _ in range(50):
            p = generate_secure_password()
            assert re.search(r'[a-z]', p), "No lowercase letter found"

    def test_contains_digit(self):
        for _ in range(50):
            p = generate_secure_password()
            assert re.search(r'[0-9]', p), "No digit found"

    def test_no_control_characters(self):
        for _ in range(50):
            p = generate_secure_password()
            for ch in p:
                assert ord(
                    ch
                ) >= 0x20, f"Control character 0x{ord(ch):02x} found in password"

    def test_passwords_are_unique(self):
        """Passwords must not repeat — generator is non-deterministic."""
        passwords = {generate_secure_password() for _ in range(20)}
        assert len(passwords) == 20, "Duplicate passwords generated"

    def test_returns_string(self):
        p = generate_secure_password()
        assert isinstance(p, str)


class TestValidatePasswordStrength:
    """Tests for validate_password_strength()."""

    # --- valid passwords ---

    def test_valid_all_character_types(self):
        valid, msg = validate_password_strength("Abc1!Abc1!Abc1!")
        assert valid is True
        assert msg is None

    def test_valid_minimum_length(self):
        # Exactly 12 chars with upper, lower, digit, special
        valid, msg = validate_password_strength("Aa1!Aa1!Aa1!")
        assert valid is True

    def test_valid_long_password(self):
        valid, msg = validate_password_strength("A" * 100 + "a1!")
        assert valid is True

    # --- invalid: too short ---

    def test_invalid_too_short(self):
        valid, msg = validate_password_strength("Aa1!")
        assert valid is False
        assert msg is not None

    def test_invalid_exactly_11_chars(self):
        valid, msg = validate_password_strength("Aa1!Aa1!Aa1")
        assert valid is False

    # --- invalid: missing character types ---

    def test_invalid_only_lowercase(self):
        valid, msg = validate_password_strength("abcdefghijkl")
        assert valid is False

    def test_invalid_only_upper_and_lower(self):
        # Only 2 of 4 types — fails PASSWORD_MIN_CHAR_TYPES = 3
        valid, msg = validate_password_strength("AaBbCcDdEeFf")
        assert valid is False

    def test_valid_three_types_no_special(self):
        # 3 types (upper + lower + digit) is enough (MIN_CHAR_TYPES = 3)
        valid, msg = validate_password_strength("AaBbCc123456")
        assert valid is True

    def test_valid_three_types_no_digit(self):
        # upper + lower + special = 3 types
        valid, msg = validate_password_strength("AaBbCc!!!!!!!")
        assert valid is True

    # --- invalid: empty / too long edge cases ---

    def test_invalid_empty(self):
        valid, msg = validate_password_strength("")
        assert valid is False

    def test_invalid_exceeds_max_length(self):
        valid, msg = validate_password_strength("Aa1!" * 64 + "X")  # 257 chars
        assert valid is False

    def test_valid_exactly_max_length(self):
        # 255 chars: 64*"Aa1!" = 256 chars, trim to 255
        p = ("Aa1!" * 64)[:255]
        valid, msg = validate_password_strength(p)
        assert valid is True

    # --- error message format ---

    def test_error_message_is_string_on_failure(self):
        valid, msg = validate_password_strength("short")
        assert isinstance(msg, str)

    def test_no_error_message_on_success(self):
        valid, msg = validate_password_strength("ValidPass1!")
        # "ValidPass1!" = V A L I D P A S S 1 ! = 11 chars — too short
        assert valid is False  # confirm it's too short
        valid, msg = validate_password_strength("ValidPass1!X")  # 12 chars
        assert valid is True
        assert msg is None
