###########################################################
## Standard rules for building an executable file that
## uses the Binder runtime services.
##
## Additional inputs from base_rules.make:
## None.
###########################################################

SHARED_LIBRARIES+= libbinder
	
## All Binder executables must link with this static library.
LIB_FILES+= \
	$(OUT_LIBRARIES)/libbinder_glue.a

include $(BUILD_SYSTEM)/base_rules.make

$(full_target): $(all_objects) $(all_libraries)
	$(transform-o-to-executable)
