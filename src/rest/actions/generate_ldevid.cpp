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

#include "generate_ldevid.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <unistd.h>

#include "common/code_utils.hpp"
#include "rest/json.hpp"
#include "rest/services.hpp"

#include <cJSON.h>

#include "common/logging.hpp"

#if OTBR_ENABLE_MDNS_OPENTHREAD
#include <openthread/mdns.h>
#endif

// Traefik certificate paths (matches cgi-bin/paths.env)
#define TRAEFIK_CERTS_DIR "/var/lib/traefik/certs"
#define IDEVID_CERT_PATH TRAEFIK_CERTS_DIR "/idevid-chain.pem"
#define LDEVID_KEY_PATH TRAEFIK_CERTS_DIR "/ldevid-key.pem"
#define LDEVID_CERT_PATH TRAEFIK_CERTS_DIR "/ldevid-chain.pem"

namespace otbr {
namespace rest {
namespace actions {

GenerateLDevID::GenerateLDevID(const cJSON &aJson, Services &aServices)
    : BasicActions(aJson, aServices)
    , mCurrentDateTime{nullptr}
    , mCertGenerated{false}
{
    cJSON *value;

    // Extract currentDateTime from attributes if present (optional)
    value = cJSON_GetObjectItemCaseSensitive(mJson, KEY_CURRENT_DATETIME);
    if (value != nullptr && cJSON_IsString(value))
    {
        mCurrentDateTime = value->valuestring;
    }

    mStatus = kActionStatusPending;
    Update();

    return;
}

GenerateLDevID::~GenerateLDevID()
{
    Stop();
}

std::string GenerateLDevID::GetTypeName() const
{
    return std::string(GENERATE_LDEVID_ACTION_TYPE_NAME);
}

void GenerateLDevID::Update(void)
{
    otError error;

    switch (mStatus)
    {
    case kActionStatusPending:
        mStatus = kActionStatusActive;
        OT_FALL_THROUGH;

    case kActionStatusActive:
        if (!mCertGenerated)
        {
            error = GenerateCertificate();
            if (error == OT_ERROR_NONE)
            {
                mCertGenerated = true;
                mStatus        = kActionStatusCompleted;
            }
            else
            {
                otbrLogErr("Failed to generate LDevID certificate: %d", error);
                mStatus = kActionStatusFailed;
            }
        }
        break;

    default:
        break;
    }
}

void GenerateLDevID::Stop(void)
{
    switch (mStatus)
    {
    case kActionStatusPending:
    case kActionStatusActive:
        mStatus = kActionStatusStopped;
        break;

    default:
        break;
    }
}

otError GenerateLDevID::GenerateCertificate(void)
{
    otError error = OT_ERROR_NONE;
    char    cmd[2048];
    char    serialNumber[256] = {0};

    // Note: currentDateTime parameter is intended for constrained servers without NTP/time sync.
    // Implementation note: mCurrentDateTime could be used to set system time (e.g., via settimeofday)
    // before certificate generation to ensure valid notBefore/notAfter timestamps when system clock is incorrect.
    if (mCurrentDateTime != nullptr)
    {
        otbrLogInfo("Generating LDevID certificate (currentDateTime: %s provided but not used, cert uses system time)",
                    mCurrentDateTime);
    }
    else
    {
        otbrLogInfo("Generating LDevID certificate (using system time)");
    }

    // Extract serial number from IDevID certificate (optional)
    FILE *fp = popen("openssl x509 -in " IDEVID_CERT_PATH " -noout -subject 2>/dev/null | "
                     "sed -n 's/.*serialNumber = \\([^,]*\\).*/\\1/p'",
                     "r");
    if (fp != nullptr)
    {
        if (fgets(serialNumber, sizeof(serialNumber), fp) != nullptr)
        {
            // Remove trailing newline
            size_t len = strlen(serialNumber);
            if (len > 0 && serialNumber[len - 1] == '\n')
            {
                serialNumber[len - 1] = '\0';
            }
            otbrLogInfo("Extracted serial number from IDevID: %s", serialNumber);
        }
        pclose(fp);
    }

    // Use placeholder if no serial number found in IDevID
    if (serialNumber[0] == '\0')
    {
        strncpy(serialNumber, "not available", sizeof(serialNumber) - 1);
        otbrLogInfo("No serial number in IDevID, using placeholder: %s", serialNumber);
    }

    char hostname[256];

#if OTBR_ENABLE_MDNS_OPENTHREAD
    // Get mDNS hostname from OpenThread
    // This respects any custom configuration set via otMdnsSetLocalHostName()
    // If not set, OpenThread auto-generates it (typically ot<extaddr>)
    otInstance *instance     = mServices.GetInstance();
    const char *mdnsHostname = otMdnsGetLocalHostName(instance);

    if (mdnsHostname != nullptr && mdnsHostname[0] != '\0')
    {
        strncpy(hostname, mdnsHostname, sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
        otbrLogInfo("Using mDNS hostname: %s.local (from OpenThread mDNS)", hostname);
    }
    else
#endif
    {
        // Use system hostname when OpenThread mDNS is not enabled or hostname is not available
        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            strncpy(hostname, "otbr", sizeof(hostname));
        }
        otbrLogInfo("Using system hostname: %s.local", hostname);
    }

    // Generate EC private key directly to final location
    snprintf(cmd, sizeof(cmd), "openssl ecparam -genkey -name secp256r1 -out %s 2>/dev/null", LDEVID_KEY_PATH);
    if (system(cmd) != 0)
    {
        otbrLogErr("Failed to generate private key");
        ExitNow(error = OT_ERROR_FAILED);
    }

    // Set proper permissions on private key (read/write for owner only)
    snprintf(cmd, sizeof(cmd), "chmod 600 %s && chown traefik:traefik %s", LDEVID_KEY_PATH, LDEVID_KEY_PATH);
    system(cmd);

    // Generate self-signed certificate directly to final location
    snprintf(cmd, sizeof(cmd),
             "openssl req -x509 -new -key %s -out %s "
             "-days 365 -subj '/C=CH/O=OpenThread/serialNumber=%s/CN=%s.local' "
             "-addext 'subjectAltName=DNS:%s.local,DNS:localhost' 2>/dev/null",
             LDEVID_KEY_PATH, LDEVID_CERT_PATH, serialNumber, hostname, hostname);

    if (system(cmd) != 0)
    {
        otbrLogErr("Failed to generate certificate");
        ExitNow(error = OT_ERROR_FAILED);
    }

    // Set proper permissions on certificate (readable by all, owned by traefik)
    snprintf(cmd, sizeof(cmd), "chmod 644 %s && chown traefik:traefik %s", LDEVID_CERT_PATH, LDEVID_CERT_PATH);
    system(cmd);

    otbrLogInfo("Successfully generated LDevID certificate and key");
    otbrLogInfo("  Certificate: %s", LDEVID_CERT_PATH);
    otbrLogInfo("  Private key: %s", LDEVID_KEY_PATH);
    otbrLogInfo("To activate, POST to /api/auth/enroll with empty body or {\"est_mode\":\"push\"}");

exit:
    return error;
}

cJSON *GenerateLDevID::Jsonify(std::set<std::string> aFieldset)
{
    cJSON      *attributes = cJSON_CreateObject();
    const char *status     = GetStatusString();

    if (hasKey(aFieldset, KEY_CURRENT_DATETIME) && mCurrentDateTime != nullptr)
    {
        cJSON_AddItemToObject(attributes, KEY_CURRENT_DATETIME, cJSON_CreateString(mCurrentDateTime));
    }

    if (hasKey(aFieldset, KEY_STATUS))
    {
        cJSON_AddItemToObject(attributes, KEY_STATUS, cJSON_CreateString(status));
    }

    // Certificate is saved to local file system, not returned in response
    // Client should call /api/auth/enroll to activate the generated certificate

    return attributes;
}

bool GenerateLDevID::Validate(const cJSON &aJson)
{
    bool   success = true;
    cJSON *value;

    // currentDateTime is optional - if present, validate it's a string
    value = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_CURRENT_DATETIME);
    if (value != nullptr)
    {
        VerifyOrExit(cJSON_IsString(value), success = false);
        // Note: Could validate datetime format (ISO 8601) for stricter input validation
    }

exit:
    return success;
}

} // namespace actions
} // namespace rest
} // namespace otbr
