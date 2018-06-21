#include <cstdio>

#include "common/code_utils.hpp"
#include "pskc-generator/pskc.hpp"
#include "utils/hex.hpp"

/**
 * Constants.
 */
enum
{
    kMaxNetworkName = 16,
    kMaxPassphrase  = 255,
    kSizeExtPanId   = 8,
};

void help(void)
{
    printf("pskc - generate PSKc\n"
           "SYNTAX:\n"
           "    pskc <PASSPHRASE> <EXTPANID> <NETWORK_NAME>\n"
           "EXAMPLE:\n"
           "    pskc 654321 1122334455667788 OpenThread\n");
}

int printPSKc(const char *aPassphrase, const char *aExtPanId, const char *aNetworkName)
{
    uint8_t extpanid[kSizeExtPanId];
    size_t  length;
    int     ret = -1;

    ot::Psk::Pskc  pskcComputer;
    const uint8_t *pskc;

    length = strlen(aPassphrase);
    VerifyOrExit(length > 0, printf("PASSPHRASE must not be empty.\n"));
    VerifyOrExit(length <= kMaxPassphrase,
                 printf("PASSPHRASE Passphrase must be no more than %d bytes.\n", kMaxPassphrase));

    length = strlen(aExtPanId);
    VerifyOrExit(length == kSizeExtPanId * 2, printf("EXTPANID length must be %d bytes.\n", kSizeExtPanId));
    for (size_t i = 0; i < length; i++)
    {
        VerifyOrExit((aExtPanId[i] <= '9' && aExtPanId[i] >= '0') || (aExtPanId[i] <= 'f' && aExtPanId[i] >= 'a') ||
                         (aExtPanId[i] <= 'F' && aExtPanId[i] >= 'A'),
                     printf("EXTPANID must be encoded in hex.\n"));
    }
    ot::Utils::Hex2Bytes(aExtPanId, extpanid, sizeof(extpanid));

    length = strlen(aNetworkName);
    VerifyOrExit(length > 0, printf("NETWORK_NAME must not be empty.\n"));
    VerifyOrExit(length <= kMaxNetworkName,
                 printf("NETWOR_KNAME length must be no more than %d bytes.\n", kMaxNetworkName));

    pskc = pskcComputer.ComputePskc(extpanid, aNetworkName, aPassphrase);
    for (int i = 0; i < 16; i++)
    {
        printf("%02x", pskc[i]);
    }
    printf("\n");
    ret = 0;

exit:
    return ret;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    VerifyOrExit(argc == 4, help(), ret = -1);
    ret = printPSKc(argv[1], argv[2], argv[3]);

exit:
    return ret;
}
