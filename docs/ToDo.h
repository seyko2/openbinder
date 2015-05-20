/*!
@page OpenBinderToDo To Do Items for OpenBinder

@section ProcessManagement Process and Security Management
@todo Better process and security management.

The current implementation provides the basic mechansisms of security and
process management (via SContext::GetContext() and SContext::NewProcess()),
but there is no implementation yet of a particular policy.

Addressing this probably means adding a few things that get set up by
smooved:

-# A "process manager" to replace the generic /processes directory, which
can do things like instantiate a component for you in an appropriate process
(potentially creating a new process for you at that time).  SContext::New()
can then work with this API to make sure that components are instantiated in
a process appropriate for the current security restrictions.  As part of this,
there will also be a new argument or two added to SContext for the caller
to say something about process management &mdash; such as "never use my process"
or "always use my process if possible".

-# A security manager, integrated to some degree with the process manager,
that decides who can go in what process.  This may be based on code signing
or other security policies.

In addition, some work needs to be done so this is actually secure
(appropriately hiding things under /processes outside of the system
context) and works in the user context (which currently because of the
catalog mirror in /processes can't see the IProcessManager...  maybe
add a facility to catalog mirror to selectively make underlying interfaces
visible, and don't allow casting from IProcessManager to the other
normal catalog interfaces).

@section ReorganizeSources Reorganize Sources
@todo Reorganize sources.

The Support Kit has turned into a dumping ground for everything, and really needs
to be broken into smaller pieces.  One proposal is to change ITextOutput and
ITextInput to something like (SAbstractTextOutput and SAbstractTextInput) so they
no longer depend on the higher-level Binder APIs, so that we can pull those APIs
out into their own "binder" kit:

<b>Support Kit</b>

- support/Atom.h
- support/atomic.h
- support/Autolock.h
- support/Bitfield.h
- support/BitstreamReader.h
- support/BufferChain.h
- support/Buffer.h
- support/ByteOrder.h
- support/CallStack.h
- support/ConditionVariable.h
- support/Debug.h
- support/Errors.h
- support/EventFlag.h
- support/Flattenable.h
- support/HashTable.h
- support/SAbstractStream.h
- support/KeyedVector.h
- support/List.h
- support/Locker.h
- support/RegExp.h
- support/SharedBuffer.h
- support/SortedVector.h
- support/StopWatch.h
- support/String.h
- support/StringTokenizer.h
- support/StringUtils.h
- support/SupportBuild.h
- support/SupportDefs.h
- support/TextCoder.h
- support/Thread.h
- support/TLS.h
- support/TypeConstants.h
- support/TypeFuncs.h
- support/URL.h
- support/Vector.h
- support/VectorIO.h

<b>Binder Kit</b>

- binder/Autobinder.h
- binder/Binder.h
- binder/ByteStream.h
- binder/BCommand.h
- binder/Context.h
- binder/Handler.h
- binder/IBinder.h
- binder/IByteStream.h
- binder/IHandler.h
- binder/IInterface.h
- binder/IMemory.h
- binder/InstantiateComponent.h
- binder/IStorage.h
- binder/Iterator.h
- binder/KernelStreams.h
- binder/KeyID.h
- binder/Looper.h
- binder/Memory.h
- binder/MessageCodes.h
- binder/Message.h
- binder/MessageList.h
- binder/Node.h
- binder/NullStreams.h
- binder/Observer.h
- binder/Package.h
- binder/Parcel.h
- binder/Process.h
- binder/Selector.h
- binder/SGetOpts.h
- binder/StaticValue.h
- binder/StdIO.h
- binder/SwappedValue.h
- binder/TextStream.h
- binder/TokenSource.h
- binder/Value.h

- binder/IByteStream.idl
- binder/ICatalog.idl
- binder/ICatalogPermissions.idl
- binder/ICommand.h
- binder/IDatum.idl
- binder/IIterable.idl
- binder/IIterator.idl
- binder/IMemory.idl
- binder/INode.idl
- binder/INodeObserver.idl
- binder/IProcess.idl
- binder/IRandomIterator.idl
- binder/ISelector.idl
- binder/IUuid.idl
- binder/IVirtualMachine.idl

<b>Storage Kit</b>

- storage/BufferIO.h
- storage/Catalog.h
- storage/CatalogMirror.h
- storage/ContentProvider.h
- storage/DatumLord.h
- storage/GenericCache.h
- storage/GenericDatum.h
- storage/GenericIterable.h
- storage/MemoryStore.h
- storage/Pipe.h
- storage/StringIO.h
- storage/StructuredNode.h

(Note that a few implementations of the lower-level classes may make use of
code in a higher-level class, so they will need to be bundled into the same
shared library, but there should be no public dependencies between them.
For example, the Support Kit may want to use binder/StdIO.h and storage/MemoryStore.h
as a convenience for printing from its internal debugging mechanisms.)

@section ReworkScripting Rework Scripting
@todo Rework scripting in IBinder.

Replace IBinder::Effect() with explicit
functions for method call, put property, get property.  These can then return
an error code indicating whether the property/method exists, so that scripting
languages can operate on an object's method or property without knowing the
exact IBinder/IInterface it is implemented on.  Better yet, there can be an
API on IBinder to do a property get or method call on "any interface", which
the default BBinder implementation can handle by doing an Inspect() on itself
and then trying all of the available interfaces.

@section BetterArrays Improve Array Marshalling/Unmarshalling
@todo Improve the way we handle marshalling and unmarshalling of arrays.

Currently you can easily define array structures in IDL, using the sequence
construct:

@code
	typedef sequence<SValue> ArgList;
@endcode

Underneath, this results in the C++ class SVector<> templatized over the appropriate
type.  While this works fine for static interfaces, it is problematic for dynamic
languages such as the Binder Shell: these languages operate only on types at the
SValue level, for which there is no formal definition of an array.  In fact, the
marshalling of an SVector<> can happen in two different ways: either as an SValue
containing a single blob of data holding an array of simple types (such as int32_t),
or an SValue of complex mappings, where the key is the index into the array.

To solve this, SValue should probably get built-in knowledge of arrays, which is
the representation SVector uses when marshalling and unmarshalling.  This shouldn't
be too much effort, though it will be tricky dealing with arrays of objects or
other SValues, which need special handling as they are created, copied, and
destroyed.  With that in place, new syntax can then be added to Binder Shell for
creating these kinds of SValues.

@section PackageManagerData Package manager needs to use data model
@todo Pacakge manager needs to use the data model

Right now a lot of the package manager data is presented as blobs of
SValue mappings (such as /packages/components/foo or /packages/interfaces/foo),
and this really should make better use of the data model.  In both of
these cases, for example, it should probably use something like
a BStructuredNode for a table representation, which ultimately should
allow SQL-like queries on the data.

@section LinksToDo Links
@todo Lots of improvements to links

- Add "synchronous" flag that can be supplied when pushing.
- Add priority, for control of ordering of links.
- Add a way for link targets to return information (through a new
  kind of push), and/or stop the push for any following targets.
- Use copy-on-write for link data, to eliminate copying when
  pushing links (by avoiding the need to hold a lock while
  iterating over link records).

@section PidgenToDo pidgen

@subsection FixForwardDecs Fix Forward Declarations in IDL Files
@todo PIDGEN: currently doesn't deal with an IDL file that contains a forward declaration
and declaration of the same interface.

For example:

@code
interface IOne

interface ITwo
{
properties:
IOne one;
}

interface IOne
{
}
@endcode

This is due to how pidgen deals with imports and thus ends up stripping out the IOne
forward declaration (see main.cpp, generate_from_idl(), the loop with the comment
"store things from the parse tree into digestable formats".)

You can work around this by putting the interfaces in to separate files:

@code
interface IOne

interface ITwo
{
properties:
IOne one;
}
#endcode

@code
import <IOne.idl>

interface IOne
{
}
@endcode

@subsection FixMultilineComments Fix multi-line comments
@todo PIDGEN: Fix multi-line comments

Currently, multi-line comments contain \\r\\n rather than just \\n

@subsection PidgenSValueReturn Improve SValue return
@todo PIDGEN: Improve SValue return

When returning SValues, change it to return SValue()
rather than using a stack variable.

@subsection GenerateB_IMPLEMENT B_IMPLEMENT_IINTERFACE_FLATTEN_FUNCS
@todo PIDGEN: B_IMPLEMENT_IINTERFACE_FLATTEN_FUNCS

Anytime an interface is seen, automatically generate
B_IMPLEMENT_IINTERFACE_FLATTEN_FUNCS for that interface.
(Right now, the user needs to put those in when using that
interface in a sequence.)

@subsection PidgenWptr Support wptr in a sequence?
@todo PIDGEN: Support wptr in sequence?

Right now it defaults to sptr only.

@subsection PidgenUnlink UnlinkSomeInterface() should take a wptr<>
@todo PIDGEN: UnlinkSomeInterface() should take a wptr<>

The UnlinkSomeInterface() method generated by pidgen takes a sptr<>;
it should take a wptr<> to match IBinder::Unlink().

*/
