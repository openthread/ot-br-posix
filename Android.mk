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

LOCAL_PATH := $(call my-dir)

OTBR_MDNS ?= mojo

ifeq ($(OTBR_MDNS),mojo)
include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libmdns_mojom
LOCAL_MODULE_TAGS := eng
LOCAL_CPP_EXTENSION := .cc

LOCAL_C_INCLUDES := \
    external/libchrome \
    external/gtest/include \
    $(NULL)

LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter

LOCAL_CPPFLAGS += -std=c++14

LOCAL_MOJOM_FILES := \
    third_party/chromecast/mojom/mdns.mojom \
    $(NULL)

LOCAL_MOJOM_TYPEMAP_FILES :=

LIB_MOJO_ROOT := external/libchrome
LOCAL_SOURCE_ROOT := $(LOCAL_PATH)

include external/libchrome/generate_mojom_sources.mk

MDNS_MOJOM_SRCS := $(gen_src)

LOCAL_SHARED_LIBRARIES += libchrome libmojo

include $(BUILD_STATIC_LIBRARY)
endif

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libotbr-dbus-client
LOCAL_MODULE_TAGS := eng

LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/src \
    external/openthread/include \
    $(NULL)

LOCAL_SRC_FILES = \
    src/dbus/client/client_error.cpp \
    src/dbus/client/thread_api_dbus.cpp \
    src/dbus/common/dbus_message_helper.cpp \
    src/dbus/common/dbus_message_helper_openthread.cpp \
    src/dbus/common/error.cpp \
    $(NULL)

LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/src \
    $(LOCAL_PATH)/include \
    $(NULL)

LOCAL_SHARED_LIBRARIES += libdbus

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE := otbr-agent
LOCAL_MODULE_TAGS := eng
LOCAL_SHARED_LIBRARIES := libdbus

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/src \
    external/libchrome \
    external/gtest/include \
    external/openthread/include \
    external/openthread/src \
    external/openthread/src/posix/platform/include \
    $(NULL)

LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter
LOCAL_CFLAGS += \
    -DPACKAGE_VERSION=\"0.01.00\" \
    -DOTBR_ENABLE_DBUS_SERVER=1 \
    $(NULL)

LOCAL_CPPFLAGS += -std=c++14

LOCAL_SRC_FILES := \
    src/agent/agent_instance.cpp \
    src/agent/border_agent.cpp \
    src/agent/main.cpp \
    src/agent/ncp_openthread.cpp \
    src/agent/thread_helper.cpp \
    src/common/logging.cpp \
    src/dbus/common/dbus_message_helper.cpp \
    src/dbus/common/dbus_message_helper_openthread.cpp \
    src/dbus/common/error.cpp \
    src/dbus/server/dbus_agent.cpp \
    src/dbus/server/dbus_object.cpp \
    src/dbus/server/dbus_thread_object.cpp \
    src/dbus/server/error_helper.cpp \
    src/utils/event_emitter.cpp \
    src/utils/hex.cpp \
    src/utils/strcpy_utils.cpp \
    $(NULL)

LOCAL_STATIC_LIBRARIES += \
    libopenthread-ncp \
    libopenthread-cli \
    ot-core \
    $(NULL)

LOCAL_LDLIBS := \
    -lutil

ifeq ($(OTBR_MDNS),mDNSResponder)
LOCAL_SRC_FILES += \
    src/mdns/mdns_mdnssd.cpp \
    $(NULL)

LOCAL_SHARED_LIBRARIES += libmdnssd
else
ifeq ($(OTBR_MDNS),mojo)
LOCAL_CFLAGS += \
    -DOTBR_ENABLE_MDNS_MOJO=1 \
    $(NULL)

LOCAL_SRC_FILES += \
    src/mdns/mdns_mojo.cpp \
    $(NULL)

# The generated header files are not in dependency chain.
# Force dependency here
LOCAL_ADDITIONAL_DEPENDENCIES += $(MDNS_MOJOM_SRCS)
LOCAL_STATIC_LIBRARIES += libmdns_mojom
LOCAL_SHARED_LIBRARIES += libchrome libmojo
endif
endif

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE := otbr-agent.conf
LOCAL_MODULE_TAGS := eng

LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/dbus-1/system.d
LOCAL_SRC_FILES := src/agent/otbr-agent.conf
$(LOCAL_PATH)/src/agent/otbr-agent.conf: $(LOCAL_PATH)/src/agent/otbr-agent.conf.in
	sed 's/@OTBR_AGENT_USER@/root/g' $< > $@

include $(BUILD_PREBUILT)
