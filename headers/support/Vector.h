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

#ifndef _SUPPORT_VECTOR_H
#define _SUPPORT_VECTOR_H

/*!	@file support/Vector.h
	@ingroup CoreSupportUtilities
	@brief Simple array-like container class.

	Implemented as a general purpose abstract base-class SAbstractVector,
	with the concrete class SVector layered on top and
	templatized on the array type.
*/

#include <support/SupportDefs.h>
#include <support/Value.h>
#include <support/TypeFuncs.h>
#include <support/Debug.h>
#include <support/Flattenable.h>

#include <ErrorMgr.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/*--------------------------------------------------------*/
/*----- SAbstractVector abstract base class --------------*/

//!	Abstract type-independent implementation of a vector (array).
class SAbstractVector
{
public:
							SAbstractVector(size_t element_size);
							SAbstractVector(const SAbstractVector& o);
							//! WARNING: Your subclass must call MakeEmpty() in its own destructor!
	virtual					~SAbstractVector();
	
			SAbstractVector&operator=(const SAbstractVector& o);
	
	/* Size stats */
	
			void			SetCapacity(size_t total_space);
			void			SetExtraCapacity(size_t extra_space);
			size_t			Capacity() const;
			
			size_t			ItemSize() const;
			
			size_t			CountItems() const;
			
	/* Data access */

			const void*		At(size_t index) const;
			void*			EditAt(size_t index);
			
			const void*		Array() const;
			void*			EditArray();
			
	/* Array modification */
	
			ssize_t			Add(const void* newElement);
			ssize_t			AddAt(const void* newElement, size_t index);
			status_t		SetSize(size_t total_count, const void *protoElement);
			
			ssize_t			AddVector(const SAbstractVector& o);
			ssize_t			AddVectorAt(const SAbstractVector& o, size_t index = SSIZE_MAX);
			ssize_t			AddArray(const void* array, size_t count);
			ssize_t			AddArrayAt(const void* array, size_t count, size_t index = SSIZE_MAX);

			ssize_t			ReplaceAt(const void* newItem, size_t index);

			void			MakeEmpty();
			void			RemoveItemsAt(size_t index, size_t count = 1);
			status_t		MoveItems(size_t newIndex, size_t oldIndex, size_t count = 1);
			
	static	void			MoveBefore(SAbstractVector* to, SAbstractVector* from, size_t count);
	static	void			MoveAfter(SAbstractVector* to, SAbstractVector* from, size_t count);
			void			Swap(SAbstractVector& o);
	
	/* To/From SValue */
			SValue			AsValue() const;
			status_t		SetFromValue(const SValue& value);

protected:
	virtual	void			PerformConstruct(void* base, size_t count) const = 0;
	virtual	void			PerformCopy(void* to, const void* from, size_t count) const = 0;
	virtual	void			PerformReplicate(void *to, const void* protoElement, size_t count) const = 0;
	virtual	void			PerformDestroy(void* base, size_t count) const = 0;
	
	virtual	void			PerformMoveBefore(void* to, void* from, size_t count) const = 0;
	virtual	void			PerformMoveAfter(void* to, void* from, size_t count) const = 0;
	
	virtual	void			PerformAssign(void* to, const void* from, size_t count) const = 0;

	virtual SValue			PerformAsValue(const void* from, size_t count) const = 0;
	virtual status_t		PerformSetFromValue(void* to, const SValue& value, size_t count) = 0;

private:
	
	virtual	status_t		_ReservedUntypedVector1();
	virtual	status_t		_ReservedUntypedVector2();
	virtual	status_t		_ReservedUntypedVector3();
	virtual	status_t		_ReservedUntypedVector4();
	virtual	status_t		_ReservedUntypedVector5();
	virtual	status_t		_ReservedUntypedVector6();
	virtual	status_t		_ReservedUntypedVector7();
	virtual	status_t		_ReservedUntypedVector8();
	virtual	status_t		_ReservedUntypedVector9();
	virtual	status_t		_ReservedUntypedVector10();
	
			uint8_t*		grow(size_t amount, size_t factor=3, size_t pos=0xFFFFFFFF);
			uint8_t*		shrink(size_t amount, size_t factor=4, size_t pos=0xFFFFFFFF);
			const uint8_t*	data() const;
			uint8_t*		edit_data();

			const size_t	m_elementSize;
			size_t			m_size;
			uint8_t*		m_base;
			
			const size_t	m_localSpace;
			size_t			m_avail;
			
			union {
				uint8_t*	heap;
				uint8_t		local[8];
			} m_data;
			
			int32_t			_reserved[2];
};

// Type optimizations.
#if !defined(_MSC_VER)
void BMoveBefore(SAbstractVector* to, SAbstractVector* from, size_t count);
void BMoveAfter(SAbstractVector* to, SAbstractVector* from, size_t count);
void BSwap(SAbstractVector& v1, SAbstractVector& v2);
#endif

/*--------------------------------------------------------*/
/*----- SVector concrete class ---------------------------*/

//!	Templatized vector (array) container class.
/*!	Conceptually similar to an STL vector<>, however designed
	to hide its implementation to reduce code bloat and
	enable binary compatibility.

	Editing particular items in the vector require explicitly
	calling EditItemAt(), so that we can support copy-on-write
	semantics in the future.

	@note This class is designed to not use exceptions, so
	error handling semantics -- such as out of memory --
	are different than container classes in the STL.
*/
template<class TYPE>
class SVector : private SAbstractVector
{
public:
			typedef TYPE	value_type;

public:
							SVector();
							SVector(const SVector<TYPE>& o);
	virtual					~SVector();
	
			SVector<TYPE>&	operator=(const SVector<TYPE>& o);
	
	/* Size stats */
	
			void			SetCapacity(size_t total_space);
			void			SetExtraCapacity(size_t extra_space);
			size_t			Capacity() const;
			
			size_t			CountItems() const;
	
	/* Data access */

			const TYPE&		operator[](size_t i) const;
			const TYPE&		ItemAt(size_t i) const;
			TYPE&			EditItemAt(size_t i);
	
			const TYPE*		Array() const;
			TYPE*			EditArray();
	
	/* Array modification */
	
			ssize_t			AddItem();
			ssize_t			AddItem(const TYPE& item);
#if !(__CC_ARM)
			ssize_t			AddItemAt(size_t index);
#endif
			ssize_t			AddItemAt(const TYPE& item, size_t index);
			status_t		SetSize(size_t total_count);
			status_t		SetSize(size_t total_count, const TYPE& protoElement);
			
			ssize_t			ReplaceItemAt(const TYPE& item, size_t index);
	
			ssize_t			AddVector(const SVector<TYPE>& o);
			ssize_t			AddVectorAt(const SVector<TYPE>& o, size_t index);
			ssize_t			AddArray(const TYPE *array, size_t count);
			ssize_t			AddArrayAt(const TYPE *array, size_t count, size_t index);
			
			void			MakeEmpty();
			void			RemoveItemsAt(size_t index, size_t count = 1);
			status_t		MoveItems(size_t newIndex, size_t oldIndex, size_t count = 1);
	
	static	void			MoveBefore(SVector<TYPE>& to, SVector<TYPE>& from, size_t count);
	static	void			MoveAfter(SVector<TYPE>& to, SVector<TYPE>& from, size_t count);
			void			Swap(SVector<TYPE>& o);
			
	/* Use as a stack */
	
			void			Push();
			void			Push(const TYPE& item);
			TYPE &			EditTop();
			const TYPE &	Top() const;
			void			Pop();

	/* To/From SValue */
			SValue			AsValue() const;
			status_t		SetFromValue(const SValue& value);

protected:
	virtual	void			PerformConstruct(void* base, size_t count) const;
	virtual	void			PerformCopy(void* to, const void* from, size_t count) const;
	virtual	void			PerformReplicate(void* to, const void* protoElement, size_t count) const;
	virtual	void			PerformDestroy(void* base, size_t count) const;
	
	virtual	void			PerformMoveBefore(void* to, void* from, size_t count) const;
	virtual	void			PerformMoveAfter(void* to, void* from, size_t count) const;
	
	virtual	void			PerformAssign(void* to, const void* from, size_t count) const;

	virtual SValue			PerformAsValue(const void* from, size_t count) const;
	virtual status_t		PerformSetFromValue(void* to, const SValue& value, size_t count);

	SAbstractVector&		AbstractVector() { return *this; }
};

#if !defined(_MSC_VER)
// Type optimizations.
template<class TYPE>
void BMoveBefore(SVector<TYPE>* to, SVector<TYPE>* from, size_t count = 1);
template<class TYPE>
void BMoveAfter(SVector<TYPE>* to, SVector<TYPE>* from, size_t count = 1);
template<class TYPE>
void BSwap(SVector<TYPE>& v1, SVector<TYPE>& v2);
#endif // _MSC_VER

/*-------------------------------------------------------------*/
// BArrayAsValue and BArrayConstruct 
// (called by PerformAsValue and PerformSetFromValue)
// have default implementations that just return errors.  
// These two functions have been specialized for plain old data 
// types, but to marshal an SVector of a user type, you must
// use one of the implement macros (or write your own).
// B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS
// - If you can treat your type as a stream of bits
// B_IMPLEMENT_SFLATTENABLE_FLATTEN_FUNCS
// - If your type is derived from SFlattenable
// B_IMPLEMENT_FLATTEN_FUNCS
// - If your type implements
// - SValue TYPE::AsValue() and TYPE::TYPE(const SValue&)
// These macros are defined in TypeFuncs.h
/*-------------------------------------------------------------*/

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

inline size_t SAbstractVector::ItemSize() const
{
	return m_elementSize;
}

inline size_t SAbstractVector::CountItems() const
{
	return m_size;
}

inline const void* SAbstractVector::At(size_t index) const
{
	DbgOnlyFatalErrorIf(index >= m_size || m_base == NULL, "Vector index out of range");
	return m_base + (index*m_elementSize);
}

inline const void* SAbstractVector::Array() const
{
	return m_base;
}

#if !defined(_MSC_VER)

inline void BMoveBefore(SAbstractVector* to, SAbstractVector* from, size_t count)
{
	SAbstractVector::MoveBefore(to, from, count);
}

inline void BMoveAfter(SAbstractVector* to, SAbstractVector* from, size_t count)
{
	SAbstractVector::MoveAfter(to, from, count);
}

inline void BSwap(SAbstractVector& v1, SAbstractVector& v2)
{
	v1.Swap(v2);
}

#endif // _MSC_VER

/*-------------------------------------------------------------*/

template<class TYPE> inline
SVector<TYPE>::SVector()
	:	SAbstractVector(sizeof(TYPE[2])/2)
{
}

template<class TYPE> inline
SVector<TYPE>::SVector(const SVector<TYPE>& o)
	:	SAbstractVector(o)
{
}

template<class TYPE> inline
SVector<TYPE>::~SVector()
{
	MakeEmpty();
}

template<class TYPE> inline
SVector<TYPE>& SVector<TYPE>::operator=(const SVector<TYPE>& o)
{
	SAbstractVector::operator=(o);
	return *this;
}

template<class TYPE> inline
void SVector<TYPE>::SetCapacity(size_t total_space)
{
	SAbstractVector::SetCapacity(total_space);
}

template<class TYPE> inline
void SVector<TYPE>::SetExtraCapacity(size_t extra_space)
{
	SAbstractVector::SetExtraCapacity(extra_space);
}

template<class TYPE> inline
size_t SVector<TYPE>::Capacity() const
{
	return SAbstractVector::Capacity();
}

template<class TYPE> inline
size_t SVector<TYPE>::CountItems() const
{
	return SAbstractVector::CountItems();
}

template<class TYPE> inline
const TYPE& SVector<TYPE>::operator[](size_t i) const
{
	// Don't use SAbstractVector::At() here, because we know
	// the item size so can do it slightly more efficiently.
	DbgOnlyFatalErrorIf(i >= CountItems() || Array() == NULL, "Vector index out of range");
	return *( static_cast<const TYPE*>(SAbstractVector::Array()) + i );
}

template<class TYPE> inline
const TYPE& SVector<TYPE>::ItemAt(size_t i) const
{
	// Don't use SAbstractVector::At() here, because we know
	// the item size so can do it slightly more efficiently.
	DbgOnlyFatalErrorIf(i >= CountItems() || Array() == NULL, "Vector index out of range");
	return *( static_cast<const TYPE*>(SAbstractVector::Array()) + i );
}

template<class TYPE> inline
TYPE& SVector<TYPE>::EditItemAt(size_t i)
{
	DbgOnlyFatalErrorIf(i >= CountItems() || Array() == NULL, "Vector index out of range");
	return *static_cast<TYPE*>(EditAt(i));
}

template<class TYPE> inline
const TYPE* SVector<TYPE>::Array() const
{
	return static_cast<const TYPE*>(SAbstractVector::Array());
}

template<class TYPE> inline
TYPE* SVector<TYPE>::EditArray()
{
	return static_cast<TYPE*>(SAbstractVector::EditArray());
}

template<class TYPE> inline
ssize_t SVector<TYPE>::AddItem()
{
	return Add(NULL);
}

template<class TYPE> inline
ssize_t SVector<TYPE>::AddItem(const TYPE &item)
{
	return Add(&item);
}

#if !(__CC_ARM)
template<class TYPE> inline
ssize_t SVector<TYPE>::AddItemAt(size_t index)
{
	return AddAt(NULL, index);
}
#endif

template<class TYPE> inline
ssize_t SVector<TYPE>::AddItemAt(const TYPE &item, size_t index)
{
	return AddAt(&item, index);
}

template<class TYPE> inline
status_t SVector<TYPE>::SetSize(size_t total_count)
{
	return SAbstractVector::SetSize(total_count, NULL);
}

template<class TYPE> inline
status_t SVector<TYPE>::SetSize(size_t total_count, const TYPE& protoElement)
{
	return SAbstractVector::SetSize(total_count, &protoElement);
}

template<class TYPE> inline
ssize_t SVector<TYPE>::ReplaceItemAt(const TYPE& item, size_t index)
{
	return ReplaceAt(&item, index);
}

template<class TYPE> inline
ssize_t SVector<TYPE>::AddVector(const SVector<TYPE>& o)
{
	return SAbstractVector::AddVector(o);
}

template<class TYPE> inline
ssize_t SVector<TYPE>::AddVectorAt(const SVector<TYPE>& o, size_t index)
{
	return SAbstractVector::AddVectorAt(o, index);
}

template<class TYPE> inline
ssize_t SVector<TYPE>::AddArray(const TYPE *array, size_t count)
{
	return SAbstractVector::AddArray(array, count);
}

template<class TYPE> inline
ssize_t SVector<TYPE>::AddArrayAt(const TYPE *array, size_t count, size_t index)
{
	return SAbstractVector::AddArrayAt(array, count, index);
}

template<class TYPE> inline
void SVector<TYPE>::MakeEmpty()
{
	SAbstractVector::MakeEmpty();
}

template<class TYPE> inline
void SVector<TYPE>::RemoveItemsAt(size_t index, size_t count)
{
	SAbstractVector::RemoveItemsAt(index, count);
}

template<class TYPE> inline
status_t SVector<TYPE>::MoveItems(size_t newIndex, size_t oldIndex, size_t count)
{
	return SAbstractVector::MoveItems(newIndex, oldIndex, count);
}

template<class TYPE> inline
void SVector<TYPE>::Swap(SVector<TYPE>& o)
{
	SAbstractVector::Swap(o);
}

template<class TYPE> inline
void SVector<TYPE>::Push()
{
	AddItem();
}

template<class TYPE> inline
void SVector<TYPE>::Push(const TYPE& item)
{
	AddItem(item);
}

template<class TYPE> inline
TYPE& SVector<TYPE>::EditTop()
{
	DbgOnlyFatalErrorIf(CountItems() == 0, "EditTop: Stack empty");
	return EditItemAt(CountItems()-1);
}

template<class TYPE> inline
const TYPE& SVector<TYPE>::Top() const
{
	DbgOnlyFatalErrorIf(CountItems() == 0, "Top: Stack empty");
	return ItemAt(CountItems()-1);
}

template<class TYPE> inline
void SVector<TYPE>::Pop()
{
	DbgOnlyFatalErrorIf(CountItems() == 0, "Pop: Stack empty");
	RemoveItemsAt(CountItems()-1);
}

template<class TYPE> inline
SValue SVector<TYPE>::AsValue() const
{
	return SAbstractVector::AsValue();
}

template<class TYPE> inline
status_t SVector<TYPE>::SetFromValue(const SValue& value)
{
	return SAbstractVector::SetFromValue(value);
}

template<class TYPE>
void SVector<TYPE>::PerformConstruct(void* base, size_t count) const
{
	BConstruct((TYPE*)(base), count);
}

template<class TYPE>
void SVector<TYPE>::PerformCopy(void* to, const void* from, size_t count) const
{
	BCopy((TYPE*)(to), (const TYPE*)(from), count);
}

template<class TYPE>
void SVector<TYPE>::PerformReplicate(void *to, const void* protoElement, size_t count) const
{
	BReplicate((TYPE*)(to), (const TYPE*)(protoElement), count);
}

template<class TYPE>
void SVector<TYPE>::PerformDestroy(void* base, size_t count) const
{
	BDestroy((TYPE*)(base), count);
}

template<class TYPE>
void SVector<TYPE>::PerformMoveBefore(	void* to, void* from, size_t count) const
{
	BMoveBefore((TYPE*)(to), (TYPE*)(from), count);
}

template<class TYPE>
void SVector<TYPE>::PerformMoveAfter(	void* to, void* from, size_t count) const
{
	BMoveAfter((TYPE*)(to), (TYPE*)(from), count);
}

template<class TYPE>
void SVector<TYPE>::PerformAssign(	void* to, const void* from, size_t count) const
{
	BAssign((TYPE*)(to), (const TYPE*)(from), count);
}

// -----------------------------------------------------------------
// Some higher level TypeFuncs that need to know about SValue
// -----------------------------------------------------------------

// Default implementation of BArrayAsValue and BArrayConstruct
// (called by PerformAsValue and PerformSetFromValue)
// return an error ... the user must specialize for user types

template<class TYPE>
inline SValue BArrayAsValue(const TYPE*, size_t)
{
	DbgOnlyFatalError("You need to implement your type specific BArrayAsValue");
	return SValue();
}

template<class TYPE>
inline status_t BArrayConstruct(TYPE*, const SValue&, size_t)
{
	DbgOnlyFatalError("You need to implement your type specific BArrayConstruct");
	return B_UNSUPPORTED;
}

template<class TYPE>
SValue SVector<TYPE>::PerformAsValue(const void* from, size_t count) const
{
	return BArrayAsValue((const TYPE*)(from), count);
}

template<class TYPE>
status_t SVector<TYPE>::PerformSetFromValue(void* to, const SValue& value, size_t count)
{
	return BArrayConstruct((TYPE*)(to), value, count);
}

/*-------------------------------------------------------------*/

#if !defined(_MSC_VER)

template<class TYPE> inline
void BMoveBefore(SVector<TYPE>* to, SVector<TYPE>* from, size_t count)
{
	BMoveBefore((SAbstractVector*)(to), (SAbstractVector*)(from), count);
}

template<class TYPE> inline
void BMoveAfter(SVector<TYPE>* to, SVector<TYPE>* from, size_t count)
{
	BMoveAfter((SAbstractVector*)(to), (SAbstractVector*)(from), count);
}

template<class TYPE> inline
void BSwap(SVector<TYPE>& v1, SVector<TYPE>& v2)
{
	v1.Swap(v2);
}

#endif // _MSC_VER

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif
