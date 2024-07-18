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

/**
 * @file
 *   This file includes definitions for the spinel based Thread controller.
 */

#ifndef OTBR_AGENT_NCP_SPINEL_HPP_
#define OTBR_AGENT_NCP_SPINEL_HPP_

#include <functional>

#include <openthread/dataset.h>
#include <openthread/error.h>
#include <openthread/link.h>
#include <openthread/thread.h>

#include "lib/spinel/spinel.h"
#include "lib/spinel/spinel_driver.hpp"

#include "common/task_runner.hpp"

namespace otbr {
namespace Ncp {

/**
 * This interface is an observer to subscribe the network properties from NCP.
 *
 */
class PropsObserver
{
public:
    /**
     * Updates the device role.
     *
     * @param[in] aRole  The device role.
     *
     */
    virtual void SetDeviceRole(otDeviceRole aRole) = 0;

    /**
     * The destructor.
     *
     */
    virtual ~PropsObserver(void) = default;
};

/**
 * The class provides methods for controlling the Thread stack on the network co-processor (NCP).
 *
 */
class NcpSpinel
{
public:
    /**
     * Constructor.
     *
     */
    NcpSpinel(void);

    /**
     * Do the initialization.
     *
     * @param[in]  aSpinelDriver   A reference to the SpinelDriver instance that this object depends.
     * @param[in]  aObserver       A reference to the Network properties observer.
     *
     */
    void Init(ot::Spinel::SpinelDriver &aSpinelDriver, PropsObserver &aObserver);

    /**
     * Do the de-initialization.
     *
     */
    void Deinit(void);

    /**
     * Returns the Co-processor version string.
     *
     */
    const char *GetCoprocessorVersion(void) { return mSpinelDriver->GetVersion(); }

private:
    using FailureHandler = std::function<void(otError)>;

    static constexpr uint8_t kMaxTids = 16;

    template <typename Function, typename... Args> static void CallAndClear(Function &aFunc, Args &&...aArgs)
    {
        if (aFunc)
        {
            aFunc(std::forward<Args>(aArgs)...);
            aFunc = nullptr;
        }
    }

    static otbrError SpinelDataUnpack(const uint8_t *aDataIn, spinel_size_t aDataLen, const char *aPackFormat, ...);

    static void HandleReceivedFrame(const uint8_t *aFrame,
                                    uint16_t       aLength,
                                    uint8_t        aHeader,
                                    bool          &aSave,
                                    void          *aContext);
    void        HandleReceivedFrame(const uint8_t *aFrame, uint16_t aLength, uint8_t aHeader, bool &aShouldSaveFrame);
    static void HandleSavedFrame(const uint8_t *aFrame, uint16_t aLength, void *aContext);

    static otDeviceRole SpinelRoleToDeviceRole(spinel_net_role_t aRole);

    void HandleNotification(const uint8_t *aFrame, uint16_t aLength);
    void HandleResponse(spinel_tid_t aTid, const uint8_t *aFrame, uint16_t aLength);
    void HandleValueIs(spinel_prop_key_t aKey, const uint8_t *aBuffer, uint16_t aLength);

    spinel_tid_t GetNextTid(void);
    void         FreeTid(spinel_tid_t tid) { mCmdTidsInUse &= ~(1 << tid); }

    ot::Spinel::SpinelDriver *mSpinelDriver;
    uint16_t                  mCmdTidsInUse;              ///< Used transaction ids.
    spinel_tid_t              mCmdNextTid;                ///< Next available transaction id.
    spinel_prop_key_t         mWaitingKeyTable[kMaxTids]; ///< The property keys of ongoing transactions.

    TaskRunner mTaskRunner;

    PropsObserver *mPropsObserver;
};

} // namespace Ncp
} // namespace otbr

#endif // OTBR_AGENT_NCP_SPINEL_HPP_
