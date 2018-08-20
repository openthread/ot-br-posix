/*
 *    Copyright (c) 2018, The OpenThread Authors.
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
 *   The file is the header of tmf client and net topology data structure
 */

#ifndef OTBR_TMF_CLIENT_HPP_
#define OTBR_TMF_CLIENT_HPP_

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>

#include "commissioner_constants.hpp"
#include "commissioner_proxy.hpp"
#include "net_topology_info.hpp"
#include "agent/coap.hpp"

namespace ot {
namespace BorderRouter {

enum
{
    kTypeListTlvType = 0x12,
    kLeaderDataType  = 0x06,
    kAddressListType = 0x08,
    kChildTableType  = 0x10,
    kRoute64Type     = 0x05,
};

class TmfClient
{
public:
    /**
     * The constructor to initialize Tmf client
     *
     * @param[in]    aProxy         proxy to send and receive data through
     *
     */
    TmfClient(CommissionerProxy *aProxy);

    /**
     * This method queries node information(mle address and rloc16) from thread node
     *
     * @param[in]    aAddr          thread node address
     *
     * @returns thread node information
     *
     */
    NodeInfo QueryNodeInfo(const in6_addr &aAddr);

    /**
     * This method queries all ipv6 addresses of a thread node
     *
     * @param[in]    aAddr          thread node address
     *
     * @returns all ipv6 addresses of node
     *
     */
    std::vector<struct in6_addr> QueryAllV6Addresses(const in6_addr &aAddr);

    /**
     * This method queries thread network leader data
     *
     * @param[in]    aAddr          thread node address
     *
     * @returns leader data of the network this node belongs to
     *
     */
    LeaderData QueryLeaderData(const in6_addr &aAddr);

    /**
     * This method queries all childs of a thread router
     *
     * @param[in]    aAddr          router node address
     *
     * @returns vector of all child information of this router
     *
     */
    std::vector<ChildTableEntry> QueryChildTable(const in6_addr &aAddr);

    /**
     * This method queries routing information from thread router node
     *
     * @param[in]    aAddr          router node address
     *
     * @returns vector of links established from this router
     *
     */
    std::vector<LinkInfo> QueryRouteInfo(const in6_addr &aAddr);

    /**
     * This method conducts a search over the thread network to extract topology
     *
     * @param[in]    aAddr          any node address in thread network
     *
     * @returns topology information of the network this node belongs to
     *
     */
    NetworkInfo TraverseNetwork(const in6_addr &aAddr);

    ~TmfClient();

private:
    TmfClient(const TmfClient &);
    TmfClient &operator=(const TmfClient &);

    size_t PostCoapAndWaitForResponse(sockaddr_in6 aDestAddr, const char *aUri, uint8_t *aPayload, size_t aLength);

    void QueryDiagnosticData(const struct in6_addr &aDestAddr, uint8_t aQueryType);

    NodeInfo              GetNodeInfo(const struct in6_addr &aAddr);
    std::vector<NodeInfo> GetChildNodes(const struct in6_addr &rlocPrefix, uint8_t routerID);

    static ssize_t SendCoap(const uint8_t *aBuffer,
                            uint16_t       aLength,
                            const uint8_t *aIp6,
                            uint16_t       aPort,
                            void *         aContext);
    static void    HandleCoapResponse(const Coap::Message &aMessage, void *aContext);

    Coap::Agent *mCoapAgent;
    int          mCoapToken;

    uint8_t mResponseBuffer[kSizeMaxPacket];
    size_t  mResponseSize;
    bool    mResponseHandled;

    CommissionerProxy * mProxy;
    struct sockaddr_in6 mDestAddr;

    static const uint16_t kTmfPort;
    static const char     kDiagUri[];
};

} // namespace BorderRouter
} // namespace ot

#endif // OTBR_TMF_CLIENT_HPP_
