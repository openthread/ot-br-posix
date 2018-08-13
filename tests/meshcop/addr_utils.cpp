#include "addr_utils.hpp"
#include <string.h>

namespace ot {
namespace BorderRouter {

static const uint8_t kLociid[] = {
    0x00, 0x00, 0x00, 0xff, 0xfe, 0x00,
};

uint16_t toRloc16(uint8_t routerID, uint16_t childID)
{
    uint16_t rloc16 = routerID;
    return (rloc16 << 10) | childID;
}

#define IPSTR_BUFSIZE (((INET6_ADDRSTRLEN > INET_ADDRSTRLEN) ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN) + 1)
char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch (sa->sa_family)
    {
    case AF_INET:
        inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen);
        break;

    case AF_INET6:
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s, maxlen);
        break;

    default:
        strncpy(s, "Unknown AF", maxlen);
        return NULL;
    }

    return s;
}

struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint16_t aRloc16)
{
    in6_addr addr                          = aPrefix;
    reinterpret_cast<uint16_t *>(&addr)[7] = htons(aRloc16);
    return addr;
}

struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint8_t aRouterID, uint16_t aChildID)
{
    return ConcatRloc16Address(aPrefix, toRloc16(aRouterID, aChildID));
}

struct in6_addr FindRloc16Address(const std::vector<struct in6_addr> &aAddrs)
{
    struct in6_addr foundAddr;
    memset(&foundAddr, 0, sizeof(in6_addr));
    for (size_t i = 0; i < aAddrs.size(); i++)
    {
        const uint8_t *buf = reinterpret_cast<const uint8_t *>(&aAddrs[i]);
        if (!memcmp(&buf[8], &kLociid[0], sizeof(kLociid)) && buf[14] != 0xfc)
        {
            foundAddr = aAddrs[i];
        }
    }
    return foundAddr;
}

struct in6_addr FindMLEIDAddress(const std::vector<struct in6_addr> &aAddrs)
{
    struct in6_addr foundAddr;
    memset(&foundAddr, 0, sizeof(in6_addr));
    for (size_t i = 0; i < aAddrs.size(); i++)
    {
        const uint8_t *buf = reinterpret_cast<const uint8_t *>(&aAddrs[i]);
        if (buf[0] == 0xfd && memcmp(&buf[8], &kLociid[0], sizeof(kLociid)))
        {
            foundAddr = aAddrs[i];
        }
    }
    return foundAddr;
}

struct in6_addr GetRlocPrefix(const std::vector<struct in6_addr> &aAddrs)
{
    struct in6_addr rlocAddr = FindRloc16Address(aAddrs);
    return ToRlocPrefix(rlocAddr);
}

struct in6_addr ToRlocPrefix(const struct in6_addr &aRlocAddr)
{
    struct in6_addr prefix                   = aRlocAddr;
    reinterpret_cast<uint16_t *>(&prefix)[7] = 0;
    return prefix;
}

} // namespace BorderRouter
} // namespace ot
