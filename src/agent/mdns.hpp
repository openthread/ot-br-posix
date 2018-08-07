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

#ifndef MDNS_HPP_
#define MDNS_HPP_

#include <sys/select.h>

#include "common/types.hpp"

namespace ot {

namespace BorderRouter {

namespace Mdns {

/**
 * MDNS state values.
 *
 */
enum State
{
    kStateIdle,  ///< Unable to publishing service.
    kStateReady, ///< Ready for publishing service.
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
     * @param[in]   aName               The name of this service.
     * @param[in]   aType               The type of this service.
     * @param[in]   aPort               The port number of this service.
     * @param[in]   ...                 Pointers to null-terminated string of key and value for text record.
     *                                  The last argument must be NULL.
     *
     * @retval  OTBR_ERROR_NONE     Successfully published or updated the service.
     * @retval  OTBR_ERROR_ERRNO    Failed to publish or update the service.
     *
     */
    virtual otbrError PublishService(uint16_t aPort, const char *aName, const char *aType, ...) = 0;

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
     * @param[in]   aHost               The host where these services is residing on.
     * @param[in]   aDomain             The domain to register in.
     * @param[in]   aHandler            The function to be called when this service state changed.
     * @param[in]   aContext            A pointer to application-specific context.
     *
     * @returns A pointer to the newly created MDNS publisher.
     *
     */
    static Publisher *Create(int          aProtocol,
                             const char * aHost,
                             const char * aDomain,
                             StateHandler aHandler,
                             void *       aContext);

    /**
     * This function destroies the MDNS publisher.
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

} // namespace BorderRouter

} // namespace ot

#endif // MDNS_HPP_
