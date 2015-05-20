###########################################################
## Standard rules for building an executable file.
##
## Additional inputs from base_rules.make:
## None.
###########################################################

HOST := 1

include $(BUILD_SYSTEM)/base_rules.make

$(full_target): $(all_objects) $(all_libraries)
	$(transform-host-o-to-executable)

HOST := 
