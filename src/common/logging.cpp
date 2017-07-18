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
#include <string.h>
#include <syslog.h>

static int        sLevel = LOG_INFO;
static const char HEX_CHARS[] = "0123456789abcdef";

void otbrLogInit(const char *aIdent, int aLevel)
{
    assert(aIdent);
    assert(aLevel >= LOG_EMERG && aLevel <= LOG_DEBUG);

    openlog(aIdent, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    sLevel = aLevel;
}

void otbrLog(int aLevel, const char *aFormat, ...)
{
    assert(aFormat);
    assert(aLevel >= LOG_EMERG && aLevel <= LOG_DEBUG);

    if (aLevel > sLevel)
    {
        return;
    }

    va_list args;
    va_start(args, aFormat);
    vsyslog(aLevel, aFormat, args);
    va_end(args);
}

void otbrDump(int aLevel, const char *aPrefix, const void *aMemory, size_t aSize)
{
    assert(aPrefix && aMemory);
    assert(aLevel >= LOG_EMERG && aLevel <= LOG_DEBUG);

    if (aLevel > sLevel)
    {
        return;
    }

    char *hex = new char[aSize * 2 + 1];
    char *ch = hex - 1;

    for (const uint8_t *mem = static_cast<const uint8_t *>(aMemory), *end = mem + aSize;
         mem != end; ++mem)
    {
        *++ch = HEX_CHARS[(*mem) >> 4];
        *++ch = HEX_CHARS[(*mem) & 0x0f];
    }
    hex[aSize * 2] = 0;

    syslog(aLevel, "%s #%zu %s", aPrefix, aSize, hex);
    delete[] hex;
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
    closelog();
}
