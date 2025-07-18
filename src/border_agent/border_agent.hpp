/*
 *    Copyright (c) 2017, The OpenThread Authors.
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

/**
 * @file
 *   This file includes definition for Thread border agent.
 */

#ifndef OTBR_AGENT_BORDER_AGENT_HPP_
#define OTBR_AGENT_BORDER_AGENT_HPP_

#include "openthread-br/config.h"

#if OTBR_ENABLE_BORDER_AGENT

#include <vector>

#include <stdint.h>

#include "backbone_router/backbone_agent.hpp"
#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "host/rcp_host.hpp"
#include "mdns/mdns.hpp"
#include "sdp_proxy/advertising_proxy.hpp"
#include "sdp_proxy/discovery_proxy.hpp"
#include "trel_dnssd/trel_dnssd.hpp"

#ifndef OTBR_VENDOR_NAME
#define OTBR_VENDOR_NAME "OpenThread"
#endif

#ifndef OTBR_PRODUCT_NAME
#define OTBR_PRODUCT_NAME "BorderRouter"
#endif

#ifndef OTBR_MESHCOP_SERVICE_INSTANCE_NAME
#define OTBR_MESHCOP_SERVICE_INSTANCE_NAME (OTBR_VENDOR_NAME " " OTBR_PRODUCT_NAME)
#endif

namespace otbr {

/**
 * @addtogroup border-router-border-agent
 *
 * @brief
 *   This module includes definition for Thread border agent
 *
 * @{
 */

/**
 * This class implements Thread border agent functionality.
 */
class BorderAgent : private NonCopyable
#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    ,
                    public Mdns::StateObserver
#endif
{
public:
    typedef std::vector<uint8_t>                        TxtData;          ///< TXT data (encoded).
    typedef std::map<std::string, std::vector<uint8_t>> VendorTxtEntries; ///< Vendor TXT entry map.

    /**
     * Callback to signal when vendor TXT Data entries get set or changed.
     */
    using VendorTxtDataChangedCallback = std::function<void(const TxtData &)>;

    /**
     * The constructor to initialize the Thread border agent.
     *
     * @param[in] aPublisher  A reference to the mDNS Publisher.
     */
#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    BorderAgent(Mdns::Publisher &aPublisher);
#else
    BorderAgent(void);
#endif

    ~BorderAgent(void) = default;

    /**
     * Deinitializes the Thread Border Agent.
     */
    void Deinit(void);

    /**
     * Overrides MeshCoP service (i.e. _meshcop._udp) instance name, product name, vendor name and vendor OUI.
     *
     * This method must be called before this BorderAgent is enabled by SetEnabled.
     *
     * @param[in] aServiceInstanceName    The service instance name; suffix may be appended to this value to avoid
     *                                    name conflicts.
     * @param[in] aProductName            The product name; must not exceed length of kMaxProductNameLength
     *                                    and an empty string will be ignored.
     * @param[in] aVendorName             The vendor name; must not exceed length of kMaxVendorNameLength
     *                                    and an empty string will be ignored.
     * @param[in] aVendorOui              The vendor OUI; must have length of 3 bytes or be empty and ignored.
     * @param[in] aNonStandardTxtEntries  Non-standard (vendor-specific) TXT entries whose key MUST start with "v"
     *
     * @returns OTBR_ERROR_INVALID_ARGS  If aVendorName, aProductName or aVendorOui exceeds the
     *                                   allowed ranges or invalid keys are found in aNonStandardTxtEntries
     * @returns OTBR_ERROR_NONE          If successfully set the meshcop service values.
     */
    otbrError SetMeshCoPServiceValues(const std::string              &aServiceInstanceName,
                                      const std::string              &aProductName,
                                      const std::string              &aVendorName,
                                      const std::vector<uint8_t>     &aVendorOui             = {},
                                      const Mdns::Publisher::TxtList &aNonStandardTxtEntries = {});

    /**
     * This method enables/disables the Border Agent.
     *
     * @param[in] aIsEnabled  Whether to enable the Border Agent.
     */
    void SetEnabled(bool aIsEnabled);

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE

    /**
     * This method handles mDNS publisher's state changes.
     *
     * @param[in] aState  The state of mDNS publisher.
     */
    void HandleMdnsState(Mdns::Publisher::State aState) override;

    /**
     * This method handles Border Agent state changes.
     *
     * @param[in] aIsActive     If the Border Agent is active.
     * @param[in] aPort         The UDP port of the Border Agent service.
     * @param[in] aOtTxtData    The MeshCoP TXT data generated by OT.
     */
    void HandleBorderAgentMeshCoPServiceChanged(bool aIsActive, uint16_t aPort, const TxtData &aOtTxtData);

    /**
     * This method handles Epskc state changes.
     *
     * @param[in] aEpskcState    The Epskc state.
     * @param[in] aPort          The UDP port of the Epskc service if it's active.
     */
    void HandleEpskcStateChanged(otBorderAgentEphemeralKeyState aEpskcState, uint16_t aPort);

#endif // OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE

    /**
     * Sets the callback to be notified when vendor TXT Data gets set or changed.
     *
     * @param[in] aCallback  The callback.
     */
    void SetVendorTxtDataChangedCallback(VendorTxtDataChangedCallback aCallback);

    /**
     * This method creates ephemeral key in the Border Agent.
     *
     * @param[out] aEphemeralKey  The ephemeral key digit string of length 9 with first 8 digits randomly
     *                            generated, and the last 9th digit as verhoeff checksum.
     *
     * @returns OTBR_ERROR_INVALID_ARGS  If Verhoeff checksum calculate returns error.
     * @returns OTBR_ERROR_NONE          If successfully generate the ePSKc.
     */
    static otbrError CreateEphemeralKey(std::string &aEphemeralKey);

#if OTBR_ENABLE_DBUS_SERVER
    /**
     * This method updates the vendor MeshCoP TXT entries.
     *
     * @param[in] aVendorEntries  A map of vendor MeshCoP TXT entries.
     */
    void UpdateVendorMeshCoPTxtEntries(const VendorTxtEntries &aVendorEntries);
#endif

private:
    void ClearState(void);
    void Start(void);
    void Stop(void);
    bool IsEnabled(void) const { return mIsEnabled; }

    void EncodeVendorTxtData(const VendorTxtEntries &aVendorEntries);

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    void PublishMeshCoPService(void);
    void UpdateMeshCoPService(void);
    void UnpublishMeshCoPService(void);

    std::string GetServiceInstanceName(void) const;
    std::string GetAlternativeServiceInstanceName(void) const;

    void PublishEpskcService(uint16_t aPort);
    void UnpublishEpskcService(void);
#endif

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    Mdns::Publisher &mPublisher;
#endif
    bool mIsEnabled;

    std::vector<uint8_t> mVendorOui;

    std::string mVendorName;
    std::string mProductName;

    TxtData                      mVendorTxtData; // Encoded vendor-specific TXT data.
    VendorTxtDataChangedCallback mVendorTxtDataChangedCallback;

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    TxtData mOtTxtData; // Encoded TXT data from OpenThread core Border Agent module
#endif

    // The base service instance name typically consists of the vendor and product name. But it can
    // also be overridden by `OTBR_MESHCOP_SERVICE_INSTANCE_NAME` or method `SetMeshCoPServiceValues()`.
    // For example, this value can be "OpenThread Border Router".
    std::string mBaseServiceInstanceName;

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    // The actual instance name advertised in the mDNS service. This is usually the value of
    // `mBaseServiceInstanceName` plus the Extended Address and optional random number for avoiding
    // conflicts. For example, this value can be "OpenThread Border Router #7AC3" or
    // "OpenThread Border Router #7AC3 (14379)".
    std::string mServiceInstanceName;

    bool         mIsInitialized;
    otExtAddress mExtAddress;
    uint16_t     mMeshCoPUdpPort;
    bool         mBaIsActive;
#endif
};

/**
 * @}
 */

} // namespace otbr

#endif // OTBR_ENABLE_BORDER_AGENT

#endif // OTBR_AGENT_BORDER_AGENT_HPP_
