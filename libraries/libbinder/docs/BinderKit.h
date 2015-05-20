/*!
@page BinderKit Binder Kit

<div class="header">
<center>\< @ref SHandlerPatterns | @ref OpenBinder | @ref BinderRecipes \></center>
<hr>
</div>

The Binder could be described as a "framework framework". It doesn't do
anything itself, but is an enabling tool for implementing other rich
frameworks, such as the view hierarchy, media framework, etc.

Conceptually, the Binder has many similarities to COM and CORBA &mdash; it
defines a generic object/component model, a standard mechanism for
describing interfaces to objects (through IDL), allows component instances
to live in different processes and takes care of IPC details when
interactions occur between them, and has a facility for registering,
discovering, and instantiating components.

Unlike COM and CORBA, the Binder includes a number of basic features
that make it more suitable for system-level programming.  For example,
the @ref BinderIPCMechanism propagates the current thread priority
across IPC calls, which is important for multimedia applications. 
It also supports recursion
between processes (and generally models IPC as a local function call),
so that the system can arbitrarily place components in different
processes, depending on security/stability vs. performance behavior
determined at run time.

The Binder also includes facilities for registering components (through
static "manifest" descriptions, not requiring a registry as in COM),
which clients can use to discover and make use of components/features in
the system.  The Binder system takes care of loading and unloading as
needed the executable images implementing components, effectively
eliminating the need for manual implementation of plug-in management.

In lieu of a Windows-like Registry, the @ref BinderDataModel is used to
provide a hierarchical namespace containing active objects as well as
simple data.  This namespace is itself defined through a set of standard
Binder interfaces, so anyone can implement a Binder component that fully
participates in the namespace.  For example, there is a Binder component
called the "Settings Catalog" that implements the namespace APIs for
simple data on top of a flat persistent storage; an instance of this
component is placed in the full namespace at "/settings" for others to use.

A rich multi-threaded environment is supported by the Binder, based on
a threading model like COM's "free threaded" components.  Binder components
are generally expected to be thread-safe; @ref BinderThreading describes
the high-level threading facilities and guidelines that are used to make
it easier to write multi-threaded code and make efficient use of threads
in the system.

The Binder Kit is built on top of the @ref SupportKit, which provides a
standard set of basic programming APIs, such as reference counted objects,
threading, and containers.

@note The current source tree has the Binder Kit and @ref SupportKit
combined together in the same directory (support).  This page only
describes the Binder APIs there.

@section Topics Topics

- @subpage BinderRecipes is a quick introduction to what programming with
  the Binder in C++ looks like.
- @subpage BinderShellTutorial provides a high-level overview of the Binder
  system, using the Binder Shell for illustration.
  - @ref BinderShellData provides more detail on creating and manipulating
    SValue objects with the shell.
  - @ref BinderShellSyntax is a reference on the special syntactical
    features of the shell.
- @subpage BinderTerminology is your one-stop-shopping for all the Binder
  concepts and words you'll see thrown around.
- @subpage BinderProcessModel describes the Binder's (optional) process
  communication and management features.
  - @ref BinderIPCMechanism details how IPC works on Linux.
- @subpage pidgen describes the Binder's IDL syntax and how to use the
  pidgen tool to turn it into C++ code.
- @subpage BinderInspect provides detailed information on Binder interface casting.
*/

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/*!
@page BinderTerminology Binder Terminology

<center>\< @ref BinderShellSyntax | @ref BinderKit | @ref BinderProcessModel \></center>
<hr>

@section Alphabetical Alphabetical

- @ref BinderDefn
- @ref BinderObjectDefn
- @ref BinderNamespaceDefn
- @ref ComponentDefn
- @ref ContextDefn
- @ref InspectDefn
- @ref InterfaceDefn
- @ref LinkDefn
- @ref PackageDefn
- @ref PackageManagerDefn
- @ref ServiceDefn

@section Conceptual Conceptual

@subsection BinderDefn Binder

We use this word in two ways: <em>The Binder</em> refers to the overall Binder architecture;
<em>a Binder</em> is a particular implementation of a Binder interface.  The name
"Binder" comes from its ability to provide bindings to functions and data from one
language or execution environment to another.

@subsection BinderObjectDefn Binder Object

Any entity that implements IBinder, the core @ref BinderDefn interface.  In practice, this
generally means a class that derives from BBinder, the standard base implementation of
IBinder.  A Binder object is able to execute the core Binder operations: inspect, effect
(a generalization of put/get/invoke), link, etc.

"Binder Object" is a very general concept: it says nothing about the language used, how the
object was created (through the Package Manager or just manually by calling C++ new),
the process it lives in, etc.  It is simply something that has implemented IBinder,
however that may be.

What is the difference between "Binder object" and "a Binder"?  Any class that implements
IBinder is a Binder object; that class may implement IBinder multiple times for each of
the interfaces it supports, and each of those is "a Binder".  So a Binder object may have
muliple Binders.  For example, see BIndexedDataNode, a class that contains two
Binders: one for its INode interface, and one for its IIterable interface.

@subsection ComponentDefn Component

A @ref BinderObjectDefn that has been published in the Package Manager.  Components are
instantiated with SContext::New() or SContext::RemoteNew(), allowing both late binding to
the implementation and the instantiation of components in other languages and
processes.  This is in contrast to instantiating a Binder Object with the C++ operator
new, where you must link to a specific C++ implementation and instantiate the object in
your local process.

Components use a Java-style naming scheme, such as com.palmsource.apps.AddressBook.  Most
system components are in the org.openbinder.* namespace.

@subsection PackageDefn Package

A Package contains one or more @ref ComponentDefn implementations.
The key elements of a package are (1) the executable code implementing those
components, and (2) a "manifest file" that describes the
components implemented by that code.  The code of a package exports a function
called InstantiateComponent(), which is the factory for the components it
implements.  The @ref PackageManagerDefn scans through all of the manifest files in the
system to collect information about all of the available components,
and returns the information associated with a component as needed.

Note that unlike COM, component information is maintained statically.  There is
not a Registry-like entity that keeps track of information about the components;
instead, a component's information is constructed as needed directly from the
manifest file that is part of a package.

@subsection PackageManagerDefn Package Manager

The Package Manager is a sub-system of the Binder responsible for keeping
track of the available components and implementing the dynamic component
instantiation and management facilities.

There are actually two distinct parts to the Package Manager:

-# The Package Manager service, published in /packages in the @ref BinderNamespaceDefn,
is responsible for collecting package information and providing access to it through
the @ref BinderNamespaceDefn.  In particular, it presents under /packages/components
all of the available components and information associated with them that is needed
to load and instantiate them in a process.

-# The SPackage, SPackageSptr, SSharedObject, and BProcess classes are responsible
for loading package code into a process, managing its lifetime, providing access
to resources in a package, and instantiating objects from it.

Finally, the SContext::New() API brings these pieces together two provide
the Binder's formal component instantiation facility: it queries the Package Manager
service for information about the requested component, and then calls
IProcess::InstantiateComponent() to have that component loaded and instantied in
the desired process.

@subsection InterfaceDefn Interface

A well-defined set of methods, properties, and events that a @ref BinderDefn can implement.  These
are usually described in IDL (see <tt>interfaces/...</tt>), converted by the pidgen tool
to a C++ header and implementation.  In C++, instead of deriving directly from BBinder,
you will usually derive from some Bn* class generated by pidgen (such as BnDatum), which
gives you a BBinder that is configured to implement a particular interface.

@subsection InspectDefn Inspect

Every @ref InterfaceDefn has an associated @ref BinderDefn.  A @ref BinderObjectDefn that
implements multiple interfaces will thus have multiple Binders, one for each of its interfaces.
You cast between these Binders/Interfaces using the IBinder::Inspect() call.
@ref BinderInspect has more details on this process.

@subsection BinderNamespaceDefn Binder Namespace

The Binder Namespace is a hierarchical orginization of @ref BinderDefn objects.  It is
constructed using the @ref BinderDataModel interfaces.  Directories
implement INode, and may implement any other interfaces for additional capabilities
(almost always IIterable, very often ICatalog).  Leaf objects implement IDatum if they
contain a flat data stream (such as a file), but are not required to do so and often implement
other interfaces.

@subsection ContextDefn Context

Each @ref BinderObjectDefn is created in a "context", represented by the SContext
object.  The context is the thing that holds all of the global state the object
can access &mdash; services, settings, information about components it can instantiate,
etc.  The SContext you are running in is easily accessible through the
BBinder::Context() method.  For example, to find the Window Manager service, you
would write code within your Binder Object along the lines of
<em>Context().LookupService(SString("window"))</em>.

SContext is, in fact, just holding the root INode of a @ref BinderNamespaceDefn,
providing convenience functions for doing common operations with the namespace.
Components usually don't get an SContext of the @e root namespace &mdash; to enforce
security, the system makes additional namespaces that are derived from the root
namespace but with modifications such as "can not publish new services".

@subsection ServiceDefn Service

A previously instantiated @ref BinderDefn object that has been published under /services in the
@ref BinderNamespaceDefn.  These are usually instantiated by the boot script when the system
starts up, and exist forever.

You can basically think of them as singleton components, though the Binder does not
currently provide any explicit way to define singletons.  That is, there is no way to say
"this component is a singleton" so that every attempt to instantiate the component
returns the same object; instead, you must explicitly instantiate it and publish it in
the namespace up front, for others to discover there.

@subsection LinkDefn Link

A Link is an active data/event connection from one @ref BinderDefn to another.  It is
created with the IBinder::Link() method, expressing a binding going from the Binder
being called to the given target.  The source Binder uses BBinder::Push() to send
data through a link.

There are two kinds of links, data and events.  All @ref InterfaceDefn properties are also
data links, providing a data-flow mechanism.  This can be used, for example, to attach the
current value of some property
in one object to the value of some other propety in a differnt object; when the value
of the former changes, that new value will be propagated to the later.

Event links are created explicitly in IDL in the "event:" section.  Events are linked
to methods on a target, resulting in a method invocation when the event is pushed.

See @ref ShellLinks for an example of links using the Binder Shell.

*/

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/*!
@page BinderInspect Binder Inspect() Details

<center>\< @ref pidgen | @ref BinderKit | @ref StorageKit \></center>
<hr>

The IBinder::Inspect() API is the Binder's mechanism for performing 
interface casts.  (If you are familiar with COM, this is similar to the 
IUnknown::QueryInterface() method found there.)

Interfaces are identified by an <em>interface descriptor</em>.  This is a 
string SValue, where the string is a unique name of that interface.  We 
use Java-style naming conventions for interfaces to ensure they have 
unique names; all system-level interfaces are in the org.openbinder.* 
namespace, and all of our application-level interfaces should be in the 
com.palmsource.* namespace.

You will not normally write these strings yourself.  Instead, pidgen 
generates them for you from the IDL file.  Given an interface IFoo, 
pidgen will generate a method IFoo::Descriptor() that returns the 
descriptor for that interface.

- @ref CastingInterfaces
- @ref ImplementingInterfaceCasts
- @ref RestrictingInterfaceAccess

@section CastingInterfaces Casting Interfaces

When working in C++, you will not normally call Inspect() yourself.  
Instead, you will use the template function 
interface_cast<INTERFACE>(), which will call Inspect() for you and 
return the result as the requested C++ interface.  For example:

@code
SValue v;
sptr<IBinder> b;

sptr<IFoo> foo(interface_cast<IFoo>(v));
sptr<IFoo> foo(interface_cast<IFoo>(b));
@endcode

You can also call Inspect() yourself directly for special situations.  
The API is:

@code
virtual SValue
Inspect(const sptr<IBinder>& caller,
        const SValue &which,
        uint32_t flags = 0);
@endcode

The @a caller is the IBinder you are calling Inspect() on, @a which
is the interface descriptor(s) you desire, and @a flags is currently 
not used (must always be 0).

For example, here is a call to inspect to get the IView interface 
binder:

@code
sptr<IBinder> origBinder;
sptr<IBinder> newBinder(origBinder->Inspect(origBinder, IView::Descriptor()));
@endcode

Like other places where we use SValue operations, the @a which
parameter can also be a set of SValue mappings to retrieve multiple 
interfaces in one call, or B_WILD_VALUE to retrieve all interfaces.  
(Note that currently due to IPC limitations, in some cases 
B_WILD_VALUE will fail because it returns too many IBinder objects.)

Here is an example of retrieving two interfaces:

@code
sptr<IBinder> origBinder;

SValue which(IView::Descriptor(), IView::Descriptor());
which.JoinItem(IViewManager::Descriptor(), IViewManager::Descriptor());
SValue result = origBinder->Inspect(origBinder, which);

sptr<IBinder> viewBinder(result[IView::Descriptor()].AsBinder());
sptr<IBinder> viewManagerBinder(result[IViewManager::Descriptor()].AsBinder());
@endcode

Note that for C++ programming this is not a very useful technique, 
since to get the C++ interface you still need to do 
interface_cast<>(), which will itself do its own IBinder::Inspect() call.  
You can get around this using IFoo::AsInterfaceNoInspect(), but unless 
there is some very clear performance critical section for which it 
makes sense, you are best off just sticking with interface_cast<>().

@section ImplementingInterfaceCasts Implementing Interface Casts

With a binder object of only one interface, there is usually no need 
for you to do anything.  Pidgen will generate for you a standard 
implementation of Inspect() in the BnFoo class, which returns the 
interface being implemented.  By deriving from BnFoo as normal, you 
will get this implementation and everything will work as expected.

When you implement an object with multiple interfaces, you will often 
need to allow the user to cast between them.  By default you are not 
able to cast from one interface to another.  To allow this, you need to 
re-implement Inspect() so that calling the method on one IBinder will 
also return the information for the other interfaces.

@par
<em>C++ background: when writing an object with multiple interfaces, 
you have multiple BBinder objects (via multiple inheritance), one for 
each of those interfaces.  Each of these BBinder objects has its own 
Inspect() method.  If the deriving class does not itself implement 
Inspect(), then they remain separate, and a call to Inspect() on 
one will just use that one's original implementation.</em>

To implement Inspect(), all you need to do is merge the inspect 
result of all of your base classes.  For example, here is the 
implementation from the Application Manager service, which implements 
both IApplication and IApplicationManager:

@code
SValue
BApplicationManager::Inspect(
	const sptr<IBinder>& caller, const SValue &which, uint32_t flags)
{
	SValue result(BnApplication::Inspect(caller, which, flags));
	result.Join(BnApplicationManager::Inspect(caller, which, flags));
	return result;
}
@endcode

@section RestrictingInterfaceAccess Restricting Interface Access

Sometimes you want to restrict the ways the clients can cast between 
interfaces.  For example, in the view hierarchy, a layout manager 
implements the interfaces IView, IViewParent, and IViewManager.  We 
want to set these casting policies:

- IViewParent can only cast to itself (IViewParent).
- IViewManager can cast to either itself or IViewParent.
- IView can cast to itself, IViewManager, and IViewParent.

This essentially defines a hierarchy of capabilities &mdash; holding an 
IViewParent gives you no other capabilies, while an IView gives you 
access to all other view capabilities.

In order to accomplish this, we need to look at the @a caller
parameter of Inspect() to determine which interface the call is
coming from, and use that to decide which of the base classes we will 
call.  Here is an example implementation of BViewGroup::Inspect()
that implements the casting policy described above:

@code
SValue
BViewGroup::Inspect(
	const sptr<IBinder>& caller, const SValue& v, uint32_t flags)
{
	// Figure out which interfaces to inspect.
	uint32_t which = 0;
	if (caller == (BnViewParent*)this) which |= 1;
	if (caller == (BnViewManager*)this) which |= 2|1;
	if (caller == (BnView*)this) which |= 4|2|1;

	SValue result;
	if (which&1) result.Join(BnViewParent::Inspect(caller, v, flags));
	if (which&2) result.Join(BnViewManager::Inspect(caller, v, flags));
	if (which&4) result.Join(BnView::Inspect(caller, v, flags));

	return result;
}
@endcode

One thing it is very important to note is that this implementation of 
Inspect() will only return results when the @a caller is one of the 
interfaces it provides.  This is different than the default 
implementation, which will return the interface regardless of the 
caller &mdash; allowing casts to that interface from any other interface.

Because of this difference in semantics, when you write a subclass that 
is implementing this kind of Inspect(), you should clearly document in 
the header file that you are doing and what casting rules it uses.  
Any subclass of your own class that wants to implement a new interface 
needs to know about your Inspect() implementation, so that it can 
call it correctly to implement its own desired casts.

For example, BListView derives from BViewGroup and implements a new 
interface, IListView.  It would like to allow casts from IListView to 
any of the BViewGroup interfaces.  Just calling 
BViewGroup::Inspect() will not do this, because our implementation 
only allows casts from its known interfaces.  Thus the implementation 
of BListView::Inspect() will look something like this:

@code
SValue
BListView::Inspect(
	const sptr<IBinder>& caller, const SValue &which, uint32_t flags)
{
	SValue result;

	// IListView and IView get access to the IListView interface.
	if (caller == (BnListView*)this || caller == (BnView*)this)
		result.Join(BnListView::Inspect(caller, which, flags));

	// If inspecting from IListView, we give full access to the object &mdash;
	// that is, the same permissions as if inspecting from IView.
	if (caller == (BnListView*)this)
		result.Join(BViewGroup::Inspect((BnView*)this, which, flags));
	else
		result.Join(BViewGroup::Inspect(caller, which, flags));

	return result;
}
@endcode

The trick here is that if the caller is inspecting from IListView, we 
call up to BViewGroup::Inspect() with the caller changed to the IView
binder, so that it will return all interfaces.

*/
