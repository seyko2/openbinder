/*!
@page BinderRecipes Binder Recipes

<div class="header">
<center>\< @ref BinderKit | @ref BinderKit | @ref BinderShellTutorial \></center>
<hr>
</div>

This page provides simple recipes for common things you will
do with the Binder.  These serve as a good first look at what
it is like to use the Binder's C++ APIs.

-# @ref CallBinderService
-# @ref WritingBinderObject
-# @ref GivingBinderObject
-# @ref CreatingComponent

@section CallBinderService Calling a Binder Service

The first thing you will usually want to do is call on to
an existing Binder @ref ServiceDefn.  In this example, we will use
the Informant service to broadcast a message to others who
may be interested.

@subsection UsingBinderInterface Using a Binder Interface

@code
#include <services/Informant.h>
@endcode

Binder interfaces are described through IDL, and the @ref pidgen
tool used used to convert them to a C++ API.  To use an
interface, you will need to include the C++ header that pidgen
generated.

The Informant interface is located at "interfaces/services/IInformant.idl",
so the header you need to include is "services/IInformant.h".

@subsection ContactService Contact the Service

@code
sptr<IBinder> informantBinder
	= Context().LookupService(SString("informant"));
@endcode

Every Binder object is associated with an SContext, which
is your global connection with the rest of the system. All
Binder objects have BBinder as a base class, so
from the code inside your object you can retrieve your context
through the BBinder::Context() API.

The SContext class includes various methods for making use
of the information in the context.  Here, we use
SContext::LookupService() to retrieve, by name, a reference
on a service that was previously published.  The object
we get back is a pointer to an IBinder, which is the abstract
interface to any @ref BinderObjectDefn.

Note the use of <code>sptr<IBinder></code> instead of a raw
C++ pointer.  You should always use @ref sptr<> (or @ref wptr<>)
when working with Binder objects, because they are reference counted
and these classes will automatically hold a reference on the
object for you.

@subsection GetBinderInterface Get a Binder Interface

@code
sptr<IInformant> informant
	= interface_cast<IInformant>(informantBinder);
@endcode

The @ref interface_cast<> method finds an IDL @ref InterfaceDefn associated
with a Binder object.  In this case we are looking for the IInformant
interface.  This is conceptually the same as
QueryInterface() in COM.  In the Binder, it asks the object
whether it supports that interface and for the IBinder
associated with it, and returns the C++ interface requested.
If the object is not in the local process, a proxy is created
for it and returned.

The result of @ref interface_cast<> is a @ref sptr<> to the requested
C++ interface or NULL if the object does ot implement that
interface.

@subsection CallBinderObject Call the Binder Object

@code
if (informant != NULL) {
	informant->Inform(	SValue::String("myMessage"),
						SValue::String("myData") );
}
@endcode

We now have a fully functional C++ interface.  It is pointing
to the actual target object if this is a local C++ class, or a proxy
object to the implementation  if it is somewhere else.

In either case, we can make calls on the interface just like
on a local C++ class.  The object will stay around as
long as we hold a sptr<> referencing the interface.

In this case, we will call the IInformant::Inform() method
to broadcast a notification.  The two arguments supply
the information to be broadcast, which is a Binder type called
an SValue.  This is like a variant in COM or GValue in GLib: it
carries a type as well as data, for dynamic typing.  Here we are
supplying two SValues holding string data, giving the message name and
data to send.

@subsection AlternativeContactService Alternative to Contact the Service

Sometimes you are not a Binder object, but have some other
code that needs to make use of the Binder.  In this case,
you need a way to get an SContext object through which you
can find other Binder objects.  This can be done through
the SContext::UserContext() API.

@code
SContext context = SContext::UserContext();
	
sptr<IBinder> informantBinder =
	context.LookupService(SString("informant"));
@endcode

Note that SContext::UserContext() should only be used when
you don't otherwise have an SContext available through
BBinder or some other facility.  You will normally want
to use the SContext that was provided to you, since your
caller may have customized it in some way.

@subsection CallExample Complete Example: Calling a Binder Service

To review, here is our complete example of calling a Binder service:

@code
#include <services/Informant.h>

sptr<IBinder> informantBinder
	= Context().LookupService(SString("informant"));

sptr<IInformant> informant
	= interface_cast<IInformant>(informantBinder);

if (informant != NULL) {
	informant->Inform(	SValue::String("myMessage"),
						SValue::String("myData") );
}
@endcode

@section WritingBinderObject Writing a Binder Object

We are now going to look at what you do to write your
own Binder object that others can call.

@subsection ImplementingBinderInterface Implementing a Binder Interface

@code
#include <services/IInformant.h>
@endcode

The interface we are going to implement is IInformed, a part
of the Informant service, and so defined in the same file
"services/IInformant.h" we saw above.  The IInformed interface
is the recipient of a message broadcast through IInformant.

@par
<i>Note that another common way to broadcast
information is through a Binder @ref LinkDefn, which allows you to declare
properties and events on an interface that can be monitored by
other objects.  See @ref ShellLinks for an example.</i>

@subsection DefineObjectClass Define the Object Class

@code
class MyWatcher : public BnInformed, public SPackageSptr
{
public:
@endcode

MyWatcher is the class we are implementing.

The BnInformed class is generated by @ref pidgen from the IInformed
interface in the <tt>IInformant.idl</tt> file.  BnInformed is a subclass
of IInformed, which provides the basic mechanism for implementing
a concrete IInformed class.  In particular, it includes a BBinder
object: this gives you an IBinder API that can be used by
other languages and processes, and BnInformed implements the
unmarshalling code to let those clients call your IInformed
implementation.

The SPackageSptr class is a special part of the @ref PackageManagerDefn.
It is essentially a "strong pointer" to your code package,
allowing it to monitor how your package is being used to ensure
that your code stays loaded as long as it is needed.  This also
gives your implementation access to your associated SPackage
object, through which you can retrieve resources such as strings
and bitmaps.

@subsection ImplementConstructor Implement Constructor
@code
	MyWatcher(const SContext& context)
		: BnInformed(context)
	{
	}
@endcode

This is a typical implementation of a Binder object's constructor.
It takes the SContext that the object is being instantiated in, and
hands that off to its base classes.  Doing this allows it to later
use BBinder::Context() to retrieve that context.

@subsection ImplementIInformedAPI Implement the IInformed API

@code
	void OnInform(	const SValue& information,
					const SValue& cookie, const SValue& key)
	{
@endcode

IInformed has a single method, IInformed::OnInform().  All methods
and properties on an interface base class are pure virtuals, so you
must implement them to have a concrete class that can be instantiated.

We see SValue, our typed data container, here again as the arguments
this method receives.  In addition to simple typed data, an SValue
can contain complex data structures and mappings of other SValues.
This is a convenient facility to propagate whatever data we would
like through the informant.

@subsection ImplementOnInform Implement OnInform() Method

@code
		bout << "Got informed: " << information << endl;
	}
};
@endcode

Your implementation of an interface method can do whatever it wants,
just like any other C++ class.  In the implementation here, we are
going to use a Binder formatted text output stream to print the
arguments we received.

The "bout" object is an ITextOutput stream that writes to the
standard output stream.  Most Binder objects (SValue here, and also
SString, SMessage, etc) can be written as text to a text output
stream like "bout".  It is basically like the standard C++ output
streams, but has some additional features for indentation, tagging
lines, and managing the output of multiple concurrent threads.
(See @ref TEXT_OUTPUT_FORMAT for runtime options to control text
stream output.)

@subsection WritingExample Complete Example: Writing a Binder Object

To review, here is our complete example of writing a Binder object

@code
#include <services/Informant.h>

class MyWatcher : public BnInformed, public SPackageSptr
{
public:

	MyWatcher(const SContext& context)
		: BnInformed(context)
	{
	}

	void OnInform(	const SValue& information,
					const SValue& cookie, const SValue& key)
	{
		bout << "Got informed: " << information << endl;
	}
};
@endcode

@section GivingBinderObject Giving a Binder Object to a Service

A key aspect of the Binder is transfering object references between
interfaces.  When those interfaces are remote, this is how you
set up communication paths between languages and processes &mdash; even
though, to the client, all of the work necessary to do this is invisible.

@subsection GetInformantService Get the Informant Service

@code
sptr<IInformant>  informant =
	interface_cast<IInformant>(
		Context().LookupService(SString("informant")));
@endcode

Just like we saw in the first example, the first thing we need
to do is retrieve the informant service.  Note that error checking
is only needed at the end &mdash; @ref interface_cast<> will gracefully
propagate errors.  Also, none of the Binder APIs currently use
exceptions to propagate error conditions.

@subsection InstantiateBinderObject Instantiate New Binder Object

@code
sptr<IBinder> informed = new MyWatcher;
@endcode

Here we create a new instance of the MyWatcher class that was
previously implemented.

Note again the @ref sptr<> smart pointer class.  It will hold a reference
on the object, and reference counting is designed so that we can
directly assign a newly created object to a sptr<> without ever
having to worry about reference counts.

The pointer we have her could just as well have been a sptr<MyWatcher>,
but all we need later on is the IBinder class so that is what we
decided to use.

@subsection GiveObjectInformant Give Object to Informant

@code
if (informant != NULL && informed != NULL)
{
	informant->RegisterForCallback(
		SValue::String("myMessage"), informed,
		SValue::String("OnInform"));
}
@endcode

After doing the appropriate error checking, we will call the informant's
IInformant::RegisterForCallback() API to give our object to it.  At this
point the informant now has a reference on MyWatcher, and if they are
in different processes a remote connection has been established.  The
MyWatcher instance we created will remain around as long as the
informant holds a reference on it.

The SValue::String("OnInform") parameter we pass in specifies the method for
the informant to call.  The informant uses the Binder's scripting
protocol (which is a basic capability of any Binder object) to call any
method on the IBinder that was given to it.  When it does this, your
OnInform() method will execute just as it would if a C++ program had
called it directly.

@subsection GivingExample Complete Example: Giving a Binder Object to a Service

To review, here is our complete example of giving a Binder object
to a service.

@code
#include <services/Informant.h>

sptr<IInformant>  informant =
	interface_cast<IInformant>(
		Context().LookupService(SString("informant")));

sptr<IBinder> informed = new MyWatcher;

if (informant != NULL && informed != NULL)
{
	informant->RegisterForCallback(
		SValue::String("myMessage"), informed,
		SValue::String("OnInform"));
}
@endcode

@section CreatingComponent Creating a Component

Let's now turn the Binder object we wrote (MyWatcher) into a Binder
@ref ComponentDefn.

A component is a Binder object that is published so that others can
instantiate it, without having to link to its implementation.  The
@ref PackageManagerDefn keeps track of all Binder components and
the information needed to instantiate them.
	
In addition to the MyWatcher implementation, there are two more things
you need to add to turn it into a full Binder component:
	
-# A <tt>Manifest.xml</tt> file, an XML document that tells the package
	manager about your component.
-# An <tt>InstantiateComponent()</tt> function, the factory function that
	generates instances of your component.

In addition, we will need a Makefile that puts all of this stuff
together into the appropriate package structure.

@subsection MyWatcherManifest MyWatcher's Manifest

The manifest for our component will be very simple:

@verbatim
<manifest>
	<component>
		<interface name="org.openbinder.services.IInformed" />
	</component>
</manifest>
@endverbatim
	
This manifest says that there is a single component in the
package, which implements the interface <tt>org.openbinder.services.IInformed</tt>.
The name used to instantiate the component will simply be the package
name itself, which we will define later in our Makefile.

@subsection FactoryFunction The Factory Function

@code
#include <support/InstantiateComponent.h>

sptr<IBinder>
InstantiateComponent(	const SString& component,
						const SContext& context,
						const SValue &args)
{
@endcode

This is the function exported from your package, which the @ref PackageManagerDefn
calls when it needs to make a new instance of one of your components.  Note
that we include the "support/InstantiateComponent.h" header, which declares the
<tt>InstantiateComponent()</tt> function so it will be properly exported.

The @a component argument you receive is the @em local name of the desired component,
that is the full component name with the package name at the beginning removed.

The @a context argument is the Binder context the component is being instantiated in.

The @a args are data to be supplied to the component constructor.  That is,
it will contain an SValue of key/value mapping pairs supplying the arguments.

@code
	if (component == "")
		return static_cast<BnInformed*>(new MyWatcher(context));
	return NULL;
}
@endcode

Your implementation of <tt>InstantiateComponent()</tt> will need to look at the
@a component argument to determine which component to instantiate.  Here,
we have only one component we have implemented.  The component's name
is the same as the package name, so the @a component argument here for
that component will be "" &mdash; that is, there is no suffix beyond the
base package name for the component.

The standard C++ new operator is used to make a new instance of the
component.  Note that we pass the @a context to the MyWatcher class's
constructor so it can later retrieve it through BBinder::Context()
if needed.

The cast here is used to select the default interface that is returned.
In this case it is not actually needed, because we are only implementing
a single interface.

Note @ref sptr<> again here, used implicitly due to the function return
type, taking care of all reference counts for us.

If the requested component is not one we implement (which shouldn't
happen unless our manifest file is incorrect), we simply return NULL.

@subsection MyWatcherMakefile MyWatcher's Makefile

@verbatim
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

BASE_PATH:= $(LOCAL_PATH)
PACKAGE_NAMESPACE:= org.openbinder.samples
PACKAGE_LEAF:= MyWatcher
SRC_FILES:= \
	MyWatcher.cpp

include $(BUILD_PACKAGE)
@endverbatim

This is an example of a Makefile that will use the OpenBinder build
system to build the MyWatcher component into a Binder package.  Note
in particular the PACKAGE_NAMESPACE and PACKAGE_LEAF variables, which
together set the base page name &mdash; in this case,
<tt>org.openbinder.samples.MyWatcher</tt>.

This Makefile will result in the creation of a directory
<tt>build/packages/org.openbinder.samples.MyWatcher</tt> that contains:

- @b MyWatcher.so &mdash; the code implementing the MyWatcher package.
- @b Manifest.xml &mdash; the Manifest.xml file described previously.

The Package Manager uses the directory name to infer the base name
for your package.

@subsection InstantiatingMyWatcherComponent Instantiating a MyWatcher Component

Now that we have a component, we can use the SContext::New() API to
create an instance of it.

@code
sptr<IBinder>  informedBinder =
	Context().New(SString("org.openbinder.samples.MyWatcher"));
	
sptr<IInformed> informed =
	interface_cast<IInformed>(informedBinder);
@endcode

The result of SContext::New() is an IBinder, the Binder object of the
newly instantiated component, which you can then cast to the desired
interface.

The above code can be done from any process.  The component implementation
will be loaded into the local process, if needed.  In addition, the
component can be implemented in another language or the system can decide
it should be instantiated in a different process; in either case, proxies for IPC
and/or marshalling will be created as needed, and you have no need to be
aware that this has happened.

@subsection ObjectsComponents Objects vs. Components

Compare the previous code for directly instantiating a MyWatcher object:

@code
sptr<IBinder> informed = new MyWatcher;
@endcode

Using SContext::New() gives you the same thing: it works and behaves just
like a local object.  However, you now don't have to know about the
component implementation up-front (linking directly to a specific library
implementing it), and SContext::New() works across processes and languages.

Note, however, that the following code is not possible with SContext::New():

@code
sptr<MyWatcher> informed = new MyWatcher;
@endcode

In particular, though in some specific cases, the following may
work (when the instantiated component is in the same process and
language), this should not be done:

@code
sptr<MyWatcher> informed = dynamic_cast<MyWatcher*>(
	Context().New(SString("org.openbinder.samples.MyWatcher")).ptr();
@endcode

*/
