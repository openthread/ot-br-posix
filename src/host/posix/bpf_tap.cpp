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

#define OTBR_LOG_TAG "BPF"

#include "host/posix/bpf_tap.hpp"

#include <errno.h>
#include <fcntl.h>
#include <net/bpf.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common/logging.hpp"

namespace otbr {

BpfTap::BpfTap(void)
    : mFd(-1)
    , mDlt(0)
    , mBufLen(0)
{
}

BpfTap::~BpfTap(void)
{
    Close();
}

otbrError BpfTap::Open(const std::string &aIfName, bool aSeeSent)
{
    otbrError    error = OTBR_ERROR_NONE;
    int          savedErrno;
    struct ifreq ifr;
    unsigned int value;

    VerifyOrExit(!IsOpen(), error = OTBR_ERROR_INVALID_STATE);

    // The BPF device does not clone on every platform: take the first free unit.
    for (int unit = 0; unit < kMaxUnits && mFd < 0; unit++)
    {
        std::string path = "/dev/bpf" + std::to_string(unit);

        mFd = open(path.c_str(), O_RDWR | O_CLOEXEC);
        if (mFd < 0 && errno != EBUSY)
        {
            break;
        }
    }
    VerifyOrExit(mFd >= 0, error = OTBR_ERROR_ERRNO);

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, aIfName.c_str(), sizeof(ifr.ifr_name) - 1);
    VerifyOrExit(ioctl(mFd, BIOCSETIF, &ifr) == 0, error = OTBR_ERROR_ERRNO);

    // Deliver frames as they arrive rather than when the buffer fills.
    value = 1;
    VerifyOrExit(ioctl(mFd, BIOCIMMEDIATE, &value) == 0, error = OTBR_ERROR_ERRNO);

    // Frames written to the tap carry their own link-layer header.
    value = 1;
    VerifyOrExit(ioctl(mFd, BIOCSHDRCMPLT, &value) == 0, error = OTBR_ERROR_ERRNO);

    value = aSeeSent ? 1 : 0;
    VerifyOrExit(ioctl(mFd, BIOCSSEESENT, &value) == 0, error = OTBR_ERROR_ERRNO);

    VerifyOrExit(ioctl(mFd, BIOCGDLT, &mDlt) == 0, error = OTBR_ERROR_ERRNO);
    VerifyOrExit(mDlt == DLT_NULL || mDlt == DLT_EN10MB, error = OTBR_ERROR_NOT_IMPLEMENTED);

    VerifyOrExit(ioctl(mFd, BIOCGBLEN, &mBufLen) == 0, error = OTBR_ERROR_ERRNO);
    mBuffer.resize(mBufLen);

    otbrLogInfo("Attached to %s (dlt %u, see sent %d)", aIfName.c_str(), mDlt, aSeeSent);

exit:
    if (error != OTBR_ERROR_NONE && error != OTBR_ERROR_INVALID_STATE)
    {
        savedErrno = errno;
        otbrLogWarning("Failed to attach to %s: %s", aIfName.c_str(),
                       error == OTBR_ERROR_ERRNO ? strerror(savedErrno) : "unsupported link type");
        Close();
        errno = savedErrno;
    }
    return error;
}

void BpfTap::Close(void)
{
    if (mFd >= 0)
    {
        close(mFd);
        mFd = -1;
    }
    mDlt    = 0;
    mBufLen = 0;
    mBuffer.clear();
}

size_t BpfTap::GetLinkHeaderLength(void) const
{
    size_t length;

    switch (mDlt)
    {
    case DLT_NULL:
        length = 4;
        break;
    case DLT_EN10MB:
        length = 14;
        break;
    default:
        length = 0;
        break;
    }

    return length;
}

otbrError BpfTap::SetFilter(const struct bpf_insn *aInsns, size_t aCount)
{
    otbrError          error = OTBR_ERROR_NONE;
    struct bpf_program program;

    VerifyOrExit(IsOpen(), error = OTBR_ERROR_INVALID_STATE);

    program.bf_len   = static_cast<u_int>(aCount);
    program.bf_insns = const_cast<struct bpf_insn *>(aInsns);
    VerifyOrExit(ioctl(mFd, BIOCSETF, &program) == 0, error = OTBR_ERROR_ERRNO);

exit:
    return error;
}

otbrError BpfTap::Read(const FrameHandler &aHandler)
{
    otbrError error = OTBR_ERROR_NONE;
    ssize_t   count;
    size_t    offset = 0;

    VerifyOrExit(IsOpen(), error = OTBR_ERROR_INVALID_STATE);

    count = read(mFd, mBuffer.data(), mBuffer.size());
    if (count < 0)
    {
        VerifyOrExit(errno == EAGAIN || errno == EINTR, error = OTBR_ERROR_ERRNO);
        ExitNow();
    }

    while (offset + sizeof(struct bpf_hdr) <= static_cast<size_t>(count))
    {
        struct bpf_hdr header;
        size_t         frameOffset;

        memcpy(&header, mBuffer.data() + offset, sizeof(header));
        frameOffset = offset + header.bh_hdrlen;
        VerifyOrExit(frameOffset + header.bh_caplen <= static_cast<size_t>(count), error = OTBR_ERROR_PARSE);

        aHandler(mBuffer.data() + frameOffset, header.bh_caplen);
        offset += BPF_WORDALIGN(header.bh_hdrlen + header.bh_caplen);
    }

exit:
    return error;
}

otbrError BpfTap::Write(const uint8_t *aFrame, size_t aLength)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(IsOpen(), error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(write(mFd, aFrame, aLength) == static_cast<ssize_t>(aLength), error = OTBR_ERROR_ERRNO);

exit:
    return error;
}

} // namespace otbr
