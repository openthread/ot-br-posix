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

#ifndef OTBR_LOG_TAG
#define OTBR_LOG_TAG ""
#endif

/**
 * Logging level.
 *
 */
typedef enum
{
    OTBR_LOG_LEVEL_CRIT, ///< Critical conditions.
    OTBR_LOG_LEVEL_WARN, ///< Warning conditions.
    OTBR_LOG_LEVEL_NOTE, ///< Normal but significant condition.
    OTBR_LOG_LEVEL_INFO, ///< Informational.
    OTBR_LOG_LEVEL_DEBG, ///< Debug-level messages.
} otbrLogLevel;

/**
 * Get current log level
 */
otbrLogLevel otbrLogGetLevel(void);

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
void otbrLogInit(const char *aIdent, otbrLogLevel aLevel, bool aPrintStderr);

/**
 * This function log at level @p aLevel.
 *
 * @param[in]   aLevel         Log level of the logger.
 * @param[in]   aLogTag        Log tag.
 * @param[in]   aFormat        Format string as in printf.
 *
 */
void otbrLog(otbrLogLevel aLevel, const char *aLogTag, const char *aFormat, ...);

/**
 * This function log at level @p aLevel.
 *
 * @param[in]   aLevel  Log level of the logger.
 * @param[in]   aFormat Format string as in printf.
 *
 */
void otbrLogv(otbrLogLevel aLevel, const char *aFormat, va_list);

/**
 * This function dump memory as hex string at level @p aLevel.
 *
 * @param[in]   aLevel  Log level of the logger.
 * @param[in]   aPrefix String before dumping memory.
 * @param[in]   aMemory The pointer to the memory to be dumped.
 * @param[in]   aSize   The size of memory in bytes to be dumped.
 *
 */
void otbrDump(otbrLogLevel aLevel, const char *aPrefix, const void *aMemory, size_t aSize);

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

/**
 * This macro log a action result according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_LEVEL_INFO,
 * otherwise OTBR_LOG_LEVEL_WARN.
 *
 * @param[in]   aError    The action result.
 * @param[in]   aFormat   Format string as in printf.
 * @param[in]   ...       Arguments for the format specification.
 *
 */
#define otbrLogResult(aError, aFormat, ...)                                                                        \
    do                                                                                                             \
    {                                                                                                              \
        otbrError _err = (aError);                                                                                 \
        otbrLog(_err == OTBR_ERROR_NONE ? OTBR_LOG_LEVEL_INFO : OTBR_LOG_LEVEL_WARN, OTBR_LOG_TAG, aFormat ": %s", \
                ##__VA_ARGS__, otbrErrorString(_err));                                                             \
    } while (0)

/**
 * @def LogCrit
 *
 * Logging at log level critical.
 *
 * @param[in] ...  Arguments for the format specification.
 *
 */
/**
 * @def LogWarn
 *
 * Logging at log level warnning.
 *
 * @param[in] ...  Arguments for the format specification.
 *
 */
/**
 * @def LogNote
 *
 * Logging at log level notice.
 *
 * @param[in] ...  Arguments for the format specification.
 *
 */
/**
 * @def LogInfo
 *
 * Logging at log level information.
 *
 * @param[in] ...  Arguments for the format specification.
 *
 */
/**
 * @def LogDebg
 *
 * Logging at log level debug.
 *
 * @param[in] ...  Arguments for the format specification.
 *
 */
#define otbrLogCrit(...) otbrLog(OTBR_LOG_LEVEL_CRIT, OTBR_LOG_TAG, __VA_ARGS__)
#define otbrLogWarn(...) otbrLog(OTBR_LOG_LEVEL_WARN, OTBR_LOG_TAG, __VA_ARGS__)
#define otbrLogNote(...) otbrLog(OTBR_LOG_LEVEL_NOTE, OTBR_LOG_TAG, __VA_ARGS__)
#define otbrLogInfo(...) otbrLog(OTBR_LOG_LEVEL_INFO, OTBR_LOG_TAG, __VA_ARGS__)
#define otbrLogDebg(...) otbrLog(OTBR_LOG_LEVEL_DEBG, OTBR_LOG_TAG, __VA_ARGS__)

#endif // OTBR_COMMON_LOGGING_HPP_
