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

#include <CppUTest/TestHarness.h>

#include <stdio.h>
#include <time.h>

#include "common/logging.hpp"

TEST_GROUP(Logging)
{
};

TEST(Logging, TestLoggingHigherLevel)
{
    char ident[20];

    sprintf(ident, "otbr-test-%ld", clock());
    otbrLogInit(ident, OTBR_LOG_INFO);
    otbrLog(OTBR_LOG_DEBUG, "cool");
    otbrLogDeinit();

    char cmd[128];
    sprintf(cmd, "grep '%s.\\+cool' /var/log/syslog", ident);
    CHECK(0 != system(cmd));
}

TEST(Logging, TestLoggingEqualLevel)
{
    char ident[20];

    sprintf(ident, "otbr-test-%ld", clock());
    otbrLogInit(ident, OTBR_LOG_INFO);
    otbrLog(OTBR_LOG_INFO, "cool");
    otbrLogDeinit();

    char cmd[128];
    sprintf(cmd, "grep '%s.\\+cool' /var/log/syslog", ident);
    CHECK(0 == system(cmd));
}

TEST(Logging, TestLoggingLowerLevel)
{
    char ident[20];

    sprintf(ident, "otbr-test-%ld", clock());
    otbrLogInit(ident, OTBR_LOG_INFO);
    otbrLog(OTBR_LOG_WARNING, "cool");
    otbrLogDeinit();

    char cmd[128];
    sprintf(cmd, "grep '%s.\\+cool' /var/log/syslog", ident);
    CHECK(0 == system(cmd));
}

TEST(Logging, TestLoggingDump)
{
    char ident[20];

    sprintf(ident, "otbr-test-%ld", clock());
    otbrLogInit(ident, OTBR_LOG_INFO);
    otbrDump(OTBR_LOG_INFO, "cool", "cool", 4);
    otbrLogDeinit();

    char cmd[128];
    sprintf(cmd, "grep '%s.\\+#4 636f6f6c$' /var/log/syslog", ident);
    CHECK(0 == system(cmd));
}
