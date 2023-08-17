/*
 *  Copyright (c) 2023, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements SHA-256.
 */

#include "sha256.hpp"

namespace otbr {

Sha256::Sha256(void)
{
    otError error;

    mContext.mContext     = &mContextStorage;
    mContext.mContextSize = sizeof(mContextStorage);

    SuccessOrExit(error = otPlatCryptoSha256Init(&mContext));

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogErr("Error otPlatCryptoSha256Init: %s", otThreadErrorToString(error));
    }
}

Sha256::~Sha256(void)
{
    otError error;

    SuccessOrExit(error = otPlatCryptoSha256Deinit(&mContext));

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogErr("Error otPlatCryptoSha256Deinit: %s", otThreadErrorToString(error));
    }
}

void Sha256::Start(void)
{
    otError error;

    SuccessOrExit(error = otPlatCryptoSha256Start(&mContext));

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogErr("Error otPlatCryptoSha256Start: %s", otThreadErrorToString(error));
    }
}

void Sha256::Update(const void *aBuf, uint16_t aBufLength)
{
    otError error;

    SuccessOrExit(error = otPlatCryptoSha256Update(&mContext, aBuf, aBufLength));

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogErr("Error otPlatCryptoSha256Update: %s", otThreadErrorToString(error));
    }
}

void Sha256::Finish(Hash &aHash)
{
    otError error;

    SuccessOrExit(error = otPlatCryptoSha256Finish(&mContext, aHash.m8, Hash::kSize));

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogErr("Error otPlatCryptoSha256Finish: %s", otThreadErrorToString(error));
    }
}
} // namespace otbr
