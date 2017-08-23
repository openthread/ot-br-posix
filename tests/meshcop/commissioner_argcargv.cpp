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
 *   The file implements the command line params for the commissioner test app.
 */

#include "commissioner.hpp"

using namespace ot;
using namespace ot::Utils;
using namespace ot::BorderRouter;

/** see: commissioner_argcargv.hpp, processes 1 command line parameter */
int argcargv::parse_args(void)
{
    const char                *arg;
    const struct argcargv_opt *opt;

    /* Done? */
    if (mARGx >= mARGC)
    {
        return -1;
    }

    arg = mARGV[mARGx];
    /* consume this one */
    mARGx += 1;

    /* automatically support forms of "-help" */
    if ((0 == strcmp("-h", arg)) ||
        (0 == strcmp("-?", arg)) ||
        (0 == strcmp("-help", arg)) ||
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

    (*(opt->handler))(this);
    return 0;
}


/** see: commissioner_argcargv.hpp, fetch an --ARG STRING pair, storing the result */
const char *argcargv::str_param(char *puthere, size_t bufsiz)
{
    const char *cp;
    size_t      len;

    mARGx += 1; /* skip parameter name */
    if (mARGx > mARGC)
    {
        usage("Missing: %s VALUE\n", mARGV[mARGx - 2]);
    }

    cp = mARGV[ mARGx - 1];

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


/** see: commissioner_argcargv.hpp, fetch an --ARG HEXSTRING pair, and decode the hex string storing the result */
void argcargv::hex_param(char *ascii_puthere, uint8_t *bin_puthere, int bin_len)
{
    int n;

    str_param(ascii_puthere, (bin_len * 2) + 1);

    n = Hex2Bytes(ascii_puthere, bin_puthere, bin_len);

    /* hex strings must be *complete* */
    if (n != bin_len)
    {
        usage("Param: %s, invalid hex value %s\n",
              mARGV[ mARGx - 2 ], mARGV[ mARGx - 1]);
    }
}

/** see: commissioner_argcargv.hpp, fetch an --ARG NUMBER pair, returns the numeric value */
int argcargv::num_param(void)
{
    const char *s;
    char       *ep;
    int         v;

    /* fetch as string */
    s = str_param(NULL, 100);

    /* then convert */
    v = strtol(s, &ep, 0);
    if ((ep == s) || (*ep != 0))
    {
        usage("Not a number: %s %s\n", mARGV[mARGx - 2], mARGV[mARGx - 1]);
    }
    return v;
}


/** see: commissioner_argcargv.hpp, constructor for the commissioner argc/argv parser */
argcargv::argcargv(int argc, char **argv)
{
    mARGC = argc;
    mARGV = argv;
    mARGx = 1; /* skip the app name */

    memset(&(mOpts[0]), 0, sizeof(mOpts));
}


/** see: commissioner_argcargv.hpp, add an option to be decoded and its handler */
void argcargv::add_option(const char *name,
                          void (*handler)(argcargv *pThis),
                          const char *valuehelp,
                          const char *helptext)
{
    struct argcargv_opt *opt;
    int                  x;

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
        CommissionerUtilsFail("internal error: Too many cmdline opts!\n");
    }

    opt->name = name;
    opt->handler = handler;
    opt->helptext = helptext;
    if (valuehelp == NULL)
    {
        valuehelp = "";
    }
    opt->valuehelp = valuehelp;
}

/** see: commissioner_argcargv.hpp, print error message & application usage */
void argcargv::usage(const char *fmt, ...)
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

    struct argcargv_opt *opt;

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
static void handle_steering_length(argcargv *pThis)
{
    int v;

    v = pThis->num_param();
    if ((v < 1) || (v > 16))
    {
        pThis->usage("invalid steering length: %d", v);
    }

    gContext.mJoiner.mSteeringData.SetLength(v);
}

/** Handle border router ip address on command line */
static void handle_ip_addr(argcargv *pThis)
{
    pThis->str_param(gContext.mAgent.mAddress_ascii, sizeof(gContext.mAgent.mAddress_ascii));
}


/** Handle border router ip port on command line */
static void handle_ip_port(argcargv *pThis)
{
    pThis->str_param(gContext.mAgent.mPort_ascii, sizeof(gContext.mAgent.mPort_ascii));
}

/** Handle hex encoded HASHMAC on command line */
static void handle_hashmac(argcargv *pThis)
{
    bool ok;

    pThis->hex_param(gContext.mJoiner.mHashMac.ascii,
                     gContext.mJoiner.mHashMac.bin,
                     sizeof(gContext.mJoiner.mHashMac.bin));

    /* once hash mac is know, we can compute the steering data
     * We assume we have the xpanid & network name.
     */
    ok = CommissionerComputeSteering();
    if (!ok)
    {
        pThis->usage("Invalid HASHMAC: %s\n", gContext.mJoiner.mHashMac.ascii);
    }

}


/** Handle joining device EUI64 on the command line */
static void handle_eui64(argcargv *pThis)
{
    bool ok;

    pThis->hex_param(gContext.mJoiner.mEui64.ascii,
                     gContext.mJoiner.mEui64.bin,
                     sizeof(gContext.mJoiner.mEui64.bin));

    /* once we have this, we can calculate the HASHMAC
     * and the steering data.
     */
    ok = CommissionerComputeHashMac();
    ok = ok && CommissionerComputeSteering();
    if (!ok)
    {
        pThis->usage("Invalid EUI64: %s\n", gContext.mJoiner.mEui64.ascii);
    }
}

/** Handle the preshared joining credential for the joining device on the command line */
static void handle_pskd(argcargv *pThis)
{
    const char *whybad;
    int         ch;
    int         len, x;

    /* assume not bad */
    whybad = NULL;

    /* get the parameter */
    pThis->str_param(gContext.mJoiner.mPSKd_ascii, sizeof(gContext.mJoiner.mPSKd_ascii));

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
    len = strlen(gContext.mJoiner.mPSKd_ascii);
    if ((len < 6) || (len > 32))
    {
        whybad = "invalid length (range: 6..32)";
    }
    else
    {

        for (x = 0; x < len; x++)
        {
            ch = gContext.mJoiner.mPSKd_ascii[x];

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
        pThis->usage("Illegal PSKd: \"%s\", %s\n",
                     gContext.mJoiner.mPSKd_ascii, whybad);
    }
}


/** Handle a pre-computed border agent preshared key, the PSKc
 * This is derived from the Networkname, Xpanid & passphrase
 */
static void handle_pskc_bin(argcargv *pThis)
{
    pThis->hex_param(gContext.mAgent.mPSKc.ascii,
                     gContext.mAgent.mPSKc.bin,
                     sizeof(gContext.mAgent.mPSKc.bin));
    otbrLog(OTBR_LOG_INFO, "PSKC on command line is: %s\n", gContext.mAgent.mPSKc.ascii);
}



/** handle the xpanid command line parameter */
static void handle_xpanid(argcargv *pThis)
{
    pThis->hex_param(gContext.mAgent.mXpanid.ascii,
                     gContext.mAgent.mXpanid.bin,
                     sizeof(gContext.mAgent.mXpanid.bin));
}


/* handle the networkname command line parameter */
static void handle_netname(argcargv *pThis)
{
    pThis->str_param(gContext.mAgent.mNetworkName, sizeof(gContext.mAgent.mNetworkName));
}

/** handle the border router pass phrase command line parameter */
static void handle_agent_passphrase(argcargv *pThis)
{
    pThis->str_param(gContext.mAgent.mPassPhrase, sizeof(gContext.mAgent.mPassPhrase));
}

/** Handle log fileanme on the command line */
static void handle_log_filename(argcargv *pThis)
{
    char filename[ PATH_MAX ];

    pThis->str_param(filename, sizeof(filename));

    otbrLogSetFilename(filename);
}

/** compute the pskc from command line params */
static void handle_compute_pskc(argcargv *pThis)
{
    (void)(pThis);
    CommissionerComputePskc();
    /* we print this in a way scripts can easily parse */
    fprintf(stdout, "PSKc: %s\n", gContext.mAgent.mPSKc.ascii);
    exit(EXIT_SUCCESS);
}

/** commandline handling for flag that says we commission (not test) */
static void handle_commission_device(argcargv *pThis)
{
    (void)(pThis);
    gContext.commission_device = true;
}


/** compute hashmac of EUI64 on command line */
static void handle_compute_hashmac(argcargv *pThis)
{
    (void)pThis;

    CommissionerComputeHashMac();
    /* print so scripts can easily parse */
    fprintf(stdout, "eiu64: %s\n", gContext.mJoiner.mEui64.ascii);
    fprintf(stdout, "hashmac: %s\n", gContext.mJoiner.mHashMac.ascii);
    exit(EXIT_SUCCESS);
}


/** compute steering based on command line */
static void handle_compute_steering(argcargv *pThis)
{
    (void)pThis;

    CommissionerComputeSteering();

    /* print so scripts can easily parse */
    fprintf(stdout, "eiu64: %s\n", gContext.mJoiner.mEui64.ascii);
    fprintf(stdout, "hashmac: %s\n", gContext.mJoiner.mHashMac.ascii);
    fprintf(stdout, "steering-len: %d\n", gContext.mJoiner.mSteeringData.GetLength());
    fprintf(stdout, "steering-hex: %s\n",
            CommissionerUtilsHexString(
                gContext.mJoiner.mSteeringData.GetDataPointer(),
                gContext.mJoiner.mSteeringData.GetLength()));

    exit(EXIT_SUCCESS);
}


/** handle debug level on command line */
static void handle_debug_level(argcargv *pThis)
{
    int n;

    n = pThis->num_param();
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
static void handle_allow_all_joiners(argcargv *pThis)
{
    (void)(pThis);
    bool ok;

    gContext.mJoiner.mAllowAny = true;
    /* once we have this, we can calculate the HASHMAC
     * and the steering data.
     */
    ok = CommissionerComputeSteering();
    if (!ok)
    {
        CommissionerUtilsFail("Cannot compute steering\n");
    }
}

/** user wants to disable COMM_KA transmissions for test purposes */
static void handle_comm_ka_disabled(argcargv *pThis)
{
    (void)(pThis);
    gContext.mCOMM_KA.mDisabled = true;
}

/** user wants to adjust COMM_KA transmission rate */
static void handle_comm_ka_rate(argcargv *pThis)
{
    int n;

    n = pThis->num_param();
    /* sanity... */

    /* note: 86400 = 1 day in seconds */
    if ((n < 3) || (n > 86400))
    {
        pThis->usage("comm-ka rate must be (n>3) && (n < 86400), not: %d\n", n);
    }
    gContext.mCOMM_KA.mTxRate = n;
}

/** Adjust total envelope timeout for test automation reasons */
static void handle_comm_envelope_timeout(argcargv *pThis)
{
    int n;

    /*
     * This exists because if the COMM_KA keeps going
     * nothing will stop... and test scripts run forever!
     */

    n = pThis->num_param();
    /* between 1 second and 1 day.. */
    if ((n < 1) || (n > (86400)))
    {
        pThis->usage("Invalid envelope time, range: 1 <= n <= 86400, not %d\n", n);
    }
    gContext.mEnvelopeTimeout = n;
}

/* handle disabling syslog on command line */
static void handle_no_syslog(argcargv *pThis)
{
    (void)(pThis);
    otbrLogEnableSyslog(false);
}


/** Called by main(), to process commissioner command line arguments */
void commissioner_argcargv(int argc, char **argv)
{
    argcargv args(argc, argv);

    args.add_option("--selftest",                CommissionerCmdLineSelfTest,    "",
                    "perform internal selftests");
    args.add_option("--joiner-eui64",            handle_eui64,                   "VALUE",       "joiner EUI64 value");
    args.add_option("--hashmac",                 handle_hashmac,                 "VALUE",       "joiner HASHMAC value");
    args.add_option("--agent-passphrase",        handle_agent_passphrase,        "VALUE",
                    "Pass phrase for agent");
    args.add_option("--network-name",            handle_netname,                 "VALUE",
                    "UTF8 encoded network name");
    args.add_option("--xpanid",                  handle_xpanid,                  "VALUE",       "xpanid in hex");
    args.add_option("--pskc-bin",                handle_pskc_bin,                "VALUE",
                    "Precomputed PSKc in hex notation");
    args.add_option("--joiner-passphrase",       handle_pskd,                    "VALUE",       "PSKd for joiner");
    args.add_option("--steering-length",         handle_steering_length,         "NUMBER",
                    "Length of steering data 1..15");
    args.add_option("--allow-all-joiners",       handle_allow_all_joiners,       "",
                    "Allow any device to join");
    args.add_option("--agent-addr",              handle_ip_addr,                 "VALUE",
                    "ip address of border router agent");
    args.add_option("--agent-port",              handle_ip_port,                 "VALUE",
                    "ip port used by border router agent");
    args.add_option("--log-filename",            handle_log_filename,            "FILENAME",    "set logfilename");
    args.add_option("--compute-pskc",            handle_compute_pskc,            "",
                    "compute and print the pskc from parameters");
    args.add_option("--compute-hashmac",         handle_compute_hashmac,         "",
                    "compute and print the hashmac of the given eui64");
    args.add_option("--compute-steering",        handle_compute_steering,        "",
                    "compute and print steering data");
    args.add_option("--comm-ka-disabled",        handle_comm_ka_disabled,        "",
                    "Disable COMM_KA transmissions");
    args.add_option("--comm-ka-rate",            handle_comm_ka_rate,            "",
                    "Set COMM_KA transmission rate");
    args.add_option("--disable-syslog",          handle_no_syslog,               "",
                    "Disable log via syslog");
    args.add_option("--comm-envelope-timeout",   handle_comm_envelope_timeout,   "VALUE",
                    "Set the total envelope timeout for commissioning");

    args.add_option("--commission-device",       handle_commission_device,       "",
                    "Enable device commissioning");

    args.add_option("--debug-level",             handle_debug_level,             "NUMBER",
                    "Enable debug output at level VALUE (higher=more)");
    if (argc == 1)
    {
        args.usage("No parameters!\n");
    }
    /* parse the args */
    while (args.parse_args() != -1)
        ;
}
