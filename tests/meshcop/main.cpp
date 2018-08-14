#include "commissioner.hpp"
#include "commissioner_argcargv.hpp"
#include "device_hash.hpp"
#include <cstdio>
#include <cstdlib>
#include "common/logging.hpp"
#include "utils/hex.hpp"

using namespace ot;
using namespace ot::BorderRouter;

int main(int argc, char **argv)
{
    uint8_t      pskcBin[OT_PSKC_LENGTH];
    SteeringData steeringData;
    otbrLogInit("Commission server", OTBR_LOG_ERR);
    CommissionerArgs args = commissioner_argcargv(argc, argv);

    if (args.mHasPSKc)
    {
        memcpy(pskcBin, args.mPSKc_bin, sizeof(pskcBin));
    }
    else
    {
        ComputePskc(args.mXpanid_bin, args.mNetworkName, args.mPassPhrase, pskcBin);
    }

    if (args.mNeedComputePSKc)
    {
        char pskcAscii[2 * OT_PSKC_LENGTH + 1];
        pskcAscii[2 * OT_PSKC_LENGTH] = '\0';
        Utils::Bytes2Hex(pskcBin, OT_PSKC_LENGTH, pskcAscii);
        fprintf(stdout, "PSKc: %s\n", pskcAscii);
        exit(EXIT_SUCCESS);
    }

    steeringData = ComputeSteeringData(args.mSteeringLength, args.mAllowAllJoiners, args.mJoinerEui64_bin);
    if (args.mNeedComputeJoinerSteering || args.mNeedComputeJoinerHashMac)
    {
        uint8_t hashMacBin[kEui64Len];
        char    eui64Ascii[2 * kEui64Len + 1];
        char    hashMacAscii[2 * kEui64Len + 1];
        eui64Ascii[2 * kEui64Len]   = '\0';
        hashMacAscii[2 * kEui64Len] = '\0';
        ComputeHashMac(args.mJoinerEui64_bin, hashMacBin);
        Utils::Bytes2Hex(args.mJoinerEui64_bin, kEui64Len, eui64Ascii);
        Utils::Bytes2Hex(hashMacBin, kEui64Len, hashMacAscii);
        fprintf(stdout, "eiu64: %s\n", eui64Ascii);
        fprintf(stdout, "hashmac: %s\n", hashMacAscii);
        if (args.mNeedComputeJoinerSteering)
        {
            char *steeringDataAscii                         = new char[2 * steeringData.GetLength() + 1];
            steeringDataAscii[2 * steeringData.GetLength()] = 0;
            Utils::Bytes2Hex(steeringData.GetDataPointer(), steeringData.GetLength(), steeringDataAscii);
            fprintf(stdout, "steering-len: %d\n", steeringData.GetLength());
            fprintf(stdout, "steering-hex: %s\n", steeringDataAscii);
            delete[] steeringDataAscii;
        }
        exit(EXIT_SUCCESS);
    }

    if (args.mNeedCommissionDevice)
    {
        int kaRate = args.mSendCommKATxRate;
        if (!args.mNeedSendCommKA)
        {
            kaRate = 0;
        }
        Commissioner commissioner(pskcBin, args.mJoinerPSKd_ascii, steeringData, kaRate);
        sockaddr_in  addr;
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(atoi(args.mAgentPort_ascii));
        inet_pton(AF_INET, args.mAgentAddress_ascii, &addr.sin_addr);
        commissioner.Connect(addr);
        while (commissioner.IsCommissioner())
        {
            int            maxFd   = -1;
            struct timeval timeout = {10, 0};
            int            rval;
            fd_set         readFdSet;
            fd_set         writeFdSet;
            fd_set         errorFdSet;
            FD_ZERO(&readFdSet);
            FD_ZERO(&writeFdSet);
            FD_ZERO(&errorFdSet);
            commissioner.UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
            rval = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);
            if ((rval < 0) && (errno != EINTR))
            {
                rval = OTBR_ERROR_ERRNO;
                otbrLog(OTBR_LOG_ERR, "select() failed", strerror(errno));
                break;
            }
            commissioner.Process(readFdSet, writeFdSet, errorFdSet);
        }
        commissioner.Disconnect();
    }
    return 0;
}
