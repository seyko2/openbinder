LOCAL_PATH:= $(call my-dir)

include $(SRC_LIBRARIES)/libbinder/app/Makefile
include $(SRC_LIBRARIES)/libbinder/arch/Makefile
include $(SRC_LIBRARIES)/libbinder/package/Makefile
include $(SRC_LIBRARIES)/libbinder/services/Makefile
include $(SRC_LIBRARIES)/libbinder/storage/Makefile
include $(SRC_LIBRARIES)/libbinder/support/Makefile
include $(SRC_LIBRARIES)/libbinder/xml/Makefile

include $(CLEAR_VARS)

BASE_PATH:= $(LOCAL_PATH)
TARGET_PATH:= $(OUT_LIBRARIES)
NO_INTERFACE_DEPENDENCY:= 1
DONT_BUILD_TARGET:= 1

SRC_FILES:= \
	$(appBootstrapSources) \
	$(archBootstrapSources) \
	$(packageBootstrapSources) \
	$(servicesBootstrapSources) \
	$(storageBootstrapSources) \
	$(supportBootstrapSources) \
	$(xmlBootstrapSources)
TARGET:= libbinder_bootstrap

# We normally want to do a release build for the
# bootstrap library, since it is used as part of the build.
LOCAL_CFLAGS+= -DLIBBE_BOOTSTRAP=1 $(TOOL_CFLAGS)
LOCAL_LDFLAGS:= $(TOOL_LDFLAGS)

include $(BUILD_HOST_STATIC_LIBRARY)

include $(CLEAR_VARS)

BASE_PATH:= $(LOCAL_PATH)
TARGET_PATH:= $(OUT_LIBRARIES)

SRC_FILES:= \
	$(appSources) \
	$(archSources) \
	$(packageSources) \
	$(servicesSources) \
	$(storageSources) \
	$(supportSources) \
	$(xmlSources)
IDL_FILES:= \
	$(appIDLs) \
	$(archIDLs) \
	$(packageIDLs) \
	$(servicesIDLs) \
	$(storageIDLs) \
	$(supportIDLs) \
	$(xmlIDLs)
LIB_FILES:= \
	$(OUT_LIBRARIES)/libbinder_glue.a
LOCAL_CFLAGS:=
TARGET:= libbinder

include $(BUILD_SHARED_LIBRARY)
