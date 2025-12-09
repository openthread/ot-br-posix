/*
 *  Copyright (c) 2025, The OpenThread Authors.
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
 * This file includes implementations for cryptographically secure pseudorandom number generator utilities.
 */

#include "csprng.hpp"

#include <assert.h>

#include <mbedtls/ctr_drbg.h>

#include "common/code_utils.hpp"
#include "common/types.hpp"
#include "host/posix/entropy.hpp"

namespace otbr {

static constexpr uint16_t kEntropyMinThreshold = 16;

static int handleMbedtlsEntropyPoll(void *aData, unsigned char *aOutput, size_t aInLen, size_t *aOutLen)
{
    int rval = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;

    SuccessOrExit(Entropy::GetEntropy(reinterpret_cast<uint8_t *>(aOutput), static_cast<uint16_t>(aInLen)));
    VerifyOrExit(aOutLen != nullptr);

    *aOutLen = aInLen;
    rval     = 0;

exit:
    OTBR_UNUSED_VARIABLE(aData);
    return rval;
}

Csprng &Csprng::GetInstance(void)
{
    static Csprng instance;
    return instance;
}

otbrError Csprng::RandomGet(uint8_t *aBuffer, uint16_t aSize)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mInitialized, error = OTBR_ERROR_INVALID_STATE);

    if (mbedtls_ctr_drbg_random(&mCtrDrbgContext, static_cast<unsigned char *>(aBuffer), static_cast<size_t>(aSize)) <
        0)
    {
        error = OTBR_ERROR_INVALID_ARGS;
    }

exit:
    return error;
}

Csprng::Csprng(void)
    : mInitialized(false)
{
    mbedtls_entropy_init(&mEntropyContext);
#ifndef OT_MBEDTLS_STRONG_DEFAULT_ENTROPY_PRESENT
    if (mbedtls_entropy_add_source(&mEntropyContext, handleMbedtlsEntropyPoll, nullptr, kEntropyMinThreshold,
                                   MBEDTLS_ENTROPY_SOURCE_STRONG) != 0)
    {
        otbrLogWarning("Failed to add custom entropy source to mbedtls");
    }
#endif
    mbedtls_ctr_drbg_init(&mCtrDrbgContext);

    if (mbedtls_ctr_drbg_seed(&mCtrDrbgContext, mbedtls_entropy_func, &mEntropyContext, nullptr, 0) == 0)
    {
        mInitialized = true;
    }
}

Csprng::~Csprng(void)
{
    mbedtls_entropy_free(&mEntropyContext);
    mbedtls_ctr_drbg_free(&mCtrDrbgContext);
}

} // namespace otbr
