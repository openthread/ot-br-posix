#include "device_hash.hpp"
#include "commission_common.hpp"
#include "web/pskc-generator/pskc.hpp"
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#include <cstdio>
#include <mbedtls/certs.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/ecjpake.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/timing.h>

namespace ot {
namespace BorderRouter {

void ComputePskc(const uint8_t *aExtPanIdBin, const char *aNetworkName, const char *aPassphrase, uint8_t *aPskcOutBuf)
{
    ot::Psk::Pskc  pskc;
    const uint8_t *pskcBin = pskc.ComputePskc(aExtPanIdBin, aNetworkName, aPassphrase);
    memcpy(aPskcOutBuf, pskcBin, OT_PSKC_LENGTH);
}

void ComputeHashMac(uint8_t *aEui64Bin, uint8_t *aHashMacOutBuf)
{
    mbedtls_sha256_context sha256;
    uint8_t                hash_result[32];
    mbedtls_sha256_init(&sha256);
    mbedtls_sha256_starts(&sha256, 0);
    mbedtls_sha256_update(&sha256, aEui64Bin, kEui64Len);
    mbedtls_sha256_finish(&sha256, hash_result);
    /* Bytes 0..7, is the new data */
    memcpy(aHashMacOutBuf, hash_result, 8);
    /* Set the locally admin bit, byte 0, bit 1 */
    aHashMacOutBuf[0] |= 2;
}

SteeringData ComputeSteeringData(uint8_t length, bool aAllowAny, uint8_t *aEui64Bin)
{
    SteeringData data;
    data.Init();
    data.SetLength(length);
    data.Clear();
    if (aAllowAny)
    {
        data.Set();
    }
    else
    {
        uint8_t hashMacBin[kEui64Len];
        ComputeHashMac(aEui64Bin, hashMacBin);
        data.Clear();
        data.ComputeBloomFilter(hashMacBin);
    }
    return data;
}

} // namespace BorderRouter
} // namespace ot
