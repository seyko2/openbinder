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

#ifndef _SUPPORT_ASSOCIATIVEVECTOR_H
#define _SUPPORT_ASSOCIATIVEVECTOR_H

/*!	@file support/KeyedVector.h
	@ingroup CoreSupportUtilities
	@brief A templatized key/value mapping.
*/

#include <support/Vector.h>
#include <support/SortedVector.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/*--------------------------------------------------------*/
/*----- SAbstractKeyedVector class -----------------------*/

//!	Abstract type-independent implementation of SKeyedVector.
class SAbstractKeyedVector
{
public:
	inline					SAbstractKeyedVector();
	inline virtual			~SAbstractKeyedVector();

			const void*		AbstractValueAt(size_t index) const;
			const void*		AbstractKeyAt(size_t index) const;
			const void*		KeyedFor(const void* key, bool* found) const;
			const void*		FloorKeyedFor(const void* key, bool* found) const;
			const void*		CeilKeyedFor(const void* key, bool* found) const;
			void*			EditKeyedFor(const void* key, bool* found);
			ssize_t			AddKeyed(const void* key, const void* value);
			void			RemoveItemsAt(size_t index, size_t count);
			ssize_t			RemoveKeyed(const void* key);
			size_t			CountItems() const;

private:
			int32_t			_reserved_data;		// align to 8 bytes
};

/*--------------------------------------------------------*/
/*----- SKeyedVector class -------------------------------*/

//!	Templatized container of key/value pairs.
/*!	This template class defines a container object holding a set of
	key/value pairs of arbitrary types.  It is built on top of
	the SVector and SSortedVector classes (containing the value and
	key data, respectively).  As such, look-up operations in
	a keyed vector are a binary search -- an O(log(N)) operation.

	When constructing a keyed vector, you can optionally supply
	a default value (if not supplied, this will be the result of the
	default constructor for the value type).  When requesting a key
	that does not exist in the vector, the default value will be
	returned -- but ONLY for read-only operations.  Edit operations
	on an undefined key will fail.

	Note that this class is designed to not use exceptions, so
	error handling semantics -- out of memory, key not found --
	are different than container classes in the STL.
*/
template<class KEY, class VALUE>
class SKeyedVector : private SAbstractKeyedVector
{
public:
			typedef KEY	key_type;
			typedef VALUE	value_type;

public:
							SKeyedVector();
							SKeyedVector(const VALUE& undef);
							SKeyedVector(	const SSortedVector<KEY>& keys,
											const SVector<VALUE>& values,
											const VALUE& undef = VALUE());
	virtual					~SKeyedVector();
	
			//!	Copy another keyed vector in to this one.
			void			SetTo(const SKeyedVector<KEY,VALUE>& o);
	
	/* Size stats */
	
			//!	Set the total space in the keyed vector.
			void			SetCapacity(size_t total_space);
			//!	Set the amount of space in the keyed vector for new items.
			void			SetExtraCapacity(size_t extra_space);
			//!	Return the total space allocated for the vector.
			size_t			Capacity() const;
			
			//!	Return the number of actual items in the vector.
			size_t			CountItems() const;
		
	/* Value by Key */

			//!	Retrieve the value corresponding to the given key.
			const VALUE&	ValueFor(const KEY& key, bool* found = NULL) const;
			//!	Synonym for ValueFor().
	inline	const VALUE&	operator[](const KEY& key) const { return ValueFor(key); }

			//!	Perform a floor operation and return the resulting value.
			const VALUE&	FloorValueFor(const KEY& key, bool* found = NULL) const;
			//!	Like FloorValueFor(), but performs a key ceiling operation.
			const VALUE&	CeilValueFor(const KEY& key, bool* found = NULL) const;

			//!	Retrieve a value for editing.
			VALUE&			EditValueFor(const KEY& key, bool* found = NULL);
			
	/* Value/Key by index */

			//!	Return key at a specific index in the vector.
			const KEY&		KeyAt(size_t i) const;
			//!	Return value at a specific index in the vector.
			const VALUE&	ValueAt(size_t i) const;
			//!	Edit value at a specific index in the vector.
			VALUE&			EditValueAt(size_t i);
			
			//!	Return the raw SSortedVector of keys.
			const SSortedVector<KEY>&	KeyVector() const;
			//!	Return the raw SSortedVector of values.
			const SVector<VALUE>&		ValueVector() const;
			//!	Return an editable SSortedVector of values.
			SVector<VALUE>&				ValueVector();
			
	/* List manipulation */

			//!	As per SSortedVector::IndexOf().
			ssize_t			IndexOf(const KEY& key) const;
			//!	As per SSortedVector::FloorIndexOf().
			ssize_t			FloorIndexOf(const KEY& key) const;
			//!	As per SSortedVector::CeilIndexOf().
			ssize_t			CeilIndexOf(const KEY& key) const;
			//!	As per SSortedVector::GetIndexOf().
			bool			GetIndexOf(const KEY& key, size_t* index) const;
			
			//!	Add a new key/value pair to the vector.
			ssize_t			AddItem(const KEY& key, const VALUE& value);
			
			//!	Remove one or more items from the vector, starting at 'index'.
			void			RemoveItemsAt(size_t index, size_t count = 1);
			//!	Remove the item for the given key.
			ssize_t			RemoveItemFor(const KEY& key);
			
			//!	Remove all data.
			void			MakeEmpty();
			
			//!	Swap contents of this keyed vector with another.
			void			Swap(SKeyedVector<KEY,VALUE>& o);
			
private:
							SKeyedVector(const SKeyedVector<KEY,VALUE>& o);
			SKeyedVector<KEY,VALUE>&	operator=(const SKeyedVector<KEY,VALUE>& o);

			SSortedVector<KEY>	m_keys;
			SVector<VALUE>		m_values;
			VALUE				m_undefined;
};

// Type optimizations.
template<class KEY, class VALUE>
void BSwap(SKeyedVector<KEY, VALUE>& v1, SKeyedVector<KEY, VALUE>& v2);

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

SAbstractKeyedVector::SAbstractKeyedVector()
	: _reserved_data(0)
{
}

SAbstractKeyedVector::~SAbstractKeyedVector()
{
}

/*-------------------------------------------------------------*/

template<class KEY, class VALUE> inline
SKeyedVector<KEY,VALUE>::SKeyedVector()
	:	m_undefined(VALUE())
{
}

template<class KEY, class VALUE> inline
SKeyedVector<KEY,VALUE>::SKeyedVector(const VALUE& undef)
	:	m_undefined(undef)
{
}

template<class KEY, class VALUE> inline
SKeyedVector<KEY,VALUE>::SKeyedVector(const SSortedVector<KEY>& keys,
			const SVector<VALUE>& values, const VALUE& undef)
	:	m_keys(keys), m_values(values), m_undefined(undef)
{
}

template<class KEY, class VALUE> inline
SKeyedVector<KEY,VALUE>::~SKeyedVector()
{
}

/*!	This is functionally the same as an = operator.  The =
	operator is not defined for this class because it can
	be a very expensive operation -- so you must explicitly
	call SetTo(). */
template<class KEY, class VALUE> inline
void SKeyedVector<KEY,VALUE>::SetTo(const SKeyedVector<KEY,VALUE>& o)
{
	m_keys = o.m_keys; m_values = o.m_values; m_undefined = o.m_undefined;
}

/*!	This function is used to make room in the vector
	before a series of operations.  It specifies the
	total number of items you would like to have room
	for in the vector, however it will NOT decrease
	the capacity below the number of actual items
	currently in the vector. */
template<class KEY, class VALUE> inline
void SKeyedVector<KEY,VALUE>::SetCapacity(size_t total_space)
{
	m_keys.SetCapacity(total_space); m_values.SetCapacity(total_space);
}

/*!	This is like SetCapacity(), but sets the amount of
	space beyond the number of items currently in the vector. */
template<class KEY, class VALUE> inline
void SKeyedVector<KEY,VALUE>::SetExtraCapacity(size_t extra_space)
{
	m_keys.SetExtraCapacity(extra_space); m_values.SetExtraCapacity(extra_space);
}

/*!	This will be >= CountItems() -- the difference is the number
	of items in the vector that have been allocated but not
	yet used. */
template<class KEY, class VALUE> inline
size_t SKeyedVector<KEY,VALUE>::Capacity() const
{
	return m_keys.Capacity();
}

template<class KEY, class VALUE> inline
size_t SKeyedVector<KEY,VALUE>::CountItems() const
{
	return m_keys.CountItems();
}

/*!	Returns the undefined value if the requested key does not
	exist.  In this case 'found' will be false; if the key
	does exist then 'found' is true. */
template<class KEY, class VALUE> inline
const VALUE& SKeyedVector<KEY,VALUE>::ValueFor(const KEY& key, bool* found) const
{
	return *(const VALUE*)KeyedFor(&key, found);
}

/*!	This function performs a FloorIndexOf() on the underlying SSortedVector
	to find closest key <= the requested key, and returns the value
	corresponding to that key.

	For example:
	
	SVector<int32_t, char> vector;
	
	vector.AddItem(3, 'a');
	vector.AddItem(7, 'b');

	vector.FloorValueFor(1)		=> 'a'
	vector.FloorValueFor(3)		=> 'a'
	vector.FloorValueFor(5)		=> 'a'
	vector.FloorValueFor(100)	=> 'b'
*/
template<class KEY, class VALUE> inline
const VALUE& SKeyedVector<KEY,VALUE>::FloorValueFor(const KEY& key, bool* found) const
{
	return *(const VALUE*)FloorKeyedFor(&key, found);
}

template<class KEY, class VALUE> inline
const VALUE& SKeyedVector<KEY,VALUE>::CeilValueFor(const KEY& key, bool* found) const
{
	return *(const VALUE*)CeilKeyedFor(&key, found);
}

/*!	The given key must exist.  'found' will be false if it doesn't
	exist, in which case you must not access the returned value. */
template<class KEY, class VALUE> inline
VALUE& SKeyedVector<KEY,VALUE>::EditValueFor(const KEY& key, bool* found)
{
	return *(VALUE*)EditKeyedFor(&key, found);
}

template<class KEY, class VALUE> inline
const KEY& SKeyedVector<KEY,VALUE>::KeyAt(size_t i) const
{
	return m_keys.ItemAt(i);
}

template<class KEY, class VALUE> inline
const VALUE& SKeyedVector<KEY,VALUE>::ValueAt(size_t i) const
{
	return m_values.ItemAt(i);
}

template<class KEY, class VALUE> inline
VALUE& SKeyedVector<KEY,VALUE>::EditValueAt(size_t i)
{
	return m_values.EditItemAt(i);
}

template<class KEY, class VALUE> inline
const SSortedVector<KEY>& SKeyedVector<KEY,VALUE>::KeyVector() const
{
	return m_keys;
}

template<class KEY, class VALUE> inline
const SVector<VALUE>& SKeyedVector<KEY,VALUE>::ValueVector() const
{
	return m_values;
}

template<class KEY, class VALUE> inline
SVector<VALUE>& SKeyedVector<KEY,VALUE>::ValueVector()
{
	return m_values;
}

template<class KEY, class VALUE> inline
ssize_t SKeyedVector<KEY,VALUE>::IndexOf(const KEY& key) const
{
	return m_keys.IndexOf(key);
}

template<class KEY, class VALUE> inline
ssize_t SKeyedVector<KEY,VALUE>::FloorIndexOf(const KEY& key) const
{
	return m_keys.FloorIndexOf(key);
}

template<class KEY, class VALUE> inline
ssize_t SKeyedVector<KEY,VALUE>::CeilIndexOf(const KEY& key) const
{
	return m_keys.CeilIndexOf(key);
}

template<class KEY, class VALUE> inline
bool SKeyedVector<KEY,VALUE>::GetIndexOf(const KEY& key, size_t* index) const
{
	return m_keys.GetIndexOf(key, index);
}

/*!	If the given key already exists, it is replaced with the
	new value.  Returns the index of the added key, else an
	error such as B_NO_MEMORY. */
template<class KEY, class VALUE> inline
ssize_t SKeyedVector<KEY,VALUE>::AddItem(const KEY& key, const VALUE& value)
{
	return AddKeyed(&key, &value);
}

template<class KEY, class VALUE> inline
void SKeyedVector<KEY,VALUE>::RemoveItemsAt(size_t index, size_t count)
{
	SAbstractKeyedVector::RemoveItemsAt(index, count);
}

/*!	If the key exists, the index that it was removed from is
	returned.  Otherwise an error is returned. */
template<class KEY, class VALUE> inline
ssize_t SKeyedVector<KEY,VALUE>::RemoveItemFor(const KEY& key)
{
	return RemoveKeyed(&key);
}

template<class KEY, class VALUE> inline
void SKeyedVector<KEY,VALUE>::MakeEmpty()
{
	m_keys.MakeEmpty(); m_values.MakeEmpty();
}

template<class KEY, class VALUE> inline
void SKeyedVector<KEY, VALUE>::Swap(SKeyedVector<KEY, VALUE>& o)
{
	BSwap(m_keys, o.m_keys);
	BSwap(m_values, o.m_values);
	BSwap(m_undefined, o.m_undefined);
}

template<class KEY, class VALUE> inline
void BSwap(SKeyedVector<KEY, VALUE>& v1, SKeyedVector<KEY, VALUE>& v2)
{
	v1.Swap(v2);
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif
