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

#include <storage/IndexedDataNode.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

// *********************************************************************************
// ** BIndexedDataNode *************************************************************
// *********************************************************************************

BIndexedDataNode::BIndexedDataNode(uint32_t mode)
	: SDatumGeneratorInt(mode)
{
}

BIndexedDataNode::BIndexedDataNode(const SContext& context, uint32_t mode)
	: BMetaDataNode(context)
	, BIndexedIterable(context)
	, SDatumGeneratorInt(mode)
{
}

BIndexedDataNode::~BIndexedDataNode()
{
}

lock_status_t BIndexedDataNode::Lock() const
{
	return BMetaDataNode::Lock();
}

void BIndexedDataNode::Unlock() const
{
	BMetaDataNode::Unlock();
}

SValue BIndexedDataNode::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	SValue res(BMetaDataNode::Inspect(caller, which, flags));
	res.Join(BIndexedIterable::Inspect(caller, which, flags));
	return res;
}

status_t BIndexedDataNode::LookupEntry(const SString& entry, uint32_t flags, SValue* node)
{
	SAutolock _l(Lock());
	ssize_t index = EntryIndexOfLocked(entry);
	if (index < 0) return index;

	return FetchEntryAtLocked(index, flags, node);
}

status_t BIndexedDataNode::EntryAtLocked(const sptr<IndexedIterator>& it, size_t index, uint32_t flags, SValue* key, SValue* entry)
{
	*key = SValue::String(EntryNameAtLocked(index));
	return FetchEntryAtLocked(index, flags, entry);
}

void BIndexedDataNode::ReportChangeAtLocked(size_t index, const sptr<IBinder>& editor, uint32_t changes, off_t start, off_t length)
{
	TouchLocked();

	if (BnNode::IsLinked()) {
		PushNodeChanged(this, CHANGE_DETAILS_SENT, B_UNDEFINED_VALUE);
		sptr<IBinder> entry;
		SValue v(ValueAtLocked(index));
		if (v.IsObject()) entry = v.AsBinder();
		else {
			// ADS!
			sptr<IDatum> datum = DatumAtLocked(index);
			entry = datum->AsBinder();
		}
		PushEntryModified(this, EntryNameAtLocked(index), entry);
	}

	PushIteratorChangedLocked();

	SDatumGeneratorInt::ReportChangeAtLocked(index, editor, changes, start, length);
}

status_t BIndexedDataNode::FetchEntryAtLocked(size_t index, uint32_t flags, SValue* entry)
{
	bool isObject = false;
	if (!AllowDataAtLocked(index)) flags &= ~INode::REQUEST_DATA;
	else {
		*entry = ValueAtLocked(index);
		isObject = entry->IsObject();
	}

	if ((flags & INode::REQUEST_DATA) == 0 && !isObject)
	{
		sptr<IDatum> datum = DatumAtLocked(index);
		if (datum == NULL) return B_NO_MEMORY;
		*entry = SValue::Binder(datum->AsBinder());
	}

	return B_OK;
}

bool BIndexedDataNode::AllowDataAtLocked(size_t index) const
{
	return SizeAtLocked(index) <= 2048;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
