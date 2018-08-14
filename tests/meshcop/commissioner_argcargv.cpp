/*
 *    Copyright (c) 2017-2018, The OpenThread Authors.
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
 *   The file implements the command line params for the commissioner test app.
 */

#include "commissioner_argcargv.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include "common/logging.hpp"
#include "utils/hex.hpp"

namespace ot {
namespace BorderRouter {

/** see: commissioner_ArgcArgv.hpp, processes 1 command line parameter */
int ArgcArgv::ParseArgs(void)
{
    const char *              arg;
    const struct ArgcArgvOpt *opt;

    /* Done? */
    if (mARGx >= mARGC)
    {
        return -1;
    }

    arg = mARGV[mARGx];
    /* consume this one */
    mARGx += 1;

    /* automatically support forms of "-help" */
    if ((0 == strcmp("-h", arg)) || (0 == strcmp("-?", arg)) || (0 == strcmp("-help", arg)) ||
        (0 == strcmp("--help", arg)))
    {
        usage("Help...\n");
    }

    for (opt = &mOpts[0]; opt->name; opt++)
    {
        if (0 == strcmp(arg, opt->name))
        {
            /* found */
            break;
        }
    }

    if (opt->name == NULL)
    {
        /* not found */
        usage("Unknown option: %s", arg);
    }

    (*(opt->handler))(this, &args);
    return 0;
}

/** see: commissioner_ArgcArgv.hpp, fetch an --ARG STRING pair, storing the result */
const char *ArgcArgv::StrParam(char *puthere, size_t bufsiz)
{
    const char *cp;
    size_t      len;

    mARGx += 1; /* skip parameter name */
    if (mARGx > mARGC)
    {
        usage("Missing: %s VALUE\n", mARGV[mARGx - 2]);
    }

    cp = mARGV[mARGx - 1];

    /* check length */
    len = strlen(cp);
    len++; /* room for null */
    if (len > bufsiz)
    {
        usage("Too long: %s %s\n", mARGV[mARGx - 2], cp);
    }

    /* save if requested */
    if (puthere)
    {
        strcpy(puthere, cp);
    }
    return cp;
}

/** see: commissioner_ArgcArgv.hpp, fetch an --ARG HEXSTRING pair, and decode the hex string storing the result */
void ArgcArgv::HexParam(char *ascii_puthere, uint8_t *bin_puthere, int bin_len)
{
    int n;

    StrParam(ascii_puthere, (bin_len * 2) + 1);

    n = Utils::Hex2Bytes(ascii_puthere, bin_puthere, bin_len);

    /* hex strings must be *complete* */
    if (n != bin_len)
    {
        usage("Param: %s, invalid hex value %s\n", mARGV[mARGx - 2], mARGV[mARGx - 1]);
    }
}

/** see: commissioner_ArgcArgv.hpp, fetch an --ARG NUMBER pair, returns the numeric value */
int ArgcArgv::NumParam(void)
{
    const char *s;
    char *      ep;
    int         v;

    /* fetch as string */
    s = StrParam(NULL, 100);

    /* then convert */
    v = strtol(s, &ep, 0);
    if ((ep == s) || (*ep != 0))
    {
        usage("Not a number: %s %s\n", mARGV[mARGx - 2], mARGV[mARGx - 1]);
    }
    return v;
}

/** see: commissioner_ArgcArgv.hpp, constructor for the commissioner argc/argv parser */
ArgcArgv::ArgcArgv(int argc, char **argv)
{
    mARGC = argc;
    mARGV = argv;
    mARGx = 1; /* skip the app name */

    memset(&(mOpts[0]), 0, sizeof(mOpts));
}

/** see: commissioner_ArgcArgv.hpp, add an option to be decoded and its handler */
void ArgcArgv::AddOption(const char *name,
                         void (*handler)(ArgcArgv *pThis, CommissionerArgs *args),
                         const char *valuehelp,
                         const char *helptext)
{
    struct ArgcArgvOpt *opt;
    int                 x;

    for (x = 0; x < max_opts; x++)
    {
        opt = &mOpts[x];
        if (opt->name == NULL)
        {
            break;
        }
    }
    if (x >= max_opts)
    {
        otbrLog(OTBR_LOG_ERR, "internal error: Too many cmdline opts!\n");
    }

    opt->name     = name;
    opt->handler  = handler;
    opt->helptext = helptext;
    if (valuehelp == NULL)
    {
        valuehelp = "";
    }
    opt->valuehelp = valuehelp;
}

/** see: commissioner_ArgcArgv.hpp, print error message & application usage */
void ArgcArgv::usage(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s OPTIONS....\n", mARGV[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "Where OPTIONS are:\n");
    fprintf(stderr, "\n");

    struct ArgcArgvOpt *opt;

    for (opt = &mOpts[0]; opt->name; opt++)
    {
        int n;

        n = fprintf(stderr, "    %s %s", opt->name, opt->valuehelp);
        fprintf(stderr, "%*s%s\n", (30 - n), "", opt->helptext);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Note the order of options is important\n");
    fprintf(stderr, "Example, the option --compute-pskc, has prerequistes of\n");
    fprintf(stderr, "   --network-name NAME\n");
    fprintf(stderr, "   --xpanid VALUE\n");
    fprintf(stderr, "   --agent-passphrase VALUE\n");
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

/** Handle commissioner steering data length command line parameter */
static void handle_steering_length(ArgcArgv *pThis, CommissionerArgs *args)
{
    int v;

    v = pThis->NumParam();
    if ((v < 1) || (v > 16))
    {
        pThis->usage("invalid steering length: %d", v);
    }

    args->mSteeringLength = v;
}

/** Handle border router ip address on command line */
static void handle_ip_addr(ArgcArgv *pThis, CommissionerArgs *args)
{
    pThis->StrParam(args->mAgentAddress_ascii, sizeof(args->mXpanidAscii));
}

/** Handle border router ip port on command line */
static void handle_ip_port(ArgcArgv *pThis, CommissionerArgs *args)
{
    pThis->StrParam(args->mAgentPort_ascii, sizeof(args->mAgentPort_ascii));
}

/** Handle hex encoded HASHMAC on command line */
static void handle_hashmac(ArgcArgv *pThis, CommissionerArgs *args)
{
    pThis->HexParam(args->mJoinerHashmacAscii, args->mJoinerHashmacBin, sizeof(args->mJoinerHashmacBin));
}

/** Handle joining device EUI64 on the command line */
static void handle_eui64(ArgcArgv *pThis, CommissionerArgs *args)
{
    pThis->HexParam(args->mJoinerEui64Ascii, args->mJoinerEui64Bin, sizeof(args->mJoinerEui64Bin));
}

/** Handle the preshared joining credential for the joining device on the command line */
static void handle_pskd(ArgcArgv *pThis, CommissionerArgs *args)
{
    const char *whybad;
    int         ch;
    int         len, x;
    char        pskdAscii[kPSKdLength + 1];

    /* assume not bad */
    whybad = NULL;

    /* get the parameter */
    pThis->StrParam(pskdAscii, sizeof(pskdAscii));

    /*
     * Problem: Should we "base32" decode this per the specification?
     * Answer: No - because this needs to be identical to the CLI application
     * The CLI appication does *NOT* decode preshared key
     * thus we do not decode the base32 value here
     * We do however enforce the data..
     */

    /*
     * Joining Device Credential
     * Specification 1.1.1, Section 8.2 Table 8-1
     * Min Length 6, Max Length 32.
     *
     * Digits 0-9, Upper case only Letters A-Z
     * excluding: I,O,Q,Z
     *
     * Note: 26 letters - 4 illegals = 22 letters.
     * Thus 10 digits + 22 letters = 32 symbols.
     * Thus, "base32" encoding using the above.
     */
    len = strlen(pskdAscii);
    if ((len < 6) || (len > 32))
    {
        whybad = "invalid length (range: 6..32)";
    }
    else
    {
        for (x = 0; x < len; x++)
        {
            ch = pskdAscii[x];

            switch (ch)
            {
            case 'Z':
            case 'I':
            case 'O':
            case 'Q':
                whybad = "Letters I, O, Q and Z are not allowed";
                break;
            default:
                if (isupper(ch) || isdigit(ch))
                {
                    /* all is well */
                }
                else
                {
                    whybad = "contains non-uppercase or non-digit";
                }
                break;
            }
            if (whybad)
            {
                break;
            }
        }
    }

    if (whybad)
    {
        pThis->usage("Illegal PSKd: \"%s\", %s\n", pskdAscii, whybad);
    }
    else
    {
        memcpy(args->mJoinerPSKdAscii, pskdAscii, sizeof(pskdAscii));
    }
}

/** Handle a pre-computed border agent preshared key, the PSKc
 * This is derived from the Networkname, Xpanid & passphrase
 */
static void handle_pskc_bin(ArgcArgv *pThis, CommissionerArgs *args)
{
    pThis->HexParam(args->mPSKcAscii, args->mPSKcBin, sizeof(args->mPSKcBin));
    args->mHasPSKc = true;
    // otbrLog(OTBR_LOG_INFO, "PSKC on command line is: %s\n", gContext.mAgent.mPSKc.ascii);
}

/** handle the xpanid command line parameter */
static void handle_xpanid(ArgcArgv *pThis, CommissionerArgs *args)
{
    pThis->HexParam(args->mXpanidAscii, args->mXpanidBin, sizeof(args->mXpanidBin));
}

/* handle the networkname command line parameter */
static void handle_netname(ArgcArgv *pThis, CommissionerArgs *args)
{
    pThis->StrParam(args->mNetworkName, sizeof(args->mNetworkName));
}

/** handle the border router pass phrase command line parameter */
static void handle_agent_passphrase(ArgcArgv *pThis, CommissionerArgs *args)
{
    pThis->StrParam(args->mPassPhrase, sizeof(args->mPassPhrase));
}

/** Handle log fileanme on the command line */
static void handle_log_filename(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)args;
    char filename[PATH_MAX];

    pThis->StrParam(filename, sizeof(filename));

    otbrLogSetFilename(filename);
}

/** compute the pskc from command line params */
static void handle_compute_pskc(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)pThis;
    args->mNeedComputePSKc = true;
    /* we print this in a way scripts can easily parse */
    // fprintf(stdout, "PSKc: %s\n", gContext.mAgent.mPSKc.ascii);
    // exit(EXIT_SUCCESS);
}

/** commandline handling for flag that says we commission (not test) */
static void handle_commission_device(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)pThis;
    args->mNeedCommissionDevice = true;
}

/** compute hashmac of EUI64 on command line */
static void handle_compute_hashmac(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)pThis;
    args->mNeedComputeJoinerHashMac = true;

    /* print so scripts can easily parse */
    // fprintf(stdout, "eiu64: %s\n", gContext.mJoiner.mEui64.ascii);
    // fprintf(stdout, "hashmac: %s\n", gContext.mJoiner.mHashMac.ascii);
    // exit(EXIT_SUCCESS);
}

/** compute steering based on command line */
static void handle_compute_steering(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)pThis;
    args->mNeedComputeJoinerSteering = true;

    // CommissionerComputeSteering();

    ///* print so scripts can easily parse */
    // fprintf(stdout, "eiu64: %s\n", gContext.mJoiner.mEui64.ascii);
    // fprintf(stdout, "hashmac: %s\n", gContext.mJoiner.mHashMac.ascii);
    // fprintf(stdout, "steering-len: %d\n", gContext.mJoiner.mSteeringData.GetLength());
    // fprintf(stdout, "steering-hex: %s\n",
    //        CommissionerUtilsHexString(gContext.mJoiner.mSteeringData.GetDataPointer(),
    //                                   gContext.mJoiner.mSteeringData.GetLength()));

    // exit(EXIT_SUCCESS);
}

/** handle debug level on command line */
static void handle_debug_level(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)args;
    int n;

    n = pThis->NumParam();
    if (n < OTBR_LOG_EMERG)
    {
        pThis->usage("invalid log level, must be >= %d\n", OTBR_LOG_EMERG);
    }
    if (n > OTBR_LOG_DEBUG)
    {
        n = OTBR_LOG_DEBUG;
    }
    otbrLogSetLevel(n);
}

/** handle steering allow any on command line */
static void handle_allow_all_joiners(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)(pThis);
    args->mAllowAllJoiners = true;
}

/** user wants to disable COMM_KA transmissions for test purposes */
static void handle_comm_ka_disabled(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)(pThis);
    args->mNeedSendCommKA = false;
}

/** user wants to adjust COMM_KA transmission rate */
static void handle_comm_ka_rate(ArgcArgv *pThis, CommissionerArgs *args)
{
    int n;

    n = pThis->NumParam();
    /* sanity... */

    /* note: 86400 = 1 day in seconds */
    if ((n < 3) || (n > 86400))
    {
        pThis->usage("comm-ka rate must be (n>3) && (n < 86400), not: %d\n", n);
    }
    args->mSendCommKATxRate = n;
}

/** Adjust total envelope timeout for test automation reasons */
static void handle_comm_envelope_timeout(ArgcArgv *pThis, CommissionerArgs *args)
{
    int n;

    /*
     * This exists because if the COMM_KA keeps going
     * nothing will stop... and test scripts run forever!
     */

    n = pThis->NumParam();
    /* between 1 second and 1 day.. */
    if ((n < 1) || (n > (86400)))
    {
        pThis->usage("Invalid envelope time, range: 1 <= n <= 86400, not %d\n", n);
    }
    args->mEnvelopeTimeout = n;
}

/* handle disabling syslog on command line */
static void handle_no_syslog(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)args;
    (void)pThis;
    otbrLogEnableSyslog(false);
}

/** Called by main(), to process commissioner command line arguments */
CommissionerArgs ParseArgs(int aArgc, char **aArgv)
{
    ArgcArgv args(aArgc, aArgv);

    // default value
    args.args.mEnvelopeTimeout           = 5 * 60; // 5 minutes;
    args.args.mAllowAllJoiners           = false;
    args.args.mNeedSendCommKA            = true;
    args.args.mSendCommKATxRate          = 15; // send keep alive every 15 seconds;
    args.args.mSteeringLength            = kSteeringDefaultLength;
    args.args.mNeedComputePSKc           = false;
    args.args.mNeedComputeJoinerSteering = false;
    args.args.mNeedComputeJoinerHashMac  = false;
    args.args.mNeedCommissionDevice      = true;
    args.args.mHasPSKc                   = false;

    // args.add_option("--selftest", CommissionerCmdLineSelfTest, "", "perform internal selftests");
    args.AddOption("--joiner-eui64", handle_eui64, "VALUE", "joiner EUI64 value");
    args.AddOption("--hashmac", handle_hashmac, "VALUE", "joiner HASHMAC value");
    args.AddOption("--agent-passphrase", handle_agent_passphrase, "VALUE", "Pass phrase for agent");
    args.AddOption("--network-name", handle_netname, "VALUE", "UTF8 encoded network name");
    args.AddOption("--xpanid", handle_xpanid, "VALUE", "xpanid in hex");
    args.AddOption("--pskc-bin", handle_pskc_bin, "VALUE", "Precomputed PSKc in hex notation");
    args.AddOption("--joiner-passphrase", handle_pskd, "VALUE", "PSKd for joiner");
    args.AddOption("--steering-length", handle_steering_length, "NUMBER", "Length of steering data 1..15");
    args.AddOption("--allow-all-joiners", handle_allow_all_joiners, "", "Allow any device to join");
    args.AddOption("--agent-addr", handle_ip_addr, "VALUE", "ip address of border router agent");
    args.AddOption("--agent-port", handle_ip_port, "VALUE", "ip port used by border router agent");
    args.AddOption("--log-filename", handle_log_filename, "FILENAME", "set logfilename");
    args.AddOption("--compute-pskc", handle_compute_pskc, "", "compute and print the pskc from parameters");
    args.AddOption("--compute-hashmac", handle_compute_hashmac, "", "compute and print the hashmac of the given eui64");
    args.AddOption("--compute-steering", handle_compute_steering, "", "compute and print steering data");
    args.AddOption("--comm-ka-disabled", handle_comm_ka_disabled, "", "Disable COMM_KA transmissions");
    args.AddOption("--comm-ka-rate", handle_comm_ka_rate, "", "Set COMM_KA transmission rate");
    args.AddOption("--disable-syslog", handle_no_syslog, "", "Disable log via syslog");
    args.AddOption("--comm-envelope-timeout", handle_comm_envelope_timeout, "VALUE",
                   "Set the total envelope timeout for commissioning");

    args.AddOption("--commission-device", handle_commission_device, "", "Enable device commissioning");

    args.AddOption("--debug-level", handle_debug_level, "NUMBER", "Enable debug output at level VALUE (higher=more)");
    if (aArgc == 1)
    {
        args.usage("No parameters!\n");
    }
    /* parse the args */
    while (args.ParseArgs() != -1)
        ;
    return args.args;
}

} // namespace BorderRouter
} // namespace ot
