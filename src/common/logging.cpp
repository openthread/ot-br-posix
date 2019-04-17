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

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>

#include "time.hpp"

static int        sLevel      = LOG_INFO;
static const char kHexChars[] = "0123456789abcdef";

static unsigned long sMsecsStart;
static bool          sLogCol0 = true; /* we start at col0 */
static FILE *        sLogFp;
static bool          sSyslogEnabled = true;
static bool          sSyslogOpened  = false;

#define LOGFLAG_syslog 1
#define LOGFLAG_file 2

/** Set/Clear syslog enable flag */
void otbrLogEnableSyslog(bool b)
{
    sSyslogEnabled = b;
}

/** Enable logging to a specific file */
void otbrLogSetFilename(const char *filename)
{
    if (sLogFp)
    {
        fclose(sLogFp);
        sLogFp = NULL;
    }
    sLogFp = fopen(filename, "w");
    if (sLogFp == NULL)
    {
        fprintf(stderr, "Cannot open log file: %s\n", filename);
        perror(filename);
        exit(EXIT_FAILURE);
    }
}

/** Get the current debug log level */
int otbrLogGetLevel(void)
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
static int LogCheck(int aLevel)
{
    int r;

    assert(aLevel >= LOG_EMERG && aLevel <= LOG_DEBUG);

    r = 0;

    if (sSyslogOpened && sSyslogEnabled && (aLevel <= sLevel))
    {
        r = r | LOGFLAG_syslog;
    }

    /* if someone has turned on the seperate file the most likely
     * situation is they are debugging a problem, or need a extra
     * information. In this case, we do not test the log level.
     */
    if (sLogFp != NULL)
    {
        r = r | LOGFLAG_file;
    }
    return r;
}

/** return the time, in milliseconds since application start */
static unsigned long GetMsecsNow(void)
{
    unsigned long now = ot::BorderRouter::GetNow();

    now -= sMsecsStart;
    return now;
}

/** Write this string to the private log file, inserting the timestamp at column 0 */
static void LogString(const char *cp)
{
    int ch;

    while ((ch = *cp++) != 0)
    {
        if (sLogCol0)
        {
            sLogCol0 = false;
            unsigned long n;
            n = GetMsecsNow();
            fprintf(sLogFp, "%4lu.%03lu | ", (n / 1000), (n % 1000));
        }
        if (ch == '\n')
        {
            sLogCol0 = true;
        }
        fputc(ch, sLogFp);
        if (ch == '\n')
        {
            /* force flush (in case something crashes) */
            fflush(sLogFp);
        }
    }
}

/** Print to the private log file */
static void LogVprintf(const char *fmt, va_list ap)
{
    char buf[1024];

    /* if not enabled ... leave */
    if (sLogFp == NULL)
    {
        return;
    }
    vsnprintf(buf, sizeof(buf), fmt, ap);

    LogString(buf);
}

/** Print to the private log file */
static void LogPrintf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    LogVprintf(fmt, ap);
    va_end(ap);
}

/** Initialize logging */
void otbrLogInit(const char *aIdent, int aLevel, bool aPrintStderr)
{
    assert(aIdent);
    assert(aLevel >= LOG_EMERG && aLevel <= LOG_DEBUG);

    sMsecsStart = ot::BorderRouter::GetNow();

    /* only open the syslog once... */
    if (!sSyslogOpened)
    {
        sSyslogOpened = true;
        openlog(aIdent, (LOG_CONS | LOG_PID) | (aPrintStderr ? LOG_PERROR : 0), LOG_USER);
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

    r = LogCheck(aLevel);

    if (r & LOGFLAG_file)
    {
        va_list cpy;
        va_copy(cpy, ap);
        LogVprintf(aFormat, cpy);
        va_end(cpy);

        /* logs do not end with a NEWLINE, we add one here */
        LogString("\n");
    }

    if (r & LOGFLAG_syslog)
    {
        vsyslog(aLevel, aFormat, ap);
    }
}

/** Hex dump data to the log */
void otbrDump(int aLevel, const char *aPrefix, const void *aMemory, size_t aSize)
{
    assert(aPrefix && (aMemory || aSize == 0));
    const uint8_t *pEnd;
    const uint8_t *p8;
    int            r;
    int            addr;

    r = LogCheck(aLevel);
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
        p8   = (const uint8_t *)(aMemory) + addr;

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
            *++ch = kHexChars[(*p8) >> 4];
            *++ch = kHexChars[(*p8) & 0x0f];
            *++ch = ' ';
        }
        *ch = 0;

        if (r & LOGFLAG_syslog)
        {
            syslog(aLevel, "%s: %04x: %s", aPrefix, addr, hex);
        }
        if (r & LOGFLAG_file)
        {
            LogPrintf("%s: %04x: %s\n", aPrefix, addr, hex);
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

void otbrLogResult(const char *aAction, otbrError aError)
{
    otbrLog((aError == OTBR_ERROR_NONE ? OTBR_LOG_INFO : OTBR_LOG_WARNING), "%s: %s", aAction, otbrErrorString(aError));
}

void otbrLogDeinit(void)
{
    sSyslogOpened = false;
    closelog();
}
