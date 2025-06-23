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

#ifndef OTBR_REST_ACTIONS_COMMON_HPP_
#define OTBR_REST_ACTIONS_COMMON_HPP_

#include <common/time.hpp>

#include <openthread/error.h>

#include "rest/rest_generic_collection.hpp"
#include "rest/services.hpp"
#include "rest/uuid.hpp"

struct cJSON;

namespace otbr {
namespace rest {
namespace actions {

enum ActionStatus
{
    kActionStatusPending = 0,
    kActionStatusActive,
    kActionStatusCompleted,
    kActionStatusStopped,
    kActionStatusFailed,
};

/**
 * @brief This virtual class implements a general json:api item for holding action attributes.
 *
 */
class BasicActions : public BasicCollectionItem
{
public:
    /**
     * Constructor for a Basic Actions.
     *
     * @param[in] aJson     The attributes cJSON object.
     * @param[in] aServices The shared services object to use.
     */
    BasicActions(const cJSON &aJson, Services &aServices);

    /**
     * Constructor for a Basic Actions.
     *
     * @param[in] aJson     The attributes cJSON object.
     * @param[in] aTimeout  The timeout value when action turns inactive.
     * @param[in] aServices The shared services object to use.
     */
    BasicActions(const cJSON &aJson, Seconds aTimeout, Services &aServices);

    /**
     * Destructor for a Basic Actions.
     *
     */
    ~BasicActions();

    /**
     * Get the json string for the action.
     *
     * @param[in] aKeys  The set of keys to include in the json string.
     *
     * @returns The json string.
     */
    std::string ToJsonString(std::set<std::string> aKeys) override;

    /**
     * Get the json:api item for the action.
     *
     * @param[in] aKeys  The set of keys to include in the json:api item.
     *
     * @returns The json:api item.
     */
    std::string ToJsonApiItem(std::set<std::string> aKeys) override;

    bool operator<(const BasicActions &aOther) const { return mCreatedSteady < aOther.mCreatedSteady; }
    bool operator<=(const BasicActions &aOther) const { return mCreatedSteady <= aOther.mCreatedSteady; }

    bool operator>(const BasicActions &aOther) const { return mCreatedSteady > aOther.mCreatedSteady; }
    bool operator>=(const BasicActions &aOther) const { return mCreatedSteady >= aOther.mCreatedSteady; }

    bool operator==(const BasicActions &aOther) const { return mUuid == aOther.mUuid; }

    /**
     * Called if the action is in status pending or active to perform any
     * necessary processing.
     */
    virtual void Update(void) = 0;

    /**
     * Called when a pending or active action needs to be stopped.
     *
     * A call to Update should always preceed a call to Stop to ensure any
     * already finished work is processed. If the action finishes there Stop
     * must not be called. Because of this if Stop is called the action must
     * clean up all used services and transition to state stopped.
     */
    virtual void Stop(void) = 0;

    /**
     * Creates a cJSON object representing the action. Called when a get
     * request is received to service it.
     *
     * @return  The cJSON object representing the action specific attributes.
     *          Must be cleaned up by the caller.
     */
    virtual cJSON *Jsonify(void) = 0;

    /**
     * Get the steady_clock timepoint when the action was created.
     *
     * @return  The created timepoint of the action.
     */
    Timepoint GetCreated(void) const { return mCreatedSteady; }

    /**
     * Get the timeout of the action.
     *
     * @return  The timeout of the action.
     */
    Seconds GetTimeout(void) const { return mTimeout; }

    /**
     * Returns true if now is after the timeout of this action.
     *
     * @note This will not in any way check the status of the action. It is
     *       purely comparing timepoints.
     *
     * @retval  True   If now is after the timeout of this action.
     * @retval  False  If now is before or equal the timeout of this action.
     */
    bool IsBeyondTimeout(void) const { return mCreatedSteady + mTimeout < Clock::now(); }

    /**
     * Get the status of the action.
     *
     * @return  The status of the action.
     */
    ActionStatus GetStatus(void) const { return mStatus; }

    /**
     * Get the status of the action as a string.
     *
     * @return  The status of the action as a string.
     */
    const char *GetStatusString(void) const { return StatusToString(mStatus); }

    /**
     * Is the action pending or active?
     *
     * @retval True  If the action is pending or active.
     * @retval False If the action is not pending or active.
     */
    bool IsPendingOrActive(void) const { return mStatus == kActionStatusPending || mStatus == kActionStatusActive; }

    /**
     * Convert the status of the action to string.
     *
     * @param[in] aStatus  The status of the action.
     * @return  The status of the action as a string.
     *
     */
    static const char *StatusToString(ActionStatus aStatus);

protected:
    static constexpr Seconds kDefaultTimeout{60};

    /**
     * Attempts to read the timeout property in a actions attribute cJSON
     * object.
     *
     * If the timeout property cannot be found aTimeout remains unchanged.
     *
     * @param       aJson     The attributes cJSON object.
     * @param[out]  aTimeout  Written with the timeout value if found.
     *
     * @retval  OT_ERROR_NONE       The timeout property was found and written to aTimeout
     * @retval  OT_ERROR_NOT_FOUND  The timeout property was not found or could not be parsed
     */
    static otError ReadTimeout(const cJSON *aJson, Seconds &aTimeout);
    static Seconds ReadTimeoutOrDefault(const cJSON &aJson, Seconds aDefault);

    /**
     * Attempts to read the destination property in a actions attribute cJSON
     *
     * @param       aJson     The attributes cJSON object.
     * @param[out]  aType     Written with the address type if found.
     *
     * @return  The destination property string if found, nullptr otherwise.
     */
    static const char *ReadDestination(const cJSON *aJson, AddressType &aType);

    /**
     * Adds the timeout entry to the attributes objects.
     *
     * @param aAttributes  A cJSON object to which the timeout property should be added
     */
    void JsonifyTimeout(cJSON *aAttributes) const;

    /**
     * Set the relationship to the result of this action.
     */
    void SetResult(const char *aType, std::string aUuid);

    /**
     * The steady_clock created timepoint of the action.
     */
    const Timepoint mCreatedSteady;

    /**
     * The timeout of the action.
     */
    const Seconds mTimeout;

    /**
     * The shared services object to use.
     */
    Services &mServices;

    /**
     * Class specific attributes of the item.
     */
    cJSON *mJson;

    /**
     * Relationships of the item, eg. to results.
     */
    cJSON *mRelationships;

    /**
     * The status of the action.
     */
    ActionStatus mStatus;
};

} // namespace actions
} // namespace rest
} // namespace otbr

#endif // OTBR_REST_ACTIONS_COMMON_HPP_
