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
 *   The file is the header for the command line params for the commissioner test app.
 */

#if !defined(COMMISSIONER_HPP_H)
#define COMMISSIONER_HPP_H

/**
 * Simple class to handle command line parameters
 * To avoid the use of "getopt_long()" we have this.
 */

/* forward */
class argcargv;

/* option entry in our table */
struct argcargv_opt {
    const char *name;
    void (*handler)(argcargv *);
    const char *valuehelp;
    const char *helptext;
};

class argcargv {
public:

    /** Constructor */
    argcargv( int argc, char **argv );

    /** pseudo globals for argc & argv parsing */
    int mARGC;     /* analogous to argc */
    char **mARGV;  /* analogous to argv */
    int mARGx;     /**< current argument */

    enum {
	max_opts = 40
    };
    struct argcargv_opt mOpts[max_opts];

    /** print usage error message and exit */
    void usage( const char *fmt,...);

    /** add an option to be parsed */
    void add_option( const char *name,
		     void (*handler)( argcargv *pThis ),
		     const char *valuehelp,
		     const char *help );
    
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
    void        hex_param(char *ascii_puthere, uint8_t *bin_puthere, int sizeof_bin );

    /**
     * Parse a numeric parameter from the command line
     *
     * If something is wrong print an error & usage text.
     *
     * @returns value from command line as an integer
     */
    int         num_param(void);


    /** 
     *  This parses a single command line parameter
     *
     * @returns 0 if there are more parameters to parse
     * @returns -1 if there are no more parameters to parse
     *
     * This does not handle positional parameters.
     */
    int         parse_args(void);
};

/**
 * Called from main() to parse the commissioner test app command line parameters.
 */
void commissioner_argcargv( int argc, char **argv );

#endif
