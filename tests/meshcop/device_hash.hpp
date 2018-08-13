#ifndef OTBR_DEVICE_HASH_H_
#define OTBR_DEVICE_HASH_H_

#include <stdint.h>
#include "utils/steeringdata.hpp"

namespace ot {
namespace BorderRouter {

void ComputePskc(const uint8_t *aExtPanIdBin, const char *aNetworkName, const char *aPassphrase, uint8_t *aPskcOutBuf);
void ComputeHashMac(uint8_t *aEui64Bin, uint8_t *aHashMacOutBuf);
SteeringData ComputeSteeringData(uint8_t length, bool aAllowAny, uint8_t *aEui64Bin);

}
}

#endif
