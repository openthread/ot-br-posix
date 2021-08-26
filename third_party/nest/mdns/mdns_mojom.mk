ifeq ($(OTBR_MDNS),mojo)
include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libmdns_mojom
LOCAL_MODULE_TAGS := eng
LOCAL_CPP_EXTENSION := .cc

LOCAL_C_INCLUDES := \
    external/gtest/include \
    $(NULL)

LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter

LOCAL_CPPFLAGS += -std=c++14

LOCAL_MOJOM_FILES := \
    services/network/public/mojom/ip_endpoint.mojom \
    services/network/public/mojom/ip_address.mojom \
    chromecast/internal/receiver/mdns/public/mojom/mdns.mojom \
    $(NULL)

LOCAL_MOJOM_TYPEMAP_FILES :=

LIB_MOJO_ROOT := external/gwifi/libchrome-cros
LOCAL_SOURCE_ROOT := chromium/src

include external/gwifi/libchrome-cros/generate_mojom_sources.mk

MDNS_MOJOM_SRCS := $(gen_src)

LOCAL_SHARED_LIBRARIES += libchrome libmojo

include $(BUILD_STATIC_LIBRARY)
endif
