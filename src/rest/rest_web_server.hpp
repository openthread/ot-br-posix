/*
 *  Copyright (c) 2020, The OpenThread Authors.
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
 *   This file includes definitions for RESTful HTTP server.
 */

#ifndef OTBR_REST_REST_WEB_SERVER_HPP_
#define OTBR_REST_REST_WEB_SERVER_HPP_

#include "common/time.hpp"
#include "openthread-br/config.h"

#include <httplib.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>

#include <unordered_map>
#include <vector>

#include <openthread/border_agent.h>
#include <openthread/border_router.h>
#include <openthread/dataset.h>
#include <openthread/dataset_ftd.h>
#include <openthread/netdiag.h>

#include "host/rcp_host.hpp"
#include "host/thread_helper.hpp"
#include "rest/types.hpp"

namespace otbr {
namespace rest {

/**
 * This class implements a REST server.
 */
class RestWebServer
{
public:
    /**
     * The constructor to initialize a REST server.
     *
     * @param[in] aHost  A reference to the Thread controller.
     */
    RestWebServer(Host::RcpHost &aHost);

    /**
     * The destructor destroys the server instance.
     */
    ~RestWebServer(void);

    /**
     * This method initializes the REST server.
     */
    void Init(const std::string &aRestListenAddress, int aRestListenPort);

private:
    using Request    = httplib::Request;
    using Response   = httplib::Response;
    using StatusCode = httplib::StatusCode;

    /**
     * This enumeration represents the Dataset type (active or pending).
     */
    enum class DatasetType : uint8_t
    {
        kActive,  ///< Active Dataset
        kPending, ///< Pending Dataset
    };

    void NodeInfo(const Request &aRequest, Response &aResponse) const;
    void BaId(const Request &aRequest, Response &aResponse) const;
    void ExtendedAddr(const Request &aRequest, Response &aResponse) const;
    void State(const Request &aRequest, Response &aResponse) const;
    void NetworkName(const Request &aRequest, Response &aResponse) const;
    void LeaderData(const Request &aRequest, Response &aResponse) const;
    void NumOfRoute(const Request &aRequest, Response &aResponse) const;
    void Rloc16(const Request &aRequest, Response &aResponse) const;
    void ExtendedPanId(const Request &aRequest, Response &aResponse) const;
    void Rloc(const Request &aRequest, Response &aResponse) const;
    void Dataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const;
    void DatasetActive(const Request &aRequest, Response &aResponse) const;
    void DatasetPending(const Request &aRequest, Response &aResponse) const;
    void CommissionerState(const Request &aRequest, Response &aResponse) const;
    void CommissionerJoiner(const Request &aRequest, Response &aResponse) const;
    void Diagnostic(const Request &aRequest, Response &aResponse);
    void CoprocessorVersion(const Request &aRequest, Response &aResponse) const;
    void GetNodeInfo(Response &aResponse) const;
    void DeleteNodeInfo(Response &aResponse) const;
    void GetDataBaId(Response &aResponse) const;
    void GetDataExtendedAddr(Response &aResponse) const;
    void GetDataState(Response &aResponse) const;
    void SetDataState(const Request &aRequest, Response &aResponse) const;
    void GetDataNetworkName(Response &aResponse) const;
    void GetDataLeaderData(Response &aResponse) const;
    void GetDataNumOfRoute(Response &aResponse) const;
    void GetDataRloc16(Response &aResponse) const;
    void GetDataExtendedPanId(Response &aResponse) const;
    void GetDataRloc(Response &aResponse) const;
    void GetDataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const;
    void SetDataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const;
    void GetCommissionerState(Response &aResponse) const;
    void SetCommissionerState(const Request &aRequest, Response &aResponse) const;
    void GetJoiners(Response &aResponse) const;
    void AddJoiner(const Request &aRequest, Response &aResponse) const;
    void RemoveJoiner(const Request &aRequest, Response &aResponse) const;
    void GetCoprocessorVersion(Response &aResponse) const;

    void DeleteOutDatedDiagnostic(void);
    void UpdateDiag(std::string aKey, std::vector<otNetworkDiagTlv> &aDiag);

    void DiagnosticResponseHandler(otError aError, const otMessage *aMessage, const otMessageInfo *aMessageInfo);

    otInstance *GetInstance(void) const { return mHost.GetThreadHelper()->GetInstance(); }

    void ErrorHandler(Response &aResponse, StatusCode aErrorCode) const;

    template <typename Call, typename... Args>
    auto RunInMainLoop(Call aCall, Args... aArgs) const -> decltype(aCall(aArgs...))
    {
        return mHost.GetTaskRunner().PostAndWait<decltype(aCall(aArgs...))>([&]() { return aCall(aArgs...); });
    }

    template <typename Call, typename... Args>
    auto RunInMainLoop(Milliseconds aDelay, Call aCall, Args... aArgs) const -> decltype(aCall(aArgs...))
    {
        return mHost.GetTaskRunner().PostAndWait<decltype(aCall(aArgs...))>([&]() { return aCall(aArgs...); }, aDelay);
    }

    template <typename HandlerType> httplib::Server::Handler MakeHandler(HandlerType aHandler)
    {
        return [this, aHandler](const Request &aRequest, Response &aResponse) -> void {
            (this->*aHandler)(aRequest, aResponse);
        };
    }

    Host::RcpHost &mHost;

    httplib::Server mServer;
    std::thread     mServerThread;

    std::unordered_map<std::string, DiagInfo> mDiagSet;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_REST_WEB_SERVER_HPP_
