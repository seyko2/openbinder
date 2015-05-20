/*!
@page BinderDataModel Binder Data Model

<div class="header">
<center>\< @ref StorageKit | @ref StorageKit | @ref WritingDataObjects \></center>
<hr>
</div>

The Binder defines a standard data model, which is used for
things such as creating the @ref BinderNamespaceDefn, providing
data to the BListView widget, etc.

Though the data model is documented as part of the @ref StorageKit,
it actually spans between that and the @ref BinderKit.  The core
data model interfaces and client APIs (INode, IIterable, IDatum,
ICatalog, ITable, SNode, SIterator, SDatum) are included in the
@ref BinderKit, but all of the implementation for them is in the
@ref StorageKit.

-# @ref Interfaces
  -# @ref FileSystemMapping
  -# @ref DatabaseMapping
-# @ref DataModelClients
-# @ref DetailedTopics
  -# @ref NamingEntries
  -# @ref EntriesAndIdentity
  -# @ref WhereAreTheFiles
  -# @ref DatumConsiderations
  -# @ref IDatumOrSValue

@section Interfaces The Interfaces

The basic data model is simply a set of well-defined Binder
interfaces, creating a hierarchical namespace of data.  The most
important interfaces are:

- INode: <em>"Walking a path (depth traversal)."</em>  An entry in the namespace, with sub-entries and/or meta-data.
- IDatum: <em>"Manipulating data."</em>  A single piece of atomic data in the namespace.
- IIterable: <em>"Collecting things (breadth traversal)."</em>  API for discovering the available entries/data.
- ICatalog: <em>"Adding and deleting."</em>  API for modifying the entries in an INode.
- ITable: <em>"Manipulating 2d data structures."</em> Special API for objects that contain two-dimension data.

@note Clients will usually use the SNode, SIterator, and SDatum
classes instead of calling these interfaces directly.  These
classes provide the same functionality as the interfaces, in
a somewhat more convenient form.

It is important to note that everything in the namespace is an object.
(Or more specifically, everything is an IBinder.)  This is why there is a specific
IDatum interface: it is the protocol through which you retrieve and place data
at a particular location in the namespace.

Traversal in the namespace only goes from parent to child.
<em>The core data model does not include access from a child back to its parent.</em>
The reason for this is to support hard links in the namespace –
two or more nodes that share a common child.  In this situation the child
has multiple parents, so it is not feasible for it to keep track of its parent.

This does not mean that APIs built on top of the basic data model can't have
back-pointers &mdash; an example would be exposing the view hierarchy in the
namespace &mdash; it just means that it is not a requirement.  For example, the
entire view hierarchy is visible in the namespace, and through the view
hierarchy APIs you can travel back up from a child to its parent.
Similarily, some data space objects may implement the IReferable interface
to provide a full path back to the object.

There are two major use-cases these interfaces are designed
for: filesystems and databases.  Instead of discussing the
interfaces themselves in abstract terms, we will describe
them as concrete mappings to these traditional kinds of
structured data.

@subsection FileSystemMapping The Data Model as a Filesystem

A filesystem is a hierarchical organization of directories,
where a directory can contain other directories as well as
leaf files holding data.  In addition, each file or directory
has some fixed set of meta-data associated with it, often
creation and modification dates and permission flags.

All files and directories in a filesystem implement the INode
interface.  This is to provide meta-data (such as creation and
modification date) about that item, and for directories it is
used to walk a path through the directory hierarchy.

A file also implements the IDatum interface, allowing access
to that file's data.  In particular, the IDatum::Open() method
is the equivalent to opening a file, giving by byte stream
interfaces through which you can read and write data in the file.

A directory, in contrast, implements IIterable and ICatalog.  The
IIterable interface lets you browse through the contents of
the directory &mdash; this is, for example, the API that would be used
to implement an "ls" command.  The ICatalog interface provides
various methods for adding new entries (files or directories) to
the directory, renaming entries, and deleting existing entries.

@subsection DatabaseMapping The Data Model as a Database

A database is a flat collection of tables.  Each table is
a two-dimensional structure, consisting of an arbitrary numbner
of rows, each containing a set data items corresponding to
from a fixed set of columns defined by the table.

The table of a database revolves around the IIterable interface.
This allows you to create queries on the table by calling
IIterable::NewIterator() with the appropriate options, such as
"select" for the columns to include (the projection), "sort_by"
to specify row ordering, and "where" to do filtering.  These
options correspond fairly directly to the SQL operation that
must be done on the underlying database, with the returned
IIterator being a cursor on the query results.

For each database row returned by this IIterator, you get back
an INode containing data of the table's columns for that row.
The INode::Walk() method is used to retrieve the desired column
data by name.

Each of the pieces of data associated with a row and column
is provided through the IDatum interface.  You will usually
access that data through IDatum::Value() to retain type
information.

Note that at each of these levels in the database other secondary
interfaces are often implemented.  For example, tables should
always try to implement ITable to provide some more efficient
mechanisms for modifying and watching their contents.  Tables
also usually implement INode to be able to associate a MIME type and other
meta-data with the table, and if there is a known key column
associated with the table the INode may also allow you to Walk()
to single rows in the table using that column as a name.

Rows usually implement IIterable for you to be able to step through all of
the contents of a row, looking very much like a directory in a
filesystem.  The IDatums under the row, however, usually don't
implement INode because there is no specific meta-data associated with
them.

@section DataModelClients Data Model Clients

Clients of the data model interfaces should work through the SNode,
SIterator, and SDatum classes for ease of use.

One of the most common places you come in contact with the data model
interfaces is in the @ref BinderNamespaceDefn, usually accessed through
an SContext object.  While this class has some conveniences for doing
common operations on the namespace, you can also use SContext::Root()
to directly access the INode that provides the root of its namespace.

@section DetailedTopics Detailed Topics

@subsection NamingEntries Naming Entries

The introduction of meta-data begs the question of how you walk to those
entries.  The meta-data formally exists under each INode as a separate
node called ":", so you can ls that node just like any other node:

@code
$ ls img.png/:
mimeType
creationDate
modifiedDate
@endcode

And you can get to individual pieces of meta-data simply by walking through
the attributes nod:

@code
$ cat img.png/:/mimeType
vnd.palm.catalog/vnd.palm.plain
@endcode

As a convenience, we specify that the ':' at the front of a path name
is a special identifier for the attributes namespace, so you can also
treat it is an entry at the same level as the INode it is associated with:

@code
$ cat img.png/:mimeType
vnd.palm.catalog/vnd.palm.plain
@endcode

This is how you will normally access attributes, and it is important
to allow this so that these attributes can be accessed at the same level
as the normal catalog entries.  For example, consider accessing
different pieces of data in the 'font' service:

@code
$ cat services/font/value
Some text

$ cat services/font/:mimeType
vnd.palm.catalog/vnd.palm.plain
@endcode

However other operations besides Walk() do not expose the meta-data:

@code
$ ls services/font
value
items

$ ls services/font/:
mimeType
creationDate
modifiedDate
@endcode

@subsection EntriesAndIdentity Entries and Identity

The things inside of a node are called entries.  Each entry consists
of one or more Binder interfaces, representing the capabilities of
that entry.  The node will pick one of these Binder interfaces to be
the identity of the entry.  When the node returns that entry, it returns
the IBinder for that selected identity interface.

Data model objects must obey the rule of identity persistence.  This
says that if a client requests an entry and keeps a @e strong reference on it,
then later requests the same entry again, the IBinder of the first
and second requests will be the same.  If, however, the entry in the
node has changed between the first and second requests, then the interface
must return a different IBinder for that entry.  (If the data inside the entry
changes, it is still the same entry, and thus the identity must persist.)
Of course, if a client releases all references on an entry, what it will
receive next time is completely undefined (it must be this way since it
is undefined by the underlying Binder object model as well).

It is important to state this rule explicitly because many data model
implementations will manage their entries in special ways.  Consider, for example,
a directory on a file system.  In general there will not be one object created
up-front for every entry in the underlying file system directory.  Instead, the
directory node will
construct objects for individual entries in the file system on demand,
destroying them later when all clients have finished using them.  A node
is free to manage entries however it wants – creating and destroying actual
INode and IDatum objects as needed – as long as it publicly obeys entry
identity persistence.

One other implication to be aware of is that holding a @e weak pointer on a
namespace object may not behave as you expect.  For example, trying to
promote that to a strong pointer may fail even though an attempt to
re-retrieve the same object would still succeed.  This is to allow node
implementation to dynamically construct objects on demand, meaning those
constructed objects may be destroyed after all strong references on them
are released.

@subsection WhereAreTheFiles Where Are the Files?

A question that comes up in this namespace is "how do I know whether some
entry is a file?"  Because it is possible to have entries that are both a
datum and a node, it isn't at all clear how you know whether a particular
entry you are looking at should be considered a file.  A good example to
make this more concrete is implementing a file browser:  if you are showing
the user a list of entries in a node and they click on an entry that is
both a datum and a node, do you open the datum or dig down into the node?

For this purpose our definition of a file is "any entry that is a datum".
Thus you will always open an entry if it allows that.  This definition
implies that datums will tend to appear toward the leaves of the namespace;
a datum that appears closer to the root of the namespace will tend to hide
any node beyond it.

At some point in the future we may introduce facilities to map actions to
mimeTypes, so that you could retrieve the mimeType of the object you have
and determine from that whether you should open its data or dig further
into its node.

@subsection DatumConsiderations Datum Considerations

It is very important that, in the formal data model, every piece of data
in the namespace has an object behind it – this object serves as the
identity of that data, through which you can grant access to other entities
in the system, perform monitoring operations, etc.  Simply enforcing this
as the only way to access data, however, would have a significant
performance impact: every access to a data entry would involve transferring 
new object to the client, followed by a second IPC to retrieve the data.

To address this, a client may ask an INode or IIterable to directly return
the contents of an entry's IDatum as an SValue, skipping the intermediate
IDatum object altogether.  This facility makes it much more practical to
interact with small pieces of data (file attributes, individual items in
schema databases, data in the settings catalog, etc) through the standard
namespace.

A node is not required to provide this direct data mechanism – any client
making use of it must be able to deal with receipt of an IDatum object and
retrieving the actual data through that.  An individual node may even have
different behavior for each entry – for example, a file system may allow you
to directly retrieve the data of files less than 512 bytes, but always
return datums for files larger than that.  This direct data access is purely
an optimization hint that the client makes at the time of the request.

The SDatum::FetchValue() and SDatum::FetchTruncatedValue() are very
important conveniences for clients wanting to make use of this optimization.

A similar facility is available for dealing with nodes.  When iterating you
can request that sub-nodes be collapsed into value mappings in the returned
iterator.  This is a very useful optimization for example when using a node
to populate a list view, where the list view needs to know certain entries
from each sub-node to populate the data in its rows.  As with datums, this
optimization is entirely optional and the caller needs to deal gracefully
with situations when it doesn't happen.

The SNode class provides conveniences for dealing with node collapsing.

@subsection IDatumOrSValue IDatum or SValue?

At their core, the IDatum and SValue express a similar concept: a typed
piece of data.  Ignoring the slightly different coating, what is the
difference between these?  Do we really need both of them?

The key thing to understand about these two APIs is their semantic usage.
An SValue is an anonymous blob of data.  You generally pass it by value
(each thing effectively has its own copy of the data), and there is no 
dentity attached to an SValue beyond the very primitive concept of a
C++ "pointer to an object".

An IDatum, in contrast, blatantly carries an identity. It is a Binder
object, meaning that it has a very well-defined identity that can be
carried across processes.  Because of this, it is always passed by
reference – if you give a IDatum to someone else and they modify what
they got, you will see those changes as well . This implies that the datum
API must provide reasonable support for multithreaded accesses, where-as
the SValue API is fundamentally not thread-safe.

Another way to look at this is that an SValue is a raw piece of data,
and an IDatum wraps up an SValue in a Binder interface (plus other
facilities more appropriate for dealing with larger streams of data). 

*/

/*!
@page WritingDataObjects Writing Data Objects

<center>\< @ref BinderDataModel | @ref StorageKit | fini \></center>
<hr>

When implementing your own data model object, you should make use of
the standard C++ base classes that are available.  These classes are
provided for two main reasons:

- They take care of the common types of functionality that is needed,
  so that differing data model implementations will provide the same
  level of functionality and behavior as much as possible.  This also
  means that as these classes gain new functionality, that functionality
  will become available to all implementations that were based on
  the class.

- They can reduce &mdash; often greatly &mdash; the amount of work required to
  implement a data model object.

There are a wide variety of classes available, so it can be daunting
to figure out where to start.  In general, they are designed as layers
of increasingly higher-level functionality for the three core data
model interfaces, INode, IIterable, and IDatum.

This document will start with the highest-level classes, and work
down to the simpler ones.  This should allow you to go down through
the available classes until you find one that will do what you want,
and stop there.  Remember that you should @e always use one of these
classes to implement the INode, IIterable, or IDatum interfaces; as
a very last resort, that will be BGenericNode, BGenericIterable, and
BGenericDatum, respectively.

-# @ref DesignOverview
-# @ref ImplementationHelperClasses
  -# @ref Delegation
  -# @ref DataManager
  -# @ref Tables
  -# @ref Arrays
  -# @ref Nodes
  -# @ref Iterables
  -# @ref Datums
-# @ref Example

@section DesignOverview Design Overview

Before looking at the classes in detail, there are some general
concepts behind their design that should be understood.

In general, these classes provide an implementation of the
@ref BinderDataModel interfaces on top of some other data structure
that you will provide.  Providing the data structure means
implementing a subclass of the desired data model class and filling
in the appropriate virtuals to give it access to your data.

The implementation of the data model classes and the underlying data
they access must be, however, closely related.  In particular, there are
many cases where the implementation will need to do a series of
operations during which it is assured that the back-end data
remains in a consistent state.  Because of this requirement,
the locking approach used for these classes is very different
than what you will see elsewhere.

Almost all of the data model classes explicitly expose their
internal locking, so that it can be synchronized with the
back-end data.  This takes the form of a couple public virtuals
to explicitly lock and unlock the object, and a convention for
users to know when that lock is being held.

@note As mentioned, this locking policy is @e very @e different than
what is used elsewhere in the @ref BinderKit.  You should generally
prefer the Binder's standard locking model (as described in
@ref BinderThreading), since that is more flexible and results in a
much easier to use API.

By convention, all methods in these classes that are called with
the object's lock held (or put another way, must be called while
holding that lock) have the suffix "Locked" in their name.  A
@e new method defined by one of these classes that does not have
the "Locked" suffix @e must be called @e without holding the lock
(since it may need to acquire the lock in its implementation).

In some cases, a method without the "Locked" suffix may be safe
to call with or without the lock held; this situation will be
documented.  It is also, of course, safe to call the SAtom,
BBinder, and other standard Binder APIs with or without the lock
being held, since these use the standard @ref BinderThreading model.

For example, compare BGenericNode::SetMimeType() to
BGenericNode::SetMimeTypeLocked().

The Lock() methods return a lock_status_t result, making them
convenient to use with the SAutolock class, like so:

@code
void set_catalog_mime_type(const sptr<BCatalog>& catalog)
{
	SAutolock _l(catalog->Lock());
	catalog->StoreMimeTypeLocked(mime_type);
}
@endcode

When mixing two classes together that have their own locks, you
will usually need to re-implement their Lock() and Unlock() methods
so that they share the same lock:

@code
class NodeAndIterable : public BMetaDataNode, public BIndexedIterable
{
public:
	virtual lock_status_t Lock() const { return BMetaDataNode::Lock(); }
	virtual void Unlock() const        { return BMetaDataNode::Unlock(); }
};
@endcode

This goes along with the standard mixing of the IBinder::Inspect() method
that is done when mixing together multiple interfaces.

@section ImplementationHelperClasses Implementation Helper Classes

This section gives an overview and summary of the available helper
classes for implementing data objects, going from highest level to
lowest.

@subsection Delegation Delegation

These classes allow you to wrap an existing data model object,
providing an implementation that modifies its behavior.  For example,
you could use them to rename all of the entries in a node from @e X to
@e foo-X.

- BCatalogDelegate is a delegate to an object implementing the full
catalog API &mdash; INode, IIterable, and ICatalog.

- BNodeDelegate is a delagate to an object implementing INode.

@subsection Tables Tables

A common data structure is that of a table: at the top an array
of rows, each of which itself contains an array of columns.  This is
used, for example, to provide data to a list view.

- BIndexedTableNode is a base class for providing a table structure.
Given the implementation of a few virtuals to define the rows and
columns and data access for individual table cells, this class will
expose that structure as a hierarchical data model layout of nodes
and datums.

@subsection Arrays Arrays

An array is the simplest data structure available, an ordered series
of values.  More complicated data structures are created arrays by
nesting arrays inside of each other, such as the rows inside of a
table.

- BStructuredNode is an array representing a fixed structure of data,
like a C structure.  That is, the array has a fixed size with each
index in the array corresponding to a well-defined field in the
structure.

- BCatalog is a generic data container holding an array of arbitrary
values.  Unlike most other data classes discussed here, BCatalog
has its own build-in data management.  That is, it is a concrete
class that you can instantiate without further specialization.

- BIndexedDataNode is a generic class for implementing an array,
defining nothing about what indices into the array means or what
data they may hold.  This is used to implement both BCatalog and
BStructuredNode.

@subsection Nodes Nodes

These classes provide the most generic support for implementing
the INode interface.

- BMetaDataNode is a node that maintains its own basic metadata:
MIME type, creation date, and modification date.

- BGenericNode is the most generic class, when all else fails.

@subsection Iterables Iterables

These classes provide the most generic support for imlementing
the IIterable, and thus IIterator and IRandomIterator, interfaces.

- BIndexedIterable is an iterator over an indexed series of data
items.  That is, the data is in an array with indices starting
at 0 and incrementing by 1 up to the last item.

- BGenericIterable is the most generic class, when all else fails.

@subsection Datums Datums

These classes provide the most generic support for implementing
the IDatum interface.

- SDatumGeneratorInt is a helper class for dynamically generating
datum objects that provide access to a set of data items.  These
datum objects reference back to their corresponding data item
through an integer key, making this useful for example to provide
access to an array of data.  Unlike the other array classes,
however, there is no ordering required of the integer keys &mdash; they
can be any integer value desired.

- BValueDatum is a datum providing access to a single pieces of
typed data (an SValue).  Unlike most other classes discussed here,
this is a concrete class that you can instantiate without further
specialization &mdash; it maintains the data it is holding.

- BStreamDatum provides all of the basic IDatum functionality, in
particular implementing IDatum::Open() to generate stream objects.

- BGenericDatum is the most generic class.  You should almost always
use BStreamDatum instead, so that you don't have to implement
your own stream management.

@section Example Example

This example shows the implementation of a "content provider" for
contact/person information.  It uses BSchemaDatabaseNode to expose
multiple database tables in various ways.  The top-level content
provider is a BStructuredNode, used to publish a fixed set of
entries at that level for selection of the various kinds of data
(tables) contained inside of it.

@note The classes used here are part of an old implementation of
the data model on top of Cobalt's Data Manager APIs.  They are not
built as part of the current OpenBinder distribution, but are
included to illustration how this kind of data can be presented
in the data model.

@code
#include <support/Package.h>
#include <storage/StructuredNode.h>
#include <storage/SchemaDatabaseNode.h>
#include <storage/SchemaRowIDJoin.h>

#include <support/Autolock.h>
#include <support/Iterator.h>
#include <support/Node.h>

#include <DataMgr.h>
#include <Loader.h>
#include <SchemaDatabases.h>

// Top-level content provider object, containing sub-directories
// to access the various tables of information.
class AddressContentProvider : public BStructuredNode, public SPackageSptr
{
public:
									AddressContentProvider(const SContext& context, const SValue& args);
			void					InitAtom();

	virtual	SValue					ValueAtLocked(size_t index) const;

private:
			sptr<BSchemaDatabaseNode>	m_database;
			sptr<BSchemaTableNode>		m_people;
			sptr<BSchemaTableNode>		m_phones;
			sptr<BSchemaTableNode>		m_extras;
			sptr<BSchemaRowIDJoin>		m_phonesDir;
			sptr<BSchemaRowIDJoin>		m_extrasDir;
};

// ---------------------------------------------------------------

// Various constants we will use elsewhere.

B_STATIC_STRING_VALUE_LARGE(kPersonListMimeType, "application/vnd.palm.personlist" ,);
B_STATIC_STRING_VALUE_LARGE(kPersonItemMimeType, "application/vnd.palm.personitem" ,);
B_STATIC_STRING_VALUE_LARGE(kPhoneListMimeType, "application/vnd.palm.phonelist" ,);
B_STATIC_STRING_VALUE_LARGE(kPhoneItemMimeType, "application/vnd.palm.phoneitem" ,);
B_STATIC_STRING_VALUE_LARGE(kPersonExtrasListMimeType, "application/vnd.palm.personextraslist" ,);
B_STATIC_STRING_VALUE_LARGE(kPersonExtrasItemMimeType, "application/vnd.palm.personextrasitem" ,);
B_CONST_STRING_VALUE_LARGE(kAddressDBName, "AddressDBSNu" ,);
B_CONST_STRING_VALUE_LARGE(kAddressMainTableName, "AddressBookMain" ,);
B_CONST_STRING_VALUE_LARGE(kAddressPhoneTableName, "AddressBookPhone" ,);
B_CONST_STRING_VALUE_LARGE(kAddressExtraTableName, "AddressBookExtra" ,);

// ---------------------------------------------------------------

// This class is used to implement a custom database column
// based on the "FirstName" and "LastName" columns.

const char* displayColumns[] = { "FirstName", "LastName", NULL };

class DisplayName : public BSchemaTableNode::CustomColumn, public SPackageSptr
{
public:
	DisplayName(const sptr<BSchemaTableNode>& table)
		: CustomColumn(table)
	{
	}

	virtual SValue ValueLocked(uint32_t columnOrRowID, const SValue* baseValues,
	                           const size_t* columnsToValues) const
	{
		SString result(baseValues[columnsToValues[1]].AsString());
		const SString first(baseValues[columnsToValues[0]].AsString());
		if (first != "") result += ", ";
		result += first;
		return SValue::String(result);
	}
};

// ---------------------------------------------------------------

// These are the top-level directories in the content provider.
const char* kDirNames[] = {
	"people",
	"phones",
	"extras",
	"databases",
};
enum {
	kPeopleDir = 0,
	kPhonesDir,
	kExtrasDir,
	kDatabasesDir,
	kDirCount
};

AddressContentProvider::AddressContentProvider(const SContext& context, const SValue& args)
	: BStructuredNode(context, kDirNames, kDirCount)
{
}

// Main initialization of content provider.
void AddressContentProvider::InitAtom()
{
	BStructuredNode::InitAtom();

	// Try to open the database.  If it doesn't exist, create it.
	SDatabase database(((const SString&)kAddressDBName).String(), 'add2',
		dmModeReadWrite, dbShareRead);
	if (database.StatusCheck() != errNone) {
		// Error opening database -- try to create it in case it doesn't exist.
		MemHandle	memh;
		void *		memp;
		DatabaseID	dbid;
		status_t	err = errNone;

		DmOpenRef appdb;
		SysGetModuleDatabase(SysGetRefNum(), NULL, &appdb);

		if ((memh = DmGetResource(appdb, 'scdb', SCHEMA_DEFINITION_ID)) == NULL)
			return;

		if ((memp = MemHandleLock(memh)) == NULL)
		{
			DmReleaseResource(memh);
			return;
		}

		if ((err = DmCreateDatabaseFromImage(memp, &dbid)) < errNone)
		{
			return;
		}

		MemHandleUnlock(memh);
		DmReleaseResource(memh);

		database = SDatabase(((const SString&)kAddressDBName).String(), 'add2',
			dmModeReadWrite, dbShareRead);
	}

	// Create the database node, and build up our content
	// provider's subdirectories from it.
	if (database.StatusCheck() == errNone) {

		// Create database object and retrieve its tables.
		m_database = new BSchemaDatabaseNode(Context(), database);
		m_database->Lock();
		m_people = m_database->TableForLocked(kAddressMainTableName);
		m_phones = m_database->TableForLocked(kAddressPhoneTableName);
		m_extras = m_database->TableForLocked(kAddressExtraTableName);
		m_database->Unlock();

		// Create a join between the people and phone number tables.
		if (m_people != NULL && m_phones != NULL) {
			m_phonesDir = new BSchemaRowIDJoin(Context(), m_people, m_phones,
				kPersonID, kperson);
			if (m_phonesDir != NULL) {
				SAutolock _l(m_phonesDir->Lock());
				m_phonesDir->StoreMimeTypeLocked(kPhoneListMimeType);
				m_phonesDir->StoreRowMimeTypeLocked(kPhoneItemMimeType);
			}
		}

		// Create a join between the people and extra info tables.
		if (m_people != NULL && m_extras != NULL) {
			m_extrasDir = new BSchemaRowIDJoin(Context(), m_people, m_extras,
				kPersonID, kperson);
			if (m_emailsDir != NULL) {
				SAutolock _l(m_emailsDir->Lock());
				m_extrasDir->StoreMimeTypeLocked(kPersonExtrasListMimeType);
				m_extrasDir->StoreRowMimeTypeLocked(kPersonExtrasItemMimeType);
			}
		}

		// Add a custom column to the people table and set its MIME type.
		if (m_people != NULL) {
			sptr<DisplayName> dname = new DisplayName(m_people);
			if (dname != NULL)
				dname->AttachColumn(SString("DisplayName"), displayColumns);
			SAutolock _l(m_people->Lock());
			m_people->StoreMimeTypeLocked(kPersonListMimeType);
			m_people->StoreRowMimeTypeLocked(kPersonItemMimeType);
		}
	}
}

// Provide access to our sub-directories.
SValue AddressContentProvider::ValueAtLocked(size_t index) const
{
	switch (index) {
		case kPeopleDir: return SValue::Binder((BnNode*)m_people.ptr());
		case kPhonesDir: return SValue::Binder((BnNode*)m_phonesDir.ptr());
		case kExtrasDir: return SValue::Binder((BnNode*)m_extrasDir.ptr());
		case kDatabasesDir: return SValue::Binder((BnNode*)m_database.ptr());
	}
	return SValue::Undefined();
}
@endcode

*/
