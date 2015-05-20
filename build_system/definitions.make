# These are variables we use to collect overall lists
# of things being processed.

# Full paths to all targets that can be built.
ALL_TARGETS:=

# Full path to all intermediate files created to
# build the above targets.
ALL_INTERMEDIATES:=

# A list of all C++ files generated from IDL files, so we
# can run pidgen to generate these before compiling any
# other C++ files (since we don't know what generated headers
# a C++ file may need).
ALL_INTERFACES:=

# These are short-hand names that are defined for
# explicit goals.
ALL_TARGET_NAMES:=

ifeq ($(CC),$(HOST_CC))
  HostTxt :=
  TargetTxt :=
else
  HostTxt := "Host "
  TargetTxt := "Target "
endif

###########################################################
## Retrieve the directory of the current makefile
###########################################################

# Figure out where we are.
define my-dir
$(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))
endef

###########################################################
## Retrieve a list of all makefiles immediately below some directory
###########################################################

define all-makefiles-under
$(wildcard $(1)/*/Makefile)
endef

###########################################################
## Retrieve a list of all makefiles immediately below your directory
###########################################################

define all-subdir-makefiles
$(call all-makefiles-under,$(call my-dir))
endef

###########################################################
## Function we can evaluate to introduce a dynamic dependency
###########################################################

define add-dependency
$(1): $(2)
endef

###########################################################
## Convert "$(OUT_LIBRARIES)/libXXX.so" to "-lXXX"
###########################################################

define normalize-libraries
$(foreach file,$(1),$(if $(filter \
	$(OUT_LIBRARIES)/lib%.so,$(file)),-l$(patsubst $(OUT_LIBRARIES)/lib%.so,%,$(file)),$(file)))
endef

###########################################################
## Commands for using sed to replace given variable values
###########################################################

define transform-variables-inner
sed $(foreach var,$(REPLACE_VARS),-e "s/{{$(var)}}/$(subst /,\/,$(PWD)/$($(var)))/g") $< >$@
@if [ "$(suffix $@)" = ".sh" ]; then chmod a+rx $@; fi
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-variables
@mkdir -p $(dir $@)
@echo "Sed: $(if $(WHO_AM_I),$(WHO_AM_I),$@) <= $<"
@$(transform-variables-inner)
endef

else

define transform-variables
@mkdir -p $(dir $@)
$(transform-variables-inner)
endef

endif

###########################################################
## Commands for running pidgen
###########################################################

define transform-idl-to-cpp-inner
$(PIDGEN) \
	$(foreach header,$(GLOBAL_IDL_INCLUDES),-I $(header)) \
	$(foreach header,$(IDL_INCLUDES),-I $(header)) \
	$(foreach header,$(LOCAL_HACK_IDL_INCLUDES),-I $(header)) \
	$(foreach header,$(TARGET_IDL_INCLUDES),-I $(header)) \
	-O $(dir $@) -B $(GEN_HEADERS_DIR) \
	-S $(patsubst $(INTERMEDIATE_PATH)%,$(GEN_HEADERS_DIR)%,$(dir $@)) $<
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-idl-to-cpp
@mkdir -p $(dir $@)
@mkdir -p $(patsubst $(INTERMEDIATE_PATH)%,$(GEN_HEADERS_DIR)%,$(dir $@))
@echo "IDL: $(WHO_AM_I) <= $<"
@$(transform-idl-to-cpp-inner)
endef

else

define transform-idl-to-cpp
@mkdir -p $(dir $@)
@mkdir -p $(patsubst $(INTERMEDIATE_PATH)%,$(GEN_HEADERS_DIR)%,$(dir $@))
$(transform-idl-to-cpp-inner)
endef

endif

###########################################################
## Commands for running makestrings
###########################################################

define transform-strings-to-package-inner
$(MAKESTRINGS) -O $@ $<
@touch $@
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-strings-to-package
@mkdir -p $@
@echo "Strings: $(WHO_AM_I) <= $<"
@$(transform-strings-to-package-inner)
endef

else

define transform-strings-to-package
@mkdir -p $@
$(transform-strings-to-package-inner)
endef

endif

###########################################################
## Commands for running lex
###########################################################

define transform-l-to-cpp-inner
$(LEX) -o $@ $<
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-l-to-cpp
@mkdir -p $(dir $@)
@echo "Lex: $(WHO_AM_I) <= $<"
@$(transform-l-to-cpp-inner)
endef

else

define transform-l-to-cpp
@mkdir -p $(dir $@)
$(transform-l-to-cpp-inner)
endef

endif

###########################################################
## Commands for running yacc
###########################################################

define transform-y-to-cpp-inner
$(YACC) -o $@ $<
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-y-to-cpp
@mkdir -p $(dir $@)
@echo "Yacc: $(WHO_AM_I) <= $<"
@$(transform-y-to-cpp-inner)
@mv $(@:.cpp=.hpp) $(@:.cpp=.h)
endef

else

define transform-l-to-cpp
@mkdir -p $(dir $@)
$(transform-y-to-cpp-inner)
@mv $(@:.cpp=.hpp) $(@:.cpp=.h)
endef

endif

###########################################################
## Commands for running gcc to compile a C++ file
###########################################################

define transform-cpp-to-o-inner
$(CXX) \
	$(foreach header,$(C_SYSTEM_INCLUDES),-I $(header)) \
	$(foreach header,$(C_INCLUDES),-I $(header)) \
	$(foreach header,$(LOCAL_HACK_C_INCLUDES),-I $(header)) \
	$(foreach header,$(TARGET_C_INCLUDES),-I $(header)) \
	-c $(GLOBAL_CFLAGS) $(CFLAGS) $(CXXFLAGS) $(LOCAL_HACK_CFLAGS) $(TARGET_CFLAGS) \
	-MD -o $@ $<
@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
		-e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	rm -f $(@:%.o=%.d)
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-cpp-to-o
@mkdir -p $(dir $@)
@echo "$(TargetTxt)C++: $(WHO_AM_I) <= $<"
@$(transform-cpp-to-o-inner)
endef

else

define transform-cpp-to-o
@mkdir -p $(dir $@)
$(transform-cpp-to-o-inner)
endef

endif

###########################################################
## Commands for running gcc to compile a C file
###########################################################

define transform-c-to-o-inner
$(CC) \
	$(foreach header,$(C_SYSTEM_INCLUDES),-I $(header)) \
	$(foreach header,$(C_INCLUDES),-I $(header)) \
	$(foreach header,$(LOCAL_HACK_C_INCLUDES),-I $(header)) \
	$(foreach header,$(TARGET_C_INCLUDES),-I $(header)) \
	-c $(GLOBAL_CFLAGS) $(CFLAGS) $(CXXFLAGS) $(LOCAL_HACK_CFLAGS) $(TARGET_CFLAGS) \
	-MD -o $@ $<
@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
		-e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	rm -f $(@:%.o=%.d)
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-c-to-o
@mkdir -p $(dir $@)
@echo "$(TargetTxt)C: $(WHO_AM_I) <= $<"
@$(transform-c-to-o-inner)
endef

else

define transform-c-to-o
@mkdir -p $(dir $@)
$(transform-c-to-o-inner)
endef

endif

###########################################################
## Commands for running gcc to compile a host C++ file
###########################################################

define transform-host-cpp-to-o-inner
$(HOST_CXX) \
	$(foreach header,$(C_SYSTEM_INCLUDES),-I $(header)) \
	$(foreach header,$(C_INCLUDES),-I $(header)) \
	$(foreach header,$(LOCAL_HACK_C_INCLUDES),-I $(header)) \
	$(foreach header,$(TARGET_C_INCLUDES),-I $(header)) \
	-c $(GLOBAL_CFLAGS) $(CFLAGS) $(CXXFLAGS) $(LOCAL_HACK_CFLAGS) $(TARGET_CFLAGS) \
	-MD -o $@ $<
@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
		-e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	rm -f $(@:%.o=%.d)
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-host-cpp-to-o
@mkdir -p $(dir $@)
@echo "$(HostTxt)C++: $(WHO_AM_I) <= $<"
@$(transform-host-cpp-to-o-inner)
endef

else

define transform-host-cpp-to-o
@mkdir -p $(dir $@)
$(transform-host-cpp-to-o-inner)
endef

endif

###########################################################
## Commands for running gcc to compile a C file
###########################################################

define transform-host-c-to-o-inner
$(HOST_CC) \
	$(foreach header,$(C_SYSTEM_INCLUDES),-I $(header)) \
	$(foreach header,$(C_INCLUDES),-I $(header)) \
	$(foreach header,$(LOCAL_HACK_C_INCLUDES),-I $(header)) \
	$(foreach header,$(TARGET_C_INCLUDES),-I $(header)) \
	-c $(GLOBAL_CFLAGS) $(CFLAGS) $(CXXFLAGS) $(LOCAL_HACK_CFLAGS) $(TARGET_CFLAGS) \
	-MD -o $@ $<
@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
		-e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	rm -f $(@:%.o=%.d)
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-host-c-to-o
@mkdir -p $(dir $@)
@echo "$(HostTxt)C: $(WHO_AM_I) <= $<"
@$(transform-host-c-to-o-inner)
endef

else

define transform-host-c-to-o
@mkdir -p $(dir $@)
$(transform-host-c-to-o-inner)
endef

endif

###########################################################
## Commands for running ar
###########################################################

define transform-o-to-static-lib-inner
$(AR) $(GLOBAL_ARFLAGS) $(ARFLAGS) $(LOCAL_HACK_ARFLAGS) $(TARGET_ARFLAGS) $@ $^
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-o-to-static-lib
@mkdir -p $(dir $@)
@echo "$(TargetTxt)StaticLib: $(WHO_AM_I) ($@)"
@$(transform-o-to-static-lib-inner)
endef

else

define transform-o-to-static-lib
@mkdir -p $(dir $@)
$(transform-o-to-static-lib-inner)
endef

endif

###########################################################
## Commands for running host ar
###########################################################

define transform-host-o-to-static-lib-inner
$(HOST_AR) $(GLOBAL_ARFLAGS) $(ARFLAGS) $(LOCAL_HACK_ARFLAGS) $(TARGET_ARFLAGS) $@ $^
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-host-o-to-static-lib
@mkdir -p $(dir $@)
@echo "$(HostTxt)StaticLib: $(WHO_AM_I) ($@)"
@$(transform-host-o-to-static-lib-inner)
endef

else

define transform-host-o-to-static-lib
@mkdir -p $(dir $@)
$(transform-host-o-to-static-lib-inner)
endef

endif

###########################################################
## Commands for running gcc to link a shared library or package
###########################################################

define transform-o-to-shared-lib-inner
$(CC) -shared -Wl-soname,$(notdir $@) $(GLOBAL_LDFLAGS) $(LDFLAGS) $(LOCAL_HACK_LDFLAGS) $(TARGET_LDFLAGS) \
	-o $@ $(call normalize-libraries,$^) \
	$(GLOBAL_LDLIBS) $(LOADLIBES) $(LDLIBS) $(LOCAL_HACK_LDLIBS) $(TARGET_LDLIBS)
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-o-to-shared-lib
@mkdir -p $(dir $@)
@echo "$(TargetTxt)SharedLib: $(WHO_AM_I) ($@)"
@$(transform-o-to-shared-lib-inner)
endef

define transform-o-to-package
@mkdir -p $(dir $@)
@echo "$(TargetTxt)Package: $(WHO_AM_I) ($@)"
@$(transform-o-to-shared-lib-inner)
endef

else

define transform-o-to-shared-lib
@mkdir -p $(dir $@)
$(transform-o-to-shared-lib-inner)
endef

define transform-o-to-package
@mkdir -p $(dir $@)
$(transform-o-to-shared-lib-inner)
endef

endif

###########################################################
## Commands for running gcc to link an executable
###########################################################

define transform-o-to-executable-inner
$(CC) $(GLOBAL_LDFLAGS) $(LDFLAGS) $(LOCAL_HACK_LDFLAGS) $(TARGET_LDFLAGS) \
	-o $@ $(call normalize-libraries,$^) \
	$(GLOBAL_LDLIBS) $(LOADLIBES) $(LDLIBS) $(LOCAL_HACK_LDLIBS) $(TARGET_LDLIBS)
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-o-to-executable
@mkdir -p $(dir $@)
@echo "$(TargetTxt)Executable: $(WHO_AM_I) ($@)"
@$(transform-o-to-executable-inner)
endef

else

define transform-o-to-executable
@mkdir -p $(dir $@)
$(transform-o-to-executable-inner)
endef

endif

###########################################################
## Commands for running gcc to link a host executable
###########################################################

define transform-host-o-to-executable-inner
$(HOST_CC) $(GLOBAL_LDFLAGS) $(LDFLAGS) $(LOCAL_HACK_LDFLAGS) $(TARGET_LDFLAGS) \
	-o $@ $(call normalize-libraries,$^) \
	$(GLOBAL_LDLIBS) $(LOADLIBES) $(LDLIBS) $(LOCAL_HACK_LDLIBS) $(TARGET_LDLIBS)
endef

ifeq ($(strip $(SHOW_COMMANDS)),)

define transform-host-o-to-executable
@mkdir -p $(dir $@)
@echo "$(HostTxt)Executable: $(WHO_AM_I) ($@)"
@$(transform-host-o-to-executable-inner)
endef

else

define transform-host-o-to-executable
@mkdir -p $(dir $@)
$(transform-host-o-to-executable-inner)
endef

endif

# broken:
#	$(foreach file,$^,$(if $(findstring,.a,$(suffix $file)),-l$(file),$(file)))

###########################################################
## Misc notes
###########################################################

#DEPDIR = .deps
#df = $(DEPDIR)/$(*F)

#SRCS = foo.c bar.c ...

#%.o : %.c
#	@$(MAKEDEPEND); \
#	  cp $(df).d $(df).P; \
#	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
#	      -e '/^$$/ d' -e 's/$$/ :/' < $(df).d >> $(df).P; \
#	  rm -f $(df).d
#	$(COMPILE.c) -o $@ $<

#-include $(SRCS:%.c=$(DEPDIR)/%.P)


#%.o : %.c
#	$(COMPILE.c) -MD -o $@ $<
#	@cp $*.d $*.P; \
#	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
#	      -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
#	  rm -f $*.d
