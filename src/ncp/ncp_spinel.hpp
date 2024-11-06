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
#include <memory>

#include <openthread/dataset.h>
#include <openthread/error.h>
#include <openthread/link.h>
#include <openthread/thread.h>

#include "lib/spinel/spinel.h"
#include "lib/spinel/spinel_buffer.hpp"
#include "lib/spinel/spinel_driver.hpp"
#include "lib/spinel/spinel_encoder.hpp"

#include "common/task_runner.hpp"
#include "common/types.hpp"
#include "ncp/async_task.hpp"
#include "ncp/posix/infra_if.hpp"
#include "ncp/posix/netif.hpp"

namespace otbr {
namespace Ncp {

/**
 * This interface is an observer to subscribe the network properties from NCP.
 */
class PropsObserver
{
public:
    /**
     * Updates the device role.
     *
     * @param[in] aRole  The device role.
     */
    virtual void SetDeviceRole(otDeviceRole aRole) = 0;

    /**
     * Updates the active dataset.
     *
     * @param[in] aActiveOpDatasetTlvs  The active dataset tlvs.
     */
    virtual void SetDatasetActiveTlvs(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs) = 0;

    /**
     * The destructor.
     */
    virtual ~PropsObserver(void) = default;
};

/**
 * The class provides methods for controlling the Thread stack on the network co-processor (NCP).
 */
class NcpSpinel : public Netif::Dependencies, public InfraIf::Dependencies
{
public:
    using Ip6AddressTableCallback          = std::function<void(const std::vector<Ip6AddressInfo> &)>;
    using Ip6MulticastAddressTableCallback = std::function<void(const std::vector<Ip6Address> &)>;
    using NetifStateChangedCallback        = std::function<void(bool)>;
    using Ip6ReceiveCallback               = std::function<void(const uint8_t *, uint16_t)>;
    using InfraIfSendIcmp6NdCallback = std::function<void(uint32_t, const otIp6Address &, const uint8_t *, uint16_t)>;

    /**
     * Constructor.
     */
    NcpSpinel(void);

    /**
     * Do the initialization.
     *
     * @param[in]  aSpinelDriver   A reference to the SpinelDriver instance that this object depends.
     * @param[in]  aObserver       A reference to the Network properties observer.
     */
    void Init(ot::Spinel::SpinelDriver &aSpinelDriver, PropsObserver &aObserver);

    /**
     * Do the de-initialization.
     */
    void Deinit(void);

    /**
     * Returns the Co-processor version string.
     */
    const char *GetCoprocessorVersion(void) { return mSpinelDriver->GetVersion(); }

    /**
     * This method sets the active dataset on the NCP.
     *
     * If this method is called again before the previous call completed, no action will be taken.
     * The new receiver @p aAsyncTask will be set a result OT_ERROR_BUSY.
     *
     * @param[in] aActiveOpDatasetTlvs  A reference to the active operational dataset of the Thread network.
     * @param[in] aAsyncTask            A pointer to an async result to receive the result of this operation.
     */
    void DatasetSetActiveTlvs(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs, AsyncTaskPtr aAsyncTask);

    /**
     * This method instructs the NCP to send a MGMT_SET to set Thread Pending Operational Dataset.
     *
     * If this method is called again before the previous call completed, no action will be taken.
     * The new receiver @p aAsyncTask will be set a result OT_ERROR_BUSY.
     *
     * @param[in] aPendingOpDatasetTlvsPtr  A shared pointer to the pending operational dataset of the Thread network.
     * @param[in] aAsyncTask                A pointer to an async result to receive the result of this operation.
     */
    void DatasetMgmtSetPending(std::shared_ptr<otOperationalDatasetTlvs> aPendingOpDatasetTlvsPtr,
                               AsyncTaskPtr                              aAsyncTask);

    /**
     * This method enableds/disables the IP6 on the NCP.
     *
     * If this method is called again before the previous call completed, no action will be taken.
     * The new receiver @p aAsyncTask will be set a result OT_ERROR_BUSY.
     *
     * @param[in] aEnable     TRUE to enable and FALSE to disable.
     * @param[in] aAsyncTask  A pointer to an async result to receive the result of this operation.
     */
    void Ip6SetEnabled(bool aEnable, AsyncTaskPtr aAsyncTask);

    /**
     * This method sets the callback to receive the IPv6 address table from the NCP.
     *
     * The callback will be invoked when receiving an IPv6 address table from the NCP. When the
     * callback is invoked, the callback MUST copy the otIp6AddressInfo objects and maintain it
     * if it's not used immediately (within the callback).
     *
     * @param[in] aCallback  The callback to handle the IP6 address table.
     */
    void Ip6SetAddressCallback(const Ip6AddressTableCallback &aCallback) { mIp6AddressTableCallback = aCallback; }

    /**
     * This method sets the callback to receive the IPv6 multicast address table from the NCP.
     *
     * @param[in] aCallback  The callback to handle the IPv6 address table.
     *
     * The callback will be invoked when receiving an IPv6 multicast address table from the NCP.
     * When the callback is invoked, the callback MUST copy the otIp6Address objects and maintain it
     * if it's not used immediately (within the callback).
     */
    void Ip6SetAddressMulticastCallback(const Ip6MulticastAddressTableCallback &aCallback)
    {
        mIp6MulticastAddressTableCallback = aCallback;
    }

    /**
     * This method sets the callback to receive IP6 datagrams.
     *
     * @param[in] aCallback  The callback to receive IP6 datagrams.
     */
    void Ip6SetReceiveCallback(const Ip6ReceiveCallback &aCallback) { mIp6ReceiveCallback = aCallback; }

    /**
     * This methods sends an IP6 datagram through the NCP.
     *
     * @param[in] aData      A pointer to the beginning of the IP6 datagram.
     * @param[in] aLength    The length of the datagram.
     *
     * @retval OTBR_ERROR_NONE  The datagram is sent to NCP successfully.
     * @retval OTBR_ERROR_BUSY  NcpSpinel is busy with other requests.
     */
    otbrError Ip6Send(const uint8_t *aData, uint16_t aLength) override;

    /**
     * This method enableds/disables the Thread network on the NCP.
     *
     * If this method is called again before the previous call completed, no action will be taken.
     * The new receiver @p aAsyncTask will be set a result OT_ERROR_BUSY.
     *
     * @param[in] aEnable     TRUE to enable and FALSE to disable.
     * @param[in] aAsyncTask  A pointer to an async result to receive the result of this operation.
     */
    void ThreadSetEnabled(bool aEnable, AsyncTaskPtr aAsyncTask);

    /**
     * This method instructs the device to leave the current network gracefully.
     *
     * If this method is called again before the previous call completed, no action will be taken.
     * The new receiver @p aAsyncTask will be set a result OT_ERROR_BUSY.
     *
     * @param[in] aAsyncTask  A pointer to an async result to receive the result of this operation.
     */
    void ThreadDetachGracefully(AsyncTaskPtr aAsyncTask);

    /**
     * This method instructs the NCP to erase the persistent network info.
     *
     * If this method is called again before the previous call completed, no action will be taken.
     * The new receiver @p aAsyncTask will be set a result OT_ERROR_BUSY.
     *
     * @param[in] aAsyncTask  A pointer to an async result to receive the result of this operation.
     */
    void ThreadErasePersistentInfo(AsyncTaskPtr aAsyncTask);

    /**
     * This method sets the callback invoked when the network interface state changes.
     *
     * @param[in] aCallback  The callback invoked when the network interface state changes.
     */
    void NetifSetStateChangedCallback(const NetifStateChangedCallback &aCallback)
    {
        mNetifStateChangedCallback = aCallback;
    }

    /**
     * This method sets the function to send an Icmp6 ND message on the infrastructure link.
     *
     * @param[in] aCallback  The callback to send an Icmp6 ND message on the infrastructure link.
     */
    void InfraIfSetIcmp6NdSendCallback(const InfraIfSendIcmp6NdCallback &aCallback)
    {
        mInfraIfIcmp6NdCallback = aCallback;
    }

private:
    using FailureHandler = std::function<void(otError)>;

    static constexpr uint8_t kMaxTids = 16;

    template <typename Function, typename... Args> static void SafeInvoke(Function &aFunc, Args &&...aArgs)
    {
        if (aFunc)
        {
            aFunc(std::forward<Args>(aArgs)...);
        }
    }

    static void CallAndClear(AsyncTaskPtr &aResult, otError aError, const std::string &aErrorInfo = "")
    {
        if (aResult)
        {
            aResult->SetResult(aError, aErrorInfo);
            aResult = nullptr;
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

    void      HandleNotification(const uint8_t *aFrame, uint16_t aLength);
    void      HandleResponse(spinel_tid_t aTid, const uint8_t *aFrame, uint16_t aLength);
    void      HandleValueIs(spinel_prop_key_t aKey, const uint8_t *aBuffer, uint16_t aLength);
    otbrError HandleResponseForPropSet(spinel_tid_t      aTid,
                                       spinel_prop_key_t aKey,
                                       const uint8_t    *aData,
                                       uint16_t          aLength);
    otbrError HandleResponseForPropInsert(spinel_tid_t      aTid,
                                          spinel_command_t  aCmd,
                                          spinel_prop_key_t aKey,
                                          const uint8_t    *aData,
                                          uint16_t          aLength);
    otbrError HandleResponseForPropRemove(spinel_tid_t      aTid,
                                          spinel_command_t  aCmd,
                                          spinel_prop_key_t aKey,
                                          const uint8_t    *aData,
                                          uint16_t          aLength);

    otbrError Ip6MulAddrUpdateSubscription(const otIp6Address &aAddress, bool aIsAdded) override;

    spinel_tid_t GetNextTid(void);
    void         FreeTidTableItem(spinel_tid_t aTid);

    using EncodingFunc = std::function<otError(ot::Spinel::Encoder &aEncoder)>;
    otError SendCommand(spinel_command_t aCmd, spinel_prop_key_t aKey, const EncodingFunc &aEncodingFunc);
    otError SetProperty(spinel_prop_key_t aKey, const EncodingFunc &aEncodingFunc);
    otError InsertProperty(spinel_prop_key_t aKey, const EncodingFunc &aEncodingFunc);
    otError RemoveProperty(spinel_prop_key_t aKey, const EncodingFunc &aEncodingFunc);

    otError SendEncodedFrame(void);

    otError ParseIp6AddressTable(const uint8_t *aBuf, uint16_t aLength, std::vector<Ip6AddressInfo> &aAddressTable);
    otError ParseIp6MulticastAddresses(const uint8_t *aBuf, uint16_t aLen, std::vector<Ip6Address> &aAddressList);
    otError ParseIp6StreamNet(const uint8_t *aBuf, uint16_t aLen, const uint8_t *&aData, uint16_t &aDataLen);
    otError ParseOperationalDatasetTlvs(const uint8_t *aBuf, uint16_t aLen, otOperationalDatasetTlvs &aDatasetTlvs);
    otError ParseInfraIfIcmp6Nd(const uint8_t       *aBuf,
                                uint8_t              aLen,
                                uint32_t            &aInfraIfIndex,
                                const otIp6Address *&aAddr,
                                const uint8_t      *&aData,
                                uint16_t            &aDataLen);

    otbrError SetInfraIf(uint32_t                       aInfraIfIndex,
                         bool                           aIsRunning,
                         const std::vector<Ip6Address> &aIp6Addresses) override;
    otbrError HandleIcmp6Nd(uint32_t          aInfraIfIndex,
                            const Ip6Address &aIp6Address,
                            const uint8_t    *aData,
                            uint16_t          aDataLen) override;

    ot::Spinel::SpinelDriver *mSpinelDriver;
    uint16_t                  mCmdTidsInUse; ///< Used transaction ids.
    spinel_tid_t              mCmdNextTid;   ///< Next available transaction id.

    spinel_prop_key_t mWaitingKeyTable[kMaxTids]; ///< The property keys of ongoing transactions.
    spinel_command_t  mCmdTable[kMaxTids];        ///< The mapping of spinel command and tids when the response
                                                  ///< is LAST_STATUS.

    static constexpr uint16_t kTxBufferSize = 2048;
    uint8_t                   mTxBuffer[kTxBufferSize];
    ot::Spinel::Buffer        mNcpBuffer;
    ot::Spinel::Encoder       mEncoder;
    spinel_iid_t              mIid; /// < Interface Id used to in Spinel header

    TaskRunner mTaskRunner;

    PropsObserver *mPropsObserver;

    AsyncTaskPtr mDatasetSetActiveTask;
    AsyncTaskPtr mDatasetMgmtSetPendingTask;
    AsyncTaskPtr mIp6SetEnabledTask;
    AsyncTaskPtr mThreadSetEnabledTask;
    AsyncTaskPtr mThreadDetachGracefullyTask;
    AsyncTaskPtr mThreadErasePersistentInfoTask;

    Ip6AddressTableCallback          mIp6AddressTableCallback;
    Ip6MulticastAddressTableCallback mIp6MulticastAddressTableCallback;
    Ip6ReceiveCallback               mIp6ReceiveCallback;
    NetifStateChangedCallback        mNetifStateChangedCallback;
    InfraIfSendIcmp6NdCallback       mInfraIfIcmp6NdCallback;
};

} // namespace Ncp
} // namespace otbr

#endif // OTBR_AGENT_NCP_SPINEL_HPP_
