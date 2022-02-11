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

#ifndef OTBR_GRPC_OTBR_IMPL_HPP_
#define OTBR_GRPC_OTBR_IMPL_HPP_

#include "otbr.grpc.pb.h"
#include "common/task_runner.hpp"
#include "ncp/ncp_openthread.hpp"

namespace otbr {
class OtbrImpl final : public Otbr::Service
{
public:
    explicit OtbrImpl(Ncp::ControllerOpenThread &aNcp);

private:
    grpc::Status GetProperties(grpc::ServerContext *       aContext,
                               const GetPropertiesRequest *aRequest,
                               GetPropertiesReply *        aReply) override;

    TaskRunner                 mTaskRunner;
    Ncp::ControllerOpenThread &mNcp;

public:
    static constexpr char kPropertyOtHostVersion[] = "ot_host_version";
    static constexpr char kPropertyOtRcpVersion[]  = "ot_rcp_version";
    static constexpr char kPropertyThreadVersion[] = "thread_version";
    static constexpr char kPropertyRegionCode[]    = "region_code";
};
} // namespace otbr

#endif // OTBR_GRPC_OTBR_IMPL_HPP_
