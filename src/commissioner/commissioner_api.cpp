/*
 *    Copyright (c) 2019, The OpenThread Authors.
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
 *   The file implements commissioner c api
 */

#include "commissioner_api.h"

#include "commissioner.hpp"
#include "commissioner_constants.hpp"

#include "common/code_utils.hpp"

#include <stdio.h>

using namespace ot;
using namespace ot::BorderRouter;

#define CONVERT_COMMISSIONER_HANDLE(handle, commissioner, ret)                 \
    do                                                                         \
    {                                                                          \
        VerifyOrExit(handle != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS); \
        commissioner = static_cast<Commissioner *>(handle);                    \
    } while (0)

#define CONVERT_STEERING_DATA(handle, steer, ret)                              \
    do                                                                         \
    {                                                                          \
        VerifyOrExit(handle != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS); \
        steer = static_cast<SteeringData *>(handle);                           \
    } while (0)

otbr_commissioner_result_t otbr_commissioner_create_steering_data(steering_data_handle_t *aSteeringData,
                                                                  uint8_t                 aSteeringDataLength,
                                                                  bool                    aAllowAll,
                                                                  const uint8_t *         aJoinerEui64)
{
    SteeringData *             steeringData;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;
    uint8_t                    joinerId[kEui64Len];

    VerifyOrExit(aSteeringData != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);

    steeringData   = new SteeringData();
    *aSteeringData = steeringData;
    if (aSteeringDataLength == 0)
    {
        aSteeringDataLength = aAllowAll ? 1 : kSteeringDefaultLength;
    }
    steeringData->Init(aSteeringDataLength);

    if (aAllowAll)
    {
        steeringData->Set();
    }
    else
    {
        VerifyOrExit(aJoinerEui64 != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);

        steeringData->ComputeJoinerId(aJoinerEui64, joinerId);
        steeringData->ComputeBloomFilter(joinerId);
    }
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_free_steering_data(steering_data_handle_t aSteeringData)
{
    SteeringData *             steeringData;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;

    CONVERT_STEERING_DATA(aSteeringData, steeringData, ret);
    delete steeringData;
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_compute_pskc(uint8_t *      aPskcBin,
                                                          size_t         aPskcSize,
                                                          const uint8_t *aExtPanId,
                                                          const char *   aNetworkName,
                                                          const char *   aNetworkPassword)
{
    ot::Psk::Pskc              pskc;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;

    VerifyOrExit(aPskcBin != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aPskcSize >= kPSKcLength, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aNetworkName != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aNetworkPassword != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);

    memcpy(aPskcBin, pskc.ComputePskc(aExtPanId, aNetworkName, aNetworkPassword), kPSKcLength);
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_create_commissioner_handle(otbr_commissioner_handle_t *aHandle,
                                                                        const uint8_t *             aPskcBin,
                                                                        int                         aKeepAliveRate)
{
    Commissioner *             commissioner;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;

    VerifyOrExit(aHandle != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aPskcBin != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);

    commissioner = new Commissioner(aPskcBin, aKeepAliveRate);
    *aHandle     = commissioner;
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_free_commissioner_handle(otbr_commissioner_handle_t aHandle)
{
    Commissioner *             commissioner;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;

    CONVERT_COMMISSIONER_HANDLE(aHandle, commissioner, ret);
    delete commissioner;
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_set_joiner(otbr_commissioner_handle_t aHandle,
                                                        const char *               aPskdAscii,
                                                        steering_data_handle_t     aSteeringData)
{
    Commissioner *             commissioner;
    SteeringData *             steeringData;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;

    CONVERT_COMMISSIONER_HANDLE(aHandle, commissioner, ret);
    CONVERT_STEERING_DATA(aSteeringData, steeringData, ret);
    commissioner->SetJoiner(aPskdAscii, *steeringData);
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_update_fd_set(otbr_commissioner_handle_t aHandle,
                                                           fd_set *                   aReadFdSet,
                                                           fd_set *                   aWriteFdSet,
                                                           fd_set *                   aErrorFdSet,
                                                           int *                      aMaxFd,
                                                           timeval *                  aTimeout)
{
    Commissioner *             commissioner;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;

    CONVERT_COMMISSIONER_HANDLE(aHandle, commissioner, ret);
    VerifyOrExit(aReadFdSet != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aWriteFdSet != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aErrorFdSet != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aMaxFd != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aTimeout != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);

    commissioner->UpdateFdSet(*aReadFdSet, *aWriteFdSet, *aErrorFdSet, *aMaxFd, *aTimeout);
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_process(otbr_commissioner_handle_t aHandle,
                                                     const fd_set *             aReadFdSet,
                                                     const fd_set *             aWriteFdSet,
                                                     const fd_set *             aErrorFdSet)
{
    Commissioner *             commissioner;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;

    CONVERT_COMMISSIONER_HANDLE(aHandle, commissioner, ret);
    VerifyOrExit(aReadFdSet != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aWriteFdSet != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aErrorFdSet != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);

    commissioner->Process(*aReadFdSet, *aWriteFdSet, *aErrorFdSet);
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_is_valid(otbr_commissioner_handle_t aHandle, bool *aRet)
{
    Commissioner *             commissioner;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;

    CONVERT_COMMISSIONER_HANDLE(aHandle, commissioner, ret);
    VerifyOrExit(aRet != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);

    *aRet = commissioner->IsValid();
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_is_accepted(otbr_commissioner_handle_t aHandle, bool *aRet)
{
    Commissioner *             commissioner;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;

    CONVERT_COMMISSIONER_HANDLE(aHandle, commissioner, ret);
    VerifyOrExit(aRet != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);

    *aRet = commissioner->IsCommissionerAccepted();
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_connect_dtls(otbr_commissioner_handle_t aHandle,
                                                          const char *               aHost,
                                                          const char *               aPort)
{
    Commissioner *             commissioner;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;
    int                        innerRet;

    CONVERT_COMMISSIONER_HANDLE(aHandle, commissioner, ret);
    VerifyOrExit(aHost != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);
    VerifyOrExit(aPort != nullptr, ret = OTBR_COMMISSIONER_INVALID_ARGS);

    VerifyOrExit(commissioner->InitDtls(aHost, aPort) == 0, ret = OTBR_COMMISSIONER_INTERNAL_ERROR);
    do
    {
        innerRet = commissioner->TryDtlsHandshake();
    } while (innerRet == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    VerifyOrExit(commissioner->IsValid(), ret = OTBR_COMMISSIONER_INTERNAL_ERROR);
exit:
    return ret;
}

otbr_commissioner_result_t otbr_commissioner_petition(otbr_commissioner_handle_t aHandle)
{
    Commissioner *             commissioner;
    otbr_commissioner_result_t ret = OTBR_COMMISSIONER_SUCCESS;

    CONVERT_COMMISSIONER_HANDLE(aHandle, commissioner, ret);
    commissioner->CommissionerPetition();
exit:
    return ret;
}
