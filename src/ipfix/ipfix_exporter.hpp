/*
 *  Copyright (c) 2025, The OpenThread Authors.
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

/**
 * @file
 *   This file includes definitions for the IPFIX Exporter (exporting process).
 */

#ifndef OTBR_IPFIX_EXPORTER_HPP_
#define OTBR_IPFIX_EXPORTER_HPP_

#include "openthread-br/config.h"

#if OTBR_ENABLE_IPFIX

#include <chrono>
#include <cstdint>
#include <stdint.h>

extern "C" {
#include <fixbuf/public.h>
}

#include <openthread/instance.h>
#include <openthread/srp_server.h>
#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"
#include "host/rcp_host.hpp"

struct otInstance;
namespace otbr {

namespace Host {
class RcpHost;
}

/**
 * This class implements IPFIX Exporter (exporting process).
 */
class IpfixExporter : public MainloopProcessor
{
public:
    /**
     * This constructor initializes the IPFIX Exporter object.
     *
     * @param[in] aHost       The OpenThread controller instance.
     */
    explicit IpfixExporter(Host::RcpHost &aHost);
    ~IpfixExporter() = default;

    /**
     * This method starts the IPFIX exporter (exporting process).
     */
    void Start();

    /**
     * This method stops the IPFIX exporter (exporting process).
     */
    void Stop();

    /**
     * This method checks if the IPFIX exporter (exporter process) is started or not.
     *
     * @returns  true if the exporter is started.
     * @returns  false if the exporter is not started.
     */
    bool IsStarted() const { return mStarted; }

private:
    otInstance *GetInstance(void) const { return mHost.GetInstance(); }

    /**
     * This method updates the mainloop context for the next IPFIX export time.
     *
     * @param[in] aMainloop  A reference to the mainloop to be updated.
     */
    void Update(MainloopContext &aMainloop) override;

    /**
     * This method processes mainloop events to export the IPFIX flow records in IPFIX messages at each export time.
     *
     * @param[in] aMainloop  A reference to the mainloop context.
     */
    void Process(const MainloopContext &aMainloop) override;

    /**
     * This method schedules the time for the next IPFIX export time.
     *
     */
    void ScheduleNextTick();

    using Clock = std::chrono::steady_clock;
    Host::RcpHost &mHost;
    bool           mStarted{false};

    std::chrono::seconds mPeriod{60}; // Time period between two consecutive IPFIX export event (60 seconds by default).
    Clock::time_point    mNextTick{}; // Scheduled time for the next IPFIX export
    uint64_t mOtbrSystemInitTimeMSec{0};      // OTBR system time at the initialisation of the IPFIX export module
    uint64_t mRealSystemInitEpochTimeMSec{0}; // Real epoch time at the initialisation of the IPFIX export module

    /**
     * Represents the libfixbuf defined structures used by the OTBR exporting process to export the IPFIX messages.
     */
    fbInfoModel_t *mInfoModel{nullptr};
    fbSession_t   *mSession{nullptr};
    fbExporter_t  *mExporter{nullptr};
    fbTemplate_t  *mTemplate{nullptr};
    fBuf_t        *mBuf{nullptr};
    uint16_t       mTemplateId{0};
};

} // namespace otbr

#endif // OTBR_ENABLE_IPFIX

#endif // OTBR_IPFIX_EXPORTER_HPP_
