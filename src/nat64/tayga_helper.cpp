/*
 *    Copyright (c) 2022, The OpenThread Authors.
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

#if OTBR_ENABLE_TAYGA_NAT64

#include <fstream>

#include <openthread/border_router.h>

#include "common/code_utils.hpp"
#include "common/types.hpp"
#include "nat64/tayga_helper.hpp"
#include "utils/system_utils.hpp"

namespace otbr {
namespace Tayga {
namespace {

static const char kTaygaConf[]       = "/etc/tayga.conf";
static const char kTaygaConfTmp[]    = "/etc/tayga_tmp.conf";
static const char kTaygaRestartCmd[] = OTBR_TAYGA_RESTART_CMD;
static const char kTaygaStopCmd[]    = OTBR_TAYGA_STOP_CMD;
static const char kPrefixLineStart[] = "prefix ";

bool UpdatePrefix(const std::string &prefix)
{
    bool          is_copied  = false;
    bool          is_updated = false;
    std::ifstream file(kTaygaConf);
    std::ofstream tmp_file(kTaygaConfTmp);
    std::string   line;

    VerifyOrExit(file && tmp_file);

    while (std::getline(file, line))
    {
        if (line.find(kPrefixLineStart, 0) == 0)
        {
            VerifyOrExit(line.find(prefix, 0) == std::string::npos, is_updated = false);

            line       = kPrefixLineStart + prefix;
            is_updated = true;
        }
        tmp_file << line << std::endl;
        VerifyOrExit(tmp_file.good(), is_updated = false);
    }

    VerifyOrExit(file.eof(), is_updated = false);

    file.close();
    tmp_file.close();
    is_copied = true;

    if (is_updated)
    {
        VerifyOrExit(remove(kTaygaConf) == 0 && rename(kTaygaConfTmp, kTaygaConf) == 0, is_updated = false);
    }

exit:
    if (!is_copied)
    {
        file.close();
        tmp_file.close();
    }

    if (!is_updated)
    {
        remove(kTaygaConfTmp);
    }

    otbrLogInfo("NAT64 prefix in Tayga configuration file %s updated", is_updated ? "is" : "isn't");

    return is_updated;
}

} // namespace

void ConfigTaygaNat64Prefix(otInstance *aInstance)
{
    otbrError   error      = OTBR_ERROR_NONE;
    bool        is_updated = false;
    otIp6Prefix nat64Prefix;
    Ip6Prefix   prefix;

    VerifyOrExit(otBorderRoutingGetNat64Prefix(aInstance, &nat64Prefix) == OT_ERROR_NONE,
                 error = OTBR_ERROR_OPENTHREAD);

    prefix.Set(nat64Prefix);
    is_updated = UpdatePrefix(prefix.ToString());

    VerifyOrExit(is_updated && SystemUtils::ExecuteCommand(kTaygaRestartCmd) == 0, error = OTBR_ERROR_ERRNO);

exit:
    if (error == OTBR_ERROR_OPENTHREAD)
    {
        otbrLogInfo("Failed to get nat64 prefix. Stopping Tayga...");
        SystemUtils::ExecuteCommand(kTaygaStopCmd);
    }
    else if (error == OTBR_ERROR_ERRNO)
    {
        otbrLogInfo("Didn't re-configure Tayga: %s", otbrErrorString(error));
    }
    else
    {
        otbrLogInfo("Configured Tayga with NAT64 prefix: %s", prefix.ToString().c_str());
    }
}

} // namespace Tayga
} // namespace otbr
#endif // OTBR_ENABLE_TAYGA_NAT64
