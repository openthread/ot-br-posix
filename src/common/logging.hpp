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
 * This file define logging interface.
 */
#ifndef OTBR_COMMON_LOGGING_HPP_
#define OTBR_COMMON_LOGGING_HPP_

#include "openthread-br/config.h"

#include <stdarg.h>
#include <stddef.h>

#include "common/types.hpp"

/**
 * Logging level, which is identical to syslog
 *
 */
enum
{
    OTBR_LOG_EMERG,   /* system is unusable */
    OTBR_LOG_ALERT,   /* action must be taken immediately */
    OTBR_LOG_CRIT,    /* critical conditions */
    OTBR_LOG_ERR,     /* error conditions */
    OTBR_LOG_WARNING, /* warning conditions */
    OTBR_LOG_NOTICE,  /* normal but significant condition */
    OTBR_LOG_INFO,    /* informational */
    OTBR_LOG_DEBUG,   /* debug-level messages */
};

/**
 * Get current log level
 */
int otbrLogGetLevel(void);

/**
 * Control log to syslog
 *
 * @param[in] enable true to log to/via syslog
 *
 */
void otbrLogEnableSyslog(bool aEnabled);

/**
 * This function initialize the logging service.
 *
 * @param[in]   aIdent          Identity of the logger.
 * @param[in]   aLevel          Log level of the logger.
 * @param[in]   aPrintStderr    Whether to log to stderr.
 *
 */
void otbrLogInit(const char *aIdent, int aLevel, bool aPrintStderr);

/**
 * This function log at level @p aLevel.
 *
 * @param[in]   aLevel  Log level of the logger.
 * @param[in]   aFormat Format string as in printf.
 *
 */
void otbrLog(int aLevel, const char *aFormat, ...);

/**
 * This macro log a action result according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_INFO,
 * otherwise OTBR_LOG_WARNING.
 *
 * @param[in]   aError  The action result.
 * @param[in]   aFormat Format string as in printf.
 *
 */
#define otbrLogResult(aError, aFormat, ...)                                                                \
    do                                                                                                     \
    {                                                                                                      \
        otbrError _err = (aError);                                                                         \
        otbrLog(_err == OTBR_ERROR_NONE ? OTBR_LOG_INFO : OTBR_LOG_WARNING, aFormat ": %s", ##__VA_ARGS__, \
                otbrErrorString(_err));                                                                    \
    } while (0)

/**
 * This function log at level @p aLevel.
 *
 * @param[in]   aLevel  Log level of the logger.
 * @param[in]   aFormat Format string as in printf.
 *
 */
void otbrLogv(int aLevel, const char *aFormat, va_list);

/**
 * This function dump memory as hex string at level @p aLevel.
 *
 * @param[in]   aLevel  Log level of the logger.
 * @param[in]   aPrefix String before dumping memory.
 * @param[in]   aMemory The pointer to the memory to be dumped.
 * @param[in]   aSize   The size of memory in bytes to be dumped.
 *
 */
void otbrDump(int aLevel, const char *aPrefix, const void *aMemory, size_t aSize);

/**
 * This function converts error code to string.
 *
 * @param[in]   aError      The error code.
 *
 * @returns The string information of error.
 *
 */
const char *otbrErrorString(otbrError aError);

/**
 * This function deinitializes the logging service.
 *
 */
void otbrLogDeinit(void);

#endif // OTBR_COMMON_LOGGING_HPP_
