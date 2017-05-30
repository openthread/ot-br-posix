/*
 *    Copyright (c) 2017, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definition for Thread Management Framework(TMF) Tlv.
 */

#ifndef TLV_HPP_
#define TLV_HPP_

namespace ot {

/**
 * This class implements TMF Tlv functionality.
 *
 */
class Tlv
{
    enum
    {
        kLengthEscape = 0xff, ///< This length value indicates the actual length is of two-bytes length.
    };

public:
    /**
     * This method returns the Tlv type.
     *
     * @returns The Tlv type.
     *
     */
    uint8_t GetType(void) const {
        return mType;
    }

    /**
     * This method returns the Tlv length.
     *
     * @returns The Tlv length.
     *
     */
    uint16_t GetLength(void) const {
        return (mLength != kLengthEscape ? mLength : ((&mLength)[1] << 8 | (&mLength)[2]));
    }

    /**
     * This method returns a pointer to the value.
     *
     * @returns The Tlv value.
     *
     */
    const void *GetValue(void) const {
        return reinterpret_cast<const uint8_t *>(this) + sizeof(mType) +
               (mLength != kLengthEscape ? sizeof(mLength) : (sizeof(uint16_t)  + sizeof(mLength)));
    }

    /**
     * This method returns the value as a uint16_t.
     *
     * @returns The uint16_t value.
     *
     */
    uint16_t GetValueUInt16(void) const {
        const uint8_t *p = static_cast<const uint8_t *>(GetValue());

        return (p[0] << 8 | p[1]);
    }

    /**
     * This method returns the pointer to the next Tlv.
     *
     * @returns A pointer to the next Tlv.
     *
     */
    const Tlv *GetNext(void) const {
        return reinterpret_cast<const Tlv *>(static_cast<const uint8_t *>(GetValue()) + GetLength());
    }

private:
    uint8_t mType;
    uint8_t mLength;
};

} // namespace ot

#endif  // TLV_HPP_
