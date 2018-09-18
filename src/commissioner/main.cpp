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
#include "device_hash.hpp"
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
    uint8_t          pskcBin[OT_PSKC_LENGTH];
    SteeringData     steeringData;
    CommissionerArgs args = ParseArgs(argc, argv);

    otbrLogInit("Commission server", OTBR_LOG_INFO);

    if (args.mHasPSKc)
    {
        memcpy(pskcBin, args.mPSKcBin, sizeof(pskcBin));
    }
    else
    {
        ComputePskc(args.mXpanidBin, args.mNetworkName, args.mPassPhrase, pskcBin);
    }

    if (args.mNeedComputePSKc)
    {
        char pskcAscii[2 * OT_PSKC_LENGTH + 1];

        pskcAscii[2 * OT_PSKC_LENGTH] = '\0';
        Utils::Bytes2Hex(pskcBin, OT_PSKC_LENGTH, pskcAscii);
        fprintf(stdout, "PSKc: %s\n", pskcAscii);
        exit(EXIT_SUCCESS);
    }

    steeringData = ComputeSteeringData(args.mSteeringLength, args.mAllowAllJoiners, args.mJoinerEui64Bin);
    if (args.mNeedComputeJoinerSteering || args.mNeedComputeJoinerHashMac)
    {
        uint8_t hashMacBin[kEui64Len];
        char    eui64Ascii[2 * kEui64Len + 1];
        char    hashMacAscii[2 * kEui64Len + 1];

        eui64Ascii[2 * kEui64Len]   = '\0';
        hashMacAscii[2 * kEui64Len] = '\0';
        ComputeHashMac(args.mJoinerEui64Bin, hashMacBin);
        Utils::Bytes2Hex(args.mJoinerEui64Bin, kEui64Len, eui64Ascii);
        Utils::Bytes2Hex(hashMacBin, kEui64Len, hashMacAscii);
        fprintf(stdout, "eui64: %s\n", eui64Ascii);
        fprintf(stdout, "hashmac: %s\n", hashMacAscii);
        if (args.mNeedComputeJoinerSteering)
        {
            char *steeringDataAscii = new char[2 * steeringData.GetLength() + 1];

            steeringDataAscii[2 * steeringData.GetLength()] = 0;
            Utils::Bytes2Hex(steeringData.GetBloomFilter(), steeringData.GetLength(), steeringDataAscii);
            fprintf(stdout, "steering-len: %d\n", steeringData.GetLength());
            fprintf(stdout, "steering-hex: %s\n", steeringDataAscii);
            delete[] steeringDataAscii;
        }
        exit(EXIT_SUCCESS);
    }

    signal(SIGTERM, HandleSignal);

    if (args.mNeedCommissionDevice)
    {
        int  kaRate = args.mSendCommKATxRate;
        int  ret;
        bool joinerSetDone = false;

        if (!args.mNeedSendCommKA)
        {
            kaRate = 0;
        }
        srand(time(0));
        Commissioner commissioner(pskcBin, kaRate);
        commissioner.InitDtls(args.mAgentAddress_ascii, args.mAgentPort_ascii);
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
                commissioner.SetJoiner(args.mJoinerPSKdAscii, steeringData);
                joinerSetDone = true;
            }
        }
    }
    return 0;
}
