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

#include <storage/IndexedTableNode.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

/* The size_t value with all bits set */
#define MAX_SIZE_T           (~(size_t)0)

// *********************************************************************************
// ** BIndexedTableNode ************************************************************
// *********************************************************************************

BIndexedTableNode::BIndexedTableNode(uint32_t mode)
	: m_mode(mode)
	, m_entryMimeType(MimeTypeLocked())
	, m_nodes(NULL)
{
}

BIndexedTableNode::BIndexedTableNode(const SContext& context, uint32_t mode)
	: BMetaDataNode(context)
	, BIndexedIterable(context)
	, m_mode(mode)
	, m_entryMimeType(MimeTypeLocked())
	, m_nodes(NULL)
{
}

BIndexedTableNode::~BIndexedTableNode()
{
	if (m_nodes) delete m_nodes;
}

lock_status_t BIndexedTableNode::Lock() const
{
	return BMetaDataNode::Lock();
}

void BIndexedTableNode::Unlock() const
{
	BMetaDataNode::Unlock();
}

SValue BIndexedTableNode::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	SValue res(BMetaDataNode::Inspect(caller, which, flags));
	res.Join(BIndexedIterable::Inspect(caller, which, flags));
	return res;
}

status_t BIndexedTableNode::LookupEntry(const SString& entry, uint32_t flags, SValue* node)
{
	SAutolock _l(Lock());
	ssize_t index = EntryIndexOfLocked(entry);
	if (index < 0) return index;

	return FetchEntryAtLocked(NULL, index, flags, node);
}

status_t BIndexedTableNode::EntryAtLocked(const sptr<IndexedIterator>& it, size_t index, uint32_t flags, SValue* key, SValue* entry)
{
	*key = SValue::String(EntryNameAtLocked(index));
	return FetchEntryAtLocked(it, index, flags, entry);
}

status_t BIndexedTableNode::FetchEntryAtLocked(const sptr<IndexedIterator>& it, size_t index, uint32_t flags, SValue* entry)
{
	if ((flags & COLLAPSE_NODE) != 0) {
		// Need to do collapsing optimization.
		//!	@todo should be smarter about this, maybe even get rid of COLLAPSE_NODE.

		// Start out with a fresh SValue.
		entry->Undefine();

		// If data is not being requested, we need to have the row's node from which
		// we can generate datums.
		sptr<BIndexedDataNode> node;
		if ((flags&REQUEST_DATA) == 0) {
			node = NodeAtLocked(index);
			if (node == NULL) return B_NO_MEMORY;
		}

		// Retrieve selection set.
		SValue select;
		if (it != NULL) select = it->SelectArgsLocked();

		SValue key, value;
		if (select.IsDefined()) {
			// Selection set is defined; retrieve only the items in it.
			void* cookie = NULL;
			while (select.GetNextItem(&cookie, &key, &value) == B_OK) {
				status_t err;
				SString name(value.AsString(&err));
				if (err == B_OK) {
					ssize_t col = ColumnIndexOfLocked(name);
					err = col;
					if (col >= 0) {
						err = FetchValueAtLocked(index, node, col, flags, &key);
						if (err == B_OK) entry->JoinItem(value, key);	// col_name -> fetched_value
					}
				}
			}
		} else {
			// Selection is not defined; just return everything.
			SValue value;
			const size_t N(CountEntriesLocked());
			for (size_t i=0; i<N; i++) {
				if (FetchValueAtLocked(index, node, i, flags, &value) == B_OK)
					entry->JoinItem(SValue::String(ColumnNameAtLocked(i)), value);
			}
		}

		return B_OK;
	}

	sptr<BIndexedDataNode> node = NodeAtLocked(index);
	if (node == NULL) return B_NO_MEMORY;
	*entry = SValue::Binder((BnNode*)node.ptr());
	return B_OK;
}

status_t BIndexedTableNode::FetchValueAtLocked(size_t row, const sptr<BIndexedDataNode>& rowNode, size_t column, uint32_t flags, SValue* entry)
{
	*entry = ValueAtLocked(row, column);
	if ((flags & INode::REQUEST_DATA) == 0 && !entry->IsObject())
	{
		sptr<IDatum> d = rowNode->DatumAtLocked(column);
		sptr<IBinder> b = d->AsBinder();
		if (b == NULL) return B_NO_MEMORY;
		*entry = SValue::Binder(b);
	}

	return B_OK;
}

SValue BIndexedTableNode::ColumnNames() const
{
	SValue names;

	SAutolock _l(Lock());
	const size_t N(CountColumnsLocked());
	for (size_t i=0; i<N; i++) names.Join(SValue::String(ColumnNameAtLocked(i)));

	return names;
}

B_CONST_STRING_VALUE_LARGE(kSchemaType,		"type", );
B_CONST_STRING_VALUE_LARGE(kSchemaName,		"name", );

SValue BIndexedTableNode::Schema() const
{
	SValue schema;

	SAutolock _l(Lock());
	const size_t N(CountColumnsLocked());
	for (size_t i=0; i<N; i++) {
		SValue value;
		//value.JoinItem(BV_DATABASE_ID, SValue::Int32(ci.id));
		//value.JoinItem(BV_DATABASE_MAX_SIZE, SValue::Int32(ci.maxSize));
		value.JoinItem(kSchemaType, SValue::Int32(B_VALUE_TYPE));
		//value.JoinItem(BV_DATABASE_NAME, SValue::Int8(ci.attrib));

		schema.JoinItem(SValue::String(ColumnNameAtLocked(i)), value);
	}

	return schema;
}

status_t BIndexedTableNode::CreateRow(SString* inoutName, const SValue& columns, uint32_t flags, sptr<INode>* createdRow)
{
	return B_UNSUPPORTED;
}

status_t BIndexedTableNode::RemoveRow(const sptr<INode>& row)
{
	return B_UNSUPPORTED;
}

status_t BIndexedTableNode::AddColumn(const SString& name, uint32_t type, size_t maxSize, uint32_t flags, const SValue& extras)
{
	return B_UNSUPPORTED;
}

status_t BIndexedTableNode::RemoveColumn(const SString& name)
{
	return B_UNSUPPORTED;
}

void BIndexedTableNode::SetMimeType(const SString&)
{
}

SString BIndexedTableNode::EntryMimeTypeLocked() const
{
	return m_entryMimeType;
}

status_t BIndexedTableNode::StoreEntryMimeTypeLocked(const SString& mimeType)
{
	m_entryMimeType = mimeType;
	return B_OK;
}

void BIndexedTableNode::CreateSelectionLocked(SValue* inoutSelection, SValue* outCookie) const
{
	SValue which(*inoutSelection);
	SValue key, value;
	void* cookie = NULL;
	while (which.GetNextItem(&cookie, &key, &value) == B_OK) {
		status_t err;
		SString name(value.AsString(&err));
		if (err == B_OK) {
			ssize_t col = ColumnIndexOfLocked(name);
			if (col >= 0) continue;
		}
		inoutSelection->RemoveItem(key, value);
	}
}

sptr<BIndexedDataNode> BIndexedTableNode::NodeAtLocked(size_t index)
{
	sptr<BIndexedDataNode> node = ActiveNodeAtLocked(index);
	if (node != NULL) return node;

	// If we don't already have the node vector, create it now.
	if (!m_nodes) m_nodes = new SKeyedVector<size_t, wptr<RowNode> >;
	if (!m_nodes) return node;

	sptr<RowNode> newNode(NewRowNodeLocked(index, m_mode));
	if (newNode != NULL) {
		m_nodes->AddItem(index, newNode);
		return newNode.ptr();
	}

	// Error when creating datum -- delete datum vector if it is
	// empty.
	if (m_nodes->CountItems() == 0) {
		delete m_nodes;
		m_nodes = NULL;
	}

	return node;
}

sptr<BIndexedDataNode> BIndexedTableNode::ActiveNodeAtLocked(size_t index)
{
	sptr<BIndexedDataNode> node;
	if (m_nodes) {
		bool found;
		const wptr<RowNode>& n(m_nodes->ValueFor(index, &found));
		if (found) {
			node = n.promote().ptr();
		}
	}
	return node;
}

void BIndexedTableNode::UpdateEntryIndicesLocked(size_t index, ssize_t delta)
{
	if (!m_nodes) return;
	ssize_t pos = m_nodes->CeilIndexOf(index);
	if (pos < 0) return;

	size_t N(m_nodes->CountItems());
	const size_t endDel=index-delta;
	while ((size_t)pos < N) {
		// Warning: nasty ugly const cast to avoid a lot of overhead!
		size_t& posIndex = const_cast<size_t&>(m_nodes->KeyAt(pos));
		// NOTE: We know that all objects in the list are valid, because
		// they are removed as part of their destruction.  So we don't
		// need to do a promote here.  In fact, we don't WANT to do a
		// promote, because we absolutely need to update m_index even if
		// it is in the process of being destroyed.
		RowNode* node = m_nodes->ValueAt(pos).unsafe_ptr_access();
		if (posIndex >= index) {
			if (delta < 0 && posIndex < endDel) {
				m_nodes->RemoveItemsAt(pos);
				// should come up with a better error code.
				//! @todo Should we push something when the entry is deleted?
				if (node != NULL) node->m_index = B_ENTRY_NOT_FOUND;
				N--;
			} else {
				posIndex += delta;
				if (node != NULL) node->m_index = posIndex;
			}
		}
	}
}

void BIndexedTableNode::UpdateColumnIndicesLocked(size_t index, ssize_t delta)
{
	const size_t N(m_nodes->CountItems());
	for (size_t i=0; i<N; i++) {
		// NOTE: We know that all objects in the list are reasonably valid,
		// because they are removed as part of their destruction.  So we don't
		// need to do a promote here.
		RowNode* node = m_nodes->ValueAt(i).unsafe_ptr_access();
		node->UpdateDatumIndicesLocked(index, delta);
	}
}

void BIndexedTableNode::SetValueAtLocked(size_t row, size_t col, const SValue& value)
{
	SValue oldValue = ValueAtLocked(row, col);
	if (oldValue != value) {
		const uint32_t oldType = oldValue.IsDefined() ? oldValue.Type() : ValueTypeAtLocked(row, col);
		const off_t oldSize = oldValue.IsDefined() ? (off_t)oldValue.Length() : SizeAtLocked(row, col);
		oldValue.Undefine(); // no longer needed.
		const status_t err = StoreValueAtLocked(row, col, value);
		if (err == B_OK) {
			uint32_t changes = BStreamDatum::DATA_CHANGED;	// assume all data has changed; don't do memcmp().
			if (oldType != value.Type()) changes |= BStreamDatum::TYPE_CHANGED;
			if (oldSize != value.Length()) changes |= BStreamDatum::SIZE_CHANGED;
			ReportChangeAtLocked(row, col, NULL, changes, 0, oldSize >= value.Length() ? oldSize : value.Length());
		}
	}
}

uint32_t BIndexedTableNode::ValueTypeAtLocked(size_t row, size_t col) const
{
	return ValueAtLocked(row, col).Type();
}

status_t BIndexedTableNode::StoreValueTypeAtLocked(size_t row, size_t col, uint32_t type)
{
	// XXX NULL-terminate string values?
	SValue tmp(ValueAtLocked(row, col));
	status_t err = tmp.SetType(type);
	if (err == B_OK) {
		return StoreValueAtLocked(row, col, tmp);
	}
	return err;
}

off_t BIndexedTableNode::SizeAtLocked(size_t row, size_t col) const
{
	return ValueAtLocked(row, col).Length();
}

status_t BIndexedTableNode::StoreSizeAtLocked(size_t row, size_t col, off_t size)
{
	const size_t s = (size_t)size;
	if (s != size) return B_OUT_OF_RANGE;

	SValue tmp(ValueAtLocked(row, col));

	const size_t len = tmp.Length();
	void* d = tmp.BeginEditBytes(tmp.Type(), s, B_EDIT_VALUE_DATA);
	if (d) {
		if (s > len) memset(((char*)d)+len, 0, s-len);
		tmp.EndEditBytes(s);
		return StoreValueAtLocked(row, col, tmp);
	}

	return B_NO_MEMORY;
}

void BIndexedTableNode::ReportChangeAtLocked(size_t row, size_t col, const sptr<IBinder>& editor, uint32_t changes, off_t start, off_t length)
{
	TouchLocked();

	//! @todo Should only push this event for iterators with select queries?
	PushIteratorChangedLocked();

	sptr<BIndexedDataNode> node = ActiveNodeAtLocked(row);
	if (node != NULL) {
		node->BIndexedDataNode::ReportChangeAtLocked(col, editor, changes, start, length);
	}

	if (BnTable::IsLinked()) {
		if (node == NULL) node = NodeAtLocked(row);
		PushTableChanged(this, node != NULL ? ITable::CHANGE_DETAILS_SENT : 0, B_UNDEFINED_VALUE);
		if (node != NULL) {
			PushCellModified(this, EntryNameAtLocked(row), ColumnNameAtLocked(col), (BnNode*)node.ptr());
		}
	}
}

const void* BIndexedTableNode::StartReadingAtLocked(size_t row, size_t col, off_t position, ssize_t* inoutSize, uint32_t flags) const
{
	m_tmpValue = ValueAtLocked(row, col);

	const size_t len = m_tmpValue.Length();
	if (position > len) {
		*inoutSize = 0;
		return NULL;
	}

	const void* d = ((const char*)m_tmpValue.Data()) + (size_t)position;
	if (((size_t)position) + (*inoutSize) > len) {
		*inoutSize = len-((size_t)position);
	}

	return d;
}

void BIndexedTableNode::FinishReadingAtLocked(size_t row, size_t col, const void* data) const
{
	m_tmpValue.Undefine();
}

void* BIndexedTableNode::StartWritingAtLocked(size_t row, size_t col, off_t position, ssize_t* inoutSize, uint32_t flags)
{
	if ((position+*inoutSize) > MAX_SIZE_T) {
		*inoutSize = B_OUT_OF_RANGE;
		return NULL;
	}

	m_tmpValue = ValueAtLocked(row, col);

	const size_t wlen = (size_t)position + (size_t)*inoutSize;
	const size_t len = m_tmpValue.Length();
	m_writeLen = (len > wlen && (flags&B_WRITE_END) == 0) ? len : wlen;
	void* d = m_tmpValue.BeginEditBytes(m_tmpValue.Type(), m_writeLen, B_EDIT_VALUE_DATA);
	if (!d) {
		*inoutSize = m_tmpValue.IsSimple() ? B_NO_MEMORY : B_NOT_ALLOWED;
		return NULL;
	}

	return ((char*)d) + (size_t)position;
}

void BIndexedTableNode::FinishWritingAtLocked(size_t row, size_t col, void* data)
{
	m_tmpValue.EndEditBytes(m_writeLen);
	StoreValueAtLocked(row, col, m_tmpValue);
	m_tmpValue.Undefine();
}

sptr<BIndexedTableNode::RowNode> BIndexedTableNode::NewRowNodeLocked(size_t index, uint32_t mode)
{
	return new RowNode(Context(), this, index, mode);
}

// =================================================================================
// =================================================================================
// =================================================================================

BIndexedTableNode::RowNode::RowNode(const SContext& context, const sptr<BIndexedTableNode>& owner, size_t rowIndex, uint32_t mode)
	: BIndexedDataNode(context, mode)
	, m_owner(owner)
	, m_index(rowIndex)
{
}

BIndexedTableNode::RowNode::~RowNode()
{
	SAutolock _l(Lock());
	if (m_index >= 0) {
		DbgOnlyFatalErrorIf(m_owner->m_nodes == NULL, "BIndexedTableNode::RowNode: destroyed after m_nodes vector was deleted.");
		DbgOnlyFatalErrorIf(m_owner->m_nodes->ValueFor(m_index) != this, "BIndexedTableNode::RowNode: m_index is inconsistent with m_nodes.");
		m_owner->m_nodes->RemoveItemFor(m_index);
		if (m_owner->m_nodes->CountItems() == 0) {
			delete m_owner->m_nodes;
			m_owner->m_nodes = NULL;
		}
	}
}

lock_status_t BIndexedTableNode::RowNode::Lock() const
{
	return m_owner->Lock();
}

void BIndexedTableNode::RowNode::Unlock() const
{
	return m_owner->Unlock();
}

status_t BIndexedTableNode::RowNode::FinishAtom(const void* )
{
	return FINISH_ATOM_ASYNC;
}

bool BIndexedTableNode::RowNode::HoldRefForLink(const SValue& , uint32_t )
{
	return true;
}

SString BIndexedTableNode::RowNode::MimeTypeLocked() const
{
	return m_owner->EntryMimeTypeLocked();
}

status_t BIndexedTableNode::RowNode::StoreMimeTypeLocked(const SString& value)
{
	return m_owner->StoreEntryMimeTypeLocked(value);
}

size_t BIndexedTableNode::RowNode::CountEntriesLocked() const
{
	return m_owner->CountColumnsLocked();
}

ssize_t BIndexedTableNode::RowNode::EntryIndexOfLocked(const SString& entry) const
{
	return m_owner->ColumnIndexOfLocked(entry);
}

SString BIndexedTableNode::RowNode::EntryNameAtLocked(size_t index) const
{
	return m_owner->ColumnNameAtLocked(index);
}

uint32_t BIndexedTableNode::RowNode::ValueTypeAtLocked(size_t index) const
{
	return m_owner->ValueTypeAtLocked(m_index, index);
}

status_t BIndexedTableNode::RowNode::StoreValueTypeAtLocked(size_t index, uint32_t type)
{
	return m_owner->StoreValueTypeAtLocked(m_index, index, type);
}

off_t BIndexedTableNode::RowNode::SizeAtLocked(size_t index) const
{
	return m_owner->SizeAtLocked(m_index, index);
}

status_t BIndexedTableNode::RowNode::StoreSizeAtLocked(size_t index, off_t size)
{
	return m_owner->StoreSizeAtLocked(m_index, index, size);
}

SValue BIndexedTableNode::RowNode::ValueAtLocked(size_t index) const
{
	return m_owner->ValueAtLocked(m_index, index);
}

status_t BIndexedTableNode::RowNode::StoreValueAtLocked(size_t index, const SValue& value)
{
	return m_owner->StoreValueAtLocked(m_index, index, value);
}

void BIndexedTableNode::RowNode::ReportChangeAtLocked(size_t index, const sptr<IBinder>& editor, uint32_t changes, off_t start, off_t length)
{
	m_owner->ReportChangeAtLocked(m_index, index, editor, changes, start, length);
}

const void* BIndexedTableNode::RowNode::StartReadingAtLocked(	size_t index, off_t position, ssize_t* inoutSize, uint32_t flags) const
{
	return m_owner->StartReadingAtLocked(m_index, index, position, inoutSize, flags);
}

void BIndexedTableNode::RowNode::FinishReadingAtLocked(size_t index, const void* data) const
{
	m_owner->FinishReadingAtLocked(m_index, index, data);
}

void* BIndexedTableNode::RowNode::StartWritingAtLocked(size_t index, off_t position, ssize_t* inoutSize, uint32_t flags)
{
	return m_owner->StartWritingAtLocked(m_index, index, position, inoutSize, flags);
}

void BIndexedTableNode::RowNode::FinishWritingAtLocked(size_t index, void* data)
{
	m_owner->FinishWritingAtLocked(m_index, index, data);
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
