#ifndef OTBR_UTILS_HPP_
#define OTBR_UTILS_HPP_

#include <cstdio> 
#include <stdint.h> 
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>

namespace ot {
namespace BorderRouter {
namespace utils {

inline uint16_t LengthOf(const void *aStart, const void *aEnd)
{
    return static_cast<const uint8_t *>(aEnd) - static_cast<const uint8_t *>(aStart);
}

template <typename T> T min(const T &lhs, const T &rhs)
{
    return lhs < rhs ? lhs : rhs;
}

template <typename T> T max(const T &lhs, const T &rhs)
{
    return lhs > rhs ? lhs : rhs;
}

inline uint16_t toRloc16(uint8_t routerID, uint16_t childID)
{
    uint16_t rloc16 = routerID;
    return (rloc16 << 10) | childID;
}

char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen);

} // namespace utils
} // namespace BorderRouter
} // namespace ot

#endif
