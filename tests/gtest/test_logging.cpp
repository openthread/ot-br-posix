/*
 *    Copyright (c) 2017, The OpenThread Authors.
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

#define OTBR_LOG_TAG "TEST"

#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "common/logging.hpp"

// The tests read the message back from /var/log/syslog, which only Linux has.
#ifdef __linux__

static std::string GetUniqueIdent(void)
{
    long long nowUs =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();

    return "otbr-test-" + std::to_string(getpid()) + "-" + std::to_string(nowUs);
}

TEST(Logging, TestLoggingHigherLevel)
{
    std::string ident = GetUniqueIdent();

    otbrLogInit(ident.c_str(), OTBR_LOG_INFO, true, false);
    otbrLog(OTBR_LOG_DEBUG, OTBR_LOG_TAG, "cool-higher");
    otbrLogDeinit();
    sleep(0);

    std::string cmd    = "grep '" + ident + ".*cool-higher' /var/log/syslog";
    int         status = system(cmd.c_str());
    ASSERT_NE(status, -1);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 1);
}

TEST(Logging, TestLoggingEqualLevel)
{
    std::string ident = GetUniqueIdent();

    otbrLogInit(ident.c_str(), OTBR_LOG_INFO, true, false);
    otbrLog(OTBR_LOG_INFO, OTBR_LOG_TAG, "cool-equal");
    otbrLogDeinit();
    sleep(0);

    std::string cmd = "grep '" + ident + ".*cool-equal' /var/log/syslog";
    printf("CMD = %s\n", cmd.c_str());
    EXPECT_EQ(system(cmd.c_str()), 0);
}

TEST(Logging, TestLoggingEqualLevelNoSyslog)
{
    std::string ident = GetUniqueIdent();

    otbrLogInit(ident.c_str(), OTBR_LOG_INFO, true, true);
    otbrLog(OTBR_LOG_INFO, OTBR_LOG_TAG, "cool-equal-no-syslog");
    otbrLogDeinit();
    sleep(0);

    std::string cmd = "grep '" + ident + ".*cool-equal-no-syslog' /var/log/syslog";
    printf("CMD = %s\n", cmd.c_str());
    int status = system(cmd.c_str());
    ASSERT_NE(status, -1);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 1);
}

TEST(Logging, TestLoggingLowerLevel)
{
    std::string ident = GetUniqueIdent();

    otbrLogInit(ident.c_str(), OTBR_LOG_INFO, true, false);
    otbrLog(OTBR_LOG_WARNING, OTBR_LOG_TAG, "cool-lower");
    otbrLogDeinit();
    sleep(0);

    std::string cmd = "grep '" + ident + ".*cool-lower' /var/log/syslog";
    EXPECT_EQ(system(cmd.c_str()), 0);
}

TEST(Logging, TestLoggingDump)
{
    std::string ident = GetUniqueIdent();
    std::string cmd;

    otbrLogInit(ident.c_str(), OTBR_LOG_DEBUG, true, false);
    const char s[] = "one super long string with lots of text";
    otbrDump(OTBR_LOG_INFO, "Test", "foobar", s, sizeof(s));
    otbrLogDeinit();
    sleep(0);

    /*
     * Above produces output like this
     * otbr-test-5976[47088]: foobar: 0000: 6f 6e 65 20 73 75 70 65 72 20 6c 6f 6e 67 20 73
     * otbr-test-5976[47088]: foobar: 0010: 74 72 69 6e 67 20 77 69 74 68 20 6c 6f 74 73 20
     * otbr-test-5976[47088]: foobar: 0020: 6f 66 20 74 65 78 74 00
     */

    cmd = "grep '" + ident + ".*: foobar: 0000: 6f 6e 65 20 73 75 70 65 72 20 6c 6f 6e 67 20 73' /var/log/syslog";
    EXPECT_EQ(system(cmd.c_str()), 0);

    cmd = "grep '" + ident + ".*: foobar: 0010: 74 72 69 6e 67 20 77 69 74 68 20 6c 6f 74 73 20' /var/log/syslog";
    EXPECT_EQ(system(cmd.c_str()), 0);

    cmd = "grep '" + ident + ".*: foobar: 0020: 6f 66 20 74 65 78 74 00' /var/log/syslog";
    EXPECT_EQ(system(cmd.c_str()), 0);
}

#endif // __linux__
