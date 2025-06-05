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

#if OTBR_ENABLE_TELEMETRY_DATA_API && OTBR_ENABLE_BORDER_AGENT

#define OTBR_LOG_TAG "TEST"

#include <openthread/history_tracker.h>
#include <openthread/platform/alarm-milli.h>

#include "common/logging.hpp"
#include "proto/thread_telemetry.pb.h"
#include "utils/telemetry_retriever_border_agent.hpp"

// Mock implementations
void otbrLog(otbrLogLevel, const char *, const char *, ...)
{
}

uint32_t otPlatAlarmMilliGetNow(void)
{
    static uint32_t sNow = 1000000;
    return sNow++;
}

class TestEpskcEventTracker
{
public:
    class Iterator : public otHistoryTrackerIterator
    {
    public:
        void     Init(void) { ResetEntryNumber(), SetInitTime(); }
        uint16_t GetEntryNumber(void) const { return mData16; }
        void     ResetEntryNumber(void) { mData16 = 0; }
        void     IncrementEntryNumber(void) { mData16++; }
        uint32_t GetInitTime(void) const { return mData32; }
        void     SetInitTime(void) { mData32 = otPlatAlarmMilliGetNow(); }
    };

    void AddEpskcEvent(otHistoryTrackerBorderAgentEpskcEvent aEvent)
    {
        mEvents.push_back({aEvent, otPlatAlarmMilliGetNow()});
    }

    std::vector<std::pair<otHistoryTrackerBorderAgentEpskcEvent, uint32_t>> mEvents;
};

static TestEpskcEventTracker sEventTracker;

void otHistoryTrackerInitIterator(otHistoryTrackerIterator *aIterator)
{
    static_cast<TestEpskcEventTracker::Iterator *>(aIterator)->Init();
}

const otHistoryTrackerBorderAgentEpskcEvent *otHistoryTrackerIterateBorderAgentEpskcEventHistory(
    otInstance               *aInstance,
    otHistoryTrackerIterator *aIterator,
    uint32_t                 *aEntryAge)
{
    (void)aInstance;

    TestEpskcEventTracker::Iterator       *iterator = static_cast<TestEpskcEventTracker::Iterator *>(aIterator);
    uint16_t                               entryNum = iterator->GetEntryNumber();
    otHistoryTrackerBorderAgentEpskcEvent *result   = nullptr;

    if (entryNum < sEventTracker.mEvents.size())
    {
        uint16_t reverseIndex = sEventTracker.mEvents.size() - entryNum - 1;
        result                = &sEventTracker.mEvents[reverseIndex].first;
        *aEntryAge            = iterator->GetInitTime() - sEventTracker.mEvents[reverseIndex].second;
        iterator->IncrementEntryNumber();
    }

    return result;
}

// Test cases
TEST(Telemetry, RetrieveEpskcJourneyInfoCorrectly)
{
    otbr::agent::TelemetryRetriever::BorderAgent retriever(nullptr);
    threadnetwork::TelemetryData                 telemetryData;
    auto borderAgentInfo = telemetryData.mutable_wpan_border_router()->mutable_border_agent_info();

    // 1. Add a basic Epskc journey and verify the fields are correct.
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_ACTIVATED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_CONNECTED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_LOCAL_CLOSE);

    retriever.RetrieveEpskcJourneyInfo(borderAgentInfo);

    ASSERT_EQ(borderAgentInfo->border_agent_epskc_journey_info_size(), 1);
    auto epskcJourneyInfo = borderAgentInfo->border_agent_epskc_journey_info(0);
    ASSERT_TRUE(epskcJourneyInfo.has_activated_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_connected_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_petitioned_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_retrieved_active_dataset_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_retrieved_pending_dataset_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_keep_alive_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_deactivated_msec());
    ASSERT_EQ(epskcJourneyInfo.deactivated_reason(),
              threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_LOCAL_CLOSE);

    // 2. Add two Epskc journeys and verify that the previous one won't be uploaded again.
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_ACTIVATED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_MAX_ATTEMPTS);

    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_ACTIVATED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_CONNECTED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_PETITIONED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_REMOTE_CLOSE);

    borderAgentInfo->Clear();
    retriever.RetrieveEpskcJourneyInfo(borderAgentInfo);
    ASSERT_EQ(borderAgentInfo->border_agent_epskc_journey_info_size(), 2);

    epskcJourneyInfo = borderAgentInfo->border_agent_epskc_journey_info(0);
    ASSERT_TRUE(epskcJourneyInfo.has_activated_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_connected_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_petitioned_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_retrieved_active_dataset_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_retrieved_pending_dataset_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_keep_alive_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_deactivated_msec());
    ASSERT_EQ(epskcJourneyInfo.deactivated_reason(),
              threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_MAX_ATTEMPTS);

    epskcJourneyInfo = borderAgentInfo->border_agent_epskc_journey_info(1);
    ASSERT_TRUE(epskcJourneyInfo.has_activated_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_connected_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_petitioned_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_deactivated_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_retrieved_active_dataset_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_retrieved_pending_dataset_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_keep_alive_msec());
    ASSERT_EQ(epskcJourneyInfo.deactivated_reason(),
              threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_REMOTE_CLOSE);

    // 3. Add an uncompleted Epskc journey and verify that nothing will be fetched.
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_ACTIVATED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_CONNECTED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_PETITIONED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_RETRIEVED_ACTIVE_DATASET);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_RETRIEVED_PENDING_DATASET);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_KEEP_ALIVE);

    borderAgentInfo->Clear();
    retriever.RetrieveEpskcJourneyInfo(borderAgentInfo);
    ASSERT_EQ(borderAgentInfo->border_agent_epskc_journey_info_size(), 0);

    // 4. Complete the last Epskc journey and add one more journey. Verify that there are two journeys.
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_SESSION_TIMEOUT);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_ACTIVATED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_CONNECTED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_PETITIONED);
    sEventTracker.AddEpskcEvent(OT_HISTORY_TRACKER_BORDER_AGENT_EPSKC_EVENT_DEACTIVATED_SESSION_ERROR);

    borderAgentInfo->Clear();
    retriever.RetrieveEpskcJourneyInfo(borderAgentInfo);
    ASSERT_EQ(borderAgentInfo->border_agent_epskc_journey_info_size(), 2);

    epskcJourneyInfo = borderAgentInfo->border_agent_epskc_journey_info(0);
    ASSERT_TRUE(epskcJourneyInfo.has_activated_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_connected_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_petitioned_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_retrieved_active_dataset_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_retrieved_pending_dataset_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_keep_alive_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_deactivated_msec());
    ASSERT_EQ(epskcJourneyInfo.deactivated_reason(),
              threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_SESSION_TIMEOUT);

    epskcJourneyInfo = borderAgentInfo->border_agent_epskc_journey_info(1);
    ASSERT_TRUE(epskcJourneyInfo.has_activated_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_connected_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_petitioned_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_retrieved_active_dataset_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_retrieved_pending_dataset_msec());
    ASSERT_FALSE(epskcJourneyInfo.has_keep_alive_msec());
    ASSERT_TRUE(epskcJourneyInfo.has_deactivated_msec());
    ASSERT_EQ(epskcJourneyInfo.deactivated_reason(),
              threadnetwork::TelemetryData::EPSKC_DEACTIVATED_REASON_SESSION_ERROR);
}

#endif // OTBR_ENABLE_TELEMETRY_DATA_API && OTBR_ENABLE_BORDER_AGENT
