/*
 *    Copyright (c) 2026, The OpenThread Authors.
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
 *   This file declares the interface the pf firewall backend uses to run
 *   pfctl(8), and its process-spawning implementation.
 */

#ifndef OTBR_FIREWALL_PFCTL_HPP_
#define OTBR_FIREWALL_PFCTL_HPP_

#include "openthread-br/config.h"

#include <string>
#include <vector>

#include "common/types.hpp"

namespace otbr {
namespace Firewall {

/**
 * Runs pfctl(8) for the pf firewall backend.
 *
 * The backend drives pf exclusively through pfctl: Apple does not ship the
 * pf ioctl interface (net/pfvar.h) in the public SDK, and pfctl is part of
 * the base system, so no build-time dependency is needed. Tests substitute a
 * fake.
 */
class IPfctl
{
public:
    virtual ~IPfctl(void) = default;

    /**
     * Runs pfctl with the given arguments, feeding @p aInput on its stdin.
     *
     * @param[in]  aArgs    The arguments, not including the program name.
     * @param[in]  aInput   Data written to pfctl's stdin (may be empty).
     * @param[out] aOutput  What pfctl wrote to stdout.
     * @param[out] aError   What pfctl wrote to stderr.
     *
     * @retval OTBR_ERROR_NONE   pfctl exited with status 0.
     * @retval OTBR_ERROR_ERRNO  pfctl could not be run, or exited non-zero.
     */
    virtual otbrError Run(const std::vector<std::string> &aArgs,
                          const std::string              &aInput,
                          std::string                    &aOutput,
                          std::string                    &aError) = 0;
};

/**
 * Runs /sbin/pfctl as a child process, without a shell.
 */
class PfctlProcess : public IPfctl
{
public:
    otbrError Run(const std::vector<std::string> &aArgs,
                  const std::string              &aInput,
                  std::string                    &aOutput,
                  std::string                    &aError) override;

private:
    static const char kPfctlPath[];
};

} // namespace Firewall
} // namespace otbr

#endif // OTBR_FIREWALL_PFCTL_HPP_
