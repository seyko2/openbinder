###########################################################
## Standard rules for building a static library.
##
## Additional inputs from base_rules.make:
## None.
##
## TARGET_SUFFIX will be set for you.
###########################################################

ifeq ($(strip $(TARGET_SUFFIX)),)
TARGET_SUFFIX := .a
endif

include $(BUILD_SYSTEM)/base_rules.make

$(full_target): $(all_objects)
	$(transform-o-to-static-lib)
