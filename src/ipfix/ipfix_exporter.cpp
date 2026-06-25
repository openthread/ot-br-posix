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
 *   The file implements the IPFIX Exporter (exporting process).
 */

#define OTBR_LOG_TAG "IPFIX_EXPORTER"

#include "ipfix_exporter.hpp"

#if OTBR_ENABLE_IPFIX

#include <cstring>

extern "C" {
#include <arpa/inet.h>
#include <fixbuf/public.h>
#include <stddef.h>
}

#include <chrono>
#include <openthread/border_routing.h>
#include <openthread/ipfix.h>
#include <openthread/link.h>
#include <openthread/platform/time.h>
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/time.hpp"
#include "host/rcp_host.hpp"

#ifdef __linux__
#include <net/if.h>
#endif

#define IPFIX_COLLECTOR_HOST "mycollectorhost" ///< IP address of the IPFIX collector (collecting process).
#define IPFIX_COLLECTOR_PORT "4739" ///< Destination port number of the IPFIX collector (standard UDP IPFIX port).

// Definition of the PEN and IE IDs used for the entreprise specific information elements.
#define OT_IPFIX_PEN 32473
#define IE_ID_SRC_EXT_ADDR 700
#define IE_ID_DST_EXT_ADDR 701
#define IE_ID_SRC_RLOC16 702
#define IE_ID_DST_RLOC16 703
#define IE_ID_SRC_NETWORK 704
#define IE_ID_DST_NETWORK 705
#define IE_ID_FRAME_COUNT 706

namespace otbr {

IpfixExporter::IpfixExporter(Host::RcpHost &aHost)
    : mHost(aHost)
{
    mPeriod = std::chrono::seconds(60);
}

/**
 * Represents the IPFIX flow records to be sent in the IPFIX message by the exporting process.
 */

typedef struct __attribute__((packed))
{
    uint8_t  sourceIPv6Address[16];
    uint8_t  destinationIPv6Address[16];
    uint16_t sourceTransportPort;
    uint16_t destinationTransportPort;
    uint8_t  protocolIdentifier;
    uint8_t  icmpTypeIPv6;
    uint8_t  icmpCodeIPv6;
    uint64_t packetTotalCount;
    uint64_t octetTotalCount;
    uint64_t flowStartMilliseconds;
    uint64_t flowEndMilliseconds;
    uint8_t  threadSrcNetworkId;
    uint8_t  threadDstNetworkId;
    uint8_t  threadSrcExtAddr[8];
    uint8_t  threadDstExtAddr[8];
    uint16_t threadSrcRloc16;
    uint16_t threadDstRloc16;
    uint64_t threadFrameCount;
} IpfixExportRecord;

/**
 * Definition of the IPFIX template record to be sent in the IPFIX message by the exporting process.
 */

static fbInfoElementSpec_t ipfixTemplateSpec[] = {
    // Standard IANA information elements
    {(char *)"sourceIPv6Address", 16, 0},
    {(char *)"destinationIPv6Address", 16, 0},
    {(char *)"sourceTransportPort", 2, 0},
    {(char *)"destinationTransportPort", 2, 0},
    {(char *)"protocolIdentifier", 1, 0},
    {(char *)"icmpTypeIPv6", 1, 0},
    {(char *)"icmpCodeIPv6", 1, 0},
    {(char *)"packetTotalCount", 8, 0},
    {(char *)"octetTotalCount", 8, 0},
    {(char *)"flowStartMilliseconds", 8, 0},
    {(char *)"flowEndMilliseconds", 8, 0},
    // Entreprise specific information elements
    {(char *)"threadSrcNetworkId", 1, OT_IPFIX_PEN},
    {(char *)"threadDstNetworkId", 1, OT_IPFIX_PEN},
    {(char *)"threadSrcExtAddr", 8, OT_IPFIX_PEN},
    {(char *)"threadDstExtAddr", 8, OT_IPFIX_PEN},
    {(char *)"threadSrcRloc16", 2, OT_IPFIX_PEN},
    {(char *)"threadDstRloc16", 2, OT_IPFIX_PEN},
    {(char *)"threadFrameCount", 8, OT_IPFIX_PEN},
    FB_IESPEC_NULL};

/**
 * Returns the integer value (in uint8_t format) of the corresponding network interface ID.
 *
 * @param[in] aNetworkInterface  The network interface.
 * @param[in] aInstance  A pointer to the OpenThread instance.
 *
 * @returns 0 if aNetworkInterface = OT_IPFIX_INTERFACE_OTBR
 * @returns 1 if aNetworkInterface = OT_IPFIX_INTERFACE_THREAD_NETWORK
 * @returns 2 if aNetworkInterface = OT_IPFIX_INTERFACE_AIL_NETWORK
 * @returns 3 if aNetworkInterface = OT_IPFIX_INTERFACE_WIFI_NETWORK
 * @returns 4 if aNetworkInterface = OT_IPFIX_INTERFACE_ETHERNET_NETWORK
 *
 */

static uint8_t MapNetworkToId(otIpfixFlowInterface aNetworkInterface, otInstance *aInstance)
{
    if (aNetworkInterface == OT_IPFIX_INTERFACE_THREAD_NETWORK)
    {
        return (uint8_t)OT_IPFIX_INTERFACE_THREAD_NETWORK;
    }
    if (aNetworkInterface == OT_IPFIX_INTERFACE_OTBR)
    {
        return (uint8_t)OT_IPFIX_INTERFACE_OTBR;
    }

    uint32_t infraIfIndex = 0;
    bool     isRunning    = false;

    if (otBorderRoutingGetInfraIfInfo(aInstance, &infraIfIndex, &isRunning) == OT_ERROR_NONE)
    {
#ifdef __linux__
        char ifName[IF_NAMESIZE];
        if (if_indextoname(infraIfIndex, ifName) != nullptr)
        {
            if (ifName[0] == 'w')
            {
                return (uint8_t)OT_IPFIX_INTERFACE_WIFI_NETWORK;
            }
            else if (ifName[0] == 'e')
            {
                return (uint8_t)OT_IPFIX_INTERFACE_ETHERNET_NETWORK;
            }
        }
#endif
    }
    return (uint8_t)OT_IPFIX_INTERFACE_AIL_NETWORK;
}

void IpfixExporter::Start()
{
    GError *err = nullptr;

    mInfoModel = fbInfoModelAlloc();

    // Adding the custom enterpise specific information elements in the IPFIX information model
    fbInfoElement_t custom_elements[] = {
        FB_IE_INIT("threadSrcExtAddr", OT_IPFIX_PEN, IE_ID_SRC_EXT_ADDR, 8, FB_IE_F_NONE),
        FB_IE_INIT("threadDstExtAddr", OT_IPFIX_PEN, IE_ID_DST_EXT_ADDR, 8, FB_IE_F_NONE),
        FB_IE_INIT("threadSrcRloc16", OT_IPFIX_PEN, IE_ID_SRC_RLOC16, 2, FB_IE_F_ENDIAN),
        FB_IE_INIT("threadDstRloc16", OT_IPFIX_PEN, IE_ID_DST_RLOC16, 2, FB_IE_F_ENDIAN),
        FB_IE_INIT("threadSrcNetworkId", OT_IPFIX_PEN, IE_ID_SRC_NETWORK, 1, FB_IE_F_NONE),
        FB_IE_INIT("threadDstNetworkId", OT_IPFIX_PEN, IE_ID_DST_NETWORK, 1, FB_IE_F_NONE),
        FB_IE_INIT("threadFrameCount", OT_IPFIX_PEN, IE_ID_FRAME_COUNT, 8, FB_IE_F_ENDIAN),
        FB_IE_NULL};
    fbInfoModelAddElementArray(mInfoModel, custom_elements);

    mSession = fbSessionAlloc(mInfoModel);
    // Setting the observation domain ID to the PAN ID of the Thread network the OTBR is connected to
    otInstance *instance = mHost.GetInstance();
    uint16_t    panId    = otLinkGetPanId(instance);
    fbSessionSetDomain(mSession, (uint32_t)panId);
    otbrLogInfo("IPFIX Exporter : The observation domain ID is configured with PAN ID = 0x%04x", panId);

    uint64_t nowMonotonicMs = otPlatTimeGet() / 1000;
    auto     nowEpoch       = std::chrono::system_clock::now();
    uint64_t nowEpochMs = std::chrono::duration_cast<std::chrono::milliseconds>(nowEpoch.time_since_epoch()).count();
    mOtbrSystemInitTimeMSec      = nowMonotonicMs;
    mRealSystemInitEpochTimeMSec = nowEpochMs;

    // Configuration of the network connection with the IPFIX collector (collecting process)
    fbConnSpec_t spec;
    std::memset(&spec, 0, sizeof(spec));
    spec.host      = (char *)IPFIX_COLLECTOR_HOST;
    spec.svc       = (char *)IPFIX_COLLECTOR_PORT;
    spec.transport = FB_UDP;

    mExporter = fbExporterAllocNet(&spec);
    if (!mExporter)
    {
        return;
    }

    // Configuration of the IPFIX template record

    mTemplate = fbTemplateAlloc(mInfoModel);
    if (!fbTemplateAppendSpecArray(mTemplate, ipfixTemplateSpec, 0xffffffff, &err))
    {
        otbrLogErr("IPFIX Exporter : IPFIX template record Error : %s", err->message);
        g_clear_error(&err);
    }
    mTemplateId = fbSessionAddTemplate(mSession, TRUE, FB_TID_AUTO, mTemplate, &err);
    fbSessionAddTemplate(mSession, FALSE, mTemplateId, mTemplate, &err);

    // Configuration of the IPFIX export buffer
    mBuf = fBufAllocForExport(mSession, mExporter);
    if (!fBufSetInternalTemplate(mBuf, mTemplateId, &err))
    {
        if (err)
            g_clear_error(&err);
    }
    if (!fBufSetExportTemplate(mBuf, mTemplateId, &err))
    {
        if (err)
            g_clear_error(&err);
    }
    VerifyOrExit(!mStarted);
    mStarted = true;
    ScheduleNextTick();

exit:
    return;
}

void IpfixExporter::Stop()
{
    VerifyOrExit(mStarted);
    if (mBuf)
    {
        fBufFree(mBuf);
        mBuf = nullptr;
    }
    else
    {
        if (mSession)
            fbSessionFree(mSession);
    }
    if (mInfoModel)
        fbInfoModelFree(mInfoModel);

    mStarted = false;
    otbrLogInfo("IPFIX Exporter : The IPFIX Exporter has been stopped");

exit:
    return;
}

void IpfixExporter::ScheduleNextTick()
{
    mNextTick = Clock::now() + mPeriod;
}

void IpfixExporter::Update(MainloopContext &aMainloop)
{
    if (!mStarted)
        return;

    auto now     = Clock::now();
    auto delay   = std::chrono::duration_cast<Microseconds>(mNextTick - now);
    auto timeout = FromTimeval<Microseconds>(aMainloop.mTimeout);

    if (delay < Microseconds::zero())
        delay = Microseconds::zero();

    if (delay <= timeout)
    {
        aMainloop.mTimeout.tv_sec  = delay.count() / 1000000;
        aMainloop.mTimeout.tv_usec = delay.count() % 1000000;
    }
}

void IpfixExporter::Process(const MainloopContext &aMainloop)
{
    if (!mStarted)
        return;

    otInstance *instance = mHost.GetInstance();
    auto        now      = Clock::now();

    if (now >= mNextTick)
    {
        // Retrieving all the IPFIX flow records from the IPFIX hashtable
        uint16_t        myflow_count = otIpfixGetFlowCount(instance);
        otIpfixFlowInfo myflow_buffer[OT_IPFIX_MAX_FLOWS];
        otIpfixGetFlowTable(instance, myflow_buffer);

        if (mBuf)
        {
            GError *err = nullptr;

            // Exporting the IPFIX template record to the the collecting process (IPFIX collector)

            if (!fbSessionExportTemplates(mSession, &err))
            {
                if (err)
                {
                    otbrLogWarning("IPFIX Exporter : IPFIX template record export failed: %s", err->message);
                    g_clear_error(&err);
                }
            }

            // Adding all the IPFIX flow data records to the IPFIX export buffer (IPFIX message)

            if (myflow_count > 0)
            {
                for (uint16_t i = 0; i < myflow_count; i++)
                {
                    const otIpfixFlowInfo &otFlow = myflow_buffer[i];
                    IpfixExportRecord      record;
                    std::memset(&record, 0, sizeof(record));

                    memcpy(record.sourceIPv6Address, otFlow.mSourceAddress.mFields.m8, 16);
                    memcpy(record.destinationIPv6Address, otFlow.mDestinationAddress.mFields.m8, 16);
                    record.sourceTransportPort      = otFlow.mSourcePort;
                    record.destinationTransportPort = otFlow.mDestinationPort;
                    record.protocolIdentifier       = otFlow.mIpProto;
                    record.icmpTypeIPv6             = otFlow.mIcmp6Type;
                    record.icmpCodeIPv6             = otFlow.mIcmp6Code;
                    record.packetTotalCount         = otFlow.mPacketsCount;
                    record.octetTotalCount          = otFlow.mBytesCount;
                    if (otFlow.mFlowStartTime >= mOtbrSystemInitTimeMSec)
                    {
                        record.flowStartMilliseconds =
                            mRealSystemInitEpochTimeMSec + (otFlow.mFlowStartTime - mOtbrSystemInitTimeMSec);
                    }
                    else
                    {
                        record.flowStartMilliseconds = mRealSystemInitEpochTimeMSec;
                    }
                    if (otFlow.mFlowEndTime >= mOtbrSystemInitTimeMSec)
                    {
                        record.flowEndMilliseconds =
                            mRealSystemInitEpochTimeMSec + (otFlow.mFlowEndTime - mOtbrSystemInitTimeMSec);
                    }
                    else
                    {
                        record.flowEndMilliseconds = mRealSystemInitEpochTimeMSec;
                    }
                    record.threadSrcNetworkId = MapNetworkToId(otFlow.mSourceNetwork, instance);
                    record.threadDstNetworkId = MapNetworkToId(otFlow.mDestinationNetwork, instance);
                    memcpy(record.threadSrcExtAddr, otFlow.mThreadSrcMacAddress.m8, 8);
                    memcpy(record.threadDstExtAddr, otFlow.mThreadDestMacAddress.m8, 8);
                    record.threadSrcRloc16  = otFlow.mThreadSrcRloc16Address;
                    record.threadDstRloc16  = otFlow.mThreadDestRloc16Address;
                    record.threadFrameCount = otFlow.mThreadFramesCount;

                    if (!fBufAppend(mBuf, (uint8_t *)&record, sizeof(record), &err))
                    {
                        otbrLogWarning("IPFIX Exporter : IPFIX data record export failed for flow %d", i);
                        if (err)
                            g_clear_error(&err);
                    }
                }
            }

            // Exporting the IPFIX data records to the collecting process (IPFIX collector)
            if (!fBufEmit(mBuf, &err))
            {
                otbrLogErr("IPFIX Exporter : Export of the IPFIX message failed");
                if (err)
                    g_clear_error(&err);
            }
            else
            {
                if (myflow_count > 0)
                {
                    otbrLogInfo("IPFIX Exporter : Exported %u IPFIX records.", myflow_count);
                }
            }
        }
        // Resetting the IPFIX hashtable and scheduling the next export time
        otIpfixResetFlowTable(instance);
        ScheduleNextTick();
    }
    OTBR_UNUSED_VARIABLE(aMainloop);
}

} // namespace otbr

#endif // OTBR_ENABLE_IPFIX