/*!
@page APIConventions API Conventions

<div class="header">
<center>\< @ref BinderOverview | @ref OpenBinder | @ref BinderThreading \></center>
<hr>
</div>

This is a description of the various programming conventions we use.  
It is based on conventions from the Binder, but is applicable to C++ 
programming in general.  It covers both C++ language features and 
styles as well as conventions specific to the Binder itself.

These conventions are intended to be very minimal &mdash; the focus is 
almost entirely on the public API that is exposed to developers, and 
not on detailing where every brace and tab goes in the actual 
implementation.

Code implemented on top of the Binder should follow these conventions.  
Other parts of the system may or may not follow the various conventions 
described here, as desired by those authors; however, if your code will 
be seen by third party developers it is highly encouraged that you 
adopt these conventions to ensure a consistent developer experience.

  -# @ref GeneralNamespaces
  -# @ref CppConventions
    -# @ref CppNamespaces
    -# @ref CppExceptions
    -# @ref CppTemplates
  -# @ref ClassConventions
    -# @ref ClassNames
    -# @ref ClassPublicProtectedMethods
      -# @ref ClassProperties
      -# @ref ClassCallbacks
      -# @ref ClassDataGeneration
      -# @ref ClassConversions
      -# @ref ClassStatus
      -# @ref ClassLocking
    -# @ref ClassPrivateMethods
    -# @ref ClassMemberVariables
    -# @ref ClassTypeMarshalling
    -# @ref ClassMemoryManagement
  -# @ref OtherCpp
    -# @ref CppHeaderFiles
    -# @ref CppConstants
      -# @ref CppIntegers
      -# @ref CppErrorCodes
      -# @ref CppStringsValues
    -# @ref CppGlobalFunctions
    -# @ref CppComments
  -# @ref InterfaceConventions
    -# @ref InterfaceNamespaces
    -# @ref InterfaceProperties
    -# @ref InterfaceMethods
      -# @ref InterfaceFactories
    -# @ref InterfaceEvents
    -# @ref InterfaceConstants
    -# @ref InterfaceComments

@section GeneralNamespaces General Namespaces

We use a Java-style convention for component, interface, and other 
names.  These are also reflected in C++ namespaces, where each segment 
of the name is a part of a nested namespace.  There are two namespaces 
you will typically use.

@par org.openbinder.*
These are components, interfaces, and other things that are 
considered to be a part of the platform.  As such, there are a part of 
the platform API and must maintain compatibility as the platform 
develops.

@par com.palmsource.*
These things are not a part of the platform API, 
and will change in incompatible ways between releases.

For example, the Address Book may have a org.openbinder.apps.Address.Database 
component, which is the IDatabaseTable for accessing the Address Book 
data.  This is a component that everyone should use for working with 
that database, and so it is a part of the platform APIs and given the 
org.openbinder.* prefix.

At the same time the Address Book may have a 
com.palmsource.apps.Address.Supermenu component, which it uses 
to show a super-menu for opening an address entry.  This component is 
part of the Adddress Book's private implementation and should not be 
used by others, so is in the com.palmsource.* namespace.

Note that something being in the org.openbinder.* namespace is probably not 
completely a guarantee of compatibility.  For example, there are 
various private system interfaces (such as 
org.openbinder.app.IApplicationManager) that are not part of the SDK and 
developers should not use; this is enforced by controlling which IDL 
files are included in the SDK.  In general (and especially for 
component names), however, these rules should be followed.

@section CppConventions C++ Language Conventions

@subsection CppNamespaces Namespaces

We intend to use namespaces in C++, however they aren't currently 
enabled.  You will see a lot of stuff around that purports to use 
namespaces, however it is all currently commented out and thus probably 
broken.  (When the Binder was originally being implemented it did use 
namespaces, however that had to be removed when we ported to ADS and 
Visual Studio 6, neither of which supported namespaces.  At some point 
we need to go through all of the code and fix its use of namespaces so 
that we can re-enable them, but this hasn't been a high priority.)

All of our OS APIs are placed into the platform-level namespace as 
described above, and then into a second-level namespace within that for 
the kit to which it belongs.  For example:

@code
namespace palmos {
namespace support {

class SConditionVariable
{
	...
};

} }	// namespace palmos::support
@endcode

A namespace can have public "using" statements with it, if that 
namespace's API physically depends on another:

@code
namespace palmos {
namespace render {

using namespace support;

class SRect
{
	// Need the Support Kit's SValue class for this.
	SRect(const SValue& value, status_t* outError);
	...
};

} }	// namespace palmos::render
@endcode

Note that given the above code (saying <tt>palmos::render</tt> builds on top 
of <tt>palmos::support</tt>), it would be invalid within the <tt>palmos::support</tt>
namespace to write "<tt>using render</tt>".  That is, there should be no 
circular dependencies between kits/namespaces.

It is @e never okay to have a "<tt>using</tt>" statement in the global 
namespace in a public header.  You must only place them within other 
namespaces.  (This is because it is impossible to automatically 
disambiguate symbols once they are in the global namespace.)  However, 
you can do whatever you want as part of your implementation.  Thus most 
<tt>.cpp</tt> files do something like this:

@code
#include <render/Rect.h>

using namespace palmos::render;

SRect::SRect()
{
}

...
@endcode

@subsection CppExceptions Exceptions

We don't use exceptions in the Binder.  This is primarily because our 
ARM compiler (ADS) doesn't support them.  In addition, for them to be 
useful we would probably need to be able to trasparently propagate them 
across IPC calls, and it's not clear how we would go about doing that.

@subsection CppTemplates Templates

There is some careful use of templates in our APIs.  In general you 
should feel free to use templates where it makes sense (to the extent 
that our compilers actually support them), however you do have to be 
careful.

The main thing to watch out for is that we ultimately need to support 
binary compatibility with our APIs, and this is very problematic with 
templates because by their nature they expose their implementation.  
Thus you can never export template functions from a library, because 
you will end up with multiple (potentially different) implementations 
of the template compiled in to the library and its clients.  (Not to 
mention the fact as our shared library tools currently exist you will 
be unable to link to the library due to duplicate symbol errors.)

There are two basic approaches we take to templates.  The first is to 
just make the template very simple, so that the implementation is 
trivial (and thus won't change) and can be safely compiled in to every 
client.  A good example of this is the sptr<> and wptr<>
templates in support/Atom.h.

For more complicated templates, we take an approach that is a variation 
on the above.  The template class is divided into two parts, a base 
class containing the interesting implementation and a set of pure 
virtual methods, and then a template class deriving from the base class 
that implements the pure virtuals based on the type it is templatized 
over.  In this way the template class is again a very simple 
implementation that doesn't need to be exported from the library.  The 
SAbstractVector and SVector classes in support/Vector.h are a good
example of this approach.

@section ClassConventions Class Conventions

@subsection ClassNames Class Names

C++ class names are MixedCase with one of three prefixes indicating the 
type of class:

<UL>
 <LI> @b I is used for Binder interfaces.  For example, 
IView.  These classes have as a base the class IInterface
(with the single exception of IBinder).  They are pure
virtual classes, providing no actual implementation.
 <LI> @b B is used for Binder classes.  This is any class that 
derives directly or indirectly from BBinder.  For 
example, BView.  These classes are usually concrete 
(they can be instantiated), though that is not always the case.
 <LI> @b S is used for all other classes.  Most often these are type 
classes, such as SValue or SMessage, but 
they may be other things as well such as SAtom or SHandler.
</UL>

Note there are currently many places in the source tree that don't 
follow these conventions.  For example, you will often see classes with 
a "P" prefix, which is from an old iteration of the conventions.  
People will be cleaning up the code as they get a chance.

@subsection ClassPublicProtectedMethods Public/Protected Method Names

Public methods should be written in MixedCase().  In addition, we have 
some developing conventions for constructing method names.  Note that 
from an API compatibility perspective, protected APIs are no different 
than public ones.

@subsubsection ClassProperties Properties

@code
	void SetProperty(int32_t property);	// setter function
	int32_t Property() const;			// getter function
@endcode

Never directly expose member variables in the public/protected 
sections.  If performance is an issue, simply make inline getter/setter 
methods (and clearly comment the method variable that it must stay
the same for binary compatibility).

@subsubsection ClassCallbacks Call-backs

@code
	virtual void OnKeyDown(const SString& key, const SMessage& msg);
@endcode

A hook function that subclasses override should use the "On" prefix.  
Note that Binder interfaces will rarely if ever use "On", because they 
define an external protocol into the class.  "On" here is used for an 
internal protocol of the class making calls on itself for more derived 
classes to override.

@subsubsection ClassDataGeneration Data Generation

@code
	IView GenerateCell(SValue name, size_t row, SValue cellData, [inout]float itemHeight);

	virtual SString GenerateTitle() = 0;
	virtual SString GenerateTitleIcon();  //!< defaults to none
@endcode

We have started using a convention in the activity classes for the word 
"Generate" at the front of virtual functions that create some piece of 
data to return to the caller.  This is used in both interfaces (for 
example IColumn::GenerateCell() creates a new IView object for a cell 
in the column) and call-backs in classes (for example 
BInteractiveActivity::GenerateTitle() creates a string for the 
window's title).

@subsubsection ClassConversions Conversions

@code
	SValue AsValue() const;
	SRect AsRect(status_t* out_err = NULL) const;
@endcode

A method that converts a given object into another type should use the 
"As" prefix.  If the conversion can fail you should include an optional 
output parameter of an error result, though it should still generate a 
well-defined value in case of error, such as an empty string or 
undefined SValue.

@subsubsection ClassStatus Status

@code
	status_t StatusCheck() const;
@endcode

Classes that can have an error state &mdash; such as due to memory
or other error during constructor, being constructed without being
initialized, becoming invalid while being used &mdash; should have
a StatusCheck() method that returns the current status of the object.

@subsubsection ClassLocking Locking

@code
	lock_status_t		Lock() const;
	void				Unlock() const;

	virtual status_t	SetSizeLocked(off_t size);
@endcode

In some rare occasions (most significantly the generic Data Model
class implementations), we must expose locking in the public API.
This should be done by including Lock() and Unlock() methods, where
the Lock() method returns a standard lock_status_t type.  Any
other methods that are called with the lock held must have the
"Locked" suffix.  This is very useful to clearly document the locking
policies of the class.

Note that you should <em>very rarely</em> use this approach in
public APIs, because it almost always makes the API more complicated
to use and rigid.

@subsection ClassPrivateMethods Private Method Names

@code
	void private_method();
	void do_something_l();
@endcode

Private methods should be written in lower_case().  This way the 
default settings of MakeSLD will not export these symbols.

A private method that must be called with a lock held should use a "_l" 
suffix.  For example, do_something_l().  This is very useful to 
document internal locking policies in the implementation.

@subsection ClassMemberVariables Member Variables

@code
	int32_t m_someInt;
@endcode

All member variables should have a "m_" prefix and then used mixedCase 
starting with a lower-case letter.

@subsection ClassTypeMarshalling Type Marshalling

@code
class SUrl
{
public:
	...
	                SUrl(const SValue &value, status_t* out_err=NULL);

	       SValue   AsValue(int32_t form = B_FLATTEN_FORM_ACTIVE) const;
	inline operator SValue() const { return AsValue(); }
@endcode

If you are writing a stack class that is to be marshalled by pidgen (so 
it can be used in Binder interfaces), you must provide the above
constructor and methods.

Note that if AsValue() fails, it should return a status SValue using 
SValue::Status().  (If it fails due to running out of memory or other 
exception cases, use SetError().)

To support custom marshalling, you need in addition to implement the 
following functions:

@code
class SPoint
{
public:
	...

	static ssize_t  MarshalParcel(SParcel& dest, const void* var);
	static status_t UnmarshalParcel(SParcel& src, void* var);
	static status_t MarshalValue(SValue* dest, const void* var);
	static status_t UnmarshalValue(const SValue& src, void* var);
};
@endcode

@subsection ClassMemoryManagement Memory Management

In general there are two kinds of classes we use: stack-based and 
reference counted.

Reference counted classes derive from SAtom, SLightAtom or SLimAtom.
All I- and B-prefix classes are reference counted.  Instances of these
classes are always created with new, and pointed to with sptr<> or 
wptr<>; the reference counting mechanism will take care 
of calling delete for you at the appropriate time.  These classes are 
always passed by reference.

The "atom" reference counting classes are described in @ref SupportFAQ.

Stack-based classes generally correspond to types, for example 
SRect, SValue, SLayoutConstraints, etc.  You should never use 'new' to 
instantiate them, instead constructing them on the stack or in a 
container such as SVector<>.  These classes are always 
passed by value (an out or in/out parameter might seem like an 
exception, but conceptually you should think of this as copying the 
value back and forth).

Stack-based classes should be efficient to copy, so that they can 
always be passed by value.  This is important in a multithreaded 
system where returning a copy of an object can make thread safety much 
easier to deal with.  If your class can't be efficiently copied, you 
can use copy-on-write semantics.  SString, 
SValue, and SGlyphMap are all examples of 
this; they all use the convenient SSharedBuffer class 
for their implementation.

Note that SSharedBuffer and implementations using it are 
an exception to our rule about not manually managing memory...  but 
that's okay, because SSharedBuffer is there to help us implement other 
classes in the normal API that hide their memory management.

@section OtherCpp Other C++ Conventions

@subsection CppHeaderFiles Header Files

All C++ header files are placed in sub-directories named by the kit 
they belong to.  Generally a particular class in the API will be placed 
into its own header file, with that header having the same name as the 
class @e without its prefix.  For sample, the SFont class is in the 
Render Kit, so its header file is "render/Font.h".

Sometimes you will have a set of related class that make sense to go 
together in one header file.    In this case the header name will be 
representative of the kinds of classes it contains.  For example, 
"support/Atom.h" includes SAtom, SLightAtom, SLimAtom, as well as 
sptr and wptr.

You are encouraged to include multiple classes in a header, when it 
helps developers to understand the header files: it is basically a 
balancing act between a few headers with so many classes in them you 
can find the class, vs each header having one class but there being so 
many headers you are overwhelmed by them.  Use your judgement.

Every kit should have a header file in it called "kit/KitDefs.h" that 
includes global constants and other definitions for that kit.  For 
example see view/ViewDefs.h.

@subsection CppConstants Constants

Constant values are written in <tt>UPPER_CASE</tt>.  If the constant is not in 
the namespace of a specific class/interface, then it must have a B_ 
prefix.  Usually for constants outside of a class the first word in the 
constant will be a common specification of the category of constants it 
belongs to (i.e., <tt>B_VIEW_*</tt>).

@subsubsection CppIntegers Integers

Integer constants should be declared as an enumeration (often 
anonymous).  Constants that represent a mask of bits in a larger 
constant should use the suffix <tt>_MASK</tt>.  Some examples:

@code
enum {
	B_JUSTIFY_MASK		= 0x0000000F,
	B_JUSTIFY_LEFT		= 0x00000001,
	B_JUSTIFY_RIGHT		= 0x00000002,
	B_JUSTIFY_FULL		= 0x00000003,
	B_JUSTIFY_CENTER	= 0x00000004,

	B_VALIGN_MASK		= 0x000000F0,
	B_VALIGN_BASELINE	= 0x00000010,
	B_VALIGN_TOP		= 0x00000020,
	B_VALIGN_BOTTOM		= 0x00000030,
	B_VALIGN_CENTER		= 0x00000040
};
@endcode

@code
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

methods:
	...
}
@endcode

@subsubsection CppErrorCodes Error Codes

We currently treat error codes exactly like integer constants:

@code
enum {
	B_NO_MEMORY = sysErrNoFreeRAM,
	B_BAD_VALUE = sysErrParamErr,
	B_NOT_ALLOWED = sysErrNotAllowed,
	B_TIMED_OUT = sysErrTimeout
};
@endcode

@subsubsection CppStringsValues Strings and Values

SString and SValue string constants can be constructed with the 
B_CONST_STRING_VALUE_SMALL() and B_CONST_STRING_VALUE_LARGE() 
macros.  The constants these create can be both an SString and an 
SValue.  These constants can not be placed inside a class namespace, so 
they will always have the prefix BV_*.  Some examples:

@code
B_CONST_STRING_VALUE_LARGE	(BV_VIEW_BYTES,			"bytes",);
B_CONST_STRING_VALUE_SMALL	(BV_VIEW_KEY,			"key",);
B_CONST_STRING_VALUE_LARGE	(BV_VIEW_MODIFIERS,		"modifiers",);
B_CONST_STRING_VALUE_LARGE	(BV_VIEW_REPEAT,		"repeat",);
@endcode

There are other macros for building SValues of other types: 
B_CONST_INT32_VALUE() and B_CONST_FLOAT_VALUE().

@subsection CppGlobalFunctions Global Functions

Global functions use MixedCase, without a prefix.  This distinguishes 
them from posix functions (which are lower_case) and is similar to the 
C convention for functions.  Note that unlike the C convention, C++ 
functions do not require a manager prefix &mdash; they should instead be in 
the namespace of the C++ framework they are a part of.

@code
status_t StringSplit(const SString& srcStr, const SString& splitOn, SVector<SString>* strList);
status_t StringSplit(const SString& srcStr, const char* splitOn, SVector<SString>* strList);

status_t ExecuteEffect(	const sptr<IInterface>& target,
						const SValue &in, const SValue &inBindings,
						const SValue &outBindings, SValue *out,
						const effect_action_def* actions, size_t num_actions,
						uint32_t flags = 0);

template<class TYPE> void Construct(TYPE* base, size_t count = 1);
template<class TYPE> void MoveAfter(TYPE* to, TYPE* from, size_t count = 1);
template<class TYPE> void Assign(TYPE* to, const TYPE* from, size_t count = 1);
template<class TYPE> void Swap(TYPE& v1, TYPE& v2);
@endcode

@subsection CppComments Comments

We use @link http://www.stack.nl/~dimitri/doxygen/ Doxygen @endlink to automatically 
generate internal documentation from our source tree.  For good 
documentation, you must correctly comment and mark up your header files 
as described in the @link http://www.stack.nl/~dimitri/doxygen/manual.html 
Doxygen manual. @endlink

In general header files should have brief comments for all APIs, enough 
that someone browsing the file can get a basic grasp of how the API 
works without having so much detail that it is hard to get the overall 
view of what is there.  The BView header file 
(view/View.h) serves as a good example, such as:

@verbatim
/*!	@name Utilities
	Convenience functions to retrieve general information about the view and
	perform interactions with the view hierarchy. */
//@{

/*! Return the Graphic plane that was given in SetGraphicPlane() */
		SGraphicPlane			GraphicPlane() const;

//!	A not-necessarily-unique identifier for your view, supplied by the parent.
virtual	SString					ViewName() const;
//!	Called by the parent to supply a name.
virtual	void					SetViewName(const SString& value);

//!	Returns the parent or NULL if the view doesn't have a parent.
/*!	This function is potentially slow, use carefully.  It is not valid
	to call this during Draw(); this is for transaction and current state only. */
virtual	sptr<IViewParent>		Parent() const;

//!	Invalidate the view.
/*!	Use this functions preferrably over IViewParent::InvalidateChild().  If the view
	doesn't have a parent, the function returns without doing anything. */
		void					Invalidate();
//!	Invalidate() a specific rectangle in the view.
		void					Invalidate(const SRect& rect);
//!	Invalidate() a specific shape in the view.
		void					Invalidate(const SRegion& shape);


//! For implementation, don't use.
virtual void					Invalidate(BUpdate* update);

//!	Calls @c MarkTraversalPath() with @e constrain and @e layout flags.
/*!	This will eventually trigger an update as MarkTraversalPath() walks
	up in the hierarchy and encounters a @c ViewLayoutRoot object. */
virtual	status_t				InvalidateConstraints(uint32_t execFlags = 0);

//!	Request that a transaction start, if one hasn't already.
virtual	void					RequestTransaction(uint32_t execFlags = 0);

//@}
@endverbatim

@section InterfaceConventions Interface Conventions

Interfaces describe abstract contracts that others can implement and 
use.  They create the key APIs in the system, and as such particular 
care should be taken when creating them.

For the most part, a more general interface is a more powerful 
interface: the more general it is, the more places it can be used, and 
thus all code written to implement or use that interface is itself more 
reusable.

Before writing a new interface, you should first make sure there isn't 
an existing interface that will serve your needs.  Even if there is an 
existing interface that is close but doesn't quite do what you want, it 
may make sense to modify the existing interface so it is more general 
instead of designing a new one.

@subsection InterfaceNamespaces Namespaces

You should place interfaces in to namespaces like the C++ namespaces 
described previously.  Note that interfaces ultimately end up in C++ 
kits, so they should be named in the same way.

@code
namespace palmos {
namespace support {

interface INode
{
	...
}

} }	// namespace palmos::support
@endcode

This creates an interface called "org.openbinder.support.INode".

@subsection InterfaceProperties Properties

@code
	[readonly]INode attributes;
	SString mimeType;
@endcode

Property names should be mixedCase, with their first letter being 
lower-case.

By following this convention, pidgen will automatically generate for 
you C++ methods following the correct convention:

@code
	sptr<INode> Attributes() const;

	SString MimeType() const;
	void SetMimeType(const SString& value);
@endcode

@subsection InterfaceMethods Methods

@code
	status_t Register(SValue key, IBinder binder);

	status_t Walk([inout]SString path, uint32_t flags, [out]SValue node);
	status_t InvalidateChild(IView child, [inout]BUpdate update);
	status_t RemoveView(IView view, [optional]uint32_t execFlags);
	status_t GetStuff([out]SValue stuff, [optional out]SString name) const;
@endcode

Methods should be MixedCase, with their first letter being upper-case.

Output parameters can be used anywhere, though they should generally 
appear after input parameters.  Optional parameters must be the last in 
the method.

@subsubsection InterfaceFactories Factories

@code
	IIterator NewIterator(SValue args, [optional out]status_t outError);
@endcode

A factory method is one that generates a new instance of something each 
time it is called.  Such methods should begin with the "New" prefix.

@subsection InterfaceEvents Events

@code
	void EntryModified(INode who, SString name, IBinder entry);
@endcode

Events should use the same naming convention as methods, MixedCase 
starting with an upper-case letter.  They typically use a past-tense 
verb, since they are indicating that some action has happened.

@subsection InterfaceConstants Constants

@code
	enum
	{
		CREATE_DATUM		= 0x0100,
		CREATE_CATALOG		= 0x0200,
		CREATE_MASK			= 0x0300
	};
@endcode

You can use enumerations to create integral constants.  (Note that in 
our IDL syntax, enumerations must appear before any "properties:", 
"methods:", or "events:" sections.)  Like constants in C classes, they 
should be UPPER_CASE with words separated by underscores, and not us a 
prefix (the interface name is the prefix).

@subsection InterfaceComments Comments

Place comments in IDL files like you would with C++, using Doxygen 
markup.  Pidgen will automatically propagate these comments to the 
generated C++ header file, so that they can be incorporated into the 
overall documentation.

@verbatim
//!	Interface to a node in the Binder namespace
/*!	Nodes allow you to walk paths through the namespace
	and access meta-data.
*/
interface INode
{
	enum
	{
		/*!	This flag can be supplied to various catalog
			functions, to indicate you would like them to
			return the contents of an IDatum instead of the
			object itself, if possible.
		*/
		REQUEST_DATA        = 0x1000
	};

properties:
	//!	Retrieve the meta-data catalog associated with this node, or NULL if it doesn't exist.
	[readonly]INode attributes;

methods:
	//!	Walk through the namespace based on the given path.
	/*!	This function decodes the first name of the path, and recursively
		calls Walk() on the entry it finds for that.
	*/
	status_t Walk([inout]SString path, uint32_t flags, [out]SValue node);

events:
    //! This event is sent when a new entry appears in the catalog.
    /*! @param who the parent catalog in which this change occured.
        @param name the name of the entry that changed.
        @param entry the entry itself, either an INode or IDatum.
    */
    void EntryCreated(INode who, SString name, IBinder entry);
}
@endverbatim

*/
