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

#define OTBR_LOG_TAG "ADPROXY"

#include "sdp_proxy/advertising_proxy.hpp"

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY

#if !OTBR_ENABLE_MDNS_AVAHI && !OTBR_ENABLE_MDNS_MDNSSD && !OTBR_ENABLE_MDNS_MOJO
#error "The Advertising Proxy requires OTBR_ENABLE_MDNS_AVAHI, OTBR_ENABLE_MDNS_MDNSSD or OTBR_ENABLE_MDNS_MOJO"
#endif

#include <string>

#include <assert.h>

#include "common/code_utils.hpp"
#include "common/dns_utils.hpp"
#include "common/logging.hpp"

namespace otbr {

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

    otbrLogInfo("Started");

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

    otbrLogInfo("Stopped");
}

void AdvertisingProxy::AdvertisingHandler(otSrpServerServiceUpdateId aId,
                                          const otSrpServerHost *    aHost,
                                          uint32_t                   aTimeout,
                                          void *                     aContext)
{
    static_cast<AdvertisingProxy *>(aContext)->AdvertisingHandler(aId, aHost, aTimeout);
}

void AdvertisingProxy::AdvertisingHandler(otSrpServerServiceUpdateId aId,
                                          const otSrpServerHost *    aHost,
                                          uint32_t                   aTimeout)
{
    OTBR_UNUSED_VARIABLE(aTimeout);

    OutstandingUpdate *update = nullptr;
    otbrError          error  = OTBR_ERROR_NONE;

    mOutstandingUpdates.emplace_back();
    update      = &mOutstandingUpdates.back();
    update->mId = aId;

    error = PublishHostAndItsServices(aHost, update);

    if (error != OTBR_ERROR_NONE || update->mCallbackCount == 0)
    {
        mOutstandingUpdates.pop_back();
        otSrpServerHandleServiceUpdateResult(GetInstance(), aId, OtbrErrorToOtError(error));
    }
}

void AdvertisingProxy::PublishServiceHandler(const char *aName, const char *aType, otbrError aError, void *aContext)
{
    std::string       name(aName);
    std::string       type(aType);
    AdvertisingProxy *thisPtr = static_cast<AdvertisingProxy *>(aContext);

    thisPtr->mTaskRunner.Post(
        [name, type, aError, thisPtr]() { thisPtr->PublishServiceHandler(name.c_str(), type.c_str(), aError); });
}

void AdvertisingProxy::PublishServiceHandler(const char *aName, const char *aType, otbrError aError)
{
    otbrError error = OTBR_ERROR_NONE;

    otbrLogInfo("Handle publish service '%s.%s' result: %d", aName, aType, aError);

    // TODO: there may be same names between two SRP updates.
    for (auto update = mOutstandingUpdates.begin(); update != mOutstandingUpdates.end(); ++update)
    {
        for (auto nameAndType = update->mServiceNames.begin(); nameAndType != update->mServiceNames.end();
             ++nameAndType)
        {
            if (aName != nameAndType->first || !Mdns::Publisher::IsServiceTypeEqual(aType, nameAndType->second.c_str()))
            {
                continue;
            }

            if (aError != OTBR_ERROR_NONE || update->mCallbackCount == 1)
            {
                otSrpServerServiceUpdateId updateId = update->mId;

                // Erase before notifying OpenThread, because there are chances that new
                // elements may be added to `otSrpServerHandleServiceUpdateResult` and
                // the iterator will be invalidated.
                mOutstandingUpdates.erase(update);
                otSrpServerHandleServiceUpdateResult(GetInstance(), updateId, OtbrErrorToOtError(aError));
            }
            else
            {
                update->mServiceNames.erase(nameAndType);
                --update->mCallbackCount;
            }
            ExitNow();
        }
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("Failed to handle result of service %s", aName);
    }
}

void AdvertisingProxy::PublishHostHandler(const char *aName, otbrError aError, void *aContext)
{
    std::string       name(aName);
    AdvertisingProxy *thisPtr = static_cast<AdvertisingProxy *>(aContext);

    thisPtr->mTaskRunner.Post([name, aError, thisPtr]() { thisPtr->PublishHostHandler(name.c_str(), aError); });
}

void AdvertisingProxy::PublishHostHandler(const char *aName, otbrError aError)
{
    otbrError error = OTBR_ERROR_NONE;

    otbrLogInfo("Handle publish host '%s' result: %d", aName, aError);

    for (auto update = mOutstandingUpdates.begin(); update != mOutstandingUpdates.end(); ++update)
    {
        if (update->mHostNamePublished || aName != update->mHostName)
        {
            continue;
        }

        if (aError != OTBR_ERROR_NONE || update->mCallbackCount == 1)
        {
            otSrpServerServiceUpdateId updateId = update->mId;

            // Erase before notifying OpenThread, because there are chances that new
            // elements may be added to `otSrpServerHandleServiceUpdateResult` and
            // the iterator will be invalidated.
            mOutstandingUpdates.erase(update);
            otSrpServerHandleServiceUpdateResult(GetInstance(), updateId, OtbrErrorToOtError(aError));
        }
        else
        {
            update->mHostNamePublished = true;
            --update->mCallbackCount;
        }
        ExitNow();
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("Failed to handle result of host %s", aName);
    }
}

void AdvertisingProxy::PublishAllHostsAndServices(void)
{
    const otSrpServerHost *host = nullptr;

    VerifyOrExit(mPublisher.IsStarted(), mPublisher.Start());

    otbrLogInfo("Publish all hosts and services");
    while ((host = otSrpServerGetNextHost(GetInstance(), host)))
    {
        PublishHostAndItsServices(host, nullptr);
    }

exit:
    return;
}

otbrError AdvertisingProxy::PublishHostAndItsServices(const otSrpServerHost *aHost, OutstandingUpdate *aUpdate)
{
    otbrError                 error = OTBR_ERROR_NONE;
    const char *              fullHostName;
    std::string               hostName;
    std::string               hostDomain;
    const otIp6Address *      hostAddress;
    uint8_t                   hostAddressNum;
    bool                      hostDeleted;
    const otSrpServerService *service;

    fullHostName = otSrpServerHostGetFullName(aHost);

    otbrLogInfo("Advertise SRP service updates: host=%s", fullHostName);

    SuccessOrExit(error = SplitFullHostName(fullHostName, hostName, hostDomain));
    hostAddress = otSrpServerHostGetAddresses(aHost, &hostAddressNum);
    hostDeleted = otSrpServerHostIsDeleted(aHost);

    if (aUpdate)
    {
        aUpdate->mCallbackCount += !hostDeleted;
        aUpdate->mHostName = hostName;
        service            = nullptr;
        while ((service = otSrpServerHostFindNextService(aHost, service, OT_SRP_SERVER_FLAGS_BASE_TYPE_SERVICE_ONLY,
                                                         /* aServiceName */ nullptr, /* aInstanceName */ nullptr)))
        {
            aUpdate->mCallbackCount += !hostDeleted && !otSrpServerServiceIsDeleted(service);
        }
    }

    if (!hostDeleted)
    {
        // TODO: select a preferred address or advertise all addresses from SRP client.
        otbrLogInfo("Publish SRP host: %s", fullHostName);
        SuccessOrExit(error =
                          mPublisher.PublishHost(hostName.c_str(), hostAddress[0].mFields.m8, sizeof(hostAddress[0])));
    }
    else
    {
        otbrLogInfo("Unpublish SRP host: %s", fullHostName);
        SuccessOrExit(error = mPublisher.UnpublishHost(hostName.c_str()));
    }

    service = nullptr;
    while ((service = otSrpServerHostFindNextService(aHost, service, OT_SRP_SERVER_FLAGS_BASE_TYPE_SERVICE_ONLY,
                                                     /* aServiceName */ nullptr, /* aInstanceName */ nullptr)))
    {
        const char *fullServiceName = otSrpServerServiceGetFullName(service);
        std::string serviceName;
        std::string serviceType;
        std::string serviceDomain;

        SuccessOrExit(error = SplitFullServiceInstanceName(fullServiceName, serviceName, serviceType, serviceDomain));

        if (aUpdate)
        {
            aUpdate->mServiceNames.emplace_back(serviceName, serviceType);
        }

        if (!hostDeleted && !otSrpServerServiceIsDeleted(service))
        {
            Mdns::Publisher::TxtList     txtList     = MakeTxtList(service);
            Mdns::Publisher::SubTypeList subTypeList = MakeSubTypeList(service);

            otbrLogInfo("Publish SRP service: %s", fullServiceName);
            SuccessOrExit(error = mPublisher.PublishService(hostName.c_str(), otSrpServerServiceGetPort(service),
                                                            serviceName.c_str(), serviceType.c_str(), subTypeList,
                                                            txtList));
        }
        else
        {
            otbrLogInfo("Unpublish SRP service: %s", fullServiceName);
            SuccessOrExit(error = mPublisher.UnpublishService(serviceName.c_str(), serviceType.c_str()));
        }
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogInfo("Failed to advertise SRP service updates %p", aHost);
    }
    return error;
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

Mdns::Publisher::SubTypeList AdvertisingProxy::MakeSubTypeList(const otSrpServerService *aSrpService)
{
    const otSrpServerHost *      host         = otSrpServerServiceGetHost(aSrpService);
    const char *                 instanceName = otSrpServerServiceGetInstanceName(aSrpService);
    const otSrpServerService *   subService   = nullptr;
    Mdns::Publisher::SubTypeList subTypeList;

    while ((subService = otSrpServerHostFindNextService(
                host, subService, (OT_SRP_SERVER_SERVICE_FLAG_SUB_TYPE | OT_SRP_SERVER_SERVICE_FLAG_ACTIVE),
                /* aServiceName */ nullptr, instanceName)) != nullptr)
    {
        char subLabel[OT_DNS_MAX_LABEL_SIZE];

        if (otSrpServerServiceGetServiceSubTypeLabel(subService, subLabel, sizeof(subLabel)) == OT_ERROR_NONE)
        {
            subTypeList.emplace_back(subLabel);
        }
        else
        {
            otbrLogWarning("Failed to retrieve subtype of SRP service: %s", otSrpServerServiceGetFullName(aSrpService));
        }
    }

    return subTypeList;
}

} // namespace otbr

#endif // OTBR_ENABLE_SRP_ADVERTISING_PROXY
