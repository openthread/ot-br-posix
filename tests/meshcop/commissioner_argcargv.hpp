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
 *   The file is the header for the command line params for the commissioner test app.
 */

#ifndef OTBR_COMMISSIONER_HPP_H_
#define OTBR_COMMISSIONER_HPP_H_

#include "commission_common.hpp"
#include <cstring>
#include <stdint.h>
#include "web/pskc-generator/pskc.hpp"

namespace ot {
namespace BorderRouter {

/**
 * Simple class to handle command line parameters
 * To avoid the use of "getopt_long()" we have this.
 */

/* forward */
class argcargv;

struct CommissionerArgs
{
    char mAgentPort_ascii[7];
    char mAgentAddress_ascii[64];

    char    mJoinerHashmac_ascii[kEui64Len * 2 + 1];
    uint8_t mJoinerHashmac_bin[kEui64Len];
    bool    mHasJoinerHashMac;
    char    mJoinerEui64_ascii[kEui64Len * 2 + 1];
    uint8_t mJoinerEui64_bin[kEui64Len];
    bool    mAllowAllJoiners;

    char mJoinerPSKd_ascii[kPSKdLength + 1];
    int  mSteeringLength;

    bool mNeedSendCommKA;
    int  mSendCommKATxRate;
    int  mEnvelopeTimeout;

    char    mPSKc_ascii[2 * OT_PSKC_LENGTH + 1];
    uint8_t mPSKc_bin[OT_PSKC_LENGTH];
    bool    mHasPSKc;
    char    mXpanid_ascii[kXpanidLength * 2 + 1];
    uint8_t mXpanid_bin[kXpanidLength];
    char    mNetworkName[kNetworkNameLenMax + 1];
    char    mPassPhrase[kBorderRouterPassPhraseLen + 1];

    bool mNeedComputePSKc;
    bool mNeedComputeJoinerHashMac;
    bool mNeedComputeJoinerSteering;
    bool mNeedCommissionDevice;

    int mDebugLevel;
};

/* option entry in our table */
struct argcargv_opt
{
    const char *name;
    void (*handler)(argcargv *, CommissionerArgs *);
    const char *valuehelp;
    const char *helptext;
};

class argcargv
{
public:
    /** Constructor */
    argcargv(int argc, char **argv);

    /** pseudo globals for argc & argv parsing */
    int    mARGC; /* analogous to argc */
    char **mARGV; /* analogous to argv */
    int    mARGx; /**< current argument */

    /** Result **/
    CommissionerArgs args;

    enum
    {
        max_opts = 40
    };
    struct argcargv_opt mOpts[max_opts];

    /** print usage error message and exit */
    void usage(const char *fmt, ...);

    /** add an option to be parsed */
    void add_option(const char *name,
                    void (*handler)(argcargv *pThis, CommissionerArgs *args),
                    const char *valuehelp,
                    const char *help);

    /**
     * fetch/parse a string parameter
     *
     * @param puthere[out] holds parameter string
     * @param maxlen[in]   size of the puthere buffer, including space for null
     */
    const char *str_param(char *puthere, size_t maxlen);

    /**
     * Parse a hex encoded string from the command line.
     * @param ascii_puthere[out]  the ascii encoded data will be placed here
     * @param bin_puthere[out]    the resulting/decoded binary data will be here.
     * @param sizeof_bin[in]      the size of the binary buffer
     *
     * The sizeof_bin also specifies the required size of the hex
     * encoded data on the command line, for example if size_bin==4
     *
     * Then there must be exactly 8 hex digits in the command line parameter
     */
    void hex_param(char *ascii_puthere, uint8_t *bin_puthere, int sizeof_bin);

    /**
     * Parse a numeric parameter from the command line
     *
     * If something is wrong print an error & usage text.
     *
     * @returns value from command line as an integer
     */
    int num_param(void);

    /**
     *  This parses a single command line parameter
     *
     * @returns 0 if there are more parameters to parse
     * @returns -1 if there are no more parameters to parse
     *
     * This does not handle positional parameters.
     */
    int parse_args(void);
};

/**
 * Called from main() to parse the commissioner test app command line parameters.
 */
CommissionerArgs commissioner_argcargv(int argc, char **argv);

} // namespace BorderRouter
} // namespace ot

#endif
