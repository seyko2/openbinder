###########################################################
## Standard rules for building a normal shared library.
##
## Additional inputs from base_rules.make:
## None.
##
## TARGET_SUFFIX will be set for you.
###########################################################

ifeq ($(strip $(TARGET_SUFFIX)),)
TARGET_SUFFIX := .so
endif

include $(BUILD_SYSTEM)/base_rules.make

$(full_target): $(all_objects) $(all_libraries)
	$(transform-o-to-shared-lib)
