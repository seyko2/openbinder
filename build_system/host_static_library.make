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

HOST := 1

include $(BUILD_SYSTEM)/base_rules.make

$(full_target): $(all_objects)
	$(transform-host-o-to-static-lib)

HOST :=
