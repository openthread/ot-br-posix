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

/**
 * @file
 *   The file test various computations used during commissioning.
 */

#include "commissioner.hpp"

/* test the preshared key for commissioning */
static void test_pskc()
{
    int n;

    /* calculate PSKc -
     * see spec section 8.4.1.2.2 Test Vector For Derivation of PSKc
     */
    strcpy(gContext.mAgent.mPassPhrase, "12SECRETPASSWORD34");
    strcpy(gContext.mAgent.mNetworkName, "Test Network");
    strcpy(gContext.mAgent.mXpanid.ascii, "0001020304050607");
    n = Hex2Bytes(gContext.mAgent.mXpanid.ascii, gContext.mAgent.mXpanid.bin, sizeof(gContext.mAgent.mXpanid.bin));
    if (n != sizeof(gContext.mAgent.mXpanid.bin))
    {
        CommissionerUtilsFail("cannot convert xpanid\n");
    }

    CommissionerComputePskc();

    static const uint8_t expected[] = {0xc3, 0xf5, 0x93, 0x68, 0x44, 0x5a, 0x1b, 0x61,
                                       0x06, 0xbe, 0x42, 0x0a, 0x70, 0x6d, 0x4c, 0xc9};
    otbrLog(OTBR_LOG_INFO, "Expected: %s\n", CommissionerUtilsHexString(expected, sizeof(expected)));

    if (0 != memcmp(expected, gContext.mAgent.mPSKc.bin, sizeof(expected)))
    {
        CommissionerUtilsFail("PSKC calculation fails test vector\n");
    }
    otbrLog(OTBR_LOG_INFO, "PSKC: test success\n");
}

/** test steering data */
static void test_steering(void)
{
    bool ok;

    gContext.mJoiner.mSteeringData.Clear();
    gContext.mJoiner.mSteeringData.SetLength(15);

    strcpy(gContext.mJoiner.mEui64.ascii, "18b4300000000002");
    ok = CommissionerComputeHashMac();
    if (!ok)
    {
        CommissionerUtilsFail("invalid hashmac\n");
    }
    ok = CommissionerComputeSteering();
    if (!ok)
    {
        CommissionerUtilsFail("Cannot compute steering\n");
    }

    const uint8_t expected[] = {/* NOTE: this is an odd sized steering data
                                 * it is valid, the steering data must be
                                 * between 1 and 16 bytes, thus 15 is OK!
                                 */
                                0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    otbrLog(OTBR_LOG_INFO, "expected: %s\n", CommissionerUtilsHexString(expected, sizeof(expected)));
    const uint8_t *pData;
    pData = gContext.mJoiner.mSteeringData.GetDataPointer();

    if (0 != memcmp(expected, pData, sizeof(expected)))
    {
        CommissionerUtilsFail("FAIL: Steering data");
    }
    otbrLog(OTBR_LOG_INFO, "SUCCESS: Steering data\n");
}

/** the argcargv module calls this when the arg is found on the command line */
void CommissionerCmdLineSelfTest(argcargv *pThis)
{
    (void)(pThis); /* not used */
    otbrLog(OTBR_LOG_INFO, "SELFTEST START\n");
    test_pskc();
    test_steering();

    otbrLog(OTBR_LOG_INFO, "SUCCESS\n");
    fprintf(stdout, "selftest: SUCCESS\n");
    exit(EXIT_SUCCESS);
}
