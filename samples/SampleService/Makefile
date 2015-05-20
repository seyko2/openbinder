LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

BASE_PATH:= $(LOCAL_PATH)
TARGET_PATH:= $(OUT_LIBRARIES)

# Note that we include our local IDL file in the
# source list.  If, instead, the IDL file was placed
# in the "interfaces" directory, it would be put
# in the IDL_FILES list.
SRC_FILES:= \
	SampleServiceLib.cpp \
	ISampleService.idl
IDL_FILES:=
TARGET:= libsampleservice

include $(BUILD_BINDER_SHARED_LIBRARY)

include $(CLEAR_VARS)

BASE_PATH:= $(LOCAL_PATH)
PACKAGE_NAMESPACE:= org.openbinder.samples
PACKAGE_LEAF:= Service
SHARED_LIBRARIES:= libsampleservice
SRC_FILES:= \
	SampleService.cpp \
	SampleService_components.cpp

include $(BUILD_PACKAGE)
