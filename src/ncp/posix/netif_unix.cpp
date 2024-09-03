/*
 *  Copyright (c) 2024, The OpenThread Authors.
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

#if defined(__APPLE__) || defined(__NetBSD__) || defined(__OpenBSD__)

#define OTBR_LOG_TAG "NETIF"

#include "netif.hpp"

#include "common/code_utils.hpp"

namespace otbr {

// TODO: implement platform netif functionalities on unix platforms: APPLE, NetBSD, OpenBSD
//
// Currently we let otbr-agent can be compiled on unix platforms and can work under RCP mode
// but NCP mode cannot be used on unix platforms. It will crash at code here.

otbrError Netif::CreateTunDevice(const std::string &aInterfaceName)
{
    OTBR_UNUSED_VARIABLE(aInterfaceName);
    DieNow("OTBR posix not supported on this platform");
    return OTBR_ERROR_NONE;
}

otbrError Netif::InitNetlink(void)
{
    return OTBR_ERROR_NONE;
}

void Netif::PlatformSpecificInit(void)
{
    /* Empty */
}

void Netif::ProcessUnicastAddressChange(const Ip6AddressInfo &aAddressInfo, bool aIsAdded)
{
    OTBR_UNUSED_VARIABLE(aAddressInfo);
    OTBR_UNUSED_VARIABLE(aIsAdded);
}

} // namespace otbr

#endif // __APPLE__ || __NetBSD__ || __OpenBSD__
