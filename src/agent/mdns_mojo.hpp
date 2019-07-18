/*
 *    Copyright (c) 2019, The OpenThread Authors.
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
 *   This file includes definition for MDNS service based on mojo.
 */

#ifndef MDNS_MOJO_HPP__
#define MDNS_MOJO_HPP__

#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/message_loop/message_pump_for_io.h>
#ifndef TEST_IN_CHROMIUM
#include <base/task_scheduler/post_task.h>
#else
#include <base/task/post_task.h>
#endif
#include <chromecast/external_mojo/public/cpp/common.h>
#ifndef TEST_IN_CHROMIUM
#include <chromecast/external_mojo/public/cpp/external_connector.h>
#else
#include <chromecast/external_mojo/external_service_support/external_connector.h>
#endif

#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef TEST_IN_CHROMIUM
#include "agent/mdns.hpp"
#include "third_party/chromecast/mojom/mdns.mojom.h"
#else
#include "mdns.hpp"
#include "mdns_mojo_test/public/mojom/mdns.mojom.h"
#endif

#ifndef TEST_IN_CHROMIUM
#define MOJO_CONNECTOR_NS chromecast::external_mojo
#else
#define MOJO_CONNECTOR_NS chromecast::external_service_support
#endif

namespace ot {
namespace BorderRouter {
namespace Mdns {

class MdnsMojoPublisher : public Publisher
{
public:
    /**
     * The constructor to MdnsMojoPublisher
     *
     * @param[in]   aHandler    The callback function for mojo connect state change
     * @param[in]   aContext    The context for callback function
     *
     */
    MdnsMojoPublisher(StateHandler aHandler, void *aContext);

    /**
     * This method starts the MDNS service.
     *
     * @retval OTBR_ERROR_NONE  Successfully started MDNS service;
     * @retval OTBR_ERROR_MDNS  Failed to start MDNS service.
     *
     */
    otbrError Start(void) override;

    /**
     * This method stops the MDNS service.
     *
     */
    void Stop(void) override;

    /**
     * This method checks if publisher has been started.
     *
     * @retval true     Already started.
     * @retval false    Not started.
     *
     */
    bool IsStarted(void) const override;

    /**
     * This method publishes or updates a service.
     *
     * @param[in]   aName               The name of this service.
     * @param[in]   aType               The type of this service.
     * @param[in]   aPort               The port number of this service.
     * @param[in]   ...                 Pointers to null-terminated string of key
     *                                  and value for text record.
     *                                  The last argument must be NULL.
     *
     * @retval  OTBR_ERROR_NONE     Successfully published or updated the service.
     * @retval  OTBR_ERROR_ERRNO    Failed to publish or update the service.
     *
     */
    otbrError PublishService(uint16_t aPort, const char *aName, const char *aType, ...) override;

    /**
     * This method performs the MDNS processing.
     *
     * @param[in]   aReadFdSet          A reference to fd_set ready for reading.
     * @param[in]   aWriteFdSet         A reference to fd_set ready for writing.
     * @param[in]   aErrorFdSet         A reference to fd_set with error occurred.
     *
     */
    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet) override;

    /**
     * This method updates the fd_set and timeout for mainloop.
     *
     * @param[inout]    aReadFdSet      A reference to fd_set for polling read.
     * @param[inout]    aWriteFdSet     A reference to fd_set for polling read.
     * @param[inout]    aErrorFdSet     A reference to fd_set for polling error.
     * @param[inout]    aMaxFd          A reference to the current max fd in @p
     *                                  aReadFdSet and @p aWriteFdSet.
     * @param[inout]    aTimeout        A reference to the timeout. Update this
     *                                  value if the MDNS service has pending
     *                                  process in less than its current
     *                                  value.
     *
     */
    void UpdateFdSet(fd_set & aReadFdSet,
                     fd_set & aWriteFdSet,
                     fd_set & aErrorFdSet,
                     int &    aMaxFd,
                     timeval &aTimeout) override;

    ~MdnsMojoPublisher(void) override;

private:
    void PublishServiceTask(uint16_t                        aPort,
                            const std::string &             aType,
                            const std::string &             aInstanceName,
                            const std::vector<std::string> &aText);

    void StopPublishTask(void);

    void LaunchMojoThreads(void);
    void TearDownMojoThreads(void);
    void ConnectToMojo(void);
    void mMojoConnectCb(std::unique_ptr<MOJO_CONNECTOR_NS::ExternalConnector> aConnector);
    void mMojoDisconnectedCb(void);
    void mRegisterServiceCb(chromecast::mojom::MdnsResult aResult);

    scoped_refptr<base::SingleThreadTaskRunner>           mMojoTaskRunner;
    std::unique_ptr<std::thread>                          mMojoCoreThread;
    base::Closure                                         mMojoCoreThreadQuitClosure;
    std::unique_ptr<MOJO_CONNECTOR_NS::ExternalConnector> mConnector;
    chromecast::mojom::MdnsResponderPtr                   mResponder;

    std::vector<std::pair<std::string, std::string>> mPublishedServices;

    StateHandler mStateHandler;
    void *       mContext;
    bool         mStarted;

    MdnsMojoPublisher(const MdnsMojoPublisher &) = delete;
    MdnsMojoPublisher &operator=(const MdnsMojoPublisher &) = delete;
};

} // namespace Mdns
} // namespace BorderRouter
} // namespace ot

#endif
