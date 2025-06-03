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

#if OTBR_ENABLE_TELEMETRY_DATA_API && OTBR_ENABLE_BORDER_AGENT

#define OTBR_LOG_TAG "UTILS"

#include "utils/telemetry_retriever_border_agent.hpp"

#include <algorithm>

#include <openthread/history_tracker.h>
#include <openthread/platform/alarm-milli.h>

#include "common/logging.hpp"

namespace otbr {
namespace agent {
namespace TelemetryRetriever {

BorderAgent::BorderAgent(otInstance *aInstance)
    : mInstance(aInstance)
    , mEpskcLastRetrievedTimestamp(0)
{
}

void BorderAgent::RetrieveEpskcJourneyInfo(threadnetwork::TelemetryData::BorderAgentInfo *aBorderAgentInfo)
{
    const auto                                               &unRetrievedEpskcEvents = GetUnretrievedEpskcEvents();
    threadnetwork::TelemetryData_BorderAgentEpskcJourneyInfo *epskcJourney           = nullptr;
    bool                                                      journeyCompleted       = true;

    for (size_t i = 0; i < unRetrievedEpskcEvents.size(); i++)
    {
        if (journeyCompleted)
        {
            epskcJourney     = aBorderAgentInfo->add_border_agent_epskc_journey_info();
            journeyCompleted = false;
        }

        switch (unRetrievedEpskcEvents[i].first)
        {
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_ACTIVATED:
            epskcJourney->set_activated_msec(unRetrievedEpskcEvents[i].second);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_CONNECTED:
            epskcJourney->set_connected_msec(unRetrievedEpskcEvents[i].second);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_PETITIONED:
            epskcJourney->set_petitioned_msec(unRetrievedEpskcEvents[i].second);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_RETRIEVED_ACTIVE_DATASET:
            epskcJourney->set_retrieved_active_dataset_msec(unRetrievedEpskcEvents[i].second);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_RETRIEVED_PENDING_DATASET:
            epskcJourney->set_retrieved_pending_dataset_msec(unRetrievedEpskcEvents[i].second);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_KEEP_ALIVE:
            epskcJourney->set_keep_alive_msec(unRetrievedEpskcEvents[i].second);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_LOCAL_CLOSE:
            epskcJourney->set_deactivated_reason(threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_LOCAL_CLOSE);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_REMOTE_CLOSE:
            epskcJourney->set_deactivated_reason(threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_REMOTE_CLOSE);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_SESSION_ERROR:
            epskcJourney->set_deactivated_reason(threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_SESSION_ERROR);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_SESSION_TIMEOUT:
            epskcJourney->set_deactivated_reason(
                threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_SESSION_TIMEOUT);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_MAX_ATTEMPTS:
            epskcJourney->set_deactivated_reason(threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_MAX_ATTEMPTS);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_EPSKC_TIMEOUT:
            epskcJourney->set_deactivated_reason(threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_EPSKC_TIMEOUT);
            break;
        case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_UNKNOWN:
            epskcJourney->set_deactivated_reason(threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_UNKNOWN);
            break;
        default:
            // Unexpected event type, log a warning.
            otbrLogWarning("Unexpected ePSKc event type: %d", unRetrievedEpskcEvents[i].first);
            break;
        }

        if (IsEpskcDeactivationEvent(unRetrievedEpskcEvents[i].first))
        {
            epskcJourney->set_deactivated_msec(unRetrievedEpskcEvents[i].second);
            journeyCompleted = true;
        }
    }
}

bool BorderAgent::IsEpskcDeactivationEvent(const otHistoryTrackerBorderAgentEpskcEvent &aEvent)
{
    bool result = false;

    switch (aEvent)
    {
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_ACTIVATED:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_CONNECTED:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_PETITIONED:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_RETRIEVED_ACTIVE_DATASET:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_RETRIEVED_PENDING_DATASET:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_KEEP_ALIVE:
        result = false;
        break;
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_LOCAL_CLOSE:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_REMOTE_CLOSE:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_SESSION_ERROR:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_SESSION_TIMEOUT:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_MAX_ATTEMPTS:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_EPSKC_TIMEOUT:
    case OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_UNKNOWN:
        result = true;
        break;
    default:
        result = false;
        otbrLogWarning("Unexpected ePSKc event type: %d", aEvent);
        break;
    }

    return result;
}

std::vector<std::pair<otHistoryTrackerBorderAgentEpskcEvent, uint32_t>> BorderAgent::GetUnretrievedEpskcEvents(void)
{
    std::vector<std::pair<otHistoryTrackerBorderAgentEpskcEvent, uint32_t>> unRetrievedEvents;

    const otHistoryTrackerBorderAgentEpskcEvent *epskcJourney = nullptr;
    otHistoryTrackerIterator                     iter;
    uint32_t                                     age;
    uint32_t                                     curTimestamp;

    otHistoryTrackerInitIterator(&iter);
    curTimestamp = otPlatAlarmMilliGetNow();

    while ((epskcJourney = otHistoryTrackerIterateBorderAgentEpskcEventHistory(mInstance, &iter, &age)) != nullptr)
    {
        /*
         *       event   event   last retrieve time T1             current time T2
         *         ^       ^             ^                              ^
         * | ----- x ----- x ----------- | ------------ x ----- x ----- |
         *                                              ^
         *                                          age < T2 - T1
         */
        if (age < curTimestamp - mEpskcLastRetrievedTimestamp)
        {
            unRetrievedEvents.push_back({*epskcJourney, curTimestamp - age});
        }
        else
        {
            break;
        }
    }

    std::reverse(unRetrievedEvents.begin(), unRetrievedEvents.end());

    ///< Ensure the events retrieved make up a complete journey. The remaining events will be uploaded next time.
    while (!unRetrievedEvents.empty() && !IsEpskcDeactivationEvent(unRetrievedEvents.back().first))
    {
        unRetrievedEvents.pop_back();
    }
    if (!unRetrievedEvents.empty())
    {
        mEpskcLastRetrievedTimestamp = unRetrievedEvents.back().second;
    }

    return unRetrievedEvents;
}

} // namespace TelemetryRetriever
} // namespace agent
} // namespace otbr

#endif // OTBR_ENABLE_TELEMETRY_DATA_API && OTBR_ENABLE_BORDER_AGENT
