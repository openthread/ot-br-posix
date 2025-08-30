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

#ifndef EXTENSIONS_SERVICES_HPP_
#define EXTENSIONS_SERVICES_HPP_

#include "common/mainloop.hpp"
#include "openthread/instance.h"
#include "openthread/ip6.h"

namespace otbr {
namespace rest {

class ActionsList;
class CommissionerManager;
class DevicesCollection;
class DiagnosticsCollection;
class NetworkDiagHandler;

struct ServiceList;

enum AddressType
{
    kAddressTypeExt = 0,
    kAddressTypeMleid,
    kAddressTypeRloc,
};

const char *AddressTypeToString(AddressType aType);

class Services : public MainloopProcessor
{
public:
    Services(void);
    ~Services(void);

    void Init(otInstance *aInstance);

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

    /**
     * Attempts to convert a address string to an IPv6 address.
     *
     * @note If the address type is kAddressTypeExt a lookup using the devices
     *       collection is required.
     *
     * @param[in]  aAddressString  The address to convert.
     * @param[in]  aType           The type of address of aAddressString.
     * @param[out] aAddress        The converted address.
     *
     * @retval OT_ERROR_NONE       The address conversion was performed successfully.
     * @retval OT_ERROR_PARSE      The address string was not a valid address of type aType.
     * @retval OT_ERROR_NOT_FOUND  The address lookup failed.
     */
    otError LookupAddress(const char *aAddressString, AddressType aType, otIp6Address &aAddress);

    otInstance *GetInstance(void) { return mInstance; }

    ActionsList &GetActionsList(void);

    CommissionerManager &GetCommissionerManager(void);

    DevicesCollection &GetDevicesCollection(void);

    DiagnosticsCollection &GetDiagnosticsCollection(void);

    NetworkDiagHandler &GetNetworkDiagHandler(void);

private:
    otInstance  *mInstance;
    ServiceList *mServices;
};

} // namespace rest
} // namespace otbr

#endif // EXTENSIONS_SERVICES_HPP_
