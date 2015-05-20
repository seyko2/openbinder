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

#include <support/Iterator.h>

#include <support/IDatum.h>
#include <support/IIterable.h>
#include <support/IIterator.h>
#include <support/INode.h>
#include <support/Looper.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

SIterator::SIterator()
	:	m_index((size_t)B_NO_INIT)
{
}

SIterator::SIterator(const SContext& context, const SString& path, const SValue& args)
{
	SetTo(context, path, args);
}

SIterator::SIterator(const sptr<IIterable>& dir, const SValue& args)
	:	m_index((size_t)B_NO_INIT)
{
	if (dir != NULL) {
		m_iterator = dir->NewIterator(args, (status_t*)&m_index);
		if (m_iterator != NULL) m_index = 0;
	}
}

SIterator::SIterator(const sptr<IIterator>& it)
	:	m_iterator(it),
		m_index(it != NULL ? 0 : (size_t)B_NO_INIT)
{
}

SIterator::SIterator(const sptr<INode>& node, const SValue& args)
	:	m_index((size_t)B_NO_INIT)
{
	if (node != NULL)
	{
		sptr<IIterable> it = interface_cast<IIterable>(node->AsBinder());
		SetTo(it, args);
	}
}

SIterator::~SIterator()
{
}

status_t SIterator::SetTo(const SContext& context, const SString& path, const SValue& args)
{
	sptr<IIterable> dir = IIterable::AsInterface(context.Lookup(path));
	return SetTo(dir, args);
}

status_t SIterator::SetTo(const sptr<IIterable>& dir, const SValue& args)
{
	if (dir != NULL) {
		sptr<IIterator> it(dir->NewIterator(args, (status_t*)&m_index));
		if (it != NULL) {
			return SetTo(it);
		}
	} else {
		m_index = (size_t)B_NO_INIT;
	}
	m_iterator = NULL;
	return (status_t)m_index;
}

status_t SIterator::SetTo(const sptr<IIterator>& it)
{
	m_iterator = it;
	m_index = it != NULL ? 0 : (size_t)B_NO_INIT;
	m_values.MakeEmpty();
	return (status_t)m_index;
}

status_t SIterator::StatusCheck() const
{
	return (m_iterator == NULL) ? (status_t)m_index : B_OK;
}

status_t SIterator::ErrorCheck() const
{
	return StatusCheck();
}

status_t SIterator::Next(SValue* key, SValue* value, uint32_t flags, size_t count)
{
	if (m_iterator == NULL) return (status_t)m_index;
	
	SLocker::Autolock lock(m_lock);

	if (m_index >= m_values.CountItems())
	{
		
		status_t err = m_iterator->Next(&m_keys, &m_values, flags, count);
		if (err != B_OK) return err;
		DbgOnlyFatalErrorIf(m_values.CountItems() == 0, "Iterator returned B_OK but no results!");
		DbgOnlyFatalErrorIf(m_values.CountItems() != 0 && m_keys.CountItems() != m_values.CountItems(), "Iterator returned key count inconsistent with value count!");
		if (m_values.CountItems() == 0) return B_END_OF_DATA;

		// Continue itererator with new data.
		m_index = 0;
	}
	
	*key = m_keys.CountItems() > 0 ? m_keys.ItemAt(m_index) : B_WILD_VALUE;
	*value = m_values.ItemAt(m_index);
	
	m_index++;

	return B_OK;
}

SValue SIterator::ApplyFlags(const sptr<IIterable> &iterable, uint32_t flags)
{
	if (flags & INode::COLLAPSE_CATALOG) {
		SValue result;
		SValue k, v;
		SIterator iterator(iterable->NewIterator());
		
		while (B_OK == iterator.Next(&k, &v, flags)) {
			if ((flags & INode::REQUEST_DATA) == 0) {
				result.JoinItem(k,v);
			} else {
				sptr<IDatum> datum = interface_cast<IDatum>(v);
				if (datum != NULL) {
					v = datum->Value();
				}
				result.JoinItem(k, v);
			}
		}
		
		return result;
	} else {
		return SValue::Binder(iterable->AsBinder());
	}
}


// ==================================================================================


BValueIterator::BValueIterator(const SValue& value)
	:	m_value(value),
		m_cookie(NULL)
{
}

BValueIterator::~BValueIterator()
{
}

SValue BValueIterator::Options() const
{
	return SValue::Undefined();
}

status_t BValueIterator::Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count)
{
	SLocker::Autolock lock(m_lock);
	
	keys->MakeEmpty();
	values->MakeEmpty();

	// if the count is zero lets just try to return 16 values
	if (count == 0) count = 16;

	size_t index = 0;
	SValue key, value;
	while (m_value.GetNextItem(&m_cookie, &key, &value) == B_OK)
	{
		keys->AddItem(key);
		values->AddItem(value);
		index++;
		if (index >= IIterator::BINDER_IPC_LIMIT || index >= count) break;
	}

	return (index == 0) ? B_END_OF_DATA : B_OK;
}

status_t BValueIterator::Remove()
{
	return B_UNSUPPORTED;
}

// ==================================================================================

BRandomIterator::BRandomIterator(const sptr<IIterator>& iter)
	: BnRandomIterator(), 
	BObserver(SContext(NULL)),
	m_iterator(iter),
	m_position(0),
	m_lock("BRandomIterator lock")
{
}

BRandomIterator::~BRandomIterator()
{
	m_iterator->AsBinder()->Unlink(static_cast<BObserver*>(this), B_UNDEFINED_VALUE, B_UNLINK_ALL_TARGETS);
}

void
BRandomIterator::InitAtom()
{
	// Link to IteratorChanged for contained iterator
	// so that we can also push it along to anyone linked to me 
	// (and so we can update our state also)
	m_iterator->LinkIterator(this->AsBinder(), SValue(BV_ITERATOR_CHANGED, BV_ITERATOR_CHANGED), B_WEAK_BINDER_LINK);
}

void
BRandomIterator::Observed(const SValue& key, const SValue& value)
{
	// If we get the BV_ITERATOR_CHANGED event, then we just push it along
	// so anyone linked with us will get notified also.
	// We also need to clear our data cache since it is now stale.
	if (key == BV_ITERATOR_CHANGED) {
		m_lock.Lock();
		m_keys.MakeEmpty();
		m_values.MakeEmpty();
		m_position = 0;
		m_lock.Unlock();
		this->PushIteratorChanged(this);
	}
}

SValue
BRandomIterator::Options() const
{
	return m_iterator->Options();
}

status_t
BRandomIterator::Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count)
{
	SNestedLocker::Autolock lock(m_lock);

	status_t result = B_END_OF_DATA;
	size_t const totalItems = Count();
	size_t index = 0;

	if ((count == 0) || (count > IIterator::BINDER_IPC_LIMIT))
		count = IIterator::BINDER_IPC_LIMIT;

	keys->MakeEmpty();
	values->MakeEmpty();

	while ((index < count) && (m_position < totalItems))
	{
		keys->AddItem(m_keys[m_position]);
		if ((flags & INode::REQUEST_DATA))
		{
			sptr<IDatum> datum = IDatum::AsInterface(m_values[m_position]);
			if (datum != NULL)
			{
				values->AddItem(datum->Value());
			}
			else
			{
				// if this wasn't a datum, then just add it
				values->AddItem(m_values[m_position]);
			}
		}
		else
		{
			values->AddItem(m_values[m_position]);
		}
		m_position++;
		index++;
		result = B_OK;
	}
	return result;
}

status_t BRandomIterator::Remove()
{
	return B_UNSUPPORTED;
}

size_t
BRandomIterator::Count() const
{
	SNestedLocker::Autolock lock(m_lock);
	size_t count = m_keys.CountItems();
	if (!count && (m_iterator != 0)) {
		// load the cache
		IIterator::ValueList keys;
		IIterator::ValueList values;
		while (m_iterator->Next(&keys, &values, 0) == B_OK) {
			m_keys.AddVector(keys);
			m_values.AddVector(values);
			keys.MakeEmpty();
			values.MakeEmpty();
		}
		count = m_keys.CountItems();
	}
	return count;
}

size_t
BRandomIterator::Position() const
{
	// atomic enough
	return m_position;
}

void
BRandomIterator::SetPosition(size_t value)
{
	SNestedLocker::Autolock lock(m_lock);
	// For properties, we silently fail if something goes wrong.  Hmmm.
	if (m_iterator != NULL) {
		if (value < Count()) {
			m_position = value;
		}
	}
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
