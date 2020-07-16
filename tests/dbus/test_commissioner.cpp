/*
 *    Copyright (c) 2020, The OpenThread Authors.
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

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include <dbus/dbus.h>
#include <unistd.h>

#include "test_utils.hpp"
#include "common/code_utils.hpp"
#include "dbus/client/thread_api_dbus.hpp"
#include "dbus/common/constants.hpp"

using otbr::DBus::ClientError;
using otbr::DBus::CommissionerJoinerEvent;
using otbr::DBus::CommissionerState;
using otbr::DBus::JoinerInfo;
using otbr::DBus::JoinerType;
using otbr::DBus::ThreadApiDBus;

using UniqueDBusConnection = std::unique_ptr<DBusConnection, DBusConnectionDeleter>;

int main(void)
{
    DBusError                      error;
    UniqueDBusConnection           connection;
    std::unique_ptr<ThreadApiDBus> api;
    int                            commissionerPetitionCount = 0;
    int                            commissionerActiveCount   = 0;
    int                            joinerStartCount          = 0;
    int                            joinerConnectedCount      = 0;
    int                            joinerFinalizeCount       = 0;
    int                            joinerEndCount            = 0;
    JoinerInfo                     joinerInfo{JoinerType::JOINER_ANY, 0, 0, "ABCDEF", 300};

    dbus_error_init(&error);
    connection = UniqueDBusConnection(dbus_bus_get(DBUS_BUS_SYSTEM, &error));

    VerifyOrExit(connection != nullptr);

    VerifyOrExit(dbus_bus_register(connection.get(), &error) == true);

    api = std::unique_ptr<ThreadApiDBus>(new ThreadApiDBus(connection.get()));
    TEST_ASSERT(api->CommissionerStart(
                    [&commissionerPetitionCount, &commissionerActiveCount, &api, joinerInfo](CommissionerState aState) {
                        CommissionerState state;

                        printf("Commisioner state %d\n", static_cast<int>(aState));
                        switch (aState)
                        {
                        case CommissionerState::COMMISSIONER_STATE_DISABLED:
                            TEST_ASSERT(false);
                            break;
                        case CommissionerState::COMMISSIONER_STATE_PETITION:
                            commissionerPetitionCount++;
                            break;
                        case CommissionerState::COMMISSIONER_STATE_ACTIVE:
                            commissionerActiveCount++;
                            printf("Commissioner add joiner\n");
                            TEST_ASSERT(api->GetCommissionerState(state) == ClientError::ERROR_NONE);
                            TEST_ASSERT(state == aState);
                            TEST_ASSERT(api->CommissionerAddJoiner(joinerInfo) == ClientError::ERROR_NONE);
                            break;
                        default:
                            TEST_ASSERT(false);
                            break;
                        }
                    },
                    [&api, &joinerStartCount, &joinerConnectedCount, &joinerFinalizeCount, &joinerEndCount,
                     &commissionerActiveCount, &commissionerPetitionCount](CommissionerJoinerEvent aEvent,
                                                                           const JoinerInfo &aInfo, uint64_t aJoinerId,
                                                                           bool aJoinerIdPresent) {
                        printf("JoinerEvent %d JoinerId %" PRIu64 "\n", static_cast<int>(aEvent), aJoinerId);
                        switch (aEvent)
                        {
                        case CommissionerJoinerEvent::COMMISSIONER_JOINER_START:
                            joinerStartCount++;
                            break;
                        case CommissionerJoinerEvent::COMMISSIONER_JOINER_CONNECTED:
                            joinerConnectedCount++;
                            break;
                        case CommissionerJoinerEvent::COMMISSIONER_JOINER_FINALIZE:
                            joinerFinalizeCount++;
                            break;
                        case CommissionerJoinerEvent::COMMISSIONER_JOINER_END:
                            joinerEndCount++;
                            break;
                        case CommissionerJoinerEvent::COMMISSIONER_JOINER_REMOVED:
                        default:
                            TEST_ASSERT(false);
                            break;
                        }
                        TEST_ASSERT(aInfo.mType == JoinerType::JOINER_ANY);
                        TEST_ASSERT(aJoinerIdPresent);

                        if (aEvent == CommissionerJoinerEvent::COMMISSIONER_JOINER_END)
                        {
                            TEST_ASSERT(joinerStartCount == 1);
                            TEST_ASSERT(joinerConnectedCount == 1);
                            TEST_ASSERT(joinerFinalizeCount == 1);
                            TEST_ASSERT(joinerEndCount == 1);
                            TEST_ASSERT(commissionerPetitionCount == 1);
                            TEST_ASSERT(commissionerActiveCount == 1);
                            TEST_ASSERT(aJoinerIdPresent);
                            TEST_ASSERT(api->CommissionerStop() == ClientError::ERROR_NONE);
                            exit(0);
                        }
                    }) == ClientError::ERROR_NONE);

    while (true)
    {
        dbus_connection_read_write_dispatch(connection.get(), 0);
    }

exit:
    dbus_error_free(&error);
    return 0;
};
