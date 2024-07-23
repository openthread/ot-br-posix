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

#include <openthread-br/otbr-posix-config.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#if defined(__APPLE__)
#if OTBR_POSIX_CONFIG_MACOS_TUN_OPTION == OTBR_POSIX_CONFIG_MACOS_UTUN
#include <net/if_utun.h>
#endif
#if OTBR_POSIX_CONFIG_MACOS_TUN_OPTION == OTBR_POSIX_CONFIG_MACOS_TUN
#include <sys/ioccom.h>
// FIX ME: include the tun_ioctl.h file (challenging, as it's location depends on where the developer puts it)
#define TUNSIFHEAD _IOW('t', 96, int)
#define TUNGIFHEAD _IOR('t', 97, int)
#endif
#include <sys/kern_control.h>

#endif // defined(__APPLE__)

#if defined(__NetBSD__) || defined(__FreeBSD__)
#include <net/if_tun.h>
#endif // defined(__NetBSD__) || defined(__FreeBSD__)

#include "common/code_utils.hpp"
#include "common/logging.hpp"

#ifndef OTBR_POSIX_TUN_DEVICE
#if defined(__NetBSD__) || defined(__FreeBSD__)
#define OTBR_POSIX_TUN_DEVICE "/dev/tun0"
#elif defined(__APPLE__)
#if OTBR_POSIX_CONFIG_MACOS_TUN_OPTION == OTBR_POSIX_CONFIG_MACOS_UTUN
#define OTBR_POSIX_TUN_DEVICE // not used - calculated dynamically
#elif OTBR_POSIX_CONFIG_MACOS_TUN_OPTION == OTBR_POSIX_CONFIG_MACOS_TUN
#define OTBR_POSIX_TUN_DEVICE "/dev/tun0"
#endif
#endif // defined(__APPLE__)
#else
#define OTBR_POSIX_TUN_DEVICE "/dev/net/tun"
#endif

namespace otbr {
namespace Posix {

// TODO: implement platform netif functionalities on unix platforms: APPLE, NetBSD, OpenBSD
//
// Currently we let otbr-agent can be compiled on unix platforms and can work under RCP mode
// but NCP mode cannot be used on unix platforms. It will crash at code here.

void Netif::PlatInit(void)
{
    DieNow("OTBR posix not supported on this platform");
}

#if defined(__APPLE__) && (OTBR_POSIX_CONFIG_MACOS_TUN_OPTION == OTBR_POSIX_CONFIG_MACOS_UTUN)
void Netif::PlatConfigureTunDevice(void)
{
    DieNow("OTBR posix not supported on this platform");
}
#endif // defined(__APPLE__) && (OTBR_POSIX_CONFIG_MACOS_TUN_OPTION == OTBR_POSIX_CONFIG_MACOS_UTUN)

#if defined(__NetBSD__) ||                                                                         \
    (defined(__APPLE__) && (OTBR_POSIX_CONFIG_MACOS_TUN_OPTION == OTBR_POSIX_CONFIG_MACOS_TUN)) || \
    defined(__FreeBSD__)
void Netif::PlatConfigureTunDevice(void)
{
    DieNow("OTBR posix not supported on this platform");
}
#endif

void Netif::PlatConfigureNetLink(void)
{
    DieNow("OTBR posix not supported on this platform");
}

void Netif::PlatProcessNetlinkEvent(void)
{
    DieNow("OTBR posix not supported on this platform");
}

void Netif::PlatUpdateUnicast(const Ip6AddressInfo &aAddressInfo, bool aIsAdded)
{
    OTBR_UNUSED_VARIABLE(aAddressInfo);
    OTBR_UNUSED_VARIABLE(aIsAdded);
    DieNow("OTBR posix not supported on this platform");
}

} // namespace Posix
} // namespace otbr

#endif // __APPLE__ || __NetBSD__ || __OpenBSD__
