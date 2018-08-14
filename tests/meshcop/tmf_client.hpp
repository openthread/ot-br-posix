#ifndef OTBR_TMF_CLIENT_H_
#define OTBR_TMF_CLIENT_H_

#include "commission_common.hpp"
#include "commissioner_proxy.hpp"
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <vector>
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

struct LinkInfo
{
    uint16_t fromRloc16; // in host byte order
    uint16_t toRloc16;   // in host byte order
    uint8_t  routeCost;
    uint8_t  outQualityLevel;
    uint8_t  inQualityLevel;
};

#pragma pack(push, 0)
struct LeaderData
{
    uint32_t partitionID;
    uint8_t  weighting;
    uint8_t  version;
    uint8_t  stateVersion;
    uint8_t  routerID;
};

struct ChildTableEntry
{
    uint8_t  timeOut;
    uint16_t childID;
    uint8_t  mode;
};
#pragma pack(pop)

struct NodeInfo
{
    struct in6_addr mleAddr;
    uint16_t        rloc16; // in host byte order
};

struct NetworkInfo
{
    NodeInfo              leaderNode;
    std::vector<NodeInfo> nodes;
    std::vector<LinkInfo> links;
};

class TmfClient
{
public:
    TmfClient(CommissionerProxy *proxy);

    NodeInfo QueryNodeInfo(const in6_addr &aAddr);

    std::vector<struct in6_addr> QueryAllV6Addresses(const in6_addr &aAddr);

    LeaderData QueryLeaderData(const in6_addr &aAddr);

    std::vector<ChildTableEntry> QueryChildTable(const in6_addr &aAddr);

    std::vector<LinkInfo> QueryRouteInfo(const in6_addr &aAddr);

    NetworkInfo TraverseNetwork(const in6_addr &aAddr);

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

    uint8_t mCoapBuffer[kSizeMaxPacket];
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

#endif
