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
 *   The file computes various values used during commissioning.
 */

#include "commissioner.hpp"


/** the hashmac is used in the steering data */
bool compute_hashmac(void)
{
    /* given ascii eui64, compute hashmac */
    mbedtls_sha256_context sha256;
    uint8_t                hash_result[32];
    int                    r;

    /* convert ascii EUI64 into BIN EUI64 */
    otbrLog(OTBR_LOG_INFO, "eui64: %s", gContext.mJoiner.mEui64.ascii);

    /* do we need to do this? */
    if (gContext.mJoiner.mHashMac.ascii[0])
    {
        otbrLog(OTBR_LOG_INFO, "note: hashmac already computed or provided");
    }
    else
    {

        if (gContext.mJoiner.mEui64.ascii[0] == 0)
        {
            fail("MISSING EUI64 address\n");
            return false;
        }

        r =  Hex2Bytes(gContext.mJoiner.mEui64.ascii,
                       gContext.mJoiner.mEui64.bin,
                       sizeof(gContext.mJoiner.mEui64.bin));
        if (r != 8)
        {
            fail("eui64 wrong length, or non-hex data\n");
            return false;
        }
        mbedtls_sha256_init(&sha256);
        mbedtls_sha256_starts(&sha256, 0);
        mbedtls_sha256_update(&sha256,
                              gContext.mJoiner.mEui64.bin,
                              sizeof(gContext.mJoiner.mEui64.bin));

        mbedtls_sha256_finish(&sha256, hash_result);
        /* Bytes 0..7, is the new data */
        memcpy(gContext.mJoiner.mHashMac.bin, hash_result, 8);
        /* Set the locally admin bit, byte 0, bit 1 */
        gContext.mJoiner.mHashMac.bin[0] |= 2;
        /* we now have the HASHMAC value */
        /* convert to ascii */
        Bytes2Hex(gContext.mJoiner.mHashMac.bin,
                  8,
                  gContext.mJoiner.mHashMac.ascii);
    }
    otbrLog(OTBR_LOG_INFO, "hash-mac: %s", gContext.mJoiner.mHashMac.ascii);

    /* success */
    return true;
}


/** compute preshared key for commissioner */
bool compute_pskc(void)
{

    const uint8_t *pKey;

    /* do we need to do this? */
    if (gContext.mAgent.mPSKc.ascii[0])
    {
        otbrLog(OTBR_LOG_INFO, "note: PSKc already computed, or provided");
    }
    else
    {

        otbrLog(OTBR_LOG_INFO, "xpanid: %s", gContext.mAgent.mXpanid.ascii);
        if (gContext.mAgent.mXpanid.ascii[0] == 0)
        {
            fail("compute PSKc: Missing xpanid\n");
            return false;
        }

        otbrLog(OTBR_LOG_INFO, "networkname: %s", gContext.mAgent.mNetworkName);
        if (gContext.mAgent.mNetworkName[0] == 0)
        {
            fail("compute PSKc: Missing networkname\n");
            return false;
        }

        otbrLog(OTBR_LOG_INFO, "passphrase: %s", gContext.mAgent.mPassPhrase);
        if (gContext.mAgent.mPassPhrase[0] == 0)
        {
            fail("compute PSKc: Missing br passphrase\n");
            return false;
        }

        otbrLog(OTBR_LOG_INFO, "note: calculating PSKc");
        pKey = gContext.mAgent.mPSKc.mTool.ComputePskc(gContext.mAgent.mXpanid.bin,
                                                       gContext.mAgent.mNetworkName,
                                                       gContext.mAgent.mPassPhrase);

        memcpy(gContext.mAgent.mPSKc.bin, pKey, OT_PSKC_LENGTH);
        /* convert to ascii for log purposes */
        Bytes2Hex(gContext.mAgent.mPSKc.bin, OT_PSKC_LENGTH, gContext.mAgent.mPSKc.ascii);
    }
    otbrLog(OTBR_LOG_INFO, "pskc: %s", gContext.mAgent.mPSKc.ascii);
    /* have a return here so this function matchs other "compute" functions */
    return true; /* success */
}

/** compute the steering data */
bool compute_steering(void)
{
    bool ok;

    do
    {
        if (gContext.mJoiner.mAllowAny)
        {
            otbrLog(OTBR_LOG_INFO, "JOINER: Allow any ingore hashmac");
            gContext.mJoiner.mSteeringData.Set();
            ok = true;
            break;
        }

        /* We require a hashmac */
        ok = compute_hashmac();
        if (!ok)
        {
            otbrLog(OTBR_LOG_INFO, "error: Cannot calculate steering data, bad hashmac");
            break;
        }

        gContext.mJoiner.mSteeringData.Clear();
        gContext.mJoiner.mSteeringData.ComputeBloomFilter(gContext.mJoiner.mHashMac.bin);


    }
    while (0);

    /* log result */
    if (ok)
    {
        int            n;
        const uint8_t *pBytes;

        n = gContext.mJoiner.mSteeringData.GetLength();

        pBytes = gContext.mJoiner.mSteeringData.GetDataPointer();

        otbrLog(OTBR_LOG_INFO, "steering-len: %d", n);
        otbrLog(OTBR_LOG_INFO, "steering-hex: %s", hex_string(pBytes, n));
    }
    return ok;
}
