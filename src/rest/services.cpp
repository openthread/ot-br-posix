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

#include "services.hpp"

#include "actions_list.hpp"
#include "commissioner_manager.hpp"
#include "rest_server_common.hpp"
#include "common/code_utils.hpp"

namespace otbr {
namespace rest {

struct ServiceList
{
    ServiceList(Services &aServices, otInstance *aInstance)
        : mActionsList(aServices)
        , mCommissionerManager(aInstance)
    {
    }

    ~ServiceList()
    {
        // Actions may use other services so they need to be destroyed first
        mActionsList.DeleteAllActions();
    }

    ActionsList         mActionsList;
    CommissionerManager mCommissionerManager;
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

void Services::Process(void)
{
    mServices->mCommissionerManager.Process();
    mServices->mActionsList.UpdateAllActions();
}

ActionsList &Services::GetActionsList(void)
{
    return mServices->mActionsList;
}

CommissionerManager &Services::GetCommissionerManager(void)
{
    return mServices->mCommissionerManager;
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
