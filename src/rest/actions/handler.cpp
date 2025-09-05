/*
 *  Copyright (c) 2025, The OpenThread Authors.
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

#include "handler.hpp"

#include <unordered_map>

#include "add_thread_device.hpp"
#include "discover_network.hpp"
#include "energy_scan.hpp"
#include "network_diagnostic.hpp"
#include "reset_diagnostic_counters.hpp"

namespace otbr {
namespace rest {
namespace actions {

static std::unordered_map<std::string, Handler> sHandlers = {
    {ADD_DEVICE_ACTION_TYPE_NAME, Handler::MakeHandler<AddThreadDevice>()},
    {ENERGY_SCAN_ACTION_TYPE_NAME, Handler::MakeHandler<EnergyScan>()},
    {NETWORK_DIAG_ACTION_TYPE_NAME, Handler::MakeHandler<NetworkDiagnostic>()},
    {RESET_DIAG_COUNTERS_ACTION_TYPE_NAME, Handler::MakeHandler<ResetDiagnosticCounters>()},
    {DISCOVER_NETWORK_ACTION_TYPE_NAME, Handler::MakeHandler<DiscoverNetwork>()},
};

const Handler *FindHandler(const char *aName)
{
    const Handler *result = nullptr;
    auto           it     = sHandlers.find(aName);

    if (it != sHandlers.end())
    {
        result = &it->second;
    }

    return result;
}

} // namespace actions
} // namespace rest
} // namespace otbr
