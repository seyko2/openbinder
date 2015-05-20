###########################################################
## Standard rules for building a Binder package, consisting
## of a manifest of components and shared library that
## implements those components.
##
## Additional inputs from base_rules.make:
## PACKAGE_NAMESPACE: The namespace of the package, i.e. org.openbinder.tools
## PACKAGE_LEAF: The leaf name of the package, i.e. BinderShell
## MANIFEST: (Optional) Name of source manifest file.
##
## TARGET, TARGET_PATH, TARGET_SUFFIX, and TARGET_NAME will
## be set for you.
###########################################################

full_package := $(patsubst .%.,%,$(PACKAGE_NAMESPACE).$(PACKAGE_LEAF))

ifeq ($(strip $(TARGET_SUFFIX)),)
TARGET_SUFFIX :=
endif
ifeq ($(strip $(TARGET)),)
TARGET := $(full_package)
endif
ifeq ($(strip $(TARGET_PATH)),)
TARGET_PATH := $(OUT_PACKAGES)
endif
ifeq ($(strip $(TARGET_NAME)),)
TARGET_NAME := $(full_package)
endif
ifeq ($(strip $(MANIFEST)),)
MANIFEST := Manifest.xml
endif

## All Binder packages must link with these static libraries.
SHARED_LIBRARIES+= libbinder
LIB_FILES+= \
	$(OUT_LIBRARIES)/libbinder_component_glue.a

#	$(OUT_LIBRARIES)/libbinder_glue.a \

include $(BUILD_SYSTEM)/base_rules.make

# Be a little tricky here: the final target for this
# package is the package -directory-, which is dependent
# on all of the things inside that package -- currently
# the shared library and manifest file.

full_manifest:= $(full_target)/Manifest.xml

ifeq ($(strip $(STRING_FILES)),)
string_files := $(wildcard $(TOPDIR)$(BASE_PATH)/Strings.xml)
else
string_files := $(addprefix, $(TOPDIR)$(BASE_PATH)/$(STRING_FILES))
string
endif

# This is a little bogus -- we drive the makestrings depenedency
# off the base resources directory.
ifneq ($(strip $(string_files)),)
string_target := $(full_target)/resources
$(string_target): $(string_files) $(MAKESTRINGS)
	$(transform-strings-to-package)
else
string_target :=
endif

$(full_manifest): $(TOPDIR)$(BASE_PATH)/$(MANIFEST)
	@mkdir -p $(dir $@)
	@cp -f $^ $@

$(full_target): $(full_manifest) $(string_target)
	@touch $@
	
shell_sources := $(filter %.bsh,$(SRC_FILES))
shell_objects := $(addprefix $(full_target)/,$(shell_sources))
ALL_INTERMEDIATES += $(shell_objects)

ifneq ($(strip $(shell_sources)),)
$(full_target): $(shell_objects)
$(shell_objects): $(full_target)/%: $(BASE_PATH)/%
	$(transform-variables)
$(shell_objects): REPLACE_VARS:= OUT_PACKAGES OUT_SCRIPTS
endif

ifneq ($(strip $(all_objects)),)
$(full_target): $(full_target)/$(PACKAGE_LEAF).so
$(full_target)/$(PACKAGE_LEAF).so: $(all_objects) $(all_libraries)
	$(transform-o-to-package)
endif
