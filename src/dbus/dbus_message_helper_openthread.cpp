#include <string.h>

#include "dbus/dbus_message_helper.hpp"

namespace otbr {
namespace dbus {

otbrError DBusMessageExtract(DBusMessageIter *aIter, otError &aValue)
{
    uint8_t   val = OT_ERROR_FAILED;
    otbrError err = DBusMessageExtract(aIter, val);

    VerifyOrExit(err == OTBR_ERROR_NONE);
    aValue = static_cast<otError>(val);
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, otActiveScanResult &aValue)
{
    DBusMessageIter      sub;
    otbrError            err = OTBR_ERROR_DBUS;
    std::string          networkName;
    std::vector<uint8_t> steeringData;
    uint64_t             extAddr, extPanId;
    uint8_t              version;
    bool                 isNative, isJoinable;

    memset(&aValue, 0, sizeof(aValue));

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRUCT);
    dbus_message_iter_recurse(aIter, &sub);

    SuccessOrExit(DBusMessageExtract(&sub, extAddr));
    memcpy(&aValue.mExtAddress, &extAddr, sizeof(extAddr));

    SuccessOrExit(DBusMessageExtract(&sub, networkName));
    VerifyOrExit(networkName.size() < sizeof(aValue.mNetworkName.m8));
    memcpy(&aValue.mNetworkName.m8, networkName.c_str(), networkName.length());

    SuccessOrExit(DBusMessageExtract(&sub, extPanId));
    memcpy(&aValue.mExtendedPanId, &extPanId, sizeof(extPanId));

    SuccessOrExit(DBusMessageExtract(&sub, steeringData));
    VerifyOrExit(steeringData.size() <= sizeof(aValue.mSteeringData.m8));
    memcpy(&aValue.mSteeringData.m8, &steeringData[0], steeringData.size());
    aValue.mSteeringData.mLength = static_cast<uint8_t>(steeringData.size());

    SuccessOrExit(DBusMessageExtract(&sub, aValue.mPanId));
    SuccessOrExit(DBusMessageExtract(&sub, aValue.mJoinerUdpPort));
    SuccessOrExit(DBusMessageExtract(&sub, aValue.mChannel));
    SuccessOrExit(DBusMessageExtract(&sub, aValue.mRssi));
    SuccessOrExit(DBusMessageExtract(&sub, aValue.mLqi));

    SuccessOrExit(DBusMessageExtract(&sub, version));
    aValue.mVersion = version;

    SuccessOrExit(DBusMessageExtract(&sub, isNative));
    aValue.mIsNative = isNative;

    SuccessOrExit(DBusMessageExtract(&sub, isJoinable));
    aValue.mIsJoinable = isJoinable;

    dbus_message_iter_next(aIter);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const otError &aValue)
{
    return DBusMessageEncode(aIter, static_cast<uint8_t>(aValue));
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const otActiveScanResult &aValue)
{
    DBusMessageIter sub;
    otbrError       err        = OTBR_ERROR_DBUS;
    uint8_t         version    = aValue.mVersion;
    bool            isNative   = aValue.mIsNative;
    bool            isJoinable = aValue.mIsJoinable;

    std::vector<uint8_t> steeringData(aValue.mSteeringData.m8, aValue.mSteeringData.m8 + aValue.mSteeringData.mLength);

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_STRUCT, nullptr, &sub) == true);
    VerifyOrExit(dbus_message_iter_append_basic(&sub, DBUS_TYPE_UINT64, &aValue.mExtAddress) == true);
    SuccessOrExit(DBusMessageEncode(&sub, aValue.mNetworkName.m8));
    VerifyOrExit(dbus_message_iter_append_basic(&sub, DBUS_TYPE_UINT64, &aValue.mExtendedPanId) == true);
    SuccessOrExit(DBusMessageEncode(&sub, steeringData));
    SuccessOrExit(DBusMessageEncode(&sub, aValue.mPanId));
    SuccessOrExit(DBusMessageEncode(&sub, aValue.mJoinerUdpPort));
    SuccessOrExit(DBusMessageEncode(&sub, aValue.mChannel));
    SuccessOrExit(DBusMessageEncode(&sub, aValue.mRssi));
    SuccessOrExit(DBusMessageEncode(&sub, aValue.mLqi));

    SuccessOrExit(DBusMessageEncode(&sub, version));

    SuccessOrExit(DBusMessageEncode(&sub, isNative));

    SuccessOrExit(DBusMessageEncode(&sub, isJoinable));

    VerifyOrExit(dbus_message_iter_close_container(aIter, &sub) == true);

    err = OTBR_ERROR_NONE;
exit:
    return err;
}

} // namespace dbus
} // namespace otbr
