/*
 *  Copyright (c) 2026, The OpenThread Authors.
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

#include "pcapng.hpp"

#include <errno.h>
#include <string.h>

#include "common/code_utils.hpp"
#include "common/encoding.hpp"

namespace ot {
namespace Extcap {

namespace {

// Helpers to build PCAPNG blocks in a buffer. Multi-byte values are little endian, matching the byte-order
// magic in the SHB.

void Put16(uint8_t *&aCursor, uint16_t aValue)
{
    LittleEndian::WriteUint16(aValue, aCursor);
    aCursor += sizeof(aValue);
}

void Put32(uint8_t *&aCursor, uint32_t aValue)
{
    LittleEndian::WriteUint32(aValue, aCursor);
    aCursor += sizeof(aValue);
}

void PadTo32Bits(const uint8_t *aStart, uint8_t *&aCursor)
{
    while ((aCursor - aStart) % 4 != 0)
    {
        *aCursor++ = 0;
    }
}

/**
 * Starts a block at @p aBlock: block type and a placeholder for the block total length.
 */
uint8_t *BeginBlock(uint8_t *aBlock, uint32_t aBlockType)
{
    uint8_t *cursor = aBlock;

    Put32(cursor, aBlockType);
    Put32(cursor, 0);

    return cursor;
}

/**
 * Pads the block body, then writes the block total length at both ends of the block.
 *
 * @returns The block total length.
 */
size_t EndBlock(uint8_t *aBlock, uint8_t *aCursor)
{
    uint32_t totalLength;

    PadTo32Bits(aBlock, aCursor);
    totalLength = static_cast<uint32_t>(aCursor - aBlock) + sizeof(uint32_t);
    LittleEndian::WriteUint32(totalLength, aBlock + sizeof(uint32_t));
    LittleEndian::WriteUint32(totalLength, aCursor);

    return totalLength;
}

} // namespace

PcapngWriter::PcapngWriter(void)
    : mFile(nullptr)
{
}

PcapngWriter::~PcapngWriter(void)
{
    Close();
}

otError PcapngWriter::Open(const char *aFilename)
{
    uint8_t  shb[64];
    uint8_t  idb[64];
    uint8_t *cursor;
    otError  error = OT_ERROR_NONE;

    Close();

    VerifyOrExit(aFilename != nullptr, error = OT_ERROR_INVALID_ARGS);

    mFile = fopen(aFilename, "wb");
    VerifyOrExit(mFile != nullptr, error = OT_ERROR_FAILED);

    // Section Header Block (SHB)
    cursor = BeginBlock(shb, kPcapngShbType);
    Put32(cursor, kPcapngByteOrderMagic);
    Put16(cursor, kPcapngVersionMajor);
    Put16(cursor, kPcapngVersionMinor);
    Put32(cursor, 0xffffffff); // Section length: unspecified (-1)
    Put32(cursor, 0xffffffff);
    SuccessOrExit(error = Write(shb, EndBlock(shb, cursor)));

    // Interface Description Block (IDB)
    cursor = BeginBlock(idb, kPcapngIdbType);
    Put16(cursor, static_cast<uint16_t>(kPcapngLinkTypeIeee802154));
    Put16(cursor, 0); // Reserved
    Put32(cursor, kPcapngSnapLen);
    SuccessOrExit(error = Write(idb, EndBlock(idb, cursor)));

exit:
    if (error != OT_ERROR_NONE)
    {
        int savedErrno = errno;

        Close();
        errno = savedErrno;
    }
    return error;
}

void PcapngWriter::Close(void)
{
    if (mFile != nullptr)
    {
        fclose(mFile);
        mFile = nullptr;
    }
}

otError PcapngWriter::WriteFrame(const otRadioFrame &aFrame, uint64_t aTimeUs)
{
    // TAP header (4 bytes) + Channel, FCS, RSSI and LQI TLVs (8 bytes each)
    static constexpr uint16_t kTapLength = 4 + 4 * 8;

    // EPB header (28 bytes) + TAP + PSDU + padding (up to 3 bytes) + trailing block length (4 bytes)
    uint8_t  epb[28 + kTapLength + OT_RADIO_FRAME_MAX_SIZE + 3 + 4];
    uint8_t *cursor;
    otError  error     = OT_ERROR_NONE;
    uint32_t packetLen = kTapLength + aFrame.mLength;
    float    rssi      = static_cast<float>(aFrame.mInfo.mRxInfo.mRssi);
    uint32_t rssiBits;

    static_assert(sizeof(rssi) == sizeof(rssiBits), "Size mismatch between float and uint32_t");

    VerifyOrExit(mFile != nullptr, error = OT_ERROR_INVALID_STATE);
    VerifyOrExit((aFrame.mPsdu != nullptr || aFrame.mLength == 0) && aFrame.mLength <= OT_RADIO_FRAME_MAX_SIZE,
                 error = OT_ERROR_INVALID_ARGS);

    // Enhanced Packet Block (EPB) header
    cursor = BeginBlock(epb, kPcapngEpbType);
    Put32(cursor, 0); // Interface ID
    Put32(cursor, static_cast<uint32_t>(aTimeUs >> 32));
    Put32(cursor, static_cast<uint32_t>(aTimeUs & 0xffffffff));
    Put32(cursor, packetLen); // Captured length
    Put32(cursor, packetLen); // Original length

    // IEEE 802.15.4 TAP header: version, reserved, length. Then TLVs: type, length, value padded to 32 bits.
    *cursor++ = kTapVersion;
    *cursor++ = 0;
    Put16(cursor, kTapLength);

    Put16(cursor, kTapChannelType);
    Put16(cursor, kTapChannelLength);
    Put16(cursor, aFrame.mChannel);
    *cursor++ = kTapChannelPage;
    PadTo32Bits(epb, cursor);

    Put16(cursor, kTapFcsType);
    Put16(cursor, kTapFcsLength);
    *cursor++ = kTapFcsValue;
    PadTo32Bits(epb, cursor);

    memcpy(&rssiBits, &rssi, sizeof(rssiBits));
    Put16(cursor, kTapRssiType);
    Put16(cursor, kTapRssiLength);
    Put32(cursor, rssiBits);

    Put16(cursor, kTapLqiType);
    Put16(cursor, kTapLqiLength);
    *cursor++ = aFrame.mInfo.mRxInfo.mLqi;
    PadTo32Bits(epb, cursor);

    if (aFrame.mLength > 0)
    {
        memcpy(cursor, aFrame.mPsdu, aFrame.mLength);
        cursor += aFrame.mLength;
    }

    error = Write(epb, EndBlock(epb, cursor));

exit:
    return error;
}

otError PcapngWriter::Write(const uint8_t *aData, size_t aLength)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(fwrite(aData, 1, aLength, mFile) == aLength && fflush(mFile) == 0, error = OT_ERROR_FAILED);

exit:
    if (error != OT_ERROR_NONE)
    {
        // Preserve errno (e.g. EPIPE when Wireshark closes the FIFO) for the caller.
        int savedErrno = errno;

        Close();
        errno = savedErrno;
    }
    return error;
}

} // namespace Extcap
} // namespace ot
