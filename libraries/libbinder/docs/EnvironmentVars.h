/*!
@page EnvironmentVars Environment Variables

<div class="header">
<center>\< @ref BinderThreading | @ref OpenBinder | @ref SupportKit \></center>
<hr>
</div>

The following are environment variables supported by the main
Binder system &mdash; that is, libbinder, and thus everything that
links against it.

-# @ref EnvVarPaths
  -# @ref BINDER_PACKAGE_PATH
-# @ref EnvVarBehavior
  -# @ref TEXT_OUTPUT_FORMAT
  -# @ref BINDER_NO_CONTEXT_SECURITY
  -# @ref BINDER_SHOW_LOAD_UNLOAD
  -# @ref BINDER_SINGLE_PROCESS
-# @ref EnvVarDebugging
  -# @ref ATOM_DEBUG
  -# @ref BINDER_IPC_PROFILE
  -# @ref BINDER_PROCESS_WRAPPER
  -# @ref LOCK_DEBUG
  -# @ref VECTOR_PROFILE
   
@section EnvVarPaths Paths

These variables allow you to control where the Binder finds various
parts of itself.

@anchor BINDER_PACKAGE_PATH
@subsection BINDER_PACKAGE_PATH

This is a ":"&mdash;separated list of directories where the
@ref PackageManagerDefn should find packages.  This variable @em must be set when
running @ref smooved, so that when it starts the package manager it will be
able to find the Binder Shell component and other services it needs.

For example:

@verbatim
export BINDER_PACKAGE_PATH=~/openbinder/build/packages
@endverbatim

@section EnvVarBehavior Behavior

These variables control how the Binder system behaves and displays
messages.

@anchor TEXT_OUTPUT_FORMAT
@subsection TEXT_OUTPUT_FORMAT

This variable allows you to control formatting of text sent to the
standard output streams (bout, berr, etc).  It contains a series
of characters describing which formatting options to enable.  Each
character can be prefixed with a '!', indicating that option should
be disbled.  The available options are:

- @b m: Multi-threaded output.  The text stream will perform buffering
  of each thread writing into it, so that threads only write complete
  lines.  This avoids the mixing together of partial lines when two
  threads are writing at the same time.
- @b c: Colored output.  Each line of output is colored by the thread
  that produced it.  The color is selected based on the thread ID.
- @b p: Add a prefix to the front of each line with the id of the
  process it came from.
- @b t: Add a prefix to the front of each line with the id of the
  thread it came from.
- @b w: Add a prefix to the front of each line with the timestamp
  when it was written.

For example, to enable multi-threaded colored output with a prefix
showing the process but not the thread, you can do this:

@verbatim
export TEXT_OUTPUT_FORMAT='mcp!t'
@endverbatim

@anchor BINDER_NO_CONTEXT_SECURITY
@subsection BINDER_NO_CONTEXT_SECURITY

Setting this variable to a non-zero value disables the simple context
permissions in @ref smooved, so that any process can get access to any
context.  By default, processes only get access to the user context.

@anchor BINDER_SHOW_LOAD_UNLOAD
@subsection BINDER_SHOW_LOAD_UNLOAD

When set to a non-zero integer, the Binder will print out informative
messages as it loads and unloads package code.

@anchor BINDER_SINGLE_PROCESS
@subsection BINDER_SINGLE_PROCESS

When set to a non-zero integer, the Binder will attempt to keep code
running in th same process, as much as possible.  Currently this means
that calling SContext::NewProcess() will return the local IProcess,
unless the flag SContext::REQUIRE_REMOTE is used.

See @ref BinderProcessModel for more information on how the Binder
manages processes.

It is important to understand the distinction between using
<tt>BINDER_SINGLE_PROCESS=1</tt> with the Binder kernel module available,
and not having the kernel module at all:

@subsubsection WithKernelModule With Kernel Module

When the kernel module is available, even if you set
<tt>BINDER_SINGLE_PROCESS=1</tt>, the Binder runtime will still
open the module and use it for various things.  For example, it
will still be used to manage the thread pool and schedule message
delivery to SHandler objects.

In addition, @ref smooved will still set itself as the SContext host.
This means that you can still create other processes (such as with
the bsh command), which can connect back to the root context and
do IPC with other objects.

@subsubsection WithoutKernelModule Without Kernel Module

When the kernel module is not available, there is no IPC available
at all.  For example, trying to run the bsh command will result in
an error saying it could not find the root context, just as if
smooved hadn't been running at all.

A more subtle distinction is that in this case the thread pool
can't use the kernel module to schedule SHandler objects.  Instead,
it uses a different purely user-space implementation.  This
can lead to subtle differences in behavior between the two environments.
(Assuming there aren't outright bugs in one implementation or
the other.)

@section EnvVarDebugging Debugging

These variables allow you to enable and control various debugging
facilities provided by the Binder.

@anchor ATOM_DEBUG
@subsection ATOM_DEBUG

Atom debugging is a tool for tracking SAtom reference count leaks.
See @ref SAtomDebugging for more information.

@anchor BINDER_IPC_PROFILE
@subsection BINDER_IPC_PROFILE

Setting this variable to 1 will enable a profiler of the Binder's IPC
mechanism, designed to help client programs discover "hot spots"
where they are doing too much IPC.

In each process, the profiler makes a note of each outgoing IPC operation
from that process and the stack crawl associated with it.  After every X
number of IPCs have happened, it prints a report of the total number of
IPCs and the top N instigators of IPCs organized by stack crawl.

When the IPC profiler is enabled, you can use these additional environment
variables to customize its behavior:

- @b BINDER_IPC_PROFILE_DUMP_PERIOD is the number of IPC operations
	between each time the profiler statistics are displayed.  The
	default value is 500.
- @b BINDER_IPC_PROFILE_MAX_ITEMS is the maximum number of unique
	stack crawls display &mdash; i.e., the top N callers to display.  The
	default value is 10.
- @b BINDER_IPC_PROFILE_STACK_DEPTH is the maximum depth that stack
	crawls can be.  This is useful to tune what statistics are being
	displayed.  For example, a smaller number will result in fewer
	bins because there are fewer possible paths into the IPC mechanism.
- @b BINDER_IPC_PROFILE_SYMBOLS if non-zero, the default, will display
	symbol information in the stack crawl, making it much more verbose
	but easier to understand.

@anchor BINDER_PROCESS_WRAPPER
@subsection BINDER_PROCESS_WRAPPER

The variable supplies a "wrapper" command to run between caller
of SContext::NewProcess() and the actual process it returns.  This
can be useful to insert xterm and/or gdb sessions into the process,
for separating the output of multiple processes and easily catching
crashes.

Using this variable usually requires creating a set of scripts and
files to accomplish what you want in the wrapper process.  The
OpenBinder build system generates some scripts under build/scripts
that create an xterm with gdb running inside of it.  You can set
@c BINDER_PROCESS_WRAPPER as follows to use these scripts:

@verbatim
export BINDER_PROCESS_WRAPPER=~/openbinder/build/scripts/process_wrapper.sh
@endverbatim

@anchor LOCK_DEBUG
@subsection LOCK_DEBUG

These variables control SLocker's debugging / deadlock detection code.  This
is enabled by default in debug builds.  You can explicitly turn it off by
setting @c LOCK_DEBUG=0.  The @c LOCK_DEBUG_STACK_CRAWLS can likewise be set to 0
to not collect stack information as the deadlock detection runs &mdash; this can
allow you to run deadlock detection with less performance impact.

@anchor VECTOR_PROFILE
@subsection VECTOR_PROFILE

The @c VECTOR_PROFILE tools collects usage statistics on the SVector class.
When enabled, it will print regular reports of the kinds of SVector
operations that are being performed.  The variable @c VECTOR_PROFILE_DUMP_PERIOD
controls how often reports are printed; its default value is 1000.
*/
