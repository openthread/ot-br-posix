/*
 *    Copyright (c) 2026, The OpenThread Authors.
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
 *   This file includes definitions for a packet tap on a network interface
 *   through the BSD Packet Filter.
 */

#ifndef OTBR_HOST_POSIX_BPF_TAP_HPP_
#define OTBR_HOST_POSIX_BPF_TAP_HPP_

#include "openthread-br/config.h"

#include <functional>
#include <string>
#include <vector>

#include <stdint.h>
#include <sys/types.h>

#include "common/code_utils.hpp"
#include "common/types.hpp"

struct bpf_insn;

namespace otbr {

/**
 * A packet tap on one network interface through the BSD Packet Filter.
 *
 * The tap delivers the frames the kernel receives on the interface, and
 * optionally the ones it transmits, and can inject frames into the
 * interface's transmit path. For a utun interface the transmit path is the
 * userspace client that owns the tunnel, so writing to the tap hands the
 * frame to that client.
 */
class BpfTap : private NonCopyable
{
public:
    /**
     * Receives one captured frame, link-layer header included.
     *
     * @param[in] aFrame   The frame.
     * @param[in] aLength  The captured length of the frame.
     */
    using FrameHandler = std::function<void(const uint8_t *aFrame, size_t aLength)>;

    BpfTap(void);
    ~BpfTap(void);

    /**
     * Attaches the tap to an interface.
     *
     * @param[in] aIfName   The interface name.
     * @param[in] aSeeSent  Whether frames transmitted by this host are delivered too.
     *
     * @retval OTBR_ERROR_NONE             The tap is attached.
     * @retval OTBR_ERROR_INVALID_STATE    The tap is already attached.
     * @retval OTBR_ERROR_NOT_IMPLEMENTED  The interface's link type is not supported.
     * @retval OTBR_ERROR_ERRNO            A BPF device could not be opened or configured.
     */
    otbrError Open(const std::string &aIfName, bool aSeeSent);

    /**
     * Detaches the tap.
     */
    void Close(void);

    bool     IsOpen(void) const { return mFd >= 0; }
    int      GetFd(void) const { return mFd; }
    uint32_t GetDataLinkType(void) const { return mDlt; }

    /**
     * Returns the length of the link-layer header the interface's frames carry.
     */
    size_t GetLinkHeaderLength(void) const;

    /**
     * Installs a packet filter program.
     *
     * @param[in] aInsns  The program.
     * @param[in] aCount  The number of instructions.
     */
    otbrError SetFilter(const struct bpf_insn *aInsns, size_t aCount);

    /**
     * Reads every frame currently buffered and hands each to @p aHandler.
     */
    otbrError Read(const FrameHandler &aHandler);

    /**
     * Injects a frame into the interface's transmit path.
     *
     * @param[in] aFrame   The frame, link-layer header included.
     * @param[in] aLength  The frame length.
     */
    otbrError Write(const uint8_t *aFrame, size_t aLength);

private:
    static constexpr int kMaxUnits = 256;

    int                  mFd;
    uint32_t             mDlt;
    uint32_t             mBufLen;
    std::vector<uint8_t> mBuffer;
};

} // namespace otbr

#endif // OTBR_HOST_POSIX_BPF_TAP_HPP_
