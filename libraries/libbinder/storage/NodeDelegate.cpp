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

#include <storage/NodeDelegate.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

// *********************************************************************************
// ** BNodeDelegate ****************************************************************
// *********************************************************************************

BNodeDelegate::BNodeDelegate(const SContext& context, const sptr<INode>& base)
	: BGenericNode(context)
	, BnNodeObserver(context)
	, m_baseNode(base)
{
}

BNodeDelegate::~BNodeDelegate()
{
	if (m_linkedToNode) {
		m_baseNode->AsBinder()->Unlink((BnNodeObserver*)this, B_WILD_VALUE, B_WEAK_BINDER_LINK|B_SYNC_BINDER_LINK);
	}
}

status_t BNodeDelegate::Link(const sptr<IBinder>& target, const SValue& bindings, uint32_t flags)
{
	SAutolock _l(Lock());

	// If this is the first link, start watching everything that happens in the
	// underlying node.
	if (m_baseNode != NULL && !BnNode::IsLinked() && !m_linkedToNode) {
		m_baseNode->AsBinder()->Link((BnNodeObserver*)this, B_WILD_VALUE, B_WEAK_BINDER_LINK|B_SYNC_BINDER_LINK);
		m_linkedToNode = true;
	}

	return BnNode::Link(target, bindings, flags);
}

status_t BNodeDelegate::Unlink(const wptr<IBinder>& target, const SValue& bindings, uint32_t flags)
{
	SAutolock _l(Lock());

	status_t err = BnNode::Unlink(target, bindings, flags);

	// If this was the last link, stop watching the underlying node.
	if (!BnNode::IsLinked() && m_linkedToNode) {
		m_baseNode->AsBinder()->Unlink((BnNodeObserver*)this, B_WILD_VALUE, B_WEAK_BINDER_LINK|B_SYNC_BINDER_LINK);
		m_linkedToNode = false;
	}

	return err;
}

status_t BNodeDelegate::LookupEntry(const SString& entry, uint32_t flags, SValue* node)
{
	SString name(entry);
	status_t err = m_baseNode->Walk(&name, flags, node);
	// Note: We need to return B_ENTRY_NOT_FOUND to Walk() if we want it to
	// try to call CreateNode()/CreateDatum() for us.
	return err == B_OK ? B_OK : B_ENTRY_NOT_FOUND;
}

SString BNodeDelegate::MimeTypeLocked() const
{
	return m_baseNode->MimeType();
}

status_t BNodeDelegate::StoreMimeTypeLocked(const SString& value)
{
	m_baseNode->SetMimeType(value);
	return B_OK;
}

nsecs_t BNodeDelegate::CreationDateLocked() const
{
	return m_baseNode->CreationDate();
}

status_t BNodeDelegate::StoreCreationDateLocked(nsecs_t value)
{
	m_baseNode->SetCreationDate(value);
	return B_OK;
}

nsecs_t BNodeDelegate::ModifiedDateLocked() const
{
	return m_baseNode->ModifiedDate();
}

status_t BNodeDelegate::StoreModifiedDateLocked(nsecs_t value)
{
	m_baseNode->SetModifiedDate(value);
	return B_OK;
}

void BNodeDelegate::NodeChanged(const sptr<INode>& node, uint32_t flags, const SValue& hints)
{
	PushNodeChanged(this, flags, hints);
}

void BNodeDelegate::EntryCreated(const sptr<INode>& node, const SString& name, const sptr<IBinder>& entry)
{
	PushEntryCreated(this, name, entry);
}

void BNodeDelegate::EntryModified(const sptr<INode>& node, const SString& name, const sptr<IBinder>& entry)
{
	PushEntryModified(this, name, entry);
}

void BNodeDelegate::EntryRemoved(const sptr<INode>& node, const SString& name)
{
	PushEntryRemoved(this, name);
}

void BNodeDelegate::EntryRenamed(const sptr<INode>& node, const SString& old_name, const SString& new_name, const sptr<IBinder>& entry)
{
	PushEntryRenamed(this, old_name, new_name, entry);
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
