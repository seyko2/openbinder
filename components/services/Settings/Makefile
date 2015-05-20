LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

BASE_PATH:= $(LOCAL_PATH)
PACKAGE_NAMESPACE:= org.openbinder.services
PACKAGE_LEAF:= Settings
LOCAL_CFLAGS:= -DOPENBINDER_SETTINGS_BUILD=1
SRC_FILES:= \
	SettingsCatalog.cpp \
	main.cpp

#	Legacy.cpp \

include $(BUILD_PACKAGE)
