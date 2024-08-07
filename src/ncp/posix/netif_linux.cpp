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

#ifdef __linux__

#define OTBR_LOG_TAG "NETIF"

#include "netif.hpp"

#include <assert.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"

#ifndef OTBR_POSIX_TUN_DEVICE
#define OTBR_POSIX_TUN_DEVICE "/dev/net/tun"
#endif

namespace otbr {

otbrError Netif::CreateTunDevice(const std::string &aInterfaceName)
{
    ifreq     ifr;
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(aInterfaceName.size() < IFNAMSIZ, error = OTBR_ERROR_INVALID_ARGS);

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if (aInterfaceName.size() > 0)
    {
        strncpy(ifr.ifr_name, aInterfaceName.c_str(), aInterfaceName.size());
    }
    else
    {
        strncpy(ifr.ifr_name, "wpan%d", IFNAMSIZ);
    }

    mTunFd = open(OTBR_POSIX_TUN_DEVICE, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    VerifyOrExit(mTunFd >= 0, error = OTBR_ERROR_ERRNO);

    VerifyOrExit(ioctl(mTunFd, TUNSETIFF, &ifr) == 0, error = OTBR_ERROR_ERRNO);

    mNetifName.assign(ifr.ifr_name, strlen(ifr.ifr_name));
    otbrLogInfo("Netif name: %s", mNetifName.c_str());

    VerifyOrExit(ioctl(mTunFd, TUNSETLINK, ARPHRD_NONE) == 0, error = OTBR_ERROR_ERRNO);

    ifr.ifr_mtu = static_cast<int>(kIp6Mtu);
    VerifyOrExit(ioctl(mIpFd, SIOCSIFMTU, &ifr) == 0, error = OTBR_ERROR_ERRNO);

exit:
    return error;
}

} // namespace otbr

#endif // __linux__
