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

#include <storage/IndexedIterable.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

// *********************************************************************************
// ** BIndexedIterable *************************************************************
// *********************************************************************************

BIndexedIterable::BIndexedIterable()
{
}

BIndexedIterable::BIndexedIterable(const SContext& context)
	: BGenericIterable(context)
{
}

BIndexedIterable::~BIndexedIterable()
{
}

sptr<BGenericIterable::GenericIterator> BIndexedIterable::NewGenericIterator(const SValue& args)
{
	return new IndexedIterator(Context(), this);
}

status_t BIndexedIterable::RemoveEntryAtLocked(size_t index)
{
	return B_UNSUPPORTED;
}

void BIndexedIterable::CreateSelectionLocked(SValue* inoutSelection, SValue* outCookie) const
{
	inoutSelection->Undefine();
}

void BIndexedIterable::CreateSortOrderLocked(SValue* inoutSortBy, SVector<ssize_t>* outOrder) const
{
	inoutSortBy->Undefine();
	outOrder->MakeEmpty();
}

struct update_indices_info
{
	size_t index;
	ssize_t delta;
};

bool BIndexedIterable::update_indices_func(	const sptr<BGenericIterable>& iterable,
	const sptr<GenericIterator>& iterator, void* cookie)
{
	update_indices_info* info = reinterpret_cast<update_indices_info*>(cookie);
	static_cast<IndexedIterator*>(iterator.ptr())->update_indices_l(info->index, info->delta);
	return true;
}

void BIndexedIterable::UpdateIteratorIndicesLocked(size_t index, ssize_t delta)
{
	update_indices_info info;
	info.index = index;
	info.delta = delta;
	ForEachIteratorLocked(update_indices_func, &info);
}

// =================================================================================
// =================================================================================
// =================================================================================

BIndexedIterable::IndexedIterator::IndexedIterator(const SContext& context, const sptr<BGenericIterable>& owner)
	: GenericIterator(context, owner)
	, m_position(0)
{
}

BIndexedIterable::IndexedIterator::~IndexedIterator()
{
}

SValue BIndexedIterable::IndexedIterator::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	return BnRandomIterator::Inspect(caller, which, flags);
}

SValue BIndexedIterable::IndexedIterator::Options() const
{
	SAutolock _l(Owner()->Lock());
	SValue opts(BV_ITERABLE_SELECT, m_selectArgs);
	opts.JoinItem(BV_ITERABLE_ORDER_BY, m_orderArgs);
	return opts;
}

size_t BIndexedIterable::IndexedIterator::Count() const
{
	return m_count;
}

size_t BIndexedIterable::IndexedIterator::Position() const
{
	return m_position;
}

void BIndexedIterable::IndexedIterator::SetPosition(size_t p)
{
	SAutolock _l(Owner()->Lock());
	m_position = p < m_count ? p : m_count;
}

status_t BIndexedIterable::IndexedIterator::ParseArgs(const SValue& args)
{
	{
		SAutolock _l(Owner()->Lock());
		m_selectArgs = args[BV_ITERABLE_SELECT];
		static_cast<BIndexedIterable*>(Owner().ptr())->CreateSelectionLocked(&m_selectArgs, &m_selectCookie);
		m_orderArgs = args[BV_ITERABLE_ORDER_BY];
		static_cast<BIndexedIterable*>(Owner().ptr())->CreateSortOrderLocked(&m_orderArgs, &m_order);
		m_count = static_cast<BIndexedIterable*>(Owner().ptr())->CountEntriesLocked();
	}
	return GenericIterator::ParseArgs(args);
}

status_t BIndexedIterable::IndexedIterator::NextLocked(uint32_t flags, SValue* key, SValue* entry)
{
	const ssize_t index = MoveIndexLocked();
	if (index >= 0) {
		return static_cast<BIndexedIterable*>(Owner().ptr())->EntryAtLocked(this, index, flags, key, entry);
	}

	// Can't return this item for some reason...  fill in dummy values.
	*key = B_NULL_VALUE; *entry = B_NULL_VALUE;

	// B_ENTRY_NOT_FOUND means that this item in the iterator has been
	// deleted from the iterable, which is not really an error from the
	// client's perspective so don't return an error code for that.
	return index == B_ENTRY_NOT_FOUND ? B_OK : (status_t)index;
}

status_t BIndexedIterable::IndexedIterator::RemoveLocked()
{
	const ssize_t index = CurrentIndexLocked();
	if (index >= 0) {
		return static_cast<BIndexedIterable*>(Owner().ptr())->RemoveEntryAtLocked(index);
	}
	return (status_t)index;
}

ssize_t BIndexedIterable::IndexedIterator::CurrentIndexLocked() const
{
	const size_t cur = m_position;
	if (cur < m_count) {
		if (m_order.CountItems() == 0) return cur;
		return m_order[cur];
	}
	return B_END_OF_DATA;
}

ssize_t BIndexedIterable::IndexedIterator::MoveIndexLocked()
{
	const size_t cur = m_position;
	const size_t N = m_count;
	if (cur < N) {
		m_position = cur+1;
		if (m_order.CountItems() == 0) {
			return cur;
		}
		return m_order[cur];
	}
	return B_END_OF_DATA;
}

void BIndexedIterable::IndexedIterator::update_indices_l(size_t index, ssize_t delta)
{
	const size_t N = m_count;

	if (m_order.CountItems() == 0) {
		// If we don't currently have a permutation vector, it is
		// time to make one: we don't want the change in items to
		// show up in this iterator, so we no longer can have a
		// 1:1 mapping between iterator position and iterable index.
		// This is the easy case: the iterator position is a direct
		// mapping to the contents of the iterable, so we just need
		// to adjust the position as specified by the change.
		m_order.SetSize(N);
		ssize_t* a = m_order.EditArray();
		if (a) {
			for (size_t i=0; i<N; i++) a[i] = i;
		}

#if 0
		// This code would just directly update the position... for
		// what it's worth.
		if (m_position < index) {
			// Position is before the change, nothing to do.
			return;
		}

		if (delta > 0 || (m_position-delta) >= index) {
			// Position is after the change, move it by the
			// changed amount.
			m_position += delta;
			return;
		}

		// The position is one of the deleted items.  In this case,
		// just push the position back to the beginning of the delete.
		m_position = index;
		return;
#endif
	}

	// Update the indices of our permutation vector to still point
	// to the same items.
	ssize_t* a = m_order.EditArray();
	if (a) {
		if (delta > 0) {
			// Adding new items.
			for (size_t i=0; i<N; i++, a++) {
				if (*a >= (ssize_t)index) (*a) += delta;
			}
		} else {
			// Removing old items.
			const size_t end=index-delta;
			for (size_t i=0; i<N; i++, a++) {
				if (*a >= (ssize_t)index) {
					if (*a >= (ssize_t)end) (*a) += delta;
					else (*a) = B_ENTRY_NOT_FOUND;
				}
			}
		}
	}
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
