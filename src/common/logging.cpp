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

#include "common/logging.hpp"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>

#include "common/code_utils.hpp"
#include "common/time.hpp"

static otbrLogLevel sLevel            = OTBR_LOG_LEVEL_INFO;
static const char   sLevelString[][7] = {
    "[CRIT]", "[WARN]", "[NOTE]", "[INFO]", "[DEBG]",
};

static int ToSyslogLogLevel(otbrLogLevel aLogLevel)
{
    int logLevel;

    switch (aLogLevel)
    {
    case OTBR_LOG_LEVEL_CRIT:
        logLevel = LOG_CRIT;
        break;
    case OTBR_LOG_LEVEL_WARN:
        logLevel = LOG_WARNING;
        break;
    case OTBR_LOG_LEVEL_NOTE:
        logLevel = LOG_NOTICE;
        break;
    case OTBR_LOG_LEVEL_INFO:
        logLevel = LOG_INFO;
        break;
    case OTBR_LOG_LEVEL_DEBG:
        logLevel = LOG_DEBUG;
        break;
    default:
        assert(false);
        logLevel = LOG_DEBUG;
        break;
    }

    return logLevel;
}

/** Get the current debug log level */
otbrLogLevel otbrLogGetLevel(void)
{
    return sLevel;
}

/** Initialize logging */
void otbrLogInit(const char *aIdent, otbrLogLevel aLevel, bool aPrintStderr)
{
    assert(aIdent);
    assert(aLevel >= OTBR_LOG_LEVEL_CRIT && aLevel <= OTBR_LOG_LEVEL_DEBG);

    openlog(aIdent, (LOG_CONS | LOG_PID) | (aPrintStderr ? LOG_PERROR : 0), LOG_USER);
    sLevel = aLevel;
}

/** log to the syslog or log file */
void otbrLog(otbrLogLevel aLevel, const char *aRegionPrefix, const char *aFormat, ...)
{
    const uint16_t kBufferSize = 1024;
    va_list        ap;
    std::string    logString;
    char           buffer[kBufferSize];

    va_start(ap, aFormat);

    VerifyOrExit(aLevel <= sLevel);
    VerifyOrExit(vsnprintf(buffer, sizeof(buffer), aFormat, ap) > 0);

    logString.append(sLevelString[aLevel]);
    logString.append(aRegionPrefix);
    logString.append(buffer);

    syslog(ToSyslogLogLevel(aLevel), "%s", logString.c_str());

    va_end(ap);

exit:
    return;
}

/** log to the syslog or log file */
void otbrLogv(otbrLogLevel aLevel, const char *aFormat, va_list ap)
{
    assert(aFormat);

    if (aLevel <= sLevel)
    {
        vsyslog(ToSyslogLogLevel(aLevel), aFormat, ap);
    }
}

/** Hex dump data to the log */
void otbrDump(otbrLogLevel aLevel, const char *aPrefix, const void *aMemory, size_t aSize)
{
    static const char kHexChars[] = "0123456789abcdef";
    assert(aPrefix && (aMemory || aSize == 0));
    const uint8_t *pEnd;
    const uint8_t *p8;
    int            addr;

    if (aLevel >= sLevel)
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

        syslog(ToSyslogLogLevel(aLevel), "%s: %04x: %s", aPrefix, addr, hex);
    }
}

const char *otbrErrorString(otbrError aError)
{
    const char *error;

    switch (aError)
    {
    case OTBR_ERROR_NONE:
        error = "OK";
        break;

    case OTBR_ERROR_ERRNO:
        error = strerror(errno);
        break;

    case OTBR_ERROR_DBUS:
        error = "DBUS error";
        break;

    case OTBR_ERROR_MDNS:
        error = "MDNS error";
        break;

    case OTBR_ERROR_OPENTHREAD:
        error = "OpenThread error";
        break;

    case OTBR_ERROR_NOT_FOUND:
        error = "Not found";
        break;

    case OTBR_ERROR_PARSE:
        error = "Parse error";
        break;

    case OTBR_ERROR_NOT_IMPLEMENTED:
        error = "Not implemented";
        break;

    case OTBR_ERROR_INVALID_ARGS:
        error = "Invalid arguments";
        break;

    default:
        error = "Unknown";
    }

    return error;
}

void otbrLogDeinit(void)
{
    closelog();
}
