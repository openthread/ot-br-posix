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

#include <vector>

#include "common/mainloop.hpp"
#include "common/types.hpp"

namespace otbr {

class CliDaemon
{
public:
    class Dependencies
    {
    public:
        virtual ~Dependencies(void) = default;

        virtual otbrError InputCommandLine(const char *aLine);
    };

    explicit CliDaemon(Dependencies &aDependencies);

    otbrError Init(const std::string &aNetIfName);
    void      Deinit(void);

    void HandleCommandOutput(const char *aOutput);
    void Process(const MainloopContext &aContext);
    void UpdateFdSet(MainloopContext &aContext);

private:
    static constexpr size_t kCliMaxLineLength = OTBR_CONFIG_CLI_MAX_LINE_LENGTH;

    void Clear(void);

    std::string GetSocketFilename(const std::string &aNetIfName, const char *aSuffix) const;

    otbrError CreateListenSocket(const std::string &aNetIfName);
    void      InitializeSessionSocket(void);

    int mListenSocket;
    int mDaemonLock;
    int mSessionSocket;

    Dependencies &mDeps;
};

} // namespace otbr

#endif // OTBR_AGENT_POSIX_DAEMON_HPP_
