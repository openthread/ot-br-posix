LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libcoap
LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/third_party/libcoap/repo/include/coap \
    $(LOCAL_PATH)/third_party/libcoap/repo/include/ \
    $(LOCAL_PATH)/third_party/libcoap/preconfig \
    $(NULL)

LOCAL_CFLAGS += \
    -Wall \
    -Wextra \
    -Wno-unused-parameter \
    -DWITH_POSIX=1 \
    -DCOAP_MAX_PDU_SIZE=1400 \
    $(NULL)

LOCAL_SRC_FILES += \
    third_party/libcoap/repo/src/address.c \
    third_party/libcoap/repo/src/async.c \
    third_party/libcoap/repo/src/block.c \
    third_party/libcoap/repo/src/coap_io.c \
    third_party/libcoap/repo/src/coap_time.c \
    third_party/libcoap/repo/src/debug.c \
    third_party/libcoap/repo/src/encode.c \
    third_party/libcoap/repo/src/hashkey.c \
    third_party/libcoap/repo/src/mem.c \
    third_party/libcoap/repo/src/net.c \
    third_party/libcoap/repo/src/option.c \
    third_party/libcoap/repo/src/pdu.c \
    third_party/libcoap/repo/src/resource.c \
    third_party/libcoap/repo/src/str.c \
    third_party/libcoap/repo/src/subscribe.c \
    third_party/libcoap/repo/src/uri.c \
    $(NULL)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_C_INCLUDES)

include $(BUILD_STATIC_LIBRARY)

