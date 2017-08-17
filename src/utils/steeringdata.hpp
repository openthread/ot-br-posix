/*
 *  Copyright (c) 2017, The OpenThread Authors.
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
 *   This file provides commissioner steering data calculations
 */


#ifndef STEERINGDATA_HPP
#define STEERINGDATA_HPP

#include <stdint.h> /* c99 types */
#include <stddef.h> /* size_t */
#include <string.h> /* memset */

namespace ot {

/**
 * This class represents Steering Data
 */

class SteeringData
{
public:

    /**
     * Sets the length of the steering data
     */
    void SetLength(int n) { mLength = n; }

    /**
     * Returns the length of the steering data.
     */
    int  GetLength(void) { return mLength; }

    /**
     * Init the steering data.
     *
     */
    void Init(void) { SetLength(16); Clear(); }


    /**
     * This method sets all bits in the Bloom Filter to zero.
     *
     */
    void Clear(void) { memset(mSteeringData, 0, sizeof(mSteeringData)); }

    /**
     * Ths method sets all bits in the Bloom Filter to one.
     *
     */
    void Set(void) { memset(mSteeringData, 0xff, sizeof(mSteeringData)); }

    /**
     * Ths method indicates whether or not the SteeringData allows all Joiners.
     *
     * @retval TRUE   If the SteeringData allows all Joiners.
     * @retval FALSE  If the SteeringData doesn't allow any Joiner.
     *
     */
    bool DoesAllowAny(void) {
        bool rval = true;

        for (uint8_t i = 0; i < GetLength(); i++)
        {
            if (mSteeringData[i] != 0xff)
            {
                rval = false;
                break;
            }
        }

        return rval;
    }

    /**
     * This method returns the number of bits in the Bloom Filter.
     *
     * @returns The number of bits in the Bloom Filter.
     *
     */
    uint8_t GetNumBits(void) { return GetLength() * 8; }

    /**
     * This method indicates whether or not bit @p aBit is set.
     *
     * @param[in]  aBit  The bit offset.
     *
     * @retval TRUE   If bit @p aBit is set.
     * @retval FALSE  If bit @p aBit is not set.
     *
     */
    bool GetBit(uint8_t aBit) {
        int b;
        int m;

        b = GetLength() - 1 - (aBit / 8);
        m = (1 << (aBit % 8));
        return (mSteeringData[b] & m) != 0;
    }

    /**
     * This method clears bit @p aBit.
     *
     * @param[in]  aBit  The bit offset.
     *
     */
    void ClearBit(uint8_t aBit) {
        int b;
        int m;

        b = GetLength() - 1 - (aBit / 8);
        m = (1 << (aBit % 8));
        mSteeringData[b] &= ~(m);
    }

    /**
     * This method sets bit @p aBit.
     *
     * @param[in]  aBit  The bit offset.
     *
     */
    void SetBit(uint8_t aBit) {
        int b;
        int m;

        b = GetLength() - 1 - (aBit / 8);
        m = 1 << (aBit % 8);
        b = mSteeringData[b] |= m;
    }

    /**
     * Ths method indicates whether or not the SteeringData is all zeros.
     *
     * @retval TRUE   If the SteeringData is all zeros.
     * @retval FALSE  If the SteeringData isn't all zeros.
     *
     */
    bool IsCleared(void);

    /**
     * This method computes the Bloom Filter.
     *
     * @param[in]  pEui64  Extended address
     *
     */
    void ComputeBloomFilter(const uint8_t *pEui64);

    /**
     * This method uses an ASCII representation of an EUI64
     * to compute the Bloom filter.
     *
     * @param[in] pEui64  Ascii EUI64 text.
     *
     * @returns true if ascii EUI64 is valid
     * @returns false if ascii EUI64 is not well formed.
     */
    bool ComputeBloomFilterAscii(const char *pEui64);

    /**
     * This method returns a pointer to the steering data.
     * @sa GetByteCount() to determine the length
     */
    const uint8_t *GetDataPointer(void) { return &mSteeringData[0]; }

private:
    /* SPEC states steering ata can be upto 16 bytes long */
    uint8_t mSteeringData[16];
    uint8_t mLength;
};

} /* namespace ot */

#endif
