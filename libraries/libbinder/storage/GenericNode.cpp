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

#include <storage/GenericNode.h>

#include <storage/ValueDatum.h>

#include <support/Autolock.h>
#include <support/Catalog.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

// *********************************************************************************
// ** LOCAL DEFINITIONS ************************************************************
// *********************************************************************************

B_STATIC_STRING(kCatalogMimeType, "application/vnd.palm.catalog", );
B_STATIC_STRING(kMimeType, "mimeType", );
B_STATIC_STRING(kCreationDate, "creationDate", );
B_STATIC_STRING(kModifiedDate, "modifiedDate", );

enum {
	kMimeTypeIndex = 0,
	kCreationDateIndex,
	kModifiedDateIndex,
	kNumMetaData
};

// *********************************************************************************
// ** BGenericNode *****************************************************************
// *********************************************************************************

class BGenericNode::node_state : public BIndexedCatalog
{
public:
	node_state(const sptr<BGenericNode>& data);

	virtual	lock_status_t Lock() const;
	virtual	void Unlock() const;

	virtual	status_t AddEntry(const SString& name, const SValue& entry);
	virtual	status_t RemoveEntry(const SString& name);
	virtual	status_t RenameEntry(const SString& entry, const SString& name);
	
	virtual	sptr<INode> CreateNode(SString* name, status_t* err);
	virtual	sptr<IDatum> CreateDatum(SString* name, uint32_t flags, status_t* err);
	
	//! Return the entry at the given index.
	virtual status_t EntryAtLocked(size_t index, uint32_t flags, SValue* key, SValue* entry);
	//! Return the number of entries in this catalog
	virtual size_t	CountEntriesLocked() const;
	//! Lookup an entry in this catalog.
	virtual status_t LookupEntry(const SString& entry, uint32_t flags, SValue* node);

	sptr<BValueDatum> GetDatumLocked(size_t index, bool create=false);
	void RemoveDatumLocked(size_t index, BValueDatum* datum);

protected:
	virtual ~node_state();

private:
	const sptr<BGenericNode>	m_data;
	wptr<BValueDatum>			m_datums[kNumMetaData];
};

// =================================================================================
// =================================================================================
// =================================================================================

class BGenericNode::meta_datum : public BValueDatum
{
public:
									meta_datum(const sptr<BGenericNode>& data, size_t index, const SValue& val, uint32_t mode = IDatum::READ_WRITE);

	virtual	void					ReportChangeLocked(const sptr<IBinder>& editor, uint32_t changes, off_t start=-1, off_t length=-1);

	virtual	lock_status_t			Lock() const;
	virtual	void					Unlock() const;

	virtual	status_t				StoreValueTypeLocked(uint32_t type);
	virtual	status_t				StoreSizeLocked(off_t size);
	virtual	status_t				StoreValueLocked(const SValue& value);

protected:
	virtual							~meta_datum();

private:
	const sptr<BGenericNode>		m_data;
	const size_t					m_index;
};

// =================================================================================
// =================================================================================
// =================================================================================

BGenericNode::BGenericNode()
	: m_lock("BGenericNode::m_lock")
{
}

BGenericNode::BGenericNode(const SContext& context)
	: BnNode(context)
	, m_lock("BGenericNode::m_lock")
{
}

BGenericNode::~BGenericNode()
{
}

sptr<INode> BGenericNode::Attributes() const
{
	SAutolock _l(Lock());
	const sptr<node_state> state = get_state_l();
	return state.ptr();
}

SString BGenericNode::MimeType() const
{
	SAutolock _l(Lock());
	return MimeTypeLocked();
}

void BGenericNode::SetMimeType(const SString& value)
{
	SAutolock _l(Lock());
	SetMimeTypeLocked(value);
}

nsecs_t BGenericNode::CreationDate() const
{
	SAutolock _l(Lock());
	return CreationDateLocked();
}

void BGenericNode::SetCreationDate(nsecs_t value)
{
	SAutolock _l(Lock());
	SetCreationDateLocked(value);
}

nsecs_t BGenericNode::ModifiedDate() const
{
	SAutolock _l(Lock());
	return ModifiedDateLocked();
}

void BGenericNode::SetModifiedDate(nsecs_t value)
{
	SAutolock _l(Lock());
	SetModifiedDateLocked(value);
}

status_t BGenericNode::Walk(SString* path, uint32_t flags, SValue* node)
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
		const bool atLeaf = path->Length() == 0;

		// check to see if this this part of the path begins with a ":"
		if (name.ByteAt(0) == ':')
		{
			// XXX OPTIMIZE: Don't go through Attributes() node
			// for the simple ":blah" construct.
			const sptr<INode> attrs = Attributes();
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
			err = LookupEntry(name, atLeaf ? flags : (flags&~(REQUEST_DATA|COLLAPSE_NODE)), node);
		
			if (err == B_ENTRY_NOT_FOUND)
			{
				// Before we lose the name...  if this entry didn't
				// exist, we need to create it.
				// XXX This code should be moved to another function
				// to reduce instruction cache line usage here.

				if ((flags & INode::CREATE_MASK))
				{
					if ((flags & INode::CREATE_DATUM) && atLeaf)
					{
						sptr<IDatum> newdatum = CreateDatum(&name, flags, &err);
						
						// someone could have created the entry before we could
						// so just return that entry.
						if (err == B_ENTRY_EXISTS)
						{
							err = LookupEntry(name, flags, node);
						}
						else if (err == B_OK)
						{
							*node = SValue::Binder(newdatum->AsBinder());
						}
					}
					else
					{
						sptr<INode> newnode = CreateNode(&name, &err);

						if (err == B_ENTRY_EXISTS)
						{
							err = LookupEntry(name, flags&~(REQUEST_DATA|COLLAPSE_NODE), node);
						}
						else if (err == B_OK)
						{
							*node = SValue::Binder(newnode->AsBinder());
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

status_t BGenericNode::SetMimeTypeLocked(const SString& value)
{
	if (value != MimeTypeLocked()) PushMimeType(value);
	const status_t err = StoreMimeTypeLocked(value);
	sptr<node_state> state = m_state.promote();
	if (state == NULL) return err;
	sptr<BValueDatum> datum = state->GetDatumLocked(kMimeTypeIndex);
	datum->StoreValueLocked(SValue::String(value));
	return err;
}

status_t BGenericNode::SetCreationDateLocked(nsecs_t value)
{
	if (value != CreationDateLocked()) PushCreationDate(value);
	const status_t err = StoreCreationDateLocked(value);
	sptr<node_state> state = m_state.promote();
	if (state == NULL) return err;
	sptr<BValueDatum> datum = state->GetDatumLocked(kCreationDateIndex);
	if (datum == NULL) return err;
	datum->StoreValueLocked(SValue::Time(value));
	return err;
}

status_t BGenericNode::SetModifiedDateLocked(nsecs_t value)
{
	if (value != ModifiedDateLocked()) PushModifiedDate(value);
	const status_t err = StoreModifiedDateLocked(value);
	sptr<node_state> state = m_state.promote();
	if (state == NULL) return err;
	sptr<BValueDatum> datum = state->GetDatumLocked(kModifiedDateIndex);
	if (datum == NULL) return err;
	datum->StoreValueLocked(SValue::Time(value));
	return err;
}

sptr<INode> BGenericNode::CreateNode(SString* /*name*/, status_t* err)
{
	*err = B_UNSUPPORTED;
	return NULL;
}

sptr<IDatum> BGenericNode::CreateDatum(SString* /*name*/, uint32_t /*flags*/, status_t* err)
{
	*err = B_UNSUPPORTED;
	return NULL;
}

SString BGenericNode::MimeTypeLocked() const
{
	return SString(kCatalogMimeType);
}

status_t BGenericNode::StoreMimeTypeLocked(const SString& /*value*/)
{
	return B_UNSUPPORTED;
}

nsecs_t BGenericNode::CreationDateLocked() const
{
	return 0;
}

status_t BGenericNode::StoreCreationDateLocked(nsecs_t /*value*/)
{
	return B_UNSUPPORTED;
}

nsecs_t BGenericNode::ModifiedDateLocked() const
{
	return 0;
}

status_t BGenericNode::StoreModifiedDateLocked(nsecs_t /*value*/)
{
	return B_UNSUPPORTED;
}

static ssize_t meta_name_to_index(const SString& name)
{
	if (name == kMimeType) {
		return kMimeTypeIndex;
	}
	if (name == kCreationDate) {
		return kCreationDateIndex;
	}
	if (name == kModifiedDate) {
		return kModifiedDateIndex;
	}
	return B_ERROR;
}

status_t BGenericNode::LookupMetaEntry(const SString& entry, uint32_t flags, SValue* node)
{
	ssize_t i = meta_name_to_index(entry);
	if (i >= 0) {
		SAutolock _l(Lock());
		SValue dummy;
		return MetaEntryAtLocked(-1-i, flags, &dummy, node);
	}
	return B_ENTRY_NOT_FOUND;
}

status_t BGenericNode::CreateMetaEntry(const SString& name, const SValue& initialValue, sptr<IDatum>* outDatum)
{
	ssize_t i = meta_name_to_index(name);
	return i >= 0 ? B_ENTRY_EXISTS : B_UNSUPPORTED;
}

status_t BGenericNode::RemoveMetaEntry(const SString& name)
{
	ssize_t i = meta_name_to_index(name);
	return i >= 0 ? B_NOT_ALLOWED : B_UNSUPPORTED;
}

status_t BGenericNode::RenameMetaEntry(const SString& old_name, const SString& new_name)
{
	ssize_t i = meta_name_to_index(old_name);
	if (i >= 0) return B_NOT_ALLOWED;
	i = meta_name_to_index(new_name);
	return i >= 0 ? B_ENTRY_EXISTS : B_UNSUPPORTED;
}

status_t BGenericNode::MetaEntryAtLocked(ssize_t index, uint32_t flags, SValue* key, SValue* entry)
{
	index = -1-index;

	if (flags&REQUEST_DATA) {
		return get_meta_value_l(index, key, entry);
	}

	// First get the name of this entry.
	status_t err = get_meta_value_l(index, key, NULL);
	if (err != B_OK) return B_END_OF_DATA;

	sptr<node_state> state = get_state_l();
	if (state != NULL) {
		sptr<BValueDatum> datum = state->GetDatumLocked(index, true);
		if (datum != NULL) {
			*entry = SValue::Binder(datum->AsBinder());
			return B_OK;
		}
	}

	return B_NO_MEMORY;
}

size_t BGenericNode::CountMetaEntriesLocked() const
{
	return 0;
}

lock_status_t BGenericNode::Lock() const
{
	return m_lock.Lock();
}

void BGenericNode::Unlock() const
{
	m_lock.Unlock();
}

sptr<BGenericNode::node_state> BGenericNode::get_state_l() const
{
	sptr<node_state> state = m_state.promote();
	if (state == NULL) {
		state = new node_state(const_cast<BGenericNode*>(this));
		m_state = state;
	}
	return state;
}

status_t BGenericNode::get_meta_value_l(ssize_t index, SValue* key, SValue* value)
{
	switch (index) {
		case kMimeTypeIndex:
			if (key) *key = kMimeType;
			if (value) *value = SValue::String(MimeTypeLocked());
			return B_OK;
		case kCreationDateIndex:
			if (key) *key = kCreationDate;
			if (value) *value = SValue::Time(CreationDateLocked());
			return B_OK;
		case kModifiedDateIndex:
			if (key) *key = kModifiedDate;
			if (value) *value = SValue::Time(ModifiedDateLocked());
			return B_OK;
	}
	return B_BAD_INDEX;
}

status_t BGenericNode::datum_changed_l(size_t index, const SValue& newValue)
{
	status_t err;
	if (index == kMimeTypeIndex) {
		SString v(newValue.AsString(&err));
		if (err == B_OK) err = SetMimeTypeLocked(v);
	} else {
		nsecs_t v(newValue.AsTime(&err));
		if (err == B_OK) {
			if (index == kCreationDateIndex) err = SetCreationDateLocked(v);
			else if (index == kModifiedDateIndex) err = SetModifiedDateLocked(v);
		}
	}
	return err;
}

// =================================================================================
// =================================================================================
// =================================================================================

BGenericNode::node_state::node_state(const sptr<BGenericNode>& data)
	: m_data(data)
{
}

BGenericNode::node_state::~node_state()
{
	SAutolock _l(m_data->Lock());
	if (m_data->m_state == this) m_data->m_state = NULL;
}

lock_status_t BGenericNode::node_state::Lock() const
{
	return m_data->Lock();
}

void BGenericNode::node_state::Unlock() const
{
	m_data->Unlock();
}

status_t BGenericNode::node_state::AddEntry(const SString& name, const SValue& entry)
{
	return m_data->CreateMetaEntry(name, entry);
}

status_t BGenericNode::node_state::RemoveEntry(const SString& name)
{
	return m_data->RemoveMetaEntry(name);
}

status_t BGenericNode::node_state::RenameEntry(const SString& old_name, const SString& new_name)
{
	return m_data->RenameMetaEntry(old_name, new_name);
}

sptr<INode> BGenericNode::node_state::CreateNode(SString* name, status_t* err)
{
	*err = B_PERMISSION_DENIED;
	return NULL;
}

sptr<IDatum> BGenericNode::node_state::CreateDatum(SString* name, uint32_t flags, status_t* err)
{
	*err = B_UNSUPPORTED;
	return NULL;
}

status_t BGenericNode::node_state::EntryAtLocked(size_t index, uint32_t flags, SValue* key, SValue* entry)
{
	return m_data->MetaEntryAtLocked(ssize_t(index)-kNumMetaData, flags, key, entry);
}

size_t BGenericNode::node_state::CountEntriesLocked() const
{
	return m_data->CountMetaEntriesLocked()+kNumMetaData;
}

status_t BGenericNode::node_state::LookupEntry(const SString& entry, uint32_t flags, SValue* node)
{
	return m_data->LookupMetaEntry(entry, flags, node);
}

sptr<BValueDatum> BGenericNode::node_state::GetDatumLocked(size_t index, bool create)
{
	sptr<BValueDatum> d = m_datums[index].promote();
	if (!create) return d;
	if (d == NULL) {
		SValue val;
		m_data->get_meta_value_l(index, NULL, &val);
		// XXX Need to set READ_ONLY mode!
		d = new BGenericNode::meta_datum(m_data, index, val);
		m_datums[index] = d.ptr();
	}
	return d;
}

void BGenericNode::node_state::RemoveDatumLocked(size_t index, BValueDatum* datum)
{
	if (m_datums[index] == datum) m_datums[index] = NULL;
}

// =================================================================================
// =================================================================================
// =================================================================================

BGenericNode::meta_datum::meta_datum(const sptr<BGenericNode>& data, size_t index, const SValue& val, uint32_t mode)
	: BValueDatum(val, mode)
	, m_data(data)
	, m_index(index)
{
}

lock_status_t BGenericNode::meta_datum::Lock() const
{
	return m_data->Lock();
}

void BGenericNode::meta_datum::Unlock() const
{
	m_data->Unlock();
}

void BGenericNode::meta_datum::ReportChangeLocked(const sptr<IBinder>& editor, uint32_t changes, off_t start, off_t length)
{
	// XXX We are not dealing correctly here with type conversions, errors.
	m_data->datum_changed_l(m_index, ValueLocked());
	BValueDatum::ReportChangeLocked(editor, changes, start, length);
}

status_t BGenericNode::meta_datum::StoreValueTypeLocked(uint32_t type)
{
	return B_NOT_ALLOWED;
}

status_t BGenericNode::meta_datum::StoreSizeLocked(off_t size)
{
	if (ValueTypeLocked() == B_STRING_TYPE) return BValueDatum::StoreSizeLocked(size);
	return B_NOT_ALLOWED;
}

status_t BGenericNode::meta_datum::StoreValueLocked(const SValue& value)
{
	const uint32_t curType = ValueTypeLocked();
	if (curType != value.Type()) return B_BAD_TYPE;
	if (curType != B_STRING_TYPE && SizeLocked() != value.Length()) return B_BAD_VALUE;
	return BValueDatum::StoreValueLocked(value);
}

BGenericNode::meta_datum::~meta_datum()
{
	SAutolock _l(m_data->Lock());
	sptr<node_state> state = m_data->m_state.promote();
	if (state != NULL) {
		state->RemoveDatumLocked(m_index, this);
	}
}

// =================================================================================
// =================================================================================

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
