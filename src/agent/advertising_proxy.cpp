/*
 *    Copyright (c) 2020, The OpenThread Authors.
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
 *   The file implements the Advertising Proxy.
 */

#include "agent/advertising_proxy.hpp"

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY

#if !OTBR_ENABLE_MDNS_AVAHI && !OTBR_ENABLE_MDNS_MDNSSD && !OTBR_ENABLE_MDNS_MOJO
#error "The Advertising Proxy requires OTBR_ENABLE_MDNS_AVAHI, OTBR_ENABLE_MDNS_MDNSSD or OTBR_ENABLE_MDNS_MOJO"
#endif

#include <string>

#include <assert.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

namespace otbr {

// TODO: better to have SRP server provide separated service name labels.
static otbrError SplitFullServiceName(const std::string &aFullName,
                                      std::string &      aInstanceName,
                                      std::string &      aType,
                                      std::string &      aDomain)
{
    otbrError error = OTBR_ERROR_INVALID_ARGS;
    size_t    dotPos[3];

    dotPos[0] = aFullName.find_first_of('.');
    VerifyOrExit(dotPos[0] != std::string::npos);

    dotPos[1] = aFullName.find_first_of('.', dotPos[0] + 1);
    VerifyOrExit(dotPos[1] != std::string::npos);

    dotPos[2] = aFullName.find_first_of('.', dotPos[1] + 1);
    VerifyOrExit(dotPos[2] != std::string::npos);

    error         = OTBR_ERROR_NONE;
    aInstanceName = aFullName.substr(0, dotPos[0]);
    aType         = aFullName.substr(dotPos[0] + 1, dotPos[2] - dotPos[0] - 1);
    aDomain       = aFullName.substr(dotPos[2] + 1, aFullName.size() - dotPos[2] - 1);

exit:
    return error;
}

// TODO: better to have the SRP server to provide separated host name.
static otbrError SplitFullHostName(const std::string &aFullName, std::string &aHostName, std::string &aDomain)
{
    otbrError error = OTBR_ERROR_INVALID_ARGS;
    size_t    dotPos;

    dotPos = aFullName.find_first_of('.');
    VerifyOrExit(dotPos != std::string::npos);

    error     = OTBR_ERROR_NONE;
    aHostName = aFullName.substr(0, dotPos);
    aDomain   = aFullName.substr(dotPos + 1, aFullName.size() - dotPos - 1);

exit:
    return error;
}

static otError OtbrErrorToOtError(otbrError aError)
{
    otError error;

    switch (aError)
    {
    case OTBR_ERROR_NONE:
        error = OT_ERROR_NONE;
        break;

    case OTBR_ERROR_NOT_FOUND:
        error = OT_ERROR_NOT_FOUND;
        break;

    case OTBR_ERROR_PARSE:
        error = OT_ERROR_PARSE;
        break;

    case OTBR_ERROR_NOT_IMPLEMENTED:
        error = OT_ERROR_NOT_IMPLEMENTED;
        break;

    case OTBR_ERROR_INVALID_ARGS:
        error = OT_ERROR_INVALID_ARGS;
        break;

    case OTBR_ERROR_DUPLICATED:
        error = OT_ERROR_DUPLICATED;
        break;

    default:
        error = OT_ERROR_FAILED;
        break;
    }

    return error;
}

AdvertisingProxy::AdvertisingProxy(Ncp::ControllerOpenThread &aNcp, Mdns::Publisher &aPublisher)
    : mNcp(aNcp)
    , mPublisher(aPublisher)
{
}

otbrError AdvertisingProxy::Start(void)
{
    otSrpServerSetServiceUpdateHandler(GetInstance(), AdvertisingHandler, this);

    mPublisher.SetPublishServiceHandler(PublishServiceHandler, this);
    mPublisher.SetPublishHostHandler(PublishHostHandler, this);

    otbrLog(OTBR_LOG_INFO, "[adproxy] Started");

    return OTBR_ERROR_NONE;
}

void AdvertisingProxy::Stop()
{
    mPublisher.SetPublishServiceHandler(nullptr, nullptr);
    mPublisher.SetPublishHostHandler(nullptr, nullptr);

    // Outstanding updates will fail on the SRP server because of timeout.
    // TODO: handle this case gracefully.

    // Stop receiving SRP server events.
    if (GetInstance() != nullptr)
    {
        otSrpServerSetServiceUpdateHandler(GetInstance(), nullptr, nullptr);
    }

    otbrLog(OTBR_LOG_INFO, "[adproxy] Stopped");
}

void AdvertisingProxy::AdvertisingHandler(const otSrpServerHost *aHost, uint32_t aTimeout, void *aContext)
{
    static_cast<AdvertisingProxy *>(aContext)->AdvertisingHandler(aHost, aTimeout);
}

void AdvertisingProxy::AdvertisingHandler(const otSrpServerHost *aHost, uint32_t aTimeout)
{
    // TODO: There are corner cases that the `aHost` is freed by SRP server because
    // of timeout, but this `aHost` is passed back to SRP server and matches a newly
    // allocated otSrpServerHost object which has the same pointer value as this
    // `aHost`. This results in mismatching of the outstanding SRP updates. Solutions
    // are cleaning up the outstanding update entries before timing out or using
    // incremental ID to match oustanding SRP updates.
    OTBR_UNUSED_VARIABLE(aTimeout);

    otbrError                 error = OTBR_ERROR_NONE;
    const char *              fullHostName;
    std::string               hostName;
    std::string               hostDomain;
    const otIp6Address *      hostAddress;
    uint8_t                   hostAddressNum;
    bool                      hostDeleted;
    const otSrpServerService *service;
    OutstandingUpdate *       update;

    mOutstandingUpdates.resize(mOutstandingUpdates.size() + 1);
    update = &mOutstandingUpdates.back();

    fullHostName = otSrpServerHostGetFullName(aHost);

    otbrLog(OTBR_LOG_INFO, "[adproxy] advertise SRP service updates: host=%s", fullHostName);

    SuccessOrExit(error = SplitFullHostName(fullHostName, hostName, hostDomain));
    hostAddress = otSrpServerHostGetAddresses(aHost, &hostAddressNum);
    hostDeleted = otSrpServerHostIsDeleted(aHost);

    update->mHost = aHost;
    update->mCallbackCount += !hostDeleted;
    update->mHostName = hostName;

    service = nullptr;
    while ((service = otSrpServerHostGetNextService(aHost, service)) != nullptr)
    {
        update->mCallbackCount += !hostDeleted && !otSrpServerServiceIsDeleted(service);
    }

    if (!hostDeleted)
    {
        // TODO: select a preferred address or advertise all addresses from SRP client.
        otbrLog(OTBR_LOG_INFO, "[adproxy] publish SRP host: %s", fullHostName);
        SuccessOrExit(error =
                          mPublisher.PublishHost(hostName.c_str(), hostAddress[0].mFields.m8, sizeof(hostAddress[0])));
    }
    else
    {
        otbrLog(OTBR_LOG_INFO, "[adproxy] unpublish SRP host: %s", fullHostName);
        SuccessOrExit(error = mPublisher.UnpublishHost(hostName.c_str()));
    }

    service = nullptr;
    while ((service = otSrpServerHostGetNextService(aHost, service)) != nullptr)
    {
        const char *fullServiceName = otSrpServerServiceGetFullName(service);
        std::string serviceName;
        std::string serviceType;
        std::string serviceDomain;

        SuccessOrExit(error = SplitFullServiceName(fullServiceName, serviceName, serviceType, serviceDomain));

        update->mServiceNames.emplace_back(serviceName, serviceType);

        if (!hostDeleted && !otSrpServerServiceIsDeleted(service))
        {
            Mdns::Publisher::TxtList txtList = MakeTxtList(service);

            otbrLog(OTBR_LOG_INFO, "[adproxy] publish SRP service: %s", fullServiceName);
            SuccessOrExit(error = mPublisher.PublishService(hostName.c_str(), otSrpServerServiceGetPort(service),
                                                            serviceName.c_str(), serviceType.c_str(), txtList));
        }
        else
        {
            otbrLog(OTBR_LOG_INFO, "[adproxy] unpublish SRP service: %s", fullServiceName);
            SuccessOrExit(error = mPublisher.UnpublishService(serviceName.c_str(), serviceType.c_str()));
        }
    }

exit:
    if (error != OTBR_ERROR_NONE || update->mCallbackCount == 0)
    {
        if (error != OTBR_ERROR_NONE)
        {
            otbrLog(OTBR_LOG_INFO, "[adproxy] failed to advertise SRP service updates %p", aHost);
        }

        mOutstandingUpdates.pop_back();
        otSrpServerHandleServiceUpdateResult(GetInstance(), aHost, OtbrErrorToOtError(error));
    }
}

void AdvertisingProxy::PublishServiceHandler(const char *aName, const char *aType, otbrError aError, void *aContext)
{
    static_cast<AdvertisingProxy *>(aContext)->PublishServiceHandler(aName, aType, aError);
}

void AdvertisingProxy::PublishServiceHandler(const char *aName, const char *aType, otbrError aError)
{
    otbrError error = OTBR_ERROR_NONE;

    otbrLog(OTBR_LOG_INFO, "[adproxy] handle publish service '%s.%s' result: %d", aName, aType, aError);

    // TODO: there may be same names between two SRP updates.
    for (auto update = mOutstandingUpdates.begin(); update != mOutstandingUpdates.end(); ++update)
    {
        for (const auto &nameAndType : update->mServiceNames)
        {
            if (aName != nameAndType.first || !Mdns::Publisher::IsServiceTypeEqual(aType, nameAndType.second.c_str()))
            {
                continue;
            }

            if (aError != OTBR_ERROR_NONE || update->mCallbackCount == 1)
            {
                otSrpServerHandleServiceUpdateResult(GetInstance(), update->mHost, OtbrErrorToOtError(aError));
                mOutstandingUpdates.erase(update);
            }
            else
            {
                --update->mCallbackCount;
            }
            ExitNow();
        }
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_WARNING, "[adproxy] failed to handle result of service %s", aName);
    }
}

void AdvertisingProxy::PublishHostHandler(const char *aName, otbrError aError, void *aContext)
{
    static_cast<AdvertisingProxy *>(aContext)->PublishHostHandler(aName, aError);
}

void AdvertisingProxy::PublishHostHandler(const char *aName, otbrError aError)
{
    otbrError error = OTBR_ERROR_NONE;

    otbrLog(OTBR_LOG_INFO, "[adproxy] handle publish host '%s' result: %d", aName, aError);

    for (auto update = mOutstandingUpdates.begin(); update != mOutstandingUpdates.end(); ++update)
    {
        if (aName != update->mHostName)
        {
            continue;
        }

        if (aError != OTBR_ERROR_NONE || update->mCallbackCount == 1)
        {
            otSrpServerHandleServiceUpdateResult(GetInstance(), update->mHost, OtbrErrorToOtError(aError));
            mOutstandingUpdates.erase(update);
        }
        else
        {
            --update->mCallbackCount;
        }
        ExitNow();
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_WARNING, "[adproxy] failed to handle result of host %s", aName);
    }
}

Mdns::Publisher::TxtList AdvertisingProxy::MakeTxtList(const otSrpServerService *aSrpService)
{
    const uint8_t *          txtData;
    uint16_t                 txtDataLength = 0;
    otDnsTxtEntryIterator    iterator;
    otDnsTxtEntry            txtEntry;
    Mdns::Publisher::TxtList txtList;

    txtData = otSrpServerServiceGetTxtData(aSrpService, &txtDataLength);

    otDnsInitTxtEntryIterator(&iterator, txtData, txtDataLength);

    while (otDnsGetNextTxtEntry(&iterator, &txtEntry) == OT_ERROR_NONE)
    {
        txtList.emplace_back(txtEntry.mKey, txtEntry.mValue, txtEntry.mValueLength);
    }

    return txtList;
}

} // namespace otbr

#endif // OTBR_ENABLE_SRP_ADVERTISING_PROXY
