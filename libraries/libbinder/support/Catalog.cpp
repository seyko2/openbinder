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

#include <support/Catalog.h>

#include <support/Autolock.h>
#include <support/MemoryStore.h>
#include <support/StdIO.h>
#include <support/Looper.h>

#include <support/IIterator.h>

#include <support_p/WindowsCompatibility.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// =================================================================================
status_t SWalkHelper::HelperWalk(SString* path, uint32_t flags, SValue* node)
{
	status_t err;

	{
		// Look up the next path element.  We are doing this
		// inside a block so that we don't have to hold a
		// reference on the temporary name here while recursing
		// into the next catalog.  (Actually, it would be nice
		// to completely remove this temporary...)
		SString name;
		path->PathRemoveRoot(&name);

		// check to see if this this part of the path begins with a ":"
		if (name.ByteAt(0) == ':')
		{
			const sptr<INode> attrs = HelperAttributesCatalog();
			err = B_ENTRY_NOT_FOUND;
			if (attrs != NULL)
			{
				if (name.Length() == 1)
				{
					// it was just the ":"
					*node = attrs->AsBinder();
					err = B_OK;
				}
				else
				{
					name.Remove(0, 1);
					// we need to walk the attributes catalog
					err = attrs->Walk(&name, flags, node);
				}
			}
		}
		else
		{
			err = HelperLookupEntry(name, flags, node);
		
			if (err == B_ENTRY_NOT_FOUND)
			{
				// Before we lose the name...  if this entry didn't
				// exist, we need to create it.
				// XXX This code should be moved to another function
				// to reduce instruction cache line usage here.

				if ((flags & INode::CREATE_MASK))
				{
					if ((flags & INode::CREATE_DATUM) && path->Length() == 0)
					{
						sptr<IDatum> datum = HelperCreateDatum(&name, flags, &err);
						
						// someone could have created the entry before we could
						// so just return that entry.
						if (err == B_ENTRY_EXISTS)
						{
							err = HelperLookupEntry(name, flags, node);
						}
						else if (err == B_OK)
						{
							*node = SValue::Binder(datum->AsBinder());
						}
					}
					else
					{
						sptr<ICatalog> catalog = HelperCreateCatalog(&name, &err);

						if (err == B_ENTRY_EXISTS)
						{
							err = HelperLookupEntry(name, flags, node);
						}
						else if (err == B_OK)
						{
							*node = SValue::Binder(catalog->AsBinder());
						}
					}
				}
				else
				{
					err = B_ENTRY_NOT_FOUND;
				}
			}
		}
	}

	if (path->Length() > 0)
	{
		sptr<INode> inode = interface_cast<INode>(*node);
		if (inode == NULL)
		{
			// We haven't completed traversing the path,
			// but we have reached the end of the namespace.
			// That would be an error.
			err = B_ENTRY_NOT_FOUND;
		}
		else
		{
			// only walk to the catalog if the path is not empty
			// and if the next catalog to walk is not remote.
			// if it is remote return the catalog since we should not
			// cross process boundries.

			sptr<IBinder> binder = inode->AsBinder();

			// none shall pass...the process boundary. just return
			// what we found from lookup if the binder is remote.
			if (binder->RemoteBinder() == NULL)
			{
				return inode->Walk(path, flags, node);
			}
		}
	}
	
	return err;
}

sptr<INode> SWalkHelper::HelperAttributesCatalog() const
{
	return NULL;
}

sptr<ICatalog> SWalkHelper::HelperCreateCatalog(SString* name, status_t* err)
{
	*err = B_UNSUPPORTED;
	return NULL;
}

sptr<IDatum> SWalkHelper::HelperCreateDatum(SString* name, uint32_t flags, status_t* err)
{
	*err = B_UNSUPPORTED;
	return NULL;
}

// =================================================================================

BMetaDataCatalog::BMetaDataCatalog(const SContext& context)
	: BIndexedCatalog(context)
	, m_attrs(NULL)
{
}

BMetaDataCatalog::~BMetaDataCatalog()
{
	delete m_attrs;
}

status_t BMetaDataCatalog::LookupMetaEntry(const SString& entry, uint32_t flags, SValue* node)
{
	if (BMetaDataNode::LookupMetaEntry(entry, flags, node) == B_OK) return B_OK;

	SAutolock _l(Lock());
	if (m_attrs) {
		// XXX Need to generate datums.
		bool found;
		*node = m_attrs->ValueFor(entry, &found);
		if (found) return B_OK;
	}

	return B_ENTRY_NOT_FOUND;
}

status_t BMetaDataCatalog::CreateMetaEntry(const SString& name, const SValue& initialValue, sptr<IDatum>* outDatum)
{
	status_t err = BMetaDataNode::CreateMetaEntry(name, initialValue, outDatum);
	if (err != B_UNSUPPORTED) return err;

	SAutolock _l(Lock());
	if (!m_attrs) {
		m_attrs = new SKeyedVector<SString, SValue>();
		if (!m_attrs) return B_NO_MEMORY;
	}
	// XXX Need to generate datums.
	return m_attrs->AddItem(name, initialValue);
}

status_t BMetaDataCatalog::RemoveMetaEntry(const SString& name)
{
	status_t err = BMetaDataNode::RemoveMetaEntry(name);
	if (err != B_UNSUPPORTED) return err;

	SAutolock _l(Lock());
	if (m_attrs) {
		if (m_attrs->RemoveItemFor(name) >= B_OK) return B_OK;
	}
	return B_ENTRY_NOT_FOUND;
}

status_t BMetaDataCatalog::RenameMetaEntry(const SString& old_name, const SString& new_name)
{
	status_t err = BMetaDataNode::RenameMetaEntry(old_name, new_name);
	if (err != B_UNSUPPORTED) return err;

	SAutolock _l(Lock());
	bool found;
	SValue val(m_attrs->ValueFor(old_name, &found));
	if (found) {
		if (m_attrs->IndexOf(new_name) >= 0) return B_ENTRY_EXISTS;
		ssize_t err = m_attrs->AddItem(new_name, val);
		if (err >= B_OK) {
			m_attrs->RemoveItemFor(old_name);
			return B_OK;
		}
		return err;
	}
	return B_ENTRY_NOT_FOUND;
}

status_t BMetaDataCatalog::MetaEntryAtLocked(ssize_t index, uint32_t flags, SValue* key, SValue* entry)
{
	if (index < 0) return BMetaDataNode::MetaEntryAtLocked(index, flags, key, entry);

	if (m_attrs == NULL || index >= (ssize_t)m_attrs->CountItems()) return B_END_OF_DATA;
	// XXX Need to generate datums.
	*key = SValue::String(m_attrs->KeyAt(index));
	*entry = m_attrs->ValueAt(index);
	return B_OK;
}

size_t BMetaDataCatalog::CountMetaEntriesLocked() const
{
	return m_attrs ? m_attrs->CountItems() : 0;
}

// =================================================================================

BCatalog::BCatalog()
	:	BMetaDataCatalog(SContext())
{
}

BCatalog::BCatalog(const SContext& context)
	:	BMetaDataCatalog(context)
	,	SDatumGeneratorInt(context)
{
}

BCatalog::~BCatalog()
{
}

void BCatalog::InitAtom()
{
}

lock_status_t BCatalog::Lock() const
{
	return BMetaDataCatalog::Lock();
}

void BCatalog::Unlock() const
{
	BMetaDataCatalog::Unlock();
}

ssize_t BCatalog::AddEntryLocked(const SString& name, const SValue& entry, sptr<IBinder>* outEntry, bool* replaced)
{
	ssize_t index = m_entries.IndexOf(name);
	SValue* val;
	if (index >= 0) {
		SetValueAtLocked(index, entry);		// use this so we send out change notifications.
		val = &m_entries.EditValueAt(index);
		if (replaced) *replaced = true;
	} else {
		index = m_entries.AddItem(name, entry);
		if (index < 0) return index;
		UpdateIteratorIndicesLocked(index, 1);
		UpdateDatumIndicesLocked(index, 1);
		val = &m_entries.EditValueAt(index);
		if (replaced) *replaced = false;
	}

	if (outEntry) {
		*outEntry = val->AsBinder();
		if (*outEntry == NULL) {
			// This entry is not an object; we need to generate a datum to wrap it.
			*outEntry = DatumAtLocked(index)->AsBinder();
		}
	}

	return index;
}

status_t BCatalog::AddEntry(const SString& name, const SValue& entry)
{
	sptr<IBinder> binder;

	Lock();
	bool found;
	ssize_t index = AddEntryLocked(name, entry, &binder, &found);
	if (index >= 0) TouchLocked();
	Unlock();

	//! @todo Fix this by hooking in to ReportChangeAtLocked()!!!

	status_t err = B_NO_MEMORY;
	if (index >= 0)
	{
		// send the modified or creation event
		if (found)
		{
			OnEntryModified(name, binder);
			if (BnNode::IsLinked()) {
				PushNodeChanged(this, INode::CHANGE_DETAILS_SENT, B_UNDEFINED_VALUE);
				PushEntryModified(this, name, binder);
			}
		}
		else
		{
			OnEntryCreated(name, binder);
			if (BnNode::IsLinked()) {
				PushNodeChanged(this, INode::CHANGE_DETAILS_SENT, B_UNDEFINED_VALUE);
				PushEntryCreated(this, name, binder);
			}
		}
		
		err = B_OK;
	}
	
	return err;
}

status_t BCatalog::RemoveEntry(const SString& name)
{
	Lock();
	ssize_t index = m_entries.RemoveItemFor(name);
	if (index >= 0) {
		UpdateIteratorIndicesLocked(index, -1);
		UpdateDatumIndicesLocked(index, -1);
		TouchLocked();
	}
	Unlock();

	status_t err = B_ENTRY_NOT_FOUND;
	if (index >= 0)
	{
		OnEntryRemoved(name);
		if (BnNode::IsLinked()) {
			PushNodeChanged(this, INode::CHANGE_DETAILS_SENT, B_UNDEFINED_VALUE);
			PushEntryRemoved(this, name);
		}
		err = B_OK;
	}
	
	return err;
}

status_t BCatalog::RenameEntry(const SString& old_name, const SString& new_name)
{
	status_t err;
	sptr<IBinder> binder;
		
	{
		SAutolock _l(Lock());
		
		bool old = has_entry_l(old_name);
		bool nuu = has_entry_l(new_name);
		
		// can't rename a entry that doesn't exist or rename
		// an entry that exists to another entry that exists.
		if (!old) return B_ENTRY_NOT_FOUND;
		if (nuu) return B_ENTRY_EXISTS;
		
		//! @todo Need to keep the same IDatum object.

		ssize_t pos;
		const SValue val = m_entries.ValueFor(old_name);
		pos = m_entries.RemoveItemFor(old_name);
		if (pos >= 0) {
			UpdateIteratorIndicesLocked(pos, -1);
			UpdateDatumIndicesLocked(pos, -1);
			pos = m_entries.AddItem(new_name, val);
			if (pos >= 0) {
				UpdateIteratorIndicesLocked(pos, 1);
				UpdateDatumIndicesLocked(pos, 1);
				binder = val.AsBinder();
				if (binder == NULL) binder = DatumAtLocked(pos)->AsBinder();
				TouchLocked();
			}
		}
		
		err = (pos >= 0) ? B_OK : B_NO_MEMORY;
	}
	
	//! @todo Fix to only get the entry object if linked!!!

	// send the renamed event;
	if (err == B_OK)
	{
		OnEntryRenamed(old_name, new_name, binder);
		if (BnNode::IsLinked()) {
			PushNodeChanged(this, INode::CHANGE_DETAILS_SENT, B_UNDEFINED_VALUE);
			PushEntryRenamed(this, old_name, new_name, binder);
		}
	}
	
	return err;
}

sptr<INode> BCatalog::CreateNode(SString* name, status_t* err)
{
	sptr<INode> catalog;
	
	{
		SAutolock _l(Lock());

		if (has_entry_l(*name))
		{
			*err = B_ENTRY_EXISTS;
			catalog = NULL;
		}
		else
		{
			*err = B_OK;
			catalog = InstantiateNodeLocked(*name, err);
			if (catalog == NULL && *err == B_OK) *err = B_NO_MEMORY;
			if (*err == B_OK)
			{
				sptr<IBinder> binder;
				bool found;
				AddEntryLocked(*name, SValue::Binder(catalog->AsBinder()), &binder, &found);
				TouchLocked();
			}
		}
	}

	if (catalog != NULL)
	{
		// send the creation event
		OnEntryCreated(*name, catalog->AsBinder());
		if (BnNode::IsLinked()) {
			PushNodeChanged(this, INode::CHANGE_DETAILS_SENT, B_UNDEFINED_VALUE);
			PushEntryCreated(this, *name, catalog->AsBinder());
		}
	}

	return catalog;
}

sptr<INode> BCatalog::InstantiateNodeLocked(const SString &, status_t *err)
{
	return new BCatalog(Context());
}

sptr<IDatum> BCatalog::CreateDatum(SString* name, uint32_t flags, status_t* err)
{
	sptr<IBinder> binder;
	sptr<IDatum> datum;

	{
		SAutolock _l(Lock());
		if (has_entry_l(*name))
		{
			*err = B_ENTRY_EXISTS;
		}
		else
		{
			bool found;
			ssize_t pos = AddEntryLocked(*name, SValue(B_RAW_TYPE, "", 0), &binder, &found);
			if (pos >= 0) pos = binder != NULL ? B_OK : B_NO_MEMORY;
			*err = pos;
			datum = interface_cast<IDatum>(binder);
			TouchLocked();
		}
	}
	
	if (datum != NULL)
	{
		// send the entry created event
		OnEntryCreated(*name, datum->AsBinder());
		if (BnNode::IsLinked()) {
			PushNodeChanged(this, INode::CHANGE_DETAILS_SENT, B_UNDEFINED_VALUE);
			PushEntryCreated(this, *name, datum->AsBinder());
		}
	}

	return datum;
}

void BCatalog::OnEntryCreated(const SString &, const sptr<IBinder> &)
{
}

void BCatalog::OnEntryModified(const SString &, const sptr<IBinder> &)
{
}

void BCatalog::OnEntryRenamed(const SString &, const SString &, const sptr<IBinder> &)
{
}

void BCatalog::OnEntryRemoved(const SString &)
{
}


bool BCatalog::has_entry_l(const SString& name)
{
	if (m_entries.IndexOf(name) >= 0)
		return true;
	
	return false;
}

status_t BCatalog::EntryAtLocked(size_t index, uint32_t flags, SValue* key, SValue* entry)
{
	if (index >= m_entries.CountItems()) return B_END_OF_DATA;
	
//#if BUILD_TYPE == BUILD_TYPE_DEBUG
//	printf("EntryAt() flags = %p\n", flags);
//#endif
	
	*key = SValue::String(m_entries.KeyAt(index));
	*entry = m_entries.ValueAt(index); 

	if ((flags & INode::REQUEST_DATA) == 0 && !entry->IsObject())
	{
		*entry = SValue::Binder(DatumAtLocked(index)->AsBinder());
	}

	return B_OK;	
}

size_t BCatalog::CountEntriesLocked() const
{
	return m_entries.CountItems();
}

status_t BCatalog::LookupEntry(const SString& entry, uint32_t flags, SValue* node)
{
	SAutolock _l(Lock());

	ssize_t index = m_entries.IndexOf(entry);
	if (index < 0) return B_ENTRY_NOT_FOUND;

	*node = m_entries.ValueAt(index);
	if ((flags & INode::REQUEST_DATA) == 0 && !node->IsObject())
	{
		*node = SValue::Binder(DatumAtLocked(index)->AsBinder());
	}

	return B_OK;
}

SValue BCatalog::ValueAtLocked(size_t index) const
{
	return m_entries.ValueAt(index);
}

status_t BCatalog::StoreValueAtLocked(size_t index, const SValue& value)
{
	m_entries.EditValueAt(index) = value;
	return B_OK;
}

// ==================================================================================

BGenericCatalog::BGenericCatalog()
{
}

BGenericCatalog::BGenericCatalog(const SContext& context)
	:	BnCatalog(context),
		BMetaDataNode(context),
		BnIterable(context)
{
}

BGenericCatalog::~BGenericCatalog()
{
}

SValue BGenericCatalog::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	return SValue(BnCatalog::Inspect(caller, which, flags))
			.Join(BnNode::Inspect(caller, which, flags))
			.Join(BnIterable::Inspect(caller, which, flags));
}

sptr<INode> BGenericCatalog::CreateNode(SString* name, status_t* err)
{
	return BMetaDataNode::CreateNode(name, err);
}

sptr<IDatum> BGenericCatalog::CreateDatum(SString* name, uint32_t flags, status_t* err)
{
	return BMetaDataNode::CreateDatum(name, flags, err);
}

// ==================================================================================

BIndexedCatalog::BIndexedCatalog()
{
}

BIndexedCatalog::BIndexedCatalog(const SContext& context)
	: BnCatalog(context)
	, BMetaDataNode(context)
	, BIndexedIterable(context)
{
}

BIndexedCatalog::~BIndexedCatalog()
{
}

SValue BIndexedCatalog::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	return SValue(BnCatalog::Inspect(caller, which, flags))
			.Join(BnNode::Inspect(caller, which, flags))
			.Join(BIndexedIterable::Inspect(caller, which, flags));
}

lock_status_t BIndexedCatalog::Lock() const
{
	return BMetaDataNode::Lock();
}

void BIndexedCatalog::Unlock() const
{
	BMetaDataNode::Unlock();
}

status_t BIndexedCatalog::EntryAtLocked(const sptr<IndexedIterator>& , size_t index,
	uint32_t flags, SValue* key, SValue* entry)
{
	return EntryAtLocked(index, flags, key, entry);
}

sptr<INode> BIndexedCatalog::CreateNode(SString* name, status_t* err)
{
	return BMetaDataNode::CreateNode(name, err);
}

sptr<IDatum> BIndexedCatalog::CreateDatum(SString* name, uint32_t flags, status_t* err)
{
	return BMetaDataNode::CreateDatum(name, flags, err);
}

void BIndexedCatalog::EntryAddedAt(uint32_t index)
{
	SAutolock _l(Lock());
	UpdateIteratorIndicesLocked(index, 1);
}

void BIndexedCatalog::EntryRemovedAt(uint32_t index)
{
	SAutolock _l(Lock());
	UpdateIteratorIndicesLocked(index, -1);
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
