/*
 *  Copyright (c) 2024, The OpenThread Authors.
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

#ifndef OTBR_REST_ACTIONS_NETWORK_DIAGNOSTIC_HPP_
#define OTBR_REST_ACTIONS_NETWORK_DIAGNOSTIC_HPP_

#include "action.hpp"

#include "rest/diagnostic_types.hpp"

namespace otbr {
namespace rest {
namespace actions {

#define NETWORK_DIAG_ACTION_TYPE_NAME "getNetworkDiagnosticTask"

class NetworkDiagnostic : public BasicActions
{
public:
    /**
     * Constructor for a NetworkDiagnostic.
     *
     * @param[in] aJson     The attributes cJSON object.
     * @param[in] aServices The shared services object to use.
     */
    NetworkDiagnostic(const cJSON &aJson, Services &aServices);

    /**
     * Destructor for a NetworkDiagnostic.
     *
     */
    virtual ~NetworkDiagnostic(void) override;

    /**
     * Get the type name of the action.
     *
     * @returns The type name.
     */
    std::string GetTypeName() const override;

    /**
     * Called if the action is in status pending or active to perform any
     * necessary processing.
     */
    virtual void Update(void) override;

    /**
     * Called when a pending or active action needs to be stopped.
     *
     * A call to Update should always preceed a call to Stop to ensure any
     * already finished work is processed. If the action finishes there Stop
     * must not be called. Because of this if Stop is called the action must
     * clean up all used services and transition to state stopped.
     */
    virtual void Stop(void) override;

    /**
     * Get the json object for the action.
     *
     * @returns The json object.
     */
    virtual cJSON *Jsonify(void) override;

    /**
     * Validate the json object provides required attributes such that the NetworkDiagnostic action is processable.
     *
     * Returns true if the json object is valid, false otherwise.
     */
    static bool Validate(const cJSON &aJson);

private:
    const char *mDestination; ///< Pointer to string in mJson
    AddressType mDestinationType;

    uint8_t  mTypeList[DiagnosticTypes::kMaxTotalCount];
    uint32_t mTypeCount;
};

} // namespace actions
} // namespace rest
} // namespace otbr

#endif // OTBR_REST_ACTIONS_NETWORK_DIAGNOSTIC_HPP_
