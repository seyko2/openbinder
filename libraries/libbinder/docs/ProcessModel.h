/*!
@page BinderProcessModel Binder Process Model

<div class="header">
<center>\< @ref BinderTerminology | @ref BinderKit | @ref BinderIPCMechanism \></center>
<hr>
</div>

One could argue that you can't really understand what the Binder is 
about until you understand how it deals with processes.    
Conceptually, the Binder deals with processes in a very different way 
than you are probably used to thinking about them.  This approach to 
processes is, in a way, its purest expression of the system-level 
component model it provides.

The process model described here is made possible by the underlying
@subpage BinderIPCMechanism features that allow operations
remote objects to behave like local functional calls.

This IPC model is important to the Binder because it provides a
high degree of process transparency.  That is, developers for the
most part don't have to worry about which process a particular
@ref BinderObjectDefn exists in, but can simply treat any instance
as if it is in the local process.  This allows for a lot of
flexibility and scalability of systems built on top of the Binder,
in that we can decided dynamically at run-time how they are to
be distributed across processes (if any).

One immediately obvious benefit of this flexibility is that the
entire Binder system is able to run without the process model
being available at all.  If the kernel driver implementing
the @ref BinderIPCMechanism is not available, it will simply run
everything in a single process.  Likewise, the @ref BINDER_SINGLE_PROCESS
environment variable can be set to have everything run in one process,
for ease of debugging.

This flexibility in process distribution leads, as described here,
to some fundamental advances in the ways that high-level programmers
can think about processes.

-# @ref ProcessBasics
-# @ref BinderProcessIssues
-# @ref CreatingUsingProcesses
-# @ref ProcessLifetime
-# @ref DynamicProcessBinding

@section ProcessBasics Process Basics

The best way to describe how the Binder manages processes is in 
contrast to what a system typically does, so first we will review the 
traditional approach seen elsewhere.

Processes are used in an operating system to protect one piece of code 
from disruption due to misbehavior of another piece of code.  What this 
typically means is that each running application is given its own 
process, so a problem in one application won't cause trouble for other 
running applications.

Thus, the act of starting an application means to the operating system, 
"create a new process for the application, load the application code 
into that process, and start a thread running in that process at the 
beginning of the application."

This model makes sense because applications are generally fairly large, 
independent entities: when one application is running, it very rarely 
needs to know or wants to care at all about what other applications are 
doing at the same time.  The process it is running in gives it its own 
isolated world, through which it can be protected from others and 
others can be protected from it.

@section BinderProcessIssues Binder Process Issues

There are many things working together to make the traditional process 
model described above actually happen:

- The kernel is responsible for creating and managing the basic 
  process mechanism.
- The runtime is responsible for loading and managing code in a 
  process.
- The shell (graphical or command line) is responsible for making the 
  user action "run an application/command" map to lower-level operation 
  of creating a process and running the associated code there.

Note that the Binder process model is designed to sit on top of 
standard kernel and runtime process support, such as fork()+exec() on 
Unix.  It does not supplant or modify those in any way, but simply uses 
them in a different way than is done by traditional shells.

@par
<em>Unix actually provides a superset of the traditional process model 
being discussed.  Calling fork() without an exec() will cause a new 
process to be created that is a complete duplicate of the current 
process.  The Binder does not preclude using this alternative model 
alongside it, however the semantics of forking a multithreaded program 
are problematic enough that it is not a process model the Binder needs 
to support for itself.  For example, on Linux a fork() does not 
duplicate any other threads in the original process, leaving parts the 
new process in a fairly questionable state.</em>

In the Binder itself, there is no concept of an "application" &mdash; an 
application is built on @e top of the core Binder services, but is not 
something the Binder itself defines.  Instead, the Binder revolves 
around smaller, more generic components.  An application is one 
specific kind of component you can create with the Binder, but you can 
just as well create components that represent controls on the screen, 
file-like data streams, a window manager, etc.  To the Binder itself, 
these are all the same thing. 

The goal, then, is to define a process model for the Binder that is 
independent of an application.  Clearly, we can't just treat every 
single component someone creates as a potential application and give it 
its own process, so some other approach must be taken.

@section CreatingUsingProcesses Creating and Using Processes

Processes in the Binder are modelled as sandboxes or containers in 
which to run components.  Instead of having some code you want to run 
and then creating a process in which to run it, the Binder's approach 
is to first create an empty process, into which you can then 
instantiate any arbitrary code components.

In fact, a process in the Binder can best be thought of as just another 
kind of component.  There is a special API you call to create it, but 
what you get back is just another Binder interface (called, not 
surprisingly, IProcess) that then allows you to interact with your new 
process.

Here is some example Binder code that will make a new process, and then 
create a new component inside of that process:

@code
sptr<IProcess> process = Context().NewProcess(
	SString("my empty process"));

sptr<IBinder> obj = Context().RemoteNew(
	SValue::String("org.openbinder.services.WindowManager"), process);
@endcode

Or the same thing from the Binder Shell:

@verbatim
/$ p=$[new_process "my empty process"]
Result: IBinder::shnd(0x2)

/$ c=$[new -r $p org.openbinder.services.WindowManager]
Result: IBinder::shnd(0x3)
@endverbatim

After running this code, you now have a Window Manager service running 
in its own process.  Note that if the service being instantiated is an 
application, the end result would be the same as with a traditional 
process model.  For example:

@code
sptr<IProcess> process = Context().NewProcess(
	SString("AddressBook"));

sptr<IBinder> obj = Context().RemoteNew(
	SValue::String("com.palmsource.apps.AddressBook"), process);
@endcode

Or:

@verbatim
/$ p=$[new_process "AddressBook"]
Result: IBinder::shnd(0x2)

/$ app=$[new -r $p com.palmsource.apps.AddressBook]
Result: IBinder::shnd(0x3)
@endverbatim

With this code we have started a process running the Address Book 
application/component.

With the Binder, however, we have a lot more flexibility in how we use 
processes.  For example, we can run a number of related components in 
the same process in order to reduce the overhead needed for them:

@code
sptr<IProcess> process = Context().NewProcess(
	SString("my empty process"));

sptr<IBinder> surface = Context().RemoteNew(
	SValue::String("org.openbinder.services.Surface"), process);

sptr<IBinder> wm = Context().RemoteNew(
	SValue::String("org.openbinder.services.WindowManager"), process);
@endcode

Or:

@verbatim
/$ p=$[new_process "my empty process"]
Result: IBinder::shnd(0x2)

/$ surface=$[new -r $p org.openbinder.services.Surface]
Result: IBinder::shnd(0x3)

/$ wm=$[new -r $p org.openbinder.services.WindowManager]
Result: IBinder::shnd(0x4)
@endverbatim


The division between processes can be done completely at run-time, 
depending on factors such as the capabilities of the hardware, the 
permissions needed by components, and third party components installed 
on the device.

@section ProcessLifetime Process Lifetime

In addition to deciding what process to spawn and how to distribute
components between them, we also need to decide when those processes
go away.

The default rule for the Binder is that a process lasts as long as
there are any @em strong references on its objects that are held
by other processes.  In the most trivial case, this means that
after creating an empty process you can make it go away by releasing
your reference on that object:

@verbatim
/$ p=$[new_process "my temporary process"]
Result: IBinder::shnd(0x4)

/$ p=
Result: ""
[SIGCHLD handler] child process 23356 exited normally with exit value 0
@endverbatim

More interesting, if you create any components in that process, then
it will continue to stay around as long as other processes are using
those components:

@verbatim
/$ p=$[new_process "my temporary process"]
Result: IBinder::shnd(0x2)

/$ c=$[new -r $p org.openbinder.tools.commands.BPerf]
Result: IBinder::shnd(0x4)

/$ p=
Result: ""

/$ c=
Result: ""
[SIGCHLD handler] child process 23359 exited normally with exit value 0
@endverbatim

This is a very powerful feature, as it allows you to easily create
temporary processes as sandboxes, which will automatically stay around
for only as long as they are needed.  An example of how you could
use this feature would be in a web browser: if you have some code
that you don't want running in the main browser process (such as a third
party extension for displaying a special kind of media), you can create
a temporary process in which it will run, and as long as others are using
that code the process will continue to exist.

Even better, your web browser can hold a weak reference on that temporary
process.  Then, whenever it has new content to display, it can try to
promote that reference and if the promotion succeeds then it can continue
using the process for creating additional instances of the component.
Otherwise, the current process has exited and it will need to make a new
process.

Sometimes, however, you want to have more control over the lifetime of
a process.  This can be accomplished with the SLooper::StopProcess()
method or @c stop_process shell command.  These ask that the process stop
as soon as all strong references on its process object go away, allowing
you to let the process go away even if others have references to some of
its components.  In addition, you can specify an option to force the
process to exit immediately, even ignoring anyone holding references on
the process object.

See @ref ShellProcessLifetime in the @ref BinderShellTutorial for more
examples of this feature.

@section DynamicProcessBinding Dynamic Process Binding

The normal way you instantiate a component is with the SContext::New() 
method:

@code
sptr<IBinder> obj = Context().New(
	SValue::String("com.vender.MyApp.Something"));
@endcode

Unlike the RemoteNew() call, New() does not specify the process the new 
component should be created in.  What this usually means is 
just that the component will always be instantiated in the local 
process.  However, because the Binder hides the details of which 
processes components live in, it is not required to use the same 
process.

A simple form of this dynamic process binding can be seen with the
@ref BINDER_SINGLE_PROCESS option.  This is a fairly brute-force
approach, which simply forces SContext::NewProcess() to not create
a new process unless the caller specifies that one is absolutely necessary.
In other words, "run as much of the system as possible in a single
process."

The current OpenBinder distribution also includes an early implementation
of more extensive process management.  This is provided through the
IProcessManager interface, and a simple implementation of that API
which currently lives within @ref smooved itself.

This implementation
allows a component to say that it would like a dedicated process for
its package, and have itself instantiated there instead of the local
process of the caller.  The example code in @c samples/ServiceProcess
demonstrates this functionality, as can be seen in the
@ref ShellDynamicProcesses section of the @ref BinderShellTutorial.

As mentioned, though, the current implementation is quite primitive
and with the underlying architecture there is are @em many more
interesting things that could be done.
For example, the Process Manager could look at the signature of the 
component's executable and permissions needed by the component itself 
and, if they are in conflict with the current process, behind the 
scenes create the component in a more appropriate process.  By doing 
so, we can enable a number of useful scenarios for dealing with 
security and other protection issues:

- The current process is very trusted (complete access to the entire 
  system), but calls New() to create a component that is not correctly 
  signed to be run in such an environment.  The Process Manager creates a 
  new untrusted process and instantiates the component there instead.

- The current process is completely untrusted, but calls New() to 
  create a component that needs access to the telephone module.  The Process
  Manager checks the signature of the new component's code and, 
  seeing that it is safe, instatiates the new component in an existing 
  process that is used for telephony-related functions.

- The current process is completely untrusted, but calls New() to 
  create a component that needs access to the telephone module.  That 
  component also specifies that it can only be used by others who also 
  are allowed access to the telephone module.  The Process Manager  sees
  that the current process is not allowed this functionality and fails the 
  instanstiation.

- The current process has the same permissions as a component being 
  instantiated.  However, that component's manifest says that it would 
  like to be run in its own process.  On a high-end device, the Process Manager 
  creates a new process for it and returns a remove object.  On a low-end device,
  it instantiates the component in the local process.

- The current process has the same permissions as a component being 
  instantiated.  However, that component's manifest says that all of the 
  components in that package should run in the same process.  There is 
  already another component from that package running in a different 
  process, so the Process Manager finds that other process and instantiates
  the new component there.  (Hey, we now do this one!  Progress!)

*/
