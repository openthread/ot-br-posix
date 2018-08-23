#
#  Copyright (c) 2018, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= otbr-agent
LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/src \
    $(LOCAL_PATH)/third_party/wpantund \
    $(LOCAL_PATH)/third_party/wpantund/repo/src \
    $(LOCAL_PATH)/third_party/wpantund/repo/src/ipc-dbus \
    $(LOCAL_PATH)/third_party/wpantund/repo/src/util \
    $(LOCAL_PATH)/third_party/wpantund/repo/src/wpantund \
    $(LOCAL_PATH)/third_party/wpantund/repo/src/wpanctl \
    $(LOCAL_PATH)/third_party/wpantund/repo/third_party/assert-macros \
    $(LOCAL_PATH)/third_party/wpantund/repo/third_party/openthread/src/ncp \
    $(NULL)

LOCAL_CFLAGS := \
    -DPACKAGE_VERSION=\"0.01.00\" \
    $(NULL)

LOCAL_SRC_FILES := \
    third_party/wpantund/repo/src/util/string-utils.c \
    third_party/wpantund/repo/src/wpanctl/wpanctl-utils.c \
    src/agent/agent_instance.cpp \
    src/agent/border_agent.cpp \
    src/agent/mdns_mdnssd.cpp \
    src/agent/main.cpp \
    src/agent/ncp_wpantund.cpp \
    src/common/event_emitter.cpp \
    src/common/logging.cpp \
    src/utils/hex.cpp \
    $(NULL)

LOCAL_SHARED_LIBRARIES := libdbus libmdnssd

include $(BUILD_EXECUTABLE)
