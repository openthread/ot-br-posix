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
 *   The file defines commissioner c api
 */

#ifndef OTBR_COMMISSIONER_C_API_H_
#define OTBR_COMMISSIONER_C_API_H_

#include <stdint.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

enum otbr_commissioner_result_t
{
    OTBR_COMMISSIONER_SUCCESS        = 0,
    OTBR_COMMISSIONER_INTERNAL_ERROR = -1,
    OTBR_COMMISSIONER_INVALID_ARGS   = -0xffff
};

typedef void *otbr_commissioner_handle_t;
typedef void *steering_data_handle_t;

/**
 * This function creates steering data to filter joiners.
 *
 * @param[out]   aSteeringData          Buffer to store steering data handle.
 * @param[in]    aSteeringDataLength    Length of steering data.
 * @param[in]    aAllowAll              Whether to allow devices with any eui64 to join.
 * @param[in]    aJoinerEui64           Target joiner eui64, only used when aAllowAll==false.
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr was provided for @p aSteeringData or
 *         @p aJoinerEui64 when aAllowAll==false .
 * @retval OTBR_COMMISSIONER_SUCCESS if steering data handle was successfully created.
 *
 */
otbr_commissioner_result_t otbr_commissioner_create_steering_data(steering_data_handle_t *aSteeringData,
                                                                  uint8_t                 aSteeringDataLength,
                                                                  bool                    aAllowAll,
                                                                  const uint8_t *         aJoinerEui64);

/**
 * This function frees steering data.
 *
 * @param[in]    aSteeringData          Steering data to be freed.
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for @p aSteeringData.
 * @retval OTBR_COMMISSIONER_SUCCESS if steering data handle was successfully freed.
 *
 */
otbr_commissioner_result_t otbr_commissioner_free_steering_data(steering_data_handle_t aSteeringData);

/**
 * This function computes pskc for commissioner.
 *
 * @param[out]   aPskcBin           Buffer to store computed pskc.
 * @param[in]    aPskcSize          Size of aPskcBin.
 * @param[in]    aExtPanId          Extended panid of thread network.
 * @param[in]    aNetworkName       Thread network name.
 * @param[in]    aNetworkPassword   Thread network password.
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for @p aPskcBin, @p aNetworkName
 *         or @p aNetworkPassword, or aPskcSize is smaller then kPSKcLength(16).
 * @retval OTBR_COMMISSIONER_SUCCESS if pskc is successfully computed.
 *
 */
otbr_commissioner_result_t otbr_commissioner_compute_pskc(uint8_t *      aPskcBin,
                                                          size_t         aPskcSize,
                                                          const uint8_t *aExtPanId,
                                                          const char *   aNetworkName,
                                                          const char *   aNetworkPassword);

/**
 * This function create commissioner handle.
 *
 * @param[out]   aHandle            Buffer to store commissioner handle.
 * @param[in]    aPskcBin           Computed pskc.
 * @param[in]    aKeepAliveRate     Rate of sending keep alive message to leader.
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for @p aHandle or @p aPskcBin.
 * @retval OTBR_COMMISSIONER_SUCCESS if commissioner handle successfully created.
 *
 */
otbr_commissioner_result_t otbr_commissioner_create_commissioner_handle(otbr_commissioner_handle_t *aHandle,
                                                                        const uint8_t *             aPskcBin,
                                                                        int                         aKeepAliveRate);

/**
 * This function free commissioner handle.
 *
 * @param[in]    aHandle            Commissioner handle to be freed.
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for @p aHandle
 * @retval OTBR_COMMISSIONER_SUCCESS if commissioner handle successfully freed.
 *
 */
otbr_commissioner_result_t otbr_commissioner_free_commissioner_handle(otbr_commissioner_handle_t aHandle);

/**
 * This function sets joiner pskd and steering data.
 *
 * @param[in]    aHandle            Commissioner handle.
 * @param[in]    aPskdAscii         Joiner pskd in ascii format.
 * @param[in]    aSteeringData      Steering data to filter joiner.
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for @p aHandle or @p aPskdAscii.
 * @retval OTBR_COMMISSIONER_SUCCESS if joiner successfully set.
 *
 */
otbr_commissioner_result_t otbr_commissioner_set_joiner(otbr_commissioner_handle_t aHandle,
                                                        const char *               aPskdAscii,
                                                        steering_data_handle_t     aSteeringData);

/**
 * This function updates the fd_set and timeout for mainloop.
 *
 * @param[in]       aHandle         Commissioner handle
 * @param[inout]    aReadFdSet      Fd_set for polling read.
 * @param[inout]    aWriteFdSet     Fd_set for polling write.
 * @param[inout]    aErrorFdSet     Fd_set for polling error.
 * @param[inout]    aMaxFd          The current max fd in @p aReadFdSet and @p aWriteFdSet.
 * @param[inout]    aTimeout        The timeout.
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for any parameter.
 * @retval OTBR_COMMISSIONER_SUCCESS if fd_set and timeout are successfully updated.
 *
 */
otbr_commissioner_result_t otbr_commissioner_update_fd_set(otbr_commissioner_handle_t aHandle,
                                                           fd_set *                   aReadFdSet,
                                                           fd_set *                   aWriteFdSet,
                                                           fd_set *                   aErrorFdSet,
                                                           int *                      aMaxFd,
                                                           timeval *                  aTimeout);

/**
 * This method performs the session processing.
 *
 * @param[in]   aHandle             Commissioner handle
 * @param[in]   aReadFdSet          Fd_set ready for reading.
 * @param[in]   aWriteFdSet         Fd_set ready for writing.
 * @param[in]   aErrorFdSet         Fd_set with error occurred.
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for any parameter.
 * @retval OTBR_COMMISSIONER_SUCCESS if session processing successfully completes.
 *
 */
otbr_commissioner_result_t otbr_commissioner_process(otbr_commissioner_handle_t aHandle,
                                                     const fd_set *             aReadFdSet,
                                                     const fd_set *             aWriteFdSet,
                                                     const fd_set *             aErrorFdSet);

/**
 * This method retval whether the commissioner is valid.
 *
 * @param[in]       aHandle         Commissioner handle.
 * @param[out]      aRet            Buffer to store whether commissioner is valid
 *
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for any parameter.
 * @retval OTBR_COMMISSIONER_SUCCESS if query successfully completes.
 *
 */
otbr_commissioner_result_t otbr_commissioner_is_valid(otbr_commissioner_handle_t aHandle, bool *aRet);

/**
 * This method retval whether the commissioner petition has succeeded.
 *
 * @param[in]       aHandle         Commissioner handle.
 * @param[out]      aRet            Buffer to store whether petition has succeeded
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for any parameter.
 * @retval OTBR_COMMISSIONER_SUCCESS if query successfully completes.
 *
 */
otbr_commissioner_result_t otbr_commissioner_is_accepted(otbr_commissioner_handle_t aHandle, bool *aRet);

/**
 * This method connects commissioner to border agent
 *
 * @param[in]       aHandle         Commissioner handle.
 * @param[in]       aHost           Address of border agent.
 * @param[in]       aPort           Port of border agent.
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for any parameter.
 * @retval OTBR_COMMISSIONER_INTERNAL_ERROR if connect fails.
 * @retval OTBR_COMMISSIONER_SUCCESS upon connection established.
 *
 */
otbr_commissioner_result_t otbr_commissioner_connect_dtls(otbr_commissioner_handle_t aHandle,
                                                          const char *               aHost,
                                                          const char *               aPort);

/**
 * This method sends commissioner petition coap request
 *
 * @param[in]       aHandle         commissioner handle
 *
 * @retval OTBR_COMMISSIONER_INVALID_ARGS if nullptr provided for @p aHandle
 * @retval OTBR_COMMISSIONER_SUCCESS if petition request is successfully sent.
 *
 */
otbr_commissioner_result_t otbr_commissioner_petition(otbr_commissioner_handle_t aHandle);

#ifdef __cplusplus
} // extern "C"
#endif
#endif
