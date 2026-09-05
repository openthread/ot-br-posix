/*
 *    Copyright (c) 2026, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#define OTBR_LOG_TAG "FIREWALL"

#include "firewall/pfctl.hpp"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

extern char **environ;

namespace otbr {
namespace Firewall {

const char PfctlProcess::kPfctlPath[] = "/sbin/pfctl";

namespace {

void ClosePipeEnd(int &aFd)
{
    if (aFd >= 0)
    {
        close(aFd);
        aFd = -1;
    }
}

bool CreatePipe(int aFds[2])
{
    bool ok = (pipe(aFds) == 0);

    // The parent's ends must not leak into pfctl (or any other child); the
    // ends pfctl needs are dup2'd onto 0/1/2 by the spawn file actions, which
    // clears close-on-exec on the copies.
    if (ok)
    {
        ok = (fcntl(aFds[0], F_SETFD, FD_CLOEXEC) == 0) && (fcntl(aFds[1], F_SETFD, FD_CLOEXEC) == 0);
    }

    return ok;
}

// Appends what is readable from @p aFd to @p aOut; returns false at EOF or on error.
bool DrainPipe(int aFd, std::string &aOut)
{
    char    buf[1024];
    ssize_t count;

    do
    {
        count = read(aFd, buf, sizeof(buf));
    } while (count < 0 && errno == EINTR);

    if (count > 0)
    {
        aOut.append(buf, static_cast<size_t>(count));
    }

    return count > 0;
}

} // namespace

otbrError PfctlProcess::Run(const std::vector<std::string> &aArgs,
                            const std::string              &aInput,
                            std::string                    &aOutput,
                            std::string                    &aError)
{
    otbrError                  error      = OTBR_ERROR_NONE;
    int                        inPipe[2]  = {-1, -1};
    int                        outPipe[2] = {-1, -1};
    int                        errPipe[2] = {-1, -1};
    posix_spawn_file_actions_t actions;
    bool                       hasActions = false;
    pid_t                      pid        = -1;
    int                        status     = 0;
    int                        rc;
    size_t                     written = 0;
    std::vector<char *>        argv;

    aOutput.clear();
    aError.clear();

    VerifyOrExit(CreatePipe(inPipe) && CreatePipe(outPipe) && CreatePipe(errPipe), error = OTBR_ERROR_ERRNO);

    // The posix_spawn family returns the error number instead of setting errno.
    rc = posix_spawn_file_actions_init(&actions);
    VerifyOrExit(rc == 0, errno = rc; error = OTBR_ERROR_ERRNO);
    hasActions = true;
    rc         = posix_spawn_file_actions_adddup2(&actions, inPipe[0], STDIN_FILENO);
    VerifyOrExit(rc == 0, errno = rc; error = OTBR_ERROR_ERRNO);
    rc = posix_spawn_file_actions_adddup2(&actions, outPipe[1], STDOUT_FILENO);
    VerifyOrExit(rc == 0, errno = rc; error = OTBR_ERROR_ERRNO);
    rc = posix_spawn_file_actions_adddup2(&actions, errPipe[1], STDERR_FILENO);
    VerifyOrExit(rc == 0, errno = rc; error = OTBR_ERROR_ERRNO);

    argv.push_back(const_cast<char *>("pfctl"));
    for (const std::string &arg : aArgs)
    {
        argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);

    rc = posix_spawn(&pid, kPfctlPath, &actions, nullptr, argv.data(), environ);
    VerifyOrExit(rc == 0, errno = rc; error = OTBR_ERROR_ERRNO);

    ClosePipeEnd(inPipe[0]);
    ClosePipeEnd(outPipe[1]);
    ClosePipeEnd(errPipe[1]);

#ifdef F_SETNOSIGPIPE
    // If pfctl exits before it has read all of its input (a syntax error in
    // the ruleset, say), the next write would raise SIGPIPE and take the
    // agent down with it; ask for EPIPE instead so the failure is reported.
    fcntl(inPipe[1], F_SETNOSIGPIPE, 1);
#endif

    // pfctl reads its whole input (a ruleset of a few hundred bytes) before it
    // produces any output, so writing it all first cannot deadlock on the
    // output pipes.
    while (written < aInput.size())
    {
        ssize_t count = write(inPipe[1], aInput.data() + written, aInput.size() - written);

        if (count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            // pfctl is gone: stop writing, but still drain what it said on
            // stderr and report its exit status below.
            VerifyOrExit(errno == EPIPE, error = OTBR_ERROR_ERRNO);
            break;
        }
        written += static_cast<size_t>(count);
    }
    ClosePipeEnd(inPipe[1]);

    // Drain stdout and stderr together so neither can fill up and stall pfctl.
    while (outPipe[0] >= 0 || errPipe[0] >= 0)
    {
        struct pollfd fds[2];
        nfds_t        count = 0;

        if (outPipe[0] >= 0)
        {
            fds[count].fd      = outPipe[0];
            fds[count].events  = POLLIN;
            fds[count].revents = 0;
            count++;
        }
        if (errPipe[0] >= 0)
        {
            fds[count].fd      = errPipe[0];
            fds[count].events  = POLLIN;
            fds[count].revents = 0;
            count++;
        }

        if (poll(fds, count, -1) < 0)
        {
            VerifyOrExit(errno == EINTR, error = OTBR_ERROR_ERRNO);
            continue;
        }

        for (nfds_t i = 0; i < count; i++)
        {
            if (fds[i].revents == 0)
            {
                continue;
            }
            if (fds[i].fd == outPipe[0])
            {
                if (!DrainPipe(outPipe[0], aOutput))
                {
                    ClosePipeEnd(outPipe[0]);
                }
            }
            else if (fds[i].fd == errPipe[0])
            {
                if (!DrainPipe(errPipe[0], aError))
                {
                    ClosePipeEnd(errPipe[0]);
                }
            }
        }
    }

    while (waitpid(pid, &status, 0) < 0)
    {
        VerifyOrExit(errno == EINTR, error = OTBR_ERROR_ERRNO);
    }
    pid = -1;

    VerifyOrExit(WIFEXITED(status) && WEXITSTATUS(status) == 0, error = OTBR_ERROR_ERRNO);

exit:
    // Our pipe ends go first, so a pfctl that is still reading stdin sees EOF
    // rather than waiting on us while we wait on it.
    ClosePipeEnd(inPipe[0]);
    ClosePipeEnd(inPipe[1]);
    ClosePipeEnd(outPipe[0]);
    ClosePipeEnd(outPipe[1]);
    ClosePipeEnd(errPipe[0]);
    ClosePipeEnd(errPipe[1]);
    if (pid > 0)
    {
        // Spawned but not reaped because something failed part-way: don't
        // block on a child that may hang, and don't leave a zombie behind.
        kill(pid, SIGKILL);
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        {
        }
    }
    if (hasActions)
    {
        posix_spawn_file_actions_destroy(&actions);
    }

    return error;
}

} // namespace Firewall
} // namespace otbr
