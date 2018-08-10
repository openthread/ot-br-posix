#include "border_agent_session.hpp"
#include "common/logging.hpp"
#include "utils/hex.hpp"
#include <cstdio>

using namespace ot::BorderRouter;

int main()
{
    const char networkName[] = "OpenThreadDemo";
    const char passPhrase[]  = "123456";
    const char joinerPassPhrase[]  = "ABCDEF";
    const char xpanidAscii[] = "1111111122222222";
    uint8_t    xpanidBin[kXpanidLength];

    otbrLogInit("Commission server", OTBR_LOG_ERR);

    ot::Utils::Hex2Bytes(xpanidAscii, xpanidBin, sizeof(xpanidBin));
    BorderAgentDtlsSession s(xpanidBin, networkName, passPhrase, joinerPassPhrase);
    sockaddr_in            addr;
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(49191);
    inet_pton(AF_INET, "172.30.55.241", &addr.sin_addr);
    s.Connect(addr);
    s.SetupProxyServer();
    while (true)
    {
        int            maxFd   = -1;
        struct timeval timeout = {10, 0};
        int            rval;
        fd_set         readFdSet;
        fd_set         writeFdSet;
        fd_set         errorFdSet;
        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_ZERO(&errorFdSet);
        s.UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
        rval = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);
        if ((rval < 0) && (errno != EINTR))
        {
            rval = OTBR_ERROR_ERRNO;
            otbrLog(OTBR_LOG_ERR, "select() failed", strerror(errno));
            break;
        }
        s.Process(readFdSet, writeFdSet, errorFdSet);
    }
    s.Disconnect();
    s.ShutDownPorxyServer();
    return 0;
}
