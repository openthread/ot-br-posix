#ifndef OTBR_COMMISSION_COMMON_H_
#define OTBR_COMMISSION_COMMON_H_

/**
 * Constants
 */
enum
{
    /* max size of a network packet */
    kSizeMaxPacket = 1500,

    /* delay between failed attempts to petition */
    kPetitionAttemptDelay = 5,

    /* max retry for petition */
    kPetitionMaxRetry = 2,

    /* Default size of steering data */
    kSteeringDefaultLength = 15,

    /* how long is an EUI64 in bytes */
    kEui64Len = (64 / 8),

    /* how long is a PSKd in bytes */
    kPSKdLength = 32,

    /* What port does our internal server use? */
    kPortJoinerSession = 49192,

    /* 64bit xpanid length in bytes */
    kXpanidLength = (64 / 8), /* 64bits */

    /* specification: 8.10.4 */
    kNetworkNameLenMax = 16,

    /* Spec is not specific about this items max length, so we choose 64 */
    kBorderRouterPassPhraseLen = 64,

};

#define FORWARD_PORT 23581

#endif
