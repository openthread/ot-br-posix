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
 *   This file includes definition for MDNS service.
 */

#ifndef OTBR_AGENT_MDNS_HPP_
#define OTBR_AGENT_MDNS_HPP_

#include <vector>

#include <sys/select.h>

#include "common/types.hpp"

namespace otbr {

namespace Mdns {

/**
 * @addtogroup border-router-mdns
 *
 * @brief
 *   This module includes definition for MDNS service.
 *
 * @{
 */

/**
 * This interface defines the functionality of MDNS service.
 *
 */
class Publisher
{
public:
    /**
     * This structure represents a name/value pair of the TXT record.
     *
     */
    struct TxtEntry
    {
        const char *   mName;        ///< The name of the TXT entry.
        const uint8_t *mValue;       ///< The value of the TXT entry.
        size_t         mValueLength; ///< The length of the value of the TXT entry.

        TxtEntry(const char *aName, const char *aValue)
            : TxtEntry(aName, reinterpret_cast<const uint8_t *>(aValue), strlen(aValue))
        {
        }

        TxtEntry(const char *aName, const uint8_t *aValue, size_t aValueLength)
            : mName(aName)
            , mValue(aValue)
            , mValueLength(aValueLength)
        {
        }
    };

    typedef std::vector<TxtEntry> TxtList;

    /**
     * MDNS state values.
     *
     */
    enum class State
    {
        kIdle,  ///< Unable to publishing service.
        kReady, ///< Ready for publishing service.
    };

    /**
     * This function pointer is called when MDNS service state changed.
     *
     * @param[in]   aContext        A pointer to application-specific context.
     * @param[in]   aState          The new state.
     *
     */
    typedef void (*StateHandler)(void *aContext, State aState);

    /**
     * This method starts the MDNS service.
     *
     * @retval OTBR_ERROR_NONE  Successfully started MDNS service;
     * @retval OTBR_ERROR_MDNS  Failed to start MDNS service.
     *
     */
    virtual otbrError Start(void) = 0;

    /**
     * This method stops the MDNS service.
     *
     */
    virtual void Stop(void) = 0;

    /**
     * This method checks if publisher has been started.
     *
     * @retval true     Already started.
     * @retval false    Not started.
     *
     */
    virtual bool IsStarted(void) const = 0;

    /**
     * This method publishes or updates a service.
     *
     * @param[in]   aHostName           The name of the host which this service resides on. If NULL is provided,
     *                                  this service resides on local host and it is the implementation to provide
     *                                  specific host name. Otherwise, the caller MUST publish the host with method
     *                                  PublishHost.
     * @param[in]   aName               The name of this service.
     * @param[in]   aType               The type of this service.
     * @param[in]   aPort               The port number of this service.
     * @param[in]   aTxtList            A list of TXT name/value pairs.
     *
     * @retval  OTBR_ERROR_NONE     Successfully published or updated the service.
     * @retval  OTBR_ERROR_ERRNO    Failed to publish or update the service.
     *
     */
    virtual otbrError PublishService(const char *   aHostName,
                                     uint16_t       aPort,
                                     const char *   aName,
                                     const char *   aType,
                                     const TxtList &aTxtList) = 0;

    /**
     * This method un-publishes a service.
     *
     * @param[in]   aName               The name of this service.
     * @param[in]   aType               The type of this service.
     *
     * @retval  OTBR_ERROR_NONE     Successfully un-published the service.
     * @retval  OTBR_ERROR_ERRNO    Failed to un-publish the service.
     *
     */
    virtual otbrError UnpublishService(const char *aName, const char *aType) = 0;

    /**
     * This method publishes or updates a host.
     *
     * Publishing a host is advertising an A/AAAA RR for the host name. This method should be called
     * before a service with non-null host name is published.
     *
     * @param[in]  aName           The name of the host.
     * @param[in]  aAddress        The address of the host.
     * @param[in]  aAddressLength  The length of @p aAddress.
     *
     * @retval  OTBR_ERROR_NONE          Successfully published or updated the host.
     * @retval  OTBR_ERROR_INVALID_ARGS  The arguments are not valid.
     * @retval  OTBR_ERROR_ERRNO         Failed to publish or update the host.
     *
     */
    virtual otbrError PublishHost(const char *aName, const uint8_t *aAddress, uint8_t aAddressLength) = 0;

    /**
     * This method un-publishes a host.
     *
     * @param[in]  aName  A host name.
     *
     * @retval  OTBR_ERROR_NONE     Successfully un-published the host.
     * @retval  OTBR_ERROR_ERRNO    Failed to un-publish the host.
     *
     */
    virtual otbrError UnpublishHost(const char *aName) = 0;

    /**
     * This method performs the MDNS processing.
     *
     * @param[in]   aReadFdSet          A reference to fd_set ready for reading.
     * @param[in]   aWriteFdSet         A reference to fd_set ready for writing.
     * @param[in]   aErrorFdSet         A reference to fd_set with error occurred.
     *
     */
    virtual void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet) = 0;

    /**
     * This method updates the fd_set and timeout for mainloop.
     *
     * @param[inout]    aReadFdSet      A reference to fd_set for polling read.
     * @param[inout]    aWriteFdSet     A reference to fd_set for polling read.
     * @param[inout]    aErrorFdSet     A reference to fd_set for polling error.
     * @param[inout]    aMaxFd          A reference to the current max fd in @p aReadFdSet and @p aWriteFdSet.
     * @param[inout]    aTimeout        A reference to the timeout. Update this value if the MDNS service has
     *                                  pending process in less than its current value.
     *
     */
    virtual void UpdateFdSet(fd_set & aReadFdSet,
                             fd_set & aWriteFdSet,
                             fd_set & aErrorFdSet,
                             int &    aMaxFd,
                             timeval &aTimeout) = 0;

    virtual ~Publisher(void) {}

    /**
     * This function creates a MDNS publisher.
     *
     * @param[in]   aProtocol           Protocol to use for publishing. AF_INET6, AF_INET or AF_UNSPEC.
     * @param[in]   aDomain             The domain to register in. Set nullptr to use default mDNS domain ("local.").
     * @param[in]   aHandler            The function to be called when this service state changed.
     * @param[in]   aContext            A pointer to application-specific context.
     *
     * @returns A pointer to the newly created MDNS publisher.
     *
     */
    static Publisher *Create(int aProtocol, const char *aDomain, StateHandler aHandler, void *aContext);

    /**
     * This function destroys the MDNS publisher.
     *
     * @param[in]   aPublisher          A pointer to the publisher.
     *
     */
    static void Destroy(Publisher *aPublisher);
};

/**
 * @}
 */

} // namespace Mdns

} // namespace otbr

#endif // OTBR_AGENT_MDNS_HPP_
