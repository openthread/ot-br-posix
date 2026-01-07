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
 *   This file includes definitions of the posix Entropy of otbr-agent.
 */

#ifndef OTBR_AGENT_POSIX_ENTROPY_HPP_
#define OTBR_AGENT_POSIX_ENTROPY_HPP_

#include <stdint.h>

#include "common/types.hpp"

namespace otbr {

class Entropy
{
public:
    /**
     * Fill buffer with entropy.
     *
     * @param[out]  aOutput              A pointer to where the random values are placed.  Must not be NULL.
     * @param[in]   aOutputLength        Size of @p aOutput.
     *
     * @retval OTBR_ERROR_NONE          Successfully filled @p aOutput with true random values.
     * @retval OTBR_ERROR_ERRNO         Failed to fill @p aOutput with true random values.
     * @retval OTBR_ERROR_INVALID_ARGS  @p aOutput was set to NULL or @p aOutputLength is 0.
     */
    static otbrError GetEntropy(uint8_t *aOutput, uint16_t aOutputLength);
};

} // namespace otbr

#endif // OTBR_AGENT_POSIX_ENTROPY_HPP_
