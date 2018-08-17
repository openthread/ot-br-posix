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
 *   The file implements tmf client
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "addr_utils.hpp"
#include "commissioner_utils.hpp"
#include "tmf_client.hpp"
#include "udp_encapsulation_tlv.hpp"
#include "bit_extraction.hpp"

namespace ot {
namespace BorderRouter {

enum {
    kChildTimeoutBeginBit = 0,
    kChildTimeoutEndBit = 5,
    kChildIdBeginBit = 7,
    kChildIdEndBit = 16,
    kSelfRouteData = 0x01,
    kOutQualityBeginBit = 0,
    kInQaulityBeginBit = 2,
    kRouteCostBeginBit = 4,
    kRouteCostEndBit = 8,
};

const uint16_t TmfClient::kTmfPort   = 61631;
const char     TmfClient::kDiagUri[] = "d/dg";

TmfClient::TmfClient(CommissionerProxy *proxy)
    : mCoapAgent(Coap::Agent::Create(&TmfClient::SendCoap, this))
    , mCoapToken(rand())
    , mProxy(proxy)
{
}

ssize_t TmfClient::SendCoap(const uint8_t *aBuffer,
                            uint16_t       aLength,
                            const uint8_t *aIp6,
                            uint16_t       aPort,
                            void *         aContext)
{
    TmfClient *client = static_cast<TmfClient *>(aContext);

    return client->mProxy->SendTo(client->mDestAddr, aBuffer, aLength);

    (void)aIp6;
    (void)aPort;
}

void TmfClient::HandleCoapResponse(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    const uint8_t *payload    = aMessage.GetPayload(length);
    TmfClient *    client     = static_cast<TmfClient *>(aContext);
    size_t         copyLength = utils::Min<size_t>(sizeof(client->mResponseBuffer), length);

    memcpy(client->mResponseBuffer, payload, copyLength);
    client->mResponseSize    = copyLength;
    client->mResponseHandled = true;
}

size_t TmfClient::PostCoapAndWaitForResponse(sockaddr_in6 dest, const char *uri, uint8_t *payload, size_t length)
{
    int      ret;
    uint8_t  buffer[kSizeMaxPacket];
    sockaddr_in6 srcAddr;
    uint16_t token         = ++mCoapToken;
    token                  = htons(token);
    Coap::Message *message = mCoapAgent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                                    reinterpret_cast<const uint8_t *>(&token), sizeof(token));

    message->SetPath(uri);
    message->SetPayload(payload, length);
    mDestAddr = dest;

    mCoapAgent->Send(*message, NULL, 0, HandleCoapResponse, this);
    mCoapAgent->FreeMessage(message);

    mResponseHandled = false;
    do
    {
        ret = mProxy->RecvFrom(buffer, sizeof(buffer), srcAddr);
        if (ret > 0)
        {
            mCoapAgent->Input(buffer, static_cast<uint16_t>(ret), NULL, 0);
            if (mResponseHandled)
            {
                mResponseHandled = false;
                break;
            }
        }
    } while (ret >= 0);

    return mResponseSize;
}

void TmfClient::QueryDiagnosticData(const struct in6_addr &destAddr, uint8_t queryType)
{
    uint8_t requestBuffer[kSizeMaxPacket];
    sockaddr_in6 dest;
    Tlv *   tlv = reinterpret_cast<Tlv *>(requestBuffer);

    dest.sin6_addr = destAddr;
    dest.sin6_port = htons(kTmfPort);
    tlv->SetType(kTypeListTlvType);
    tlv->SetValue(queryType);
    tlv = tlv->GetNext();
    PostCoapAndWaitForResponse(dest, kDiagUri, requestBuffer, utils::LengthOf(requestBuffer, tlv));
}

static std::vector<struct in6_addr> ParseAddressesTLV(const uint8_t *aBuffer)
{
    std::vector<struct in6_addr> addresses;
    const ot::Tlv *              tlv = reinterpret_cast<const Tlv *>(aBuffer);
    uint16_t payloadLength = tlv->GetLength();
    size_t                 addrCount = payloadLength / sizeof(struct in6_addr);

    assert(tlv->GetType() == kAddressListType);
    assert(payloadLength % sizeof(struct in6_addr) == 0);
    const struct in6_addr *payload   = reinterpret_cast<const struct in6_addr *>(tlv->GetValue());
    for (size_t i = 0; i < addrCount; i++)
    {
        addresses.push_back(payload[i]);
    }
    return addresses;
}

std::vector<struct in6_addr> TmfClient::QueryAllV6Addresses(const in6_addr &aAddr)
{
    QueryDiagnosticData(aAddr, kAddressListType);
    return ParseAddressesTLV(mResponseBuffer);
}

static LeaderData ParseLeaderDataTlv(const uint8_t *aBuffer)
{
    std::vector<struct in6_addr> addresses;
    const ot::Tlv *              tlv = reinterpret_cast<const Tlv *>(aBuffer);
    assert(tlv->GetType() == kLeaderDataType);
    uint16_t       payloadLength = tlv->GetLength();
    const uint8_t *payload       = reinterpret_cast<const uint8_t *>(tlv->GetValue());
    LeaderData     data;

    assert(sizeof(data) == payloadLength);
    memcpy(&data, payload, sizeof(data));
    return data;
}

LeaderData TmfClient::QueryLeaderData(const in6_addr &aAddr)
{
    QueryDiagnosticData(aAddr, kLeaderDataType);
    return ParseLeaderDataTlv(mResponseBuffer);
}

static std::vector<ChildTableEntry> ParseChildTableTlv(const uint8_t *aBuffer)
{
    static const size_t          kChildTableEntrySize = 3;
    std::vector<struct in6_addr> addresses;
    const ot::Tlv *              tlv = reinterpret_cast<const Tlv *>(aBuffer);
    assert(tlv->GetType() == kChildTableType);
    uint16_t payloadLength = tlv->GetLength();
    size_t                       childCount = payloadLength / kChildTableEntrySize;
    const uint8_t *              payload    = reinterpret_cast<const uint8_t *>(tlv->GetValue());
    std::vector<ChildTableEntry> childTable;

    childTable.reserve(childCount);
    for (size_t i = 0; i < childCount; i++)
    {
        ChildTableEntry entry;
        const uint8_t * nowEntry = payload + i * kChildTableEntrySize;

        entry.timeOut            = ExtractBits<kChildTimeoutBeginBit, kChildTimeoutEndBit>(nowEntry);
        entry.childID            = ExtractBits<kChildIdBeginBit, kChildIdEndBit>(nowEntry);
        entry.mode               = nowEntry[2];
        childTable.push_back(entry);
    }
    return childTable;
}

std::vector<ChildTableEntry> TmfClient::QueryChildTable(const in6_addr &aAddr)
{
    QueryDiagnosticData(aAddr, kChildTableType);
    return ParseChildTableTlv(mResponseBuffer);
}

static std::vector<LinkInfo> ParseRoute64Tlv(const uint8_t *aBuffer)
{
    const ot::Tlv *       tlv           = reinterpret_cast<const Tlv *>(aBuffer);
    uint16_t              payloadLength = tlv->GetLength();
    const uint8_t *       buf           = reinterpret_cast<const uint8_t *>(tlv->GetValue()) + 1;
    std::vector<int>      connectedRouterIds;
    size_t                remainBufSize = payloadLength - 1;
    std::vector<LinkInfo> linkInfos;
    const uint8_t *       routeDataBuf;
    uint8_t               selfRouteId;
    size_t                i;
    uint8_t               nowRouterId = 0;

    assert(tlv->GetType() == kRoute64Type);
    // Route64 Tlv format is a bit map for router ids alive,
    // followed by route data one byte for each router
    for (i = 0; remainBufSize > 0; i++, remainBufSize--)
    {
        uint8_t mask = buf[i];

        for (int j = 0; j < 8; j++, nowRouterId++)
        {
            if (mask & 0x80)
            {
                connectedRouterIds.push_back(nowRouterId);
                remainBufSize--;
            }
            mask <<= 1;
        }
    }

    routeDataBuf = buf + i;
    selfRouteId  = 0;
    for (size_t j = 0; j < connectedRouterIds.size(); j++)
    {
        uint8_t routeData = routeDataBuf[j];

        if (routeData == kSelfRouteData)
        {
            selfRouteId = connectedRouterIds[j];
        }
        else
        {
            LinkInfo info;
            uint8_t  routeCost = ExtractBits<kRouteCostBeginBit, kRouteCostEndBit>(&routeData);

            if (routeCost != 0)
            {
                info.toRloc16        = ToRloc16(connectedRouterIds[j], 0);
                info.outQualityLevel = ExtractBits<kOutQualityBeginBit, kInQaulityBeginBit>(&routeData);
                info.inQualityLevel  = ExtractBits<kInQaulityBeginBit, kRouteCostBeginBit>(&routeData);
                linkInfos.push_back(info);
            }
        }
    }
    for (i = 0; i < linkInfos.size(); i++)
    {
        linkInfos[i].fromRloc16 = ToRloc16(selfRouteId, 0);
    }
    return linkInfos;
}

std::vector<LinkInfo> TmfClient::QueryRouteInfo(const in6_addr &aAddr)
{
    QueryDiagnosticData(aAddr, kRoute64Type);
    return ParseRoute64Tlv(mResponseBuffer);
}

NodeInfo TmfClient::GetNodeInfo(const struct in6_addr &aAddr)
{
    NodeInfo                     nodeInfo;
    std::vector<struct in6_addr> addresses;
    struct in6_addr              mleAddr;
    struct in6_addr              rlocAddr;

    addresses        = QueryAllV6Addresses(aAddr);
    mleAddr          = FindMLEIDAddress(addresses);
    rlocAddr         = FindRloc16Address(addresses);
    nodeInfo.rloc16  = ntohs(reinterpret_cast<uint16_t *>(&rlocAddr)[7]);
    nodeInfo.mleAddr = mleAddr;
    return nodeInfo;
}

std::vector<NodeInfo> TmfClient::GetChildNodes(const struct in6_addr &rlocPrefix, uint8_t routerID)
{
    struct in6_addr              aRlocAddr  = ConcatRloc16Address(rlocPrefix, routerID, 0);
    std::vector<ChildTableEntry> childTable = QueryChildTable(aRlocAddr);
    std::vector<NodeInfo>        nodeInfos;

    for (size_t i = 0; i < childTable.size(); i++)
    {
        NodeInfo nodeInfo = GetNodeInfo(ConcatRloc16Address(rlocPrefix, routerID, childTable[i].childID));

        nodeInfos.push_back(nodeInfo);
    }
    return nodeInfos;
}

NetworkInfo TmfClient::TraverseNetwork(const in6_addr &aAddr)
{
    std::set<uint8_t>     visitedIDs;
    std::vector<uint8_t>  frontierRouterIDs;
    NetworkInfo           networkInfo;
    LeaderData            leaderData       = QueryLeaderData(aAddr);
    std::vector<in6_addr> addresses        = QueryAllV6Addresses(aAddr);
    struct in6_addr       rlocPrefix       = GetRlocPrefix(addresses);
    struct in6_addr       leaderRloc16Addr = ConcatRloc16Address(rlocPrefix, leaderData.routerID, 0);

    networkInfo.leaderNode                 = GetNodeInfo(leaderRloc16Addr);
    frontierRouterIDs.push_back(leaderData.routerID);
    visitedIDs.insert(leaderData.routerID);
    while (!frontierRouterIDs.empty())
    {
        std::vector<uint8_t> newFrontierIDs;

        for (size_t i = 0; i < frontierRouterIDs.size(); i++)
        {
            std::vector<LinkInfo> routerLinks;
            std::vector<NodeInfo> childNodes;
            uint8_t               routerID       = frontierRouterIDs[i];
            struct in6_addr       routerRlocAddr = ConcatRloc16Address(rlocPrefix, routerID, 0);

            NodeInfo              routerNode     = GetNodeInfo(routerRlocAddr);
            networkInfo.nodes.push_back(routerNode);

            childNodes = GetChildNodes(rlocPrefix, routerID);
            for (size_t j = 0; j < childNodes.size(); j++)
            {
                LinkInfo        childLink;
                const NodeInfo &childNode = childNodes[j];

                networkInfo.nodes.push_back(childNode);
                childLink.fromRloc16      = routerNode.rloc16;
                childLink.toRloc16        = childNode.rloc16;
                childLink.routeCost       = 0;
                childLink.inQualityLevel  = 0;
                childLink.outQualityLevel = 0;
                networkInfo.links.push_back(childLink);
            }

            routerLinks = QueryRouteInfo(routerRlocAddr);
            for (size_t j = 0; j < routerLinks.size(); j++)
            {
                uint8_t neighbourRouterID = (routerLinks[j].toRloc16 >> 10);

                if (neighbourRouterID < routerID)
                {
                    networkInfo.links.push_back(routerLinks[j]);
                }
                if (visitedIDs.find(neighbourRouterID) == visitedIDs.end())
                {
                    newFrontierIDs.push_back(neighbourRouterID);
                    visitedIDs.insert(neighbourRouterID);
                }
            }
        }
        frontierRouterIDs = newFrontierIDs;
    }
    return networkInfo;
}

TmfClient::~TmfClient()
{
    Coap::Agent::Destroy(mCoapAgent);
}

} // namespace BorderRouter
} // namespace ot
