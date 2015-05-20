/*!
@page pidgen pidgen

<div class="header">
<center>\< @ref BinderIPCMechanism | @ref BinderKit | @ref BinderInspect \></center>
<hr>
</div>

The Palm Interface Definition Generator (pidgen) is a tool that
makes it easier to generate Binder interfaces.  Pidgen parses
interface files written in Interface Definition Language (IDL) and
generates the necessary code for objects declared in an interface to
travel across processes. Pidgen does not write your code for you;
you still have to write the actual implementation (that is, you have
to write the BClass).

Pidgen parses IDL, which is similar to COM and CORBA's IDL.
IDL shares syntactical similarities with C++, but IDL is not the same
as C++. The marshaling and unmarshaling code that pidgen
generates is C++. Note that pidgen does not parse C++, it simply
generates C++.

Here is what is automatically generated when pidgen parses an
IDL file:

- A @b .h file containing declarations for the objects in an
  interface. Essentially, the .h file is a header in C++.
- A @b .cpp file containing no implementation code, only
  marshalling/unmarshalling code to help objects travel across
  processes. The code in the .cpp file generates hooks for
  methods and properties (to direct calls to the proper local
  entity), creates remote proxies so that non-local calls can be
  initiated, and generates some local transaction code (the
  BnClass, from which the BClass can be derived). In the .cpp
  file, SValues are used to package objects across processes.

-# @ref WritingIDLFiles
  -# @ref Functions
  -# @ref Events
  -# @ref Properties
  -# @ref Attributes
    -# @ref InOutInout
    -# @ref local
    -# @ref readonly
    -# @ref optional
    -# @ref weak
    -# @ref reserved
-# @ref TypesInPidgen
-# @ref SequenceArtguments
-# @ref LinkConvenienceMethods
-# @ref MorePidgenFeatures
-# @ref InvokingPidgen
  -# @ref PidgenCommandLine
  -# @ref PidgenJam
-# @ref ExampleIDLFiles
  -# @ref IDatumIDL
  -# @ref INodeIDL
  -# @ref IIteratorIDL

@section WritingIDLFiles Writing IDL Files

IDL files describe interfaces and the functions and properties
associated with those interfaces.

The concept of an interface is central to IDL. An interface file is a set
of definitions that are abstract representations of the underlying
system. Developers can use these definitions to safely send objects
declared in the interface across processes, without having to know
how the underlying system is implemented. It is the abstract
representation of the parts of your system’s underlying
implementation that is open to modification by third-party
developers.

The purpose of using pidgen is to produce safe, proven code that
enables objects defined in an interface to be used across processes.

To specify an interface, declare it in an IDL file as follows:

@code
interface xyz
@endcode

You can declare multiple interfaces in one IDL file, but the
convention is to declare at least interface IXyz in the IDL file named
IXyz.idl. For example, you would declare interface IWidget in
IWidget.idl.

Usually, declarations of interfaces are enclosed in namespaces to
help pidgen map its C++ code output to the correct locations. For
example, an interface that lives in the app kit is declared like this:

@code
namespace palmos {
namespace widget {
	interface IWidget {
		...
	}
}}
@endcode

By convention, interfaces names begin with the letter I; for example,
IRender or ICommand.

Two kinds of interface declarations are possible in IDL:

- Regular interface declaration with functions, events, and
  properties.
- Forward declared interface. Forward declared interfaces let
  pidgen know of the existence of an interface that is not
  specified in the current IDL file. To forward declare an
  interface, you declare an interface without functions and
  properties. For example:
@code
namespace palmos {
namespace support {
	interface IByteOutput
	interface IByteInput
}}
@endcode

You can also import interfaces from other IDL files and make the
imported interfaces visible to the current interface. Both regular and
forward declared interfaces can be imported. Imports are a good
way to tell pidgen about specific custom types needed by the
current interface.

To use imports, specify where the imported IDL file lives when
pidgen is invoked with the -I flag. For example:

@code
pidgen IWidget.idl -I C:\PDK\interfaces\widget
@endcode

For detailed information on invoking PIDGen, see @ref InvokingPidgen.

Then in IWidget.idl, include the following code:

@code
import <render/Font.idl>
@endcode

When pidgen sees the import directive, it looks for Font.idl in
the directory specified by the -I flag. More than one IDL can be
specified, and pidgen will search all of them until it finds a match
or exhausts all the directory options.

Any interfaces declared in, or imported by, imported interfaces are
then available for use by the interface you’re declaring.

So far, you have the following code in Widget.idl:

@code
import <render/Font.idl>
namespace palmos {
namespace widget{
	interface IWidget {
		...
	}
}}
@endcode

We can now populate IWidget with functions, events, and
properties.

@subsection Functions

Functions are declared C++ style, preceded by the word "methods".

@code
interface IWidget
{
methods:
	void RequestGestures(uint8_t gestures);
	void DefaultFont(SString font);
	void Poked();
	void Leaned();
	...
}
@endcode

@subsection Events

Events are declared C++ style as well, preceded by the word
"events".

@code
interface IWidget
{
	...
events:
	void WidgetPoked(IWidget widget);
	void WidgetLeaned(IWidget widget);
	...
}
@endcode

Events are methods that are called when specific things take place.
For each event in your interface, pidgen generates a corresponding
push method on its BnClass.  For the example above, these would be
BnWidget::PushPoked() and BnWidget::PushLeaned().  When implementing
this interface, you can call these push functions whenever you would
like to send out the event.

Clients that are interested in being notified of the event link to the
event.

@code
widget->Link(watcher, SValue("WidgetPoked", "HandleWidgetPoked");
@endcode

The client also provides the HandleWidgetPoked() method,
which is then called whenever the WidgetPoked() method is
called by the widget object.

See @ref LinkConvenienceMethods for more information about linking.

@subsection Properties

Properties are similarly preceded by the word "properties" and
declared in the style of member variable declaration for C++ classes.

@code
interface IWidget
{
	...
properties:
	[readonly]SValue	key;
	SString				label;
	SValue				fontDescription;
	[readonly]BFont		font;
	bool				enabled;
	bool				hasFocus;
	int32_t				interactionState;
	[readonly]uint8_t	gestureFlags;
	SValue				value;
	...
}
@endcode

Pidgen also generates a Push function for all properties on an
interface, very much like it does for @ref Events.  You should
use these to tell others when your object's state changes, so
they can link to your properties to monitor their value.  This
is an intrinsic part of the Binder's data-flow mechanism.

@subsection Attributes

There are several attributes available that provide additional
information when describing a function, property, or event.

@subsubsection InOutInout in, out, inout

The @e in attribute identifies a parameter to a function or event as an
input, while the @e out attribute identifies a parameter as an output.

The @e inout attribute indicates that a parameter to a function or
event is both an input and an output.

Here is an example of using attributes when declaring a method.

@code
SValue SetProperty([in]SValue key, [inout]SValue value);
@endcode

If @e in or @e out is not specified for a parameter, its direction is assumed
to be @e in.

@subsubsection local

The @a local attribute can be used if you want to create an interface
that uses the Binder object model, but doesn’t care about crossprocess
or multi-language support.

You can specify the local attribute on either the entire interface or
on specific methods within the interface.

Here is an example of defining an interface that is entirely local.

@code
[local] interface IColor
{
	...
}
@endcode

Here is an example of defining a specific method that is local.

@code
interface IColorModel
{
methods:
	...
	[local] int32_t LocalLookupColor(int32_t rgb);
	...
}
@endcode

Local interfaces or methods generate less code, but have restrictions
on how they can be used:

- Local interfaces or methods cannot be called from a different
  process.
- Local interfaces or methods cannot be called from a different
  language.
- Local interfaces or methods cannot be the target of Binder
  linking.

@subsubsection readonly

The @e readonly attribute specifies that a given property is read-only.

@subsubsection optional

The @a optional attribute specifies that a method argument is not
required.  If that argument is not supplied, a default value for
the argument's type will be used.  For numeric types (integers and
floats), the default value is zero; for SValue it is B_UNDEFINED_VALUE;
for strings it is ""; for interfaces it is NULL.

@subsubsection weak

The weak attribute is used when declaring a property, or a method
or event that needs to accept a weak reference to an object as a
parameter. The IDL to implement this is demonstrated here.

@code
properties:
	IView view;
	[weak] IViewParent parent;
methods:
	void GiveAView([in] IView view);
	void GiveAParent([in weak] IViewParent parent);
@endcode

The resulting C++ code is shown below.

@code
virtual sptr<IView> View() const;
virtual void SetView(const sptr<IView>& value);
virtual wptr<IViewParent> Parent() const;
virtual void SetParent(const wptr<IViewParent>& value);

virtual void GiveAView(const sptr<IView>& value);
virtual void GiveAParent(const wptr<IViewParent>& value);
@endcode

The C++ binding passes all interfaces as smart pointers; the default
is to use a strong reference, so if you need to use a weak reference,
be sure to specify the weak attribute.

@subsubsection reserved

The @e reserved attribute can be applied to either functions or
properties. It provides the ability to create reserved slots in the
implementation so that future versions of the interface can add new
methods or properties without forcing clients to be recompiled.
Using the reserved attribute requires some planning—you need
to estimate how much future growth to expect. If the interface has a
limited number of properties and methods, and little potential for
growth, then you may only need a handful of reserved properties
and methods. More complex interfaces may benefit from a larger
number of reserved slots.

@note The reserved slot feature has not been fully tested.

To add reserved slots to your interface, you must first decide how
many reserved methods and properties you need. Then, all you
need to do is add code similar to that seen here.

@code
[reserved] int32_t _reservedProperty1;
...
[reserved] int32_t _reservedPropertyN;

[reserved] status_t _ReservedMethod1;
...
[reserved] status_t _ReservedMethodM;
@endcode

Reserved properties and functions use the same syntax as normal
properties and functions, but simply include the reserved
attribute. There are no rules for the actual signatures of the reserved
properties and methods, but the convention is to follow the example
shown above.

When you decide to add a new method or property to your
interface, you do so by replacing one of the reserved items with
your new property or method.

An important thing to note: if the new property you’re creating is a
readonly property, there’s an extra step to take. That’s because
reserved properties get slots automatically reserved for their getter
and setter methods, and read-only properties don’t need a setter
method. So, to maintain binary compatibility, you need to do
something like what’s shown here.

@code
[readonly] IFoo myFooProperty;
[reserved readonly] int32_t _reservedProperty1;
@endcode

Since the reserved property is read-only, it saves one method slot,
thereby restoring binary compatibility.

@section TypesInPidgen Types in Pidgen

Pidgen recognizes and marshals several standard types:

@verbatim
bool   char    string
short  long    double   float
int8   int16   int32    int64
uint8  uint16  uint32   uint64
size_t ssize_t status_t bigtime_t off_t
SValue SString
@endverbatim

In addition, interface types are converted to sptr<> and wptr<>
smart pointers, as described in the @ref weak attribute.

If you want to marshal types other the ones listed, the available
options are:

- @b typedefs – Typedefs can be declared the way they are declared
  in C++, but only in the interface where they are needed.
@code
interface ISurface
{
	typedef uint64 pixel_format;
properties:
	[readonly]pixel_format arg
	...
}
@endcode

- @b type – Pidgen doesn’t have to understand a custom type in
order to marshal it, because SValues are used to send things
cross-process. If the type you need has translators to SValue
(specifically, type.ASValue() converts a type it into an
SValue, and type.SetTo(SValue) converts a type from an
SValue) pidgen can marshal type across processes.
@par
  The way to declare type is
@code
type SMessage {
methods:
	SValue ASValue();
	SMessage SMessage(SValue o);
}
@endcode

@section SequenceArtguments Sequence Arguments

Pidgen supports sequence arguments to methods. A sequence is a
one-dimensional array; the term comes from CORBA IDL. A
sequence is mapped to an SVector in C++.

An @e out sequence will be completely overwritten by the method,
while an @e inout sequence can be modified as desired by the
method. Sequences specified as an @e in parameter are passed by
reference, such as "<CODE>const SVector<> &</CODE>").
Sequences may also be returned, however, returning a sequence requires a
copy constructor, so using an @e out parameter is more efficient.

A sequence must be named by a typedef before it can be used, as
demonstrated here.

@code
	typedef sequence<int32_t> CategoryList;
method:
	void GetCategories([out] CategoryList list);
@endcode

The above IDL creates a typedef and member function in
the resulting interface class as seen here.

@code
typedef SVector<int32_t> CategoryList;
void GetCategories(CategoryList *list);
@endcode

The typedef is scoped within the interface class, so any external
usage of the typedef must be qualified, using syntax such as
<CODE>ClassName::CategoryList</CODE>.

When using a sequence, <CODE>SVector::AsValue()</CODE> and
<CODE>SVector::SetFromValue()</CODE> are called. These methods, in turn,
call two helper functions. The helper functions are specialized for all
the basic types. However, for custom types, you can use one of three
helper macros to define the needed functions:

- Use B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS if
  your type is of fixed size and you can treat it as a simple
  stream of bits.
- Use B_IMPLEMENT_SFLATTENABLE_FLATTEN_FUNCS if
  your type is derived from SFlattenable.
- Use B_IMPLEMENT_FLATTEN_FUNCS if your type
  implements <CODE>TYPE::AsValue()</CODE> and
  <CODE>TYPE(const SValue &)</CODE>.

@note At this time, the SFlattenable class is not available for
licensee use.

@section LinkConvenienceMethods Link Convenience Methods

Pidgen generates convenience methods to make linking easier.
Much of the time, when you create a link, it’s necessary to perform
casting to get to the right Binder object. The link convenience
methods do this for you. For example, you can replace the following
two lines:

@code
sptr<IBinder> menuBinder = m_menu->IMenu::AsBinder();
menuBinder->Link(this->AsBinder(), ...);
@endcode

with the single link convenience method call shown below.

@code
m_menu->LinkMenu(this->AsBinder(), ...);
@endcode

@section MorePidgenFeatures More Pidgen Features

Other PIDGen features include:

- <B>Using directives</B>. If necessary, you can declare usage of
  additional namespaces within interfaces:
@code
interface ISurface
{
	using namespace palmos::render;
	...
}
@endcode

- <B>Enums and structs</B>. Pidgen cannot understand enumerated
  types or structs, unless translators are provided to and from
  SValues (specifically, <CODE>type.ASValue()</CODE> converts a type it
  into an SValue, and <CODE>type.SetTo(SValue)</CODE> converts a type
  from an SValue). However, pidgen can include your enums
  or structs in the generated header if they are declared within
  the IDL file.

@section InvokingPidgen Invoking Pidgen

You can invoke pidgen manually from the command line, or set up
your Palm OS Cobalt integrated build environment so that the Jam
build tool invokes PIDGen automatically.

@subsection PidgenCommandLine Invoking Pidgen from the Command Line

To invoke pidgen from the command line, go to /PDK/tools,
where you will find pidgen.exe. Call pidgen as follows:

@code
pidgen idlfilename
@endcode

where @a idlfilename is the name of your IDL file.

You can type -h to see the list of flags available. Flags include:
- <CODE>import path | -I path</CODE><br>
  Adds @a path to the list of import directories. This flag is
  required when using imports in an interface.
- <CODE>output-dir path | -O path</CODE><br>
  Specifies output directory for generated files. The default is
  the directory where the IDL file is located.
- <CODE>output-header-dir path | -S path</CODE><br>
  The output directory for generated header files. When used
  in conjunction with -O, header files go in the output-header
  directory and .cpp files go in the -O directory. The default
  directory is the directory where the IDL file is located.

@note If pidgen is called with more than one IDL file specified,
files will run in sequence, all using the same set of flags.

@subsection PidgenJam Letting Jam Invoke Pidgen Automatically

You can set up the Palm OS Cobalt build system to invoke pidgen
automatically. To do this, edit or create the appropriate Jamfile. For
detailed information on creating Jamfiles, see Inside Palm OS Cobalt:
Building a ROM Image.

For example, the Jamfile for PDK/interfaces/app might start
like this:
@code
# Jamfile to build app interface files
SubDir TOP platform interfaces PDK app ;
# Build IDL-generated files
InterfaceIdl IApplication.cpp : IApplication.idl : libbinder ;
InterfaceIdl ICommand.cpp : ICommand.idl : libbinder ;
...
@endcode

Make sure the generated files work before you specify them in the
Jamfile.

@section ExampleIDLFiles Example IDL Files

Here are some complete examples of what IDL files look like.
To see more IDL files, look in PDK/interfaces.

@subsection IDatumIDL support/IDatum.idl

@code
namespace palmos {
namespace support {

interface IDatum
{
	enum
	{
		READ_ONLY			= 0x0000,
		WRITE_ONLY			= 0x0001,
		READ_WRITE			= 0x0002,
		READ_WRITE_MASK		= 0x0003,

		ERASE_DATUM			= 0x0200,
		OPEN_AT_END			= 0x0400
	};

	enum
	{   
		NO_COPY_REDIRECTION     = 0x0001
	};

properties:
	uint32_t valueType;
	off_t size;
	SValue value;
	
methods:
	IBinder Open(uint32_t mode, [optional]IBinder editor, [optional]uint32_t newType);
	status_t CopyTo(IDatum dest, [optional]uint32_t flags);
	status_t CopyFrom(IDatum src, [optional]uint32_t flags);

events:
	void DatumChanged(IDatum who, IBinder editor, off_t start, off_t length);
}

} }	// namespace palmos::support
@endcode

@subsection INodeIDL support/INode.idl

@code
namespace palmos {
namespace support {

interface INode
{
	enum
	{
		REQUEST_DATA        = 0x1000,
		COLLAPSE_NODE		= 0x2000,
		IGNORE_PROJECTION	= 0x4000
	};

	enum
	{
		CREATE_DATUM		= 0x0100,
		CREATE_NODE			= 0x0200,
		CREATE_MASK			= 0x0300
	};

	enum
	{
		CHANGE_DETAILS_SENT	= 0x0001
	};

properties:
	[readonly]INode attributes;

	SString mimeType;
	nsecs_t creationDate;
	nsecs_t modifiedDate;

methods:
	status_t Walk([inout]SString path, uint32_t flags, [out]SValue node);

events:
	void NodeChanged(INode who, uint32_t flags, SValue hints);
    void EntryCreated(INode who, SString name, IBinder entry);
	void EntryModified(INode who, SString name, IBinder entry);
    void EntryRemoved(INode who, SString name);
    void EntryRenamed(INode who, SString old_name, SString new_name, IBinder entry);
}
@endcode

@subsection IIteratorIDL support/IIterator.idl

@code
namespace palmos {
namespace support {

interface IIterator
{
	enum
	{
		BINDER_IPC_LIMIT = 0x05
	};

	enum
	{
		REQUEST_DATA        = 0x1000,
		COLLAPSE_NODE		= 0x2000,
		IGNORE_PROJECTION	= 0x4000
	};

	typedef sequence<SValue> ValueList;

properties:
	[readonly]SValue options;

methods:
	status_t Next([out]ValueList keys, [out]ValueList values, uint32_t flags, [optional]size_t count);

events:
	void IteratorChanged(IIterator it);
}

@endcode
*/
