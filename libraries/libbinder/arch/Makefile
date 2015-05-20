
# Look at CC commands to determine which implementation
# of atomics ops should be used

ifneq (,$(findstring arm,$(shell $(HOST_CC) -dumpmachine)))
  bootstrap_arch_name := ARM
else
  ifneq (,$(findstring 86,$(shell $(HOST_CC) -dumpmachine)))
    bootstrap_arch_name := x86
  else
    bootstrap_arch_name := generic
  endif
endif

ifneq (,$(findstring arm,$(shell $(CC) -dumpmachine)))
  arch_name := ARM
else
  ifneq (,$(findstring 86,$(shell $(CC) -dumpmachine)))
    arch_name := x86
  else
    arch_name := generic
  endif
endif

archBootstrapSources:= \
	arch/$(bootstrap_arch_name)/atomic.c

archSources:= \
	arch/$(arch_name)/atomic.c

