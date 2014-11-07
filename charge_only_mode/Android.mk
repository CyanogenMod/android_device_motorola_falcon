# Copyright 2005 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    alarm.c \
    events.c \
    hardware.c \
    main.c

LOCAL_SRC_FILES += \
    draw.c \
    screen.c

LOCAL_STATIC_LIBRARIES := libunz libcutils libc liblog

LOCAL_C_INCLUDES := external/zlib

LOCAL_CFLAGS := -I$(LOCAL_PATH)/assets

LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE:= charge_only_mode

LOCAL_FORCE_STATIC_EXECUTABLE := true

include $(BUILD_EXECUTABLE)
