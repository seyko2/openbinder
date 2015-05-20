/*
 * Copyright (c) 2005 Palmsource, Inc.
 * 
 * This software is licensed as described in the file LICENSE, which
 * you should have received as part of this distribution. The terms
 * are also available at http://www.openbinder.org/license.html.
 * 
 * This software consists of voluntary contributions made by many
 * individuals. For the exact contribution history, see the revision
 * history and logs, available at http://www.openbinder.org
 */

#ifndef _DMPROVIDER_SCHEMATABLENODE_H
#define _DMPROVIDER_SCHEMATABLENODE_H

/*!	@file dmprovider/SchemaTableNode.h
	@ingroup CoreDataManagerProvider
	@brief INode providing access to a single table in the Data Manager.
*/

#include <storage/IndexedDataNode.h>
#include <storage/GenericIterable.h>
#include <storage/MetaDataNode.h>
#include <storage/SDatabase.h>

#include <support/ICatalog.h>
#include <support/ITable.h>
#include <storage/IReferable.h>

#include <SchemaDatabases.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace dmprovider {
using namespace palmos::storage;
#endif

/*!	@addtogroup CoreDataManagerProvider
	@{
*/

B_CONST_STRING_VALUE_SMALL(BV_DATABASE_ID,			"id", );
B_CONST_STRING_VALUE_LARGE(BV_DATABASE_MAX_SIZE,	"max_size", );
B_CONST_STRING_VALUE_LARGE(BV_DATABASE_TYPE,		"type", );
B_CONST_STRING_VALUE_LARGE(BV_DATABASE_ATTR,		"attr", );
B_CONST_STRING_VALUE_LARGE(BV_DATABASE_NAME,		"name", );

//!	INode providing access to a single table in the Data Manager.
/*!	This class can be instantiated by yourself on a particular
	schema database, or generated for you by BSchemaDatabaseNode.

	@todo Implement efficient IDatum stream read/write operations.
	@todo Fix data access to use cursor ID instead of always row ID.
	@todo Implement adding and removing of columns in underlying table.
	@todo Make everything update when columns are added/removed.

	@nosubgrouping
	
	@note These classes are not a part of the OpenBinder build, but included
	as an example for other similar implementations.
*/
class BSchemaTableNode : public BMetaDataNode, public BGenericIterable,
	public BnCatalog, public BnTable, public BnReferable
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									BSchemaTableNode(	const SContext& context,
														const SDatabase& database,
														const SString& table,
														const SString& keyColumn = SString(),
														const SString& refPath = SString());
protected:
	virtual							~BSchemaTableNode();
public:

			//!	Disambiguate.
	virtual	lock_status_t			Lock() const;
			//!	Disambiguate.
	virtual	void					Unlock() const;
			//!	Disambiguate.
	inline	SContext				Context() { return BMetaDataNode::Context(); }
			//!	Make INode, IIterable, ITable, and IReferable accessible.
	virtual	SValue					Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);
			//!	Open and initialize the database.
	virtual	void					InitAtom();
			//!	Returns initialization state of object, such as error code when opening database.
			status_t				StatusCheck();

	//@}

	// --------------------------------------------------------------
	/*!	@name Configuration
		Functions for customizing how the table is exposed.  See the
		CustomColumn class for the API for adding new computed columns
		to the table. */
	//@{

			//!	Base class for implementing special computed/generated columns in the table.
			class CustomColumn;

			//!	By default, clients are not allowed to modify the MIME type.
			/*!	To use the normal BMetaDataNode implementation, override
				this to call directly to BGenericNode::SetMimeType(). 
				To change the MIME type yourself, you can call
				SetMimeTypeLocked(). */
	virtual	void					SetMimeType(const SString& value);

			//!	Retrieve the MIME type of entries (row nodes).
			SString					RowMimeTypeLocked() const;

			//!	Set the MIME type that all entries (row nodes) will report.
			status_t				StoreRowMimeTypeLocked(const SString& mimeType);

			//!	Return the name of this table.
			SString					TableName() const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Data Model Implementation
		Provide default implementation based on new capabilities. */
	//@{

			//!	An INode representing a row in the database table.
			class RowNode;
			//!	Special classes for directly retrieving data from database table.
			class DataAccessor;

			//!	Implemented by calling LookupNode().
	virtual status_t				LookupEntry(const SString& entry, uint32_t flags, SValue* node);

			//!	Implemented by converting name to a rowID and then calling NodeForLocked().
	virtual status_t				LookupNode(const SString& entry, sptr<RowNode>* outNode);

			//!	Return INode object for a particular table row ID.
			sptr<RowNode>			NodeForLocked(uint32_t rowID);

			//!	Return the name associated with a particular row ID.
			SString					EntryNameForLocked(uint32_t rowID) const;

			//!	Return true if the given row ID exists in the table.
			bool					RowIDIsValidLocked(uint32_t rowID) const;

			//!	Return the index for a column entry name in the table.
			/*!	Returns B_ENTRY_NOT_FOUND if no column with the given name exists. */
			ssize_t					ColumnIndexOfLocked(const SString& entry) const;

			//!	Return the name of a column at a given index.
			/*!	The index must be in the range of [0..CountColumnsLocked()].  It
				does no bounds checking for you. */
			SString					ColumnNameAtLocked(size_t index) const;

			//!	Return the type of a column at a given index.
			/*!	The index must be in the range of [0..CountColumnsLocked()].  It
				does no bounds checking for you. */
			type_code				ColumnTypeAtLocked(size_t index) const;

			//!	Returns the total number of columns in the table.
			size_t					CountColumnsLocked() const;

			//!	Map custom columns to underlying database columns when sorting by them.
	virtual	void					MapSQLOrderColumn(SValue* inoutColumnName, SValue* inoutOrder) const;

			//!	Similar to BStreamDatum::ReportChangeLocked(), using same change flags.
			/*!	Also pushes the ITable change events. */
	virtual	void					ReportChangeAtLocked(size_t rowid, size_t col, const sptr<IBinder>& editor, uint32_t changes, off_t start=-1, off_t length=-1);

	//@}

	// --------------------------------------------------------------
	/*! @name ITable Implementation */
	//@{

	virtual	SValue					ColumnNames() const;
	virtual	SValue					Schema() const;
	virtual	status_t				CreateRow(SString* name, const SValue& columns, uint32_t flags = 0, sptr<INode>* createdRow = NULL);
	virtual	status_t				RemoveRow(const sptr<INode>& row);
	virtual	status_t				AddColumn(const SString& name, uint32_t typeCode, size_t maxSize, uint32_t flags, const SValue& extras);
	virtual	status_t				RemoveColumn(const SString& name);

	//@}

	// --------------------------------------------------------------
	/*! @name ICatalog Implementation */
	//@{

	virtual	status_t				AddEntry(const SString& name, const SValue& entry);
	virtual	status_t				RemoveEntry(const SString& name);
	virtual	status_t				RenameEntry(const SString& entry, const SString& name);
	
	virtual	sptr<INode>				CreateNode(SString* name, status_t* err);
	virtual	sptr<IDatum>			CreateDatum(SString* name, uint32_t flags, status_t* err);

	//@}

	// --------------------------------------------------------------
	/*! @name IReferable Implementation */
	//@{

	virtual	SString					Reference() const;

	//@}

	//  ------------------------------------------------------------------
	/*!	@name Object Generation */
	//@{

			//!	Iterator over rows in the table, providing access to SQL queries on the underlying database table.
			class QueryIterator;

	virtual	sptr<GenericIterator>	NewGenericIterator(const SValue& args);

			//!	Called when a new row object needs to be created.
			/*!	The default implementation instantiates and returns a
				new IndexedDatum object. */
	virtual	sptr<RowNode>			NewRowNodeLocked(uint32_t rowID);

	//@}

private:
									BSchemaTableNode(const BSchemaTableNode&);
			BSchemaTableNode&		operator=(const BSchemaTableNode&);

	friend class CustomColumn;
	friend class DataAccessor;
	friend class RowNode;
	friend class QueryIterator;

			struct column_info
			{
				size_t		id;				// DM id for this column
				size_t		index;			// local index of column
				ssize_t		customIndex;	// if custom column, index into that array; else < 0.
				size_t		maxSize;		// maximum data in column
				SString		name;			// name of column
				type_code	type;			// local type code of column
				uint32_t	dmType;			// DM type code of column
				bool		writable : 1;	// can be modified by clients?
				bool		varsize: 1;		// is this variable-length data?
			};

			struct custom_column_info
			{
				sptr<CustomColumn>	column;
				SString				name;
				SVector<SString>	baseColumns;
				SVector<size_t>		baseIndices;
			};

	static	status_t				build_column_info(const DbTableDefinitionType* table, SVector<column_info>* outColumns);
			void					retrieve_schema();
			status_t				new_row_l(const SValue& columns, SString* newName = NULL, sptr<INode>* newRow = NULL);
			status_t				add_custom_column(const sptr<CustomColumn>& column, const SString& name, const char** baseColumnNames);
			status_t				rem_custom_column(const sptr<CustomColumn>& column, const SString& name);
			uint32_t				name_to_rowid_l(const SString& entry) const;
			SValue					entry_key_for_l(uint32_t rowID, status_t* outError) const;

	const	SDatabase								m_database;
	const	SString									m_table;
	const	SString									m_keyColumn;
	const	SString									m_refPath;

			status_t								m_status;
			DmOpenRef								m_ref;
			SString									m_rowMimeType;
			SVector<column_info>					m_columns;
			SVector<custom_column_info>				m_customColumns;
			SKeyedVector<SString, ssize_t>			m_columnNameToIndex;
			SKeyedVector<uint32_t, wptr<RowNode> >	m_nodes;

			SLocker									m_accessorsLock;
			SSortedVector<wptr<DataAccessor> >		m_accessors;
};

// --------------------------------------------------------------------------

//!	Base class for implementing custom column data in BSchemaTableNode.
/*!	Derive from this class and implement its virtuals, then add an
	instance to the table with BSchemaTableNode::AddCustomColumn().

	@nosubgrouping
*/
class BSchemaTableNode::CustomColumn : public virtual SAtom
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction. */
	//@{
											CustomColumn(const sptr<BSchemaTableNode>& table);
	virtual									~CustomColumn();

	inline	const sptr<BSchemaTableNode>&	Table() const { return m_table; }

	//@}

	// --------------------------------------------------------------
	/*!	@name Column Management
		Attach/detach from table. */
	//@{

			//!	Attach this custom data to a column name in the table.
			status_t						AttachColumn(const SString& name, const char** baseColumnNames = NULL);

			//!	Detach this custom column from the table.
			status_t						DetachColumn(const SString& name);

	//@}

	// --------------------------------------------------------------
	/*!	@name Data Computation
		Bind to columns, retrieve column data. */
	//@{

			//!	Retrieve data for this column.
	virtual	SValue							ValueLocked(uint32_t columnOrRowID, const SValue* baseValues, const size_t* columnsToValues) const = 0;
	virtual	status_t						SetValueLocked(uint32_t columnOrRowID, const SValue& inValue, SValue* inoutRealValues);

	//@}

private:
			const sptr<BSchemaTableNode>	m_table;
};

// --------------------------------------------------------------------------

//!	Class providing direct access to BSchemaTableNode data.
/*!	This class provides efficient access to data in the table.

	@nosubgrouping
*/
class BSchemaTableNode::DataAccessor : public virtual SAtom
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction. */
	//@{
			//!	Instantiate object and create connection to table.
											DataAccessor(const sptr<BSchemaTableNode>& table);
protected:
	virtual									~DataAccessor();
			//!	Attach to the owning BSchemaTableNode.
	virtual	void							InitAtom();
public:

	inline	const sptr<BSchemaTableNode>&	Table() const { return m_table; }

	//@}

	// --------------------------------------------------------------
	/*!	@name Column Management
		Bind to columns, retrieve column information. */
	//@{

			status_t						SetColumnsLocked(const SValue& columns, SValue* outSelectedColumns = NULL);
			size_t							CountColumnsLocked() const;
			size_t							ColumnIDLocked(size_t column) const;
			const column_info&				ColumnInfoLocked(size_t column) const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Data setting/retrieval
		Get/set data from table from a particular row. */
	//@{

			status_t						LoadDataLocked(uint32_t rowID, uint32_t flags, SValue* outData, size_t maxDataSize = 256);
			SValue							ColumnValueLocked(uint32_t rowID, size_t column, uint32_t flags, size_t maxDataSize = 2048) const;
			size_t							ColumnSizeLocked(uint32_t rowID, size_t column) const;

			status_t						SetColumnValueLocked(uint32_t rowID, size_t column, const SValue &value);

	//@}

private:
	inline	void							ensure_column_counts_l() const;
			void							get_column_counts_l();
			const column_info&				base_column_info_l(size_t column) const;
			const column_info&				custom_column_info_l(size_t column) const;
			status_t						build_mappings_and_indices_l();
			status_t						prep_column_read_l(uint32_t row, const column_info& ci, uint32_t flags, DbSchemaColumnValueType* outData, SValue* intoValue, size_t maxDataSize) const;
			status_t						compute_column_l(uint32_t row, const column_info& ci, uint32_t flags, const SValue* baseValues, const size_t* columnToValue, SValue* intoValue, size_t maxDataSize) const;
			status_t						set_computed_column_value_l(uint32_t row, size_t column, const SValue &inValue);
			void							release_buffers_l();
			SValue							computed_column_value_l(uint32_t rowID, size_t column, uint32_t flags, size_t maxDataSize = 2048) const;

			const sptr<BSchemaTableNode>	m_table;
			bool							m_knowNumColumns;

			// This is the number of columns we need from the database table to compute
			// custom columns, but which were not explicitly requested.
			size_t							m_numDepColumns;
			// This is the number of columns in the database table whose value we have
			// been asked to return.
			size_t							m_numBaseColumns;
			// This is the number of columns we need to return which are computed from
			// other columns in the database.
			size_t							m_numCustomColumns;
			// Mapping from [0..m_numDepColumns..m_numDepColumns+m_numBaseColumns) to
			// the index into the table's column info list.
			SVector<ssize_t>				m_databaseColumns;
			// Mapping from [0..m_numCustomColumns) to the index into the table's
			// column info list.
			SVector<ssize_t>				m_customColumns;

			// These are constructed by build_mappings_and_indices_l().
			SValue							m_columnNames;
			SVector<size_t>					m_indices;				// [0..m_numBaseColumns..m_numBaseColumns+m_numCustomColumns)
			SVector<SValue>					m_values;				// [0..m_numDepColumns..m_numDepColumns+m_numBaseColumns..m_numDepColumns+m_numBaseColumns+m_numCustomColumns)
			SVector<DbSchemaColumnValueType> m_columnData;			// [0..m_numDepColumns..m_numDepColumns+m_numBaseColumns)
			SVector<SVector<size_t> >		m_customColumnData;		// [0..m_numCustomColumns)
};

// --------------------------------------------------------------------------

//!	Node object generated for a row in BSchemaTableNode.
/*!	This object provides read/write access to a particular row in the
	table, based on the table's schema.

	@nosubgrouping
*/
class BSchemaTableNode::RowNode : public BIndexedDataNode, public BnCatalog, public BnReferable, public BSchemaTableNode::DataAccessor
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									RowNode(const SContext& context, const sptr<BSchemaTableNode>& owner, uint32_t rowID);
protected:
	virtual							~RowNode();
			//!	Disambiguate.
	inline	SContext				Context() { return RowNode::Context(); }
			//!	Make BIndexedDataNode, ICatalog and IReferable accessible.
	virtual	SValue					Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);
			//!	Initialize both base classes.
	virtual	void					InitAtom();
			//!	The destructor must call Lock(), so return FINISH_ATOM_ASYNC to avoid deadlocks.
	virtual	status_t				FinishAtom(const void* id);
			//!	Make this object stay around while there are links on it.
	virtual	bool					HoldRefForLink(const SValue& binding, uint32_t flags);
public:

			//!	Reimplemented to use the BSchemaTableNode lock.
	virtual	lock_status_t			Lock() const;
			//!	Reimplemented to use the BSchemaTableNode lock.
	virtual	void					Unlock() const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Implementation
		Reimplement RowNode to modify the database contents. */
	//@{

			//!	Return BSchemaTableNode::RowMimeTypeLocked().
	virtual	SString					MimeTypeLocked() const;
			//!	Return B_UNSUPPORTED (clients can not modify the MIME type).
	virtual	status_t				StoreMimeTypeLocked(const SString& value);
			//!	Return BSchemaTableNode::CountColumnsLocked().
	virtual	size_t					CountEntriesLocked() const;
			//!	Return BSchemaTableNode::ColumnIndexOfLocked().
	virtual	ssize_t					EntryIndexOfLocked(const SString& entry) const;
			//!	Return BSchemaTableNode::ColumnNameAtLocked().
	virtual	SString					EntryNameAtLocked(size_t index) const;
			//!	Return B_UNSUPPORTED -- value type comes from schema column.
	virtual	status_t				StoreValueTypeAtLocked(size_t index, uint32_t type);
			//!	Return size by directly accessing database.
	virtual	off_t					SizeAtLocked(size_t index) const;
			//!	Change size by directly accessing database, if this is a variable size column.
	virtual	status_t				StoreSizeAtLocked(size_t index, off_t size);
			//!	Retrieve data as a value.
	virtual	SValue					ValueAtLocked(size_t index) const;
			//!	Modify contents of database.
	virtual	status_t				StoreValueAtLocked(size_t index, const SValue& value);
			//!	Call BIndexedTableNode::ReportChangeAtLocked(m_index, index, ...).
	virtual	void					ReportChangeAtLocked(size_t index, const sptr<IBinder>& editor, uint32_t changes, off_t start=-1, off_t length=-1);

			//! If the entry exists, call through to StoreValueAtLocked
			/*! It seems like it wouldn't be hard to support all of the "extra"
				fields directly in the regular row, which would make the row impelenting ICatalog
				make a bit more sense.  Not sure if we actually want to do this or not; it's a
				policy decision. */
	virtual	status_t				AddEntry(const SString& name, const SValue& entry);
			//! Return B_UNSUPPORTED - can't remove entries from a single row, have to use ITable
	virtual	status_t				RemoveEntry(const SString& name);
			//! Return B_UNSUPPORTED - can't rename entries in a single row, have to use ITable
	virtual	status_t				RenameEntry(const SString& entry, const SString& name);
			//! Return B_UNSUPPORTED - rows can't create new sub-tables without implementing a join
	virtual	sptr<INode>				CreateNode(SString* name, status_t* err);
			//! Return B_UNSUPPORTED - though should it allow "creation" of data with the right name?
	virtual	sptr<IDatum>			CreateDatum(SString* name, uint32_t flags, status_t* err);

	//@}

	// --------------------------------------------------------------
	/*! @name IReferable Implementation */
	//@{

	virtual	SString					Reference() const;

	//@}

			const uint32_t			m_rowID;

private:
									RowNode(const RowNode&);
			RowNode&				operator=(const RowNode&);
};

// --------------------------------------------------------------------------

//!	Iterator over a BSchemaTableNode.
/*!	Custom iterator implementation that uses a cursor to step over the
	results of a query.

	@nosubgrouping
*/
class BSchemaTableNode::QueryIterator : public BGenericIterable::GenericIterator, public BSchemaTableNode::DataAccessor
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									QueryIterator(const SContext& context, const sptr<BSchemaTableNode>& owner);
protected:
	virtual							~QueryIterator();
			//! We support both IIterator and IRandomIterator.
	virtual SValue					Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);
public:
			//!	Call base class initializers.
	virtual	void					InitAtom();
			//!	Return FINISH_ATOM_ASYNC, same as GenericIterator::FinishAtom().
	virtual	status_t				FinishAtom(const void* id);

	//@}

	virtual	SValue					Options() const;
	virtual status_t				Remove();
	virtual size_t					Count() const;
	virtual	size_t					Position() const;
	virtual void					SetPosition(size_t p);

	virtual	status_t				ParseArgs(const SValue& args);

			//!	Provide default filter on all selected rows.
			/*!	If a derived class has not provided their own implementation of
				BGenericIterable::CreateSQLFilter(), this class will generate
				a filter that matches if any of the selected columns match
				the filter text. */
	virtual	SValue					CreateSQLFilter(const SValue& filter) const;

	virtual	status_t				NextLocked(uint32_t flags, SValue* key, SValue* entry);

private:
									QueryIterator(const QueryIterator&);
			QueryIterator&			operator=(const QueryIterator&);

			DmOpenRef				m_ref;
			status_t				m_status;
			SValue					m_args;
			SString					m_query;

			uint32_t				m_cursor;
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::dmprovider
#endif

#endif // _DMPROVIDER_SCHEMATABLENODE_H
