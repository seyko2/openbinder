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

#include <dmprovider/SchemaRowIDJoin.h>

#include <support/Autolock.h>
#include <support/Datum.h>
#include <support/Node.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace dmprovider {
#endif

// *********************************************************************************
// ** BSchemaRowIDJoin ************************************************************
// *********************************************************************************

BSchemaRowIDJoin::BSchemaRowIDJoin(const SContext& context,
	const sptr<BSchemaTableNode>& primary, const sptr<BSchemaTableNode>& join, const SString& joinIDCol, const SString& primaryColumnName)
	: BMetaDataNode(context)
	, BGenericIterable(context)
	, m_primaryTable(primary)
	, m_joinTable(join)
	, m_joinIDColumn(joinIDCol)
	, m_primaryColumnName(primaryColumnName)
	, m_joinIDIndex(-1)
	, m_rowMimeType(MimeTypeLocked())
	, m_primaryData(new BSchemaTableNode::DataAccessor(m_primaryTable))
	, m_joinData(new BSchemaTableNode::DataAccessor(m_joinTable))
{
	if (m_joinData != NULL) {
		SAutolock _l(m_joinTable->Lock());
		m_joinIDIndex = m_joinTable->ColumnIndexOfLocked(m_joinIDColumn);
	}
#if 0
	// Bind the data objects to the columns, so we will have consistent
	// column indices no matter what happens to the underlying tables.
	// XXX Should deal with new columns appearing.

	if (m_primaryData != NULL) {
		SAutolock _l(m_primaryData->Table()->Lock());
		m_primaryData->SetColumnsLocked(B_WILD_VALUE);
	}
	if (m_joinData != NULL) {
		SAutolock _l(m_joinData->Table()->Lock());
		m_joinData->SetColumnsLocked(B_WILD_VALUE);
	}
#endif
}

BSchemaRowIDJoin::~BSchemaRowIDJoin()
{
}

lock_status_t BSchemaRowIDJoin::Lock() const
{
	return BMetaDataNode::Lock();
}

void BSchemaRowIDJoin::Unlock() const
{
	BMetaDataNode::Unlock();
}

SValue BSchemaRowIDJoin::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	SValue result(BMetaDataNode::Inspect(caller, which, flags));
	result.Join(BGenericIterable::Inspect(caller, which, flags));
	return result;
}

status_t BSchemaRowIDJoin::StatusCheck() const
{
	return m_primaryData != NULL && m_joinData != NULL ? B_OK : B_NO_MEMORY;
}

void BSchemaRowIDJoin::SetMimeType(const SString& /*value*/)
{
}

SString BSchemaRowIDJoin::RowMimeTypeLocked() const
{
	return m_rowMimeType;
}

status_t BSchemaRowIDJoin::StoreRowMimeTypeLocked(const SString& mimeType)
{
	m_rowMimeType = mimeType;
	return B_OK;
}

static void split_join_name(SString* inNameOutPri, SString* outJoin)
{
	// XXX Should escape '+' in the name.

	const int32_t p = inNameOutPri->FindFirst('+');
	const int32_t len = inNameOutPri->Length();
	if (p < 0 || (p+1) >= len) return;
	inNameOutPri->CopyInto(*outJoin, p+1, inNameOutPri->Length()-(p+1));
	inNameOutPri->Truncate(p);
}

status_t BSchemaRowIDJoin::LookupEntry(const SString& entry, uint32_t flags, SValue* node)
{
	// Convert entry name to the corresponding entries in the two tables.

	SString primaryName(entry);
	SString joinName;
	split_join_name(&primaryName, &joinName);

	// Find the corresponding nodes in the two tables.

	sptr<BSchemaTableNode::RowNode> primaryNode;
	sptr<BSchemaTableNode::RowNode> joinNode;
	status_t err;

	err = m_primaryTable->LookupNode(primaryName, &primaryNode);
	if (err != B_OK) return err;
	err = m_joinTable->LookupNode(joinName, &joinNode);
	if (err != B_OK) return err;

	// Validate that this combination of nodes is actually a result of a join.

	m_joinTable->Lock();
	ssize_t col = m_joinTable->ColumnIndexOfLocked(m_joinIDColumn);
	SValue joinIDValue;
	if (col >= 0) joinIDValue = m_joinData->ColumnValueLocked(joinNode->m_rowID, col, INode::REQUEST_DATA);
	m_joinTable->Unlock();

	size_t joinID(joinIDValue.AsInt32());
	if (joinID != primaryNode->m_rowID) return B_ENTRY_NOT_FOUND;

	// Return the join node.

	Lock();
	sptr<RowNode> n = NodeForLocked(primaryNode, joinNode);
	Unlock();
	if (n != NULL) {
		*node = SValue::Binder((BnNode*)n.ptr());
		return B_OK;
	}

	return B_NO_MEMORY;
}

sptr<BSchemaRowIDJoin::RowNode> BSchemaRowIDJoin::NodeForLocked(const sptr<BSchemaTableNode::RowNode>& priNode, const sptr<BSchemaTableNode::RowNode>& joinNode)
{
	node_key key;
	key.primary = priNode;
	key.join = joinNode;

	sptr<RowNode> node(m_nodes.ValueFor(key).promote());
	if (node != NULL) return node;

	node = NewRowNodeLocked(priNode, joinNode);
	if (node != NULL) {
		m_nodes.AddItem(key, node);
	}

	return node;
}

sptr<BGenericIterable::GenericIterator> BSchemaRowIDJoin::NewGenericIterator(const SValue& args)
{
	return new JoinIterator(Context(), this);
}

sptr<BSchemaRowIDJoin::RowNode> BSchemaRowIDJoin::NewRowNodeLocked(const sptr<BSchemaTableNode::RowNode>& priNode, const sptr<BSchemaTableNode::RowNode>& joinNode)
{
	return new RowNode(Context(), this, priNode, joinNode);
}

// =================================================================================

BSchemaRowIDJoin::RowNode::RowNode(const SContext& context,
	const sptr<BSchemaRowIDJoin>& owner, const sptr<BSchemaTableNode::RowNode>& primary, const sptr<BSchemaTableNode::RowNode>& join)
	: BMetaDataNode(context)
	, BIndexedIterable(context)
	, m_owner(owner)
	, m_primary(primary)
	, m_join(join)
{
}

BSchemaRowIDJoin::RowNode::~RowNode()
{
}

status_t BSchemaRowIDJoin::RowNode::FinishAtom(const void* id)
{
	return FINISH_ATOM_ASYNC;
}

bool BSchemaRowIDJoin::RowNode::HoldRefForLink(const SValue& binding, uint32_t flags)
{
	return true;
}

lock_status_t BSchemaRowIDJoin::RowNode::Lock() const
{
	return m_owner->Lock();
}

void BSchemaRowIDJoin::RowNode::Unlock() const
{
	return m_owner->Unlock();
}

SValue BSchemaRowIDJoin::RowNode::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	SValue result(BMetaDataNode::Inspect(caller, which, flags));
	result.Join(BIndexedIterable::Inspect(caller, which, flags));
	return result;
}

status_t BSchemaRowIDJoin::RowNode::LookupEntry(const SString& entry, uint32_t flags, SValue* node)
{
	if (entry == m_owner->m_joinIDColumn) return B_ENTRY_NOT_FOUND;
	status_t err = m_join->LookupEntry(entry, flags, node);
	if (err != B_OK) {
		if (entry == m_owner->m_primaryColumnName) {
			*node = SValue::Binder((BnNode*)m_primary.ptr());
			err = B_OK;
		} else {
			err = m_primary->LookupEntry(entry, flags, node);
		}
	}
	return err;
}

status_t BSchemaRowIDJoin::RowNode::EntryAtLocked(const sptr<IndexedIterator>& it, size_t index, uint32_t flags, SValue* key, SValue* entry)
{
	m_join->Lock();
	size_t joinN = m_join->CountEntriesLocked();
	if (m_owner->m_joinIDIndex >= 0) joinN--;
	if (index < joinN) {
		if ((ssize_t)index >= m_owner->m_joinIDIndex) index++;
		status_t err = m_join->EntryAtLocked(NULL, index, flags, key, entry);
		m_join->Unlock();
		return err;
	}
	m_join->Unlock();

	index -= joinN;

	if (m_owner->m_primaryColumnName.Length() > 0) {
		if (index == 0) {
			*key = SValue::String(m_owner->m_primaryColumnName);
			*entry = SValue::Binder((BnNode*)m_primary.ptr());
			return B_OK;
		}
	} else {
		m_primary->Lock();
		size_t priN = m_primary->CountEntriesLocked();
		if (index < priN) {
			status_t err = m_primary->EntryAtLocked(NULL, index, flags, key, entry);
			m_primary->Unlock();
			return err;
		}
		m_primary->Unlock();
	}

	return B_BAD_INDEX;
}

SString BSchemaRowIDJoin::RowNode::MimeTypeLocked() const
{
	return m_owner->RowMimeTypeLocked();
}

status_t BSchemaRowIDJoin::RowNode::StoreMimeTypeLocked(const SString& value)
{
	return B_UNSUPPORTED;
}

size_t BSchemaRowIDJoin::RowNode::CountEntriesLocked() const
{
	m_join->Lock();
	size_t joinN = m_join->CountEntriesLocked();
	m_join->Unlock();

	if (m_owner->m_joinIDIndex >= 0) joinN--;

	size_t priN = 1;
	if (m_owner->m_primaryColumnName.Length() == 0) {
		m_primary->Lock();
		priN = m_primary->CountEntriesLocked();
		m_primary->Unlock();
	}

	return priN + joinN;
}

// =================================================================================

BSchemaRowIDJoin::JoinIterator::JoinIterator(const SContext& context, const sptr<BSchemaRowIDJoin>& owner)
	: GenericIterator(context, owner.ptr())
	, m_owner(owner)
{
}

BSchemaRowIDJoin::JoinIterator::~JoinIterator()
{
}

SValue BSchemaRowIDJoin::JoinIterator::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	return BnRandomIterator::Inspect(caller, which, flags);
}

status_t BSchemaRowIDJoin::JoinIterator::FinishAtom(const void* id)
{
	return FINISH_ATOM_ASYNC;
}

SValue BSchemaRowIDJoin::JoinIterator::Options() const
{
	return m_args;
}

status_t BSchemaRowIDJoin::JoinIterator::Remove()
{
	return B_UNSUPPORTED;
}

size_t BSchemaRowIDJoin::JoinIterator::Count() const
{
	return m_joinIterator != NULL ? m_joinIterator->Count() : 0;
}

size_t BSchemaRowIDJoin::JoinIterator::Position() const
{
	return m_joinIterator != NULL ? m_joinIterator->Position() : 0;
}

void BSchemaRowIDJoin::JoinIterator::SetPosition(size_t p)
{
	if (m_joinIterator != NULL) m_joinIterator->SetPosition(p);
}

status_t BSchemaRowIDJoin::JoinIterator::ParseArgs(const SValue& args)
{
	SAutolock _l(m_owner->Lock());

	//bout << "Initial iterator args: " << args << endl;

	// We always need to retrieve the join node ID column.
	// XXX This should be hidden from the client!!!
	m_args = args;
	SValue columns(m_args[BV_ITERABLE_SELECT]);
	if (columns.IsDefined() && !columns.IsWild()) {
		columns.Join(SValue::String(m_owner->m_joinIDColumn));
		m_args.Overlay(SValue(BV_ITERABLE_SELECT, columns));
	}

	status_t err;
	sptr<IIterator> genIt = m_owner->m_joinTable->NewIterator(m_args, &err);
	if (err != B_OK) return err;
	m_joinIterator = static_cast<BSchemaTableNode::QueryIterator*>(genIt.ptr());

	m_primaryData = new BSchemaTableNode::DataAccessor(m_owner->m_primaryTable);
	if (m_primaryData == NULL) return B_NO_MEMORY;

	columns = args[BV_ITERABLE_SELECT];
	m_includePrimaryNode = false;

	if (m_owner->m_primaryColumnName.Length() > 0) {
		// We are supplying the primary column data as a single entry
		// under which you can find the real node.
		if (columns.IsWild() || columns.HasItem(B_WILD_VALUE, SValue::String(m_owner->m_primaryColumnName))) {
			m_includePrimaryNode = true;
			columns = SValue::String(m_owner->m_primaryColumnName);
		} else {
			columns.Undefine();
		}
	} else {
		// We are merging the primary column data in to this row.
		SAutolock _l(m_primaryData->Table()->Lock());

		SValue priColumns;
		err = m_primaryData->SetColumnsLocked(columns, &priColumns);
		if (err != B_OK) return err;

		if (priColumns.IsWild()) {
			columns.Undefine();
			const size_t N(m_primaryData->CountColumnsLocked());
			for (size_t i=0; i<N; i++) {
				columns.Join(SValue::String(m_primaryData->ColumnInfoLocked(i).name));
			}
		} else {
			columns = priColumns;
		}
	}

	m_args = m_joinIterator->Options();
	m_args.Join(SValue(BV_ITERABLE_SELECT, columns));

	//bout << "Final iterator args: " << m_args << endl;

	return B_OK;
}

status_t BSchemaRowIDJoin::JoinIterator::NextLocked(uint32_t flags, SValue* key, SValue* entry)
{
	SValue joinKey;

	// First retrieve columns from the join row.
	m_joinIterator->Owner()->Lock();
	status_t err = m_joinIterator->NextLocked(flags, &joinKey, entry);
	m_joinIterator->Owner()->Unlock();

	if (err != B_OK) return err;

	if (entry->IsObject()) flags &= ~INode::COLLAPSE_CATALOG;

	// Find the corresponding row id in the primary table.
	SNode node(*entry);
	SDatum rowIDDatum(node.Walk(m_owner->m_joinIDColumn));
	uint32_t rowID(rowIDDatum.FetchValue(4).AsInt32(&err));
	if (err != B_OK) return err;

	{
		SAutolock _l(m_primaryData->Table()->Lock());

		SString name(m_primaryData->Table()->EntryNameForLocked(rowID));
		name += "+";
		name += joinKey.AsString();
		*key = SValue::String(name);

		if ((flags & (INode::COLLAPSE_CATALOG|INode::REQUEST_DATA)) == (INode::COLLAPSE_CATALOG|INode::REQUEST_DATA)) {
			if (m_owner->m_primaryColumnName.Length() > 0) {
				if (m_includePrimaryNode) {
					if (m_primaryData->Table()->RowIDIsValidLocked(rowID)) {
						sptr<BSchemaTableNode::RowNode> priObj = m_primaryData->Table()->NodeForLocked(rowID);
						if (priObj != NULL) {
							entry->JoinItem(SValue::String(m_owner->m_primaryColumnName), SValue::Binder((BnNode*)priObj.ptr()));
						} else {
							err = B_NO_MEMORY;
						}
					}
				}
			} else {
				SValue priValue;
				err = m_primaryData->LoadDataLocked(rowID, flags, &priValue);
				if (err != B_OK) {
					entry->Join(priValue);
				}
			}
		} else {
			sptr<INode> joinNode(interface_cast<INode>(*entry));
			sptr<BSchemaTableNode::RowNode> priObj = m_primaryData->Table()->NodeForLocked(rowID);
			sptr<BSchemaTableNode::RowNode> joinObj = static_cast<BSchemaTableNode::RowNode*>(joinNode.ptr());
			sptr<RowNode> node;
			if (joinObj != NULL && priObj != NULL) {
				node = m_owner->NodeForLocked(priObj, joinObj);
				*entry = SValue::Binder((BnNode*)node.ptr());
			}
			if (node == NULL) err = B_NO_MEMORY;
		}

	}

	return err;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
