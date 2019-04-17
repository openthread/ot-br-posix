/*
 *    Copyright (c) 2018, The OpenThread Authors.
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
 *   The file is the entry point for commissioner
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <signal.h>

#include "commissioner.hpp"
#include "commissioner_argcargv.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "utils/hex.hpp"

using namespace ot;
using namespace ot::BorderRouter;

static void HandleSignal(int aSignal)
{
    signal(aSignal, SIG_DFL);
}

int main(int argc, char **argv)
{
    otbrError        error;
    CommissionerArgs args;
    int              ret = 0;

    SuccessOrExit(error = ParseArgs(argc, argv, args));

    otbrLogInit("Commissioner", args.mDebugLevel, true);
    signal(SIGTERM, HandleSignal);
    signal(SIGINT, HandleSignal);

    srand(static_cast<unsigned int>(time(0)));

    {
        Commissioner commissioner(args.mPSKc, args.mKeepAliveInterval);
        bool         joinerSetDone = false;

        commissioner.InitDtls(args.mAgentHost, args.mAgentPort);

        do
        {
            ret = commissioner.TryDtlsHandshake();
        } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

        if (commissioner.IsValid())
        {
            commissioner.CommissionerPetition();
        }

        while (commissioner.IsValid())
        {
            int            maxFd   = -1;
            struct timeval timeout = {10, 0};
            int            rval;

            fd_set readFdSet;
            fd_set writeFdSet;
            fd_set errorFdSet;

            FD_ZERO(&readFdSet);
            FD_ZERO(&writeFdSet);
            FD_ZERO(&errorFdSet);
            commissioner.UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
            rval = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);
            if (rval < 0)
            {
                otbrLog(OTBR_LOG_ERR, "select() failed", strerror(errno));
                break;
            }
            commissioner.Process(readFdSet, writeFdSet, errorFdSet);
            if (commissioner.IsCommissionerAccepted() && !joinerSetDone)
            {
                commissioner.SetJoiner(args.mPSKd, args.mSteeringData);
                joinerSetDone = true;
            }
        }
    }

exit:
    return error;
}
