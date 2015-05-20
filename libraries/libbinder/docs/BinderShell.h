/*!
@page BinderShellTutorial Binder Shell Tutorial

<div class="header">
<center>\< @ref BinderRecipes | @ref BinderKit | @ref BinderShellData \></center>
<hr>
</div>

The Binder Shell is a POSIX-compatible shell (though there are some missing
features, such as pipes), which includes special extensions for interacting
with the Binder.  The shell serves as an example of another language that
can work with the Binder runtime, and you can even write Binder components
with the shell.

Shell commands themselves are Binder components.  That is, when executing
a command, the shell finds and instantiates a Binder component that
implements the specified command.  This allows the shell and the commands
to negotiate extensively about what process the command will run in &mdash; for
example, a command can request that it run in a specific process next to
an associated server, or the shell can allow some or all commands to run
in its own process to improve performance.

The vast majority of the Binder system is accessible through the shell,
due to the underlying scripting protocol supported by the Binder.  For
example, you can access the persistent data store, running services,
and Binder components.  Because of this, the shell provides a useful way
to introduce many of the Binder concepts.  All of the operations described
in this tutorial map directly to corresponding calls in the underlying
C++ APIs.

Note that the Binder Shell is not currently a replacement for a Linux
command shell.  In particular, it currently is not able to execute
traditional Linux commands (via fork() and exec()), nor does it provide
access to the underlying Linux filesystems.  These are not design
limitations of the shell, and we would like to add these facilities in
the future.

-# @ref AccessingBinderShell
  -# @ref ShellPrompt
-# @ref BinderNamespaceData
  -# @ref SettingsCatalog
  -# @ref BinderData
  -# @ref VariablesCommands
-# @ref BinderObjects
  -# @ref ServicesCatalog
  -# @ref ExampleInformant
  -# @ref BinderInterfaces
  -# @ref DataModelInterfaces
-# @ref ComponentsProcesses
  -# @ref BinderComponents
  -# @ref ProcessesShell
  -# @ref WorkingWithProcesses
  -# @ref ShellProcessLifetime
  -# @ref ShellDynamicProcesses
  -# @ref BinderObjectIdentity
  -# @ref Contexts
-# @ref PackageManager
  -# @ref Packages
  -# @ref PackageContents
-# @ref ShellAdvancedTopics
  -# @ref StrongWeakPointers
  -# @ref ShellLinks

@section ShellAdditionalMaterial Additional Material

- @subpage BinderShellData provides more detail on how to manipulate
  SValue data with the shell.
- @subpage BinderShellSyntax is a reference on the new syntax
  introduced by the shell.

@section AccessingBinderShell Accessing the Binder Shell

From outside of the Binder system, there are two main ways you can
access the shell:

- When starting smooved (the Binder root server), the -n or -s option
will cause smooved to dump into an interactive Binder Shell session
once it has finished with the boot process.  This is what the build system's
"make runshell" target uses.

- When smooved is already running, the "bsh" Linux command will connect
to the running smooved session and run a Binder Shell in the caller's
environment.  This is similar, for example, to using "bash" to start a
new shell session with bash.

From within the Binder system, you have a number of programmatic options:

- The method SContext::RunScript() will create a new shell that executes
the string given to it, like the traditional POSIX system() call.

- You can instantiate the Binder Shell component (org.openbinder.tools.BinderShell)
yourself and have it execute whatever input you desire.

- You can use the Binder Shell as a VM (called org.openbinder.tools.BinderShell.VM)
to write Binder components in shell.

All of the examples here will assume you have gotten into the shell
through the build system's "make runshell" command.

Once you have the shell running, the first command you will want to know
about is "help".  This command can can be used in four different ways:

- <b>help</b> by itself will display general information abiout the shell.
- <b>help commands</b> will display a list of all shell commands that are
available and the component implementing them.  This is done through a
query on the @ref PackageManagerDefn.
- <b>help \<<i>command</i>\></b> will display the documentation specifically for
@a command.  This is provided through the standard command interface
(ICommand), so the result depends on the what the command has implemented.
- <b>help examples</b> Shows some examples of things you can do with the
shell.  Note that some of these may not be relevant to the OpenBinder
environment.

@subsection ShellPrompt The Shell Prompt

When the shell is waiting for input, it will print at the begining of the line a prompt
to indicate it is doing so.  This prompt is usually one of three things,
depending on what state the shell is in:

- <b>CD/#</b> Indicates the shell is ready for the next command, and
  running on the system context.  This is the usual the state when
  using smooved &mdash; or "make runshell" &mdash; to start the shell.
- <b>CD/$</b> Indicates the shell is ready for the next command, and
  running on the user context.  This is the usual the state when using
  the "bsh" command or other mechanism to run the shell.
- <b>\></b> Indicates the shell is waiting for more text for the
  current command.

The "CD" portion is the current directory of the shell, which usually
starts out at the root of the namespace.  As such, these are the exact
shell prompts you will usually see.

@verbatim
/#
@endverbatim

@verbatim
/$
@endverbatim

@verbatim
>
@endverbatim

@section BinderNamespaceData The Binder Namespace and Data

The first command we are usually interested from a shell is "ls".  There is
such a command provided by the shell, but from its output you will start to
notice that the Binder Shell is a little different:

@verbatim
/# ls
contexts/
packages/
processes/
services/
settings/
who_am_i
@endverbatim

What you are seeing here is the root of the @ref BinderNamespaceDefn,
called a "Binder Context".  This is a global namespace of data and
services, and all of the normal shell filesystem commands work on
this namespace.  The Binder Context is your connection to the rest of
the Binder system &mdash; it gives access to the information needed to
instantiate components, find running services, etc.

@subsection SettingsCatalog /settings: The Settings Catalog

The first directory in the namespace we will look at is @c /settings,
the "settings catalog".  This is a repository of persistent data &mdash;
anything you place here will be put into persistent storage (currently
as an XML file stored on the Linux filesystem), which is read back the
next time smooved starts.

For example, we can use the @c publish command to place a piece of
data into the settings catalog:

@verbatim
/# publish /settings/vendor/foo bar
@endverbatim

And then later retrieve that data:

@verbatim
/# lookup /settings/vendor/foo
Result: "bar"
@endverbatim

If we were to now kill smooved and restart it, we would see that our
data was still there:

@verbatim
/# ls -l /settings/vendor
foo -> "bar"
@endverbatim

(Note that <tt>ls -l</tt> in the Binder Shell displays the data that is
stored at each entry listed.)

@subsection BinderData Binder Data

Let's take a step back now and talk a little more about what we were
seeing in the last examples.

All data in the Binder must be language independent &mdash; that is, it must
carry type as well as value information, have a consistent
representation, and be able to express complex types.  This facility
is provided by a typed data API called SValue.

An SValue is similar to a COM variant (or GLib GValue), in that it holds
a typed piece of data, such as an integer, string, etc.  In addition,
SValue can also contain complex sets of mappings of any such data value
(including other mappings).  SValue also does not impose a fixed set of
types, instead defining a generic representation of a type constant,
length, and blob of data.

Unlike a standard POSIX shell, all data that we work with in the Binder
Shell is an SValue.  This includes environment variables, command arguments,
and command results.  Thus when we executed this command:

@verbatim
/# lookup /settings/vendor/foo
Result: "bar"
@endverbatim

What we were seeing is that the lookup command returned the data it found
under @c /settings/vendor/foo, which is an SValue containing the string "bar".

The default data type that you create in the shell is a string, to match
the standard POSIX semantics.  To create types of other values, you can
use a new <tt>\@{ }</tt> shell syntax:

- <tt>\@{ 10 }</tt> is an integer.
- <tt>\@{ foo, bar }</tt> is a set (here of strings).
- <tt>\@{ foo->bar }</tt> is a mapping (here of strings).
- <tt>\@{ foo->10, 2->bar }</tt> is a set of mappings (here of strings and integers).

Given that, we can use this with the @c publish command to place more
complex data into the settings catalog:

@verbatim
/# publish /settings/map @{ 10 -> "99" }

/# lookup /settings/map
Result: int32_t(10 or 0xa) -> "99"
@endverbatim

@note It is important to understand what we have done here: "settings" is
a directory, and "map" is a data entry we have made in that directory.
The "map" entry contains a @e single data item, containing the complex
mapping <tt>\@{ 10 -> "99" }</tt>

The <tt>\@{ }</tt> syntax is extremely rich.  Here is a brief summary of
common things you can do with it:

@verbatim
/# F=@{ 1.0 }              # Make a floating point number
/# I=@{ [abcd] }           # Make a 32 bit integer of ASCII chars
/# B=@{ true }             # Make a boolean
/# S=@{ "1" }              # Make a string
/# I64=@{ (int64_t)10 }    # Make a 64 bit integer
/# T=@{ (nsecs_t)123456 }  # Make a time
/# B=@{ (bool)$I64 }       # Convert to a boolean
/# I=@{ (int32_t)$S }      # Convert to an integer
/# S=@{ (string)$B }       # Convert to a string
/# M1=@{ a->$S }           # Make a mapping
/# M2=@{ a->$B, b->"foo" } # Make set of mappings
/# M=@{ $M1 + $M2 }        # Combine the mappings
/# V=@{ $M[a] }            # Look up a value in a mapping
@endverbatim

See @ref BinderShellData for more details on the
<tt>\@{ }</tt> syntax and SValue data types.

@subsection VariablesCommands Variables and Commands

As mentioned, environment variables in the Binder Shell are typed:

@verbatim
/# VAR=something
Result: "something"
	
/# VAR=@{ 10 -> "99" }
Result: int32_t(10 or 0xa) -> "99"
@endverbatim

In addition, command results are typed and the Binder Shell
introduces a new <tt>$[ ]</tt> syntax for easily retrieving the result
of a command, much like the standard <tt>$( )</tt> syntax for
retrieving a command's output:

@verbatim
/# VAR=$[lookup /settings/vendor/foo]
Result: "bar"

/# echo $VAR
bar
@endverbatim

This facility will be used extensively later as we look at new
commands that generate complex data results.

@section BinderObjects Binder Objects

In addition to plain data, the type values you work with in the
Binder Shell can be a more interesting kind of data &mdash; a @ref BinderObjectDefn.
This facility allows the Binder Shell to deeply integrate with the
rest of the Binder runtime.

@subsection ServicesCatalog /services: The Services Catalog

The next directory we will look at is <tt>/services</tt>, which is
a standard catalog holding various active services:

@verbatim
/# ls /services
informant
memory_dealer
tokens/
@endverbatim

The data here is our new type of SValue, an object.  For example, if we
use the @c lookup command to retrieve an entry in this catalog, this is
what we will see (when called from the same process as the object):

@verbatim
/# lookup /services/informant
Result: sptr<IBinder>(0x80966bc 9Informant)
@endverbatim

Such an object is an active entity, holding both state and the
implementation for manipulating that state.

Binder objects are reference counted &mdash; the object will be destroyed
for you when all references on it go away.  Unlike the data we saw before,
objects are passed by reference, even when going across processes.
For example, if you call a shell command with
an object as an argument, that command will be operating on the same
object as the one you gave it.  Normal data, in contrast, is copied, so
the command could not modify the data being held by the caller.

Every Binder object provides a set of properties and methods that others
can use.  The Binder Shell commands @c get and @c put are used to retrieve
and modify the properties on an object, respectively.  The command
@c invoke is used to call a method on an object.  For all of these
commands, the first argument is the object to operate on &mdash; either an
actual object value, or a path in the namespace to an object.

@subsection ExampleInformant Example: Informant

As a demonstration, we can use the shell to invoke a method on the
"informant" service.  This is a Binder component that allows other
objects to register for and broadcast arbitrary notifications.  It
has a method for broadcasting notifications that we can call like
this:

@verbatim
/# invoke /services/informant Inform myMessage "This is some data"
@endverbatim

Well, okay, that wasn't very thrilling &mdash; when the system first starts
up, nobody has registered to receive a notification, so there is not
much we can do.

Recall, however, that we mentioned earlier that Binder Shell commands
are Binder components...  and that includes the Binder Shell itself.
In fact, there is a special shell variable called @c $SELF that provides
the IBinder object for the current shell.  This gives us a more
interesting opportunity.

First, let's define a function in our current shell session:

@verbatim
/# function Receiver() { echo "Receiver: " $1 "," $2 "," $3; }
@endverbatim

We can now use the @c $SELF variable to register our shell with the
informant, giving the name of our function as the method it should
call:

@verbatim
/# invoke /services/informant RegisterForCallback "myMessage" $SELF "Receiver"
@endverbatim

And now let's try again the first method invocation:

@verbatim
/# invoke /services/informant Inform myMessage "This is some data"
/#
Receiver:  This is some data , myMessage ,
@endverbatim

What we see here is that after calling Inform on the informant and returning,
the Informant has then called back on our shell to tell it the message
that was broadcast.

A look at this same kind of operation through the C++ APIs can be found
in @ref BinderRecipes.

@subsection BinderInterfaces Binder Interfaces

Something we have glossed over so far is just how we know what methods and
properties an object has. This information is provided by an "interface",
a formal specification of a set of properties and methods.  This specification
is written in a language called IDL (Interface Description Language), like
COM and CORBA.  You can find all of the interfaces included with OpenBinder
in the "interfaces" directory &mdash; for example, the IInformant interface we
have been playing with is defined in <tt>interfaces/services/IInformant.idl</tt>.

If you are already familiar with COM or CORBA, one thing that is worth
pointing out is that in the Binder these interfaces are not the core
communication protocol.  Instead, the standard Binder language (as defined by
IBinder) is a dynamically typed scripting protocol, which the Binder Shell
sits directly on
top of. Interfaces, then, are just formalized specifications of a scripting
protocol that an IBinder provides, but you do not need to know about a
specific interface to operate on a Binder object.

We can now look at the informant IDL file to see just what we were
doing to that service:

@code
namespace palmos {
namespace services {

interface IInformant
{
methods:
	// flags are the IBinder link flags
	status_t	RegisterForCallback(SValue key,
									IBinder target,
									SValue method,
									[optional]uint32_t flags,
									[optional]SValue cookie);

	...

	status_t	Inform(SValue key, SValue information);
}

} }	// namespace palmos::services
@endcode

You can use the @c inspect command in the shell to find out about the
interfaces that an object implements.  For example, we can look at our
informant service:

@verbatim
/# inspect /services/informant
Result: "org.openbinder.services.IInformant" -> sptr<IBinder>(0x8096794 9Informant)
@endverbatim

An interface can also implement multiple interfaces, in which case
@c inspect will return all of them:

@verbatim
/# inspect /processes
Result: {
	"org.openbinder.support.INode" -> sptr<IBinder>(0x808b5a4 14ProcessManager),
	"org.openbinder.support.ICatalog" -> sptr<IBinder>(0x808b594 14ProcessManager),
	"org.openbinder.support.IIterable" -> sptr<IBinder>(0x808b5b4 14ProcessManager),
	"org.openbinder.support.IProcessManager" -> sptr<IBinder>(0x808b6b8 14ProcessManager)
}
@endverbatim

This brings us to another important use of @c inspect, interface selection.
You can supply a second parameter to @c inspect that specifies an interface
you would like; in this case inspect will return either the Binder object of
that interface or @c undefined if the interface is not implemented.

@verbatim
/# inspect /processes org.openbinder.support.IProcessManager
Result: sptr<IBinder>(0x808b6b8 14ProcessManager)
@endverbatim

Note that like the @c lookup command, @c inspect returns its result as
a typed value.  You can use the <tt>$[ ]</tt> syntax we saw previously to
retrieve that value.

@subsection DataModelInterfaces Data Model Interfaces

You may have noticed some interesting interfaces when we inspected /processes: INode,
IIterable, and ICatalog.  These are standard interfaces for an object that
contains a set of entries...  that is, a directory.  In fact, all of the
shell commands we have been using that operate on the namespace (<tt>ls</tt>,
<tt>publish</tt>, etc) are simply making calls on these standard interfaces.

In other words, the entire Binder namespace is simply a hierarchy of active
objects.  Some of these are generic directories that we can place anything
inside of (such as the @c /services directory), but often these are special implementations
of the namespace interfaces.  For example, the Settings Catalog that we looked
at previously provides its own implementation that keeps track of all the
data placed into it and saves that data out to an XML file.

As an example, we can retrieve the INode interface for the @c /services directory:

@verbatim
/# n=$[inspect /services org.openbinder.support.INode]
Result: sptr<IBinder>(0x8091d74 8BCatalog)
@endverbatim

One of the things that INode does is provide access to meta-data about
the namespace entries, which we can now retrieve through direct calls on
the interface:

@verbatim
/# get $n mimeType
Result: "application/vnd.palm.catalog"

/# get $n modifiedDate
Result: nsecs_t(13107d 10h 59m 57s 746ms 551us or 0xfb7648f415af8d8)
@endverbatim

It is not uncommon for a service to implement both an API specific to that
service, as well as the generic data model interfaces.  This allows it to
publish parts of itself in a standard way that is easily accessible through
the shell and everything else that operates on the namespace, while at the
same time providing a more specialized interface for more complicated
interactions.

@section ComponentsProcesses Components and Processes

We are now going to look at where Binder objects come from and how they
can be used to perform complicated system-level operations.

@subsection BinderComponents Binder Components

A @ref ComponentDefn is the implementation (class) of a Binder object that has
been bundled up in a @ref PackageDefn so that the system knows about it.  You
identify a component by name, and we use a Java-style naming
convention &mdash; the prefix being a domain name.  For example, components
that are part of the OpenBinder package start with <tt>org.openbinder</tt>.
In addition, for historical reasons components that are a part of the
Binder runtime itself use the prefix <tt>palmos</tt>.

A new component instance is created with the @c new command, which takes
the name of the component to instantiate and returns an IBinder object
of the new instance.

@verbatim
/# s=$[new org.openbinder.samples.Service]
Result: sptr<IBinder>(0x80a0194 13SampleService)

/# invoke $s Test
Result: int32_t(12 or 0xc)
@endverbatim

These shell commands have instantiated a sample service component that is
included with OpenBinder and called a method on it.

You can optionally supply a second argument to @c new, which is a set
of mappings providing arguments to the component constructor.

@verbatim
/# s=$[new org.openbinder.samples.Service @{start->500}]
Result: sptr<IBinder>(0x80a19a4 13SampleService)

/# invoke $s Test
Result: int32_t(501 or 0x1f5)
@endverbatim

An alternative syntax for this allows you to bundle the component name
with its arguments as a single complex data mapping.

@verbatim
/# s=$[new @{ org.openbinder.samples.Service->@{start->500} }]
Result: sptr<IBinder>(0x80a19a4 13SampleService)

/# invoke $s Test
Result: int32_t(501 or 0x1f5)
@endverbatim

This last form can be very useful when providing a component name to
another entity that will instantiate it &mdash; it allows you to bundle
any other additional data along with the component.

@subsection ProcessesShell /processes and new_process

Processes in the Binder are simply other objects.  The main difference
between a process object and a normal object is that instead of @c new,
you use the special command @c new_process to create a new process object.

@verbatim
/# p=$[new_process my_process]
LOCK DEBUGGING ENABLED!  LOCK_DEBUG=15  LOCK_DEBUG_STACK_CRAWLS=1
START: binderproc #29572 my_process
Result: IBinder::shnd(0x2)
@endverbatim

The result we see here is a new kind of Binder object, a handle.  Instead
of being a pointer to an object in the local process, it is a descriptor
to an object that lives in another process.  Besides how it looks when
printed, however, it operates and behaves just like the objects we have
seen so far.

In fact, we can use the @c inspect command to find out a little more about
this process object we have:

@verbatim
/# inspect $p
Result: "org.openbinder.support.IProcess" -> IBinder::shnd(0x2)
@endverbatim

If you want, you can go and look at the IProcess IDL file.  However, you
generally won't use that interface directly, but rather use the process
object with other APIs.

The @c /processes catalog is used as a place where you can store
references to processes that will be shared between components.  The
normal boot script included with OpenBinder uses this to publish a
process called "background" that can be used for components that don't
need a process of their own.

@subsection WorkingWithProcesses Working With Processes

A common way to work with Binder processes is to use @c new_process to
create a new empty process (a sandbox) and then create components inside
of it.  You do this with the @c -r option on @c new, which tells @c new
which process you want the object created in.  Let's go back to our
original example of creating a component and now create it in our process.

@verbatim
/# s=$[new -r $p org.openbinder.samples.Service]
Result: IBinder::shnd(0x3)

/# invoke $s Test
Result: int32_t(502 or 0x1f6)
@endverbatim

Notice that besides the kind of object returned, our service works just
like it did when we had it running in the local process.  This is a
very important characteristic of the Binder &mdash; it completely hides
the location of objects, making remote objects look and behave just like
local objects.  This gives us a lot of flexibility in deciding, even
at run time, how objects will be distributed across processes.

When looking at the overall system, you can see where IPC will happen
simply by looking at which process objects are located in, and which objects
call on to each other.  Communication between objects is always 1:1 &mdash;
once you transfer an object reference from one process to another, that
will set up a direct communication channel between them and your own
process will no longer be involved.  Even if your own process completely
goes away, the connection you set up between the other two processes
will remain undisturbed.

@subsection ShellProcessLifetime Process Lifetime

Normally a process will stay around "as long as it is needed."  A
process being needed is defined as there being remote references
on any of its objects.  (More specifically, remote @e strong references.)
Thus, we can see that once we remove all references on the process
we previously created, it will go away automatically:

@verbatim
/# p=
Result: ""

/# s=
Result: ""
/# EXIT: binderproc #29572 my_process
[SIGCHLD handler] child process 29572 exited normally with exit value 0
@endverbatim

The @c stop_process command gives you more control over this behavior.
It takes a process object as an argument, and will have the process
go away as soon as all remote reference on just that process object
have been removed.

@verbatim
/# p=$[new_process my_process]
LOCK DEBUGGING ENABLED!  LOCK_DEBUG=15  LOCK_DEBUG_STACK_CRAWLS=1
START: binderproc #29607 my_process
Result: IBinder::shnd(0x2)

/# s=$[new -r $p org.openbinder.samples.Service]
Result: IBinder::shnd(0x3)

/# stop_process $p

/# p=
Result: ""
/# EXIT: binderproc #29607 my_process
[SIGCHLD handler] child process 29607 exited normally with exit value 0

/# invoke $s Test
Result: Unknown error (0x80004a03)
@endverbatim

In addition, if you use the @c -f flag with @c stop_process then the
process will go away immediately, no matter what references there are
on it.

@verbatim
/# stop_process -f $PROCESS
/# EXIT: binderproc #29568 background
hackbod@hackbod:~/lsrc/open-source/openbinder/main/dist$
@endverbatim

Here we used @c stop_process with our own process to make it exit.  The
line about the "background" process exiting is because this also causes
us to drop our reference on the background process that the boot script
had created.

@subsection ShellDynamicProcesses Dynamic Processes

Another way that processes can be started is because a component, in its
manifest, specified that it would like to run in a particular process.
The ServiceProcess sample is a demonstration of this, which we can see
when instantiating the component:

@verbatim
/# s=$[new org.openbinder.samples.ServiceProcess]
LOCK DEBUGGING ENABLED!  LOCK_DEBUG=15  LOCK_DEBUG_STACK_CRAWLS=1
START: binderproc #29634 /home/hackbod/lsrc/open-source/openbinder/
	main/dist/build/packages/org.openbinder.samples.ServiceProcess
Result: IBinder::shnd(0x3)
@endverbatim

Here the component has asked that it be instantiated in a process
that is dedicated to components in its package.  When the first
instance of the component (or other such components in this package)
is created, that process is started and the component created inside
of it.  Additional instances of the component will be placed into the
same process.

We can now see this process in the @c /process catalog:

@verbatim
/# ls /processes
/home/hackbod/lsrc/open-source/openbinder/main/dist/build/
	packages/org.openbinder.samples.ServiceProcess
background
system
@endverbatim

Note that the current process manager uses the path to the component
implementation as a unique name for the process, however the process
manager itself is just a component that can be customized to provide
whatever behavior you desire.

If we make another instance of the service component, it will be created
in the same process as the first one:

@verbatim
/# s2=$[new org.openbinder.samples.ServiceProcess]
Result: IBinder::shnd(0x4)
@endverbatim

Likewise other components in the package can also specify that
they would like to run in the package's process:

@verbatim
/# s3=$[new org.openbinder.samples.ServiceProcess.Command]
Result: IBinder::shnd(0x5)
@endverbatim

Of course they don't need to do so, in which case they will
be instantiated as normal, usually in the process of the
caller.

As we saw before in @ref ShellProcessLifetime, this process will
continue to remain around only for as long as it is actually
needed:

@verbatim
/# s=
Result: ""
/# s2=
Result: ""
/# s3=
Result: ""
/# EXIT: binderproc #21330 /home/hackbod/lsrc/open-source/openbinder/
	main/dist/build/packages/org.openbinder.samples.ServiceProcess
[SIGCHLD handler] child process 21330 exited normally with exit value 0
@endverbatim

For more on processes, see the @ref BinderProcessModel.

@subsection BinderObjectIdentity Binder Object Identity

Objects have a unique identity that is always maintained across
processes.  For example:

@verbatim
/# p=$[new_process secondary]
LOCK DEBUGGING ENABLED!  LOCK_DEBUG=15  LOCK_DEBUG_STACK_CRAWLS=1
START: binderproc #29658 secondary
Result: IBinder::shnd(0x4)

/# su -r $p
Starting sub-shell in system context...

/# v1=$[lookup /services/informant]
Result: IBinder::shnd(0x7)

/# v2=$[lookup /services/informant]
Result: IBinder::shnd(0x7)
@endverbatim

What we are seeing here is that when an object reference is transfered
between processes (here from the smooved to the "secondary" process
that we created), as long as the receiving process already knows about
that object it will always get the same identity.

However, that object will have different identities in different
processes:

@verbatim
/# exit
/# v3=$[lookup /services/informant]
Result: sptr<IBinder>(0x8096744 9Informant)
@endverbatim

The @ref BinderIPCMechanism is responsible for correctly mapping between
these identities as objects move between processes.  In essence,
this provides a capability-style security model for Binder objects &mdash;
you can only perform operations for which you have been granted
explicit access through a Binder object, and you can always validate
the identity of an object (against an existing reference to it) when
you receive it.

@subsection Contexts /contexts: Multiple Contexts

We say that a Binder object runs in a context.  The context is the
"global" namespace it can access, and has have been implicitly used with
@c ls, @c lookup, and other commands.  A component is
"instantiated inside" of a context &mdash; that is, you use a context
to instantiate a component, and that new component instance also
uses your context.

There can be more than one context.  The default boot script that
we have been using with smooved creates two contexts, which you
can see by looking at the @c /contexts directory:

@verbatim
/# ls /contexts
system/
user/
@endverbatim

The <em>system context</em> is the one we have been in, and has
complete access to all system resources.  The <em>user context</em>
is a more restricted environment, which is not able to do things
like modify the contents of the @c /services directory.

You can use the @c su command to switch between contexts (and
processes, as we saw earlier):

@verbatim
/# lookup who_am_i
Result: "System Context"

/# su user
Starting sub-shell in new context...

/$ lookup who_am_i
Result: "User Context"

/$ exit
/# lookup who_am_i
Result: "System Context"
@endverbatim

The "who_am_i" entry is created by the boot script in each context,
as a debugging aid.

@section PackageManager The Package Manager

The @ref PackageManagerDefn is the entity responsible for keeping
track of all available components and information about them.  It
is involved in the first step of the @c new command, providing the
information about the component needed to find and instantiate it.

@subsection Packages /packages: Package Information

The @ref PackageManagerDefn service is placed in the namespace at @c /packages,
and under that directory provides a hierarchy of information
associated with packages.  One of the most important things here
is the @c components sub-directory, which contains information about
every component in the system:

@verbatim
/# ls /packages/components
org.openbinder.samples.Component
org.openbinder.samples.Service
org.openbinder.samples.ServiceProcess
org.openbinder.samples.ShellService
org.openbinder.kits.support.CatalogMirror
org.openbinder.services.MemoryDealer
org.openbinder.services.Settings
org.openbinder.services.TokenSource
org.openbinder.services.base.Informant
org.openbinder.tools.BinderShell
org.openbinder.tools.BinderShell.Cat
org.openbinder.tools.BinderShell.Clear
...
@endverbatim

Under each of these entries is a block of data describing everything
about the component.

@verbatim
/# lookup /packages/components/org.openbinder.tools.BinderShell
Result: {
        "file" -> "/home/hackbod/lsrc/open-source/openbinder/main/dist/build/packages/org.openbinder.tools.BinderShell/BinderShell.so",
        "package" -> "org.openbinder.tools.BinderShell",
        "modified" -> int32_t(1132535954 or 0x43812092),
        "interface" -> "org.openbinder.tools.ICommand",
        "packagedir" -> "/home/hackbod/lsrc/open-source/openbinder/main/dist/build/packages/org.openbinder.tools.BinderShell"
}
@endverbatim

When the Package Manager is started, a set of queries on the package data is
also created and published in its directory.  These allow you to find components for
various purposes.  For example, the Binder Shell itself uses the
<tt>/packages/bin</tt> query to find the component that implements a
particular shell command.

@verbatim
/# ls /packages/bin
[
atom
bperf
cat
clear
components
@endverbatim

@verbatim
/# lookup /packages/bin/cat
Result: "org.openbinder.tools.BinderShell.Cat"
@endverbatim

You can also do queries for components implementing specific interfaces
and other data in the manifest.

@subsection PackageContents Package Contents

On disk, a package is
a filesystem directory containing at least a Manifest.xml file describing
the contents of the package and one or more files (.so files for C++ code)
containing the implementation of the components in the package.  The
system takes care of loading and unloading the component implementation
as needed.

The manifest is an XML file that provides all information to the package
manager about the components in the package.  Note that unlike COM,
component information comes only from the static manifest file &mdash; there is
no need to register a component with the package manager, and the package
manager is free to reconstruct its component information from scratch at
any time from the package manifests.

A typical manifest file can be seen in the SampleComponent sample, which
implements a Binder Shell command.

@verbatim
<manifest>
	<component>
		<interface name="org.openbinder.app.ICommand" />
		<property id="bin" type="string">sample_component</property>
	</component>
</manifest>
@endverbatim

Here we can see that the package contains a single component, whose
name is the same as the package name.  That component implements
the ICommand interface.  The <tt>\<property\></tt> tag here adds an additional
property, called "bin".  This is the property the Package Manager looks for to
construct the <tt>/packages/bin</tt> query, and thus how you make
the component visible to the Binder Shell.

Here is a more complicated manifest, from the ServiceProcess
sample.  This contains two components, both of which would like
to run in their own process for the entire package.  The second
component is a shell command.

@verbatim
<manifest>
	<component>
		<process prefer="package" />
		<interface name="org.openbinder.support.INode" />
		<interface name="org.openbinder.support.IIterable" />
		<interface name="org.openbinder.support.ICatalog" />
	</component>
	<component local="Command">
		<process prefer="package" />
		<interface name="org.openbinder.app.ICommand" />
		<property id="bin" type="string">service_command</property>
	</component>
</manifest>
@endverbatim	

@section ShellAdvancedTopics Advanced Topics

The remaining material in this document covers some advanced topics
that you will not normally encounter in the shell itself, but are
important in using the underlying C++ APIs.  Just as with the other
topics we have covered, the shell serves as a useful environment
in which to illustrate these concepts.

@subsection StrongWeakPointers Strong and Weak Pointers

Recall that when we are working with objects in the shell, we
are using a reference counting mechanism (based on SAtom) so that the
object will remain around as long as there are others using it.

@verbatim
/# s=$[new org.openbinder.samples.Service]
Result: sptr<IBinder>(0x809f86c 13SampleService)
@endverbatim

There are actually two kinds of object pointers.  The one we
have seen so far is a "strong pointer" or "sptr" because it
ensures that the object will remain valid.

The other type of reference is called a "weak pointer" or "wptr".
You won't normally see these in the shell, however, we can use
a cast to explicitly create a weak pointer:

@verbatim
/# w=@{(wptr)$s}
Result: wptr<IBinder>(0x809f86c 13SampleService)
@endverbatim

Unlike a strong pointer, an object is allowed to go away while
others hold weak pointers to it.  We can see this in action
if we now clear the strong pointer:

@verbatim
/# s=
Result: ""
@endverbatim

What happens to our weak pointer in this case is a little subtle.
If we try to invoke a method on that, it will fail, because the
object is no longer valid:

@verbatim
/# invoke $w Test
invoke: invalid target object ''.  Not going to do an invoke.
Result: Unknown error (0x80000502)
@endverbatim

However, the weak pointer itself still needs to hold @e something,
so if we print it we will see that it still has a valid pointer, even
though the object is no longer usable:

@verbatim
/# echo $w
SValue(wptr<IBinder>(0x809f86c))
@endverbatim

Notice the subtle point that though we are still seeing a valid
address printed, we no longer see the actual class name like we did before.

If we now try to cast that weak pointer, we will see that this
fails:

@verbatim
/# s=@{(sptr)$w}
Result: sptr<IBinder>((nil))
@endverbatim

This is, in fact, what happened when we tried to use @c invoke &mdash;
it implicitly tries to convert its input to a strong pointer so
it can call it, which fails.  In the C++ APIs, this conversion
is done explicitly through the wptr<>::promote() method.

Note that all of these rules for weak pointers hold for processes
as well as objects.  That is, a process's lifetime is determined
by @e strong pointers on its objects &mdash; holding a weak pointer
on an object, including the IProcess object itself, will not prevent
the process from going away.

@subsection ShellLinks Links

Links are a facility of the Binder object model that allows an object
to send data and events out of itself, rather than passively relying
on calls coming in.  It is conceptually similar to, for example,
signals in the Qt toolkit.

There are two kinds of links an object can push: properties and methods.
Properties can only be linked to other properties, and events can only
be linked to methods.  This restriction is because the scripting
protocol for properties and methods is different.

You will generally find out about what you can link to by looking at
an interface's IDL file.  As an example, let's look at the interface
for INode, one of the data model interfaces.  It can be found in
interfaces/support/INode.idl.  The thing we are interested in is this
event declared towards the end:

@code
namespace palmos {
namespace support {

interface INode
{
	...

events:
	...
	
    // This event is sent when a new entry appears in the catalog.
    /*	"who" is the parent node in which this change occured.
        "name" is the name of the entry that changed.
        "entry" is the entry itself, usually either an INode or IDatum.
    */
    void EntryCreated(INode who, SString name, IBinder entry);
	
	...
}
@endcode

Given that, let's write a shell function that handles the same
method signature:

@verbatim
/# function handleEntryCreated() {
	echo "Created: parent=" $1
	echo "Created: name=" $2
	echo "Created: entry=" $3
}
@endverbatim

We can now use the @c link shell command to set up a link from the @c /services
directory to our new shell method.

@verbatim
/# n=$[inspect /services org.openbinder.support.INode]
Result: sptr<IBinder>(0x8091dc4 8BCatalog)

/# link $n $SELF @{EntryCreated->handleEntryCreated}
@endverbatim

This says "make a link from the object @c $n to the object @c $SELF, such that
when @c EntryCreated is pushed from @c $n we will have the @c handleEntryCreated
method called on @c $SELF".  And thus, upon adding a new entry to
@c /services, we will see this:

@verbatim
/# publish /services/test linktest
Publishing: /services/test
/# Created: parent= SValue(sptr<IBinder>(0xb69063f4 8BCatalog))
Created: name= test
Created: entry= SValue(sptr<IBinder>(0x8081254 N18SDatumGeneratorInt12IndexedDatumE))
@endverbatim

Links are a very powerful mechanism, though not the only way to achieve the
same result.  Depending on your needs, you can just as well write your own
notification mechanism for a specialized purpose, or use IInformant for
a more generalized implementation of broadcasting without using links.
A plain link, however, has significant advantages in being clearly documented
in IDL and a standard mechanism that many other things will be able to use
without being written specifically to receive your event.


*/

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/*!
@page BinderShellData Binder Shell Data

<center>\< @ref BinderShellTutorial | @ref BinderKit | @ref BinderShellSyntax \></center>
<hr>

An important new feature of the Binder Shell is that all of its data
operations make use of SValue.  That is, all environment variables,
command arguments, and command results are actually typed data.

The default type the shell uses is a string (for POSIX compatibility),
as can be seen here using the standard shell syntax of assigning a value
to an environment variable:

@verbatim
/# S=foo
Result: "foo"
@endverbatim

There is also an extension to the POSIX shell syntax, <tt>\@{ ... }</tt>,
that allows you to construct values of specific types using the
underlying SValue APIs.  Unless otherwise specified, the data inside
of the braces will be interpreted as an integer, float, boolean, or string:

@verbatim
/# I=@{1}
Result: int32_t(1 or 0x1)

/# I=@{[abcd]}
Result: int32_t(1633837924 or 0x61626364)

/# F=@{1.0}
Result: float(1.0)

/# B=@{false}
Result: false

/# B=@{true}
Result: true

/# S=@{"1"}
Result: "1"

/# S=@{foo}
Result: "foo"
@endverbatim

You can also use conversion operators to generate some additional types
or convert existing variables to another type:

@verbatim
/# I64=@{ (int64_t)10 }
Result: int64_t(10 or 0xa)

/# T=@{ (nsecs_t)123456 }
Result: nsecs_t(123us 456ns or 0x1e240)

/# B=@{ (bool)$I64 }
Result: true

/# I=@{ (int32_t)$B }
Result: int32_t(1 or 0x1)

/# S=@{ (string)$I }
Result: "1"

/# S=@{ (string)$B }
Result: "true"
@endverbatim

A value can also contain a complex mapping of other values. 
Such mappings (and sets) are constructed with the <tt>-\></tt> and
<tt>,</tt> operators:

@verbatim
/# M=@{ a -> b }
Result: "a" -> "b"

/# S=@{ a, b }
Result: {
	"a",
	"b"
}

/# M2=@{ $M, c -> d }
Result: {
	"a" -> "b",
	"c" -> "d"
}
@endverbatim

There are three types of operations that can be used to build values
containing multiple mappings.  A join is created with the <tt>,</tt> or
<tt>+</tt> operator, which you have already seen.  It is
non-destructive &mdash; the result contains all data supplied on both sides of the join.

@verbatim
/# M=@{ a->b, a->c }
Result: "a" -> {
	"b",
	"c"
}
@endverbatim

An overlay operation is performed with the <tt>+\<</tt> operator.  It is like
a join, except that it selects items on the right side when the same key
appears on both the left and right sides.  In other words, it replaces values
on the left with values on the right, leaving the left side as-is when there
is nothing on the right.

@verbatim
/# M=@{ {a->b,c->d} +< {a->e,f->g} }
Result: {
	"a" -> "e",
	"c" -> "d",
	"f" -> "g"
}
@endverbatim

An inherit operation is performed with the <tt>+\></tt> operator.  It is the opposite
of an overlay, selecting values on the left side instead of the right.

@verbatim
/# M=@{ {a->b,c->d} +> {a->e,f->g} }
Result: {
	"a" -> "b",
	"c" -> "d",
	"f" -> "g"
}
@endverbatim

You can use the <tt>[]</tt> operator to look up a value from a mapping key:

@verbatim
/# M=@{ a -> b + c -> d }
Result: {
	"a" -> "b",
	"c" -> "d"
}

/# V=@{ $M[c] }
Result: "d"
@endverbatim

There are three special types of values with different semantics than normal types,
especially when used in mappings.  The @c undefined value is the state of a
variable that has not been set; trying to create a mapping from some value to
@c undefined will result in @c undefined:

@verbatim
/# U=@{undef}
Result: undefined

/# M=@{ a -> undef }
Result: undefined

/# M=@{ undef -> a }
Result: undefined
@endverbatim

The @c wild value is one that means "everything."  In fact, every value you
build is a mapping; a value containing a single data item is really the
mapping <tt>{ wild -> value }</tt>, but the leading @c wild is dropped for
convenience.  This allows us to create a consistent set of mapping operations
on values, which are well-defined regardless of the contents of the values.

@verbatim
/# W=@{wild}
Result: wild

/# M=@{ wild -> a }
Result: "a"

/# M=@{ a -> wild }
Result: "a" -> wild
@endverbatim

The @c null value means "not specified" or "empty".  It is different than
undefined in that it has no special significance in terms of mappings,
however it does have special meaning when marshalling and
unmarshalling across Binder calls &mdash; it specifies that the default parameter
value should be used.

@verbatim
/# N=@{null}
Result: null

/# M=@{ null -> a }
Result: null -> "a"

/# M=@{ a -> null }
Result: "a" -> null
@endverbatim
*/

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/*!
@page BinderShellSyntax Binder Shell Syntax

<center>\< @ref BinderShellData | @ref BinderKit | @ref BinderTerminology \></center>
<hr>

This page provides a more detailed reference on the syntax and capabilities
of the Binder shell.

The Binder Shell is a basic implementation of the POSIX shell syntax
(spec at http://www.opengroup.org/onlinepubs/009695399/utilities/xcu_chap02.html) with
a few extensions. While it should be able to parse the full POSIX grammar correctly,
there are a few features, most notably pipes, that aren't implemented yet.

Besides these limitations, the shell provides the same general environment one
expects from a POSIX shell:

@verbatim
variable="something"

if [ "$variable" -eq "something" ]
    echo "Variable is something"
else
    echo "Variable is something else"
fi
@endverbatim

For the rest of this document we will assume that you are already familiar
(as much as you want to be) with POSIX shell syntax.

@section BinderShellExtensions Binder Shell Extensions

The Binder Shell introduces a few extensions to standard POSIX shell
that make it much more useful in the Binder environment.  Most of these are
semantic changes, though there are a few important new grammatical constructs.

@subsection VariablesAreSValues All environment variables, command arguments, and return codes are SValues

This means that most data handled by the shell is typed, not just a simple string.
The arguments to a function, its return code, and environment variables in the
shell can contain strings, integers, flattened types (rect, point, etc), or even
references to objects or complex mappings.

As these values propagate between the command and the shell, their type is
propagated as well. For example, if you have an environment variable
@c SOMETHING that contains a reference to an IBinder object, you can pass it
to a function like a normal environment variable:

@verbatim
inspect $SOMETHING
@endverbatim

The second argument that the @c inspect command receives will be a SValue
containing a pointer to the same binder.  The command can similarly return a
result SValue containing an object that the shell can use.

@subsection CommandResultSyntax New syntax $[cmd ...] for retrieving the result of a command

The standard POSIX shell defines the syntax <tt>$(cmd ...)</tt> to embed
the output of a command into the place this construct appears.  The Binder
Shell adds a variant <tt>$[cmd ...]</tt>, which embeds the result of a
command.  Because command results are now an SValue, this allows you to
propagate typed data through the shell.

For example, there is a @c lookup command that returns the SValue of a
particular property in the context.  This can then be assigned to an
environment variable:

@verbatim
SURFACE=$[lookup /services/surface]
@endverbatim

The environment variable can be passed to another command:

@verbatim
put $SURFACE show_updates true
@endverbatim

One could even write commands that take typed data as input and generate
a typed result. For example, if we had two theoretical commands, rect and
point, which generate SValues containing data of their respective types,
we could write shell code like this:

@verbatim
RECT=$[rect 0 0 100 120]
POINT=$[point 10 10]

NEWRECT=$[rect_offset $RECT $POINT]
@endverbatim

Or even:

@verbatim
NEWRECT=$[rect_offset $RECT $[point 10 10]]
@endverbatim

This facility is used extensively by the standard commands for working
with the Binder runtime, such as @c new, @c new_process, and @c invoke.

@subsection ComplexValueSyntax New syntax \@{ key-\>value, ... } for building complex values

The Binder Shell also adds a new construct for conveniently
building up basic SValue types. The text inside a <tt>\@{ ... }</tt>
section defines SValue mappings (separated by <tt>-\></tt>) and sets
(separated by <tt>,</tt>); a combination of both can be used to
create multiple mappings. Some examples:

@verbatim
@{ data }                       # A simple string
@{ something, somethingelse }   # A set of strings
@{ param->value }               # A mapping
@{ {param->value}[param] }      # Lookup in a mapping
@{ param1->something,           # Multiple mappings
   param2->somethingelse }
@{ param1->{ value1, value2 } } # Mapping to a set
@{ param1->{ nested->value } }  # Mapping to a mapping
@{param1->{nested->value}}      # All spaces are optional and ignored
@endverbatim

Tokens inside of an SValue context are typed.  If the token is all
numbers, it is interpreted as an integer; if it is all numbers with
a single period, it is a float; true and false are the corresponding
boolean values; otherwise, it is a string:

@verbatim
@{ 1 }                          # An integer
@{ [abcd] }                     # An integer of ASCII characters
@{ 1.0 }                        # A float
@{ true }                       # Boolean truth
@{ 1->something }               # Integer mapped to string
@endverbatim

You can also explicitly specify the type of a token. If a token
is enclosed in double quotes, it is always converted to a string.
If you prefix it with (), you can force it to one of the fundamental
types: (int32), (float), (string), (bool):

@verbatim
@{ "1" }                        # A string
@{ (string)1.0 }                # Also a string
@{ (int64_t)10 }                # A 64 bit integer
@{ (nsecs_t)123456 }            # A time
@{ (float)1 }                   # A floating point number
@{ (int32_t)$VAR }              # Cast to an integer
@{ (bool)1 }                    # Boolean truth
@{ "true" }                     # The string "true"
@endverbatim

Finally, environment variables and command results can be used as tokens:

@verbatim
$ SURFACE=$[lookup /services/surface]

$ M1=@{ 0->$SURFACE }           # Integer mapped to an object
$ M1=@{ 0->$[lookup /services/surface] }    # Equivalent
$ M2=@{ a->$M1, b->"foo" }      # Make set of mappings
$ M=@{ $M1 + $M2 }              # Combine the mappings
$ V=@{ $M[0] }                  # Look up a value in a mapping
@endverbatim

See @ref BinderShellData for more examples.

@subsection FunctionInvocation Function invocation with $OBJECT.Function()

You can append a '.' to an environment variable name in order to
perform a function invocation if that variable holds an object.

@verbatim
$ E=$[inspect /services/surface org.openbinder.services.IErrAlert]
$ RESULT=$E.ShowAlert("Alert text")
@endverbatim

The arguments are supplied as a comma-separated list, and are parsed
as a normal command line: plain text will be interpreted as strings,
you can use <tt>$</tt> for variables, <tt>$[]</tt> to run commands,
<tt>\@{}</tt> to build typed values, etc.

@subsection ForeachCommand New foreach control structure

In addition to the standard for control structures, the Binder Shell
includes a @c foreach statement for operating on IIterable and IIterator
objects, and SValue mappings. The general syntax is:

<pre>
<b>foreach</b> [<i>key</i>] <i>value</i> [<b>in</b>|<b>over</b>] <i>data</i> [<i>data ...</i> ]; <b>do</b>
	<i>statements</i>
<b>done</b>
</pre>

If @a key is not specified, then @a value will contain
<tt>{key-\>value}</tt> mappings for each item. The data items
can be variables or other constructs.

The <tt>foreach ... in ...</tt> form allows you to operate on iterators &mdash;
the data you supply must be either an IIterable or IIterator object, or a
path to an interable object in the namespace, or a mapping containing
values that are iterables or paths. For example, to iterate over all
items in @c /services, you would write this:

@verbatim
foreach key value in /services; do
    echo $key is $value
done
@endverbatim

Resulting in:

@verbatim
informant is SValue(sptr<IBinder>(0x80967c4 9Informant))
memory_dealer is SValue(sptr<IBinder>(0x8090f34 13BMemoryDealer))
tokens is SValue(sptr<IBinder>(0x809a13c 2TS))
@endverbatim

The <tt>foreach ... over ...</tt> form allows you to operate on
value mappings &mdash; it will simply decompose the data into its
separate mappings. For example:

@verbatim
foreach k v over @{0->1, 2->3, 1->a} string $[lookup /services/informant]; do
    echo $k is $v
done
@endverbatim

Results in:

@verbatim
0 is 1
1 is a
2 is 3
SValue(wild) is string
SValue(wild) is SValue(sptr<IBinder>(0x8096f74 9Informant))
@endverbatim

This is a good example of why SValue mappings are defined the way they
are, where a simple value @c V is formally the mapping
<tt>{wild->V}</tt>.  Notice this coming into play for the last
two items, which are not normally considered to be mappings.

Also notice a special property of SValue ordering related to integers.
The order that SValue holds its sets in is not defined to be anything
useful, except in the case of integer keys: these will be in their
natural order.  This property is convenient in dealing with sets
of mappings of integer keys representing arrays.

*/
