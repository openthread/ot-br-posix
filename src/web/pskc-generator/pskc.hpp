/*
 *  Copyright (c) 2017, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements the generating pskc function.
 */

#ifndef PSKC_HPP
#define PSKC_HPP

#define EXTEND_PAN_ID_LEN 8
#define PBKDF2_SALT_MAX_LEN 30
#define PSKC_LENGTH 16
#define ITERATION_COUNTS 16384
#define MAX_PASSPHRASE_LEN 30

#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include <mbedtls/cmac.h>

namespace ot {
namespace Psk {

enum
{
    kPskcStatus_Ok                       = 0,
    kPskcStatus_InvalidArgument          = 1
};

class Pskc
{
public:
    void SetPassphrase(const char *aPassphrase)
    {
        mPassphraseLen = strlen(aPassphrase);
        memcpy(mPassphrase, aPassphrase, mPassphraseLen);
    }
    void SetSalt(const uint8_t *aExtPanId, const char *aNetworkName);
    uint8_t *GetPskc(void)
    {
        Pbkdf2Cmac();
        return mPskc;
    }
    void Pbkdf2Cmac(void);

private:
    uint8_t  mPassphrase[MAX_PASSPHRASE_LEN];
    uint16_t mPassphraseLen;
    char     mSalt[PBKDF2_SALT_MAX_LEN];
    uint16_t mSaltLen;
    uint8_t  mPskc[PSKC_LENGTH];
};

} //namespace Psk
} //namespace ot

#endif  //PSKC_HPP
