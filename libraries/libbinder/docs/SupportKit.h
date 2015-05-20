/*!
@page SupportKit Support Kit

<div class="header">
<center>\< @ref EnvironmentVars | @ref OpenBinder | @ref SAtomDebugging \></center>
<hr>
</div>

The Support Kit consist of a suite of useful utilities
and common operations.  These classes were designed and written
to assist in developing the Binder and systems built on top of
it, however they can be used in a wide variety of situations.

Be sure to also see the @ref APIConventions page for a higher-level
description of the coding conventions used by the Binder and its
related APIs.

@note The current source tree has the @ref BinderKit and Support Kit
combined together in the same directory (support).  This page only
describes the support APIs there.

-# @ref ErrorCodesSummary
-# @ref BasicTypesSummary
  -# @ref SStringSummary
  -# @ref SValueSummary
  -# @ref SUrlSummary
-# @ref ContainersSummary
  -# @ref SVectorSummary
  -# @ref SSortedVectorSummary
  -# @ref SKeyedVectorSummary
  -# @ref SListSummary
  -# @ref SBitfieldSummary
-# @ref MemoryManagementSummary
  -# @ref SAtomSummary
  -# @ref SLightAtomSummary
  -# @ref SLimAtomSummary
  -# @ref SSharedBufferSummary
-# @ref ThreadingSummary
  -# @ref SLockerSummary
  -# @ref SConditionVariableSummary
  -# @ref SHandlerSummary
  -# @ref SThreadSummary
-# @ref FormattedTextSummary
  -# @ref ITextOutputSummary
  -# @ref ITextInputSummary
-# @ref UtilitiesSummary
  -# @ref SRegExpSummary
  -# @ref STextDecoderAndSTextEncoderSummary
-# @ref DebuggingSummary
  -# @ref SCallStackSummary
  -# @ref SStopWatchSummary

@section SupportKitAdditionalMaterial Additional Material

- @subpage SAtomDebugging describes debugging facilities available for SAtom reference counts.
- @subpage SHandlerPatterns is a recipe book for some common threading techniques with SHandler.

@section ErrorCodesSummary Error Codes

The Binder APIs use status_t to return an error code; no C++ exceptions
are used.  The standard system error code constants are defined in
support/Errors.h for C++ APIs.

Note that error codes are negative constants.  A common pattern
is to use ssize_t to return a result that can be either a
(negative) error code or a (positive) index.  Thus, for example,
you will often write code like:

@code
void myFunc(const SSortedVector<SString>& stringSet)
{
	ssize_t index = stringSet.IndexOf(SString("some string"));
	if (index >= 0) {
		// ... do something ...
	} else {
		// couldn't find "some string"
	}
}
@endcode

See @ref general_error_codes_enum for the standard C++ error code
constant and @ref CoreErrors for more details on error codes.

@section BasicTypesSummary Basic Types

In addition to the standard C++ types (integers and floats), 
support classes are included for some other common types.

All of the classes described here use copy-on-write semantics for
their data, so you can cheaply copy them from one place to another.
Note that thread safety is supplied for the underlying data (if
more than one object is referencing the same copy-on-write buffer),
but the public APIs described here are @e not thread safe.  If
multiple threads may be accessing a particular instance of one of
these objects, you will need to protect it appropriately.

Usually you will pass these objects by constant reference in to a
function, so that the copy-on-write mechanism can be used to avoid
copying the object's data:

@code
void MyClass::StoreSomething(const SString& inputString)
{
	m_someString = inputString;
}
@endcode

The copy-on-write mechanism is also taken advantage of to efficiently
return these objects by value from functions, which is very useful
when writing thread safe code:

@code
SString MyClass::RetrieveSomething() const
{
	return m_someString;
}
@endcode

@subsection SStringSummary SString

The SString class represents a UTF-8 encoding string.  This is a
C-style (NUL-terminated) string.  There are a variety of methods
included for manipulating the string's data.

@subsection SValueSummary SValue

An SValue is a generic container of a typed piece of data, much
like a "variant" in COM.  The basic data structure does not define
any type semantics &mdash; it simply has generic handling for a blob
of bytes with an associated type code &mdash; however there are many
convenience functions for dealing with specific types and
performing type conversions.

An unusual feature of SValue is that it can hold complex data
structures in addition to simple typed data.  These are
constructed as sets of mappings, where the key and value of
each mapping is itself an SValue object (which may contain
further mappings).  These kinds of data structures are usually
written in this way:

@code
{
	"Key1" -> 0,
	"Key2" -> "string"
}
@endcode

Indeed, the @ref BinderShellTutorial "Binder Shell" provides a
mechanism to build SValue structures using just this syntax:

@code
$ echo @{ Key1 -> 0, Key2 -> string }
@endcode

The macros B_STATIC_STRING_VALUE_LARGE() and B_STATIC_STRING_VALUE_SMALL()
are provided to efficiently create static string constants.
Note that a constant like the following requires, at load time, for the
SValue object to be constructed and memory allocated for "some string":

@code
static SValue myValue("some string");
@endcode

Not only does this
take time to do, but it means that the SValue object itself
can't be placed in the constant section of your code, and the
data from the string must be copied out of your constant
sections and on to the heap.  As an alternative, this code
will produce a "myValue" holding the exact same string constant
as shown above:

@code
B_STATIC_STRING_VALUE_LARGE(myValue, "some string", );
@endcode

The data for this object is created entirely through static
data structures that can be placed in the executable's text
section.  The myValue constant can be used as either an SValue
or SString object, depending on what you need.

The B_STATIC_STRING_VALUE_SMALL() macro is used to create
SValue constants that are four bytes (including a string NUL
terminator) or less.  Thus the correct uses of these macros:

@code
B_STATIC_STRING_VALUE_SMALL(mySmallValue, "123", );
B_STATIC_STRING_VALUE_LARGE(myLargeValue, "1234", );
@endcode

For the very performance conscious, the SSimpleValue template
class is a very efficient way to create SValue constants at
runtime &mdash; like B_STATIC_STRING_VALUE_SMALL but for different
types that are created on the stack.  The SSimpleStatusValue
is the same kind of thing as SSimpleValue, but specifically
for an SValue holding a status code.

@subsection SUrlSummary SUrl

The SUrl is a convenience class for working with URL strings.

@section ContainersSummary Containers

A number of container classes are provided to easily build
common data structures.  Most of these are templatized classes,
providing a model similar to simple usage of the STL.  However,
unlike the STL, the implementation is @e not templatized.  This
is done to avoid the code explosion that can happen with templatized
classes, and to provide an API for which binary compatibility
can be maintained as the implementation evolves.

These classes avoid templatizing their implementation
by spliting the class into two parts.  A base class (such as
SAbstractVector) provides a type-independent implementation of
its algorithms, with a set of pure virtual functions (such as
SAbstractVector::PerformCopy()) that represent the type-dependent
operations it will need to do.

The template class you use in your code, such as SVector, derives
from its corresponding generic base class, and provides the type
specific parts of the implementation.  These are very minimal, consisting
mostly of type casts along with an implementation of the pure virtuals
needed by the base class.

Like the @ref BasicTypesSummary, none of the APIs described here are
thread safe.  You must supply your own thread protection if multiple
threads will be accessing the same class instance.

@subsection SVectorSummary SVector

The SVector is an array of data, very much like STL's vector.  Note
that the API is not the same as the STL, and in particular it does not
use exceptions for error cases.

Two-dimensional arrays are created by nesting SVector objects:

@code
SVector<SVector<int32_t> > array_2d;
@endcode

Though this kind of structure can lead to significant inefficiency
due to a lot of copying of inner objects when the outer container
is change, the SVector class provides an optimization so that in
most cases the inner SVector instances will simply be moved in
memory when rows are added or removed from the outer SVector.

These kinds of optimizations are supported by a lot of the standard
Binder type classes through specialization of the type-specific
functions in support/TypeFuncs.h.  In particular, SVector
implements BMoveBefore and BMoveAfter to provide an efficient
mechanism for moving a vector in memory without having to completely
copy and destroy the object.

@subsection SSortedVectorSummary SSortedVector

An SSortedVector is a variation on SVector that keeps its data in its
native sorted order.  It can be used to build sets of objects.
Operations on the class use a binary search (O(log n)) to find items
in the vector.

@subsection SKeyedVectorSummary SKeyedVector

The SKeyedVector is templatized over two types, holding data of one
type that is referenced by keys of the other type.  This can be
used for the same kind of situations where you would use an STL map.
Note, however, that as its name implies, this class does @e not use
a map structure, but two vectors &mdash; an SSortedVector holds the keys,
and an SVector holds the values.  Thus a lookup of a key is an
O(log n) operation.

@subsection SListSummary SList

SList is a doubly-linked list data structure.  It has been used
very little, so the API is still pretty rough and the implementation
may be buggy.

@subsection SBitfieldSummary SBitfield

An SBitfield is a special kind of SVector optimized for an array
of boolean values.

@section MemoryManagementSummary Memory Management

A number of support classes provide very rich memory management
tools.  The use of these classes &mdash; in conjunction with the
@ref ContainersSummary and @ref BasicTypesSummary &mdash; means that application
developers will very rarely need to deal directly with memory
management themselves.

The @ref SAtomDebugging page describes a rich set of
reference count debugging and leak tracking features that
are available for SAtom and SLightAtom objects.

@subsection SAtomSummary SAtom

SAtom is a base class for reference counted objects.  Classes
should virtually inherit from it (so that there will only ever
be one SAtom instance in the class), and all pointers to an
SAtom should use the sptr<T> and wptr<T> smart pointer template
classes instead of raw pointers.

@subsection SLightAtomSummary SLightAtom

The SLightAtom is a variation on SAtom that only supports
strong (sptr<T>) pointers.  This allows the class to use only
8 bytes of overhead, as opposed to the 16 bytes needed by SAtom.

@subsection SLimAtomSummary SLimAtom

A SLimAtom is an even lighter-weight reference counting class.
It is a template class, so that you can add reference counting
to an object without requiring any virtual methods.

@subsection SSharedBufferSummary SSharedBuffer

The SSharedBuffer is not a class you will often use directly.
It is the bases for a common copy-on-write mechanism that is used
to implement SString, SValue, and other such classes.

@section ThreadingSummary Threading

The Binder and its support classes provide a very rich multithreading
environment.  Part of that is due to the classes provided here, but
more important is the way the overall system and classes are
designed to work together.  The @ref BinderThreading page describes
in detail the threading model used by the Binder and how its various pieces work
together.

@subsection SLockerSummary SLocker

The SLocker is a very light-weight mutex object.  The object itself
requires only 4 bytes of memory, and operations on it only requires
a call into the kernel when a thread needs to block or wakeup.  This
is a key element of the larger @ref BinderThreading picture.

The SLocker::Autolock class is a convenient way to manage the locking
in a function.  By placing it on the stack, it will automatically
release your lock when execution exits its containing code block &mdash;
either by naturally leaving the block, a return, or an exception.

@code
class MyClass
{
public:
	void SafeFunc(const SString& string);
	{
		SLocker::Autolock _l(m_lock);
		if (m_string != "") return;
		m_string = string;
	}

private:
	SLocker m_lock;
	SString m_string;
};
@endcode

@note Take care when using SLocker::Autolock, since it is easy to unexpectedly
hold a lock while calling into code for which you did not mean
to!  In particular, the mix of sptr<> and SLocker::Autolock can be
dangerous, as shown here:
@code
void MyClass::MyFunc(sptr<Something>* inout1, sptr<Something>* inout2)
{
	SLocker::Autolock _l(m_lock);

	// This code can call into the Something destructor with m_lock
	// held.
	*inout1 = NULL;

	// This code can -still- call into the Something destructor with
	// m_lock held, because holdRef will be destroyed before _l!
	sptr<Something> holdRef(*inout2);
	*inout2 = NULL;
}
@endcode

SLocker also provides a sophisticated tool for predicting program
patterns that can result in deadlocks.  This is enabled by setting
the environment variable LOCK_DEBUG.  Setting it to 15 will enable
full lock debugging (checking dependencies across all locks), while
5 will only do error checking on locks individual (catching for
example when a lock is acquired twice by the same thread).

In addition, setting LOCK_DEBUG_STACK_CRAWLS to a non-zero integer
will include stack crawl information in the debugging instrumentation,
telling you where the problematic locks were acquired.

@subsection SConditionVariableSummary SConditionVariable

An SConditionVariable is a synchronization primitive that allows
one thread to control when another thread will block and when it
will then later wake up.  Like SLocker, it is a light-weight
primitive, using only 4 bytes and not requiring a kernel call
unless a thread needs to block or wake up.

@subsection SHandlerSummary SHandler

The SHandler class is a high-level mechanism for generating threads
for a program.  It uses the Binder's thread pool (which is shared
with the @ref BinderIPCMechanism) to dispatch threads to the SHandler
as they are needed.  This allows a lot more flexibility for the
system in managing threads, and is thus the preferred mechanism
for dealing with threads.

The @ref SHandlerPatterns is a recipe book for some common
threading techniques using SHandler.

@subsection SThreadSummary SThread

An SThread is a more traditional threading object, representing
a dedicated thread.  This can be useful in some special situations,
however most developers should prefer SHandler instead.

@section FormattedTextSummary Formatted Text

These classes are conveniences for performing formatted text
input and output.

@subsection ITextOutputSummary ITextOutput

The ITextOutput interface is supported extensively by Binder classes
for printing their state.  Most classes you use can be printed to
an ITextOutput for debugging and other purposes, and support/StdIO.h
includes some standard ITextOutput streams you can use.  For example:

@code
SValue myval(SValue::String("str1"), SValue::String("str2"));
myval.JoinItem(SValue::String("str3"), SValue::String("str4"));
bout << myval << endl;
@endcode

Also included are some convenience classes for printing data using
special formats.

- STypeCode prints an integer is a 4-character type code.
- SDuration prints an nsecs_t (a 64-bit integer) as a time duration.
- SSize prints a size_t as a memory amount (i.e., 1.5K).
- SHexDump prints an arbitrary block of data as hex digits.
- SPrintf allows you to use printf()-style formatting with an ITextOutput.
- SFloatDump prints a float as its raw bytes.

@subsection ITextInputSummary ITextInput

The ITextInput interface provides an API for reading characters and
lines from an input stream.

@section UtilitiesSummary Utilities

@subsection SStringTokenizerSummary SStringTokenizer

SStringTokenizer splits a string into words.

@subsection SRegExpSummary SRegExp

The SRegExp is a convenience class for performing regular-expression
operations on an SString.

@subsection STextDecoderAndSTextEncoderSummary STextDecoder and STextEncoder

The STextDecoder and STextEncoder provide tools for converting the
native UTF-8 text encoding to and from other encodings.

@section DebuggingSummary Debugging

A few tools are provided for help in debugging and profiling.  These
should not be used in production code.

@subsection SCallStackSummary SCallStack

An SCallStack allows you to retrieve a call stack for the current
thread's location, save it, and print it out optionally with
symbol information.

@subsection SStopWatchSummary SStopWatch

The SStopWatch can be used to perform timing of sections of code.

*/
