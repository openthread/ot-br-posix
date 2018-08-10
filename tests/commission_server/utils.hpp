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

} // namespace utils
} // namespace BorderRouter
} // namespace ot

#endif
