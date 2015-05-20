/*!
@page Building Building

<div class="header">
<center>\< @ref QuickStart | @ref OpenBinder | @ref Contents \></center>
<hr>
</div>

The OpenBinder package includes a build system that can be used for
compiling the source code, running tests, and generating the documentation.
While the basic source code has over time run on a variety of platforms
(including BeOS, Windows, and PalmOS Cobalt), this package only supports
building for Linux.

@section Requirements

The OpenBinder build system requires GNU Make 3.80.

The source code has been tested against GCC 3.3.5.  Other versions of
GCC may cause problems.  It also requires flex 2.5.31 and bison
1.875d.

The Binder kernel module has been tested against Linux kernels
2.6.10 and 2.6.12.  Earlier kernels are not supported.  The kernel
module is only required for multi-process functionality; the user-space
code will operate without it being present, though without the ability
to communicate between processes.

Building the documentation from source requires the Doxygen tool,
available at http://www.doxygen.org/.

@section QuickBuild Quick Build

Once you have extracted the OpenBinder distribution on to your
machine, you can simply type "make" from the root of the tree to
build all of the user-space code:

@verbatim
$ tar -xzf openbinder-0.8.tar.gz
$ cd openbinder-0.8
$ make
@endverbatim

This will proceed to build the
pidgen tool (the Binder's IDL compiler), compile the interfaces,
build the Binder shared library (libbinder.so), and then build
the Binder packages that are included.

Assuming there are no errors, you can use the "test" target to
run some tests in the Binder system:

@verbatim
$ make test
@endverbatim

You can also use the "runshell" target to run the Binder Shell,
an interactive command line shell that lets you interact with
the Binder system:

@verbatim
$ make runshell
...

/# ls
contexts/
packages/
processes/
services/
who_am_i

/# c=$[new org.openbinder.tools.commands.BPerf]
Result: sptr<IBinder>(0x80967d4 17BinderPerformance)

/# inspect $c
Result: "org.openbinder.app.ICommand" -> sptr<IBinder>(0x80967d4 17BinderPerformance)
@endverbatim

See @ref BinderShellTutorial for more information on the Binder Shell.
Use the "make help" target for a brief summary of the features
available in the build system.

@section MultiprocessBuild Multiprocess Build

While there are a lot of useful things you can do with the Binder in a
single process (scheduling threads with SHandler, loading components with
the Package Manager, etc), the main power of the system comes when
working with multiple processes.  Having this support requires building
the Binder kernel module, which implements a special IPC mechanism used
by the Binder to efficiently manage object references and interactions
between processes.

To build the kernel module, you will need to have built your kernel so
that its headers are available.  If that has been done, then you should
be able to use the "loadmodules" target to compile the Binder kernel
module and install it on your system:

@verbatim
$ make loadmodules
...
Loading Binder module...
You may need to enter your password
ERROR: Module binderdev does not exist in /proc/modules
sudo insmod /home/hackbod/lsrc/open-source/openbinder/main/dist/modules/binder/binderdev.ko
Binder module successfully intalled.
@endverbatim

After this has been completed, you can again run the "test" and "runshell"
targets, and now you will be running these in a multiprocess environment:

@verbatim
$ make runshell
...

/# p=$[new_process my_process]
Result: IBinder::shnd(0x2)

/# c=$[new -r $p org.openbinder.tools.commands.BPerf]
Result: IBinder::shnd(0x3)
@endverbatim

@section BuildingDocumentation Building Documentation

If you have Doxygen available, you can use the "docs" target to
build this documentation from the source code:

@verbatim
$ make docs
@endverbatim

The documentation will be generated into the directory build/docs/...

*/
