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

constexpr Milliseconds InfraLinkSelector::kInfraLinkSelectionDelay;

bool InfraLinkSelector::LinkInfo::Update(LinkState aState)
{
    bool changed = mState != aState;

    VerifyOrExit(changed);

    if (mState == kUpAndRunning && aState != kUpAndRunning)
    {
        mWasUpAndRunning = true;
        mLastRunningTime = Clock::now();
    }

    mState = aState;
exit:
    return changed;
}

InfraLinkSelector::InfraLinkSelector(std::vector<const char *> aInfraLinkNames)
    : mInfraLinkNames(std::move(aInfraLinkNames))
{
    if (mInfraLinkNames.size() >= 2)
    {
        mNetlinkSocket = CreateNetLinkRouteSocket(RTMGRP_LINK);
        VerifyOrDie(mNetlinkSocket != -1, "Failed to create netlink socket");
    }

    for (const char *name : mInfraLinkNames)
    {
        mInfraLinkInfos[name].Update(QueryInfraLinkState(name));
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
    const char *sel;

#if OTBR_ENABLE_VENDOR_INFRA_LINK_SELECT
    sel = otbrVendorInfraLinkSelect();

    if (sel == nullptr)
    {
        sel = SelectGeneric();
    }
#else
    sel = SelectGeneric();
#endif

    return sel;
}

const char *InfraLinkSelector::SelectGeneric(void)
{
    const char                  *prevInfraLink         = mCurrentInfraLink;
    InfraLinkSelector::LinkState currentInfraLinkState = kInvalid;
    auto                         now                   = Clock::now();
    LinkInfo                    *currentInfraLinkInfo  = nullptr;

    VerifyOrExit(!mInfraLinkNames.empty(), mCurrentInfraLink = kDefaultInfraLinkName);
    VerifyOrExit(mInfraLinkNames.size() > 1, mCurrentInfraLink = mInfraLinkNames.front());
    VerifyOrExit(mRequireReselect, assert(mCurrentInfraLink != nullptr));

    otbrLogInfo("Evaluating infra link among %zu netifs:", mInfraLinkNames.size());

    // Prefer `mCurrentInfraLink` if it's up and running.
    if (mCurrentInfraLink != nullptr)
    {
        currentInfraLinkInfo = &mInfraLinkInfos[mCurrentInfraLink];

        otbrLogInfo("\tInfra link %s is in state %s", mCurrentInfraLink,
                    LinkStateToString(currentInfraLinkInfo->mState));

        VerifyOrExit(currentInfraLinkInfo->mState != kUpAndRunning);
    }

    // Select an infra link with best state.
    {
        const char                  *bestInfraLink = mCurrentInfraLink;
        InfraLinkSelector::LinkState bestState     = currentInfraLinkState;

        for (const char *name : mInfraLinkNames)
        {
            if (name != mCurrentInfraLink)
            {
                LinkInfo &linkInfo = mInfraLinkInfos[name];

                otbrLogInfo("\tInfra link %s is in state %s", name, LinkStateToString(linkInfo.mState));
                if (bestInfraLink == nullptr || linkInfo.mState > bestState)
                {
                    bestInfraLink = name;
                    bestState     = linkInfo.mState;
                }
            }
        }

        VerifyOrExit(bestInfraLink != mCurrentInfraLink);

        // Prefer `mCurrentInfraLink` if no other infra link is up and running
        VerifyOrExit(mCurrentInfraLink == nullptr || bestState == kUpAndRunning);

        // Prefer `mCurrentInfraLink` if it's down for less than `kInfraLinkSelectionDelay`
        if (mCurrentInfraLink != nullptr && currentInfraLinkInfo->mWasUpAndRunning)
        {
            Milliseconds timeSinceLastRunning =
                std::chrono::duration_cast<Milliseconds>(now - currentInfraLinkInfo->mLastRunningTime);

            if (timeSinceLastRunning < kInfraLinkSelectionDelay)
            {
                Milliseconds delay = kInfraLinkSelectionDelay - timeSinceLastRunning;

                otbrLogInfo("Infra link %s was running %lldms ago, wait for %lldms to recheck.", mCurrentInfraLink,
                            timeSinceLastRunning.count(), delay.count());
                mTaskRunner.Post(delay, [this]() { mRequireReselect = true; });
                ExitNow();
            }
        }

        // Current infra link changed.
        mCurrentInfraLink = bestInfraLink;
    }

exit:
    if (mRequireReselect)
    {
        mRequireReselect = false;

        if (prevInfraLink != mCurrentInfraLink)
        {
            if (prevInfraLink == nullptr)
            {
                otbrLogNotice("Infra link selected: %s", mCurrentInfraLink);
            }
            else
            {
                otbrLogWarning("Infra link switched from %s to %s", prevInfraLink, mCurrentInfraLink);
            }
        }
        else
        {
            otbrLogInfo("Infra link unchanged: %s", mCurrentInfraLink);
        }
    }

    return mCurrentInfraLink;
}

InfraLinkSelector::LinkState InfraLinkSelector::QueryInfraLinkState(const char *aInfraLinkName)
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
        aMainloop.AddFdToReadSet(mNetlinkSocket);
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
            HandleInfraLinkStateChange(ifinfo->ifi_index);
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

void InfraLinkSelector::HandleInfraLinkStateChange(uint32_t aInfraLinkIndex)
{
    const char *infraLinkName = nullptr;

    for (const char *name : mInfraLinkNames)
    {
        if (aInfraLinkIndex == if_nametoindex(name))
        {
            infraLinkName = name;
            break;
        }
    }

    VerifyOrExit(infraLinkName != nullptr);

    {
        LinkInfo &linkInfo  = mInfraLinkInfos[infraLinkName];
        LinkState prevState = linkInfo.mState;

        if (linkInfo.Update(QueryInfraLinkState(infraLinkName)))
        {
            otbrLogInfo("Infra link name %s index %lu state changed: %s -> %s", infraLinkName, aInfraLinkIndex,
                        LinkStateToString(prevState), LinkStateToString(linkInfo.mState));
            mRequireReselect = true;
        }
    }
exit:
    return;
}

const char *InfraLinkSelector::LinkStateToString(LinkState aState)
{
    const char *str = "";

    switch (aState)
    {
    case kInvalid:
        str = "INVALID";
        break;
    case kDown:
        str = "DOWN";
        break;
    case kUp:
        str = "UP";
        break;
    case kUpAndRunning:
        str = "UP+RUNNING";
        break;
    }
    return str;
}

} // namespace Utils
} // namespace otbr

#endif // __linux__
