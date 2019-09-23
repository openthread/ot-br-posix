/*
 *  Copyright (c) 2019, The OpenThread Authors.
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

#ifndef OTUBUS_HPP_
#define OTUBUS_HPP_

#include <stdarg.h>
#include <time.h>

#include <openthread/ip6.h>
#include <openthread/link.h>
#include <openthread/udp.h>

#include "common/code_utils.hpp"
#include "common/message.hpp"
#include "net/socket.hpp"

extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include <libubox/ustream.h>
#include <libubox/utils.h>
#include <libubus.h>
}

namespace ot {
namespace BorderRouter {
namespace Ncp {
class ControllerOpenThread;
}

namespace ubus {

/**
 * @namespace otubus
 *
 * @brief
 *   This namespace contains definitions for ubus related instance.
 *
 */
extern int             uloop_available;
extern pthread_mutex_t uloop_thread_mutex;
extern pthread_cond_t  uloop_cv;

class OtUbusServer
{
public:
    /**
     * Constructor
     *
     * @param[in]  aInstance  The OpenThread instance structure.
     */
    static void initialize(Ncp::ControllerOpenThread *aController);

    static OtUbusServer &getInstance();

    void        InstallUbusObject();
    static int  Ubus_scan_handler(struct ubus_context *     aContext,
                                  struct ubus_object *      obj,
                                  struct ubus_request_data *req,
                                  const char *              method,
                                  struct blob_attr *        msg);
    static int  Ubus_channel_handler(struct ubus_context *     aContext,
                                     struct ubus_object *      obj,
                                     struct ubus_request_data *req,
                                     const char *              method,
                                     struct blob_attr *        msg);
    static int  Ubus_networkname_handler(struct ubus_context *     aContext,
                                         struct ubus_object *      obj,
                                         struct ubus_request_data *req,
                                         const char *              method,
                                         struct blob_attr *        msg);
    static int  Ubus_state_handler(struct ubus_context *     aContext,
                                   struct ubus_object *      obj,
                                   struct ubus_request_data *req,
                                   const char *              method,
                                   struct blob_attr *        msg);
    static int  Ubus_panid_handler(struct ubus_context *     aContext,
                                   struct ubus_object *      obj,
                                   struct ubus_request_data *req,
                                   const char *              method,
                                   struct blob_attr *        msg);
    static int  Ubus_pskc_handler(struct ubus_context *     aContext,
                                  struct ubus_object *      obj,
                                  struct ubus_request_data *req,
                                  const char *              method,
                                  struct blob_attr *        msg);
    static int  Ubus_masterkey_handler(struct ubus_context *     aContext,
                                       struct ubus_object *      obj,
                                       struct ubus_request_data *req,
                                       const char *              method,
                                       struct blob_attr *        msg);
    static int  Ubus_threadstart_handler(struct ubus_context *     aContext,
                                         struct ubus_object *      obj,
                                         struct ubus_request_data *req,
                                         const char *              method,
                                         struct blob_attr *        msg);
    static int  Ubus_threadstop_handler(struct ubus_context *     aContext,
                                        struct ubus_object *      obj,
                                        struct ubus_request_data *req,
                                        const char *              method,
                                        struct blob_attr *        msg);
    static int  Ubus_macfilter_addr_handler(struct ubus_context *     aContext,
                                            struct ubus_object *      obj,
                                            struct ubus_request_data *req,
                                            const char *              method,
                                            struct blob_attr *        msg);
    static int  Ubus_macfilter_state_handler(struct ubus_context *     aContext,
                                             struct ubus_object *      obj,
                                             struct ubus_request_data *req,
                                             const char *              method,
                                             struct blob_attr *        msg);
    static int  Ubus_setnetworkname_handler(struct ubus_context *     aContext,
                                            struct ubus_object *      obj,
                                            struct ubus_request_data *requ,
                                            const char *              method,
                                            struct blob_attr *        msg);
    static int  Ubus_setpanid_handler(struct ubus_context *     aContext,
                                      struct ubus_object *      obj,
                                      struct ubus_request_data *requ,
                                      const char *              method,
                                      struct blob_attr *        msg);
    static int  Ubus_setchannel_handler(struct ubus_context *     aContext,
                                        struct ubus_object *      obj,
                                        struct ubus_request_data *requ,
                                        const char *              method,
                                        struct blob_attr *        msg);
    static int  Ubus_setpskc_handler(struct ubus_context *     aContext,
                                     struct ubus_object *      obj,
                                     struct ubus_request_data *requ,
                                     const char *              method,
                                     struct blob_attr *        msg);
    static int  Ubus_setmasterkey_handler(struct ubus_context *     aContext,
                                          struct ubus_object *      obj,
                                          struct ubus_request_data *requ,
                                          const char *              method,
                                          struct blob_attr *        msg);
    static int  Ubus_parent_handler(struct ubus_context *     aContext,
                                    struct ubus_object *      obj,
                                    struct ubus_request_data *requ,
                                    const char *              method,
                                    struct blob_attr *        msg);
    static int  Ubus_neighbor_handler(struct ubus_context *     aContext,
                                      struct ubus_object *      obj,
                                      struct ubus_request_data *requ,
                                      const char *              method,
                                      struct blob_attr *        msg);
    static int  Ubus_leave_handler(struct ubus_context *     aContext,
                                   struct ubus_object *      obj,
                                   struct ubus_request_data *requ,
                                   const char *              method,
                                   struct blob_attr *        msg);
    static int  Ubus_rloc16_handler(struct ubus_context *     aContext,
                                    struct ubus_object *      obj,
                                    struct ubus_request_data *requ,
                                    const char *              method,
                                    struct blob_attr *        msg);
    static int  Ubus_extpanid_handler(struct ubus_context *     aContext,
                                      struct ubus_object *      obj,
                                      struct ubus_request_data *requ,
                                      const char *              method,
                                      struct blob_attr *        msg);
    static int  Ubus_mode_handler(struct ubus_context *     aContext,
                                  struct ubus_object *      obj,
                                  struct ubus_request_data *requ,
                                  const char *              method,
                                  struct blob_attr *        msg);
    static int  Ubus_leaderpartitionid_handler(struct ubus_context *     aContext,
                                               struct ubus_object *      obj,
                                               struct ubus_request_data *requ,
                                               const char *              method,
                                               struct blob_attr *        msg);
    static int  Ubus_setmode_handler(struct ubus_context *     aContext,
                                     struct ubus_object *      obj,
                                     struct ubus_request_data *requ,
                                     const char *              method,
                                     struct blob_attr *        msg);
    static int  Ubus_setextpanid_handler(struct ubus_context *     aContext,
                                         struct ubus_object *      obj,
                                         struct ubus_request_data *requ,
                                         const char *              method,
                                         struct blob_attr *        msg);
    static int  Ubus_setleaderpartitionid_handler(struct ubus_context *     aContext,
                                                  struct ubus_object *      obj,
                                                  struct ubus_request_data *requ,
                                                  const char *              method,
                                                  struct blob_attr *        msg);
    static int  Ubus_leaderdata_handler(struct ubus_context *     aContext,
                                        struct ubus_object *      obj,
                                        struct ubus_request_data *requ,
                                        const char *              method,
                                        struct blob_attr *        msg);
    static int  Ubus_networkdata_handler(struct ubus_context *     aContext,
                                         struct ubus_object *      obj,
                                         struct ubus_request_data *requ,
                                         const char *              method,
                                         struct blob_attr *        msg);
    static int  Ubus_joinernum_handler(struct ubus_context *     aContext,
                                       struct ubus_object *      obj,
                                       struct ubus_request_data *requ,
                                       const char *              method,
                                       struct blob_attr *        msg);
    static int  Ubus_commissionerstart_handler(struct ubus_context *     aContext,
                                               struct ubus_object *      obj,
                                               struct ubus_request_data *requ,
                                               const char *              method,
                                               struct blob_attr *        msg);
    static int  Ubus_joineradd_handler(struct ubus_context *     aContext,
                                       struct ubus_object *      obj,
                                       struct ubus_request_data *aRequest,
                                       const char *              method,
                                       struct blob_attr *        msg);
    static int  Ubus_mgmtset_handler(struct ubus_context *     aContext,
                                     struct ubus_object *      obj,
                                     struct ubus_request_data *aRequest,
                                     const char *              method,
                                     struct blob_attr *        msg);
    static int  Ubus_joinerremove_handler(struct ubus_context *     aContext,
                                          struct ubus_object *      obj,
                                          struct ubus_request_data *aRequest,
                                          const char *              method,
                                          struct blob_attr *        msg);
    static int  Ubus_macfilter_add_handler(struct ubus_context *     aContext,
                                           struct ubus_object *      obj,
                                           struct ubus_request_data *requ,
                                           const char *              method,
                                           struct blob_attr *        msg);
    static int  Ubus_macfilter_clear_handler(struct ubus_context *     aContext,
                                             struct ubus_object *      obj,
                                             struct ubus_request_data *requ,
                                             const char *              method,
                                             struct blob_attr *        msg);
    static int  Ubus_macfilter_remove_handler(struct ubus_context *     aContext,
                                              struct ubus_object *      obj,
                                              struct ubus_request_data *requ,
                                              const char *              method,
                                              struct blob_attr *        msg);
    static int  Ubus_macfilter_setstate_handler(struct ubus_context *     aContext,
                                                struct ubus_object *      obj,
                                                struct ubus_request_data *requ,
                                                const char *              method,
                                                struct blob_attr *        msg);
    static void HandleDiagnosticGetResponse(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext);
    void        HandleDiagnosticGetResponse(ot::Message &aMessage, const ot::Ip6::MessageInfo &aMessageInfo);

private:
    bool                       mIfFinishScan;
    struct ubus_context *      mContext;
    struct blob_buf            mBuf;
    struct blob_buf            mNetworkdataBuf;
    int                        mJoinerNum;
    const char *               mSockPath;
    static uint8_t             mMaxRouterId;
    Ncp::ControllerOpenThread *mController;
    otInstance *               mInstance;
    time_t                     mSecond;
    bool                       mIsCommissioner;
    otExtAddress               mJoiner[10];

    static void HandleActiveScanResult(otActiveScanResult *aResult, void *aContext);
    void        HandleActiveScanResultDetail(otActiveScanResult *aResult);
    void        OutputBytes(const uint8_t *aBytes, uint8_t aLength, char *output);
    void        OutputBytes(const uint8_t *aBytes, uint8_t aLength);
    void        ProcessScan();
    void        AppendResult(otError aError, struct ubus_context *aContext, struct ubus_request_data *aRequest);
    int         Ubus_scan_handler_Detail(struct ubus_context *     aContext,
                                         struct ubus_object *      obj,
                                         struct ubus_request_data *req,
                                         const char *              method,
                                         struct blob_attr *        msg);
    int         Ubus_neighbor_handler_Detail(struct ubus_context *     aContext,
                                             struct ubus_object *      obj,
                                             struct ubus_request_data *req,
                                             const char *              method,
                                             struct blob_attr *        msg);
    int         Ubus_parent_handler_Detail(struct ubus_context *     aContext,
                                           struct ubus_object *      obj,
                                           struct ubus_request_data *req,
                                           const char *              method,
                                           struct blob_attr *        msg);
    int         Ubus_mgmtset(struct ubus_context *     aContext,
                             struct ubus_object *      obj,
                             struct ubus_request_data *req,
                             const char *              method,
                             struct blob_attr *        msg);
    int         Ubus_leave_handler_Detail(struct ubus_context *     aContext,
                                          struct ubus_object *      obj,
                                          struct ubus_request_data *req,
                                          const char *              method,
                                          struct blob_attr *        msg);
    int         Ubus_thread_handler(struct ubus_context *aContext,
                                    struct ubus_object *,
                                    struct ubus_request_data *requ,
                                    const char *,
                                    struct blob_attr *,
                                    const char *action);
    int         Ubus_get_information(struct ubus_context *     aContext,
                                     struct ubus_object *      obj,
                                     struct ubus_request_data *req,
                                     const char *              method,
                                     struct blob_attr *        msg,
                                     const char *              action);
    int         Ubus_set_information(struct ubus_context *aContext,
                                     struct ubus_object *,
                                     struct ubus_request_data *requ,
                                     const char *,
                                     struct blob_attr *msg,
                                     const char *      action);
    int         Ubus_commissioner(struct ubus_context *aContext,
                                  struct ubus_object *,
                                  struct ubus_request_data *aRequest,
                                  const char *,
                                  struct blob_attr *,
                                  const char *action);
    static void HandleStateChanged(otCommissionerState aState, void *aContext);
    void        HandleStateChanged(otCommissionerState aState);
    static void HandleJoinerEvent(otCommissionerJoinerEvent aEvent, const otExtAddress *aJoinerId, void *aContext);
    void        HandleJoinerEvent(otCommissionerJoinerEvent aEvent, const otExtAddress *aJoinerId);
    void        getState(otInstance *mInstance, char *state);
    void        Ubus_add_fd();
    static void Ubus_reconn_timer(struct uloop_timeout *timeout);
    void        Ubus_reconn_timer_Detail(struct uloop_timeout *timeout);
    static void Ubus_connection_lost(struct ubus_context *aContext);
    int         Display_ubus_init(const char *path);
    void        Display_ubus_done();
    otError     ParseLong(char *aString, long &aLong);
    int         Hex2Bin(const char *aHex, uint8_t *aBin, uint16_t aBinLength);

private:
    enum
    {
        kDefaultJoinerTimeout = 120,
    };

    OtUbusServer(Ncp::ControllerOpenThread *aController);
};
} // namespace ubus
} // namespace BorderRouter
} // namespace ot

#endif // OTUBUS_HPP_
