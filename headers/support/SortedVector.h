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

#ifndef _SUPPORT_SORTEDVECTOR_H
#define _SUPPORT_SORTEDVECTOR_H

/*!	@file support/SortedVector.h
	@ingroup CoreSupportUtilities
	@brief An SAbstractVector whose types are kept in a consistent order.
*/

#include <support/Vector.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/*--------------------------------------------------------*/
/*----- SAbstractSortedVector abstract base class -------*/

//!	Abstract implementation for a vector that is sorted.
class SAbstractSortedVector : public SAbstractVector
{
public:
	inline					SAbstractSortedVector(size_t element_size);
	inline					SAbstractSortedVector(const SAbstractSortedVector& o);
							// WARNING: Your subclass must call MakeEmpty()
							// in its own destructor!
	inline virtual			~SAbstractSortedVector();
	
	inline	SAbstractSortedVector& operator=(const SAbstractSortedVector& o);
			
			// Note that we inherit the assignment operator, so you can
			// assign a plain SAbstractVector to an instance of this class.
	
			ssize_t			AddOrdered(const void* newElement, bool* added = NULL);
			
			ssize_t			OrderOf(const void* element) const;
			ssize_t			FloorOrderOf(const void* element) const;
			ssize_t			CeilOrderOf(const void* element) const;
			bool			GetOrderOf(const void* element, size_t* index) const;
			
			ssize_t			RemoveOrdered(const void* element);

	static	void			MoveBefore(SAbstractSortedVector* to, SAbstractSortedVector* from, size_t count);
	static	void			MoveAfter(SAbstractSortedVector* to, SAbstractSortedVector* from, size_t count);
			void			Swap(SAbstractSortedVector& o);
			
protected:
	virtual	int32_t			PerformCompare(const void* d1, const void* d2) const = 0;
	virtual	bool			PerformLessThan(const void* d1, const void* d2) const = 0;
	
private:
	virtual	status_t		_ReservedUntypedOrderedVector1();
	virtual	status_t		_ReservedUntypedOrderedVector2();
	virtual	status_t		_ReservedUntypedOrderedVector3();
	virtual	status_t		_ReservedUntypedOrderedVector4();
	virtual	status_t		_ReservedUntypedOrderedVector5();
	virtual	status_t		_ReservedUntypedOrderedVector6();
	
			int32_t			_reserved[2];
};

// Type optimizations.
#if !defined(_MSC_VER)
void BMoveBefore(SAbstractSortedVector* to, SAbstractSortedVector* from, size_t count);
void BMoveAfter(SAbstractSortedVector* to, SAbstractSortedVector* from, size_t count);
void BSwap(SAbstractSortedVector& v1, SAbstractSortedVector& v2);
#endif

/*--------------------------------------------------------*/
/*----- SSortedVector concrete class --------------------*/

//!	Templatized vector (array) that is kept in sorted order.
/*!	This is similar to SVector, except the values it contains
	are always sorted by their natural order.
*/
template<class TYPE>
class SSortedVector : private SAbstractSortedVector
{
public:
			typedef TYPE	value_type;

public:
							SSortedVector();
							SSortedVector(const SSortedVector<TYPE>& o);
	virtual					~SSortedVector();
	
			SSortedVector<TYPE>&	operator=(const SSortedVector<TYPE>& o);
	
	/* Size stats */
	
			void			SetCapacity(size_t total_space);
			void			SetExtraCapacity(size_t extra_space);
			size_t			Capacity() const;
			
			size_t			CountItems() const;
	
	/* Data access */

			const TYPE&		operator[](size_t i) const;
			const TYPE&		ItemAt(size_t i) const;
	
			ssize_t			IndexOf(const TYPE& item) const;
			ssize_t			FloorIndexOf(const TYPE& item) const;
			ssize_t			CeilIndexOf(const TYPE& item) const;
			bool			GetIndexOf(const TYPE& item, size_t* index) const;
			
			bool			HasItem(const TYPE& item) const;
			
			const TYPE*		Array() const;

	/* Array modification */
	
			ssize_t			AddItem(const TYPE& item, bool* added = NULL);
			
			void			MakeEmpty();
			void			RemoveItemsAt(size_t index, size_t count = 1);
	
			ssize_t			RemoveItemFor(const TYPE& item);
			
	static	void			MoveBefore(SSortedVector<TYPE>& to, SSortedVector<TYPE>& from, size_t count);
	static	void			MoveAfter(SSortedVector<TYPE>& to, SSortedVector<TYPE>& from, size_t count);
			void			Swap(SSortedVector<TYPE>& o);
			
protected:
	virtual	void			PerformConstruct(void* base, size_t count) const;
	virtual	void			PerformCopy(void* to, const void* from, size_t count) const;
	virtual	void			PerformReplicate(void* to, const void* protoElement, size_t count) const;
	virtual	void			PerformDestroy(void* base, size_t count) const;
	
	virtual	void			PerformMoveBefore(void* to, void* from, size_t count) const;
	virtual	void			PerformMoveAfter(void* to, void* from, size_t count) const;
	
	virtual	void			PerformAssign(void* to, const void* from, size_t count) const;
	
	virtual	int32_t			PerformCompare(const void* d1, const void* d2) const;
	virtual	bool			PerformLessThan(const void* d1, const void* d2) const;

	virtual SValue			PerformAsValue(const void* from, size_t count) const;
	virtual status_t		PerformSetFromValue(void* to, const SValue& value, size_t count);
};

#if !defined(_MSC_VER)
// Type optimizations.
template<class TYPE>
void BMoveBefore(SSortedVector<TYPE>* to, SSortedVector<TYPE>* from, size_t count = 1);
template<class TYPE>
void BMoveAfter(SSortedVector<TYPE>* to, SSortedVector<TYPE>* from, size_t count = 1);
template<class TYPE>
void BSwap(SSortedVector<TYPE>& v1, SSortedVector<TYPE>& v2);
#endif // _MSC_VER

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

#if !defined(_MSC_VER)

inline void BMoveBefore(SAbstractSortedVector* to, SAbstractSortedVector* from, size_t count)
{
	SAbstractSortedVector::MoveBefore(to, from, count);
}

inline void BMoveAfter(SAbstractSortedVector* to, SAbstractSortedVector* from, size_t count)
{
	SAbstractSortedVector::MoveAfter(to, from, count);
}

inline void BSwap(SAbstractSortedVector& v1, SAbstractSortedVector& v2)
{
	v1.Swap(v2);
}

#endif // _MSC_VER

/*-------------------------------------------------------------*/

SAbstractSortedVector::SAbstractSortedVector(size_t element_size)
	:	SAbstractVector(element_size)
{
}

SAbstractSortedVector::SAbstractSortedVector(const SAbstractSortedVector& o)
	:	SAbstractVector(o)
{
}

SAbstractSortedVector::~SAbstractSortedVector()
{
}

SAbstractSortedVector&	SAbstractSortedVector::operator=(const SAbstractSortedVector& o)
{
	SAbstractVector::operator=(o);
	return *this;
}

/*-------------------------------------------------------------*/

template<class TYPE> inline
SSortedVector<TYPE>::SSortedVector()
	:	SAbstractSortedVector(sizeof(TYPE[2])/2)
{
}

template<class TYPE> inline
SSortedVector<TYPE>::SSortedVector(const SSortedVector<TYPE>& o)
	:	SAbstractSortedVector(o)
{
}

template<class TYPE> inline
SSortedVector<TYPE>::~SSortedVector()
{
	MakeEmpty();
}

template<class TYPE> inline
SSortedVector<TYPE>& SSortedVector<TYPE>::operator=(const SSortedVector<TYPE>& o)
{
	SAbstractSortedVector::operator=(o);
	return *this;
}

template<class TYPE> inline
void SSortedVector<TYPE>::SetCapacity(size_t total_space)
{
	SAbstractVector::SetCapacity(total_space);
}

template<class TYPE> inline
void SSortedVector<TYPE>::SetExtraCapacity(size_t extra_space)
{
	SAbstractVector::SetExtraCapacity(extra_space);
}

template<class TYPE> inline
size_t SSortedVector<TYPE>::Capacity() const
{
	return SAbstractVector::Capacity();
}

template<class TYPE> inline
size_t SSortedVector<TYPE>::CountItems() const
{
	return SAbstractVector::CountItems();
}

template<class TYPE> inline
const TYPE& SSortedVector<TYPE>::operator[](size_t i) const
{
	// Don't use SAbstractVector::At() here, because we know
	// the item size so can do it slightly more efficiently.
	DbgOnlyFatalErrorIf(i >= CountItems() || Array() == NULL, "Vector index out of range");
	return *( static_cast<const TYPE*>(SAbstractVector::Array()) + i );
}

template<class TYPE> inline
const TYPE& SSortedVector<TYPE>::ItemAt(size_t i) const
{
	// Don't use SAbstractVector::At() here, because we know
	// the item size so can do it slightly more efficiently.
	DbgOnlyFatalErrorIf(i >= CountItems() || Array() == NULL, "Vector index out of range");
	return *( static_cast<const TYPE*>(SAbstractVector::Array()) + i );
}

template<class TYPE> inline
ssize_t SSortedVector<TYPE>::AddItem(const TYPE& item, bool* added)
{
	return AddOrdered(&item, added);
}

template<class TYPE> inline
ssize_t SSortedVector<TYPE>::IndexOf(const TYPE& item) const
{
	return OrderOf(&item);
}

template<class TYPE> inline
ssize_t SSortedVector<TYPE>::FloorIndexOf(const TYPE& item) const
{
	return FloorOrderOf(&item);
}

template<class TYPE> inline
ssize_t SSortedVector<TYPE>::CeilIndexOf(const TYPE& item) const
{
	return CeilOrderOf(&item);
}

template<class TYPE> inline
bool SSortedVector<TYPE>::GetIndexOf(const TYPE& item, size_t* index) const
{
	return GetOrderOf(&item, index);
}

template<class TYPE> inline
bool SSortedVector<TYPE>::HasItem(const TYPE& item) const
{
	size_t dummy;
	return GetOrderOf(&item, &dummy);
}

template<class TYPE> inline
const TYPE * SSortedVector<TYPE>::Array() const
{
	return static_cast<const TYPE*>(SAbstractVector::Array());
}

template<class TYPE> inline
void SSortedVector<TYPE>::MakeEmpty()
{
	SAbstractSortedVector::MakeEmpty();
}

template<class TYPE> inline
void SSortedVector<TYPE>::RemoveItemsAt(size_t index, size_t count)
{
	SAbstractSortedVector::RemoveItemsAt(index, count);
}

template<class TYPE> inline
ssize_t SSortedVector<TYPE>::RemoveItemFor(const TYPE& item)
{
	return RemoveOrdered(&item);
}

template<class TYPE> inline
void SSortedVector<TYPE>::Swap(SSortedVector<TYPE>& o)
{
	SAbstractSortedVector::Swap(o);
}

template<class TYPE>
void SSortedVector<TYPE>::PerformConstruct(void* base, size_t count) const
{
#if (__CC_ARM)
	BConstruct((TYPE*)(base), count);
#else
	BConstruct(static_cast<TYPE*>(base), count);
#endif
}

template<class TYPE>
void SSortedVector<TYPE>::PerformCopy(void* to, const void* from, size_t count) const
{
#if (__CC_ARM)
	BCopy((TYPE*)(to), (const TYPE*)(from), count);
#else
	BCopy(static_cast<TYPE*>(to), static_cast<const TYPE*>(from), count);
#endif
}

template<class TYPE>
void SSortedVector<TYPE>::PerformReplicate(void* to, const void* protoElement, size_t count) const
{
#if (__CC_ARM)
	BReplicate((TYPE*)(to), (const TYPE*)(protoElement), count);
#else
	BReplicate(static_cast<TYPE*>(to), static_cast<const TYPE*>(protoElement), count);
#endif
}

template<class TYPE>
void SSortedVector<TYPE>::PerformDestroy(void* base, size_t count) const
{
#if (__CC_ARM)
	BDestroy((TYPE*)(base), count);
#else
	BDestroy(static_cast<TYPE*>(base), count);
#endif
}

template<class TYPE>
void SSortedVector<TYPE>::PerformMoveBefore(void* to, void* from, size_t count) const
{
#if (__CC_ARM)
	BMoveBefore((TYPE*)(to), (TYPE*)(from), count);
#else
	BMoveBefore(static_cast<TYPE*>(to), static_cast<TYPE*>(from), count);
#endif
}

template<class TYPE>
void SSortedVector<TYPE>::PerformMoveAfter(void* to, void* from, size_t count) const
{
#if (__CC_ARM)
	BMoveAfter((TYPE*)(to), (TYPE*)(from), count);
#else
	BMoveAfter(static_cast<TYPE*>(to), static_cast<TYPE*>(from), count);
#endif
}

template<class TYPE>
void SSortedVector<TYPE>::PerformAssign(void* to, const void* from, size_t count) const
{
#if (__CC_ARM)
	BAssign((TYPE*)(to), (const TYPE*)(from), count);
#else
	BAssign(static_cast<TYPE*>(to), static_cast<const TYPE*>(from), count);
#endif
}

template<class TYPE>
int32_t SSortedVector<TYPE>::PerformCompare(const void* d1, const void* d2) const
{
#if (__CC_ARM)
	return BCompare(*(const TYPE*)(d1), *(const TYPE*)(d2));
#else
	return BCompare(*static_cast<const TYPE*>(d1), *static_cast<const TYPE*>(d2));
#endif
}

template<class TYPE>
bool SSortedVector<TYPE>::PerformLessThan(const void* d1, const void* d2) const
{
#if (__CC_ARM)
	return BLessThan(*(const TYPE*)(d1), *(const TYPE*)(d2));
#else
	return BLessThan(*static_cast<const TYPE*>(d1), *static_cast<const TYPE*>(d2));
#endif
}

template<class TYPE>
SValue SSortedVector<TYPE>::PerformAsValue(const void* from, size_t count) const
{
#if (__CC_ARM)
	return BArrayAsValue((const TYPE*)(from), count);
#else
	return BArrayAsValue(static_cast<const TYPE*>(from), count);
#endif
}

template<class TYPE>
status_t SSortedVector<TYPE>::PerformSetFromValue(void* to, const SValue& value, size_t count)
{
#if (__CC_ARM)
	return BArrayConstruct((TYPE*)(to), value, count);
#else
	return BArrayConstruct(static_cast<TYPE*>(to), value, count);
#endif
}

/*-------------------------------------------------------------*/

#if !defined(_MSC_VER)

template<class TYPE> inline
void BMoveBefore(SSortedVector<TYPE>* to, SSortedVector<TYPE>* from, size_t count)
{
#if (__CC_ARM)
	BMoveBefore((SAbstractSortedVector*)(to), (SAbstractSortedVector*)(from), count);
#else
	BMoveBefore(static_cast<SAbstractSortedVector*>(to), static_cast<SAbstractSortedVector*>(from), count);
#endif
}

template<class TYPE> inline
void BMoveAfter(SSortedVector<TYPE>* to, SSortedVector<TYPE>* from, size_t count)
{
#if (__CC_ARM)
	BMoveAfter((SAbstractSortedVector*)(to), (SAbstractSortedVector*)(from), count);
#else
	BMoveAfter(static_cast<SAbstractSortedVector*>(to), static_cast<SAbstractSortedVector*>(from), count);
#endif
}

template<class TYPE> inline
void BSwap(SSortedVector<TYPE>& v1, SSortedVector<TYPE>& v2)
{
	v1.Swap(v2);
}

#endif // _MSC_VER

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif
