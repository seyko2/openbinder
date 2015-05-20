###########################################################
## Standard rules for building a shared library that uses
## the Binder runtime services.
##
## Additional inputs from shared_library.make:
## None.
###########################################################

SHARED_LIBRARIES+= libbinder

## All Binder libraries must link with this static library.
LIB_FILES+= \
	$(OUT_LIBRARIES)/libbinder_glue.a

include $(BUILD_SYSTEM)/shared_library.make
