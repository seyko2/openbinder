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

#ifndef _SUPPORT_CATALOG_H
#define _SUPPORT_CATALOG_H

/*!	@file support/Catalog.h
	@ingroup CoreSupportDataModel
	@brief Helpers for implementing ICatalog and its associated interfaces.
*/

#include <storage/DatumGeneratorInt.h>
#include <storage/IndexedIterable.h>
#include <storage/MetaDataNode.h>

#include <support/Context.h>
#include <support/ICatalog.h>
#include <support/KeyedVector.h>
#include <support/Locker.h>
#include <support/Node.h>
#include <support/String.h>
#include <support/Value.h>


#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

// Temporary compatibility
typedef SNode SCatalog;

class SWalkHelper
{
public:
	status_t	HelperWalk(SString* path, uint32_t flags, SValue* node);
	
	virtual sptr<INode> 	HelperAttributesCatalog() const;
	virtual status_t		HelperLookupEntry(const SString& entry, uint32_t flags, SValue* node) = 0;
	virtual	sptr<ICatalog>	HelperCreateCatalog(SString* name, status_t* err);
	virtual	sptr<IDatum>	HelperCreateDatum(SString* name, uint32_t flags, status_t* err);
};

class BGenericCatalog : public BnCatalog, public BMetaDataNode, public BnIterable
{
public:
	BGenericCatalog();
	BGenericCatalog(const SContext& context);

	// Deal with multiple interfaces
	inline const SContext& Context() const { return BnCatalog::Context(); }
	virtual SValue Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);
	
	// Stub these out for ICatalog (call to BMetaDataNode implementation).
	virtual	sptr<INode> CreateNode(SString* name, status_t* err);
	virtual	sptr<IDatum> CreateDatum(SString* name, uint32_t flags, status_t* err);

protected:
	virtual ~BGenericCatalog();
};

class BIndexedCatalog : public BnCatalog, public BMetaDataNode, public BIndexedIterable
{
public:
	BIndexedCatalog();
	BIndexedCatalog(const SContext& context);
	
	// Deal with multiple interfaces
	inline const SContext& Context() const { return BnCatalog::Context(); }
	virtual SValue Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);

	//!	Tie together the locks.
	virtual	lock_status_t			Lock() const;
	virtual	void					Unlock() const;

	//!	Implement for compatibility.
	virtual	status_t				EntryAtLocked(	const sptr<IndexedIterator>& it, size_t index,
													uint32_t flags, SValue* key, SValue* entry);
	//!	Old BIndexedCatalog API.
	virtual status_t				EntryAtLocked(size_t index, uint32_t flags, SValue* key, SValue* entry) = 0;

	// Stub these out for ICatalog (call to BMetaDataNode implementation).
	virtual	sptr<INode> CreateNode(SString* name, status_t* err);
	virtual	sptr<IDatum> CreateDatum(SString* name, uint32_t flags, status_t* err);

protected:
	virtual ~BIndexedCatalog();

	//! XXX do not use these!  Use BIndexedIterable::UpdateIteratorIndicesLocked() instead!!
	void EntryAddedAt(uint32_t index);
	void EntryRemovedAt(uint32_t index);
};

class BMetaDataCatalog : public BIndexedCatalog
{
public:
	BMetaDataCatalog(const SContext& context);

	// Implement BGenericNode attributes.
	virtual status_t			LookupMetaEntry(const SString& entry, uint32_t flags, SValue* node);
	virtual	status_t			CreateMetaEntry(const SString& name, const SValue& initialValue, sptr<IDatum>* outDatum = NULL);
	virtual	status_t			RemoveMetaEntry(const SString& name);
	virtual	status_t			RenameMetaEntry(const SString& old_name, const SString& new_name);
	virtual	status_t			MetaEntryAtLocked(ssize_t index, uint32_t flags, SValue* key, SValue* entry);
	virtual	size_t				CountMetaEntriesLocked() const;

protected:
	virtual ~BMetaDataCatalog();
	
private:
	SKeyedVector<SString, SValue>*	m_attrs;
};

class BCatalog : public BMetaDataCatalog, public SDatumGeneratorInt
{
public:
	BCatalog();
	BCatalog(const SContext& context);

	virtual void InitAtom();

	//!	Tie together the locks.
	virtual	lock_status_t			Lock() const;
	virtual	void					Unlock() const;

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
	//!	Return the current value in the catalog.
	virtual	SValue ValueAtLocked(size_t index) const;
	//!	Change a value in the catalog.
	virtual	status_t StoreValueAtLocked(size_t index, const SValue& value);

	virtual	ssize_t AddEntryLocked(const SString& name, const SValue& entry, sptr<IBinder>* outEntry, bool* replaced);

protected:
	//! You can override this to create something other than a BCatalog.
	virtual sptr<INode> InstantiateNodeLocked(const SString &name, status_t *err);
	
	//! these are hook functions that get called when things happen
	virtual void OnEntryCreated(const SString &name, const sptr<IBinder> &entry);
	virtual void OnEntryModified(const SString &name, const sptr<IBinder> &entry);
	virtual void OnEntryRenamed(const SString &old_name, const SString &new_name, const sptr<IBinder> &entry);
	virtual void OnEntryRemoved(const SString &name);
	
	virtual ~BCatalog();

private:
	bool has_entry_l(const SString& name);
	
	SKeyedVector<SString, SValue> m_entries;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif // _SUPPORT_CATALOG_H

