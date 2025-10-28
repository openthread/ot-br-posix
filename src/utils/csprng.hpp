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
 * This file includes definition for cryptographically secure pseudorandom number generator utilities.
 */

#ifndef OTBR_UTILS_CSPRNG_HPP_
#define OTBR_UTILS_CSPRNG_HPP_

#include <common/types.hpp>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

namespace otbr {

class Csprng
{
public:
    /**
     * Gets the singleton instance of the generator.
     */
    static Csprng &GetInstance(void);

    /**
     * Fills a given buffer with cryptographically secure random bytes.
     *
     * @param[out] aBuffer  A pointer to a buffer to fill with the random bytes. Must not be NULL.
     * @param[in]  aSize    Size of buffer (number of bytes to fill).
     *
     * @retval OTBR_ERROR_NONE           The bytes are successfully generated.
     * @retval OTBR_ERROR_INVALID_STATE  The generation failed because of some internal state.
     * @retval OTBR_ERROR_INVALID_ARGS   @p aBuffer is NULL and @p aSize is not 0.
     */
    otbrError RandomGet(uint8_t *aBuffer, uint16_t aSize);

private:
    Csprng(void);
    ~Csprng(void);

    mbedtls_entropy_context  mEntropyContext;
    mbedtls_ctr_drbg_context mCtrDrbgContext;

    bool mInitialized;
};

} // namespace otbr

#endif // OTBR_UTILS_CSPRNG_HPP_
