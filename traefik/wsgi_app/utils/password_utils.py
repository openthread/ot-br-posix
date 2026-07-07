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
"""Password generation and validation — no external dependencies."""

import re
import secrets
import string

from ..config import (
    PASSWORD_MIN_LENGTH,
    PASSWORD_MAX_LENGTH,
    PASSWORD_REQUIRE_DIGIT,
    PASSWORD_REQUIRE_LOWERCASE,
    PASSWORD_REQUIRE_UPPERCASE,
    PASSWORD_REQUIRE_SPECIAL,
    PASSWORD_MIN_CHAR_TYPES,
)


def generate_secure_password():
    """
    Generate a secure random password according to the configured password policy:
    - Length: 16–24 characters
    - At least 3 of 4 character types: uppercase, lowercase, digit, special

    Returns:
        str: Generated password meeting policy requirements
    """
    uppercase = string.ascii_uppercase
    lowercase = string.ascii_lowercase
    digits = string.digits
    special = '!@#$%^&*()_+-=[]{}|;:,.<>?'

    # Random length between 16–24
    length = 16 + secrets.randbelow(9)

    # 3 or 4 character type pools
    pools_to_use = secrets.randbelow(2) + 3

    password_chars = [
        secrets.choice(uppercase),
        secrets.choice(lowercase),
        secrets.choice(digits),
    ]

    if pools_to_use == 4:
        password_chars.append(secrets.choice(special))
        combined_pool = uppercase + lowercase + digits + special
    else:
        combined_pool = uppercase + lowercase + digits

    for _ in range(length - len(password_chars)):
        password_chars.append(secrets.choice(combined_pool))

    secrets.SystemRandom().shuffle(password_chars)
    return ''.join(password_chars)


def validate_password_strength(password):
    """
    Validate a password against the configured policy.

    Returns:
        (bool, str | None): (True, None) if valid; (False, error_message) if not.
    """
    if len(password) < PASSWORD_MIN_LENGTH:
        return False, f"Password must be at least {PASSWORD_MIN_LENGTH} characters"

    if len(password) > PASSWORD_MAX_LENGTH:
        return False, f"Password must not exceed {PASSWORD_MAX_LENGTH} characters"

    checks_passed = 0

    if PASSWORD_REQUIRE_LOWERCASE and re.search(r'[a-z]', password):
        checks_passed += 1
    if PASSWORD_REQUIRE_UPPERCASE and re.search(r'[A-Z]', password):
        checks_passed += 1
    if PASSWORD_REQUIRE_DIGIT and re.search(r'[0-9]', password):
        checks_passed += 1
    if PASSWORD_REQUIRE_SPECIAL and re.search(r'[^a-zA-Z0-9]', password):
        checks_passed += 1

    if checks_passed < PASSWORD_MIN_CHAR_TYPES:
        return False, (
            f"Password must contain at least {PASSWORD_MIN_CHAR_TYPES} of: "
            "lowercase, uppercase, digit, special character")

    return True, None
