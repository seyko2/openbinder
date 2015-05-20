#!/bin/sh

# This is the command running inside the xterm of our
# debug wrapper.  It needs to take care of starting
# the binderproc server, so it can attach to the parent
# process.  In addition, here we run binderproc inside
# of a gdb session to allow for debugging.

# On some systems, running xterm will cause LD_LIBRARY_PATH
# to be cleared, so restore it and PATH to be safe.
export PATH=$PREV_PATH
export LD_LIBRARY_PATH=$PREV_LD_LIBRARY_PATH
#source {{OUT_SCRIPTS}}/setup_env.sh

# Start binderproc (or whatever sub-command is being run)
# inside of gdb, giving gdb an initial command script to
# automatically run the process without user intervention.
gdb -q -x {{OUT_SCRIPTS}}/process_wrapper_gdb.cmds $1
