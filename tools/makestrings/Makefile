LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

BASE_PATH:= $(LOCAL_PATH)
TARGET_PATH:= $(OUT_HOST_EXECUTABLES)
NO_INTERFACE_DEPENDENCY:= 1
SRC_FILES:= \
	Strings.cpp
LIB_FILES:= \
	$(OUT_LIBRARIES)/libbinder_bootstrap.a
TARGET:= makestrings

# We normally want to do a release build for pidgen,
# since it is used as part of the build.
LOCAL_CFLAGS:= $(TOOL_CFLAGS)
LOCAL_LDFLAGS:= $(TOOL_LDFLAGS)

include $(BUILD_HOST_EXECUTABLE)
