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

#if __ORDER_BIG_ENDIAN__
#define BYTE_ORDER_BIG_ENDIAN 1
#endif

#include "otubus.hpp"

#include <mutex>

#include <sys/eventfd.h>
#include <openthread/commissioner.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>

#include "thread/network_diagnostic_tlvs.hpp"
#undef PACKAGE
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION
#undef VERSION

#include "ncp_openthread.hpp"

namespace ot {
namespace BorderRouter {
namespace ubus {
static OtUbusServer *otUbusServerInstance = NULL;
static int           ubus_efd             = -1;
const static int     PANID_LENGTH         = 10;
const static int     XPANID_LENGTH        = 64;
const static int     MASTERKEY_LENGTH     = 64;
void *               json_uri             = NULL;

int         uloop_available;
int         bufNum;
std::mutex *ncpThreadMutex;

OtUbusServer::OtUbusServer(Ncp::ControllerOpenThread *aController)
{
    mController = aController;
    mJoinerNum  = 0;
    mSecond     = 0;
    blob_buf_init(&mNetworkdataBuf, 0);
    blob_buf_init(&mBuf, 0);
}

OtUbusServer &OtUbusServer::getInstance()
{
    return *otUbusServerInstance;
}

void OtUbusServer::initialize(Ncp::ControllerOpenThread *aController)
{
    otUbusServerInstance = new OtUbusServer(aController);
    otThreadSetReceiveDiagnosticGetCallback(aController->getInstance(), &OtUbusServer::HandleDiagnosticGetResponse,
                                            otUbusServerInstance);
}

enum
{
    SETNETWORK,
    SET_NETWORK_MAX,
};

enum
{
    PSKD,
    EUI64,
    ADD_JOINER_MAX,
};

enum
{
    MASTERKEY,
    NETWORKNAME,
    EXTPANID,
    PANID,
    CHANNEL,
    PSKC,
    MGMTSET_MAX,
};

static const struct blobmsg_policy set_networkname_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "networkname", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy set_panid_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "panid", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy set_extpanid_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "extpanid", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy set_channel_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "channel", .type = BLOBMSG_TYPE_INT32},
};

static const struct blobmsg_policy set_pskc_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "pskc", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy set_masterkey_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "masterkey", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy set_mode_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "mode", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy set_leaderpartitionid_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "leaderpartitionid", .type = BLOBMSG_TYPE_INT32},
};

static const struct blobmsg_policy macfilter_add_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "addr", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy macfilter_remove_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "addr", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy macfilter_setstate_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "state", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy remove_joiner_policy[SET_NETWORK_MAX] = {
    [SETNETWORK] = {.name = "eui64", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy add_joiner_policy[ADD_JOINER_MAX] = {
    [PSKD]  = {.name = "pskd", .type = BLOBMSG_TYPE_STRING},
    [EUI64] = {.name = "eui64", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy mgmtset_policy[MGMTSET_MAX] = {
    [MASTERKEY]   = {.name = "masterkey", .type = BLOBMSG_TYPE_STRING},
    [NETWORKNAME] = {.name = "networkname", .type = BLOBMSG_TYPE_STRING},
    [EXTPANID]    = {.name = "extpanid", .type = BLOBMSG_TYPE_STRING},
    [PANID]       = {.name = "panid", .type = BLOBMSG_TYPE_STRING},
    [CHANNEL]     = {.name = "channel", .type = BLOBMSG_TYPE_STRING},
    [PSKC]        = {.name = "pskc", .type = BLOBMSG_TYPE_STRING},
};

static const struct ubus_method otbr_methods[] = {
    {"scan", &OtUbusServer::Ubus_scan_handler, 0, 0, NULL, 0},
    {"channel", &OtUbusServer::Ubus_channel_handler, 0, 0, NULL, 0},
    {"setchannel", &OtUbusServer::Ubus_setchannel_handler, 0, 0, set_channel_policy, ARRAY_SIZE(set_channel_policy)},
    {"networkname", &OtUbusServer::Ubus_networkname_handler, 0, 0, NULL, 0},
    {"setnetworkname", &OtUbusServer::Ubus_setnetworkname_handler, 0, 0, set_networkname_policy,
     ARRAY_SIZE(set_networkname_policy)},
    {"state", &OtUbusServer::Ubus_state_handler, 0, 0, NULL, 0},
    {"panid", &OtUbusServer::Ubus_panid_handler, 0, 0, NULL, 0},
    {"setpanid", &OtUbusServer::Ubus_setpanid_handler, 0, 0, set_panid_policy, ARRAY_SIZE(set_panid_policy)},
    {"rloc16", &OtUbusServer::Ubus_rloc16_handler, 0, 0, NULL, 0},
    {"extpanid", &OtUbusServer::Ubus_extpanid_handler, 0, 0, NULL, 0},
    {"setextpanid", &OtUbusServer::Ubus_setextpanid_handler, 0, 0, set_extpanid_policy,
     ARRAY_SIZE(set_extpanid_policy)},
    {"masterkey", &OtUbusServer::Ubus_masterkey_handler, 0, 0, NULL, 0},
    {"setmasterkey", &OtUbusServer::Ubus_setmasterkey_handler, 0, 0, set_masterkey_policy,
     ARRAY_SIZE(set_masterkey_policy)},
    {"pskc", &OtUbusServer::Ubus_pskc_handler, 0, 0, NULL, 0},
    {"setpskc", &OtUbusServer::Ubus_setpskc_handler, 0, 0, set_pskc_policy, ARRAY_SIZE(set_pskc_policy)},
    {"threadstart", &OtUbusServer::Ubus_threadstart_handler, 0, 0, NULL, 0},
    {"threadstop", &OtUbusServer::Ubus_threadstop_handler, 0, 0, NULL, 0},
    {"neighbor", &OtUbusServer::Ubus_neighbor_handler, 0, 0, NULL, 0},
    {"parent", &OtUbusServer::Ubus_parent_handler, 0, 0, NULL, 0},
    {"mode", &OtUbusServer::Ubus_mode_handler, 0, 0, NULL, 0},
    {"setmode", &OtUbusServer::Ubus_setmode_handler, 0, 0, set_mode_policy, ARRAY_SIZE(set_mode_policy)},
    {"leaderpartitionid", &OtUbusServer::Ubus_leaderpartitionid_handler, 0, 0, NULL, 0},
    {"setleaderpartitionid", &OtUbusServer::Ubus_setleaderpartitionid_handler, 0, 0, set_leaderpartitionid_policy,
     ARRAY_SIZE(set_leaderpartitionid_policy)},
    {"leave", &OtUbusServer::Ubus_leave_handler, 0, 0, NULL, 0},
    {"leaderdata", &OtUbusServer::Ubus_leaderdata_handler, 0, 0, NULL, 0},
    {"networkdata", &OtUbusServer::Ubus_networkdata_handler, 0, 0, NULL, 0},
    {"commissionerstart", &OtUbusServer::Ubus_commissionerstart_handler, 0, 0, NULL, 0},
    {"joinernum", &OtUbusServer::Ubus_joinernum_handler, 0, 0, NULL, 0},
    {"joinerremove", &OtUbusServer::Ubus_joinerremove_handler, 0, 0, NULL, 0},
    {"macfiltersetstate", &OtUbusServer::Ubus_macfilter_setstate_handler, 0, 0, macfilter_setstate_policy,
     ARRAY_SIZE(macfilter_setstate_policy)},
    {"macfilteradd", &OtUbusServer::Ubus_macfilter_add_handler, 0, 0, macfilter_add_policy,
     ARRAY_SIZE(macfilter_add_policy)},
    {"macfilterremove", &OtUbusServer::Ubus_macfilter_remove_handler, 0, 0, macfilter_remove_policy,
     ARRAY_SIZE(macfilter_remove_policy)},
    {"macfilterclear", &OtUbusServer::Ubus_macfilter_clear_handler, 0, 0, NULL, 0},
    {"macfilterstate", &OtUbusServer::Ubus_macfilter_state_handler, 0, 0, NULL, 0},
    {"macfilteraddr", &OtUbusServer::Ubus_macfilter_addr_handler, 0, 0, NULL, 0},
    {"joineradd", &OtUbusServer::Ubus_joineradd_handler, 0, 0, add_joiner_policy, ARRAY_SIZE(add_joiner_policy)},
    {"mgmtset", &OtUbusServer::Ubus_mgmtset_handler, 0, 0, mgmtset_policy, ARRAY_SIZE(mgmtset_policy)},
};

static struct ubus_object_type otbr_obj_type = {"otbr_prog", 0, otbr_methods, ARRAY_SIZE(otbr_methods)};

static struct ubus_object otbr = {
    avl : {},
    name : "otbr",
    id : 0,
    path : NULL,
    type : &otbr_obj_type,
    subscribe_cb : NULL,
    has_subscribers : false,
    methods : otbr_methods,
    n_methods : ARRAY_SIZE(otbr_methods),
};

void OtUbusServer::ProcessScan()
{
    otError  error        = OT_ERROR_NONE;
    uint32_t scanChannels = 0;
    uint16_t scanDuration = 0;

    ncpThreadMutex->lock();
    mInstance = mController->getInstance();
    SuccessOrExit(error = otLinkActiveScan(otUbusServerInstance->mInstance, scanChannels, scanDuration,
                                           &OtUbusServer::HandleActiveScanResult, this));
exit:
    ncpThreadMutex->unlock();
    return;
}

void OtUbusServer::HandleActiveScanResult(otActiveScanResult *aResult, void *aContext)
{
    static_cast<OtUbusServer *>(aContext)->HandleActiveScanResultDetail(aResult);
}

void OtUbusServer::OutputBytes(const uint8_t *aBytes, uint8_t aLength, char *output)
{
    char ubusByte2char[5] = "";
    for (int i = 0; i < aLength; i++)
    {
        sprintf(ubusByte2char, "%02x", aBytes[i]);
        strcat(output, ubusByte2char);
    }
}

void OtUbusServer::AppendResult(otError aError, struct ubus_context *aContext, struct ubus_request_data *aRequest)
{
    blobmsg_add_u16(&mBuf, "Error", aError);
    ubus_send_reply(aContext, aRequest, mBuf.head);
}

void OtUbusServer::HandleActiveScanResultDetail(otActiveScanResult *aResult)
{
    void *json_list = NULL;

    char panidstring[PANID_LENGTH];
    char xpanidstring[XPANID_LENGTH] = "";

    if (aResult == NULL)
    {
        blobmsg_close_array(&mBuf, json_uri);
        mIfFinishScan = true;
        goto exit;
    }

    json_list = blobmsg_open_table(&mBuf, NULL);

    blobmsg_add_u32(&mBuf, "IsJoinable", aResult->mIsJoinable);

    blobmsg_add_string(&mBuf, "NetworkName", aResult->mNetworkName.m8);

    OutputBytes(aResult->mExtendedPanId.m8, OT_EXT_PAN_ID_SIZE, xpanidstring);
    blobmsg_add_string(&mBuf, "ExtendedPanId", xpanidstring);

    sprintf(panidstring, "0x%04x", aResult->mPanId);
    blobmsg_add_string(&mBuf, "PanId", panidstring);

    blobmsg_add_u32(&mBuf, "Channel", aResult->mChannel);

    blobmsg_add_u32(&mBuf, "Rssi", aResult->mRssi);

    blobmsg_add_u32(&mBuf, "Lqi", aResult->mLqi);

    blobmsg_close_table(&mBuf, json_list);

exit:
    return;
}

int OtUbusServer::Ubus_scan_handler(struct ubus_context *     aContext,
                                    struct ubus_object *      obj,
                                    struct ubus_request_data *aRequest,
                                    const char *              method,
                                    struct blob_attr *        msg)
{
    return getInstance().Ubus_scan_handler_Detail(aContext, obj, aRequest, method, msg);
}

int OtUbusServer::Ubus_scan_handler_Detail(struct ubus_context *aContext,
                                           struct ubus_object *,
                                           struct ubus_request_data *aRequest,
                                           const char *,
                                           struct blob_attr *)
{
    otError  error = OT_ERROR_NONE;
    uint64_t u;
    ssize_t  s;

    blob_buf_init(&mBuf, 0);
    json_uri = blobmsg_open_array(&mBuf, "scan_list");

    mIfFinishScan = 0;
    otUbusServerInstance->ProcessScan();

    u = 1;
    s = write(ubus_efd, &u, sizeof(uint64_t));
    if (s != sizeof(uint64_t))
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }

    while (!mIfFinishScan)
    {
        sleep(1);
    }

exit:
    AppendResult(error, aContext, aRequest);
    return 0;
}

int OtUbusServer::Ubus_channel_handler(struct ubus_context *     aContext,
                                       struct ubus_object *      obj,
                                       struct ubus_request_data *aRequest,
                                       const char *              method,
                                       struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "channel");
}

int OtUbusServer::Ubus_setchannel_handler(struct ubus_context *     aContext,
                                          struct ubus_object *      obj,
                                          struct ubus_request_data *aRequest,
                                          const char *              method,
                                          struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "channel");
}

int OtUbusServer::Ubus_joinernum_handler(struct ubus_context *     aContext,
                                         struct ubus_object *      obj,
                                         struct ubus_request_data *aRequest,
                                         const char *              method,
                                         struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "joinernum");
}

int OtUbusServer::Ubus_networkname_handler(struct ubus_context *     aContext,
                                           struct ubus_object *      obj,
                                           struct ubus_request_data *aRequest,
                                           const char *              method,
                                           struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "networkname");
}

int OtUbusServer::Ubus_setnetworkname_handler(struct ubus_context *     aContext,
                                              struct ubus_object *      obj,
                                              struct ubus_request_data *aRequest,
                                              const char *              method,
                                              struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "networkname");
}

int OtUbusServer::Ubus_state_handler(struct ubus_context *     aContext,
                                     struct ubus_object *      obj,
                                     struct ubus_request_data *aRequest,
                                     const char *              method,
                                     struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "state");
}

int OtUbusServer::Ubus_rloc16_handler(struct ubus_context *     aContext,
                                      struct ubus_object *      obj,
                                      struct ubus_request_data *aRequest,
                                      const char *              method,
                                      struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "rloc16");
}

int OtUbusServer::Ubus_panid_handler(struct ubus_context *     aContext,
                                     struct ubus_object *      obj,
                                     struct ubus_request_data *aRequest,
                                     const char *              method,
                                     struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "panid");
}

int OtUbusServer::Ubus_setpanid_handler(struct ubus_context *     aContext,
                                        struct ubus_object *      obj,
                                        struct ubus_request_data *aRequest,
                                        const char *              method,
                                        struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "panid");
}

int OtUbusServer::Ubus_extpanid_handler(struct ubus_context *     aContext,
                                        struct ubus_object *      obj,
                                        struct ubus_request_data *aRequest,
                                        const char *              method,
                                        struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "extpanid");
}

int OtUbusServer::Ubus_setextpanid_handler(struct ubus_context *     aContext,
                                           struct ubus_object *      obj,
                                           struct ubus_request_data *aRequest,
                                           const char *              method,
                                           struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "extpanid");
}

int OtUbusServer::Ubus_pskc_handler(struct ubus_context *     aContext,
                                    struct ubus_object *      obj,
                                    struct ubus_request_data *aRequest,
                                    const char *              method,
                                    struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "pskc");
}

int OtUbusServer::Ubus_setpskc_handler(struct ubus_context *     aContext,
                                       struct ubus_object *      obj,
                                       struct ubus_request_data *aRequest,
                                       const char *              method,
                                       struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "pskc");
}

int OtUbusServer::Ubus_masterkey_handler(struct ubus_context *     aContext,
                                         struct ubus_object *      obj,
                                         struct ubus_request_data *aRequest,
                                         const char *              method,
                                         struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "masterkey");
}

int OtUbusServer::Ubus_setmasterkey_handler(struct ubus_context *     aContext,
                                            struct ubus_object *      obj,
                                            struct ubus_request_data *aRequest,
                                            const char *              method,
                                            struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "masterkey");
}

int OtUbusServer::Ubus_threadstart_handler(struct ubus_context *     aContext,
                                           struct ubus_object *      obj,
                                           struct ubus_request_data *aRequest,
                                           const char *              method,
                                           struct blob_attr *        msg)
{
    return getInstance().Ubus_thread_handler(aContext, obj, aRequest, method, msg, "start");
}

int OtUbusServer::Ubus_threadstop_handler(struct ubus_context *     aContext,
                                          struct ubus_object *      obj,
                                          struct ubus_request_data *aRequest,
                                          const char *              method,
                                          struct blob_attr *        msg)
{
    return getInstance().Ubus_thread_handler(aContext, obj, aRequest, method, msg, "stop");
}

int OtUbusServer::Ubus_parent_handler(struct ubus_context *     aContext,
                                      struct ubus_object *      obj,
                                      struct ubus_request_data *aRequest,
                                      const char *              method,
                                      struct blob_attr *        msg)
{
    return getInstance().Ubus_parent_handler_Detail(aContext, obj, aRequest, method, msg);
}

int OtUbusServer::Ubus_neighbor_handler(struct ubus_context *     aContext,
                                        struct ubus_object *      obj,
                                        struct ubus_request_data *aRequest,
                                        const char *              method,
                                        struct blob_attr *        msg)
{
    return getInstance().Ubus_neighbor_handler_Detail(aContext, obj, aRequest, method, msg);
}

int OtUbusServer::Ubus_mode_handler(struct ubus_context *     aContext,
                                    struct ubus_object *      obj,
                                    struct ubus_request_data *aRequest,
                                    const char *              method,
                                    struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "mode");
}

int OtUbusServer::Ubus_setmode_handler(struct ubus_context *     aContext,
                                       struct ubus_object *      obj,
                                       struct ubus_request_data *aRequest,
                                       const char *              method,
                                       struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "mode");
}

int OtUbusServer::Ubus_leaderpartitionid_handler(struct ubus_context *     aContext,
                                                 struct ubus_object *      obj,
                                                 struct ubus_request_data *aRequest,
                                                 const char *              method,
                                                 struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "leaderpartitionid");
}

int OtUbusServer::Ubus_setleaderpartitionid_handler(struct ubus_context *     aContext,
                                                    struct ubus_object *      obj,
                                                    struct ubus_request_data *aRequest,
                                                    const char *              method,
                                                    struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "leaderpartitionid");
}

int OtUbusServer::Ubus_leave_handler(struct ubus_context *     aContext,
                                     struct ubus_object *      obj,
                                     struct ubus_request_data *aRequest,
                                     const char *              method,
                                     struct blob_attr *        msg)
{
    return getInstance().Ubus_leave_handler_Detail(aContext, obj, aRequest, method, msg);
}

int OtUbusServer::Ubus_leaderdata_handler(struct ubus_context *     aContext,
                                          struct ubus_object *      obj,
                                          struct ubus_request_data *aRequest,
                                          const char *              method,
                                          struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "leaderdata");
}

int OtUbusServer::Ubus_networkdata_handler(struct ubus_context *     aContext,
                                           struct ubus_object *      obj,
                                           struct ubus_request_data *aRequest,
                                           const char *              method,
                                           struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "networkdata");
}

int OtUbusServer::Ubus_commissionerstart_handler(struct ubus_context *     aContext,
                                                 struct ubus_object *      obj,
                                                 struct ubus_request_data *aRequest,
                                                 const char *              method,
                                                 struct blob_attr *        msg)
{
    return getInstance().Ubus_commissioner(aContext, obj, aRequest, method, msg, "start");
}

int OtUbusServer::Ubus_joinerremove_handler(struct ubus_context *     aContext,
                                            struct ubus_object *      obj,
                                            struct ubus_request_data *aRequest,
                                            const char *              method,
                                            struct blob_attr *        msg)
{
    return getInstance().Ubus_commissioner(aContext, obj, aRequest, method, msg, "joinerremove");
}

int OtUbusServer::Ubus_mgmtset_handler(struct ubus_context *     aContext,
                                       struct ubus_object *      obj,
                                       struct ubus_request_data *aRequest,
                                       const char *              method,
                                       struct blob_attr *        msg)
{
    return getInstance().Ubus_mgmtset(aContext, obj, aRequest, method, msg);
}

int OtUbusServer::Ubus_joineradd_handler(struct ubus_context *     aContext,
                                         struct ubus_object *      obj,
                                         struct ubus_request_data *aRequest,
                                         const char *              method,
                                         struct blob_attr *        msg)
{
    return getInstance().Ubus_commissioner(aContext, obj, aRequest, method, msg, "joineradd");
}

int OtUbusServer::Ubus_macfilter_addr_handler(struct ubus_context *     aContext,
                                              struct ubus_object *      obj,
                                              struct ubus_request_data *aRequest,
                                              const char *              method,
                                              struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "macfilteraddr");
}

int OtUbusServer::Ubus_macfilter_state_handler(struct ubus_context *     aContext,
                                               struct ubus_object *      obj,
                                               struct ubus_request_data *aRequest,
                                               const char *              method,
                                               struct blob_attr *        msg)
{
    return getInstance().Ubus_get_information(aContext, obj, aRequest, method, msg, "macfilterstate");
}

int OtUbusServer::Ubus_macfilter_add_handler(struct ubus_context *     aContext,
                                             struct ubus_object *      obj,
                                             struct ubus_request_data *aRequest,
                                             const char *              method,
                                             struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "macfilteradd");
}

int OtUbusServer::Ubus_macfilter_remove_handler(struct ubus_context *     aContext,
                                                struct ubus_object *      obj,
                                                struct ubus_request_data *aRequest,
                                                const char *              method,
                                                struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "macfilterremove");
}

int OtUbusServer::Ubus_macfilter_setstate_handler(struct ubus_context *     aContext,
                                                  struct ubus_object *      obj,
                                                  struct ubus_request_data *aRequest,
                                                  const char *              method,
                                                  struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "macfiltersetstate");
}

int OtUbusServer::Ubus_macfilter_clear_handler(struct ubus_context *     aContext,
                                               struct ubus_object *      obj,
                                               struct ubus_request_data *aRequest,
                                               const char *              method,
                                               struct blob_attr *        msg)
{
    return getInstance().Ubus_set_information(aContext, obj, aRequest, method, msg, "macfilterclear");
}

int OtUbusServer::Ubus_leave_handler_Detail(struct ubus_context *aContext,
                                            struct ubus_object *,
                                            struct ubus_request_data *aRequest,
                                            const char *,
                                            struct blob_attr *)
{
    otError  error = OT_ERROR_NONE;
    uint64_t u;
    ssize_t  s;

    ncpThreadMutex->lock();
    otInstanceFactoryReset(mController->getInstance());

    u = 1;
    s = write(ubus_efd, &u, sizeof(uint64_t));
    if (s != sizeof(uint64_t))
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }

    blob_buf_init(&mBuf, 0);

exit:
    ncpThreadMutex->unlock();
    AppendResult(error, aContext, aRequest);
    return 0;
}
int OtUbusServer::Ubus_thread_handler(struct ubus_context *aContext,
                                      struct ubus_object *,
                                      struct ubus_request_data *aRequest,
                                      const char *,
                                      struct blob_attr *,
                                      const char *action)
{
    otError error = OT_ERROR_NONE;

    blob_buf_init(&mBuf, 0);

    if (!strcmp(action, "start"))
    {
        ncpThreadMutex->lock();
        mInstance = mController->getInstance();
        SuccessOrExit(error = otIp6SetEnabled(otUbusServerInstance->mInstance, true));
        SuccessOrExit(error = otThreadSetEnabled(otUbusServerInstance->mInstance, true));
    }
    else if (!strcmp(action, "stop"))
    {
        ncpThreadMutex->lock();
        mInstance = mController->getInstance();
        SuccessOrExit(error = otThreadSetEnabled(otUbusServerInstance->mInstance, false));
        SuccessOrExit(error = otIp6SetEnabled(otUbusServerInstance->mInstance, false));
    }

exit:
    ncpThreadMutex->unlock();
    AppendResult(error, aContext, aRequest);
    return 0;
}

int OtUbusServer::Ubus_parent_handler_Detail(struct ubus_context *aContext,
                                             struct ubus_object *,
                                             struct ubus_request_data *aRequest,
                                             const char *,
                                             struct blob_attr *)
{
    otError      error = OT_ERROR_NONE;
    otRouterInfo parentInfo;
    char         ubusExtAddress[XPANID_LENGTH] = "";
    char         transfer[XPANID_LENGTH]       = "";
    void *       json_list                     = NULL;
    void *       json_array                    = NULL;

    blob_buf_init(&mBuf, 0);

    ncpThreadMutex->lock();
    mInstance = mController->getInstance();
    SuccessOrExit(error = otThreadGetParentInfo(otUbusServerInstance->mInstance, &parentInfo));

    json_array = blobmsg_open_array(&mBuf, "parent_list");
    json_list  = blobmsg_open_table(&mBuf, "parent");
    blobmsg_add_string(&mBuf, "Role", "R");

    sprintf(transfer, "0x%04x", parentInfo.mRloc16);
    blobmsg_add_string(&mBuf, "Rloc16", transfer);

    sprintf(transfer, "%3d", parentInfo.mAge);
    blobmsg_add_string(&mBuf, "Age", transfer);

    OutputBytes(parentInfo.mExtAddress.m8, sizeof(parentInfo.mExtAddress.m8), ubusExtAddress);
    blobmsg_add_string(&mBuf, "ExtAddress", ubusExtAddress);

    blobmsg_add_u16(&mBuf, "LinkQualityIn", parentInfo.mLinkQualityIn);

    blobmsg_close_table(&mBuf, json_list);
    blobmsg_close_array(&mBuf, json_array);

exit:
    ncpThreadMutex->unlock();
    AppendResult(error, aContext, aRequest);
    return error;
}

int OtUbusServer::Ubus_neighbor_handler_Detail(struct ubus_context *aContext,
                                               struct ubus_object *,
                                               struct ubus_request_data *aRequest,
                                               const char *,
                                               struct blob_attr *)
{
    otError                error = OT_ERROR_NONE;
    otNeighborInfo         ubusNeighborInfo;
    otNeighborInfoIterator iterator                      = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    char                   transfer[XPANID_LENGTH]       = "";
    void *                 json_list                     = NULL;
    char                   mode[5]                       = "";
    char                   ubusExtAddress[XPANID_LENGTH] = "";

    blob_buf_init(&mBuf, 0);

    json_uri = blobmsg_open_array(&mBuf, "neighbor_list");

    ncpThreadMutex->lock();
    mInstance = mController->getInstance();
    while (otThreadGetNextNeighborInfo(otUbusServerInstance->mInstance, &iterator, &ubusNeighborInfo) == OT_ERROR_NONE)
    {
        json_list = blobmsg_open_table(&mBuf, NULL);

        blobmsg_add_string(&mBuf, "Role", ubusNeighborInfo.mIsChild ? "C" : "R");

        sprintf(transfer, "0x%04x", ubusNeighborInfo.mRloc16);
        blobmsg_add_string(&mBuf, "Rloc16", transfer);

        sprintf(transfer, "%3d", ubusNeighborInfo.mAge);
        blobmsg_add_string(&mBuf, "Age", transfer);

        sprintf(transfer, "%8d", ubusNeighborInfo.mAverageRssi);
        blobmsg_add_string(&mBuf, "AvgRssi", transfer);

        sprintf(transfer, "%9d", ubusNeighborInfo.mLastRssi);
        blobmsg_add_string(&mBuf, "LastRssi", transfer);

        if (ubusNeighborInfo.mRxOnWhenIdle)
        {
            strcat(mode, "r");
        }

        if (ubusNeighborInfo.mSecureDataRequest)
        {
            strcat(mode, "s");
        }

        if (ubusNeighborInfo.mFullThreadDevice)
        {
            strcat(mode, "d");
        }

        if (ubusNeighborInfo.mFullNetworkData)
        {
            strcat(mode, "n");
        }
        blobmsg_add_string(&mBuf, "Mode", mode);

        OutputBytes(ubusNeighborInfo.mExtAddress.m8, sizeof(ubusNeighborInfo.mExtAddress.m8), ubusExtAddress);
        blobmsg_add_string(&mBuf, "ExtAddress", ubusExtAddress);

        blobmsg_add_u16(&mBuf, "LinkQualityIn", ubusNeighborInfo.mLinkQualityIn);

        blobmsg_close_table(&mBuf, json_list);

        memset(mode, 0, sizeof(mode));
        memset(ubusExtAddress, 0, sizeof(ubusExtAddress));
    }

    blobmsg_close_array(&mBuf, json_uri);

    ncpThreadMutex->unlock();

    AppendResult(error, aContext, aRequest);
    return 0;
}

int OtUbusServer::Ubus_mgmtset(struct ubus_context *aContext,
                               struct ubus_object *,
                               struct ubus_request_data *aRequest,
                               const char *,
                               struct blob_attr *msg)
{
    otError              error = OT_ERROR_NONE;
    struct blob_attr *   tb[MGMTSET_MAX];
    otOperationalDataset dataset;
    uint8_t              tlvs[128];
    long                 value;
    int                  length = 0;

    SuccessOrExit(error = otDatasetGetPending(otUbusServerInstance->mInstance, &dataset));

    blobmsg_parse(mgmtset_policy, MGMTSET_MAX, tb, blob_data(msg), blob_len(msg));
    if (tb[MASTERKEY] != NULL)
    {
        dataset.mComponents.mIsMasterKeyPresent = true;
        VerifyOrExit((length = Hex2Bin(blobmsg_get_string(tb[MASTERKEY]), dataset.mMasterKey.m8,
                                       sizeof(dataset.mMasterKey.m8))) == OT_MASTER_KEY_SIZE,
                     error = OT_ERROR_PARSE);
        length = 0;
    }
    if (tb[NETWORKNAME] != NULL)
    {
        dataset.mComponents.mIsNetworkNamePresent = true;
        VerifyOrExit((length = static_cast<int>(strlen(blobmsg_get_string(tb[NETWORKNAME])))) <=
                         OT_NETWORK_NAME_MAX_SIZE,
                     error = OT_ERROR_PARSE);
        memset(&dataset.mNetworkName, 0, sizeof(dataset.mNetworkName));
        memcpy(dataset.mNetworkName.m8, blobmsg_get_string(tb[NETWORKNAME]), static_cast<size_t>(length));
        length = 0;
    }
    if (tb[EXTPANID] != NULL)
    {
        dataset.mComponents.mIsExtendedPanIdPresent = true;
        VerifyOrExit(Hex2Bin(blobmsg_get_string(tb[EXTPANID]), dataset.mExtendedPanId.m8,
                             sizeof(dataset.mExtendedPanId.m8)) >= 0,
                     error = OT_ERROR_PARSE);
    }
    if (tb[PANID] != NULL)
    {
        dataset.mComponents.mIsPanIdPresent = true;
        SuccessOrExit(error = ParseLong(blobmsg_get_string(tb[PANID]), value));
        dataset.mPanId = static_cast<otPanId>(value);
    }
    if (tb[CHANNEL] != NULL)
    {
        dataset.mComponents.mIsChannelPresent = true;
        SuccessOrExit(error = ParseLong(blobmsg_get_string(tb[CHANNEL]), value));
        dataset.mChannel = static_cast<uint16_t>(value);
    }
    if (tb[PSKC] != NULL)
    {
        dataset.mComponents.mIsPskcPresent = true;
        VerifyOrExit((length = Hex2Bin(blobmsg_get_string(tb[PSKC]), dataset.mPskc.m8, sizeof(dataset.mPskc.m8))) ==
                         OT_PSKC_MAX_SIZE,
                     error = OT_ERROR_PARSE);
        length = 0;
    }
    dataset.mComponents.mIsPendingTimestampPresent = true;
    dataset.mComponents.mIsActiveTimestampPresent  = false;
    dataset.mPendingTimestamp++;
    SuccessOrExit(error = otDatasetSendMgmtPendingSet(otUbusServerInstance->mInstance, &dataset, tlvs,
                                                      static_cast<uint8_t>(length)));
exit:
    AppendResult(error, aContext, aRequest);
    return 0;
}

int OtUbusServer::Ubus_commissioner(struct ubus_context *aContext,
                                    struct ubus_object *,
                                    struct ubus_request_data *aRequest,
                                    const char *,
                                    struct blob_attr *msg,
                                    const char *      action)
{
    otError error = OT_ERROR_NONE;

    ncpThreadMutex->lock();
    mInstance = mController->getInstance();

    if (!strcmp(action, "start"))
    {
        if (otCommissionerGetState(otUbusServerInstance->mInstance) == OT_COMMISSIONER_STATE_DISABLED)
        {
            error      = otCommissionerStart(otUbusServerInstance->mInstance, &OtUbusServer::HandleStateChanged,
                                        &OtUbusServer::HandleJoinerEvent, this);
            mJoinerNum = 0;
        }
    }
    else if (!strcmp(action, "joineradd"))
    {
        struct blob_attr *  tb[ADD_JOINER_MAX];
        otExtAddress        addr;
        const otExtAddress *addrPtr = NULL;
        char *              pskd    = NULL;

        blobmsg_parse(add_joiner_policy, ADD_JOINER_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[PSKD] != NULL)
        {
            pskd = blobmsg_get_string(tb[PSKD]);
        }
        if (tb[EUI64] != NULL)
        {
            if (!strcmp(blobmsg_get_string(tb[EUI64]), "*"))
            {
                addrPtr = NULL;
                memset(&addr, 0, sizeof(addr));
            }
            else
            {
                VerifyOrExit(Hex2Bin(blobmsg_get_string(tb[EUI64]), addr.m8, sizeof(addr)) == sizeof(addr),
                             error = OT_ERROR_PARSE);
                addrPtr = &addr;
            }
        }

        unsigned long timeout = kDefaultJoinerTimeout;
        SuccessOrExit(error = otCommissionerAddJoiner(otUbusServerInstance->mInstance, addrPtr, pskd,
                                                      static_cast<uint32_t>(timeout)));
        mJoiner[mJoinerNum] = addr;
        mJoinerNum++;
    }
    else if (!strcmp(action, "joinerremove"))
    {
        struct blob_attr *  tb[SET_NETWORK_MAX];
        otExtAddress        addr;
        const otExtAddress *addrPtr = NULL;

        blobmsg_parse(remove_joiner_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            if (strcmp(blobmsg_get_string(tb[SETNETWORK]), "*") == 0)
            {
                addrPtr = NULL;
            }
            else
            {
                VerifyOrExit(Hex2Bin(blobmsg_get_string(tb[SETNETWORK]), addr.m8, sizeof(addr)) == sizeof(addr),
                             error = OT_ERROR_PARSE);
                addrPtr = &addr;
            }
        }

        SuccessOrExit(error = otCommissionerRemoveJoiner(otUbusServerInstance->mInstance, addrPtr));
        mJoinerNum--;
    }

exit:
    ncpThreadMutex->unlock();
    blob_buf_init(&mBuf, 0);
    AppendResult(error, aContext, aRequest);
    return 0;
}

void OtUbusServer::HandleStateChanged(otCommissionerState aState, void *aContext)
{
    static_cast<OtUbusServer *>(aContext)->HandleStateChanged(aState);
}

void OtUbusServer::HandleStateChanged(otCommissionerState aState)
{
    switch (aState)
    {
    case OT_COMMISSIONER_STATE_DISABLED:
        break;
    case OT_COMMISSIONER_STATE_ACTIVE:
        break;
    case OT_COMMISSIONER_STATE_PETITION:
        break;
    }
}

void OtUbusServer::HandleJoinerEvent(otCommissionerJoinerEvent aEvent, const otExtAddress *aJoinerId, void *aContext)
{
    static_cast<OtUbusServer *>(aContext)->HandleJoinerEvent(aEvent, aJoinerId);
}

void OtUbusServer::HandleJoinerEvent(otCommissionerJoinerEvent aEvent, const otExtAddress *)
{
    if (aEvent == OT_COMMISSIONER_JOINER_END)
    {
        mJoinerNum--;
    }
}

int OtUbusServer::Ubus_get_information(struct ubus_context *aContext,
                                       struct ubus_object *,
                                       struct ubus_request_data *aRequest,
                                       const char *,
                                       struct blob_attr *,
                                       const char *action)
{
    otError error = OT_ERROR_NONE;

    blob_buf_init(&mBuf, 0);

    ncpThreadMutex->lock();
    mInstance = mController->getInstance();
    if (!strcmp(action, "networkname"))
        blobmsg_add_string(&mBuf, "NetworkName", otThreadGetNetworkName(otUbusServerInstance->mInstance));
    else if (!strcmp(action, "state"))
    {
        char state[10];
        getState(otUbusServerInstance->mInstance, state);
        blobmsg_add_string(&mBuf, "State", state);
    }
    else if (!strcmp(action, "channel"))
        blobmsg_add_u32(&mBuf, "Channel", otLinkGetChannel(otUbusServerInstance->mInstance));
    else if (!strcmp(action, "panid"))
    {
        char panidstring[PANID_LENGTH];
        sprintf(panidstring, "0x%04x", otLinkGetPanId(otUbusServerInstance->mInstance));
        blobmsg_add_string(&mBuf, "PanId", panidstring);
    }
    else if (!strcmp(action, "rloc16"))
    {
        char panidstring[PANID_LENGTH];
        sprintf(panidstring, "0x%04x", otThreadGetRloc16(otUbusServerInstance->mInstance));
        blobmsg_add_string(&mBuf, "rloc16", panidstring);
    }
    else if (!strcmp(action, "masterkey"))
    {
        char           output_key[MASTERKEY_LENGTH] = "";
        const uint8_t *key = reinterpret_cast<const uint8_t *>(otThreadGetMasterKey(otUbusServerInstance->mInstance));
        OutputBytes(key, OT_MASTER_KEY_SIZE, output_key);
        blobmsg_add_string(&mBuf, "Masterkey", output_key);
    }
    else if (!strcmp(action, "pskc"))
    {
        char          output_key[MASTERKEY_LENGTH] = "";
        const otPskc *pskc                         = otThreadGetPskc(otUbusServerInstance->mInstance);
        OutputBytes(pskc->m8, OT_MASTER_KEY_SIZE, output_key);
        blobmsg_add_string(&mBuf, "pskc", output_key);
    }
    else if (!strcmp(action, "extpanid"))
    {
        char           output_panid[XPANID_LENGTH] = "";
        const uint8_t *extPanId =
            reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(otUbusServerInstance->mInstance));
        OutputBytes(extPanId, OT_EXT_PAN_ID_SIZE, output_panid);
        blobmsg_add_string(&mBuf, "ExtPanId", output_panid);
    }
    else if (!strcmp(action, "mode"))
    {
        otLinkModeConfig linkMode;
        char             mode[5] = "";

        memset(&linkMode, 0, sizeof(otLinkModeConfig));

        linkMode = otThreadGetLinkMode(otUbusServerInstance->mInstance);

        if (linkMode.mRxOnWhenIdle)
        {
            strcat(mode, "r");
        }

        if (linkMode.mSecureDataRequests)
        {
            strcat(mode, "s");
        }

        if (linkMode.mDeviceType)
        {
            strcat(mode, "d");
        }

        if (linkMode.mNetworkData)
        {
            strcat(mode, "n");
        }
        blobmsg_add_string(&mBuf, "Mode", mode);
    }
    else if (!strcmp(action, "leaderpartitionid"))
    {
        blobmsg_add_u32(&mBuf, "Leaderpartitionid", otThreadGetLocalLeaderPartitionId(otUbusServerInstance->mInstance));
    }
    else if (!strcmp(action, "leaderdata"))
    {
        otLeaderData leaderData;

        SuccessOrExit(error = otThreadGetLeaderData(mInstance, &leaderData));

        json_uri = blobmsg_open_table(&mBuf, "leaderdata");

        blobmsg_add_u32(&mBuf, "PartitionId", leaderData.mPartitionId);
        blobmsg_add_u32(&mBuf, "Weighting", leaderData.mWeighting);
        blobmsg_add_u32(&mBuf, "DataVersion", leaderData.mDataVersion);
        blobmsg_add_u32(&mBuf, "StableDataVersion", leaderData.mStableDataVersion);
        blobmsg_add_u32(&mBuf, "LeaderRouterId", leaderData.mLeaderRouterId);

        blobmsg_close_table(&mBuf, json_uri);
    }
    else if (!strcmp(action, "networkdata"))
    {
        ubus_send_reply(aContext, aRequest, mNetworkdataBuf.head);
        if (time(NULL) - mSecond > 10)
        {
            struct otIp6Address address;
            uint8_t             tlvTypes[OT_NETWORK_DIAGNOSTIC_TYPELIST_MAX_ENTRIES];
            uint8_t             count             = 0;
            char                multicastAddr[10] = "ff03::2";
            long                value;

            blob_buf_init(&mNetworkdataBuf, 0);

            SuccessOrExit(error = otIp6AddressFromString(multicastAddr, &address));

            value             = 5;
            tlvTypes[count++] = static_cast<uint8_t>(value);
            value             = 16;
            tlvTypes[count++] = static_cast<uint8_t>(value);

            bufNum = 0;
            otThreadSendDiagnosticGet(otUbusServerInstance->mInstance, &address, tlvTypes, count);
            mSecond = time(NULL);
        }
        goto exit;
    }
    else if (!strcmp(action, "joinernum"))
    {
        void *       json_table = NULL;
        void *       json_array = NULL;
        otJoinerInfo joinerInfo;
        uint16_t     iterator        = 0;
        int          joinernum       = 0;
        char         eui64[EXTPANID] = "";

        blob_buf_init(&mBuf, 0);

        json_array = blobmsg_open_array(&mBuf, "joiner_list");
        while (otCommissionerGetNextJoinerInfo(otUbusServerInstance->mInstance, &iterator, &joinerInfo) ==
               OT_ERROR_NONE)
        {
            memset(eui64, 0, sizeof(eui64));

            json_table = blobmsg_open_table(&mBuf, NULL);

            blobmsg_add_string(&mBuf, "pskc", joinerInfo.mPsk);
            OutputBytes(joinerInfo.mEui64.m8, sizeof(joinerInfo.mEui64.m8), eui64);
            blobmsg_add_string(&mBuf, "eui64", eui64);
            if (joinerInfo.mAny)
                blobmsg_add_u16(&mBuf, "isAny", 1);
            else
                blobmsg_add_u16(&mBuf, "isAny", 0);

            blobmsg_close_table(&mBuf, json_table);

            joinernum++;
        }
        blobmsg_close_array(&mBuf, json_array);

        blobmsg_add_u32(&mBuf, "joinernum", joinernum);
    }
    else if (!strcmp(action, "macfilterstate"))
    {
        otMacFilterAddressMode mode = otLinkFilterGetAddressMode(otUbusServerInstance->mInstance);

        blob_buf_init(&mBuf, 0);

        if (mode == OT_MAC_FILTER_ADDRESS_MODE_DISABLED)
        {
            blobmsg_add_string(&mBuf, "state", "disable");
        }
        else if (mode == OT_MAC_FILTER_ADDRESS_MODE_WHITELIST)
        {
            blobmsg_add_string(&mBuf, "state", "whitelist");
        }
        else if (mode == OT_MAC_FILTER_ADDRESS_MODE_BLACKLIST)
        {
            blobmsg_add_string(&mBuf, "state", "blacklist");
        }
        else
        {
            blobmsg_add_string(&mBuf, "state", "error");
        }
    }
    else if (!strcmp(action, "macfilteraddr"))
    {
        otMacFilterEntry    entry;
        otMacFilterIterator iterator = OT_MAC_FILTER_ITERATOR_INIT;

        blob_buf_init(&mBuf, 0);

        json_uri = blobmsg_open_array(&mBuf, "addrlist");

        while (otLinkFilterGetNextAddress(otUbusServerInstance->mInstance, &iterator, &entry) == OT_ERROR_NONE)
        {
            char ubusExtAddress[XPANID_LENGTH] = "";
            OutputBytes(entry.mExtAddress.m8, sizeof(entry.mExtAddress.m8), ubusExtAddress);
            blobmsg_add_string(&mBuf, "addr", ubusExtAddress);
        }

        blobmsg_close_array(&mBuf, json_uri);
    }
    else
    {
        perror("invalid argument in get information ubus\n");
    }

    AppendResult(error, aContext, aRequest);
exit:
    ncpThreadMutex->unlock();
    return 0;
}

void OtUbusServer::HandleDiagnosticGetResponse(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext)
{
    static_cast<OtUbusServer *>(aContext)->HandleDiagnosticGetResponse(
        *static_cast<ot::Message *>(aMessage), *static_cast<const ot::Ip6::MessageInfo *>(aMessageInfo));
}

void OtUbusServer::OutputBytes(const uint8_t *aBytes, uint8_t aLength)
{
    for (int i = 0; i < aLength; i++)
    {
        printf("%02x", aBytes[i]);
    }
}

void OtUbusServer::HandleDiagnosticGetResponse(ot::Message &aMessage, const ot::Ip6::MessageInfo &aMessageInfo)
{
    uint8_t  childNum = 0;
    uint16_t rloc16;
    uint16_t sockRloc16 = 0;
    void *   json_array = NULL;
    void *   json_item  = NULL;
    char     xrloc[10];
    uint16_t offset;

    ot::NetworkDiagnostic::ChildTableTlv   childTlv;
    ot::NetworkDiagnostic::ChildTableEntry childEntry;
    ot::NetworkDiagnostic::RouteTlv        routeTlv;

    char networkdata[20];
    sprintf(networkdata, "networkdata%d", bufNum);
    json_uri = blobmsg_open_table(&mNetworkdataBuf, networkdata);
    bufNum++;

    if (aMessageInfo.GetSockAddr().IsRoutingLocator())
    {
        sockRloc16 = aMessageInfo.GetPeerAddr().mFields.m16[7];
        sprintf(xrloc, "0x%04x", sockRloc16);
        blobmsg_add_string(&mNetworkdataBuf, "rloc", xrloc);
    }

    json_array = blobmsg_open_array(&mNetworkdataBuf, "routedata");
    if (ot::NetworkDiagnostic::NetworkDiagnosticTlv::GetTlv(
            aMessage, ot::NetworkDiagnostic::NetworkDiagnosticTlv::kRoute, sizeof(routeTlv), routeTlv) == OT_ERROR_NONE)
    {
        for (uint8_t i = 0, routeid = 0; i < 64; i++)
        {
            if (routeTlv.IsRouterIdSet(i))
            {
                uint8_t in, out;
                in  = routeTlv.GetLinkQualityIn(routeid);
                out = routeTlv.GetLinkQualityOut(routeid);
                if (in != 0 && out != 0)
                {
                    json_item = blobmsg_open_table(&mNetworkdataBuf, "router");
                    rloc16    = i << 10;
                    blobmsg_add_u32(&mNetworkdataBuf, "routerid", i);
                    sprintf(xrloc, "0x%04x", rloc16);
                    blobmsg_add_string(&mNetworkdataBuf, "rloc", xrloc);
                    blobmsg_close_table(&mNetworkdataBuf, json_item);
                }
                routeid++;
            }
        }
    }
    blobmsg_close_array(&mNetworkdataBuf, json_array);

    json_array = blobmsg_open_array(&mNetworkdataBuf, "childdata");
    if (ot::NetworkDiagnostic::NetworkDiagnosticTlv::GetTlv(aMessage,
                                                            ot::NetworkDiagnostic::NetworkDiagnosticTlv::kChildTable,
                                                            sizeof(childTlv), childTlv) == OT_ERROR_NONE)
    {
        childNum = childTlv.GetNumEntries();
        for (uint8_t i = 0; i < childNum; i++)
        {
            json_item = blobmsg_open_table(&mNetworkdataBuf, "child");
            ot::Tlv::GetOffset(aMessage, ot::NetworkDiagnostic::NetworkDiagnosticTlv::kChildTable, offset);
            childTlv.ReadEntry(childEntry, aMessage, offset, i);
            sprintf(xrloc, "0x%04x", (sockRloc16 | childEntry.GetChildId()));
            blobmsg_add_string(&mNetworkdataBuf, "rloc", xrloc);
            blobmsg_add_u16(&mNetworkdataBuf, "mode", childEntry.GetMode().Get());
            blobmsg_close_table(&mNetworkdataBuf, json_item);
        }
    }
    blobmsg_close_array(&mNetworkdataBuf, json_array);

    blobmsg_close_table(&mNetworkdataBuf, json_uri);
}

int OtUbusServer::Ubus_set_information(struct ubus_context *aContext,
                                       struct ubus_object *,
                                       struct ubus_request_data *aRequest,
                                       const char *,
                                       struct blob_attr *msg,
                                       const char *      action)
{
    otError error = OT_ERROR_NONE;

    blob_buf_init(&mBuf, 0);

    ncpThreadMutex->lock();
    if (!strcmp(action, "networkname"))
    {
        struct blob_attr *tb[SET_NETWORK_MAX];

        blobmsg_parse(set_networkname_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            char *newName = blobmsg_get_string(tb[SETNETWORK]);
            mInstance     = mController->getInstance();
            SuccessOrExit(error = otThreadSetNetworkName(otUbusServerInstance->mInstance, newName));
        }
    }
    else if (!strcmp(action, "channel"))
    {
        struct blob_attr *tb[SET_NETWORK_MAX];

        blobmsg_parse(set_channel_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            uint32_t channel = blobmsg_get_u32(tb[SETNETWORK]);
            mInstance        = mController->getInstance();
            SuccessOrExit(error = otLinkSetChannel(otUbusServerInstance->mInstance, static_cast<uint8_t>(channel)));
        }
    }
    else if (!strcmp(action, "panid"))
    {
        struct blob_attr *tb[SET_NETWORK_MAX];

        blobmsg_parse(set_panid_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            long  value;
            char *panid = blobmsg_get_string(tb[SETNETWORK]);
            SuccessOrExit(error = ParseLong(panid, value));
            mInstance = mController->getInstance();
            error     = otLinkSetPanId(otUbusServerInstance->mInstance, static_cast<otPanId>(value));
        }
    }
    else if (!strcmp(action, "masterkey"))
    {
        struct blob_attr *tb[SET_NETWORK_MAX];

        blobmsg_parse(set_masterkey_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            otMasterKey key;
            char *      masterkey = blobmsg_get_string(tb[SETNETWORK]);

            VerifyOrExit(Hex2Bin(masterkey, key.m8, sizeof(key.m8)) == OT_MASTER_KEY_SIZE, error = OT_ERROR_PARSE);
            mInstance = mController->getInstance();
            SuccessOrExit(error = otThreadSetMasterKey(otUbusServerInstance->mInstance, &key));
        }
    }
    else if (!strcmp(action, "pskc"))
    {
        struct blob_attr *tb[SET_NETWORK_MAX];

        blobmsg_parse(set_pskc_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            otPskc pskc;

            VerifyOrExit(Hex2Bin(blobmsg_get_string(tb[SETNETWORK]), pskc.m8, sizeof(pskc)) == OT_PSKC_MAX_SIZE,
                         error = OT_ERROR_PARSE);
            mInstance = mController->getInstance();
            SuccessOrExit(error = otThreadSetPskc(otUbusServerInstance->mInstance, &pskc));
        }
    }
    else if (!strcmp(action, "extpanid"))
    {
        struct blob_attr *tb[SET_NETWORK_MAX];

        blobmsg_parse(set_extpanid_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            otExtendedPanId extPanId;
            char *          input = blobmsg_get_string(tb[SETNETWORK]);
            VerifyOrExit(Hex2Bin(input, extPanId.m8, sizeof(extPanId)) >= 0, error = OT_ERROR_PARSE);
            mInstance = mController->getInstance();
            error     = otThreadSetExtendedPanId(otUbusServerInstance->mInstance, &extPanId);
        }
    }
    else if (!strcmp(action, "mode"))
    {
        otLinkModeConfig  linkMode;
        struct blob_attr *tb[SET_NETWORK_MAX];

        blobmsg_parse(set_mode_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            char *inputMode = blobmsg_get_string(tb[SETNETWORK]);
            for (char *ch = inputMode; *ch != '\0'; ch++)
            {
                switch (*ch)
                {
                case 'r':
                    linkMode.mRxOnWhenIdle = 1;
                    break;

                case 's':
                    linkMode.mSecureDataRequests = 1;
                    break;

                case 'd':
                    linkMode.mDeviceType = 1;
                    break;

                case 'n':
                    linkMode.mNetworkData = 1;
                    break;

                default:
                    ExitNow(error = OT_ERROR_PARSE);
                }
            }

            mInstance = mController->getInstance();
            SuccessOrExit(error = otThreadSetLinkMode(otUbusServerInstance->mInstance, linkMode));
        }
    }
    else if (!strcmp(action, "leaderpartitionid"))
    {
        struct blob_attr *tb[SET_NETWORK_MAX];

        blobmsg_parse(set_leaderpartitionid_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            uint32_t input = blobmsg_get_u32(tb[SETNETWORK]);
            mInstance      = mController->getInstance();
            otThreadSetLocalLeaderPartitionId(otUbusServerInstance->mInstance, input);
        }
    }
    else if (!strcmp(action, "macfilteradd"))
    {
        struct blob_attr *tb[SET_NETWORK_MAX];
        otExtAddress      extAddr;

        blobmsg_parse(macfilter_add_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            char *addr = blobmsg_get_string(tb[SETNETWORK]);

            VerifyOrExit(Hex2Bin(addr, extAddr.m8, OT_EXT_ADDRESS_SIZE) == OT_EXT_ADDRESS_SIZE, error = OT_ERROR_PARSE);

            mInstance = mController->getInstance();
            error     = otLinkFilterAddAddress(otUbusServerInstance->mInstance, &extAddr);

            VerifyOrExit(error == OT_ERROR_NONE || error == OT_ERROR_ALREADY);
        }
    }
    else if (!strcmp(action, "macfilterremove"))
    {
        struct blob_attr *tb[SET_NETWORK_MAX];
        otExtAddress      extAddr;

        blobmsg_parse(macfilter_remove_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            char *addr = blobmsg_get_string(tb[SETNETWORK]);
            VerifyOrExit(Hex2Bin(addr, extAddr.m8, OT_EXT_ADDRESS_SIZE) == OT_EXT_ADDRESS_SIZE, error = OT_ERROR_PARSE);

            mInstance = mController->getInstance();
            SuccessOrExit(error = otLinkFilterRemoveAddress(otUbusServerInstance->mInstance, &extAddr));
        }
    }
    else if (!strcmp(action, "macfiltersetstate"))
    {
        struct blob_attr *tb[SET_NETWORK_MAX];

        blobmsg_parse(macfilter_setstate_policy, SET_NETWORK_MAX, tb, blob_data(msg), blob_len(msg));
        if (tb[SETNETWORK] != NULL)
        {
            char *state = blobmsg_get_string(tb[SETNETWORK]);

            mInstance = mController->getInstance();

            if (strcmp(state, "disable") == 0)
            {
                SuccessOrExit(error = otLinkFilterSetAddressMode(otUbusServerInstance->mInstance,
                                                                 OT_MAC_FILTER_ADDRESS_MODE_DISABLED));
            }
            else if (strcmp(state, "whitelist") == 0)
            {
                SuccessOrExit(error = otLinkFilterSetAddressMode(otUbusServerInstance->mInstance,
                                                                 OT_MAC_FILTER_ADDRESS_MODE_WHITELIST));
            }
            else if (strcmp(state, "blacklist") == 0)
            {
                SuccessOrExit(error = otLinkFilterSetAddressMode(otUbusServerInstance->mInstance,
                                                                 OT_MAC_FILTER_ADDRESS_MODE_BLACKLIST));
            }
        }
    }
    else if (!strcmp(action, "macfilterclear"))
    {
        mInstance = mController->getInstance();
        otLinkFilterClearAddresses(otUbusServerInstance->mInstance);
    }
    else
    {
        perror("invalid argument in get information ubus\n");
    }

exit:
    ncpThreadMutex->unlock();
    AppendResult(error, aContext, aRequest);
    return 0;
}

void OtUbusServer::getState(otInstance *aInstance, char *state)
{
    switch (otThreadGetDeviceRole(aInstance))
    {
    case OT_DEVICE_ROLE_DISABLED:
        strcpy(state, "disabled");
        break;

    case OT_DEVICE_ROLE_DETACHED:
        strcpy(state, "detached");
        break;

    case OT_DEVICE_ROLE_CHILD:
        strcpy(state, "child");
        break;

    case OT_DEVICE_ROLE_ROUTER:
        strcpy(state, "router");
        break;

    case OT_DEVICE_ROLE_LEADER:
        strcpy(state, "leader");
        break;

    default:
        strcpy(state, "invalid state");
        break;
    }
}

void OtUbusServer::Ubus_add_fd()
{
    ubus_add_uloop(mContext);

#ifdef FD_CLOEXEC
    fcntl(mContext->sock.fd, F_SETFD, fcntl(mContext->sock.fd, F_GETFD) | FD_CLOEXEC);
#endif
}

void OtUbusServer::Ubus_reconn_timer(struct uloop_timeout *timeout)
{
    getInstance().Ubus_reconn_timer_Detail(timeout);
}

void OtUbusServer::Ubus_reconn_timer_Detail(struct uloop_timeout *)
{
    static struct uloop_timeout retry = {
        list : {},
        pending : false,
        cb : Ubus_reconn_timer,
        time : {},
    };
    int t = 2;

    if (ubus_reconnect(mContext, mSockPath) != 0)
    {
        uloop_timeout_set(&retry, t * 1000);
        return;
    }

    Ubus_add_fd();
}

void OtUbusServer::Ubus_connection_lost(struct ubus_context *)
{
    Ubus_reconn_timer(NULL);
}

int OtUbusServer::Display_ubus_init(const char *path)
{
    uloop_init();
    signal(SIGPIPE, SIG_IGN);

    mSockPath = path;

    mContext = ubus_connect(path);
    if (!mContext)
    {
        printf("ubus connect failed\n");
        return -1;
    }

    printf("connected as %08x\n", mContext->local_id);
    mContext->connection_lost = Ubus_connection_lost;

    /* file description */
    Ubus_add_fd();

    /* Add a object */
    if (ubus_add_object(mContext, &otbr) != 0)
    {
        printf("ubus add obj failed\n");
        return -1;
    }

    return 0;
}

void OtUbusServer::Display_ubus_done(void)
{
    if (mContext)
    {
        ubus_free(mContext);
        mContext = NULL;
    }
}

void OtUbusServer::InstallUbusObject()
{
    char *path = NULL;

    if (-1 == Display_ubus_init(path))
    {
        printf("ubus connect failed!\n");
        return;
    }

    printf("uloop_run\n");
    uloop_run();

    Display_ubus_done();

    uloop_done();
}

otError OtUbusServer::ParseLong(char *aString, long &aLong)
{
    char *endptr;
    aLong = strtol(aString, &endptr, 0);
    return (*endptr == '\0') ? OT_ERROR_NONE : OT_ERROR_PARSE;
}

int OtUbusServer::Hex2Bin(const char *aHex, uint8_t *aBin, uint16_t aBinLength)
{
    size_t      hexLength = strlen(aHex);
    const char *hexEnd    = aHex + hexLength;
    uint8_t *   cur       = aBin;
    uint8_t     numChars  = hexLength & 1;
    uint8_t     byte      = 0;
    int         rval;

    VerifyOrExit((hexLength + 1) / 2 <= aBinLength, rval = -1);

    while (aHex < hexEnd)
    {
        if ('A' <= *aHex && *aHex <= 'F')
        {
            byte |= 10 + (*aHex - 'A');
        }
        else if ('a' <= *aHex && *aHex <= 'f')
        {
            byte |= 10 + (*aHex - 'a');
        }
        else if ('0' <= *aHex && *aHex <= '9')
        {
            byte |= *aHex - '0';
        }
        else
        {
            ExitNow(rval = -1);
        }

        aHex++;
        numChars++;

        if (numChars >= 2)
        {
            numChars = 0;
            *cur++   = byte;
            byte     = 0;
        }
        else
        {
            byte <<= 4;
        }
    }

    rval = static_cast<int>(cur - aBin);

exit:
    return rval;
}

} // namespace ubus
} // namespace BorderRouter
} // namespace ot

void otUbusServerInit(ot::BorderRouter::Ncp::ControllerOpenThread *aController, std::mutex *aNcpThreadMutex)
{
    ot::BorderRouter::ubus::ncpThreadMutex = aNcpThreadMutex;
    ot::BorderRouter::ubus::OtUbusServer::initialize(aController);

    ot::BorderRouter::ubus::ubus_efd = eventfd(0, 0);
    if (ot::BorderRouter::ubus::ubus_efd == -1)
    {
        perror("ubus eventfd create failed\n");
        exit(EXIT_FAILURE);
    }
}

void otUbusServerRun(void)
{
    ot::BorderRouter::ubus::OtUbusServer::getInstance().InstallUbusObject();
}

void otUbusUpdateFdSet(fd_set &aReadFdSet, int &aMaxFd)
{
    VerifyOrExit(ot::BorderRouter::ubus::ubus_efd != -1);

    FD_SET(ot::BorderRouter::ubus::ubus_efd, &aReadFdSet);

    if (aMaxFd < ot::BorderRouter::ubus::ubus_efd)
    {
        aMaxFd = ot::BorderRouter::ubus::ubus_efd;
    }

exit:
    return;
}

void otUbusProcess(const fd_set &aReadFdSet)
{
    ssize_t  rval;
    uint64_t u;

    VerifyOrExit(ot::BorderRouter::ubus::ubus_efd != -1);

    if (FD_ISSET(ot::BorderRouter::ubus::ubus_efd, &aReadFdSet))
    {
        rval = read(ot::BorderRouter::ubus::ubus_efd, &u, sizeof(uint64_t));
        if (rval != sizeof(uint64_t))
        {
            perror("read ubus eventfd failed\n");
            exit(EXIT_FAILURE);
        }
        goto exit;
    }

exit:
    return;
}
