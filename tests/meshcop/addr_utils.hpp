#ifndef OTBR_ADDR_UTILS_H_
#define OTBR_ADDR_UTILS_H_

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <vector>

namespace ot {
namespace BorderRouter {

uint16_t toRloc16(uint8_t routerID, uint16_t childID);

char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen);

struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint16_t aRloc16);

struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint8_t aRouterID, uint16_t aChildID);

struct in6_addr FindRloc16Address(const std::vector<struct in6_addr> &aAddrs);

struct in6_addr FindMLEIDAddress(const std::vector<struct in6_addr> &aAddrs);

struct in6_addr GetRlocPrefix(const std::vector<struct in6_addr> &aAddrs);

struct in6_addr ToRlocPrefix(const struct in6_addr &aRlocAddr);

} // namespace BorderRouter
} // namespace ot

#endif
