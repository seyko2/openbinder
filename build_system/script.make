###########################################################
## Standard rules for building script file.
##
## Inputs:
## SCRIPTS: The script files to process.
## BASE_PATH: Your location.
## TARGET_PATH: (optional) Where to place the scripts.
## REPLACE_VARS: (optional) Variables to replace while processing.
## DONT_BUILD_TARGET: (optional) If set, don't build scripts by default.
###########################################################

ifeq ($(strip $(TARGET_PATH)),)
ifeq ($(strip $(DONT_BUILD_TARGET)),)
TARGET_PATH := $(OUT)/$(BASE_PATH)
else
TARGET_PATH := $(OUT_INTERMEDIATES)/$(BASE_PATH)
endif
endif

ifeq ($(strip $(REPLACE_VARS)),)
REPLACE_VARS := \
	TOP \
	TOPDIR \
	BUILD_SYSTEM \
	SRC_COMPONENTS \
	SRC_HEADERS \
	SRC_INTERFACES \
	SRC_LIBRARIES \
	SRC_MODULES \
	SRC_SCRIPTS \
	SRC_SERVERS \
	SRC_TOOLS \
	OUT \
	OUT_EXECUTABLES \
	OUT_HEADERS \
	OUT_HOST_EXECUTABLES \
	OUT_INTERMEDIATES \
	OUT_LIBRARIES \
	OUT_PACKAGES \
	OUT_SCRIPTS
endif

out_scripts:= $(addprefix $(TARGET_PATH)/,$(SCRIPTS))
ifeq ($(strip $(DONT_BUILD_TARGET)),)
ALL_TARGETS+= $(out_scripts)
else
ALL_INTERMEDIATES+= $(out_scripts)
endif

$(out_scripts): $(TARGET_PATH)/%: $(BASE_PATH)/%
	$(transform-variables)
$(out_scripts): REPLACE_VARS:=$(REPLACE_VARS)
