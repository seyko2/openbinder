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

#include <support/SortedVector.h>
#include <support/KeyedVector.h>
#include <support/Value.h>

#include <support/Debug.h>

#include <stdlib.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif


ssize_t SAbstractSortedVector::AddOrdered(const void* newElement, bool* added)
{
	size_t pos;
	if (!GetOrderOf(newElement, &pos)) {
		pos = AddAt(newElement, pos);
		if (added) *added = (static_cast<ssize_t>(pos) >= B_OK ? true : false);
	} else {
		void* elem = EditAt(pos);
		if (elem)
			PerformAssign(elem, newElement, 1);
		if (added) *added = false;
	}
	return static_cast<ssize_t>(pos);
}

ssize_t SAbstractSortedVector::OrderOf(const void* newElement) const
{
	size_t pos;
	return GetOrderOf(newElement, &pos) ? pos : B_NAME_NOT_FOUND;
}

ssize_t SAbstractSortedVector::FloorOrderOf(const void* newElement) const
{
	size_t pos;
	if (GetOrderOf(newElement, &pos)) return pos;
	return pos > 0 ? (pos-1) : (CountItems() > 0 ? 0 : B_NAME_NOT_FOUND);
}

ssize_t SAbstractSortedVector::CeilOrderOf(const void* newElement) const
{
	size_t pos;
	if (GetOrderOf(newElement, &pos)) return pos;
	const size_t N = CountItems();
	return pos < N ? pos : (N > 0 ? (N-1) : B_NAME_NOT_FOUND);
}

ssize_t SAbstractSortedVector::RemoveOrdered(const void* element)
{
	size_t pos;
	if (GetOrderOf(element, &pos)) {
		RemoveItemsAt(pos, 1);
		return pos;
	}
	return B_NAME_NOT_FOUND;
}

bool SAbstractSortedVector::GetOrderOf(const void* element, size_t* pos) const
{
	ssize_t mid, low = 0, high = CountItems()-1;
	while (low <= high) {
		mid = (low + high)/2;
		const int32_t cmp = PerformCompare(element, At(mid));
		if (cmp < 0) {
			high = mid-1;
		} else if (cmp > 0) {
			low = mid+1;
		} else {
			*pos = mid;
			return true;
		}
	}
	
	*pos = low;
	return false;
}

void SAbstractSortedVector::MoveBefore(SAbstractSortedVector* to, SAbstractSortedVector* from, size_t count)
{
	while (count > 0) {
		SAbstractVector::MoveBefore(to, from, 1);
		count--;
		to++;
		from++;
	}
}

void SAbstractSortedVector::MoveAfter(SAbstractSortedVector* to, SAbstractSortedVector* from, size_t count)
{
	to += (count-1);
	from += (count-1);
	while (count > 0) {
		SAbstractVector::MoveBefore(to, from, 1);
		count--;
		to--;
		from--;
	}
}

void SAbstractSortedVector::Swap(SAbstractSortedVector& o)
{
	SAbstractVector::Swap(*(SAbstractVector*)&o);
}

status_t SAbstractSortedVector::_ReservedUntypedOrderedVector1() { return B_UNSUPPORTED; }
status_t SAbstractSortedVector::_ReservedUntypedOrderedVector2() { return B_UNSUPPORTED; }
status_t SAbstractSortedVector::_ReservedUntypedOrderedVector3() { return B_UNSUPPORTED; }
status_t SAbstractSortedVector::_ReservedUntypedOrderedVector4() { return B_UNSUPPORTED; }
status_t SAbstractSortedVector::_ReservedUntypedOrderedVector5() { return B_UNSUPPORTED; }
status_t SAbstractSortedVector::_ReservedUntypedOrderedVector6() { return B_UNSUPPORTED; }

// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------

// This is what the SKeyedVector template class looks like
// when it is instantiated.

class KeyedVector : public SAbstractKeyedVector
{
public:
	SSortedVector<int32_t>	m_keys;
	SVector<int32_t>		m_values;
	int32_t					m_undefined;
};

#define KEYS ((SAbstractSortedVector*)(void*)&((KeyedVector*)this)->m_keys)
#define VALUES ((SAbstractVector*)(void*)&((KeyedVector*)this)->m_values)
#define UNDEFINED (&((KeyedVector*)this)->m_undefined)

size_t SAbstractKeyedVector::CountItems() const
{
	return KEYS->CountItems();
}

const void* SAbstractKeyedVector::AbstractValueAt(size_t index) const
{
	return VALUES->At(index);
}

const void* SAbstractKeyedVector::AbstractKeyAt(size_t index) const
{
	return KEYS->At(index);
}

const void* SAbstractKeyedVector::KeyedFor(const void* key, bool* found) const
{
	ssize_t pos = KEYS->OrderOf(key);
	if (pos >= (ssize_t)(VALUES->CountItems())) pos = B_ERROR;
	if (pos >= B_OK) {
		if (found) *found = true;
		return VALUES->At(pos);
	}

	if (found) *found = false;
	return UNDEFINED;
}

const void* SAbstractKeyedVector::FloorKeyedFor(const void* key, bool* found) const
{
	ssize_t pos = KEYS->FloorOrderOf(key);
	if (pos >= (ssize_t)(VALUES->CountItems())) pos = B_ERROR;
	if (pos >= B_OK) {
		if (found) *found = true;
		return VALUES->At(pos);
	}

	if (found) *found = false;
	return UNDEFINED;
}

const void* SAbstractKeyedVector::CeilKeyedFor(const void* key, bool* found) const
{
	ssize_t pos = KEYS->CeilOrderOf(key);
	if (pos >= (ssize_t)(VALUES->CountItems())) pos = B_ERROR;
	if (pos >= B_OK) {
		if (found) *found = true;
		return VALUES->At(pos);
	}

	if (found) *found = false;
	return UNDEFINED;
}

void* SAbstractKeyedVector::EditKeyedFor(const void* key, bool* found)
{
	ssize_t pos = KEYS->OrderOf(key);
	if (pos >= (ssize_t)(VALUES->CountItems())) pos = B_ERROR;
	if (pos >= B_OK) {
		if (found) *found = true;
		return VALUES->EditAt(pos);
	}

	if (found) *found = false;
	return UNDEFINED;
}

ssize_t SAbstractKeyedVector::AddKeyed(const void* key, const void* value)
{
	bool added;
	const ssize_t pos = KEYS->AddOrdered(key, &added);
	if (added) {
		const ssize_t vpos = VALUES->AddAt(value, pos);
		if (vpos < B_OK) KEYS->RemoveItemsAt(pos, 1);
		return vpos;
	}

	if (pos >= B_OK) VALUES->ReplaceAt(value, pos);

	return pos;
}

void SAbstractKeyedVector::RemoveItemsAt(size_t index, size_t count)
{
	KEYS->RemoveItemsAt(index, count);
	VALUES->RemoveItemsAt(index, count);
}

ssize_t SAbstractKeyedVector::RemoveKeyed(const void* key)
{
	const ssize_t pos = KEYS->RemoveOrdered(key);
	if (pos >= B_OK) VALUES->RemoveItemsAt(pos, 1);
	return pos;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
