/*
 *  Copyright (c) 2026, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

// C++ Standard Library headers
#include <fstream>
#include <string>
#include <vector>

// C Standard Library headers
#include <atomic>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <glob.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

// OpenThread Public headers
#include <openthread/instance.h>
#include <openthread/link.h>
#include <openthread/logging.h>
#include <openthread/openthread-system.h>
#include <openthread/platform/logging.h>
#include <openthread/platform/time.h>
#include <openthread/platform/toolchain.h>

// Local headers
#include "pcapng.hpp"
#include "common/code_utils.hpp"

using namespace ot::Extcap;

namespace {

// Translation-unit-local State (prefixed with s to denote static duration)
std::atomic<bool> sShouldQuit{false};
PcapngWriter      sPcapngWriter;
bool              sDebugEnabled = false;

void Log(const char *aFormat, ...) OT_TOOL_PRINTF_STYLE_FORMAT_ARG_CHECK(1, 2);
void LogAndPrint(const char *aFormat, ...) OT_TOOL_PRINTF_STYLE_FORMAT_ARG_CHECK(1, 2);

void Log(const char *aFormat, ...)
{
    va_list args;

    va_start(args, aFormat);
    vsyslog(LOG_INFO, aFormat, args);
    va_end(args);
}

void LogAndPrint(const char *aFormat, ...)
{
    va_list args;

    va_start(args, aFormat);
    vsyslog(LOG_INFO, aFormat, args);
    va_end(args);

    va_start(args, aFormat);
    vfprintf(stderr, aFormat, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void InitLogging(void)
{
    openlog("ot-extcap", LOG_PID | LOG_CONS, LOG_DAEMON);
    setlogmask(sDebugEnabled ? LOG_UPTO(LOG_DEBUG) : LOG_UPTO(LOG_WARNING));
    IgnoreError(otLoggingSetLevel(sDebugEnabled ? OT_LOG_LEVEL_DEBG : OT_LOG_LEVEL_WARN));
}

struct RadioUrlConfig
{
    uint64_t    mId;  // Parsed 64-bit ID
    std::string mUrl; // Radio URL
};

void Trim(std::string &aStr)
{
    size_t first = aStr.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        aStr.clear();
    }
    else
    {
        size_t last = aStr.find_last_not_of(" \t\r\n");
        aStr        = aStr.substr(first, last - first + 1);
    }
}

bool ParseId(const std::string &aStr, uint64_t &aId)
{
    char *endptr;

    errno = 0;
    // Use strtoull with base 0 to support hex (with 0x prefix) and decimal
    aId = strtoull(aStr.c_str(), &endptr, 0);

    return (endptr != aStr.c_str() && *endptr == '\0' && errno != ERANGE);
}

uint64_t Eui64ToUint64(const otExtAddress &aEui64)
{
    uint64_t id = 0;

    for (uint8_t byte : aEui64.m8)
    {
        id = (id << 8) | byte;
    }

    return id;
}

std::string GetWiresharkConfigFilePath(const char *aFilename)
{
    std::string path;
    const char *xdgConfig = getenv("XDG_CONFIG_HOME");

    if (xdgConfig != nullptr && xdgConfig[0] != '\0')
    {
        path = std::string(xdgConfig) + "/wireshark";
    }
    else
    {
        const char *home = getenv("HOME");
        if (home != nullptr && home[0] != '\0')
        {
#if __APPLE__
            path = std::string(home) + "/Library/Application Support/Wireshark";
#else
            path = std::string(home) + "/.config/wireshark";
#endif
        }
    }

    if (!path.empty())
    {
        path += "/extcap";
        if (aFilename != nullptr)
        {
            path += "/";
            path += aFilename;
        }
    }

    return path;
}

void GlobSerialPorts(glob_t &aGlobResult)
{
    int rval;

    memset(&aGlobResult, 0, sizeof(aGlobResult));

#if __APPLE__
    rval = glob("/dev/tty.usbserial-*", 0, nullptr, &aGlobResult);
    if (rval == 0 || rval == GLOB_NOMATCH)
    {
        static_cast<void>(glob("/dev/tty.usbmodem*", GLOB_APPEND, nullptr, &aGlobResult));
    }
#else
    rval = glob("/dev/ttyACM*", 0, nullptr, &aGlobResult);
    if (rval == 0 || rval == GLOB_NOMATCH)
    {
        static_cast<void>(glob("/dev/ttyUSB*", GLOB_APPEND, nullptr, &aGlobResult));
    }
#endif

    std::string   patternPath = GetWiresharkConfigFilePath("openthread_uart_patterns");
    std::ifstream file(patternPath);
    std::string   line;

    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            Trim(line);
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            static_cast<void>(glob(line.c_str(), GLOB_APPEND, nullptr, &aGlobResult));
        }
        file.close();
    }
}

std::string GetWiresharkLockFilePath(const char *aFilename)
{
    std::string path;
    const char *runtimeDir = getenv("XDG_RUNTIME_DIR");

    if (runtimeDir != nullptr && runtimeDir[0] != '\0')
    {
        path = std::string(runtimeDir) + "/" + aFilename + ".lock";
    }
    else
    {
        const char *tmpDir = getenv("TMPDIR");
        if (tmpDir == nullptr || tmpDir[0] == '\0')
        {
            tmpDir = "/tmp";
        }
        path = std::string(tmpDir) + "/ot-extcap-" + std::to_string(getuid()) + "-" + aFilename + ".lock";
    }

    return path;
}

struct RcpQueryResult
{
    uint32_t     mBaud{0};
    otExtAddress mEui64{};
};

struct RcpQuery
{
    std::string    mUrl;
    std::string    mPort; // Empty for custom URLs
    pid_t          mPid{-1};
    int            mPipeFd{-1};
    bool           mSuccess{false};
    otExtAddress   mEui64{};
    RcpQueryResult mResult{};
    size_t         mBytesRead{0};
};

void ExecuteSingleUrlProbe(const std::string &aUrl, int aPipeFdWrite)
{
    otPlatformConfig config;
    otInstance      *instance = nullptr;
    RcpQueryResult   result;
    bool             success = false;

    memset(&config, 0, sizeof(config));
    config.mCoprocessorUrls.mUrls[0] = aUrl.c_str();
    config.mCoprocessorUrls.mNum     = 1;
    config.mSpeedUpFactor            = 1;
    config.mDryRun                   = true;

    Log("Probe worker [PID %d]: Testing URL: %s", static_cast<int>(getpid()), aUrl.c_str());

    config.mCoprocessorType = otSysInitCoprocessor(&config.mCoprocessorUrls);
    VerifyOrExit(config.mCoprocessorType == OT_COPROCESSOR_RCP);

    instance = otSysInit(&config);
    VerifyOrExit(instance != nullptr);

    otLinkGetFactoryAssignedIeeeEui64(instance, &result.mEui64);

    if (aUrl.find("uart-baudrate=460800") != std::string::npos)
    {
        result.mBaud = 460800;
    }
    else if (aUrl.find("uart-baudrate=115200") != std::string::npos)
    {
        result.mBaud = 115200;
    }

    if (write(aPipeFdWrite, &result, sizeof(result)) == sizeof(result))
    {
        success = true;
    }

    otInstanceFinalize(instance);
    otSysDeinit();

exit:
    _exit(success ? 0 : 1);
}

void ExecuteDeviceQueryChildWorker(const RcpQuery &aQuery, int aPipeFdWrite)
{
    std::vector<std::string> urlsToTest;
    if (!aQuery.mPort.empty())
    {
        urlsToTest.push_back("spinel+hdlc+uart://" + aQuery.mPort + "?uart-baudrate=115200");
        urlsToTest.push_back("spinel+hdlc+uart://" + aQuery.mPort + "?uart-baudrate=460800");
    }
    else
    {
        urlsToTest.push_back(aQuery.mUrl);
    }

    bool overallSuccess = false;

    for (const auto &url : urlsToTest)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            continue;
        }

        if (pid == 0)
        {
            ExecuteSingleUrlProbe(url, aPipeFdWrite);
        }

        int status = 0;
        while (waitpid(pid, &status, 0) < 0)
        {
            if (errno != EINTR)
            {
                break;
            }
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            overallSuccess = true;
            break;
        }
    }

    close(aPipeFdWrite);
    _exit(overallSuccess ? 0 : 1);
}

void ProcessPipeResponse(RcpQuery &aQuery)
{
    uint8_t *resultBytes = reinterpret_cast<uint8_t *>(&aQuery.mResult);

    while (aQuery.mBytesRead < sizeof(aQuery.mResult))
    {
        ssize_t bytesRead =
            read(aQuery.mPipeFd, resultBytes + aQuery.mBytesRead, sizeof(aQuery.mResult) - aQuery.mBytesRead);

        if (bytesRead > 0)
        {
            aQuery.mBytesRead += bytesRead;
        }
        else if (bytesRead < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            break;
        }
        else
        {
            break;
        }
    }

    VerifyOrExit(aQuery.mBytesRead == sizeof(aQuery.mResult), {
        Log("FAILED: Query failed for %s: read %d bytes (expected %d)", aQuery.mUrl.c_str(),
            static_cast<int>(aQuery.mBytesRead), static_cast<int>(sizeof(aQuery.mResult)));
    });

    aQuery.mEui64 = aQuery.mResult.mEui64;
    if (!aQuery.mPort.empty())
    {
        aQuery.mUrl = "spinel+hdlc+uart://" + aQuery.mPort + "?uart-baudrate=" + std::to_string(aQuery.mResult.mBaud);
    }
    aQuery.mSuccess = true;
    Log("SUCCESS: Detected OpenThread sniffer device at %s with EUI-64: "
        "%02x%02x%02x%02x%02x%02x%02x%02x",
        aQuery.mUrl.c_str(), aQuery.mEui64.m8[0], aQuery.mEui64.m8[1], aQuery.mEui64.m8[2], aQuery.mEui64.m8[3],
        aQuery.mEui64.m8[4], aQuery.mEui64.m8[5], aQuery.mEui64.m8[6], aQuery.mEui64.m8[7]);

exit:
    close(aQuery.mPipeFd);
    aQuery.mPipeFd = -1;
}

void QueryRcpEui64Parallel(std::vector<RcpQuery> &aQueries, double aTimeoutSeconds)
{
    // 1. Spawn child processes for all queries in parallel
    for (auto &query : aQueries)
    {
        int pipeFds[2];
        if (pipe(pipeFds) < 0)
        {
            query.mPipeFd = -1;
            continue;
        }

        if (pipeFds[0] >= FD_SETSIZE)
        {
            close(pipeFds[0]);
            close(pipeFds[1]);
            query.mPipeFd = -1;
            continue;
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            close(pipeFds[0]);
            close(pipeFds[1]);
            query.mPipeFd = -1;
            continue;
        }

        if (pid == 0) // Child process
        {
            setpgid(0, 0);
            close(pipeFds[0]); // Close read end
            ExecuteDeviceQueryChildWorker(query, pipeFds[1]);
        }

        // Parent process
        setpgid(pid, pid);
        close(pipeFds[1]);                      // Close write end
        fcntl(pipeFds[0], F_SETFL, O_NONBLOCK); // Set read end to non-blocking
        query.mPid     = pid;
        query.mPipeFd  = pipeFds[0];
        query.mSuccess = false;
        Log("Spawned parallel child PID %d for port/URL: %s", pid,
            !query.mPort.empty() ? query.mPort.c_str() : query.mUrl.c_str());
    }

    // 2. Monitor all pipes in parallel using select() multiplexing
    double         remainingTime = aTimeoutSeconds;
    struct timeval start, now;
    gettimeofday(&start, nullptr);

    while (remainingTime > 0)
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        int maxFd       = -1;
        int activeCount = 0;

        for (const auto &query : aQueries)
        {
            if (query.mPipeFd >= 0 && !query.mSuccess)
            {
                FD_SET(query.mPipeFd, &readSet);
                if (query.mPipeFd > maxFd)
                {
                    maxFd = query.mPipeFd;
                }
                activeCount++;
            }
        }

        if (activeCount == 0)
        {
            break; // All active queries completed successfully
        }

        struct timeval timeout;
        timeout.tv_sec  = static_cast<time_t>(remainingTime);
        timeout.tv_usec = static_cast<suseconds_t>((remainingTime - timeout.tv_sec) * 1000000);

        int selectRval = select(maxFd + 1, &readSet, nullptr, nullptr, &timeout);

        if (selectRval > 0)
        {
            for (auto &query : aQueries)
            {
                if (query.mPipeFd >= 0 && !query.mSuccess && FD_ISSET(query.mPipeFd, &readSet))
                {
                    ProcessPipeResponse(query);
                }
            }
        }
        else if (selectRval == 0)
        {
            break; // Global timeout expired
        }
        else
        {
            if (errno != EINTR)
            {
                Log("select() failed in parallel query: %s", strerror(errno));
                break;
            }
        }

        // Calculate remaining timeout
        gettimeofday(&now, nullptr);
        double elapsed = (now.tv_sec - start.tv_sec) + (now.tv_usec - start.tv_usec) / 1000000.0;
        remainingTime  = aTimeoutSeconds - elapsed;
    }

    // 3. Clean up, kill, and reap any remaining active queries (timed out)
    for (auto &query : aQueries)
    {
        if (query.mPipeFd >= 0) // This means the pipe is still open, so it genuinely timed out!
        {
            Log("Query timed out for %s. Killing child process group %d", query.mUrl.c_str(), query.mPid);
            if (query.mPid > 0)
            {
                kill(-query.mPid, SIGKILL);
            }
            close(query.mPipeFd);
            query.mPipeFd = -1;
        }

        if (query.mPid > 0)
        {
            int status;
            if (waitpid(query.mPid, &status, WNOHANG) == 0)
            {
                // Child is still running (e.g. hung during deinit)
                kill(-query.mPid, SIGKILL);
                waitpid(query.mPid, &status, 0);
            }
            query.mPid = -1;
        }
    }
}

bool QueryRcpEui64(const std::string &aUrl, otExtAddress &aEui64)
{
    bool                  rval = false;
    std::vector<RcpQuery> queries;
    RcpQuery              query;

    query.mUrl     = aUrl;
    query.mPid     = -1;
    query.mPipeFd  = -1;
    query.mSuccess = false;

    queries.push_back(query);

    QueryRcpEui64Parallel(queries, 5.0);

    VerifyOrExit(queries[0].mSuccess);

    memcpy(aEui64.m8, queries[0].mEui64.m8, sizeof(aEui64.m8));
    rval = true;

exit:
    return rval;
}

std::vector<RadioUrlConfig> ReadRadioUrlConfig(void)
{
    std::vector<RadioUrlConfig> configs;
    std::string                 configPath = GetWiresharkConfigFilePath("openthread_radio_urls");
    std::ifstream               configFile(configPath);

    Log("Reading config file: %s", configPath.c_str());

    if (!configFile.is_open())
    {
        Log("Config file not found or could not be opened");
        return configs;
    }

    std::string line;
    while (std::getline(configFile, line))
    {
        // Strip comments
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos)
        {
            line = line.substr(0, commentPos);
        }

        // Trim whitespace
        Trim(line);

        if (line.empty())
        {
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos)
        {
            Log("Invalid config line (missing '='): %s", line.c_str());
            continue;
        }

        std::string key = line.substr(0, eqPos);
        std::string url = line.substr(eqPos + 1);

        // Trim key and url
        Trim(key);
        Trim(url);

        uint64_t id;
        if (!ParseId(key, id))
        {
            Log("Invalid nodeid (not a number): %s", key.c_str());
            continue;
        }

        RadioUrlConfig cfg;
        cfg.mId  = id;
        cfg.mUrl = url;

        Log("Parsed config entry: id=0x%" PRIx64 ", url='%s'", cfg.mId, cfg.mUrl.c_str());
        configs.push_back(cfg);
    }
    return configs;
}

bool UpdateConfigEntry(uint64_t aId, const std::string &aUrl, bool *aIsNew = nullptr)
{
    bool                     isNew = true;
    std::string              configPath;
    std::string              configDir;
    std::string              lockPath;
    std::ifstream            configFile;
    std::vector<std::string> outputLines;
    std::string              line;
    bool                     processed = false;
    char                     keyHex[20];
    std::ofstream            configFileOut;
    bool                     success    = false;
    int                      lockFd     = -1;
    size_t                   lastSlash  = std::string::npos;
    bool                     fileExists = false;

    snprintf(keyHex, sizeof(keyHex), "0x%" PRIx64, aId);

    configPath = GetWiresharkConfigFilePath("openthread_radio_urls");
    VerifyOrExit(!configPath.empty(), success = false);

    lastSlash = configPath.find_last_of("/\\");
    if (lastSlash != std::string::npos)
    {
        configDir = configPath.substr(0, lastSlash);
    }

    // Ensure config directory exists before opening lock file or config file
    if (!configDir.empty())
    {
        size_t pos = 0;
        while ((pos = configDir.find('/', pos + 1)) != std::string::npos)
        {
            std::string subdir = configDir.substr(0, pos);
            if (!subdir.empty())
            {
                mkdir(subdir.c_str(), 0755);
            }
        }
        mkdir(configDir.c_str(), 0755);
    }

    lockPath = GetWiresharkLockFilePath("openthread_radio_urls");
    if (!lockPath.empty())
    {
        lockFd = open(lockPath.c_str(), O_RDWR | O_CREAT, 0666);
        if (lockFd >= 0)
        {
            flock(lockFd, LOCK_EX);
        }
    }

    fileExists = (access(configPath.c_str(), F_OK) == 0);
    configFile.open(configPath);

    if (!configFile.is_open() && fileExists)
    {
        Log("Failed to open existing config file for reading: %s", configPath.c_str());
        ExitNow();
    }

    if (configFile.is_open())
    {
        while (std::getline(configFile, line))
        {
            std::string rawLine    = line;
            size_t      commentPos = line.find('#');

            if (commentPos != std::string::npos)
            {
                line = line.substr(0, commentPos);
            }

            Trim(line);

            if (line.empty())
            {
                outputLines.push_back(rawLine);
                continue;
            }

            size_t eqPos = line.find('=');

            if (eqPos != std::string::npos)
            {
                std::string key = line.substr(0, eqPos);

                Trim(key);

                uint64_t id;

                if (ParseId(key, id) && id == aId)
                {
                    // Match found! Update URL, preserving comment
                    std::string commentPart = (commentPos != std::string::npos) ? rawLine.substr(commentPos) : "";

                    rawLine = std::string(keyHex) + " = " + aUrl;

                    if (!commentPart.empty())
                    {
                        rawLine += " " + commentPart;
                    }

                    processed = true;
                    isNew     = false;
                    Log("Updating config entry for 0x%" PRIx64 ": %s", aId, aUrl.c_str());
                }
            }

            outputLines.push_back(rawLine);
        }

        configFile.close();
    }

    if (!processed)
    {
        // Append new entry
        std::string newLine = std::string(keyHex) + " = " + aUrl;

        outputLines.push_back(newLine);
        Log("Appending new config entry 0x%" PRIx64 ": %s", aId, aUrl.c_str());
    }

    // Write back
    configFileOut.open(configPath);
    VerifyOrExit(configFileOut.is_open(), Log("Failed to open config file for writing: %s", configPath.c_str()));

    for (const auto &outLine : outputLines)
    {
        configFileOut << outLine << "\n";
    }

    configFileOut.close();
    success = true;

exit:
    if (lockFd >= 0)
    {
        flock(lockFd, LOCK_UN);
        close(lockFd);
    }

    if (success && aIsNew != nullptr)
    {
        *aIsNew = isNew;
    }
    return success;
}

void SignalHandler(int aSignal)
{
    if (aSignal == SIGINT || aSignal == SIGTERM)
    {
        sShouldQuit = true;
    }
}

void HandlePcapCallback(const otRadioFrame *aFrame, bool aIsTx, void *aContext)
{
    uint64_t timeUs;

    OT_UNUSED_VARIABLE(aContext);

    if (aFrame != nullptr && sPcapngWriter.IsOpen())
    {
        timeUs = aIsTx ? aFrame->mInfo.mTxInfo.mTimestamp : aFrame->mInfo.mRxInfo.mTimestamp;

        otError error = sPcapngWriter.WriteFrame(*aFrame, aIsTx, timeUs);

        if (error != OT_ERROR_NONE)
        {
            if (error == OT_ERROR_FAILED)
            {
                // If the write failed due to EPIPE (broken pipe), it means Wireshark
                // has closed the pipe to stop the capture. We should handle this silently.
                if (errno != EPIPE)
                {
                    syslog(LOG_ERR, "Failed to write frame to PCAPNG: %s", strerror(errno));
                }
            }
            else
            {
                syslog(LOG_ERR, "Failed to write frame to PCAPNG: %s", otThreadErrorToString(error));
            }

            sShouldQuit = true;
        }
    }
}

void ListInterfaces(void)
{
    printf("extcap {version=1.0.0}{display=OpenThread Sniffer}{help=https://openthread.io}\n");

    // Read custom configs and list them instantly!
    std::vector<RadioUrlConfig> customConfigs = ReadRadioUrlConfig();
    for (const auto &cfg : customConfigs)
    {
        char idHex[20];
        snprintf(idHex, sizeof(idHex), "0x%" PRIx64, cfg.mId);
        printf("interface {value=spinel_%s}{display=OpenThread Sniffer %s}\n", idHex, idHex);
    }

    printf("interface {value=spinel_hdlc_uart}{display=OpenThread Sniffer over UART}\n");
    printf("interface {value=spinel_radio_url}{display=OpenThread Sniffer over Radio URL...}\n");
}

void DetectSerialPorts(std::vector<RcpQuery> &aQueries)
{
    glob_t globResult;

    GlobSerialPorts(globResult);

    Log("Scanning %d physical serial port(s) for OpenThread sniffer devices...", static_cast<int>(globResult.gl_pathc));
    for (size_t i = 0; i < globResult.gl_pathc; ++i)
    {
        const char *port = globResult.gl_pathv[i];
        RcpQuery    query;

        Log("Discovered serial port [%d]: %s", static_cast<int>(i), port);

        query.mPort    = port;
        query.mUrl     = "";
        query.mPid     = -1;
        query.mPipeFd  = -1;
        query.mSuccess = false;
        aQueries.push_back(query);
    }

    if (!aQueries.empty())
    {
        Log("Testing %d physical port(s) in parallel across baud rates (460800, 115200)...",
            static_cast<int>(aQueries.size()));
        QueryRcpEui64Parallel(aQueries, 5.0);

        for (const auto &query : aQueries)
        {
            if (query.mSuccess)
            {
                Log("Discovered Sniffer: Port=%s, URL=%s, EUI-64=%02x%02x%02x%02x%02x%02x%02x%02x", query.mPort.c_str(),
                    query.mUrl.c_str(), query.mEui64.m8[0], query.mEui64.m8[1], query.mEui64.m8[2], query.mEui64.m8[3],
                    query.mEui64.m8[4], query.mEui64.m8[5], query.mEui64.m8[6], query.mEui64.m8[7]);
            }
            else
            {
                Log("No response from port %s", query.mPort.c_str());
            }
        }
    }

    globfree(&globResult);
}

void DetectInterfaces(void)
{
    std::vector<RcpQuery> queries;
    int                   newCount     = 0;
    int                   updatedCount = 0;
    std::string           configPath;

    Log("Starting manual interface detection");
    printf("Scanning physical serial ports for OpenThread sniffer devices...\n");

    DetectSerialPorts(queries);

    VerifyOrExit(!queries.empty(), {
        Log("No local USB serial ports found to scan");
        printf("No new devices detected (no serial ports found).\n");
    });

    printf("Processing detection results across %d port/baudrate combinations...\n", static_cast<int>(queries.size()));

    // Persist all successfully detected devices using the shared UpdateConfigEntry helper
    for (const auto &query : queries)
    {
        if (query.mSuccess)
        {
            uint64_t devId = Eui64ToUint64(query.mEui64);
            char     euiStr[32];
            snprintf(euiStr, sizeof(euiStr), "%02x%02x%02x%02x%02x%02x%02x%02x", query.mEui64.m8[0], query.mEui64.m8[1],
                     query.mEui64.m8[2], query.mEui64.m8[3], query.mEui64.m8[4], query.mEui64.m8[5], query.mEui64.m8[6],
                     query.mEui64.m8[7]);

            Log("Found sniffer device: ID=0x%" PRIx64 " (%s), URL=%s", devId, euiStr, query.mUrl.c_str());

            bool isNew = false;

            if (UpdateConfigEntry(devId, query.mUrl, &isNew))
            {
                if (isNew)
                {
                    printf("  [NEW]     Sniffer 0x%s found at %s -> Added to config.\n", euiStr, query.mUrl.c_str());
                    newCount++;
                }
                else
                {
                    printf("  [UPDATED] Sniffer 0x%s found at %s -> Updated in config.\n", euiStr, query.mUrl.c_str());
                    updatedCount++;
                }
            }
            else
            {
                printf("  [ERROR]   Failed to update config for Sniffer 0x%s.\n", euiStr);
            }
        }
    }

    configPath = GetWiresharkConfigFilePath("openthread_radio_urls");
    printf("Detection completed. Updated config file: %s\n", configPath.c_str());
    printf("Successfully detected and updated %d devices (%d new, %d updated).\n", newCount + updatedCount, newCount,
           updatedCount);

exit:
    return;
}

void ListConfig(const char *aInterface)
{
    printf("arg {number=0}{call=--channel}{display=Channel}{tooltip=IEEE 802.15.4 "
           "channel}{type=selector}{required=true}{default=11}\n");
    // TODO: Retrieve the supported channel mask from the sniffer.
    for (int i = 11; i <= 26; ++i)
    {
        printf("value {arg=0}{value=%d}{display=%d}{default=%s}\n", i, i, (i == 11 ? "true" : "false"));
    }

    // Expose a checkbox for verbose debugging in the Wireshark GUI
    printf("arg {number=1}{call=--debug}{display=Verbose Debugging}{tooltip=Enable verbose debug "
           "logging}{type=boolflag}{default=false}\n");

    // If the special custom UART interface is selected, dynamically expose the Radio URL selector input
    // If the special custom generic interface is selected, dynamically expose the free-form text input
    if (aInterface != nullptr)
    {
        if (strcmp(aInterface, "spinel_hdlc_uart") == 0)
        {
            printf("arg {number=2}{call=--spinel-hdlc-uart}{display=Sniffer}{tooltip=Select an existing Sniffer or "
                   "click reload to scan for new USB devices}{type=selector}{reload=true}{placeholder=Select or "
                   "reload...}{required=true}\n");

            std::vector<RadioUrlConfig> customConfigs;

            customConfigs = ReadRadioUrlConfig();

            for (const auto &cfg : customConfigs)
            {
                char idHex[20];
                snprintf(idHex, sizeof(idHex), "0x%" PRIx64, cfg.mId);
                printf("value {arg=2}{value=%s}{display=OpenThread Sniffer %s}\n", cfg.mUrl.c_str(), idHex);
            }

            if (customConfigs.empty())
            {
                printf("value {arg=2}{value=}{display=Click reload button to scan for devices...}{default=true}\n");
            }
        }
        else if (strcmp(aInterface, "spinel_radio_url") == 0 || strcmp(aInterface, "radio_url") == 0)
        {
            printf("arg {number=2}{call=--radio-url}{display=Radio URL}{tooltip=Enter full Radio URL, e.g. "
                   "spinel+hdlc+fork://[path_to_rcp] or spinel+hdlc+uart://[port]}{type=string}{required=true}\n");
        }
    }
}

void ReloadRadioUrls(void)
{
    std::vector<RadioUrlConfig> customConfigs;
    std::vector<RcpQuery>       queries;

    Log("Handling reload for radio-url");

    // 1. Print already configured URLs from config file first
    customConfigs = ReadRadioUrlConfig();

    for (const auto &cfg : customConfigs)
    {
        char idHex[20];
        snprintf(idHex, sizeof(idHex), "0x%" PRIx64, cfg.mId);
        printf("value {arg=2}{value=%s}{display=OpenThread Sniffer %s}\n", cfg.mUrl.c_str(), idHex);
    }

    // 2. Scan and detect physical UART devices in parallel at 460800 and 115200
    DetectSerialPorts(queries);

    for (const auto &query : queries)
    {
        if (query.mSuccess)
        {
            uint64_t devId = Eui64ToUint64(query.mEui64);

            Log("Reload found active sniffer device: Port=%s, URL=%s, EUI-64=%02x%02x%02x%02x%02x%02x%02x%02x",
                query.mPort.c_str(), query.mUrl.c_str(), query.mEui64.m8[0], query.mEui64.m8[1], query.mEui64.m8[2],
                query.mEui64.m8[3], query.mEui64.m8[4], query.mEui64.m8[5], query.mEui64.m8[6], query.mEui64.m8[7]);

            // Persist newly discovered devices to config during reload
            UpdateConfigEntry(devId, query.mUrl);

            // Avoid duplicate options if the device was already printed in step 1
            bool alreadyConfigured = false;

            for (const auto &cfg : customConfigs)
            {
                if (cfg.mId == devId)
                {
                    alreadyConfigured = true;
                    break;
                }
            }

            if (!alreadyConfigured)
            {
                char euiStr[32];
                char displayName[128];

                snprintf(euiStr, sizeof(euiStr), "%02x%02x%02x%02x%02x%02x%02x%02x", query.mEui64.m8[0],
                         query.mEui64.m8[1], query.mEui64.m8[2], query.mEui64.m8[3], query.mEui64.m8[4],
                         query.mEui64.m8[5], query.mEui64.m8[6], query.mEui64.m8[7]);

                snprintf(displayName, sizeof(displayName), "OpenThread Sniffer 0x%s (%s)", euiStr, query.mPort.c_str());

                printf("value {arg=2}{value=%s}{display=%s}\n", query.mUrl.c_str(), displayName);
            }
        }
    }
}

void ListDlts(void)
{
    printf("dlt {number=283}{name=IEEE802_15_4_TAP}{display=IEEE 802.15.4 TAP}\n");
}

std::string ResolveInterfaceToRadioUrl(const char *aInterface, const char *aSpinelHdlcUart, const char *aRadioUrl)
{
    std::string resolvedUrl;
    std::string interfaceVal = aInterface ? aInterface : "";
    uint64_t    targetId     = 0;

    if (interfaceVal == "spinel_hdlc_uart")
    {
        VerifyOrExit(aSpinelHdlcUart != nullptr && aSpinelHdlcUart[0] != '\0',
                     { LogAndPrint("Error: Spinel HDLC UART URL must be provided for custom interface"); });

        resolvedUrl = aSpinelHdlcUart;
    }
    else if (interfaceVal == "spinel_radio_url")
    {
        VerifyOrExit(aRadioUrl != nullptr && aRadioUrl[0] != '\0',
                     { LogAndPrint("Error: Radio URL must be provided for custom interface"); });

        resolvedUrl = aRadioUrl;
    }
    else if (ParseId(interfaceVal.substr(sizeof("spinel_") - 1), targetId))
    {
        // Check if it matches a key (by numeric ID) in the custom configs
        std::vector<RadioUrlConfig> customConfigs = ReadRadioUrlConfig();
        for (const auto &cfg : customConfigs)
        {
            if (cfg.mId == targetId)
            {
                resolvedUrl = cfg.mUrl;
                Log("Matched config entry by ID 0x%" PRIx64 " to URL: %s", targetId, resolvedUrl.c_str());
                break;
            }
        }

        // If not found in config, check if the interface value can be parsed as a 64-bit ID (like an EUI-64)
        std::vector<RcpQuery> queries;

        Log("Interface value parsed as 64-bit ID: 0x%" PRIx64 ", scanning serial ports to resolve EUI-64", targetId);

        DetectSerialPorts(queries);

        for (const auto &query : queries)
        {
            if (query.mSuccess && Eui64ToUint64(query.mEui64) == targetId)
            {
                resolvedUrl = query.mUrl;
                Log("SUCCESS: Resolved EUI-64 0x%" PRIx64 " to port %s (URL: %s)", targetId, query.mPort.c_str(),
                    resolvedUrl.c_str());
                break;
            }
        }
    }
    else if (interfaceVal.compare(0, sizeof("spinel+") - 1,
                                  "spinel+") == 0) // Treat the interface value itself as the Radio URL
    {
        resolvedUrl = interfaceVal;
    }

exit:
    return resolvedUrl;
}

int RunCapture(const char *aInterface,
               const char *aFifo,
               uint8_t     aChannel,
               const char *aSpinelHdlcUart,
               const char *aRadioUrl)
{
    int              rval     = EXIT_FAILURE;
    otInstance      *instance = nullptr;
    otPlatformConfig config;
    std::string      radioUrl;
    struct sigaction sa;
    struct sigaction saPipe;

    Log("Starting capture phase");
    VerifyOrExit(aChannel >= 11 && aChannel <= 26,
                 { LogAndPrint("Invalid channel %d. Channel must be in the range 11-26.", aChannel); });
    VerifyOrExit(aFifo != nullptr, { LogAndPrint("The fifo must be provided to capture"); });

    // Setup Signal Handlers
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    memset(&saPipe, 0, sizeof(saPipe));
    saPipe.sa_handler = SIG_IGN;
    sigemptyset(&saPipe.sa_mask);
    saPipe.sa_flags = 0;
    sigaction(SIGPIPE, &saPipe, nullptr);

    // 1. Resolve the interface value to a full Radio URL
    radioUrl = ResolveInterfaceToRadioUrl(aInterface, aSpinelHdlcUart, aRadioUrl);
    VerifyOrExit(!radioUrl.empty());

    // 2. Open PCAPNG Writer (blocks until Wireshark opens the FIFO for reading)
    Log("Opening FIFO: %s (blocking until Wireshark opens it)", aFifo);
    VerifyOrExit(sPcapngWriter.Open(aFifo) == OT_ERROR_NONE,
                 { LogAndPrint("Failed to open FIFO for writing: %s", aFifo); });
    Log("FIFO opened successfully");

    // 3. Initialize OpenThread POSIX Platform
    Log("Initializing OpenThread POSIX platform with URL: %s", radioUrl.c_str());
    memset(&config, 0, sizeof(config));
    config.mCoprocessorUrls.mUrls[0] = radioUrl.c_str();
    config.mCoprocessorUrls.mNum     = 1;
    config.mSpeedUpFactor            = 1;

    if (sDebugEnabled)
    {
        IgnoreError(otLoggingSetLevel(OT_LOG_LEVEL_DEBG));
    }

    config.mCoprocessorType = otSysInitCoprocessor(&config.mCoprocessorUrls);
    Log("Coprocessor type: %d", config.mCoprocessorType);
    VerifyOrExit(config.mCoprocessorType == OT_COPROCESSOR_RCP, { LogAndPrint("Coprocessor must be RCP"); });

    instance = otSysInit(&config);
    VerifyOrExit(instance != nullptr, { LogAndPrint("Failed to initialize OpenThread instance"); });
    Log("OpenThread instance initialized successfully");

    // 4. Configure Radio using Standard Link API
    // Register PCAP callback to capture all MAC frames (RX and TX)
    otLinkSetPcapCallback(instance, HandlePcapCallback, nullptr);

    // Enable Link layer (enables MAC and Radio, but keeps interface down)
    {
        otError err = otLinkSetEnabled(instance, true);
        VerifyOrExit(err == OT_ERROR_NONE,
                     { LogAndPrint("Failed to enable Link layer: %s", otThreadErrorToString(err)); });
        Log("Link layer enabled");
    }

    // Set Channel
    {
        otError err = otLinkSetChannel(instance, aChannel);
        VerifyOrExit(err == OT_ERROR_NONE,
                     { LogAndPrint("Failed to set channel to %d: %s", aChannel, otThreadErrorToString(err)); });
        Log("Channel set to %d", aChannel);
    }

    // Enable Promiscuous mode to receive all packets
    {
        otError err = otLinkSetPromiscuous(instance, true);
        VerifyOrExit(err == OT_ERROR_NONE,
                     { LogAndPrint("Failed to enable promiscuous mode: %s", otThreadErrorToString(err)); });
        Log("Promiscuous mode enabled. Starting capture loop...");
    }

    rval = EXIT_SUCCESS;

    // 5. Main Loop
    while (!sShouldQuit)
    {
        otSysMainloopContext mainloopContext;

        mainloopContext.mMaxFd = -1;
        FD_ZERO(&mainloopContext.mReadFdSet);
        FD_ZERO(&mainloopContext.mWriteFdSet);
        FD_ZERO(&mainloopContext.mErrorFdSet);
        mainloopContext.mTimeout.tv_sec  = 10;
        mainloopContext.mTimeout.tv_usec = 0;

        otSysMainloopUpdate(instance, &mainloopContext);

        int selectRval = otSysMainloopPoll(&mainloopContext);

        if (selectRval >= 0)
        {
            otSysMainloopProcess(instance, &mainloopContext);
        }
        else if (errno != EINTR)
        {
            LogAndPrint("select() failed in main loop: %s", strerror(errno));
            rval = EXIT_FAILURE;
            break;
        }
    }

    Log("Exited main loop. shouldQuit=%d", sShouldQuit.load());

    // Cleanup
    IgnoreError(otLinkSetPromiscuous(instance, false));
    IgnoreError(otLinkSetEnabled(instance, false));
    otLinkSetPcapCallback(instance, nullptr, nullptr);

exit:
    sPcapngWriter.Close();
    if (instance != nullptr)
    {
        otInstanceFinalize(instance);
        otSysDeinit();
        Log("OpenThread instance finalized");
    }

    return rval;
}

void PrintHelp(void)
{
    printf("OpenThread Wireshark Extcap Sniffer (ot-extcap)\n\n");
    printf("Usage:\n");
    printf("  ot-extcap [options]\n\n");
    printf("Options:\n");
    printf("  --capture                       Start live packet capture\n");
    printf("  --channel <ch>                  IEEE 802.15.4 channel to capture (range: 11-26, default: 11)\n");
    printf("  --debug                         Enable verbose debug logging to syslog (LOG_DEBUG)\n");
    printf("  --detect-interfaces             Scan and detect connected physical UART sniffer devices\n");
    printf("  --extcap-config                 List configuration options for a selected interface\n");
    printf("  --extcap-dlts                   List DLTs (Data Link Types) for a selected interface\n");
    printf("  --extcap-interface <interface>  Specify the interface for capture or configuration\n");
    printf("  --extcap-interfaces             List available OpenThread sniffer interfaces\n");
    printf("  --extcap-reload-option <arg>    Reload dynamic selector options (e.g. spinel_hdlc_uart)\n");
    printf("  --extcap-version <version>      Specify the Wireshark version string\n");
    printf("  --fifo <path>                   Specify the FIFO pipe for dumping captured PCAPNG packets\n");
    printf("  --help                          Display this help message\n");
    printf("  --radio-url <url>               Specify the Radio URL for generic custom interface\n");
    printf("  --spinel-hdlc-uart <url>        Specify the Radio URL for UART sniffer interface\n");
}

} // namespace

void otPlatReset(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    syslog(LOG_WARNING, "OpenThread instance requested reset, exiting");
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    int rval = EXIT_SUCCESS;

    enum OptionShort
    {
        OT_EXTCAP_OPT_CAPTURE              = 's',
        OT_EXTCAP_OPT_CHANNEL              = 'h',
        OT_EXTCAP_OPT_DEBUG                = 'd',
        OT_EXTCAP_OPT_DETECT_INTERFACES    = 'x',
        OT_EXTCAP_OPT_EXTCAP_CONFIG        = 'c',
        OT_EXTCAP_OPT_EXTCAP_DLTS          = 'D',
        OT_EXTCAP_OPT_EXTCAP_INTERFACE     = 'g',
        OT_EXTCAP_OPT_EXTCAP_INTERFACES    = 'i',
        OT_EXTCAP_OPT_EXTCAP_RELOAD_OPTION = 'r',
        OT_EXTCAP_OPT_EXTCAP_VERSION       = 'v',
        OT_EXTCAP_OPT_FIFO                 = 'f',
        OT_EXTCAP_OPT_HELP                 = 'H',
        OT_EXTCAP_OPT_RADIO_URL            = 'U',
        OT_EXTCAP_OPT_SPINEL_HDLC_UART     = 'u',
    };

    static const struct option sLongOptions[] = {
        {"capture", no_argument, nullptr, OT_EXTCAP_OPT_CAPTURE},
        {"channel", required_argument, nullptr, OT_EXTCAP_OPT_CHANNEL},
        {"debug", no_argument, nullptr, OT_EXTCAP_OPT_DEBUG},
        {"detect-interfaces", no_argument, nullptr, OT_EXTCAP_OPT_DETECT_INTERFACES},
        {"extcap-config", no_argument, nullptr, OT_EXTCAP_OPT_EXTCAP_CONFIG},
        {"extcap-dlts", no_argument, nullptr, OT_EXTCAP_OPT_EXTCAP_DLTS},
        {"extcap-interface", required_argument, nullptr, OT_EXTCAP_OPT_EXTCAP_INTERFACE},
        {"extcap-interfaces", no_argument, nullptr, OT_EXTCAP_OPT_EXTCAP_INTERFACES},
        {"extcap-reload-option", required_argument, nullptr, OT_EXTCAP_OPT_EXTCAP_RELOAD_OPTION},
        {"extcap-version", required_argument, nullptr, OT_EXTCAP_OPT_EXTCAP_VERSION},
        {"fifo", required_argument, nullptr, OT_EXTCAP_OPT_FIFO},
        {"help", no_argument, nullptr, OT_EXTCAP_OPT_HELP},
        {"radio-url", required_argument, nullptr, OT_EXTCAP_OPT_RADIO_URL},
        {"spinel-hdlc-uart", required_argument, nullptr, OT_EXTCAP_OPT_SPINEL_HDLC_UART},
        {nullptr, 0, nullptr, 0},
    };

    bool        optInterfaces     = false;
    bool        optConfig         = false;
    bool        optDlts           = false;
    bool        optCapture        = false;
    bool        optDetect         = false;
    bool        optHelp           = false;
    const char *optInterface      = nullptr;
    const char *optFifo           = nullptr;
    const char *optSpinelHdlcUart = nullptr;
    const char *optRadioUrl       = nullptr;
    const char *optReloadOption   = nullptr;
    uint8_t     optChannel        = 11;

    for (int c; (c = getopt_long(argc, argv, "cdDf:g:h:Hir:su:U:v:x", sLongOptions, nullptr)) != -1;)
    {
        switch (c)
        {
        case OT_EXTCAP_OPT_CAPTURE:
            optCapture = true;
            break;
        case OT_EXTCAP_OPT_CHANNEL:
        {
            int val = atoi(optarg);
            if (val < 11 || val > 26)
            {
                fprintf(stderr, "Invalid channel %s. Channel must be in the range 11-26.", optarg);
                exit(EXIT_FAILURE);
            }
            optChannel = static_cast<uint8_t>(val);
            break;
        }
        case OT_EXTCAP_OPT_DEBUG:
            sDebugEnabled = true;
            break;
        case OT_EXTCAP_OPT_DETECT_INTERFACES:
            optDetect = true;
            break;
        case OT_EXTCAP_OPT_EXTCAP_CONFIG:
            optConfig = true;
            break;
        case OT_EXTCAP_OPT_EXTCAP_DLTS:
            optDlts = true;
            break;
        case OT_EXTCAP_OPT_EXTCAP_INTERFACE:
            optInterface = optarg;
            break;
        case OT_EXTCAP_OPT_EXTCAP_INTERFACES:
            optInterfaces = true;
            break;
        case OT_EXTCAP_OPT_EXTCAP_RELOAD_OPTION:
            optReloadOption = optarg;
            break;
        case OT_EXTCAP_OPT_EXTCAP_VERSION:
            if (optarg != nullptr)
            {
                int major = 0;
                int minor = 0;

                if (sscanf(optarg, "%d.%d", &major, &minor) >= 1)
                {
                    if (major < 3)
                    {
                        fprintf(stderr, "Unsupported Wireshark version %s. Minimum required version is 3.0.\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
            }
            break;
        case OT_EXTCAP_OPT_FIFO:
            optFifo = optarg;
            break;
        case OT_EXTCAP_OPT_HELP:
            optHelp = true;
            break;
        case OT_EXTCAP_OPT_RADIO_URL:
            optRadioUrl = optarg;
            break;
        case OT_EXTCAP_OPT_SPINEL_HDLC_UART:
            optSpinelHdlcUart = optarg;
            break;
        default:
            fprintf(stderr, "Invalid option\n");
            exit(EXIT_FAILURE);
        }
    }

    InitLogging();

    Log("Command line options parsed. Interfaces=%d, Detect=%d, Config=%d, Dlts=%d, Capture=%d, Interface='%s', "
        "Fifo='%s', Channel=%d, SpinelHdlcUart='%s', RadioUrl='%s', ReloadOption='%s', Debug=%d",
        optInterfaces, optDetect, optConfig, optDlts, optCapture, optInterface ? optInterface : "",
        optFifo ? optFifo : "", optChannel, optSpinelHdlcUart ? optSpinelHdlcUart : "", optRadioUrl ? optRadioUrl : "",
        optReloadOption ? optReloadOption : "", sDebugEnabled);

    if (optHelp || argc == 1)
    {
        PrintHelp();
        rval = EXIT_SUCCESS;
        ExitNow();
    }

    if (optInterfaces)
    {
        ListInterfaces();
        rval = EXIT_SUCCESS;
        ExitNow();
    }

    if (optDetect)
    {
        DetectInterfaces();
        rval = EXIT_SUCCESS;
        ExitNow();
    }

    // Intercept and handle dynamic option reload BEFORE normal configuration is printed
    if (optReloadOption != nullptr)
    {
        if (strcmp(optReloadOption, "spinel-hdlc-uart") == 0)
        {
            ReloadRadioUrls();
        }
        rval = EXIT_SUCCESS;
        ExitNow();
    }

    VerifyOrExit(optInterface != nullptr, {
        LogAndPrint("Error: An interface must be provided");
        rval = EXIT_FAILURE;
    });

    if (optConfig)
    {
        Log("Listing config options for interface: %s", optInterface ? optInterface : "none");
        ListConfig(optInterface);
        rval = EXIT_SUCCESS;
        ExitNow();
    }

    if (optDlts)
    {
        Log("Listing DLTs");
        ListDlts();
        rval = EXIT_SUCCESS;
        ExitNow();
    }

    if (optCapture)
    {
        rval = RunCapture(optInterface, optFifo, optChannel, optSpinelHdlcUart, optRadioUrl);
    }

exit:
    Log("Performing final cleanups");
    closelog();
    return rval;
}
