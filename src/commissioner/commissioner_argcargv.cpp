/*
 *    Copyright (c) 2017-2018, The OpenThread Authors.
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

/**
 * @file
 *   The file implements the command line params for the commissioner test app.
 */
#include "commissioner_argcargv.hpp"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "utils/hex.hpp"
#include "utils/pskc.hpp"

namespace ot {
namespace BorderRouter {

/** Handle the preshared joining credential for the joining device on the command line */
static bool CheckPSKd(const char *aPSKd)
{
    const char *whybad = NULL;
    size_t      len    = strlen(aPSKd);

    /*
     * Problem: Should we "base32" decode this per the specification?
     * Answer: No - because this needs to be identical to the CLI application
     * The CLI appication does *NOT* decode preshared key
     * thus we do not decode the base32 value here
     * We do however enforce the data..
     */

    /*
     * Joining Device Credential
     * Specification 1.1.1, Section 8.2 Table 8-1
     * Min Length 6, Max Length 32.
     *
     * Digits 0-9, Upper case only Letters A-Z
     * excluding: I,O,Q,Z
     *
     * Note: 26 letters - 4 illegals = 22 letters.
     * Thus 10 digits + 22 letters = 32 symbols.
     * Thus, "base32" encoding using the above.
     */
    VerifyOrExit((len >= 6) || (len <= 32), whybad = "Invalid PSKd length (range: 6..32)");

    for (size_t i = 0; i < len; ++i)
    {
        char ch = aPSKd[i];

        switch (ch)
        {
        case 'Z':
        case 'I':
        case 'O':
        case 'Q':
            ExitNow(whybad = "Letters I, O, Q and Z are not allowed");
            break;
        default:
            VerifyOrExit(isupper(ch) || isdigit(ch), whybad = "contains non-uppercase or non-digit");
            break;
        }
    }

exit:
    if (whybad != NULL)
    {
        fprintf(stderr, "Illegal PSKd: \"%s\", %s\n", aPSKd, whybad);
    }

    return whybad == NULL;
}

static void PrintUsage(const char *aProgram, FILE *aStream, int aExitCode)
{
    fprintf(aStream,
            "Syntax:\n"
            "    %s [Options]\n"
            "Options:\n"
            "    -H, --agent-host           STRING      Host of border agent\n"
            "    -P, --agent-port           NUMBER      UDP port of border agent\n"
            "    -N, --network-name         STRING      UTF-8 encoded network name\n"
            "    -C, --network-password     STRING      Thread network password\n"
            "    -X, --xpanid               HEX         Extended PAN ID in hex\n"
            "    -A, --allow-all                        Allow all joiners\n"
            "    -E, --joiner-eui64         HEX         Joiner EUI64 value\n"
            "    -D, --joiner-pskd          STRING      Joiner's base32-thread encoded PSK\n"
            "    -L, --steering-data-length NUMBER      Steering data length(1~16)\n"
            "    -l, --log-file             PATH        Log to file\n"
            "    -i, --keep-alive-interval  NUMBER      COMM_KA requests interval\n"
            "    -d, --debug-level          NUMBER      Debug level(0~7)\n"
            "    -q, --disable-syslog                   Disable log via syslog\n"
            "    -h, --help                             Print this help\n",
            aProgram);

    exit(aExitCode);
}

otbrError ParseArgs(int aArgc, char *aArgv[], CommissionerArgs &aArgs)
{
    static struct option options[] = {{"joiner-eui64", required_argument, NULL, 'E'},
                                      {"joiner-pskd", required_argument, NULL, 'D'},
                                      {"allow-all", no_argument, NULL, 'A'},
                                      {"network-password", required_argument, NULL, 'C'},
                                      {"network-name", required_argument, NULL, 'N'},
                                      {"xpanid", required_argument, NULL, 'X'},
                                      {"agent-host", required_argument, NULL, 'H'},
                                      {"agent-port", required_argument, NULL, 'P'},
                                      {"steering-data-length", required_argument, NULL, 'L'},
                                      {"log-file", required_argument, NULL, 'l'},
                                      {"disable-syslog", no_argument, NULL, 'q'},
                                      {"debug-level", required_argument, NULL, 'd'},
                                      {"keep-alive-interval", required_argument, NULL, 'i'},
                                      {"help", no_argument, NULL, 'h'},
                                      {0, 0, 0, 0}};

    uint8_t     xPanId[kXPanIdLength];
    uint8_t     joinerEui64[kEui64Len];
    otbrError   error           = OTBR_ERROR_ERRNO;
    const char *networkName     = NULL;
    const char *networkPassword = NULL;
    int         steeringLength  = 0;
    bool        isXPanIdSet     = false;
    bool        isEui64Set      = false;
    bool        allowAllJoiners = false;

    memset(&aArgs, 0, sizeof(aArgs));

    aArgs.mKeepAliveInterval = 15;
    aArgs.mDebugLevel        = OTBR_LOG_ERR;

    if (aArgc == 1)
    {
        ExitNow(PrintUsage(aArgv[0], stdout, EXIT_SUCCESS));
    }

    while (true)
    {
        int option = getopt_long(aArgc, aArgv, "E:D:AC:N:X:H:P:L:l:qd:i:h", options, NULL);

        if (option == -1)
        {
            break;
        }

        switch (option)
        {
        case 'E':
            isEui64Set = true;
            VerifyOrExit(sizeof(joinerEui64) == Utils::Hex2Bytes(optarg, joinerEui64, sizeof(joinerEui64)),
                         fprintf(stderr, "Invalid joiner EUI64!"));
            break;
        case 'D':
            aArgs.mPSKd = optarg;
            VerifyOrExit(CheckPSKd(aArgs.mPSKd));
            break;
        case 'A':
            allowAllJoiners = true;
            break;
        case 'C':
        {
            size_t len = strlen(optarg);

            VerifyOrExit(len > 0 && len <= 255, fprintf(stderr, "Network password length must be between 6 and 255!"));
            networkPassword = optarg;
            break;
        }
        case 'N':
        {
            size_t len = strlen(optarg);
            VerifyOrExit(len > 0 && len <= 16, fprintf(stderr, "Network name length must be between 1 and 16!"));

            networkName = optarg;
            break;
        }
        case 'X':
            isXPanIdSet = true;
            VerifyOrExit(sizeof(xPanId) == Utils::Hex2Bytes(optarg, xPanId, sizeof(xPanId)),
                         fprintf(stderr, "Invalid xpanid!"));
            break;
        case 'H':
            aArgs.mAgentHost = optarg;
            break;
        case 'P':
            aArgs.mAgentPort = optarg;
            break;
        case 'L':
            steeringLength = atoi(optarg);
            VerifyOrExit(steeringLength >= 1 and steeringLength <= 16,
                         fprintf(stderr, "Steering data length must be between 1 and 16!"));
            break;
        case 'q':
            otbrLogEnableSyslog(false);
            break;
        case 'l':
            otbrLogSetFilename(optarg);
            break;
        case 'd':
            aArgs.mDebugLevel = atoi(optarg);
            VerifyOrExit(aArgs.mDebugLevel >= 1 and aArgs.mDebugLevel <= 7,
                         fprintf(stderr, "Debug level must be between 1 and 7!"));
            break;
        case 'i':
            aArgs.mKeepAliveInterval = atoi(optarg);
            VerifyOrExit(aArgs.mKeepAliveInterval >= 0, fprintf(stderr, "Invalid value for keep alive interval!"));
            break;
        case 'h':
            PrintUsage(aArgv[0], stdout, EXIT_SUCCESS);
            break;
        case '?':
            PrintUsage(aArgv[0], stderr, EXIT_FAILURE);
            break;
        default:
            assert(false);
            break;
        }
    }

    VerifyOrExit(aArgs.mPSKd != NULL, fprintf(stderr, "Missing joiner PSKd!"));
    VerifyOrExit(networkName != NULL, fprintf(stderr, "Missing network name!"));
    VerifyOrExit(networkPassword != NULL, fprintf(stderr, "Missing network password!"));
    VerifyOrExit(isXPanIdSet, fprintf(stderr, "Missing extended PAN ID!"));

    if (steeringLength == 0)
    {
        steeringLength = (allowAllJoiners ? 1 : kSteeringDefaultLength);
    }

    aArgs.mSteeringData.Init(static_cast<uint8_t>(steeringLength));

    if (!allowAllJoiners)
    {
        VerifyOrExit(aArgs.mPSKd != NULL, fprintf(stderr, "Missing PSKd!"));
        VerifyOrExit(isEui64Set, fprintf(stderr, "Missing EUI64!"));

        aArgs.mSteeringData.ComputeJoinerId(joinerEui64, joinerEui64);
        aArgs.mSteeringData.ComputeBloomFilter(joinerEui64);
    }
    else
    {
        aArgs.mSteeringData.Set();
    }

    {
        ot::Psk::Pskc pskc;

        memcpy(aArgs.mPSKc, pskc.ComputePskc(xPanId, networkName, networkPassword), sizeof(aArgs.mPSKc));
    }

    error = OTBR_ERROR_NONE;

exit:
    if (error != OTBR_ERROR_NONE)
    {
        errno = EINVAL;
    }

    return error;
}

} // namespace BorderRouter
} // namespace ot
