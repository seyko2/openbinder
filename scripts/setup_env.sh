# This POSIX shell scripts sets up environment
# variables needed by the Binder runtime to find
# its packages and executables (if it hasn't been
# installed in its standard location).

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH${LD_LIBRARY_PATH:+:}{{OUT_LIBRARIES}}
export PATH=$PATH:{{OUT_HOST_EXECUTABLES}}
export BINDER_PACKAGE_PATH={{OUT_PACKAGES}}
export BINDER_DIST_PATH={{TOP}}

# This command enables the debugging wrapper for
# processes started with SContext::New().
function enable_process_wrapper
{
	export BINDER_PROCESS_WRAPPER={{OUT_SCRIPTS}}/process_wrapper.sh
}

# Disable the process debugging wrapper.
function disable_process_wrapper {
	unset BINDER_PROCESS_WRAPPER
}
