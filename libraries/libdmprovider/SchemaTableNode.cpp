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

#include <dmprovider/SchemaTableNode.h>

#include <support/Autolock.h>
#include <support/StdIO.h>

#include <DateTime.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace dmprovider {
#endif

// *********************************************************************************
// ** BSchemaTableNode *************************************************************
// *********************************************************************************

BSchemaTableNode::BSchemaTableNode(const SContext& context, const SDatabase &database,
								   const SString& table, const SString& keyColumn, const SString& refPath)
	: BMetaDataNode(context)
	, BGenericIterable(context)
	, BnCatalog(context)
	, BnTable(context)
	, BnReferable(context)
	, m_database(database)
	, m_table(table)
	, m_keyColumn(keyColumn)
	, m_refPath(refPath)
	, m_rowMimeType(MimeTypeLocked())
	, m_columnNameToIndex(B_ENTRY_NOT_FOUND)
	, m_accessorsLock("BSchemaTableNode::m_accessorsLock")
{
	m_status = m_database.Status();
	m_ref = m_database.GetOpenRef();
}

BSchemaTableNode::~BSchemaTableNode()
{
}

void BSchemaTableNode::InitAtom()
{
	if (m_status == B_OK) retrieve_schema();
}

status_t BSchemaTableNode::StatusCheck()
{
	return m_status;
}

lock_status_t BSchemaTableNode::Lock() const
{
	return BMetaDataNode::Lock();
}

void BSchemaTableNode::Unlock() const
{
	BMetaDataNode::Unlock();
}

SValue BSchemaTableNode::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	SValue res(BMetaDataNode::Inspect(caller, which, flags));
	res.Join(BGenericIterable::Inspect(caller, which, flags));
	res.Join(BnCatalog::Inspect(caller, which, flags));
	res.Join(BnTable::Inspect(caller, which, flags));
	if (m_refPath != "") res.Join(BnReferable::Inspect(caller, which, flags));
	return res;
}

void BSchemaTableNode::SetMimeType(const SString&)
{
}

SString BSchemaTableNode::RowMimeTypeLocked() const
{
	return m_rowMimeType;
}

status_t BSchemaTableNode::StoreRowMimeTypeLocked(const SString& mimeType)
{
	m_rowMimeType = mimeType;
	return B_OK;
}

SString BSchemaTableNode::TableName() const
{
	return m_table;
}

status_t BSchemaTableNode::LookupEntry(const SString& entry, uint32_t flags, SValue* node)
{
	sptr<RowNode> n;
	status_t err = LookupNode(entry, &n);
	*node = SValue::Binder((BnNode*)n.ptr());
	return err;
}

status_t BSchemaTableNode::LookupNode(const SString& entry, sptr<RowNode>* outNode)
{
	SAutolock _l(Lock());
	uint32_t rowID = name_to_rowid_l(entry);
	if (rowID != 0) {
		*outNode = NodeForLocked(rowID);
		return *outNode != NULL ? B_OK : B_NO_MEMORY;
	}
	return B_ENTRY_NOT_FOUND;
}

sptr<BSchemaTableNode::RowNode> BSchemaTableNode::NodeForLocked(uint32_t rowID)
{
	sptr<RowNode> node(m_nodes.ValueFor(rowID).promote());
	if (node != NULL) return node;

	node = NewRowNodeLocked(rowID);
	if (node != NULL) {
		m_nodes.AddItem(rowID, node);
	}

	return node;
}

SString BSchemaTableNode::EntryNameForLocked(uint32_t rowID) const
{
	status_t err;
	return entry_key_for_l(rowID, &err).AsString();
}

bool BSchemaTableNode::RowIDIsValidLocked(uint32_t rowID) const
{
	// Check to see if this is a valid row id.
	uint16_t attr;
	return DbGetRowAttr(m_ref, rowID, &attr) == errNone;
}

ssize_t BSchemaTableNode::ColumnIndexOfLocked(const SString& entry) const
{
	return m_columnNameToIndex[entry];
}

SString BSchemaTableNode::ColumnNameAtLocked(size_t index) const
{
	return m_columns[index].name;
}

type_code BSchemaTableNode::ColumnTypeAtLocked(size_t index) const
{
	return m_columns[index].type;
}

size_t BSchemaTableNode::CountColumnsLocked() const
{
	return m_columns.CountItems();
}

void BSchemaTableNode::MapSQLOrderColumn(SValue* inoutColumnName, SValue* inoutOrder) const
{
	ssize_t cid = m_columnNameToIndex[inoutColumnName->AsString()];
	if (cid < 0 || m_columns[cid].customIndex < 0) return;

	const custom_column_info& cci = m_customColumns[m_columns[cid].customIndex];
	inoutColumnName->Undefine();
	const size_t N = cci.baseColumns.CountItems();
	if (N == 1) {
		*inoutColumnName = SValue::String(cci.baseColumns[0]);
	} else {
		for (size_t i=0; i<N; i++) {
			inoutColumnName->JoinItem(SSimpleValue<int32_t>(i), SValue(SValue::String(cci.baseColumns[i]), *inoutOrder));
		}
	}
}

void BSchemaTableNode::ReportChangeAtLocked(size_t rowID, size_t col, const sptr<IBinder>& editor, uint32_t changes, off_t start, off_t length)
{
	TouchLocked();

	//! @todo Should only push this event for iterators with select queries?
	PushIteratorChangedLocked();

	sptr<RowNode> node(m_nodes.ValueFor(rowID).promote());
	if (node != NULL) {
		node->BIndexedDataNode::ReportChangeAtLocked(col, editor, changes, start, length);
	}

	if (BnTable::IsLinked()) {
		if (node == NULL) node = NodeForLocked(rowID);
		PushTableChanged(this, node != NULL ? ITable::CHANGE_DETAILS_SENT : 0, B_UNDEFINED_VALUE);
		if (node != NULL) {
			PushCellModified(this, EntryNameForLocked(rowID), ColumnNameAtLocked(col), (BnNode*)node.ptr());
		}
	}
}

SValue BSchemaTableNode::ColumnNames() const
{
	SValue names;

	SAutolock _l(Lock());
	const size_t N(m_columns.CountItems());
	for (size_t i=0; i<N; i++) names.Join(SValue::String(m_columns[i].name));

	return names;
}

SValue BSchemaTableNode::Schema() const
{
	SValue schema;

	SAutolock _l(Lock());
	const size_t N(m_columns.CountItems());
	for (size_t i=0; i<N; i++) {
		const column_info& ci(m_columns[i]);

		SValue value;
		value.JoinItem(BV_DATABASE_ID, SValue::Int32(ci.id));
		value.JoinItem(BV_DATABASE_MAX_SIZE, SValue::Int32(ci.maxSize));
		value.JoinItem(BV_DATABASE_TYPE, SValue::Int32(ci.type));
		//value.JoinItem(BV_DATABASE_NAME, SValue::Int8(ci.attrib));

		schema.JoinItem(SValue::String(ci.name), value);
	}

	return schema;
}

status_t BSchemaTableNode::CreateRow(SString* inoutName, const SValue& columns, uint32_t flags, sptr<INode>* createdRow)
{
	SAutolock _l(Lock());
	if (m_status != B_OK) return m_status;

	return new_row_l(columns, inoutName, createdRow);
}

status_t BSchemaTableNode::new_row_l(const SValue& columns, SString* newName, sptr<INode>* newRow)
{
	const size_t N(m_columns.CountItems());
	DbSchemaColumnValueType* columnValues = NULL;
	SValue* valueArray = NULL;
	uint32_t count = 0;

	if (N > 0) {
		columnValues = new DbSchemaColumnValueType[N];
		valueArray = new SValue[N];
		
		SValue key;
		void* cookie = NULL;
		while ((count < N) && (columns.GetNextItem(&cookie, &key, &valueArray[count]) == B_OK)) {
			for (size_t i = 0 ; i < N ; i++) {
				if (m_columns[i].customIndex < 0 && key.AsString() == m_columns[i].name) {
					columnValues[count].data = (void*)valueArray[count].Data();
					columnValues[count].dataSize = valueArray[count].Length();
					columnValues[count].columnID = m_columns[i].id;
					count++;
					break;
				}
			}
		}
	}

	uint32_t rowid;
	status_t err = DbInsertRow(m_ref, m_table.String(), count, columnValues, &rowid);

	if (N > 0) {
		delete[] columnValues;
		delete[] valueArray;
	}

	if (err == B_OK) {
		// Get the name of this row.
		SValue nameVal(entry_key_for_l(rowid, &err));
		SString name;
		if (err == B_OK) name = nameVal.AsString(&err);
		if (newName) *newName = name;

		// Push the create event and/or return the node, if needed.
		if (err == B_OK/* && (newRow != NULL || BnNode::IsLinked())*/) {
			sptr<RowNode> node = NodeForLocked(rowid);
			if (node != NULL) {
				if (newRow) *newRow = node.ptr();
				PushNodeChanged(this, INode::CHANGE_DETAILS_SENT, B_UNDEFINED_VALUE);
				PushEntryCreated(this, name, (BnNode*)node.ptr());

				// Now set the custom columns
				SValue key,value;
				void *cookie = NULL;
				ssize_t index = 0;
				while (columns.GetNextItem(&cookie, &key, &value) == B_OK) {
					for (size_t i = 0 ; i < N ; i++) {
						if (m_columns[i].customIndex >= 0 && key.AsString() == m_columns[i].name) {
							node->SetColumnValueLocked(rowid, i, value);
							break;
						}
					}
					index++;
				}
			} else err = B_NO_MEMORY;
		}

		if (err == B_OK) {
			PushIteratorChangedLocked();
		} else {
			// Whoops, an error occurred, remove the row we just made.
			DbDeleteRow(m_ref, rowid);
		}
	}

	return err;
}

status_t BSchemaTableNode::RemoveRow(const sptr<INode>& row)
{
	// XXX Shouldn't do a linear search!  Can we use IsLocal()
	// to get the rowid for a fast lookup?
	SAutolock _l(Lock());

	const size_t N(m_nodes.CountItems());
	for (size_t i=0; i<N; i++) {
		if (row == m_nodes.ValueAt(i).unsafe_ptr_access()) {
			uint32_t rowID = static_cast<RowNode*>(row.ptr())->m_rowID;
			status_t err;
			SString name(entry_key_for_l(rowID, &err).AsString());
			if (err == B_OK && name != "") {
				status_t err = DbDeleteRow(m_ref, rowID);
				if (err == B_OK) {
					//sptr<RowNode> node(m_nodes.ValueFor(rowID).promote());
					PushNodeChanged(this, INode::CHANGE_DETAILS_SENT, B_UNDEFINED_VALUE);
					PushEntryRemoved(this, name);
					PushIteratorChangedLocked();
				}
			}
			return err != B_OK ? err : B_NO_MEMORY;
		}
	}

	return B_ENTRY_NOT_FOUND;
}

status_t BSchemaTableNode::AddColumn(const SString& name, uint32_t type, size_t maxSize, uint32_t flags, const SValue& extras)
{
	ErrFatalError("Not implemented.");
	return B_UNSUPPORTED;

#if 0
	if (!open_database()) return B_ERROR;

	SLocker::Autolock lock(m_lock);

	DbSchemaColumnDefnType column;
	column.id = definition[BV_DATABASE_ID].AsInt32();
	column.maxSize = definition[BV_DATABASE_MAX_SIZE].AsInt32();

	SString name = definition[BV_DATABASE_NAME].AsString();
	strncpy(column.name, name.String(), name.Length());

	column.type = (uint8_t)definition[BV_DATABASE_TYPE].AsInt32();
	column.attrib = (uint8_t)definition[BV_DATABASE_ATTR].AsInt32();
	
	return DbAddColumn(m_ref, m_table.String(), &column);
#endif
}

status_t BSchemaTableNode::RemoveColumn(const SString& name)
{
	ErrFatalError("Not implemented.");
	return B_UNSUPPORTED;

#if 0
	if (!open_database()) return B_ERROR;

	SLocker::Autolock _l(m_lock);

	DbTableDefinitionType* schema = retrieve_schema();
	if (schema == NULL) return B_ERROR;

	for (uint32_t i = 0 ; i < schema->numColumns ; i++)
	{
		if (name == (const char*)schema->columnListP[i].name)
		{
			DbReleaseStorage(m_ref, schema);
			return DbRemoveColumn(m_ref, m_table.String(), schema->columnListP[i].id);
		}
	}

	DbReleaseStorage(m_ref, schema);
	return B_ERROR;
#endif
}

status_t BSchemaTableNode::AddEntry(const SString& /*name*/, const SValue& entry)
{
	// This API sucks, and probably doesn't make sense to implement here.
	//return B_UNSUPPORTED;

	/*	And yet... it is very handy to have an equivalent to INode::REQUEST_DATA.
		Maybe we should fix the API? --geh */
	sptr<INode> node;
	SString name; // should be able to return the name...
	return new_row_l(entry, &name, &node);
	// We can't return the node... in fact, there is NO WAY to return
	// a reference to the object we just created.
	//return node;
}

status_t BSchemaTableNode::RemoveEntry(const SString& name)
{
	SAutolock _l(Lock());
	uint32_t rowID = name_to_rowid_l(name);
	if (rowID != 0) {
		status_t err = DbDeleteRow(m_ref, rowID);
		if (err == B_OK) {
			//sptr<RowNode> node(m_nodes.ValueFor(rowID).promote());
			PushNodeChanged(this, INode::CHANGE_DETAILS_SENT, B_UNDEFINED_VALUE);
			PushEntryRemoved(this, name);
			PushIteratorChangedLocked();
		}
		return err;
	}
	return B_ENTRY_NOT_FOUND;
}

status_t BSchemaTableNode::RenameEntry(const SString& /*entry*/, const SString& /*name*/)
{
	// We manage the names, nobody else!
	return B_UNSUPPORTED;
}

sptr<INode> BSchemaTableNode::CreateNode(SString* name, status_t* outError)
{
	sptr<INode> node;
	status_t err = new_row_l(B_UNDEFINED_VALUE, name, &node);
	if (outError != NULL) *outError = err;
	return node;
}

sptr<IDatum> BSchemaTableNode::CreateDatum(SString* name, uint32_t flags, status_t* err)
{
	if (err) *err = B_UNSUPPORTED;
	return NULL;
}

SString BSchemaTableNode::Reference() const
{
	return m_refPath;
}

sptr<BSchemaTableNode::GenericIterator> BSchemaTableNode::NewGenericIterator(const SValue& args)
{
	return new QueryIterator(Context(), this);
}

sptr<BSchemaTableNode::RowNode> BSchemaTableNode::NewRowNodeLocked(uint32_t rowID)
{
	return new RowNode(Context(), this, rowID);
}

static uint32_t fixed_length_column_size(DbSchemaColumnType type)
{
	switch (type)
	{
		case dbBoolean:
			return sizeof(Boolean);

		case dbChar:
		case dbUInt8:
		case dbInt8:
			return sizeof(uint8_t);

		case dbUInt16:
		case dbInt16:
			return sizeof(uint16_t);

		case dbUInt32:
		case dbInt32:
			return sizeof(uint32_t);

		case dbUInt64:
		case dbInt64:
			return sizeof(uint64_t);

		case dbTime:
			return sizeof(TimeType);

		case dbDate:
			return sizeof(DateType);

		case dbDateTime:
			return sizeof(DateTimeType);

		case dbDateTimeSecs:
			return sizeof(time_t);

		case dbFloat:
			return sizeof(float);

		case dbDouble:
			return sizeof(double);

		default:
			return 0;
	}
}

static type_code db_type_to_SValue_type(DbSchemaColumnType type)
{
	switch (type)
	{
		case dbBoolean:		return B_BOOL_TYPE;
		case dbInt8:
		case dbUInt8:
		case dbChar:		return B_INT8_TYPE;
		case dbDate:
		case dbTime:
		case dbInt16:
		case dbUInt16:		return B_INT16_TYPE;
		case dbDateTimeSecs:
		case dbInt32:
		case dbUInt32: 		return B_INT32_TYPE;
		case dbInt64:
		case dbUInt64:		return B_INT64_TYPE;
		case dbFloat:		return B_FLOAT_TYPE;
		case dbDouble:		return B_DOUBLE_TYPE;
		case dbVarChar:		return B_STRING_TYPE;
		default:			return B_RAW_TYPE;
	}
}

status_t BSchemaTableNode::build_column_info(const DbTableDefinitionType* table, SVector<column_info>* outColumns)
{
	const DbSchemaColumnDefnType* schema = table->columnListP;
	for (uint32_t i=0; i<table->numColumns; i++) {
		if (schema->errCode == errNone) {
			column_info ci;
			ci.id = schema->id;
			ci.index = i;
			ci.customIndex = -1;
			ci.maxSize = fixed_length_column_size(schema->type);
			if (ci.maxSize == 0) {
				ci.maxSize = schema->maxSize;
				ci.varsize = true;
			} else {
				ci.varsize = false;
			}
			ci.name = schema->name;
			ci.type = db_type_to_SValue_type(schema->type);
			ci.dmType = schema->type;

			/*	The following appears to be incorrect.  dbSchemaColWritable is not
				set by default on any columns.  From the documentation, it seems it's
				purpose is to designate columns that are writable even on rows
				marked "read-only".  We do not do anything to obey these semantics
				right now, and this isn't the place for it, since it is a per-row
				decision.  So for now, we just set all database columns to be writable.  --geh */
			//ci.writable = schema->attrib&dbSchemaColWritable ? true : false;
			ci.writable = true;
			outColumns->AddItem(ci);
		}
		schema++;
	}
	return B_OK;
}

void BSchemaTableNode::retrieve_schema()
{
	SAutolock _l(Lock());

	DbTableDefinitionType* tableDef;
	m_status = DbGetTableSchema(m_ref, m_table.String(), &tableDef);
	if (m_status == errNone) {
		m_status = build_column_info(tableDef, &m_columns);
		DbReleaseStorage(m_ref, tableDef);

		const size_t N = m_columns.CountItems();
		for (size_t i=0; i<N; i++) {
			m_columnNameToIndex.AddItem(m_columns[i].name, i);
		}
	}
}

status_t BSchemaTableNode::add_custom_column(const sptr<CustomColumn>& column, const SString& name, const char** baseColumnNames)
{
	SAutolock _l(Lock());

	if (m_columnNameToIndex[name] >= 0) return B_ENTRY_EXISTS;

	custom_column_info cci;
	cci.column = column;
	cci.name = name;
	while (baseColumnNames && *baseColumnNames) {
		SString bname(*baseColumnNames);
		ssize_t bi(m_columnNameToIndex[bname]);
		if (bi < 0) return bi;
		if (m_columns[bi].customIndex >= 0) {
			DbgOnlyFatalError("BSchemaTableNode: Custom columns can't use other custom columns as a base.");
			return B_UNSUPPORTED;
		}
		ssize_t err = cci.baseColumns.AddItem(bname);
		if (err < 0) return err;
		err = cci.baseIndices.AddItem(bi);
		if (err < 0) return err;
		baseColumnNames++;
	}

	ssize_t customIndex = m_customColumns.AddItem(cci);
	if (customIndex < 0) return customIndex;

	size_t N = m_columns.CountItems();

	column_info ci;
	ci.id = 0;
	ci.index = N;
	ci.customIndex = customIndex;
	ci.maxSize = 0;
	ci.name = name;
	ci.type = B_UNDEFINED_TYPE;
	ci.dmType = 0;
	ci.writable = false;
	ssize_t columnIndex = m_columns.AddItem(ci);
	if (columnIndex < 0) {
		m_customColumns.RemoveItemsAt(customIndex);
		return columnIndex;
	}
	ssize_t nameIndex = m_columnNameToIndex.AddItem(ci.name, N);
	if (nameIndex < 0) {
		m_columns.RemoveItemsAt(columnIndex);
		m_customColumns.RemoveItemsAt(customIndex);
		return nameIndex;
	}

	return B_OK;
}

status_t BSchemaTableNode::rem_custom_column(const sptr<CustomColumn>& column, const SString& name)
{
	DbgOnlyFatalError("Not yet implemented!");
	return B_UNSUPPORTED;
}

// Some day these will probably need to access the table if it is using
// a custom column for the name.

uint32_t BSchemaTableNode::name_to_rowid_l(const SString& entry) const
{
	// Entries are named by their row ID number, in the form 0xnnnnnnnn
	uint32_t rowID = 0;
	if (entry.Length() == 10 && entry[(size_t)0] == '0' && entry[(size_t)1] == 'x') {
		for (size_t i=2; i<10; i++) {
			const char c(entry[i]);
			char v;
			if ((c >= '0' && c <= '9')) v = c-'0';
			else if ((c >= 'a' && c <= 'f')) v = c-'a'+10;
			else goto fail;
			rowID = (rowID<<4) | v;
		}
		if (!RowIDIsValidLocked(rowID)) rowID = 0;
	}

fail:
	return rowID;
}

SValue BSchemaTableNode::entry_key_for_l(uint32_t rowID, status_t* outError) const
{
	// note: not using sprintf() because we don't know if
	// hex digits from it will be upper-case or lower-case.
	SValue key;
	void* d = key.BeginEditBytes(B_STRING_TYPE, 11);
	if (d) {
		char* s = (char*)d;
		*s++ = '0';
		*s++ = 'x';
		for (int i=0; i<8; i++) {
			const char v = (char)((rowID>>((7-i)*4))&0xf);
			*s++ = v < 10 ? (v+'0') : (v-10+'a');
		}
		*s++ = 0;
		key.EndEditBytes();
		*outError = B_OK;
		return key;
	}

	*outError = B_NO_MEMORY;
	return key;
}

// =================================================================================

BSchemaTableNode::CustomColumn::CustomColumn(const sptr<BSchemaTableNode>& table)
	: m_table(table)
{
}

BSchemaTableNode::CustomColumn::~CustomColumn()
{
}

status_t BSchemaTableNode::CustomColumn::AttachColumn(const SString& name, const char** baseColumnNames)
{
	return m_table->add_custom_column(this, name, baseColumnNames);
}

status_t BSchemaTableNode::CustomColumn::DetachColumn(const SString& name)
{
	return m_table->rem_custom_column(this, name);
}

status_t BSchemaTableNode::CustomColumn::SetValueLocked(uint32_t columnOrRowID, const SValue& inValue, SValue* inoutRealValues)
{
	// By default, custom columns do not allow writing
	return B_UNSUPPORTED;
}

// =================================================================================

BSchemaTableNode::DataAccessor::DataAccessor(const sptr<BSchemaTableNode>& table)
	: m_table(table)
	, m_knowNumColumns(false)
	, m_numDepColumns(0)
	, m_numBaseColumns(0)
	, m_numCustomColumns(0)
{
}

BSchemaTableNode::DataAccessor::~DataAccessor()
{
	SLocker::Autolock _l(m_table->m_accessorsLock);
	m_table->m_accessors.RemoveItemFor(this);
}

void BSchemaTableNode::DataAccessor::InitAtom()
{
	SLocker::Autolock _l(m_table->m_accessorsLock);
	m_table->m_accessors.AddItem(this);
}

void BSchemaTableNode::DataAccessor::ensure_column_counts_l() const
{
	if (!m_knowNumColumns) const_cast<DataAccessor*>(this)->get_column_counts_l();
}

status_t BSchemaTableNode::DataAccessor::SetColumnsLocked(const SValue& columns, SValue* outSelectedColumns)
{
	if (!columns.IsDefined()) {
		// Give them everything!
		m_knowNumColumns = false;
		get_column_counts_l();
		m_databaseColumns.MakeEmpty();
		m_customColumns.MakeEmpty();
		m_columnNames.Undefine();
		m_indices.MakeEmpty();
		m_values.MakeEmpty();
		m_columnData.MakeEmpty();
		return B_OK;
	}

	status_t err = B_OK;

	if (columns == B_WILD_VALUE) {
		// Just use all columns.
		m_databaseColumns.MakeEmpty();
		m_customColumns.MakeEmpty();
		m_knowNumColumns = false;
		get_column_counts_l();
		if (outSelectedColumns) *outSelectedColumns = columns;
	} else {
		m_knowNumColumns = true;

		// First expand any custom columns.
		// XXX Would be nice to merge this with the second loop below.
		SValue depColumns;
		SValue badColumns;
		SValue k, v;
		void* c = NULL;
		size_t numCustom = 0;
		while (columns.GetNextItem(&c, &k, &v) == B_OK) {
			ssize_t idx = m_table->m_columnNameToIndex[v.AsString()];
			if (idx >= 0) {
				if (m_table->m_columns[idx].customIndex >= 0) {
					const custom_column_info& cci(m_table->m_customColumns[m_table->m_columns[idx].customIndex]);
					for (size_t j=0; j<cci.baseColumns.CountItems(); j++) {
						depColumns.Join(SValue::String(cci.baseColumns[j]));
					}
					numCustom++;
				}
			} else {
				badColumns.Join(v);
			}
		}

		// Figure out the columns...

		SValue allColumns(columns);							// Remove columns that don't exist.
		allColumns.Remove(badColumns);
		const size_t numReq = allColumns.CountItems();
		if (outSelectedColumns) *outSelectedColumns = allColumns;

		depColumns.Remove(allColumns);						// Only keep dependent columns that weren't explicitly requested.
		const size_t numDep = depColumns.CountItems();

		allColumns.Join(depColumns);						// This is now the complete set of columns we need.
		const size_t numAll = allColumns.CountItems();

		err = allColumns.ErrorCheck();
		if (err != B_OK) goto finish;

		err = m_databaseColumns.SetSize(numAll-numCustom);
		if (err != B_OK) goto finish;
		err = m_customColumns.SetSize(numCustom);
		if (err != B_OK) goto finish;

		DbgOnlyFatalErrorIf(numDep != (numAll-numReq), "BSchemaTableNode::DataAccessor: Dependent column count incorrect.");

		m_numDepColumns = numDep;
		m_numBaseColumns = numAll-numCustom-numDep;
		m_numCustomColumns = numCustom;

		c = NULL;
		size_t iD=0, iB=0, iC=0;
		while (allColumns.GetNextItem(&c, &k, &v) == B_OK) {
			ssize_t idx = m_table->m_columnNameToIndex[v.AsString()];
			DbgOnlyFatalErrorIf(idx < 0, "BSchemaTableNode::DataAccessor: Bad column allowed in selection.");
			if (m_table->m_columns[idx].customIndex < 0) {
				if (depColumns.HasItem(B_WILD_VALUE, v))
					m_databaseColumns.EditItemAt(iD++) = idx;
				else
					m_databaseColumns.EditItemAt(numDep + iB++) = idx;
			} else {
				m_customColumns.EditItemAt(iC++) = idx;
			}
		}

		DbgOnlyFatalErrorIf(iD != numDep || iB != (numAll-numCustom-numDep) || iC != numCustom, "BSchemaTableNode::DataAccessor: Column counts out of sync.");
	}

	err = build_mappings_and_indices_l();
	if (err == B_OK) err = m_values.SetSize(m_numDepColumns+m_numBaseColumns+m_numCustomColumns);

finish:
	if (err != B_OK) {
		// So that LoadDataLocked() won't corrupt memory.
		m_values.MakeEmpty();
	}
	return err;
}

size_t BSchemaTableNode::DataAccessor::CountColumnsLocked() const
{
	ensure_column_counts_l();
	return m_numBaseColumns+m_numCustomColumns;
}

status_t BSchemaTableNode::DataAccessor::LoadDataLocked(uint32_t rowID, uint32_t flags, SValue* outData, size_t maxDataSize)
{
	ensure_column_counts_l();
	DbgOnlyFatalErrorIf((m_numBaseColumns+m_numCustomColumns) > 0 && m_values.CountItems() == 0, "BSchemaTableNode::DataAccessor: Must use SetColumnsLocked(B_WILD_VALUE) to use LoadDataLocked() with all columns.");
	if (m_values.CountItems() == 0) {
		// Nothing to do!
		return B_OK;
	}

	const size_t					ND			= m_numDepColumns;
	const size_t					NB			= m_numBaseColumns;
	const size_t					NC			= m_numCustomColumns;
	DbSchemaColumnValueType* const	cdArray		= m_columnData.EditArray();
	SVector<size_t>* const			ccdArray	= m_customColumnData.EditArray();
	SValue* const					valueArray	= m_values.EditArray();

	status_t err = B_OK;
	size_t i;

	// Set up column data to point to data buffers.
	DbSchemaColumnValueType* cdPos = cdArray;
	SValue* valuePos = valueArray;
	for (i=0; i<ND; i++) {
		err = prep_column_read_l(rowID, m_table->m_columns[m_databaseColumns[i]], flags, cdPos++, valuePos++, maxDataSize);
		if (err != B_OK) goto finish;
	}
	for (i=0; i<NB; i++) {
		err = prep_column_read_l(rowID, base_column_info_l(i), flags, cdPos++, valuePos++, maxDataSize);
		if (err != B_OK) goto finish;
	}

	if (ND+NB > 0) {
		err = DbCopyColumnValues(m_table->m_ref, rowID, ND+NB, cdArray);
		if (err != B_OK) {
			if (err != dmErrOneOrMoreFailed) return err;

			// Something bad happened, but only to some of the columns.
			// Find the suckers and deal with them.
			for (i=0; i<ND+NB; i++) {
				DbSchemaColumnValueType& cd(cdArray[i]);
				if (cd.errCode == B_OK) continue;

				SValue& v(valueArray[i]);
				if (cd.data != NULL) {
					v.EndEditBytes();
					cd.data = NULL;
				}
				if (cd.errCode == dmErrNoColumnData) v = B_NULL_VALUE;
				else v = SValue::Status(cd.errCode);
			}
		}
	}

	// Take care of any computed columns.
	for (i=0; i<NC; i++) {
		err = compute_column_l(rowID, custom_column_info_l(i), flags, valueArray, ccdArray[i].Array(), valueArray+ND+NB+i, maxDataSize);
		if (err != B_OK) goto finish;
	}

	*outData = m_columnNames;
	DbgOnlyFatalErrorIf((m_values.CountItems()-ND) != m_indices.CountItems(), "BSchemaTableNode::DataAccessor: Value array out of sync with indices.");
	outData->ReplaceValues(valueArray+ND, m_indices.Array(), m_indices.CountItems());
	err = outData->StatusCheck();

finish:
	release_buffers_l();
	return err;
}

SValue BSchemaTableNode::DataAccessor::ColumnValueLocked(uint32_t rowID, size_t column, uint32_t flags, size_t maxDataSize) const
{
	ensure_column_counts_l();

	// Need to compute this column?
	if (column >= m_numBaseColumns) return computed_column_value_l(rowID, column-m_numBaseColumns, flags, maxDataSize);

	// Base column -- retrieve data directly from DM.

	const column_info& ci(base_column_info_l(column));
	SValue v;

	DbSchemaColumnValueType cd;
	cd.data = NULL;
	cd.dataSize = 0;
	cd.columnID = ci.id;
	cd.columnIndex = 0;
	cd.reserved = 0;

	status_t err = prep_column_read_l(rowID, ci, flags, &cd, &v, maxDataSize);
	if (err != B_OK) goto finish;

	if (cd.data) {
		err = DbCopyColumnValue(m_table->m_ref, rowID, ci.id, 0, cd.data, &cd.dataSize);
		v.EndEditBytes();

		if (err == dmErrNoColumnData) {
			v = B_NULL_VALUE;
			err = B_OK;
		}
	}

finish:
	if (err != B_OK) v = SValue::Status(err);
	return v;
}

status_t BSchemaTableNode::DataAccessor::SetColumnValueLocked(uint32_t rowID, size_t column, const SValue &value)
{
	ensure_column_counts_l();

	// Is this a custom column?
	if (column >= m_numBaseColumns) return set_computed_column_value_l(rowID, column-m_numBaseColumns, value);

	// Base column -- set data directly in DM.
	const column_info& ci(base_column_info_l(column));
	if (!ci.writable) return B_NOT_ALLOWED;
	if (value.Type() != ci.type) return B_BAD_TYPE;
	if (ci.varsize) {
		if (value.Length() > ci.maxSize) return B_BAD_VALUE;
	} else {
		if (value.Length() != ci.maxSize) return B_BAD_VALUE;
	}

	return DbWriteColumnValue(Table()->m_ref, rowID, ci.id, 0, -1, value.Data(), value.Length());
}

size_t BSchemaTableNode::DataAccessor::ColumnSizeLocked(uint32_t rowID, size_t column) const
{
	ensure_column_counts_l();

	// Need to compute this column?
	if (column >= m_numBaseColumns) {
		SValue v = computed_column_value_l(rowID, column-m_numBaseColumns, INode::REQUEST_DATA, 0x7fffffff);
		return v.Length();
	}

	const column_info& ci(ColumnInfoLocked(column));
	if (!ci.varsize) return ci.maxSize;
	uint32_t size = 0;
	DbCopyColumnValue(m_table->m_ref, rowID, ci.id, 0, NULL, &size);
	return size;
}

size_t BSchemaTableNode::DataAccessor::ColumnIDLocked(size_t column) const
{
	const column_info& ci(ColumnInfoLocked(column));
	return ci.id;
}

const BSchemaTableNode::column_info& BSchemaTableNode::DataAccessor::ColumnInfoLocked(size_t column) const
{
	ensure_column_counts_l();
	const size_t NB = m_numBaseColumns;
	return column < NB ? base_column_info_l(column) : custom_column_info_l(column-NB);
}

void BSchemaTableNode::DataAccessor::get_column_counts_l()
{
	m_knowNumColumns = true;
	m_numDepColumns = 0;
	m_numCustomColumns = m_table->m_customColumns.CountItems();
	m_numBaseColumns = m_table->m_columns.CountItems() - m_numCustomColumns;
}

const BSchemaTableNode::column_info& BSchemaTableNode::DataAccessor::base_column_info_l(size_t column) const
{
	return m_table->m_columns[m_databaseColumns.CountItems() > 0 ? m_databaseColumns[m_numDepColumns+column] : column];
}

const BSchemaTableNode::column_info& BSchemaTableNode::DataAccessor::custom_column_info_l(size_t column) const
{
	return m_table->m_columns[m_customColumns.CountItems() > 0 ? m_customColumns[column] : (column+m_numBaseColumns)];
}

status_t BSchemaTableNode::DataAccessor::build_mappings_and_indices_l()
{
	const size_t ND = m_numDepColumns;
	const size_t NB = m_numBaseColumns;
	const size_t NC = m_numCustomColumns;
	if (m_columnData.SetSize(ND+NB) != B_OK) return B_NO_MEMORY;
	if (m_customColumnData.SetSize(NC) != B_OK) return B_NO_MEMORY;

	SVector<SValue> indexKeys;

	// Build mappings and array of SValue keys.
	m_columnNames.Undefine();
	SValue v;
	for (size_t i=0; i<(ND+NB+NC); i++) {
		if (i < (ND+NB)) {
			const column_info& ci = i < ND ? m_table->m_columns[m_databaseColumns[i]] : base_column_info_l(i-ND);
			v = SValue(ci.name);
			DbSchemaColumnValueType& dat(m_columnData.EditItemAt(i));
			dat.data = NULL;
			dat.dataSize = 0;
			dat.columnID = ci.id;
			dat.columnIndex = 0;
			dat.reserved = 0;
		} else {
			const column_info& ci = custom_column_info_l(i-NB-ND);
			v = SValue(ci.name);
			DbgOnlyFatalErrorIf(ci.customIndex < 0, "BSchemaTableNode::DataAccessor: Trying to build mappings for a custom column, that is not!");
			SVector<size_t>& v(m_customColumnData.EditItemAt(i-NB-ND));
			const custom_column_info& cci(m_table->m_customColumns[ci.customIndex]);
			if (m_customColumns.CountItems() == 0) {
				v = cci.baseIndices;
			} else {
				for (size_t i=0; i<cci.baseIndices.CountItems(); i++) {
					size_t j;
					for (j=0; j<ND+NB; j++) {
						if (m_databaseColumns[j] == cci.baseIndices[i]) {
							if (v.AddItem(j) < 0) return B_NO_MEMORY;
							break;
						}
					}
					DbgOnlyFatalErrorIf(j >= ND+NB, "BSchemaTableNode::DataAccessor: Unable to find base column required for a custom column!");
				}
			}
		}

		if (i >= ND) {
			m_columnNames.JoinItem(v, B_WILD_VALUE);
			indexKeys.AddItem(v);
		}
	}
	if (m_columnNames.ErrorCheck() != B_OK) return m_columnNames.ErrorCheck();
	if (indexKeys.CountItems() != (NB+NC)) return B_NO_MEMORY;

	DbgOnlyFatalErrorIf(m_columnNames.CountItems() != NB+NC, "BSchemaTableNode::DataAccessor: SValue map size out of sync with index array.");

	// Find indices from columns to mapping.
	if (m_indices.SetSize(NB+NC) != B_OK) return B_NO_MEMORY;
	m_columnNames.GetKeyIndices(indexKeys.Array(), m_indices.EditArray(), NB+NC);
	
	return B_OK;
}

status_t BSchemaTableNode::DataAccessor::prep_column_read_l(uint32_t row, const column_info& ci, uint32_t flags, DbSchemaColumnValueType* outData, SValue* intoValue, size_t maxDataSize) const
{
	status_t err;

	if ((flags&INode::REQUEST_DATA) == 0) {
		// If not requesting data, we must return the object, so jump there.
		goto return_object;
	}

	if (ci.varsize) {
		// Note: If for any reason we won't be getting this
		// column's data, we leave outData->data NULL so that we
		// won't try to copy it later on.
		err = DbCopyColumnValue(m_table->m_ref, row, ci.id, 0, NULL, &outData->dataSize);
		if (err != B_OK) {
			if (err == dmErrNoColumnData) *intoValue = B_NULL_VALUE;
			else *intoValue = SValue::Status(err);
			return B_OK;
		}
		if (outData->dataSize > maxDataSize) {
return_object:
			sptr<RowNode> node(m_table->NodeForLocked(row));
			if (node == NULL) return B_NO_MEMORY;
			sptr<IDatum> datum(node->DatumAtLocked(ci.index));
			if (datum == NULL) return B_NO_MEMORY;
			*intoValue = SValue::Binder(datum->AsBinder());
			return B_OK;
		}
	} else {
		outData->dataSize = ci.maxSize;
	}
	outData->data = intoValue->BeginEditBytes(ci.type, outData->dataSize);

	return (outData->data != NULL ? B_OK : B_NO_MEMORY);
}

status_t BSchemaTableNode::DataAccessor::set_computed_column_value_l(uint32_t row, size_t column, const SValue &inValue)
{
	const column_info& ci(custom_column_info_l(column));
	DbgOnlyFatalErrorIf(ci.customIndex < 0, "BSchemaTableNode::DataAccessor: Trying to set value on a custom column that is not.");
	const custom_column_info& cci = m_table->m_customColumns[ci.customIndex];

	SValue* values = NULL;
	SValue* inoutRealValues = NULL;
	size_t* indices = NULL;
	const SVector<size_t>& baseIndices(cci.baseIndices);
	const size_t NB = baseIndices.CountItems();
	if (NB > 0) {
		values = new SValue[NB];
		if (values == NULL) return B_NO_MEMORY;
		inoutRealValues = new SValue[NB];
		if (inoutRealValues == NULL) {
			delete[] values;
			return B_NO_MEMORY;
		}
		for (size_t i=0; i<NB; i++) {
			values[i] = ColumnValueLocked(row, baseIndices[i], INode::REQUEST_DATA, 0x7fffffff);
			inoutRealValues[i] = values[i];
		}
	}

	status_t err = cci.column->SetValueLocked(row, inValue, inoutRealValues);

	if (err == B_OK) {
		for (size_t i=0;i<NB;i++) {
			if (inoutRealValues[i] != values[i]) {
				err = SetColumnValueLocked(row, baseIndices[i], inoutRealValues[i]);
			}
		}
	}

	delete[] values;
	delete[] inoutRealValues;

	return err;
}

status_t BSchemaTableNode::DataAccessor::compute_column_l(uint32_t row, const column_info& ci, uint32_t flags, const SValue* baseValues, const size_t* columnToValue, SValue* intoValue, size_t maxDataSize) const
{
	DbgOnlyFatalErrorIf(ci.customIndex < 0, "BSchemaTableNode::DataAccessor: Trying to compute a custom column that is not.");
	const custom_column_info& cci = m_table->m_customColumns[ci.customIndex];

	if ((flags&INode::REQUEST_DATA) == 0) {
		// If not requesting data, we must return the object, so jump there.
		goto return_object;
	}

	*intoValue = cci.column->ValueLocked(row, baseValues, columnToValue);
	if (intoValue->Length() > maxDataSize) {
return_object:
		sptr<RowNode> node(m_table->NodeForLocked(row));
		if (node == NULL) return B_NO_MEMORY;
		sptr<IDatum> datum(node->DatumAtLocked(ci.index));
		if (datum == NULL) return B_NO_MEMORY;
		*intoValue = SValue::Binder(datum->AsBinder());
		return B_OK;
	}

	return intoValue->AsStatus();
}

void BSchemaTableNode::DataAccessor::release_buffers_l()
{
	const size_t NDB = m_numDepColumns+m_numBaseColumns;
	const size_t NC = m_numCustomColumns;
	size_t i;
	for (i=0; i<NDB; i++) {
		DbSchemaColumnValueType& cd(m_columnData.EditItemAt(i));
		SValue& v(m_values.EditItemAt(i));
		if (cd.data != NULL) {
			v.EndEditBytes();
			cd.data = NULL;
		} else if (v.IsObject()) {
			v.Undefine();
		}
	}
	for (i=0; i<NC; i++) {
		SValue& v(m_values.EditItemAt(NDB+i));
		if (v.IsObject()) {
			v.Undefine();
		}
	}
}

SValue BSchemaTableNode::DataAccessor::computed_column_value_l(uint32_t rowID, size_t column, uint32_t flags, size_t maxDataSize) const
{
	const column_info& ci(custom_column_info_l(column));
	DbgOnlyFatalErrorIf(ci.customIndex < 0, "BSchemaTableNode::DataAccessor: Trying to compute a custom column that is not.");
	const custom_column_info& cci = m_table->m_customColumns[ci.customIndex];

	DbgOnlyFatalErrorIf(m_values.CountItems() > 0, "BSchemaTableNode::DataAccessor::ColumnValueLocked() not supported with a schema.");

	// XXX Should have these arrays pre-allocated (but not initialized)
	// in BSchemaTableNode to avoid malloc overhead.
	SValue* values = NULL;
	size_t* indices = NULL;
	const SVector<size_t>& baseIndices(cci.baseIndices);
	const size_t NB = baseIndices.CountItems();
	if (NB > 0) {
		values = new SValue[NB];
		if (values == NULL) return SValue::Status(B_NO_MEMORY);
		indices = new size_t[NB];
		if (indices == NULL) {
			delete[] values;
			return SValue::Status(B_NO_MEMORY);
		}
		for (size_t i=0; i<NB; i++) {
			values[i] = ColumnValueLocked(rowID, baseIndices[i], flags|INode::REQUEST_DATA, 0x7fffffff);
			indices[i] = i;
		}
	}

	SValue result;
	status_t err = compute_column_l(rowID, ci, flags, values, indices, &result, maxDataSize);
	if (err != B_OK) result = SValue::Status(err);

	delete[] indices;
	delete[] values;
	return result;
}

// =================================================================================

BSchemaTableNode::RowNode::RowNode(const SContext& context, const sptr<BSchemaTableNode>& owner, uint32_t rowID)
	: BIndexedDataNode(context)
	, DataAccessor(owner)
	, m_rowID(rowID)
{
}

BSchemaTableNode::RowNode::~RowNode()
{
	SAutolock _l(Lock());
	if (Table()->m_nodes.ValueFor(m_rowID) == this)
		Table()->m_nodes.RemoveItemFor(m_rowID);
}

lock_status_t BSchemaTableNode::RowNode::Lock() const
{
	return Table()->Lock();
}

void BSchemaTableNode::RowNode::Unlock() const
{
	return Table()->Unlock();
}

SValue BSchemaTableNode::RowNode::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	SValue res(BIndexedDataNode::Inspect(caller, which, flags));
	if (Table()->m_refPath != "") res.Join(BnReferable::Inspect(caller, which, flags));
	res.Join(BnCatalog::Inspect(caller, which, flags));
	return res;
}

void BSchemaTableNode::RowNode::InitAtom()
{
	BIndexedDataNode::InitAtom();
	DataAccessor::InitAtom();
}

status_t BSchemaTableNode::RowNode::FinishAtom(const void* )
{
	return FINISH_ATOM_ASYNC;
}

bool BSchemaTableNode::RowNode::HoldRefForLink(const SValue& , uint32_t )
{
	return true;
}

SString BSchemaTableNode::RowNode::MimeTypeLocked() const
{
	return Table()->RowMimeTypeLocked();
}

status_t BSchemaTableNode::RowNode::StoreMimeTypeLocked(const SString& value)
{
	return Table()->StoreRowMimeTypeLocked(value);
}

size_t BSchemaTableNode::RowNode::CountEntriesLocked() const
{
	return Table()->CountColumnsLocked();
}

ssize_t BSchemaTableNode::RowNode::EntryIndexOfLocked(const SString& entry) const
{
	return Table()->ColumnIndexOfLocked(entry);
}

SString BSchemaTableNode::RowNode::EntryNameAtLocked(size_t index) const
{
	return Table()->ColumnNameAtLocked(index);
}

status_t BSchemaTableNode::RowNode::StoreValueTypeAtLocked(size_t index, uint32_t type)
{
	return B_NOT_ALLOWED;
}

off_t BSchemaTableNode::RowNode::SizeAtLocked(size_t index) const
{
	return ColumnSizeLocked(m_rowID, index);
}

status_t BSchemaTableNode::RowNode::StoreSizeAtLocked(size_t index, off_t size)
{
	const size_t s = (size_t)size;
	if (s != size) return B_OUT_OF_RANGE;

	const column_info& ci(ColumnInfoLocked(index));
	if (!ci.writable) return B_NOT_ALLOWED;
	if (ci.varsize) {
		if (s > ci.maxSize) return B_OUT_OF_RANGE;
	} else {
		return (s == ci.maxSize) ? B_OK : B_OUT_OF_RANGE;
	}

	size_t curSize = ColumnSizeLocked(m_rowID, index);
	if (s < curSize) {
		// Shrinking.
		return DbWriteColumnValue(Table()->m_ref, m_rowID, ci.id, s, curSize-s, NULL, 0);
	} else {
		// Growing.
		size_t grow = s-curSize;
		char pad[512];
		memset(pad, 0, grow < 512 ? grow : 512);
		while (grow > 0) {
			size_t amt = grow < 512 ? grow : 512;
			status_t err = DbWriteColumnValue(Table()->m_ref, m_rowID, ci.id, curSize, 0, pad, amt);
			if (err != B_OK) return err;
			curSize += amt;
			grow -= amt;
		}
	}
	return B_OK;
}

SValue BSchemaTableNode::RowNode::ValueAtLocked(size_t index) const
{
	// Note: always return a copy of the data if asked.  BIndexedDataNode will take
	// care of returning an INode if the size is too large.
	return ColumnValueLocked(m_rowID, index, INode::REQUEST_DATA, 0x7fffffff);
}

status_t BSchemaTableNode::RowNode::StoreValueAtLocked(size_t index, const SValue& value)
{
	return SetColumnValueLocked(m_rowID, index, value);
}

status_t BSchemaTableNode::RowNode::AddEntry(const SString& name, const SValue& entry)
{
	SAutolock _l(Lock());
	int32_t index = Table()->ColumnIndexOfLocked(name);
	if (index >= 0) return StoreValueAtLocked(index, entry);
	return index;
}

status_t BSchemaTableNode::RowNode::RemoveEntry(const SString& /*name*/)
{
	return B_UNSUPPORTED;
}

status_t BSchemaTableNode::RowNode::RenameEntry(const SString& /*entry*/, const SString& /*name*/)
{
	return B_UNSUPPORTED;
}

sptr<INode> BSchemaTableNode::RowNode::CreateNode(SString* /*name*/, status_t* err)
{
	*err = B_UNSUPPORTED;
	return NULL;
}

sptr<IDatum> BSchemaTableNode::RowNode::CreateDatum(SString* /*name*/, uint32_t /*flags*/, status_t* err)
{
	*err = B_UNSUPPORTED;
	return NULL;
}

void BSchemaTableNode::RowNode::ReportChangeAtLocked(size_t index, const sptr<IBinder>& editor, uint32_t changes, off_t start, off_t length)
{
	Table()->ReportChangeAtLocked(m_rowID, index, editor, changes, start, length);
}

SString BSchemaTableNode::RowNode::Reference() const
{
	SString ref(Table()->Reference());
	ref.PathAppend(Table()->EntryNameForLocked(m_rowID));
	return ref;
}

// =================================================================================

BSchemaTableNode::QueryIterator::QueryIterator(const SContext& context, const sptr<BSchemaTableNode>& owner)
	: GenericIterator(context, owner.ptr())
	, DataAccessor(owner)
	, m_ref(owner->m_ref)
	, m_status(B_OK)
	, m_cursor(dbInvalidCursorID)
{
}

BSchemaTableNode::QueryIterator::~QueryIterator()
{
	if (m_cursor != dbInvalidCursorID)
	{
		DbCursorClose(m_cursor);
	}
}

SValue BSchemaTableNode::QueryIterator::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	return BnRandomIterator::Inspect(caller, which, flags);
}

void BSchemaTableNode::QueryIterator::InitAtom()
{
	GenericIterator::InitAtom();
	DataAccessor::InitAtom();
}

status_t BSchemaTableNode::QueryIterator::FinishAtom(const void* )
{
	return FINISH_ATOM_ASYNC;
}

SValue BSchemaTableNode::QueryIterator::Options() const
{
	SAutolock _l(Owner()->Lock());
	return m_args;
}

status_t BSchemaTableNode::QueryIterator::Remove()
{
	return B_UNSUPPORTED;
}

size_t BSchemaTableNode::QueryIterator::Count() const
{
	SAutolock _l(Owner()->Lock());

	if (m_cursor == dbInvalidCursorID)
	{
		DbgOnlyFatalError("Using an invalid cursor in BSchemaTableNode::QueryIterator");
		return 0;
	}

	return DbCursorGetRowCount(m_cursor);
}


size_t BSchemaTableNode::QueryIterator::Position() const
{
	SAutolock _l(Owner()->Lock());

	status_t err;
	uint32_t pos;

	if (m_cursor == dbInvalidCursorID)
	{
		DbgOnlyFatalError("Using an invalid cursor in BSchemaTableNode::QueryIterator");
		return dbCursorBOFPos;
	}

	err = DbCursorGetCurrentPosition(m_cursor, &pos);
	if (err)
	{
		// XXX?!?
		return dbCursorBOFPos;
	}

	// Convert the 1-based cursor offset to a 0-based IRandomIterator offset
	return pos - 1;
}


void BSchemaTableNode::QueryIterator::SetPosition(size_t value)
{
	SAutolock _l(Owner()->Lock());

	status_t err;
	uint32_t pos;

	if (m_cursor == dbInvalidCursorID)
	{
		DbgOnlyFatalError("Using an invalid cursor in BSchemaTableNode::QueryIterator");
		return;
	}

	// Convert the 0-based IRandomIterator offset to a 1-based cursor offset
	pos = value + 1;

	err = DbCursorSetAbsolutePosition(m_cursor, pos);
#if BUILD_TYPE != BUILD_TYPE_RELEASE
	if (err)
	{
		if (DbCursorGetRowCount(m_cursor) != 0)
		{
			DbgOnlyNonFatalError("Error setting position");
		}
	}
#endif
}

status_t BSchemaTableNode::QueryIterator::ParseArgs(const SValue& args)
{
	// -----------------------------------------------------------------
	// Retrieve column information for the new cursor.

	Owner()->Lock();
	SValue columns(args[BV_ITERABLE_SELECT]);
	if (!columns.IsDefined()) columns = B_WILD_VALUE;
	SValue selected;
	m_status = SetColumnsLocked(columns, &selected);
	if (m_status == B_OK && args[BV_ITERABLE_SELECT].IsDefined()) {
		if (selected.IsWild()) {
			// This is the case where they specified B_WILD_VALUE
			// for the selection.  Should we actually expand it into
			// the columns that were selected?
			selected.Undefine();
			const size_t N(CountColumnsLocked());
			for (size_t i=0; i<N; i++) {
				selected.Join(SValue::String(ColumnInfoLocked(i).name));
			}
		}
	}
	Owner()->Unlock();

	if (m_status != B_OK) {
		DbgOnlyFatalError("[BSchemaTableNode::QueryIterator]: failed to compute column mappings");
		return m_status;
	}

	// -----------------------------------------------------------------
	// Create the SQL query string to be used by this iterator.

	SString sql, whereStr, orderByStr;
	m_args = args;
	// Note: Data Manager doesn't currently do selection, we did it
	// above on our own.
	m_status = BuildSQLSubStrings(NULL, &whereStr, &orderByStr, &m_args);
	//m_status = BuildSQLString(&sql, &m_args, Table()->TableName());

	if (m_status != B_OK) {
		DbgOnlyFatalError("[BSchemaTableNode::QueryIterator]: bad iterator args!");
		return m_status;
	}

	// These are the columns we selected ourselves above.
	m_args.JoinItem(BV_ITERABLE_SELECT, selected);

	// Now build the full query.
	sql.Append(Table()->TableName());
	if (whereStr != "") {
		sql.Append(" ");
		sql.Append(whereStr);
	}
	if (orderByStr != "") {
		sql.Append(" ");
		sql.Append(orderByStr);

		// Make sure the data manager has a sort index for this.
		orderByStr.Prepend(" ");
		orderByStr.Prepend(Table()->TableName());
		m_status = DbAddSortIndex(m_ref, orderByStr.String());
		if (m_status != B_OK && m_status != dmErrAlreadyExists) {
			DbgOnlyFatalError("[BSchemaTableNode::QueryIterator]: failed to create sort index!");
		}
	}

	// -----------------------------------------------------------------
	// Open a cursor for our query.

	m_status = DbCursorOpen(m_ref, sql.String(), 0, &m_cursor);
	if (m_status != B_OK) {
		DbgOnlyFatalError("[BSchemaTableNode::QueryIterator]: failed to open cursor!");
		return m_status;
	}

	m_status = DbCursorMoveFirst(m_cursor);
	if (m_status != B_OK) {
		// if the cursor does not contain any records just return
		if (m_status == dmErrCursorBOF) return m_status = B_OK;
		DbgOnlyFatalError("[BSchemaTableNode::QueryIterator]: failed to set the cursors position");
	}

	return B_OK;
}

SValue BSchemaTableNode::QueryIterator::CreateSQLFilter(const SValue& filter) const
{
	SValue query = GenericIterator::CreateSQLFilter(filter);
	if (query.IsDefined()) return query;

	// At this point we know that our DataAccessor base class has been set up
	// with the selected columns, since we did that first in ParseArgs().
	// So just go through the columns and build a query to match against any
	// of the ones that are a string type.

	Owner()->Lock();
	const size_t N = CountColumnsLocked();
	const SValue kLIKE("PS_LIKE");
	const SValue kOR("OR");
	for (size_t i=0; i<N; i++) {
		const column_info& ci = ColumnInfoLocked(i);
		if (ci.type == B_STRING_TYPE && ci.customIndex < 0) {
			SValue expr(BV_ITERABLE_WHERE_LHS, SValue::String(ci.name));
			expr.JoinItem(BV_ITERABLE_WHERE_OP, kLIKE);
			expr.JoinItem(BV_ITERABLE_WHERE_RHS, filter);
			if (query.IsDefined()) {
				expr = SValue(BV_ITERABLE_WHERE_LHS, expr);
				expr.JoinItem(BV_ITERABLE_WHERE_OP, kOR);
				expr.JoinItem(BV_ITERABLE_WHERE_RHS, query);
			}
			query = expr;
		}
	}
	Owner()->Unlock();

	return query;
}

status_t BSchemaTableNode::QueryIterator::NextLocked(uint32_t flags, SValue* key, SValue* entry)
{
	if (m_status != B_OK) return m_status;
	if (m_cursor == dbInvalidCursorID) return B_NO_INIT;
	if (DbCursorIsEOF(m_cursor)) return B_END_OF_DATA;

	status_t err;

	// Always want to fill in the key, which for now is just the row
	// id.  (Will need to be more complicated when we support a key
	// column.)
	uint32_t row;
	DbCursorGetCurrentRowID(m_cursor, &row);
	if (key) {
		*key = Table()->entry_key_for_l(row, &err);
		if (err != B_OK) return err;
	}

	// If they have asked for both collapsing and data, we can do
	// our super highly-efficient optimization here.  Otherwise, we
	// need to go through the row's INode.
	if ((flags & (INode::COLLAPSE_CATALOG|INode::REQUEST_DATA)) == (INode::COLLAPSE_CATALOG|INode::REQUEST_DATA)) {

		// First, if this row has been deleted, just return without any data.
		if (DbCursorIsDeleted(m_cursor)) {
			entry->Undefine();
			err = DbCursorMoveNext(m_cursor);
			return (err == dmErrCursorEOF) ? B_OK : err;
		}

		err = LoadDataLocked(row, flags, entry);
		if (err == B_OK) {
			err = DbCursorMoveNext(m_cursor);
			if (err == dmErrCursorEOF) err = B_OK;
		} else if (err == dmErrNoColumnData || err == dmErrRecordDeleted) {
			// Silently deal with this.  Should we do something better?
			*entry = B_UNDEFINED_VALUE;
			err = B_OK;
		}
		return err;
	}

	err = DbCursorMoveNext(m_cursor);
	if (err != B_OK) {
		if (err == dmErrCursorEOF) err = B_OK;
		else return err;
	}

	sptr<RowNode> obj = Table()->NodeForLocked(row);
	if (obj == NULL) return B_NO_MEMORY;
	*entry = SValue::Binder((BnNode*)obj.ptr());
	return B_OK;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
