###########################################################
## Common instructions for a generic target.
###########################################################

ifeq ($(strip $(TARGET_PATH)),)
TARGET_PATH := $(OUT)/$(BASE_PATH)
endif

full_target := $(TARGET_PATH)/$(TARGET)$(TARGET_SUFFIX)

ifeq ($(strip $(TARGET_NAME)),)
TARGET_NAME := $(notdir $(full_target))
endif

base_intermediates:= $(OUT_INTERMEDIATES)/$(BASE_PATH)
intermediates:= $(base_intermediates)/$(TARGET)_intermediates

###########################################################
## PIDGEN: Compile .idl files to .cpp and then to .o.
## These are public IDL files that are exported from the target.
###########################################################

pidgen_sources := $(filter %.idl,$(IDL_FILES))
pidgen_cpps := $(addprefix $(intermediates)/,$(pidgen_sources:.idl=.cpp))
#pidgen_headers := $(pidgen_cpps:.cpp=.h)
pidgen_objects := $(pidgen_cpps:.cpp=.o)

ifneq ($(strip $(pidgen_cpps)),)
$(pidgen_cpps): $(intermediates)/%.cpp: $(SRC_INTERFACES)/%.idl
	$(transform-idl-to-cpp)
$(pidgen_cpps): $(PIDGEN_DEP)
$(pidgen_cpps): INTERMEDIATE_PATH:= $(patsubst ./%,%,$(intermediates))

$(pidgen_cpps) : WHO_AM_I:= $(TARGET_NAME)
$(pidgen_cpps) : GEN_HEADERS_DIR:= $(OUT_HEADERS)

$(pidgen_objects): $(intermediates)/%.o: $(intermediates)/%.cpp
ifeq ($(HOST),1)
	$(transform-host-cpp-to-o)
else
	$(transform-cpp-to-o)
endif
endif

# This is a target we can depend on to ensure we run pidgen
# before trying to compile cpps.
interfaces:: $(pidgen_cpps)

# FYI -- all the interfaces we have found.
ALL_INTERFACES+= $(pidgen_sources)

###########################################################
## PIDGEN: Compile .idl files to .cpp and then to .o.
## These are IDL files that are private to the target.
###########################################################

pidgen_local_sources := $(filter %.idl,$(SRC_FILES))
pidgen_local_cpps := $(addprefix $(intermediates)/,$(pidgen_local_sources:.idl=.cpp))
#pidgen_local_headers := $(pidgen_local_cpps:.cpp=.h)
pidgen_local_objects := $(pidgen_local_cpps:.cpp=.o)

ifneq ($(strip $(pidgen_local_cpps)),)
$(pidgen_local_cpps): $(intermediates)/%.cpp: $(TOPDIR)$(BASE_PATH)/%.idl
	$(transform-idl-to-cpp)
$(pidgen_local_cpps): $(PIDGEN_DEP)
$(pidgen_local_cpps): INTERMEDIATE_PATH:= $(patsubst ./%,%,$(intermediates))

$(pidgen_local_cpps) : WHO_AM_I:= $(TARGET_NAME)
$(pidgen_local_cpps) : GEN_HEADERS_DIR:= $(base_intermediates)

$(pidgen_local_objects): $(intermediates)/%.o: $(intermediates)/%.cpp
ifeq ($(HOST),1)
	$(transform-host-cpp-to-o)
else
	$(transform-cpp-to-o)
endif
endif

# This is a target we can depend on to ensure we run pidgen
# before trying to compile cpps.
interfaces:: $(pidgen_local_cpps)

# FYI -- all the interfaces we have found.
ALL_INTERFACES+= $(pidgen_local_sources)

###########################################################
## LEX: Compile .l files to .cpp and then to .o.
###########################################################

lex_sources := $(filter %.l,$(SRC_FILES))
lex_cpps := $(addprefix $(intermediates)/,$(lex_sources:.l=.cpp))
lex_objects := $(lex_cpps:.cpp=.o)

ifneq ($(strip $(lex_cpps)),)
$(lex_cpps): $(intermediates)/%.cpp: $(TOPDIR)$(BASE_PATH)/%.l
	$(transform-l-to-cpp)

$(lex_objects): $(intermediates)/%.o: $(intermediates)/%.cpp
ifeq ($(HOST),1)
	$(transform-host-cpp-to-o)
else
	$(transform-cpp-to-o)
endif
endif

###########################################################
## YACC: Compile .y files to .cpp and the to .o.
###########################################################

yacc_sources := $(filter %.y,$(SRC_FILES))
yacc_cpps := $(addprefix $(intermediates)/,$(yacc_sources:.y=.cpp))
yacc_headers := $(yacc_cpps:.cpp=.h)
yacc_objects := $(yacc_cpps:.cpp=.o)

ifneq ($(strip $(yacc_cpps)),)
$(yacc_cpps): $(intermediates)/%.cpp: $(TOPDIR)$(BASE_PATH)/%.y $(lex_cpps)
	$(transform-y-to-cpp)
$(yacc_headers) : $(intermediates)/%.h : $(intermediates)/%.cpp

$(yacc_objects): $(intermediates)/%.o: $(intermediates)/%.cpp
ifeq ($(HOST),1)
	$(transform-host-cpp-to-o)
else
	$(transform-cpp-to-o)
endif
endif

###########################################################
## C++: Compile .cpp files to .o.
###########################################################

cpp_sources := $(filter %.cpp,$(SRC_FILES))
cpp_objects := $(addprefix $(intermediates)/,$(cpp_sources:.cpp=.o))

ifneq ($(strip $(cpp_objects)),)
$(cpp_objects): $(intermediates)/%.o: $(TOPDIR)$(BASE_PATH)/%.cpp $(yacc_cpps)
ifeq ($(HOST),1)
	$(transform-host-cpp-to-o)
else
	$(transform-cpp-to-o)
endif
-include $(cpp_objects:%.o=%.P)
endif

# Unless told otherwise, make sure all interfaces are
# built by pidgen before trying to compile the cpps.
ifeq ($(strip $(NO_INTERFACE_DEPENDENCY)),)
$(cpp_objects): | interfaces
endif

###########################################################
## C: Compile .c files to .o.
###########################################################

c_sources := $(filter %.c,$(SRC_FILES))
c_objects := $(addprefix $(intermediates)/,$(c_sources:.c=.o))

ifneq ($(strip $(c_objects)),)
$(c_objects): $(intermediates)/%.o: $(TOPDIR)$(BASE_PATH)/%.c $(yacc_cpps)
ifeq ($(HOST),1)
	$(transform-host-c-to-o)
else
	$(transform-c-to-o)
endif
-include $(c_objects:%.o=%.P)
endif

###########################################################
## Common object handling.
###########################################################

all_objects := \
	$(cpp_objects) \
	$(c_objects) \
	$(yacc_objects) \
	$(lex_objects) \
	$(pidgen_objects) \
	$(pidgen_local_objects)

ALL_INTERMEDIATES += \
	$(all_objects) \
	$(lex_cpps) \
	$(yacc_cpps) \
	$(yacc_headers) \
	$(pidgen_cpps) \
	$(pidgen_local_cpps)

LOCAL_C_INCLUDES += $(TOPDIR)$(BASE_PATH) $(intermediates) $(base_intermediates)

###########################################################
## Common library handling.
###########################################################

all_libraries := \
	$(LIB_FILES) \
	$(addprefix $(OUT_LIBRARIES)/,$(addsuffix .so,$(SHARED_LIBRARIES)))

###########################################################
## Common definitions for target.
###########################################################

# Propagate local configuration options to this target.
$(full_target) : LOCAL_HACK_CFLAGS:= $(LOCAL_CFLAGS)
$(full_target) : LOCAL_HACK_C_INCLUDES:= $(LOCAL_C_INCLUDES)
$(full_target) : LOCAL_HACK_LDFLAGS:= $(LOCAL_LDFLAGS)
$(full_target) : LOCAL_HACK_LDLIBS:= $(LOCAL_LDLIBS)
$(full_target) : LOCAL_HACK_ARFLAGS:= $(LOCAL_ARFLAGS)

# Tell the target and all of its sub-targets who it is.
$(full_target) : WHO_AM_I:= $(TARGET_NAME)

# Provide a short-hand for building this target.
.PHONY: $(TARGET_NAME)
$(TARGET_NAME):: $(full_target)

ifeq ($(strip $(DONT_BUILD_TARGET)),)
ALL_TARGETS += $(full_target)
endif
ALL_TARGET_NAMES += $(TARGET_NAME)
