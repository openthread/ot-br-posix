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
 * @param[in]   aRegionPrefix  Log region prefix.
 * @param[in]   aFormat        Format string as in printf.
 *
 */
void otbrLog(otbrLogLevel aLevel, const char *aRegionPrefix, const char *aFormat, ...);

/**
 * This macro log a action result according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_LEVEL_INFO,
 * otherwise OTBR_LOG_LEVEL_WARN.
 *
 * @param[in]   aRegionPrefix  Log region prefix.
 * @param[in]   aError         The action result.
 * @param[in]   aFormat        Format string as in printf.
 *
 */
#define otbrLogResult(aRegionPrefix, aError, aFormat, ...)                                                          \
    do                                                                                                              \
    {                                                                                                               \
        otbrError _err = (aError);                                                                                  \
        otbrLog(_err == OTBR_ERROR_NONE ? OTBR_LOG_LEVEL_INFO : OTBR_LOG_LEVEL_WARN, aRegionPrefix, aFormat ": %s", \
                ##__VA_ARGS__, otbrErrorString(_err));                                                              \
    } while (0)

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
 * Log region prefix string definitions.
 *
 */
#define OTBR_REGION_AGENT_PREFIX "-AGENT---: "
#define OTBR_REGION_MDNS_PREFIX "-MDNS----: "
#define OTBR_REGION_DBUS_PREFIX "-DBUS----: "
#define OTBR_REGION_UBUS_PREFIX "-UBUS----: "
#define OTBR_REGION_REST_PREFIX "-REST----: "
#define OTBR_REGION_BBR_PREFIX "-BBR-----: "
#define OTBR_REGION_ADPROXY_PREFIX "-ADPROXY-: "
#define OTBR_REGION_WEB_PREFIX "-WEB-----: "

/**
 * @def otbrLogCritAgent
 *
 * This function generates a log with level critical for the Agent region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 *
 */

/**
 * @def otbrLogWarnAgent
 *
 * This function generates a log with level warning for the Agent region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogNoteAgent
 *
 * This function generates a log with level note for the Agent region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogInfoAgent
 *
 * This function generates a log with level info for the Agent region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogDebgAgent
 *
 * This function generates a log with level debug for the Agent region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */
#define otbrLogCritAgent(...) otbrLog(OTBR_LOG_LEVEL_CRIT, OTBR_REGION_AGENT_PREFIX, __VA_ARGS__)
#define otbrLogWarnAgent(...) otbrLog(OTBR_LOG_LEVEL_WARN, OTBR_REGION_AGENT_PREFIX, __VA_ARGS__)
#define otbrLogNoteAgent(...) otbrLog(OTBR_LOG_LEVEL_NOTE, OTBR_REGION_AGENT_PREFIX, __VA_ARGS__)
#define otbrLogInfoAgent(...) otbrLog(OTBR_LOG_LEVEL_INFO, OTBR_REGION_AGENT_PREFIX, __VA_ARGS__)
#define otbrLogDebgAgent(...) otbrLog(OTBR_LOG_LEVEL_DEBG, OTBR_REGION_AGENT_PREFIX, __VA_ARGS__)

/**
 * @def otbrLogResultAgent
 *
 * This function logs a action result with the Agent region prefix according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_LEVEL_INFO,
 * otherwise OTBR_LOG_LEVEL_WARN.
 *
 * @param[in]  aError   The action result.
 * @param[in]  aFormat  Format string as in printf.
 *
 */
#define otbrLogResultAgent(aError, aFormat, ...) otbrLogResult(OTBR_REGION_AGENT_PREFIX, aError, aFormat, ##__VA_ARGS__)

/**
 * @def otbrLogCritMdns
 *
 * This function generates a log with level critical for the Mdns region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 *
 */

/**
 * @def otbrLogWarnMdns
 *
 * This function generates a log with level warning for the Mdns region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogNoteMdns
 *
 * This function generates a log with level note for the Mdns region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogInfoMdns
 *
 * This function generates a log with level info for the Mdns region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogDebgMdns
 *
 * This function generates a log with level debug for the Mdns region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */
#define otbrLogCritMdns(...) otbrLog(OTBR_LOG_LEVEL_CRIT, OTBR_REGION_MDNS_PREFIX, __VA_ARGS__)
#define otbrLogWarnMdns(...) otbrLog(OTBR_LOG_LEVEL_WARN, OTBR_REGION_MDNS_PREFIX, __VA_ARGS__)
#define otbrLogNoteMdns(...) otbrLog(OTBR_LOG_LEVEL_NOTE, OTBR_REGION_MDNS_PREFIX, __VA_ARGS__)
#define otbrLogInfoMdns(...) otbrLog(OTBR_LOG_LEVEL_INFO, OTBR_REGION_MDNS_PREFIX, __VA_ARGS__)
#define otbrLogDebgMdns(...) otbrLog(OTBR_LOG_LEVEL_DEBG, OTBR_REGION_MDNS_PREFIX, __VA_ARGS__)

/**
 * @def otbrLogResultMdns
 *
 * This function logs a action result with the Mdns region prefix according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_LEVEL_INFO,
 * otherwise OTBR_LOG_LEVEL_WARN.
 *
 * @param[in]  aError   The action result.
 * @param[in]  aFormat  Format string as in printf.
 *
 */
#define otbrLogResultMdns(aError, aFormat, ...) otbrLogResult(OTBR_REGION_MDNS_PREFIX, aError, aFormat, ##__VA_ARGS__)

/**
 * @def otbrLogCritDbus
 *
 * This function generates a log with level critical for the Dbus region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 *
 */

/**
 * @def otbrLogWarnDbus
 *
 * This function generates a log with level warning for the Dbus region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogNoteDbus
 *
 * This function generates a log with level note for the Dbus region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogInfoDbus
 *
 * This function generates a log with level info for the Dbus region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogDebgDbus
 *
 * This function generates a log with level debug for the Dbus region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */
#define otbrLogCritDbus(...) otbrLog(OTBR_LOG_LEVEL_CRIT, OTBR_REGION_DBUS_PREFIX, __VA_ARGS__)
#define otbrLogWarnDbus(...) otbrLog(OTBR_LOG_LEVEL_WARN, OTBR_REGION_DBUS_PREFIX, __VA_ARGS__)
#define otbrLogNoteDbus(...) otbrLog(OTBR_LOG_LEVEL_NOTE, OTBR_REGION_DBUS_PREFIX, __VA_ARGS__)
#define otbrLogInfoDbus(...) otbrLog(OTBR_LOG_LEVEL_INFO, OTBR_REGION_DBUS_PREFIX, __VA_ARGS__)
#define otbrLogDebgDbus(...) otbrLog(OTBR_LOG_LEVEL_DEBG, OTBR_REGION_DBUS_PREFIX, __VA_ARGS__)

/**
 * @def otbrLogResultDbus
 *
 * This function logs a action result with the Dbus region prefix according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_LEVEL_INFO,
 * otherwise OTBR_LOG_LEVEL_WARN.
 *
 * @param[in]  aError   The action result.
 * @param[in]  aFormat  Format string as in printf.
 *
 */
#define otbrLogResultDbus(aError, aFormat, ...) otbrLogResult(OTBR_REGION_DBUS_PREFIX, aError, aFormat, ##__VA_ARGS__)

/**
 * @def otbrLogCritUbus
 *
 * This function generates a log with level critical for the Ubus region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 *
 */

/**
 * @def otbrLogWarnUbus
 *
 * This function generates a log with level warning for the Ubus region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogNoteUbus
 *
 * This function generates a log with level note for the Ubus region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogInfoUbus
 *
 * This function generates a log with level info for the Ubus region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogDebgUbus
 *
 * This function generates a log with level debug for the Ubus region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */
#define otbrLogCritUbus(...) otbrLog(OTBR_LOG_LEVEL_CRIT, OTBR_REGION_UBUS_PREFIX, __VA_ARGS__)
#define otbrLogWarnUbus(...) otbrLog(OTBR_LOG_LEVEL_WARN, OTBR_REGION_UBUS_PREFIX, __VA_ARGS__)
#define otbrLogNoteUbus(...) otbrLog(OTBR_LOG_LEVEL_NOTE, OTBR_REGION_UBUS_PREFIX, __VA_ARGS__)
#define otbrLogInfoUbus(...) otbrLog(OTBR_LOG_LEVEL_INFO, OTBR_REGION_UBUS_PREFIX, __VA_ARGS__)
#define otbrLogDebgUbus(...) otbrLog(OTBR_LOG_LEVEL_DEBG, OTBR_REGION_UBUS_PREFIX, __VA_ARGS__)

/**
 * @def otbrLogResultUbus
 *
 * This function logs a action result with the Ubus region prefix according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_LEVEL_INFO,
 * otherwise OTBR_LOG_LEVEL_WARN.
 *
 * @param[in]  aError   The action result.
 * @param[in]  aFormat  Format string as in printf.
 *
 */
#define otbrLogResultUbus(aError, aFormat, ...) otbrLogResult(OTBR_REGION_UBUS_PREFIX, aError, aFormat, ##__VA_ARGS__)

/**
 * @def otbrLogCritRest
 *
 * This function generates a log with level critical for the Rest region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 *
 */

/**
 * @def otbrLogWarnRest
 *
 * This function generates a log with level warning for the Rest region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogNoteRest
 *
 * This function generates a log with level note for the Rest region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogInfoRest
 *
 * This function generates a log with level info for the Rest region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogDebgRest
 *
 * This function generates a log with level debug for the Rest region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */
#define otbrLogCritRest(...) otbrLog(OTBR_LOG_LEVEL_CRIT, OTBR_REGION_REST_PREFIX, __VA_ARGS__)
#define otbrLogWarnRest(...) otbrLog(OTBR_LOG_LEVEL_WARN, OTBR_REGION_REST_PREFIX, __VA_ARGS__)
#define otbrLogNoteRest(...) otbrLog(OTBR_LOG_LEVEL_NOTE, OTBR_REGION_REST_PREFIX, __VA_ARGS__)
#define otbrLogInfoRest(...) otbrLog(OTBR_LOG_LEVEL_INFO, OTBR_REGION_REST_PREFIX, __VA_ARGS__)
#define otbrLogDebgRest(...) otbrLog(OTBR_LOG_LEVEL_DEBG, OTBR_REGION_REST_PREFIX, __VA_ARGS__)

/**
 * @def otbrLogResultRest
 *
 * This function logs a action result with the Rest region prefix according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_LEVEL_INFO,
 * otherwise OTBR_LOG_LEVEL_WARN.
 *
 * @param[in]  aError   The action result.
 * @param[in]  aFormat  Format string as in printf.
 *
 */
#define otbrLogResultRest(aError, aFormat, ...) otbrLogResult(OTBR_REGION_REST_PREFIX, aError, aFormat, ##__VA_ARGS__)

/**
 * @def otbrLogCritBbr
 *
 * This function generates a log with level critical for the Bbr region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 *
 */

/**
 * @def otbrLogWarnBbr
 *
 * This function generates a log with level warning for the Bbr region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogNoteBbr
 *
 * This function generates a log with level note for the Bbr region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogInfoBbr
 *
 * This function generates a log with level info for the Bbr region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogDebgBbr
 *
 * This function generates a log with level debug for the Bbr region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */
#define otbrLogCritBbr(...) otbrLog(OTBR_LOG_LEVEL_CRIT, OTBR_REGION_BBR_PREFIX, __VA_ARGS__)
#define otbrLogWarnBbr(...) otbrLog(OTBR_LOG_LEVEL_WARN, OTBR_REGION_BBR_PREFIX, __VA_ARGS__)
#define otbrLogNoteBbr(...) otbrLog(OTBR_LOG_LEVEL_NOTE, OTBR_REGION_BBR_PREFIX, __VA_ARGS__)
#define otbrLogInfoBbr(...) otbrLog(OTBR_LOG_LEVEL_INFO, OTBR_REGION_BBR_PREFIX, __VA_ARGS__)
#define otbrLogDebgBbr(...) otbrLog(OTBR_LOG_LEVEL_DEBG, OTBR_REGION_BBR_PREFIX, __VA_ARGS__)

/**
 * @def otbrLogResultBbr
 *
 * This function logs a action result with the Bbr region prefix according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_LEVEL_INFO,
 * otherwise OTBR_LOG_LEVEL_WARN.
 *
 * @param[in]  aError   The action result.
 * @param[in]  aFormat  Format string as in printf.
 *
 */
#define otbrLogResultBbr(aError, aFormat, ...) otbrLogResult(OTBR_REGION_BBR_PREFIX, aError, aFormat, ##__VA_ARGS__)

/**
 * @def otbrLogCritAdProxy
 *
 * This function generates a log with level critical for the AdProxy region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 *
 */

/**
 * @def otbrLogWarnAdProxy
 *
 * This function generates a log with level warning for the AdProxy region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogNoteAdProxy
 *
 * This function generates a log with level note for the AdProxy region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogInfoAdProxy
 *
 * This function generates a log with level info for the AdProxy region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogDebgAdProxy
 *
 * This function generates a log with level debug for the AdProxy region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */
#define otbrLogCritAdProxy(...) otbrLog(OTBR_LOG_LEVEL_CRIT, OTBR_REGION_ADPROXY_PREFIX, __VA_ARGS__)
#define otbrLogWarnAdProxy(...) otbrLog(OTBR_LOG_LEVEL_WARN, OTBR_REGION_ADPROXY_PREFIX, __VA_ARGS__)
#define otbrLogNoteAdProxy(...) otbrLog(OTBR_LOG_LEVEL_NOTE, OTBR_REGION_ADPROXY_PREFIX, __VA_ARGS__)
#define otbrLogInfoAdProxy(...) otbrLog(OTBR_LOG_LEVEL_INFO, OTBR_REGION_ADPROXY_PREFIX, __VA_ARGS__)
#define otbrLogDebgAdProxy(...) otbrLog(OTBR_LOG_LEVEL_DEBG, OTBR_REGION_ADPROXY_PREFIX, __VA_ARGS__)

/**
 * @def otbrLogResultAdProxy
 *
 * This function logs a action result with the AdProxy region prefix according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_LEVEL_INFO,
 * otherwise OTBR_LOG_LEVEL_WARN.
 *
 * @param[in]  aError   The action result.
 * @param[in]  aFormat  Format string as in printf.
 *
 */
#define otbrLogResultAdProxy(aError, aFormat, ...) \
    otbrLogResult(OTBR_REGION_ADPROXY_PREFIX, aError, aFormat, ##__VA_ARGS__)

/**
 * @def otbrLogCritWeb
 *
 * This function generates a log with level critical for the Web region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 *
 */

/**
 * @def otbrLogWarnWeb
 *
 * This function generates a log with level warning for the Web region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogNoteWeb
 *
 * This function generates a log with level note for the Web region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogInfoWeb
 *
 * This function generates a log with level info for the Web region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */

/**
 * @def otbrLogDebgWeb
 *
 * This function generates a log with level debug for the Web region.
 *
 * @param[in]  ...  Arguments for the format specification.
 *
 */
#define otbrLogCritWeb(...) otbrLog(OTBR_LOG_LEVEL_CRIT, OTBR_REGION_WEB_PREFIX, __VA_ARGS__)
#define otbrLogWarnWeb(...) otbrLog(OTBR_LOG_LEVEL_WARN, OTBR_REGION_WEB_PREFIX, __VA_ARGS__)
#define otbrLogNoteWeb(...) otbrLog(OTBR_LOG_LEVEL_NOTE, OTBR_REGION_WEB_PREFIX, __VA_ARGS__)
#define otbrLogInfoWeb(...) otbrLog(OTBR_LOG_LEVEL_INFO, OTBR_REGION_WEB_PREFIX, __VA_ARGS__)
#define otbrLogDebgWeb(...) otbrLog(OTBR_LOG_LEVEL_DEBG, OTBR_REGION_WEB_PREFIX, __VA_ARGS__)

/**
 * @def otbrLogResultWeb
 *
 * This function logs a action result with the Web region prefix according to @p aError.
 *
 * If @p aError is OTBR_ERROR_NONE, the log level will be OTBR_LOG_LEVEL_INFO,
 * otherwise OTBR_LOG_LEVEL_WARN.
 *
 * @param[in]  aError   The action result.
 * @param[in]  aFormat  Format string as in printf.
 *
 */
#define otbrLogResultWeb(aError, aFormat, ...) otbrLogResult(OTBR_REGION_WEB_PREFIX, aError, aFormat, ##__VA_ARGS__)

#endif // OTBR_COMMON_LOGGING_HPP_
