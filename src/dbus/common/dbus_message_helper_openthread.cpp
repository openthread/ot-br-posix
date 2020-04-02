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

#include <string.h>

#include "dbus/common/dbus_message_helper.hpp"

namespace otbr {
namespace DBus {

otbrError DBusMessageExtract(DBusMessageIter *aIter, otbrError &aError)
{
    uint8_t   val;
    otbrError err = DBusMessageExtract(aIter, val);

    VerifyOrExit(err == OTBR_ERROR_NONE);
    aError = static_cast<otbrError>(val);
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const otbrError &aError)
{
    return DBusMessageEncode(aIter, static_cast<uint8_t>(aError));
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, ActiveScanResult &aScanResult)
{
    DBusMessageIter sub;
    otbrError       err = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRUCT, err = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(aIter, &sub);

    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mExtAddress));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mNetworkName));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mExtendedPanId));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mSteeringData));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mPanId));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mJoinerUdpPort));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mChannel));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mRssi));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mLqi));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mVersion));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mIsNative));
    SuccessOrExit(err = DBusMessageExtract(&sub, aScanResult.mIsJoinable));

    dbus_message_iter_next(aIter);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const ActiveScanResult &aScanResult)
{
    DBusMessageIter sub;
    otbrError       err = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub), err = OTBR_ERROR_DBUS);
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mExtAddress));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mNetworkName));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mExtendedPanId));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mSteeringData));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mPanId));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mJoinerUdpPort));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mChannel));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mRssi));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mLqi));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mVersion));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mIsNative));
    SuccessOrExit(err = DBusMessageEncode(&sub, aScanResult.mIsJoinable));
    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub), err = OTBR_ERROR_DBUS);

    err = OTBR_ERROR_NONE;
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const LinkModeConfig &aConfig)
{
    otbrError       err = OTBR_ERROR_NONE;
    DBusMessageIter sub;

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub), err = OTBR_ERROR_DBUS);

    SuccessOrExit(err = DBusMessageEncode(&sub, aConfig.mRxOnWhenIdle));
    SuccessOrExit(err = DBusMessageEncode(&sub, aConfig.mSecureDataRequests));
    SuccessOrExit(err = DBusMessageEncode(&sub, aConfig.mDeviceType));
    SuccessOrExit(err = DBusMessageEncode(&sub, aConfig.mNetworkData));

    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub) == true, err = OTBR_ERROR_DBUS);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, LinkModeConfig &aConfig)
{
    otbrError       err = OTBR_ERROR_DBUS;
    DBusMessageIter sub;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRUCT);
    dbus_message_iter_recurse(aIter, &sub);

    SuccessOrExit(DBusMessageExtract(&sub, aConfig.mRxOnWhenIdle));
    SuccessOrExit(DBusMessageExtract(&sub, aConfig.mSecureDataRequests));
    SuccessOrExit(DBusMessageExtract(&sub, aConfig.mDeviceType));
    SuccessOrExit(DBusMessageExtract(&sub, aConfig.mNetworkData));

    dbus_message_iter_next(aIter);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const Ip6Prefix &aPrefix)
{
    DBusMessageIter sub;
    otbrError       err = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub), err = OTBR_ERROR_DBUS);
    VerifyOrExit(aPrefix.mPrefix.size() <= OTBR_IP6_PREFIX_SIZE, err = OTBR_ERROR_DBUS);

    SuccessOrExit(DBusMessageEncode(&sub, aPrefix.mPrefix));
    SuccessOrExit(DBusMessageEncode(&sub, aPrefix.mLength));

    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub));

    err = OTBR_ERROR_NONE;
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, Ip6Prefix &aPrefix)
{
    DBusMessageIter sub;
    otbrError       err = OTBR_ERROR_NONE;

    dbus_message_iter_recurse(aIter, &sub);
    SuccessOrExit(err = DBusMessageExtract(&sub, aPrefix.mPrefix));
    VerifyOrExit(aPrefix.mPrefix.size() <= OTBR_IP6_PREFIX_SIZE, err = OTBR_ERROR_DBUS);
    SuccessOrExit(err = DBusMessageExtract(&sub, aPrefix.mLength));

    dbus_message_iter_next(aIter);
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const OnMeshPrefix &aPrefix)
{
    DBusMessageIter sub, flagsSub;
    auto args = std::tie(aPrefix.mPreferred, aPrefix.mSlaac, aPrefix.mDhcp, aPrefix.mConfigure, aPrefix.mDefaultRoute,
                         aPrefix.mOnMesh, aPrefix.mStable);
    otbrError err = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub), err = OTBR_ERROR_DBUS);
    SuccessOrExit(err = ConvertToDBusMessage(&sub, std::tie(aPrefix.mPrefix, aPrefix.mPreference)));
    VerifyOrExit(dbus_message_iter_open_container(&sub, DBUS_TYPE_STRUCT, nullptr, &flagsSub), err = OTBR_ERROR_DBUS);
    SuccessOrExit(err = ConvertToDBusMessage(&flagsSub, args));
    VerifyOrExit(dbus_message_iter_close_container(&sub, &flagsSub) == true, err = OTBR_ERROR_DBUS);
    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub) == true, err = OTBR_ERROR_DBUS);
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, OnMeshPrefix &aPrefix)
{
    DBusMessageIter sub, flagsSub;
    auto            args     = std::tie(aPrefix.mPrefix, aPrefix.mPreference);
    auto            flagArgs = std::tie(aPrefix.mPreferred, aPrefix.mSlaac, aPrefix.mDhcp, aPrefix.mConfigure,
                             aPrefix.mDefaultRoute, aPrefix.mOnMesh, aPrefix.mStable);
    otbrError       err      = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRUCT, err = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(aIter, &sub);
    SuccessOrExit(err = ConvertToTuple(&sub, args));
    VerifyOrExit(dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_STRUCT, err = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(&sub, &flagsSub);
    SuccessOrExit(err = ConvertToTuple(&flagsSub, flagArgs));
    dbus_message_iter_next(aIter);
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const MacCounters &aCounters)
{
    auto args = std::tie(aCounters.mTxTotal, aCounters.mTxUnicast, aCounters.mTxBroadcast, aCounters.mTxAckRequested,
                         aCounters.mTxAcked, aCounters.mTxNoAckRequested, aCounters.mTxData, aCounters.mTxDataPoll,
                         aCounters.mTxBeacon, aCounters.mTxBeaconRequest, aCounters.mTxOther, aCounters.mTxRetry,
                         aCounters.mTxErrCca, aCounters.mTxErrAbort, aCounters.mTxErrBusyChannel, aCounters.mRxTotal,
                         aCounters.mRxUnicast, aCounters.mRxBroadcast, aCounters.mRxData, aCounters.mRxDataPoll,
                         aCounters.mRxBeacon, aCounters.mRxBeaconRequest, aCounters.mRxOther,
                         aCounters.mRxAddressFiltered, aCounters.mRxDestAddrFiltered, aCounters.mRxDuplicated,
                         aCounters.mRxErrNoFrame, aCounters.mRxErrUnknownNeighbor, aCounters.mRxErrInvalidSrcAddr,
                         aCounters.mRxErrSec, aCounters.mRxErrFcs, aCounters.mRxErrOther);
    DBusMessageIter sub;
    otbrError       err = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub), err = OTBR_ERROR_DBUS);
    SuccessOrExit(err = ConvertToDBusMessage(&sub, args));
    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub) == true, err = OTBR_ERROR_DBUS);
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, MacCounters &aCounters)
{
    auto args = std::tie(aCounters.mTxTotal, aCounters.mTxUnicast, aCounters.mTxBroadcast, aCounters.mTxAckRequested,
                         aCounters.mTxAcked, aCounters.mTxNoAckRequested, aCounters.mTxData, aCounters.mTxDataPoll,
                         aCounters.mTxBeacon, aCounters.mTxBeaconRequest, aCounters.mTxOther, aCounters.mTxRetry,
                         aCounters.mTxErrCca, aCounters.mTxErrAbort, aCounters.mTxErrBusyChannel, aCounters.mRxTotal,
                         aCounters.mRxUnicast, aCounters.mRxBroadcast, aCounters.mRxData, aCounters.mRxDataPoll,
                         aCounters.mRxBeacon, aCounters.mRxBeaconRequest, aCounters.mRxOther,
                         aCounters.mRxAddressFiltered, aCounters.mRxDestAddrFiltered, aCounters.mRxDuplicated,
                         aCounters.mRxErrNoFrame, aCounters.mRxErrUnknownNeighbor, aCounters.mRxErrInvalidSrcAddr,
                         aCounters.mRxErrSec, aCounters.mRxErrFcs, aCounters.mRxErrOther);
    DBusMessageIter sub;
    otbrError       err = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRUCT, err = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(aIter, &sub);
    SuccessOrExit(err = ConvertToTuple(&sub, args));
    dbus_message_iter_next(aIter);
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const IpCounters &aCounters)
{
    DBusMessageIter sub;
    otbrError       err = OTBR_ERROR_NONE;
    auto args = std::tie(aCounters.mTxSuccess, aCounters.mRxSuccess, aCounters.mTxFailure, aCounters.mRxFailure);

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub), err = OTBR_ERROR_DBUS);
    SuccessOrExit(err = ConvertToDBusMessage(&sub, args));
    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub) == true, err = OTBR_ERROR_DBUS);
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, IpCounters &aCounters)
{
    DBusMessageIter sub;
    otbrError       err = OTBR_ERROR_NONE;
    auto args = std::tie(aCounters.mTxSuccess, aCounters.mRxSuccess, aCounters.mTxFailure, aCounters.mRxFailure);

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRUCT, err = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(aIter, &sub);
    SuccessOrExit(err = ConvertToTuple(&sub, args));
    dbus_message_iter_next(aIter);
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const ChildInfo &aChildInfo)
{
    DBusMessageIter sub;
    otbrError       err  = OTBR_ERROR_NONE;
    auto            args = std::tie(aChildInfo.mExtAddress, aChildInfo.mTimeout, aChildInfo.mAge, aChildInfo.mRloc16,
                         aChildInfo.mChildId, aChildInfo.mNetworkDataVersion, aChildInfo.mLinkQualityIn,
                         aChildInfo.mAverageRssi, aChildInfo.mLastRssi, aChildInfo.mFrameErrorRate,
                         aChildInfo.mMessageErrorRate, aChildInfo.mRxOnWhenIdle, aChildInfo.mSecureDataRequest,
                         aChildInfo.mFullThreadDevice, aChildInfo.mFullNetworkData, aChildInfo.mIsStateRestoring);

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub), err = OTBR_ERROR_DBUS);
    SuccessOrExit(err = ConvertToDBusMessage(&sub, args));
    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub) == true, err = OTBR_ERROR_DBUS);
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, ChildInfo &aChildInfo)
{
    DBusMessageIter sub;
    otbrError       err  = OTBR_ERROR_NONE;
    auto            args = std::tie(aChildInfo.mExtAddress, aChildInfo.mTimeout, aChildInfo.mAge, aChildInfo.mRloc16,
                         aChildInfo.mChildId, aChildInfo.mNetworkDataVersion, aChildInfo.mLinkQualityIn,
                         aChildInfo.mAverageRssi, aChildInfo.mLastRssi, aChildInfo.mFrameErrorRate,
                         aChildInfo.mMessageErrorRate, aChildInfo.mRxOnWhenIdle, aChildInfo.mSecureDataRequest,
                         aChildInfo.mFullThreadDevice, aChildInfo.mFullNetworkData, aChildInfo.mIsStateRestoring);

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRUCT, err = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(aIter, &sub);
    SuccessOrExit(err = ConvertToTuple(&sub, args));
    dbus_message_iter_next(aIter);
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const NeighborInfo &aNeighborInfo)
{
    DBusMessageIter sub;
    otbrError       err  = OTBR_ERROR_NONE;
    auto            args = std::tie(aNeighborInfo.mExtAddress, aNeighborInfo.mAge, aNeighborInfo.mRloc16,
                         aNeighborInfo.mLinkFrameCounter, aNeighborInfo.mMleFrameCounter, aNeighborInfo.mLinkQualityIn,
                         aNeighborInfo.mAverageRssi, aNeighborInfo.mLastRssi, aNeighborInfo.mFrameErrorRate,
                         aNeighborInfo.mMessageErrorRate, aNeighborInfo.mRxOnWhenIdle, aNeighborInfo.mSecureDataRequest,
                         aNeighborInfo.mFullThreadDevice, aNeighborInfo.mFullNetworkData, aNeighborInfo.mIsChild);

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub), err = OTBR_ERROR_DBUS);
    SuccessOrExit(err = ConvertToDBusMessage(&sub, args));
    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub) == true, err = OTBR_ERROR_DBUS);
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, NeighborInfo &aNeighborInfo)
{
    DBusMessageIter sub;
    otbrError       err  = OTBR_ERROR_NONE;
    auto            args = std::tie(aNeighborInfo.mExtAddress, aNeighborInfo.mAge, aNeighborInfo.mRloc16,
                         aNeighborInfo.mLinkFrameCounter, aNeighborInfo.mMleFrameCounter, aNeighborInfo.mLinkQualityIn,
                         aNeighborInfo.mAverageRssi, aNeighborInfo.mLastRssi, aNeighborInfo.mFrameErrorRate,
                         aNeighborInfo.mMessageErrorRate, aNeighborInfo.mRxOnWhenIdle, aNeighborInfo.mSecureDataRequest,
                         aNeighborInfo.mFullThreadDevice, aNeighborInfo.mFullNetworkData, aNeighborInfo.mIsChild);

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRUCT, err = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(aIter, &sub);
    SuccessOrExit(err = ConvertToTuple(&sub, args));
    dbus_message_iter_next(aIter);
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const LeaderData &aLeaderData)
{
    DBusMessageIter sub;
    otbrError       err  = OTBR_ERROR_NONE;
    auto            args = std::tie(aLeaderData.mPartitionId, aLeaderData.mWeighting, aLeaderData.mDataVersion,
                         aLeaderData.mStableDataVersion, aLeaderData.mLeaderRouterId);

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub), err = OTBR_ERROR_DBUS);
    SuccessOrExit(err = ConvertToDBusMessage(&sub, args));
    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub) == true, err = OTBR_ERROR_DBUS);
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, LeaderData &aLeaderData)
{
    DBusMessageIter sub;
    otbrError       err  = OTBR_ERROR_NONE;
    auto            args = std::tie(aLeaderData.mPartitionId, aLeaderData.mWeighting, aLeaderData.mDataVersion,
                         aLeaderData.mStableDataVersion, aLeaderData.mLeaderRouterId);

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRUCT, err = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(aIter, &sub);
    SuccessOrExit(err = ConvertToTuple(&sub, args));
    dbus_message_iter_next(aIter);
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const ChannelQuality &aQuality)
{
    DBusMessageIter sub;
    otbrError       err  = OTBR_ERROR_NONE;
    auto            args = std::tie(aQuality.mChannel, aQuality.mOccupancy);

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub));
    SuccessOrExit(err = ConvertToDBusMessage(&sub, args));
    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub) == true, err = OTBR_ERROR_DBUS);
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, ChannelQuality &aQuality)
{
    DBusMessageIter sub;
    otbrError       err  = OTBR_ERROR_NONE;
    auto            args = std::tie(aQuality.mChannel, aQuality.mOccupancy);

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRUCT, err = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(aIter, &sub);
    SuccessOrExit(err = ConvertToTuple(&sub, args));
    dbus_message_iter_next(aIter);
exit:
    return err;
}

} // namespace DBus
} // namespace otbr
