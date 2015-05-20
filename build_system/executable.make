###########################################################
## Standard rules for building an executable file.
##
## Additional inputs from base_rules.make:
## None.
###########################################################

include $(BUILD_SYSTEM)/base_rules.make

$(full_target): $(all_objects) $(all_libraries)
	$(transform-o-to-executable)
