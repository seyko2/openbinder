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

#include <support/CatalogMirror.h>

#include <storage/ValueDatum.h>

#include <support/Catalog.h>
#include <support/IIterator.h>
#include <support/Autolock.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// ----------------------------------------------------------------------------------

class BMergedIterator : public BnRandomIterator
{
public:
	BMergedIterator(const SValue& options = B_UNDEFINED_VALUE);

	// IIterator
	virtual	SValue Options() const;
	virtual	status_t Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count);
	virtual status_t Remove();

	// IRandomIterator
	virtual size_t Count() const;
	virtual	size_t Position() const;
	virtual void SetPosition(size_t p);


	void Merge(const sptr<IIterator>& it);

private:
	mutable SLocker	m_lock;
	size_t m_position;
	SVector< sptr<IIterator> > m_iterators;
	SKeyedVector<SValue, SValue> m_data;
	SValue m_options;
};
	

// ----------------------------------------------------------------------------------

BMergedIterator::BMergedIterator(const SValue& options)
	:	m_position(0),
		m_options(options)
{
}

void BMergedIterator::Merge(const sptr<IIterator>& it)
{
	m_iterators.AddItem(it);

	// determine if the iterator supports the same options as
	// we do. if it does not then we have to sort it ourselves
	
	IIterator::ValueList keys;
	IIterator::ValueList values;

	while (it->Next(&keys, &values, 0, 0) == B_OK)
	{
		const size_t size = keys.CountItems();
		for (size_t j = 0 ; j < size ; j++)
		{
			m_data.AddItem(keys.ItemAt(j), values.ItemAt(j));
		}
	}
}

SValue BMergedIterator::Options() const
{
	return m_options;
}

status_t BMergedIterator::Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count)
{
	SLocker::Autolock lock(m_lock);

	status_t result = B_END_OF_DATA;
	const size_t totalItems = m_data.CountItems();
	size_t index = 0;

	if ((count == 0) || (count > IIterator::BINDER_IPC_LIMIT))
	{
		count = IIterator::BINDER_IPC_LIMIT;
	}

	keys->MakeEmpty();
	values->MakeEmpty();

	while ((index < count) && (m_position < totalItems))
	{
		keys->AddItem(m_data.KeyAt(m_position));
		SValue value = m_data.ValueAt(m_position);
		if ((flags & INode::REQUEST_DATA))
		{
			sptr<IDatum> datum = IDatum::AsInterface(value);
			if (datum != NULL)
			{
				values->AddItem(datum->Value());
			}
			else
			{
				// if this wasn't a datum, then just add it
				values->AddItem(value);
			}
		}
		else
		{
			values->AddItem(value);
		}

		m_position++;
		index++;
		result = B_OK;
	}

	return result;
}

status_t BMergedIterator::Remove()
{
	return B_UNSUPPORTED;
}

size_t BMergedIterator::Count() const
{
	SLocker::Autolock lock(m_lock);
	return m_data.CountItems();
}

size_t BMergedIterator::Position() const
{
	SLocker::Autolock lock(m_lock);
	return m_position;
}

void BMergedIterator::SetPosition(size_t value)
{
	SLocker::Autolock lock(m_lock);
	if (value < m_data.CountItems())
	{
		m_position = value;
	}
}

// ---------------------------------------------------------------------------
/*	BCatalogMirror::IteratorWrapper wraps an existing iterator
 *	As the iterator is done, it checks for
 *	any hidden or overlays and modifies the
 *	results accordingly.
 */

class BCatalogMirror::IteratorWrapper : public BnRandomIterator
{
public:
						IteratorWrapper(const sptr<BCatalogMirror>& mirror, const SValue& args = B_UNDEFINED_VALUE);
	
	virtual	SValue		Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags = 0);

	void				ModifyNext(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags);

	// IIterator
	virtual	SValue		Options() const;
	virtual	status_t	Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count);
	virtual status_t	Remove();
	// IRandomIterator
	virtual size_t		Count() const;
	virtual	size_t		Position() const;
	virtual void		SetPosition(size_t p);

	ssize_t				AddItem(const SString& name, const SValue& value);

private:
	const sptr<BCatalogMirror> m_mirror;
	SValue m_args;
	sptr<IIterator> m_iterator;
	sptr<IRandomIterator> m_randomIterator;
};

// ---------------------------------------------------------------------------
// BCatalogMirror::IteratorWrapper member functions
// ---------------------------------------------------------------------------

BCatalogMirror::IteratorWrapper::IteratorWrapper(const sptr<BCatalogMirror>& mirror, const SValue& args)
				 : m_mirror(mirror), m_args(args)
{
	// XXX Need to propagate error from NewIterator()!
	m_iterator = m_mirror->m_iterable->NewIterator(args);
	m_randomIterator = interface_cast<IRandomIterator>(m_iterator->AsBinder());
}

// ---------------------------------------------------------------------------

SValue BCatalogMirror::IteratorWrapper::Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags)
{
	SLocker::Autolock _l(m_mirror->m_permissionsLock);

	// We can be a random iterator if the target catalog itself is
	// a random iterator, AND we have not done any visible modifications
	// to the contents of the catalog.
	if (m_randomIterator != NULL && m_mirror->m_hidden.CountItems() == 0 && m_mirror->m_overlays.CountItems() == 0) {
		return BnRandomIterator::Inspect(caller, which, flags);
	}

	return which * SValue(IIterator::Descriptor(), SValue::Binder(this));
}

// ---------------------------------------------------------------------------

ssize_t
BCatalogMirror::IteratorWrapper::AddItem(const SString&, const SValue&)
{
	// AddItem is never called on IteratorWrapper
	return 0;
}

// ---------------------------------------------------------------------------

SValue
BCatalogMirror::IteratorWrapper::Options() const
{
	return m_iterator->Options();
}

// ---------------------------------------------------------------------------

status_t
BCatalogMirror::IteratorWrapper::Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count)
{		
	// If we remove all items in ModifyNext, then
	// we need to go back and get another batch
	// so we have to wrap up our Next into a loop
	status_t result = B_OK;
	while (true) {
		result = m_iterator->Next(keys, values, flags, count);
		if (result != B_OK) {
			break;
		}
		this->ModifyNext(keys, values, flags);
		if (keys->CountItems() > 0) {
			break;
		}
	}
	return result;
}

// ---------------------------------------------------------------------------

status_t BCatalogMirror::IteratorWrapper::Remove()
{
	// XXX This could be supported in some scenarios.
	return B_UNSUPPORTED;
}

// ---------------------------------------------------------------------------

size_t BCatalogMirror::IteratorWrapper::Count() const
{
	if (m_randomIterator != NULL) return m_randomIterator->Count();
	return 0;
}

// ---------------------------------------------------------------------------

size_t BCatalogMirror::IteratorWrapper::Position() const
{
	if (m_randomIterator != NULL) return m_randomIterator->Position();
	return 0;
}

// ---------------------------------------------------------------------------

void BCatalogMirror::IteratorWrapper::SetPosition(size_t p)
{
	if (m_randomIterator != NULL) m_randomIterator->SetPosition(p);
}

// ---------------------------------------------------------------------------

void
BCatalogMirror::IteratorWrapper::ModifyNext(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags)
{
	// Modify the keys and values based on what the mirror holds in its
	// hidden and overlay lists
	// Since we are reaching into the BCatalogMirror, we lock the data while
	// we are doing so

	SLocker::Autolock _l(m_mirror->m_permissionsLock);

	// Since we might be removing items, we count down so we don't
	// have to worry about adjusting index after removal
	// (turning count & i into int32_t is on purpose, if count
	// is zero, the loop should not be run)
	int32_t count = keys->CountItems();
	for (int32_t i = count-1; i >= 0; i--) {
		SString key = (*keys)[i].AsString();
		// first see if the item is on our hidden list
		// if so, remove it from Next
		// if not, then check if in overlay list and replace
		if (m_mirror->m_hidden.IndexOf(key) >=0) {
			keys->RemoveItemsAt(i);
			values->RemoveItemsAt(i);	
		}
		else {
			bool found = false;
			sptr<IBinder> entry = m_mirror->m_overlays.ValueFor(key, &found).AsBinder();
			if (found == true) {
				// once again we need to mimic ICatalog
				// if flags ask for REQUEST_DATA
				// we need to see if we can get to the IDatum
				SValue value = SValue::Binder(entry);
				if ((flags & INode::REQUEST_DATA)) {
					sptr<IDatum> datum = interface_cast<IDatum>(value);
					if (datum != NULL) {
						values->ReplaceItemAt(datum->Value(), i);
					}
					else {
						values->ReplaceItemAt(value, i);
					}
				}
				else {
					values->ReplaceItemAt(value, i);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// BCatalogMirror member functions
// ---------------------------------------------------------------------------

//! BCatalogMirror constructor - takes "catalog" or "catalog_path"
/*! BCatalogMirror can be given:
 *		@{catalog->ICatalog}
 *		@{catalog_path->SString}
 *	The ICatalog is the catalogectory that we will be mirroring
 */
BCatalogMirror::BCatalogMirror(const SContext& context, const SValue& args)
				: BnCatalogPermissions(context),
				  BnCatalog(context),
				  BnNode(context),
				  BnIterable(context),
				  BObserver(context),
				  m_permissionsLock("BCatalogMirror::m_permissionsLock"),
				  m_node(NULL),
				  m_catalog(NULL),
				  m_iterable(NULL),
				  m_writable(true),
				  m_linkedToNode(false)
{
	init_from_args(context, args);
}

// ---------------------------------------------------------------------------

BCatalogMirror::~BCatalogMirror()
{
	if (m_linkedToNode) {
		m_node->AsBinder()->Unlink((BObserver*)this, B_WILD_VALUE, B_WEAK_BINDER_LINK|B_SYNC_BINDER_LINK);
	}
}

// ---------------------------------------------------------------------------

SValue
BCatalogMirror::Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags)
{
	SValue result(BnCatalog::Inspect(caller, which, flags));
	result.Join(BnNode::Inspect(caller, which, flags));
	result.Join(BnIterable::Inspect(caller, which, flags));

	if (caller == static_cast<BnCatalogPermissions*>(this)) {
		// ICatalogPermissions is a full-access interface, you can
		// get to any others from them
		result.Join(BnCatalogPermissions::Inspect(caller, which, flags));
	}
	return result;
}

// ---------------------------------------------------------------------------

status_t BCatalogMirror::Link(const sptr<IBinder>& target, const SValue& bindings, uint32_t flags)
{
	SLocker::Autolock _l(m_permissionsLock);

	// If this is the first link, start watching everything that happens in the
	// underlying catalog.
	if (!BnNode::IsLinked() && !m_linkedToNode) {
		m_node->AsBinder()->Link((BObserver*)this, B_WILD_VALUE, B_WEAK_BINDER_LINK|B_SYNC_BINDER_LINK);
		m_linkedToNode = true;
	}

	return BnNode::Link(target, bindings, flags);
}

// ---------------------------------------------------------------------------

status_t BCatalogMirror::Unlink(const wptr<IBinder>& target, const SValue& bindings, uint32_t flags)
{
	SLocker::Autolock _l(m_permissionsLock);

	status_t err = BnNode::Unlink(target, bindings, flags);

	// If this was the last link, stop watching the underlying catalog.
	if (!BnNode::IsLinked() && m_linkedToNode) {
		m_node->AsBinder()->Unlink((BObserver*)this, B_WILD_VALUE, B_WEAK_BINDER_LINK|B_SYNC_BINDER_LINK);
		m_linkedToNode = false;
	}

	return err;
}

// ---------------------------------------------------------------------------

void BCatalogMirror::Observed(const SValue& key, const SValue& value)
{
	SString entry = value[B_1_INT32].AsString();

	m_permissionsLock.LockQuick();
		ssize_t hiddenIndex = m_hidden.IndexOf(entry);
	m_permissionsLock.Unlock();

	if (hiddenIndex < 0)
	{
		// only push if the entry is not hidden.
		BnNode::Push(SValue(key, value));
	}
}

// ---------------------------------------------------------------------------

void
BCatalogMirror::init_from_args(const SContext& context, const SValue& args)
{
	// First try to find "catalog", if that fails, use "catalog_path"
	// And then set up the catalogectory we mirror
	// If all else fails, then use /tmp so we don't crash later on.

	SValue catalogectory = args["catalog"];
	if (catalogectory.IsDefined() == false) {
		SValue catalogectoryPath = args["catalog_path"];
		if (catalogectoryPath.IsDefined()) {
			catalogectory = context.Lookup(catalogectoryPath.AsString());
		}
	}

	if (catalogectory.IsDefined() == true) {
		sptr<IBinder> binder = catalogectory.AsBinder();
		m_node = interface_cast<INode>(binder);
		m_catalog = interface_cast<ICatalog>(binder);
		m_iterable = interface_cast<IIterable>(binder);
	}

	DbgOnlyFatalErrorIf(m_node == NULL, "BCatalogMirror did not get a valid catalog");
	if (m_node == NULL) {
		catalogectory = context.Lookup(SString("/tmp"));
		sptr<IBinder> binder = catalogectory.AsBinder();
		m_node = interface_cast<INode>(binder);
		m_catalog = interface_cast<ICatalog>(binder);
		m_iterable = interface_cast<IIterable>(binder);
	}
}

// ---------------------------------------------------------------------------

bool BCatalogMirror::is_writable(const SString& name) const
{
	m_permissionsLock.LockQuick();
	bool writable = m_writable && m_hidden.IndexOf(name) < 0 && m_overlays.IndexOf(name) < 0;
	m_permissionsLock.Unlock();
	return writable;
}

// ---------------------------------------------------------------------------

status_t BCatalogMirror::AddEntry(const SString& name, const SValue& entry)
{
	if (m_catalog == NULL) return B_UNSUPPORTED;
	
	status_t err = B_PERMISSION_DENIED;
	if (is_writable(name)) {
		err = m_catalog->AddEntry(name, entry);
	}
#if BUILD_TYPE == BUILD_TYPE_DEBUG
	else {
		bout << "WARNING! CatalogMirror permissions do not allow adding " << name << endl;
	}
#endif
	return err;
}

// ---------------------------------------------------------------------------

status_t BCatalogMirror::RemoveEntry(const SString& name)
{
	if (m_catalog == NULL) return B_UNSUPPORTED;

	status_t err = B_PERMISSION_DENIED;
	if (is_writable(name)) {
		err = m_catalog->RemoveEntry(name);
	}
#if BUILD_TYPE == BUILD_TYPE_DEBUG
	else {
		bout << "WARNING! CatalogMirror permissions do not allow removing " << name << endl;
	}
#endif
	return err;
}

// ---------------------------------------------------------------------------

status_t BCatalogMirror::RenameEntry(const SString& old_name, const SString& new_name)
{
	if (m_catalog == NULL) return B_UNSUPPORTED;

	status_t err = B_PERMISSION_DENIED;
	if (is_writable(old_name) && is_writable(new_name)) {
		err = m_catalog->RenameEntry(old_name, new_name);
	}
#if BUILD_TYPE == BUILD_TYPE_DEBUG
	else {
		bout << "WARNING! CatalogMirror permissions do not allow renaming " << old_name << endl;
	}
#endif
	return err;
}

// ---------------------------------------------------------------------------

sptr<INode> BCatalogMirror::CreateNode(SString* name, status_t* err)
{
	if (m_catalog == NULL) return NULL;

	sptr<INode> catalog = NULL;
	if (is_writable(*name)) {
		catalog = m_catalog->CreateNode(name, err);
	} else {
#if BUILD_TYPE == BUILD_TYPE_DEBUG
		bout << "WARNING! CatalogMirror permissions do not allow creating node " << *name << endl;
#endif
		if (err != NULL) *err = B_PERMISSION_DENIED;
	}
	return catalog;
}

// ---------------------------------------------------------------------------

sptr<IDatum> BCatalogMirror::CreateDatum(SString* name, uint32_t flags, status_t* err)
{
	if (m_catalog == NULL) return NULL;

	sptr<IDatum> datum = NULL;
	if (is_writable(*name)) {
		datum = m_catalog->CreateDatum(name, flags, err);
	} else {
#if BUILD_TYPE == BUILD_TYPE_DEBUG
		bout << "WARNING! CatalogMirror permissions do not allow creating datum " << name << endl;
#endif
		if (err != NULL) *err = B_PERMISSION_DENIED;
	}
	return datum;
}

// ---------------------------------------------------------------------------

sptr<IIterator> BCatalogMirror::NewIterator(const SValue& args, status_t* error)
{
	IteratorWrapper* iter = (m_iterable == NULL) ? NULL : new IteratorWrapper(this, args);
	if (error) *error = (iter == NULL )? B_NO_MEMORY : B_OK;
	return iter;
}

// ---------------------------------------------------------------------------

status_t BCatalogMirror::Walk(SString* path, uint32_t flags, SValue* node)
{
	// There are multiple returns from this function
	// If we walk to a hidden node, then return not found
	// If we walk to an overlay, then figure out what to do based on type
	// Otherwise walk to original catalogectory

	// Note that path must be modified as we work, so I do modify
	// path here, but if we decide that we need to delegate to
	// m_node, then we restore to our original so that m_node
	// can then modify it as needed
	SString originalPath(*path);
	SString name;
	path->PathRemoveRoot(&name);

	m_permissionsLock.LockQuick();

		// if the path is in our hidden list then sorry - not found
		ssize_t hiddenIndex = m_hidden.IndexOf(name);
		if (hiddenIndex >=0) {
			m_permissionsLock.Unlock();
			return B_NAME_NOT_FOUND;
		}

		// if the path is in our overlay list then we need to
		// do what the ICatalog::Walk does
		// If it is an ICatalog, then keep walking, otherwise
		// return the Datum if possible.
		bool found = false;
		sptr<IBinder> entry = m_overlays.ValueFor(name, &found).AsBinder();

	m_permissionsLock.Unlock();

	if (found == true) {
		if (path->Length() > 0) {
			sptr<INode> inode = interface_cast<INode>(entry);
			if (inode != NULL) {
				return inode->Walk(path, flags, node);
			}
			else {
				return B_ENTRY_NOT_FOUND;
			}
		}
		else if ((flags & INode::REQUEST_DATA)) {
			sptr<IDatum> datum = interface_cast<IDatum>(entry);
			if (datum != NULL) {
				*node = datum->Value();
			}
			else {
				*node = SValue::Binder(entry);
			}
		}
		else {
			*node = SValue::Binder(entry);
		}
		return B_OK;
	}

	// restore path to original for m_node
	*path = originalPath;
	return m_node->Walk(path, flags, node);
}


sptr<INode> BCatalogMirror::Attributes() const
{
	return m_node->Attributes();
}

SString BCatalogMirror::MimeType() const
{
	return m_node->MimeType();
}

void BCatalogMirror::SetMimeType(const SString& value)
{
	m_node->SetMimeType(value);
}

nsecs_t BCatalogMirror::CreationDate() const
{
	return m_node->CreationDate();
}

void BCatalogMirror::SetCreationDate(nsecs_t value)
{
	m_node->SetCreationDate(value);
}

nsecs_t BCatalogMirror::ModifiedDate() const
{
	return m_node->ModifiedDate();
}

void BCatalogMirror::SetModifiedDate(nsecs_t value)
{
	m_node->SetModifiedDate(value);
}

// ---------------------------------------------------------------------------

bool
BCatalogMirror::Writable() const
{
	return m_writable;
}

// ---------------------------------------------------------------------------

void
BCatalogMirror::SetWritable(bool value)
{
	SLocker::Autolock _l(m_permissionsLock);
	m_writable = value;
}

// ---------------------------------------------------------------------------

status_t
BCatalogMirror::HideEntry(const SString& name)
{
	// Add the item to our hidden list so that
	// we can track it for Walk & Iterate

	SLocker::Autolock _l(m_permissionsLock);
	bool added = false;
	m_hidden.AddItem(name, &added);
	return added == true ? B_OK : B_NAME_IN_USE;
}

// ---------------------------------------------------------------------------

status_t
BCatalogMirror::ShowEntry(const SString& name)
{
	// See if we have the item in our hidden list
	// and if so, remove it

	SLocker::Autolock _l(m_permissionsLock);
	ssize_t index = m_hidden.IndexOf(name);
	status_t err = B_OK;
	if (index >= 0) {
		m_hidden.RemoveItemsAt(index);
	}
	else {
		err = B_BAD_VALUE;
	}

	return err;
}

// ---------------------------------------------------------------------------

status_t
BCatalogMirror::OverlayEntry(const SString& location, const SValue& item)
{
	// Add the location + item pair to our list of overlay items
	// Here we need to pretend we are ICatalog because when we go to
	// fetch the item, it needs to either be an IBinder or an IDatum
	// so just like BCatalog::AddEntry, we potentially need to wrap
	// the item

	SLocker::Autolock _l(m_permissionsLock);
	
	SValue store;
	if (item.AsBinder() == NULL) {
		sptr<IDatum> datum = new BValueDatum();
		datum->SetValue(item);
		store = datum->AsBinder();
	} else {
		store = item;
	}
	ssize_t index = m_overlays.AddItem(location, store);
	return index >= 0 ? B_OK : B_BAD_VALUE;
}

// ---------------------------------------------------------------------------

status_t
BCatalogMirror::RestoreEntry(const SString& location)
{
	// Using the location as a key, remove the location+item pair
	// from our overlay items list

	SLocker::Autolock _l(m_permissionsLock);
	ssize_t	index = m_overlays.RemoveItemFor(location);
	return index >= 0 ? B_OK : B_BAD_VALUE;
}


status_t BCatalogMirror::Merge(const sptr<ICatalog>& catalog)
{
	return B_OK;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
