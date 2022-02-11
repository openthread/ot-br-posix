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

#include "otbr_impl.hpp"

#include <future>
#include <string>
#include <unordered_map>

#include <openthread/instance.h>
#include <openthread/thread.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/toolchain.h>

#include "common/code_utils.hpp"

namespace otbr {

using ::grpc::ServerContext;
using ::grpc::Status;
using ::grpc::StatusCode;

OtbrImpl::OtbrImpl(Ncp::ControllerOpenThread &aNcp)
    : mNcp(aNcp)
{
}

Status OtbrImpl::GetProperties(ServerContext *             aContext,
                               const GetPropertiesRequest *aRequest,
                               GetPropertiesReply *        aReply)
{
    OT_UNUSED_VARIABLE(aContext);

    std::promise<void> promise;
    std::future<void>  future = promise.get_future();
    std::string        errorMessage;
    Status             status;

    mTaskRunner.Post([&promise, &aRequest, &aReply, &errorMessage, this]() {
        otError error = OT_ERROR_NONE;

        for (const std::string &propertyName : aRequest->property_names())
        {
            PropertyValue value;
            if (propertyName == kPropertyOtHostVersion)
            {
                value.set_value_string(otGetVersionString());
            }
            else if (propertyName == kPropertyOtRcpVersion)
            {
                value.set_value_string(otGetRadioVersionString(mNcp.GetInstance()));
            }
            else if (propertyName == kPropertyThreadVersion)
            {
                value.set_value_int32(otThreadGetVersion());
            }
            else if (propertyName == kPropertyRegionCode)
            {
                uint16_t region;
                SuccessOrExit(error = otPlatRadioGetRegion(mNcp.GetInstance(), &region));
                value.mutable_value_string()->resize(2);
                (*value.mutable_value_string())[0] = (region >> 8);
                (*value.mutable_value_string())[1] = (region & 0xff);
            }
            aReply->mutable_properties()->insert({propertyName, value});
        }

    exit:
        if (error != OT_ERROR_NONE)
        {
            errorMessage = std::string("OpenThread error: ") + std::to_string(static_cast<int>(error));
        }
        promise.set_value();
    });

    future.get();

    if (!errorMessage.empty())
    {
        status = Status(StatusCode::INTERNAL, errorMessage);
    }
    else
    {
        status = Status::OK;
    }
    return status;
}

constexpr char OtbrImpl::kPropertyOtHostVersion[];
constexpr char OtbrImpl::kPropertyOtRcpVersion[];
constexpr char OtbrImpl::kPropertyThreadVersion[];
constexpr char OtbrImpl::kPropertyRegionCode[];

} // namespace otbr
