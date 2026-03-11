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

/**
 * @file
 *   This file includes definitions of the Cli Daemon of otbr-agent.
 */

#ifndef OTBR_AGENT_POSIX_DAEMON_HPP_
#define OTBR_AGENT_POSIX_DAEMON_HPP_

#include <string>

#include <openthread/cli.h>
#include <openthread/platform/toolchain.h>

#include "common/mainloop.hpp"
#include "common/types.hpp"

namespace otbr {

class CliDaemon : public MainloopProcessor
{
public:
    /**
     * This enumeration defines the CLI Daemon modes.
     */
    enum : uint8_t
    {
        OTBR_DAEMON_MODE_UNIX_SOCKET = (1 << 0), ///< Enable CLI over Unix socket.
        OTBR_DAEMON_MODE_CONSOLE     = (1 << 1), ///< Enable CLI over console (stdin/stdout).
    };
    static constexpr size_t kCliMaxLineLength = OTBR_CONFIG_CLI_MAX_LINE_LENGTH;

    class Dependencies
    {
    public:
        virtual ~Dependencies(void) = default;

        virtual otbrError InputCommandLine(const char *aLine);
    };

    explicit CliDaemon(Dependencies &aDependencies, uint8_t aMode = OTBR_DAEMON_MODE_UNIX_SOCKET);

    otbrError Init(const std::string &aNetIfName);
    void      Deinit(void);

    void Update(MainloopContext &aContext) override;
    void Process(const MainloopContext &aContext) override;

    void ProcessLine(char *aLine, int aOutputFd);
    void ProcessCommand(const char *aLine, void *aContext, otCliOutputCallback aCallback);

    int OutputFormatV(const char *aFormat, va_list aArguments) OT_TOOL_PRINTF_STYLE_FORMAT_ARG_CHECK(2, 0);
    int OutputFormat(const char *aFormat, ...) OT_TOOL_PRINTF_STYLE_FORMAT_ARG_CHECK(2, 3);

private:
#if defined(HAVE_LIBEDIT) || defined(HAVE_LIBREADLINE)
    class Readline
    {
    public:
        void Init(CliDaemon &aDaemon);
        void Deinit(void);
        void Update(MainloopContext &aContext);
        void Process(const MainloopContext &aContext);

    private:
        static void       InputCallback(char *aLine);
        static CliDaemon *sDaemon;
    };

    Readline mReadline;
#else
    class Stdio
    {
    public:
        void Init(CliDaemon &aDaemon);
        void Deinit(void);
        void Update(MainloopContext &aContext);
        void Process(const MainloopContext &aContext);

    private:
        CliDaemon *mDaemon;
    };

    Stdio mStdio;
#endif

    int       UnixSocketOutputV(const char *aFormat, va_list aArguments) OT_TOOL_PRINTF_STYLE_FORMAT_ARG_CHECK(2, 0);
    int       UnixSocketOutput(const char *aOutput, size_t aLength);
    otbrError UnixSocketCreate(const std::string &aNetIfName);
    void      UnixSocketSessionInit(void);
    void      UnixSocketSessionClear(void);

    otbrError SetOutputFd(int aFd);

    void Reject(const char *aFormat, ...) OT_TOOL_PRINTF_STYLE_FORMAT_ARG_CHECK(2, 3);

    otCliOutputCallback mExternalOutputCallback        = nullptr;
    void               *mExternalOutputCallbackContext = nullptr;

    int     mListenSocket;
    int     mDaemonLock;
    int     mSessionSocket;
    int     mOutputFd;
    uint8_t mDaemonMode;

    Dependencies &mDeps;
};

} // namespace otbr

#endif // OTBR_AGENT_POSIX_DAEMON_HPP_
