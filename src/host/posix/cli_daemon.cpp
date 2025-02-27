/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#define OTBR_LOG_TAG "CLI_DAEMON"

#include "cli_daemon.hpp"

#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <openthread/cli.h>

#include "utils/socket_utils.hpp"

namespace otbr {

otbrError CliDaemon::Dependencies::InputCommandLine(const uint8_t *aBuf, uint16_t aLength)
{
    OTBR_UNUSED_VARIABLE(aBuf);
    OTBR_UNUSED_VARIABLE(aLength);

    return OTBR_ERROR_NONE;
}

static constexpr char kDefaultNetIfName[] = "wpan0";
static constexpr char kSocketBaseName[]   = "/run/openthread-";
static constexpr char kSocketSuffix[]     = ".sock";
static constexpr char kSocketLockSuffix[] = ".lock";

static constexpr size_t kMaxSocketFilenameLength = sizeof(sockaddr_un::sun_path) - 1;

std::string CliDaemon::GetSocketFilename(const char *aSuffix) const
{
    std::string fileName;
    std::string netIfName = mNetifName.empty() ? kDefaultNetIfName : mNetifName;

    fileName = kSocketBaseName + netIfName + aSuffix;
    VerifyOrDie(fileName.size() <= kMaxSocketFilenameLength, otbrErrorString(OTBR_ERROR_INVALID_ARGS));

    return fileName;
}

CliDaemon::CliDaemon(Dependencies &aDependencies)
    : mListenSocket(-1)
    , mDaemonLock(-1)
    , mSessionSocket(-1)
    , mDeps(aDependencies)
{
}

void CliDaemon::CreateListenSocketOrDie(void)
{
    struct sockaddr_un sockname;

    mListenSocket = SocketWithCloseExec(AF_UNIX, SOCK_STREAM, 0, kSocketNonBlock);
    VerifyOrDie(mListenSocket != -1, strerror(errno));

    std::string lockfile = GetSocketFilename(kSocketLockSuffix);
    mDaemonLock          = open(lockfile.c_str(), O_CREAT | O_RDONLY | O_CLOEXEC, 0600);
    VerifyOrDie(mDaemonLock != -1, strerror(errno));

    VerifyOrDie(flock(mDaemonLock, LOCK_EX | LOCK_NB) != -1, strerror(errno));

    std::string socketfile = GetSocketFilename(kSocketSuffix);
    memset(&sockname, 0, sizeof(struct sockaddr_un));

    sockname.sun_family = AF_UNIX;
    strncpy(sockname.sun_path, socketfile.c_str(), sizeof(sockname.sun_path) - 1);
    OTBR_UNUSED_VARIABLE(unlink(sockname.sun_path));

    VerifyOrDie(bind(mListenSocket, reinterpret_cast<const struct sockaddr *>(&sockname), sizeof(struct sockaddr_un)) !=
                    -1,
                strerror(errno));
}

void CliDaemon::InitializeSessionSocket(void)
{
    int newSessionSocket = -1;
    int flag             = -1;

    VerifyOrExit((newSessionSocket = accept(mListenSocket, nullptr, nullptr)) != -1);

    VerifyOrExit((flag = fcntl(newSessionSocket, F_GETFD, 0)) != -1, close(newSessionSocket));

    flag |= FD_CLOEXEC;

    VerifyOrExit((flag = fcntl(newSessionSocket, F_SETFD, flag)) != -1, close(newSessionSocket));

#ifndef __linux__
    // some platforms (macOS, Solaris) don't have MSG_NOSIGNAL
    // SOME of those (macOS, but NOT Solaris) support SO_NOSIGPIPE
    // if we have SO_NOSIGPIPE, then set it. Otherwise, we're going
    // to simply ignore it.
#if defined(SO_NOSIGPIPE)
    VerifyOrExit((flag = setsockopt(newSessionSocket, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof(flag))) != -1,
                 close(newSessionSocket));
#else
#warning "no support for MSG_NOSIGNAL or SO_NOSIGPIPE"
#endif
#endif // __linux__

    Clear();

    mSessionSocket = newSessionSocket;
    otbrLogInfo("Session socket is ready");
exit:
    if (flag == -1)
    {
        otbrLogWarning("Failed to initialize session socket: %s", strerror(errno));
    }
}

void CliDaemon::Init(const std::string &aNetIfName)
{
    // This allows implementing pseudo reset.
    VerifyOrExit(mListenSocket == -1);

    mNetifName = aNetIfName;
    CreateListenSocketOrDie();

    //
    // only accept 1 connection.
    //
    VerifyOrDie(listen(mListenSocket, 1) != -1, strerror(errno));

exit:
    return;
}

void CliDaemon::Clear(void)
{
    if (mSessionSocket != -1)
    {
        close(mSessionSocket);
        mSessionSocket = -1;
    }
}

void CliDaemon::Deinit(void)
{
    Clear();
}

void CliDaemon::UpdateFdSet(MainloopContext *aContext)
{
    assert(aContext != nullptr);
    assert(mListenSocket != -1);
    assert(mSessionSocket != -1);

    aContext->AddFdToSet(mListenSocket, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);
    aContext->AddFdToSet(mSessionSocket, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);

    return;
}

void CliDaemon::Process(const MainloopContext *aContext)
{
    ssize_t received;

    VerifyOrExit(aContext != nullptr);
    VerifyOrExit(mListenSocket != -1);

    VerifyOrDie(!FD_ISSET(mListenSocket, &aContext->mErrorFdSet), strerror(errno));

    if (FD_ISSET(mListenSocket, &aContext->mReadFdSet))
    {
        InitializeSessionSocket();
    }

    VerifyOrExit(mSessionSocket != -1);

    if (FD_ISSET(mSessionSocket, &aContext->mErrorFdSet))
    {
        Clear();
    }
    else if (FD_ISSET(mSessionSocket, &aContext->mReadFdSet))
    {
        uint8_t   buffer[kCliMaxLineLength];
        otbrError error = OTBR_ERROR_NONE;

        VerifyOrDie((received = read(mSessionSocket, buffer, sizeof(buffer))) != -1, strerror(errno));

        if (received == 0)
        {
            otbrLogInfo("Session socket closed by peer");
            Clear();
        }
        else
        {
            error = mDeps.InputCommandLine(buffer, received);

            if (error != OTBR_ERROR_NONE)
            {
                otbrLogWarning("Failed to input command line, error:%s", otbrErrorString(error));
            }
        }
    }

exit:
    return;
}

} // namespace otbr
