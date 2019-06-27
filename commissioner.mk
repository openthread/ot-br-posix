LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE := libcommissioner_mojom
LOCAL_MODULE_TAGS := eng
LOCAL_CPP_EXTENSION := .cc

LOCAL_C_INCLUDES := \
    external/libchrome \
    external/gtest/include \
    $(NULL)

LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter

LOCAL_CPPFLAGS += -std=c++14

LOCAL_MOJOM_FILES := \
    src/commissioner/mojom/thread_commissioner.mojom \
    $(NULL)

LOCAL_MOJOM_TYPEMAP_FILES :=

LIB_MOJO_ROOT := external/libchrome
LOCAL_SOURCE_ROOT := $(LOCAL_PATH)

include external/libchrome/generate_mojom_sources.mk

COMMISSIONER_MOJOM_SRC := $(gen_src)

LOCAL_SHARED_LIBRARIES += libchrome libmojo

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE := commissioner-mojo-server
LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/src \
    external/libchrome \
    external/gtest/include \
    $(NULL)

LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter
LOCAL_CFLAGS += \
    -DPACKAGE_VERSION=\"0.01.00\" \
    $(OT_CORE_CFLAGS)             \
    $(NULL)

LOCAL_CPPFLAGS += -std=c++14

LOCAL_SRC_FILES := \
    src/commissioner/addr_utils.cpp \
    src/commissioner/commissioner.cpp \
    src/commissioner/commissioner_argcargv.cpp \
    src/commissioner/joiner_session.cpp \
    src/common/coap_libcoap.cpp \
    src/common/dtls_mbedtls.cpp \
    src/common/logging.cpp \
    src/utils/hex.cpp \
    src/utils/pskc.cpp \
    src/utils/steering_data.cpp \
    src/commissioner/commission_mojo_server.cpp \
    src/commissioner/commission_mojo_server_main.cpp \
    $(NULL)

# The generated header files are not in dependency chain.
# Force dependency here
LOCAL_STATIC_LIBRARIES += libcoap ot-core
LOCAL_SHARED_LIBRARIES += libcommissioner_mojom libchrome libmojo
LOCAL_ADDITIONAL_DEPENDENCIES += $(COMMISSIONER_MOJOM_SRC)

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE := commissioner-mojo-client
LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/src \
    external/libchrome \
    external/gtest/include \
    $(NULL)

LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter
LOCAL_CFLAGS += \
    -DPACKAGE_VERSION=\"0.01.00\" \
    $(OT_CORE_CFLAGS)             \
    $(NULL)

LOCAL_CPPFLAGS += -std=c++14

LOCAL_SRC_FILES := \
    src/commissioner/commission_mojo_client.cpp \
    $(NULL)

# The generated header files are not in dependency chain.
# Force dependency here
LOCAL_SHARED_LIBRARIES += libcommissioner_mojom libchrome libmojo
LOCAL_ADDITIONAL_DEPENDENCIES += $(COMMISSIONER_MOJOM_SRC)

include $(BUILD_EXECUTABLE)

