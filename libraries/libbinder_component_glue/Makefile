LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

BASE_PATH:= $(LOCAL_PATH)
TARGET_PATH:= $(OUT_LIBRARIES)

SRC_FILES:= \
	binder_component_glue.cpp
TARGET:= libbinder_component_glue
LOCAL_C_INCLUDES+= $(SRC_LIBRARIES)/libbinder_glue

include $(BUILD_STATIC_LIBRARY)
