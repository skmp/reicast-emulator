LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := jnitest
LOCAL_CFLAGS    := -Werror
LOCAL_SRC_FILES := jnitest.c
LOCAL_LDLIBS    := -llog 

APP_CPPFLAGS += -std=c++11

include $(BUILD_SHARED_LIBRARY)