#include "ot_client.hpp"

#include <openthread/platform/toolchain.h>

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "platform-posix.h"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "utils/strcpy_utils.hpp"

namespace ot {

Client::Client(void)
    : mTimeout(kDefaultTimeout)
    , mSocket(-1)
{
}

Client::~Client(void)
{
    if (mSocket != -1)
    {
        close(mSocket);
    }
}
bool Client::Connect(void)
{
    struct sockaddr_un sockname;
    int                ret;

    mSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    VerifyOrExit(mSocket != -1, perror("socket"); ret = OT_EXIT_FAILURE);

    memset(&sockname, 0, sizeof(struct sockaddr_un));
    sockname.sun_family = AF_UNIX;
    strcpy_safe(sockname.sun_path, sizeof(sockname.sun_path), OPENTHREAD_POSIX_APP_SOCKET_NAME);

    ret = connect(mSocket, reinterpret_cast<const struct sockaddr *>(&sockname), sizeof(struct sockaddr_un));

    if (ret == -1)
    {
        otbrLog(OTBR_LOG_ERR, "OpenThread daemon is not running.");
    }

exit:
    return ret == 0;
}

char *Client::Execute(const char *aFormat, ...)
{
    va_list args;
    int     ret;
    char *  rval = NULL;
    ssize_t count;
    size_t  rxLength = 0;

    va_start(args, aFormat);
    ret = vsnprintf(&mBuffer[1], sizeof(mBuffer) - 1, aFormat, args);
    va_end(args);

    if (ret < 0)
    {
        otbrLog(OTBR_LOG_ERR, "Failed to generate command: %s", strerror(errno));
    }

    mBuffer[0] = '\n';
    ret++;

    if (ret == sizeof(mBuffer))
    {
        otbrLog(OTBR_LOG_ERR, "Command exceeds maximum limit: %d", kBufferSize);
    }

    mBuffer[ret] = '\n';
    ret++;

    count = write(mSocket, mBuffer, ret);

    if (count < ret)
    {
        mBuffer[ret] = '\0';
        otbrLog(OTBR_LOG_ERR, "Failed to send command: %s", mBuffer);
    }

    for (int i = 0; i < mTimeout; ++i)
    {
        fd_set  readFdSet;
        timeval timeout = {0, 1000};
        char *  done;

        FD_ZERO(&readFdSet);
        FD_SET(mSocket, &readFdSet);

        ret = select(mSocket + 1, &readFdSet, NULL, NULL, &timeout);
        VerifyOrExit(ret != -1 || errno == EINTR);
        if (ret <= 0)
        {
            continue;
        }

        count = read(mSocket, &mBuffer[rxLength], sizeof(mBuffer) - rxLength);
        VerifyOrExit(count > 0);
        rxLength += count;

        mBuffer[rxLength] = '\0';
        done              = strstr(mBuffer, "Done\r\n");

        if (done != NULL)
        {
            // remove trailing \r\n
            if (done - rval > 2)
            {
                done[-2] = '\0';
            }

            rval = mBuffer;
            break;
        }
    }

exit:
    return rval;
}

int Client::Scan(Dbus::WpanNetworkInfo *aNetworks, int aLength)
{
    char *result;
    int   rval = 0;

    mTimeout = 5000;
    result   = Execute("scan");
    VerifyOrExit(result != NULL);

    for (result = strtok(result, "\r\n"); result != NULL && rval < aLength; result = strtok(NULL, "\r\n"))
    {
        static const char kCliPrompt[] = "> ";
        char *            cliPrompt;
        int               matched;
        int               joinable;
        int               lqi;

        // remove prompt
        if ((cliPrompt = strstr(result, kCliPrompt)) != NULL)
        {
            if (cliPrompt == result)
            {
                result += sizeof(kCliPrompt) - 1;
            }
            else
            {
                memmove(cliPrompt, cliPrompt + sizeof(kCliPrompt) - 1, strlen(cliPrompt) - sizeof(kCliPrompt) - 1);
            }
        }

        matched = sscanf(result,
                         "| %d | %s | %" PRIx64
                         " | %hx | %02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx | %hu | %hhd | %d |",
                         &joinable, aNetworks[rval].mNetworkName, &aNetworks[rval].mExtPanId, &aNetworks[rval].mPanId,
                         &aNetworks[rval].mHardwareAddress[0], &aNetworks[rval].mHardwareAddress[1],
                         &aNetworks[rval].mHardwareAddress[2], &aNetworks[rval].mHardwareAddress[3],
                         &aNetworks[rval].mHardwareAddress[4], &aNetworks[rval].mHardwareAddress[5],
                         &aNetworks[rval].mHardwareAddress[6], &aNetworks[rval].mHardwareAddress[7],
                         &aNetworks[rval].mChannel, &aNetworks[rval].mRssi, &lqi);

        // 15 is the number of output arguments of the last sscanf()
        if (matched != 15)
        {
            continue;
        }

        aNetworks[rval].mAllowingJoin = joinable != 0;
        ++rval;
    }

    mTimeout = kDefaultTimeout;

exit:
    return rval;
}

bool Client::FactoryReset(void)
{
    const char *result;
    bool        rval = false;

    Execute("factoryreset");
    sleep(0);
    VerifyOrExit(Connect());
    result = Execute("version");
    VerifyOrExit(result != NULL);

    rval = strstr(result, "OPENTHREAD") != NULL;

exit:
    return rval;
}

} // namespace ot
