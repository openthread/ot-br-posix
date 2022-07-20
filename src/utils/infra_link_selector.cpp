/*
 *    Copyright (c) 2022, The OpenThread Authors.
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
 *   The file implements the Infrastructure Link Selector.
 */

#if __linux__

#define OTBR_LOG_TAG "ILS"

#include "utils/infra_link_selector.hpp"

#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openthread/backbone_router_ftd.h>

#include "utils/socket_utils.hpp"

namespace otbr {
namespace Utils {

constexpr std::chrono::milliseconds InfraLinkSelector::kInfraLinkSelectionDelay;

InfraLinkSelector::InfraLinkSelector(std::vector<const char *> aInfraLinkNames)
    : mInfraLinkNames(std::move(aInfraLinkNames))
{
    if (mInfraLinkNames.size() >= 2)
    {
        mNetlinkSocket = CreateNetLinkRouteSocket(RTMGRP_LINK);
        VerifyOrDie(mNetlinkSocket != -1, "Failed to create netlink socket");
    }
}

InfraLinkSelector::~InfraLinkSelector(void)
{
    if (mNetlinkSocket != -1)
    {
        close(mNetlinkSocket);
        mNetlinkSocket = -1;
    }
}

const char *InfraLinkSelector::Select(void)
{
    const char *   oldInfraLink = mCurrentInfraLink;
    auto           now          = Clock::now();
    constexpr auto kMinTime     = Clock::time_point::min();

    VerifyOrExit(!mInfraLinkNames.empty(), mCurrentInfraLink = kDefaultInfraLinkName);
    VerifyOrExit(mInfraLinkNames.size() > 1, mCurrentInfraLink = mInfraLinkNames.front());
    VerifyOrExit(mRequireReselect, assert(mCurrentInfraLink != nullptr));

    if (mCurrentInfraLink != nullptr)
    {
        // Prefer `mCurrentInfraLink` if it's up and running.
        if (GetInfraLinkState(mCurrentInfraLink) == kUpAndRunning)
        {
            mCurrentInfraLinkDownTime = kMinTime;
            ExitNow();
        }

        if (mCurrentInfraLinkDownTime == kMinTime)
        {
            mCurrentInfraLinkDownTime = now;
        }

        // Prefer `mCurrentInfraLink` if it's down for less than `kInfraLinkSelectionDelay`
        if (now - mCurrentInfraLinkDownTime < kInfraLinkSelectionDelay)
        {
            std::chrono::milliseconds delay =
                kInfraLinkSelectionDelay -
                std::chrono::duration_cast<std::chrono::milliseconds>(now - mCurrentInfraLinkDownTime);

            mTaskRunner.Post(delay, [this]() { mRequireReselect = true; });
            ExitNow();
        }
    }

    {
        const char *bestInfraLink;
        LinkState   bestState;

        EvaluateInfraLinks(bestInfraLink, bestState);

        // Prefer `mCurrentInfraLink` if no other infra link is up and running
        VerifyOrExit(mCurrentInfraLink == nullptr || bestState == kUpAndRunning);

        // Prefer the infra link in the best state
        assert(mCurrentInfraLink != bestInfraLink);

        mCurrentInfraLink         = bestInfraLink;
        mCurrentInfraLinkDownTime = bestState == kUpAndRunning ? kMinTime : now;
    }

exit:
    mRequireReselect = false;

    if (oldInfraLink != mCurrentInfraLink)
    {
        if (oldInfraLink == nullptr)
        {
            otbrLogNotice("Infra link selected: %s", mCurrentInfraLink);
        }
        else
        {
            otbrLogWarning("Infra link switched from %s to %s", oldInfraLink, mCurrentInfraLink);
        }
    }

    return mCurrentInfraLink;
}

void InfraLinkSelector::EvaluateInfraLinks(const char *&aBestInfraLink, LinkState &aBestInfraLinkState)
{
    const char *                 bestStateInfraLink = nullptr;
    InfraLinkSelector::LinkState bestState          = kInvalid;

    otbrLogInfo("Evaluating infra link among %zu netifs:", mInfraLinkNames.size());

    for (const char *name : mInfraLinkNames)
    {
        InfraLinkSelector::LinkState state = GetInfraLinkState(name);

        otbrLogInfo("\tInfra link %s is in state %d", name, state);
        if (bestStateInfraLink == nullptr || state > bestState)
        {
            bestStateInfraLink = name;
            bestState          = state;
        }
    }

    aBestInfraLink      = bestStateInfraLink;
    aBestInfraLinkState = bestState;
}

InfraLinkSelector::LinkState InfraLinkSelector::GetInfraLinkState(const char *aInfraLinkName)
{
    int                          sock = 0;
    struct ifreq                 ifReq;
    InfraLinkSelector::LinkState state = kInvalid;

    sock = SocketWithCloseExec(AF_INET6, SOCK_DGRAM, IPPROTO_IP, kSocketBlock);
    VerifyOrDie(sock != -1, "Failed to create AF_INET6 socket.");

    memset(&ifReq, 0, sizeof(ifReq));
    strncpy(ifReq.ifr_name, aInfraLinkName, sizeof(ifReq.ifr_name) - 1);

    VerifyOrExit(ioctl(sock, SIOCGIFFLAGS, &ifReq) != -1);

    state = (ifReq.ifr_flags & IFF_UP) ? ((ifReq.ifr_flags & IFF_RUNNING) ? kUpAndRunning : kUp) : kDown;

exit:
    if (sock != 0)
    {
        close(sock);
    }

    return state;
}

void InfraLinkSelector::Update(MainloopContext &aMainloop)
{
    if (mNetlinkSocket != -1)
    {
        FD_SET(mNetlinkSocket, &aMainloop.mReadFdSet);
        aMainloop.mMaxFd = std::max(mNetlinkSocket, aMainloop.mMaxFd);
    }
}

void InfraLinkSelector::Process(const MainloopContext &aMainloop)
{
    OTBR_UNUSED_VARIABLE(aMainloop);

    if (mNetlinkSocket != -1 && FD_ISSET(mNetlinkSocket, &aMainloop.mReadFdSet))
    {
        ReceiveNetLinkMessage();
    }
}
void InfraLinkSelector::ReceiveNetLinkMessage(void)
{
    const size_t kMaxNetLinkBufSize = 8192;
    ssize_t      len;
    union
    {
        nlmsghdr mHeader;
        uint8_t  mBuffer[kMaxNetLinkBufSize];
    } msgBuffer;

    len = recv(mNetlinkSocket, msgBuffer.mBuffer, sizeof(msgBuffer.mBuffer), 0);
    if (len < 0)
    {
        otbrLogWarning("Failed to receive netlink message: %s", strerror(errno));
        ExitNow();
    }

    for (struct nlmsghdr *header = &msgBuffer.mHeader; NLMSG_OK(header, static_cast<size_t>(len));
         header                  = NLMSG_NEXT(header, len))
    {
        struct ifinfomsg *ifinfo;

        switch (header->nlmsg_type)
        {
        case RTM_NEWLINK:
        case RTM_DELLINK:
            ifinfo = reinterpret_cast<struct ifinfomsg *>(NLMSG_DATA(header));
            VerifyOrExit(ifinfo->ifi_change & (IFF_UP | IFF_RUNNING));
            otbrLogInfo("RTM_NEWLINK index %d change %x flags %x ", ifinfo->ifi_index, ifinfo->ifi_change,
                        ifinfo->ifi_flags);
            mRequireReselect = true;
            break;
        case NLMSG_ERROR:
        {
            struct nlmsgerr *errMsg = reinterpret_cast<struct nlmsgerr *>(NLMSG_DATA(header));

            otbrLogWarning("netlink NLMSG_ERROR response: seq=%u, error=%d", header->nlmsg_seq, errMsg->error);
            break;
        }
        default:
            break;
        }
    }

exit:
    return;
}

} // namespace Utils
} // namespace otbr

#endif // __linux__
