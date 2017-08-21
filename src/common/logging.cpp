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

#include "logging.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>

static int        sLevel = LOG_INFO;
static const char HEX_CHARS[] = "0123456789abcdef";

static unsigned msecs_start;
static bool     log_col0 = true; /* we start at col0 */
static FILE    *log_fp;
static bool     syslog_enabled = true;
static bool     syslog_opened = false;

#define LOGFLAG_syslog 1
#define LOGFLAG_file   2

/** Set/Clear syslog enable flag */
void otbrLogEnableSyslog(bool b)
{
    syslog_enabled = b;
}

/** Enable logging to a specific file */
void otbrLogSetFilename(const char *filename)
{
    log_fp = fopen(filename, "w");
    if (log_fp == NULL)
    {
        fprintf(stderr, "Cannot open log file: %s\n", filename);
        perror(filename);
        exit(EXIT_FAILURE);
    }
}

/** Get the current debug log level */
int  otbrLogGetLevel(void)
{
    return sLevel;
}

/** Set the debug log level */
void otbrLogSetLevel(int aLevel)
{
    assert(aLevel >= LOG_EMERG && aLevel <= LOG_DEBUG);
    sLevel = aLevel;
}

/** Determine if we should not or not log, and if so where to */
static int do_log(int aLevel)
{
    int r;

    assert(aLevel >= LOG_EMERG && aLevel <= LOG_DEBUG);

    r = 0;

    if (syslog_opened && syslog_enabled && (aLevel <= sLevel))
    {
        r = r | LOGFLAG_syslog;
    }

    /* if someone has turned on the seperate file the most likely
     * situation is they are debugging a problem, or need a extra
     * information. In this case, we do not test the log level.
     */
    if (log_fp != NULL)
    {
        r = r | LOGFLAG_file;
    }
    return r;
}


/** return the time, in milliseconds since application start */
static unsigned msecs_now(void)
{
    struct timeval tv;
    uint64_t       v;

    gettimeofday(&tv, NULL);

    v = tv.tv_sec;
    v = v * 1000;
    v = v + (tv.tv_usec / 1000);

    if (msecs_start == 0)
    {
        msecs_start = (unsigned)(v);
    }
    v = v - msecs_start;
    return v;
}


/** Write this string to the private log file, inserting the timestamp at column 0 */
static void log_str(const char *cp)
{
    int ch;

    while ((ch = *cp++) != 0)
    {
        if (log_col0)
        {
            log_col0 = false;
            unsigned n;
            n = msecs_now();
            fprintf(log_fp, "%4d.%03d | ", (n / 1000), (n % 1000));
        }
        if (ch == '\n')
        {
            log_col0 = true;
        }
        fputc(ch, log_fp);
        if (ch == '\n')
        {
	    /* force flush (in case something crashes) */
            fflush(log_fp);
        }
    }
}

/** Print to the private log file */
static void log_vprintf(const char *fmt, va_list ap)
{
    char buf[1024];

    /* if not enabled ... leave */
    if (log_fp == NULL)
    {
        return;
    }
    vsnprintf(buf, sizeof(buf), fmt, ap);

    log_str(buf);
}

/** Print to the private log file */
static void log_printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_vprintf(fmt, ap);
    va_end(ap);
}


/** Initialize logging */
void otbrLogInit(const char *aIdent, int aLevel)
{
    assert(aIdent);
    assert(aLevel >= LOG_EMERG && aLevel <= LOG_DEBUG);

    /* only open the syslog once... */
    if (!syslog_opened)
    {
        syslog_opened = true;
        openlog(aIdent, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    }
    sLevel = aLevel;
}

/** log to the syslog or log file */
void otbrLog(int aLevel, const char *aFormat, ...)
{
    va_list ap;

    va_start(ap, aFormat);
    otbrLogv(aLevel, aFormat, ap);
    va_end(ap);
}

/** log to the syslog or log file */
void otbrLogv(int aLevel, const char *aFormat, va_list ap)
{
    int r;

    assert(aFormat);

    r = do_log(aLevel);


    if (r & LOGFLAG_file)
    {
        va_list cpy;
        va_copy(cpy, ap);
        log_vprintf(aFormat, cpy);
        va_end(cpy);

        /* logs do not end with a NEWLINE, we add one here */
        log_str("\n");
    }

    if (r & LOGFLAG_syslog)
    {
        vsyslog(aLevel, aFormat, ap);
    }
}

/** Hex dump data to the log */
void otbrDump(int aLevel, const char *aPrefix, const void *aMemory, size_t aSize)
{
    assert(aPrefix && aMemory);
    const uint8_t *pEnd;
    const uint8_t *p8;
    int            r;
    int            addr;

    r = do_log(aLevel);
    if (r == 0)
    {
        return;
    }


    /* break hex dumps into 16byte lines
     * In the form ADDR: XX XX XX XX ...
     */

    // we pre-increment... so subtract
    addr = -16;

    while (aSize > 0)
    {
        size_t this_size;
        char   hex[16 * 3 + 1];

        addr = addr + 16;
        p8 = (const uint8_t *)(aMemory) + addr;

        /* truncate line to max 16 bytes */
        this_size = aSize;
        if (this_size > 16)
        {
            this_size = 16;
        }
        aSize = aSize - this_size;

        char *ch = hex - 1;

        for (pEnd = p8 + this_size; p8 < pEnd; p8++)
        {
            *++ch = HEX_CHARS[(*p8) >> 4];
            *++ch = HEX_CHARS[(*p8) & 0x0f];
            *++ch = ' ';
        }
        *ch = 0;

        if (r & LOGFLAG_syslog)
        {
            syslog(aLevel, "%s: %04x: %s", aPrefix, addr, hex);
        }
        if (r & LOGFLAG_file)
        {
            log_printf("%s: %04x: %s\n", aPrefix, addr, hex);
        }
    }
}

const char *otbrErrorString(otbrError aError)
{
    const char *error = NULL;

    switch (aError)
    {
    case OTBR_ERROR_NONE:
        error = "OK";
        break;

    case OTBR_ERROR_ERRNO:
        error = strerror(errno);
        break;

    case OTBR_ERROR_DTLS:
        error = "DTLS error";
        break;

    case OTBR_ERROR_DBUS:
        error = "DBUS error";
        break;

    case OTBR_ERROR_MDNS:
        error = "MDNS error";
        break;

    default:
        assert(false);
    }

    return error;
}

void otbrLogDeinit(void)
{
    syslog_opened = false;
    closelog();
}
