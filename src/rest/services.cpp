/*
 *  Copyright (c) 2025, The OpenThread Authors.
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

#include "services.hpp"

#include "actions_list.hpp"
#include "commissioner_manager.hpp"
#include "network_diag_handler.hpp"
#include "rest_devices_coll.hpp"
#include "rest_diagnostics_coll.hpp"
#include "rest_server_common.hpp"
#include "common/code_utils.hpp"

namespace otbr {
namespace rest {

struct ServiceList
{
    ServiceList(Services &aServices, otInstance *aInstance)
        : mActionsList(aServices)
        , mCommissionerManager(aInstance)
        , mDevicesCollection()
        , mDiagnosticsCollection()
        , mNetworkDiagHandler(aServices, aInstance)
    {
    }

    ~ServiceList()
    {
        // Actions may use other services so they need to be destroyed first
        mActionsList.DeleteAllActions();
    }

    ActionsList           mActionsList;
    CommissionerManager   mCommissionerManager;
    DevicesCollection     mDevicesCollection;
    DiagnosticsCollection mDiagnosticsCollection;
    NetworkDiagHandler    mNetworkDiagHandler;
};

Services::Services(void)
    : mInstance(nullptr)
    , mServices(nullptr)
{
}

Services::~Services(void)
{
    if (mServices != nullptr)
    {
        delete mServices;
    }
}

void Services::Init(otInstance *aInstance)
{
    mInstance = aInstance;
    mServices = new ServiceList(*this, aInstance);
}

void Services::Update(MainloopContext &aMainloop)
{
    OT_UNUSED_VARIABLE(aMainloop);
    return; // No update needed for Services
}

void Services::Process(const MainloopContext &aMainloop)
{
    OT_UNUSED_VARIABLE(aMainloop);
    mServices->mCommissionerManager.Process();
    mServices->mNetworkDiagHandler.Process();
    mServices->mActionsList.UpdateAllActions();
}

otError Services::LookupAddress(const char *aAddressString, AddressType aType, otIp6Address &aAddress)
{
    otError                  error = OT_ERROR_NONE;
    otIp6InterfaceIdentifier mlEidIid;
    const otMeshLocalPrefix *prefix = otThreadGetMeshLocalPrefix(mInstance);
    const ThreadDevice      *device;
    uint16_t                 rloc = 0xfffe;

    VerifyOrExit(aAddressString != nullptr, error = OT_ERROR_PARSE);

    switch (aType)
    {
    case kAddressTypeExt:
        VerifyOrExit(strlen(aAddressString) == 16, error = OT_ERROR_PARSE);

        device = dynamic_cast<const ThreadDevice *>(mServices->mDevicesCollection.GetItem(std::string(aAddressString)));
        VerifyOrExit(device != nullptr, error = OT_ERROR_NOT_FOUND);

        memcpy(mlEidIid.mFields.m8, device->mDeviceInfo.mMlEidIid.m8, OT_IP6_IID_SIZE);
        combineMeshLocalPrefixAndIID(prefix, &mlEidIid, &aAddress);
        break;

    case kAddressTypeMleid:
        VerifyOrExit(strlen(aAddressString) == 16, error = OT_ERROR_PARSE);

        SuccessOrExit(str_to_m8(mlEidIid.mFields.m8, aAddressString, OT_IP6_IID_SIZE), error = OT_ERROR_PARSE);
        combineMeshLocalPrefixAndIID(prefix, &mlEidIid, &aAddress);
        break;

    case kAddressTypeRloc:
        VerifyOrExit(strlen(aAddressString) == 6, error = OT_ERROR_PARSE);

        sscanf(aAddressString, "%hx", &rloc);
        memcpy(&aAddress, otThreadGetRloc(mInstance), OT_IP6_ADDRESS_SIZE);
        aAddress.mFields.m16[7] = htons(rloc);
        break;
    }

exit:
    return error;
}

ActionsList &Services::GetActionsList(void)
{
    return mServices->mActionsList;
}

CommissionerManager &Services::GetCommissionerManager(void)
{
    return mServices->mCommissionerManager;
}

DevicesCollection &Services::GetDevicesCollection(void)
{
    return mServices->mDevicesCollection;
}

DiagnosticsCollection &Services::GetDiagnosticsCollection(void)
{
    return mServices->mDiagnosticsCollection;
}

NetworkDiagHandler &Services::GetNetworkDiagHandler(void)
{
    return mServices->mNetworkDiagHandler;
}

const char *AddressTypeToString(AddressType aType)
{
    static const char *const kTypeStrings[] = {
        "extended", // (0) kAddressTypeExt
        "mleid",    // (1) kAddressTypeMleid
        "rloc",     // (2) kAddressTypeRloc
    };

    static_assert(kAddressTypeExt == 0, "kAddressTypeExt value is incorrect");
    static_assert(kAddressTypeMleid == 1, "kAddressTypeMleid value is incorrect");
    static_assert(kAddressTypeRloc == 2, "kAddressTypeRloc value is incorrect");

    return kTypeStrings[aType];
}

} // namespace rest
} // namespace otbr
