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
 *   This file include definitions for the Infrastructure Link Selector.
 */

#ifndef INFRA_LINK_SELECTOR_HPP_
#define INFRA_LINK_SELECTOR_HPP_

#if __linux__

#include <assert.h>
#include <chrono>
#include <utility>
#include <vector>

#include <openthread/backbone_router_ftd.h>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/task_runner.hpp"

namespace otbr {
namespace Utils {

/**
 * This class implements Infrastructure Link Selector.
 *
 */
class InfraLinkSelector : public MainloopProcessor, private NonCopyable
{
public:
    /**
     * This enumeration infrastructure link states.
     *
     */
    enum LinkState : uint8_t
    {
        kInvalid,      ///< The infrastructure link is invalid.
        kDown,         ///< The infrastructure link is down.
        kUp,           ///< The infrastructure link is up, but not running.
        kUpAndRunning, ///< The infrastructure link is up and running.
    };

    /**
     * This constructor initializes the InfraLinkSelector instance.
     *
     * @param[in]  aInfraLinkNames  A list of infrastructure link candidates to select from.
     *
     */
    explicit InfraLinkSelector(std::vector<const char *> aInfraLinkNames);

    /**
     * This destructor destroys the InfraLinkSelector instance.
     *
     */
    ~InfraLinkSelector(void);

    /**
     * This method selects an infrastructure link among infrastructure link candidates using rules below:
     *
     * The infrastructure link in the most usable state is selected:
     *      Prefer `up and running` to `up`
     *      Prefer `up` to `down`
     *      Prefer `down` to `invalid`
     * Once an interface is selected, it's preferred if either is true:
     *      The interface is still `up and running`
     *      No other interface is `up and running`
     *      The interface has been `up and running` within last 10 seconds
     *
     * @returns  The selected infrastructure link.
     *
     */
    const char *Select(void);

private:
    using Clock = std::chrono::steady_clock;

    static constexpr const char *kDefaultInfraLinkName    = "";
    static constexpr auto        kInfraLinkSelectionDelay = std::chrono::milliseconds(10000);

    void             EvaluateInfraLinks(const char *&aBestInfraLink, LinkState &aBestInfraLinkState);
    static LinkState IsInfraLinkRunning(const char *aInfraLinkName);

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;
    void ReceiveNetLinkMessage(void);

    std::vector<const char *> mInfraLinkNames;
    int                       mNetlinkSocket    = -1;
    const char *              mCurrentInfraLink = nullptr;
    Clock::time_point         mCurrentInfraLinkDownTime;
    TaskRunner                mTaskRunner;
    bool                      mRequireReselect = true;
};

} // namespace Utils
} // namespace otbr

#endif // __linux__

#endif // INFRA_LINK_SELECTOR_HPP_
