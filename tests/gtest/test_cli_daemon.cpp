/*
 *    Copyright (c) 2025, The OpenThread Authors.
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <ifaddrs.h>
#include <iostream>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

#include <openthread/ip6.h>

#include "common/types.hpp"
#include "host/posix/cli_daemon.hpp"

// Only Test on linux platform for now.
#ifdef __linux__

TEST(CliDaemon, InitSocketCreationWithFullNetIfName)
{
    const char *netIfName  = "tun0";
    const char *socketFile = "/run/openthread-tun0.sock";
    const char *lockFile   = "/run/openthread-tun0.lock";

    otbr::CliDaemon cliDaemon;
    cliDaemon.Init(netIfName);

    struct stat st;

    EXPECT_EQ(stat(socketFile, &st), 0);
    EXPECT_EQ(stat(lockFile, &st), 0);
}

TEST(CliDaemon, InitSocketCreationWithEmptyNetIfName)
{
    const char *socketFile = "/run/openthread-wpan0.sock";
    const char *lockFile   = "/run/openthread-wpan0.lock";

    otbr::CliDaemon cliDaemon;
    cliDaemon.Init("");

    struct stat st;

    EXPECT_EQ(stat(socketFile, &st), 0);
    EXPECT_EQ(stat(lockFile, &st), 0);
}

#endif // __linux__
