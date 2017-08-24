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
 *   This is a common header for the various commissioner source files.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <sys/time.h>

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#include <mbedtls/net_sockets.h>
#include <mbedtls/debug.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecjpake.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>
#include <mbedtls/timing.h>

#include "agent/coap.hpp"
#include "agent/dtls.hpp"
#include "agent/uris.hpp"
#include "common/tlv.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "utils/hex.hpp"
#include "utils/steeringdata.hpp"
#include "web/pskc-generator/pskc.hpp"


using namespace ot;
using namespace ot::Utils;
using namespace ot::BorderRouter;


#define MBEDTLS_DEBUG_C
#define DEBUG_LEVEL 1

#define SYSLOG_IDENT "otbr-commissioner"

#include "commissioner_argcargv.hpp"

/**
 * Constants
 */
enum
{
    /* max size of a network packet */
    kSizeMaxPacket = 1500,

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



/**
 * Commissioner State
 */
enum
{
    kStateInvalid,
    kStateConnected,
    kStateAccepted,
    kStateReady,
    kStateAuthenticated,
    kStateFinalized,
    kStateDone,
    kStateError,
};


/**
 * Commissioner TLV types
 */
enum
{
    kJoinerDtlsEncapsulation
};


/**
 * Commissioner Context.
 */
struct Context
{
    /** Set to true if we should commission the device */
    bool commission_device;

    /** coap instance to talk to agent & device */
    Coap::Agent *mCoap;

    /** dtls info for talking to agent & joiner */
    Dtls::Server *mDtlsServer;

    /* the session info for agent & joiner */
    Dtls::Session *mSession;

    /* dtls context information */
    mbedtls_ssl_context *mSsl;
    mbedtls_net_context *mNet;

    /* Socket we are using with the agent */
    int mSocket;

    /* this generates our coap tokens */
    uint16_t mCoapToken;

    /* this is the commissioner session id */
    uint16_t mCommissionerSessionId;

    /** all things for the border agent */
    struct br_agent
    {

        /** network port, in form mbed likes it (ascii) */
        char mPort_ascii[ 7 ];

        /** network address, in form mbed likes it (ascii) */
        char mAddress_ascii[64];

        /** xpanid from command line & binary form */
        /* Note: xpanid is used to calculate the PSKc */
        struct
        {
            char    ascii[  (kXpanidLength * 2) + 1 ];
            uint8_t bin  [   kXpanidLength ];
        } mXpanid;

        /** network name from command line */
        /* Note: networkname is used to calculate the PSKc */
        char mNetworkName[ kNetworkNameLenMax + 1 ];

        /** border router agent pass phrase */
        /* Note: passphrase is used to calculate the PSKc */
        char mPassPhrase[ kBorderRouterPassPhraseLen + 1 ];


        /** the PSKc used with the agent */
        struct
        {

            /* this class does the calculation */
            Psk::Pskc mTool;

            /** ascii & binary of PSKc, either from calculation or cmdline */
            char    ascii[ (OT_PSKC_LENGTH * 2) + 1 ];
            uint8_t bin[ OT_PSKC_LENGTH ];
        } mPSKc;

    } mAgent;

    /** Kek with the joiner operation */
    uint8_t mKek[32];

    struct comm_ka
    {
        /** last time a COMM_KA message was sent */
        struct timeval mLastTxTv;

        /** last time a COMM_KA.rsp was received */
        struct timeval mLastRxTv;

        /** how often should these be sent in seconds */
        int mTxRate;

        /** How many have been sent & recieved */
        int mTxCnt;
        int mRxCnt;

        /** are these disabled? (for test purposes) */
        bool mDisabled;
    } mCOMM_KA;

    /** Total timeout (envelope) of the commissioning test */
    struct               timeval mEnvelopeStartTv;
    int                          mEnvelopeTimeout;

    /** Commissioner state */
    int mState;


    /** All things about the joiner */
    struct joiner
    {

        /** the EUI64 of the Joiner or HASHMAC, either from cmdline or computed */
        struct
        {
            char    ascii[ (kEui64Len * 2) + 1 ];
            uint8_t bin[ kEui64Len ];
        } mEui64, mHashMac;

        /** Set to true/false via cmdline param for test purposes. */
        bool mAllowAny;

        /** Computed steering data based on hashmac */
        SteeringData mSteeringData;

        /** UDP port of the joiner */
        uint16_t mUdpPort;

        /** Interface id of the joiner device */
        uint8_t mIid[8];

        /** the router we are using to talk to the joiner */
        uint16_t mRouterLocator;

        /** the port we are using */
        uint16_t mPortSession;

        /** This is the PSKd from the command line */
        /* this is the shared string used by the device */
        char mPSKd_ascii[ kPSKdLength + 1 ];

        /*
         * NOTE: The simplist barcode would contain:
         *
         *    v=1&&eui=_EUI64_ASCII_&&cc=PSKdASCIITEXT
         *
         * Example:
         *
         *    v=1&&eui=00124b000f6e649d&&cc=MYPASSPHRASE
         *
         */
    } mJoiner;
};

/* the single global commissioning context */
extern struct Context gContext;

/* compute the hashmac of a joiner */
bool CommissionerComputeHashMac(void);

/** Compute steering data */
bool CommissionerComputeSteering(void);

/** compute pskc */
bool CommissionerComputePskc(void);

/* return a small string with this data as hex for logging purposes */
const char *CommissionerUtilsHexString(const uint8_t *pBytes, int n);

/** command line self test handler */
void CommissionerCmdLineSelfTest(argcargv *pThis);

/** Print/log an error message and exit */
void CommissionerUtilsFail(const char *fmt, ...);
