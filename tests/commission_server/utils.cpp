#include "utils.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace ot {
namespace BorderRouter {
namespace utils {

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

} // namespace utils
} // namespace BorderRouter
} // namespace ot
