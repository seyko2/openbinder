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

#include <storage/CatalogDelegate.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

// *********************************************************************************
// ** BCatalogDelegate *************************************************************
// *********************************************************************************

BCatalogDelegate::BCatalogDelegate(const SContext& context, const sptr<IBinder>& base)
	: BNodeDelegate(context, interface_cast<INode>(base))
	, BGenericIterable(context)
	, BnCatalog(context)
	, m_baseIterable(interface_cast<IIterable>(base))
	, m_baseCatalog(interface_cast<ICatalog>(base))
{
}

BCatalogDelegate::~BCatalogDelegate()
{
}

lock_status_t BCatalogDelegate::Lock() const
{
	return BGenericNode::Lock();
}

void BCatalogDelegate::Unlock() const
{
	BGenericNode::Unlock();
}

SValue BCatalogDelegate::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	SValue result;
	if (BaseNode() != NULL) result.Join(BGenericNode::Inspect(caller, which, flags));
	if (m_baseIterable != NULL) result.Join(BnIterable::Inspect(caller, which, flags));
	if (m_baseCatalog != NULL) result.Join(BnCatalog::Inspect(caller, which, flags));
	return result;
}

sptr<BGenericIterable::GenericIterator> BCatalogDelegate::NewGenericIterator(const SValue& args)
{
	return new IteratorDelegate(Context(), this);
}

status_t BCatalogDelegate::AddEntry(const SString& name, const SValue& entry)
{
	return m_baseCatalog->AddEntry(name, entry);
}

status_t BCatalogDelegate::RemoveEntry(const SString& name)
{
	return m_baseCatalog->RemoveEntry(name);
}

status_t BCatalogDelegate::RenameEntry(const SString& entry, const SString& name)
{
	return m_baseCatalog->RenameEntry(entry, name);
}

sptr<INode> BCatalogDelegate::CreateNode(SString* name, status_t* err)
{
	return m_baseCatalog->CreateNode(name, err);
}

sptr<IDatum> BCatalogDelegate::CreateDatum(SString* name, uint32_t flags, status_t* err)
{
	return m_baseCatalog->CreateDatum(name, flags, err);
}

// =================================================================================

BCatalogDelegate::IteratorDelegate::IteratorDelegate(const SContext& context, const sptr<BGenericIterable>& owner)
	: GenericIterator(context, owner)
	, m_baseError(B_OK)
{
}

BCatalogDelegate::IteratorDelegate::~IteratorDelegate()
{
}

SValue BCatalogDelegate::IteratorDelegate::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	if (m_baseRandomIterator != NULL)
		return BnRandomIterator::Inspect(caller, which, flags);
	return GenericIterator::Inspect(caller, which, flags);
}

status_t BCatalogDelegate::IteratorDelegate::StatusCheck() const
{
	return m_baseError;
}

SValue BCatalogDelegate::IteratorDelegate::Options() const
{
	return m_baseIterator->Options();
}

status_t BCatalogDelegate::IteratorDelegate::Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count)
{
	return m_baseIterator->Next(keys, values, flags, count);
}

status_t BCatalogDelegate::IteratorDelegate::Remove()
{
	return m_baseRandomIterator->Remove();
}

size_t BCatalogDelegate::IteratorDelegate::Count() const
{
	return m_baseRandomIterator->Count();
}

size_t BCatalogDelegate::IteratorDelegate::Position() const
{
	return m_baseRandomIterator->Position();
}

void BCatalogDelegate::IteratorDelegate::SetPosition(size_t p)
{
	m_baseRandomIterator->SetPosition(p);
}

status_t BCatalogDelegate::IteratorDelegate::ParseArgs(const SValue& args)
{
	BCatalogDelegate* cat = static_cast<BCatalogDelegate*>(Owner().ptr());

	status_t err;
	sptr<IIterator> it = cat->BaseIterable()->NewIterator(args, &err);

	SAutolock _l(Owner()->Lock());
	m_baseError = err;
	m_baseIterator = it;
	m_baseRandomIterator = interface_cast<IRandomIterator>(it->AsBinder());

	return err;
}

status_t BCatalogDelegate::IteratorDelegate::NextLocked(uint32_t flags, SValue* key, SValue* entry)
{
	DbgOnlyFatalError("BCatalogDelegate::IteratorDelegate::NextLocked() not implemented");
	return B_END_OF_DATA;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
