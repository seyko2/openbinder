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

#ifndef _SUPPORT_LIST_H
#define _SUPPORT_LIST_H

/*!	@file support/List.h
	@ingroup CoreSupportUtilities
	@brief A list container class.

	Implemented as a general purpose abstract base-class
	SAbstractList, with the concrete class SList layered
	on top and templatized on the array type.
*/

#include <support/SupportDefs.h>
#include <support/TypeFuncs.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

// *************************************************************************
//! Generic type-independent implementation of a doubly-linked list.

/*!	An abstract base class for a doubly-linked list, using void pointers to
	data of a fixed size.  

	@note Not for public use; this is part of the \c SList implementation.

	@see SList
*/
class SAbstractList
{
public:
								SAbstractList(const size_t itemSize);
								SAbstractList(const SAbstractList& o);
	virtual 					~SAbstractList(void);
	
			//! Empties the list and replaces it with copied nodes from \a o .
			SAbstractList&		Duplicate(const SAbstractList& o);
	
			//! \returns Size of the node data.
			size_t				ItemSize(void) const;
			//! \returns Number of nodes in the list.
			size_t				CountItems(void) const;
			//! \returns true if list is empty.
			bool				IsEmpty(void) const;
			
struct _ListNode;
class AbstractIterator;
	
			//! Adds \a newItem before the end of the list.
			_ListNode*			Add(const void* newItem);
			//! Adds \a newItem before the list node referenced by the iterator.
			_ListNode*			AddAt(const void* newItem, const AbstractIterator& i);
			//! Adds the contents of \a subList before the end of the list.
			//! \note As a side effect, \a subList is emptied.
			_ListNode*			Splice(SAbstractList& subList);
			//! Adds the contents of \a subList before the node referenced by the iterator.
			//! \note As a side effect, \a subList is emptied.
			_ListNode*			SpliceAt(SAbstractList& subList, const AbstractIterator& i);
	
			//! Removes the node at \a i.
			AbstractIterator	Remove(AbstractIterator& i);
	
			//! Returns an iterator that references the first node.
			AbstractIterator	Begin(void) const;
			//! Returns an iterator that references the sentinel node \e after the last node.
			AbstractIterator	End(void) const;
	
			//! Removes all list nodes; equivalent to calling \c Remove() until \code Begin() == End() \endcode .
			void				MakeEmpty(void);
	
protected:
	virtual	void				PerformConstruct(void* base, size_t count) const = 0;
	virtual	void				PerformCopy(void* to, const void* from, size_t count) const = 0;
	virtual	void				PerformDestroy(void* base, size_t count) const = 0;
	virtual	void				PerformAssign(void* to, const void* from, size_t count) const = 0;
	
private:
			//! Inserts a new \c _ListNode before an in-list \c _ListNode.
			void				Insert(_ListNode* node, _ListNode* before = NULL);
			//! General insertion, in which the head and the tail of the node to
			//! be inserted may be different (insertion of a list of nodes).
			//! \c SpliceNodes will restore all double-linkages for all nodes affected
			//! (minimum 3 nodes involved; maximum 4 nodes).
			void				SpliceNodes(_ListNode* sublist_head, _ListNode* sublist_tail = NULL, _ListNode* before = NULL);
			//! Removes a single node, correcting pointers behind and ahead of it.
			void				Extract(_ListNode* node);

	virtual	status_t			_ReservedAbstractList1();
	virtual	status_t			_ReservedAbstractList2();
	virtual	status_t			_ReservedAbstractList3();
	virtual	status_t			_ReservedAbstractList4();
	virtual	status_t			_ReservedAbstractList5();
	virtual	status_t			_ReservedAbstractList6();
	virtual	status_t			_ReservedAbstractList7();
	virtual	status_t			_ReservedAbstractList8();
	virtual	status_t			_ReservedAbstractList9();
	virtual	status_t			_ReservedAbstractList10();
	
			size_t				m_itemSize;
			size_t				m_numItems;

			_ListNode*			m_end; // sentinel -- does not contain a valid list item
};

// *************************************************************************
//!	A companion class to \c SAbstractList that provides iteration over list nodes.

/*!	@note Not for public use; this is part of the \c SList implementation.

	@see SAbstractList, SList
*/
class SAbstractList::AbstractIterator 
{
public:
								AbstractIterator(SAbstractList const * domain);
								AbstractIterator(SAbstractList const * domain, _ListNode* node);
								AbstractIterator(const AbstractIterator& o);
	virtual						~AbstractIterator(void);
	
			//! Return the node at this point in the list.
			_ListNode*			operator*(void) const;

			//! Move the iterator to the next node, \b prefix \b syntax
			//! (returns the new iterator value).
			AbstractIterator&	operator++(void); // prefix
			//! Move the iterator to the next node, \b postfix \b syntax
			//! (returns the previous iterator value).
			AbstractIterator	operator++(int); // postfix
			//! Move the iterator to the previous node, \b prefix \b syntax
			//! (returns the new iterator value).
			AbstractIterator&	operator--(void); // prefix
			//! Move the iterator to the previous node, \b postfix \b syntax 
			//! (returns the previous iterator value).
			AbstractIterator	operator--(int); // postfix

			//! Assign iterators.
			AbstractIterator&	operator=(const AbstractIterator& o);

			//! Compare iterators.  Two iterators referring to the same node
			//! are defined as equivalent.
			//! @{
			bool				operator==(const AbstractIterator& o) const;
			bool				operator!=(const AbstractIterator& o) const;
			//! }@
	
			//! Return the block of data stored in the referred-to node.
			const void*			Data(void) const;
			//! Return the block of data stored in the referred-to node for editing.
			void*				EditData(void) const;
			
			//! Return a pointer to the list into which this iterator points.
			SAbstractList const * Domain() const;
	
private:
			_ListNode*			m_current;
			SAbstractList const * m_domain;
};

// *************************************************************************

template<class TYPE>
class _Iterator;

//!	A template class providing a doubly-linked list.

/*!	A concrete doubly-linked list class, parameterized by type.
	
	@see SAbstractList 
*/
template<class TYPE>
class SList : public SAbstractList
{
public:
			typedef TYPE	value_type;

public:
								SList(void);
								SList(const SList<TYPE>& o);
	virtual						~SList(void);
	
			//! Wrapper around \c Duplicate(). \sa SAbstractList
			SList<TYPE>&		operator=(const SList<TYPE>& o);
			
typedef class _Iterator<TYPE> const iterator;
typedef class _Iterator<TYPE> edit_iterator;
	
//typedef typename const _Iterator<TYPE> iterator;
//typedef typename _Iterator<TYPE> edit_iterator;
			//! Returns true if list contains at least one instance of \a item .
			typename SList::edit_iterator		IndexOf(const TYPE& item);
	
			//! Adds \a item before the end of the list.
			typename SList::edit_iterator		AddItem(const TYPE& item);
			//! Adds \a item before the list node referenced by the iterator.
			typename SList::edit_iterator		AddItemAt(const TYPE& item, const edit_iterator& i);

			//! Adds the contents of \a subList before the end of the list.
			//! \note As a side effect, \a subList is emptied.
			typename SList::edit_iterator		SpliceList(SList<TYPE> &sublist);
			//! Adds the contents of \a subList before the node referenced
			//! by the iterator.
			//! \note As a side effect, \a subList is emptied.
			typename SList::edit_iterator		SpliceListAt(SList<TYPE> &sublist, const iterator& i);
	
			//! Removes the first item matching \a item (starting from
			//! \c Begin() ) from the list.
			typename SList::edit_iterator		RemoveItemFor(const TYPE& item);
			//! Removes all items matching \a item (starting from \c Begin() )
			//! from the list.
			typename SList::edit_iterator		RemoveAllItemsFor(const TYPE& item);

			//! Removes a single item (the referent of the iterator \a i )
			//! from the list.
			typename SList::edit_iterator		RemoveItemAt(edit_iterator& i);
			
			//! Returns an iterator referencing the head of the list.
			typename SList::edit_iterator		Begin(void);
			//! Returns an iterator referencing the "end" of the list, which is 
			//! \e after the last node in the list.  (Conceptually, the iterator
			//! references a sentinel node that carries no data but terminates
			//! the list.)
			typename SList::edit_iterator		End(void);
			//! Returns a const iterator referencing the head of the list.
			typename SList::iterator			Begin(void) const;
			//! Returns a const iterator referencing the "end" of the list.
			typename SList::iterator			End(void) const;

			//! Removes all list nodes.
			void				MakeEmpty(void);

protected:
	virtual	void				PerformConstruct(void* base, size_t count) const;
	virtual	void				PerformCopy(void* to, const void* from, size_t count) const;
	virtual	void				PerformDestroy(void* base, size_t count) const;
	virtual	void				PerformAssign(void* to, const void* from, size_t count) const;
};

//!	A companion class to \c SList that provides iteration over typed list nodes.

/*!	@note Not for direct use; use \c SList<TYPE>::edit_iterator instead.

	@see SAbstractList, SList
*/
template<class TYPE>
class _Iterator : public SAbstractList::AbstractIterator
{
public:
								_Iterator(SAbstractList const * domain);
								_Iterator(SAbstractList const * domain, SAbstractList::_ListNode* node);

typedef _Iterator<TYPE> self;

			//! Access to the data stored in the list node as \c TYPE&.
			TYPE&				operator*(void);
			const TYPE&			operator*(void) const;
			//! Access to the data stored in the list node as \c TYPE* (indirected once).
			TYPE*				operator->(void);
			const TYPE*			operator->(void) const; // not super useful.

			//! Incrementors and decrementors.  \sa SAbstractList::AbstractIterator
			//! @{
			self&				operator++(void); // prefix
			self				operator++(int); // postfix
			self&				operator--(void); // prefix
			self				operator--(int); // postfix
			//! @}
};

/*!	@} */

/* No user-serviceable parts below ****************************************/

template<class TYPE>
inline _Iterator<TYPE>::_Iterator(SAbstractList const * domain)
	: SAbstractList::AbstractIterator(domain)
{
}

template<class TYPE>
inline _Iterator<TYPE>::_Iterator(SAbstractList const * domain, SAbstractList::_ListNode* node)
	: SAbstractList::AbstractIterator(domain, node)
{
}

template<class TYPE>
inline TYPE& _Iterator<TYPE>::operator*(void)
{
	return *static_cast<TYPE*>(EditData());
}

template<class TYPE>
inline const TYPE& _Iterator<TYPE>::operator*(void) const
{
	return *static_cast<TYPE*>(Data());
}

template<class TYPE>
inline TYPE* _Iterator<TYPE>::operator->(void)
{
	return static_cast<TYPE*>(EditData());
}

template<class TYPE>
inline const TYPE* _Iterator<TYPE>::operator->(void) const
{
	return static_cast<TYPE*>(Data());
}

template<class TYPE>
inline _Iterator<TYPE>& _Iterator<TYPE>::operator++(void)
{
	SAbstractList::AbstractIterator::operator++();
	
	return *this;
}

template<class TYPE>
inline _Iterator<TYPE> _Iterator<TYPE>::operator++(int o)
{
	return _Iterator<TYPE>(Domain(), *SAbstractList::AbstractIterator::operator++(o));
}

template<class TYPE>
inline _Iterator<TYPE>& _Iterator<TYPE>::operator--(void)
{
	SAbstractList::AbstractIterator::operator--();
	
	return *this;
}

template<class TYPE>
inline _Iterator<TYPE> _Iterator<TYPE>::operator--(int o)
{
	return _Iterator<TYPE>(Domain(), *SAbstractList::AbstractIterator::operator--(o));
}

template<class TYPE>
inline SList<TYPE>::SList(void)
	: SAbstractList(sizeof (TYPE[2])/2)
{
}

template<class TYPE>
inline SList<TYPE>::SList(const SList<TYPE>& o)
	: SAbstractList(o.ItemSize())
{
	Duplicate(o);
}	

template<class TYPE>
inline SList<TYPE>::~SList(void)
{
	MakeEmpty();
}

template<class TYPE>
inline SList<TYPE>& SList<TYPE>::operator=(const SList<TYPE>& o)
{
	Duplicate(o);
	
	return *this;
}

template<class TYPE>
typename SList<TYPE>::edit_iterator SList<TYPE>::IndexOf(const TYPE& item)
{
	typename SList<TYPE>::edit_iterator i = Begin();
	for (; i != End(); i++) {
		if (*i == item) return i;
	}
	
	return End();
}

template<class TYPE>
inline typename SList<TYPE>::edit_iterator SList<TYPE>::AddItem(const TYPE& item)
{
	return edit_iterator(this, Add(&item));
}

template<class TYPE>
inline typename SList<TYPE>::edit_iterator SList<TYPE>::AddItemAt(const TYPE& item,
	const edit_iterator& i)
{
	edit_iterator r = edit_iterator(this, AddAt(&item, i));
	return r;
}

template<class TYPE>
inline typename SList<TYPE>::edit_iterator 
SList<TYPE>::SpliceList(SList<TYPE> &sublist)
{
	return edit_iterator(this, Splice(sublist));
}

template<class TYPE>
inline typename SList<TYPE>::edit_iterator 
SList<TYPE>::SpliceListAt(SList<TYPE> &sublist, const iterator &i)
{
	return edit_iterator(this, SpliceAt(sublist, i));
}

template<class TYPE>
typename SList<TYPE>::edit_iterator SList<TYPE>::RemoveItemFor(const TYPE& item)
{
	typename SList<TYPE>::edit_iterator i = Begin();
	for (; i != End(); i++) {
		if (*i == item) return edit_iterator(this, *Remove(i));
	}
	
	// otherwise, return End()
	return edit_iterator(End());
}

template<class TYPE>
typename SList<TYPE>::edit_iterator SList<TYPE>::RemoveAllItemsFor(const TYPE& item)
{
	typename SList<TYPE>::edit_iterator i = Begin();
	for (; i != End();) {
		if (*i == item) i = edit_iterator(this, *Remove(i));
		else i++;
	}
	
	return i;
}

template<class TYPE>
inline typename SList<TYPE>::edit_iterator SList<TYPE>::RemoveItemAt(edit_iterator& i)
{
	return edit_iterator(this, *Remove(i));
}

template<class TYPE>
inline typename SList<TYPE>::edit_iterator SList<TYPE>::Begin(void)
{
	return edit_iterator(this, *SAbstractList::Begin());
}

template<class TYPE>
inline typename SList<TYPE>::edit_iterator SList<TYPE>::End(void)
{
	return edit_iterator(this, *SAbstractList::End());
}

template<class TYPE>
inline typename SList<TYPE>::iterator SList<TYPE>::Begin(void) const
{
	return iterator(this, *SAbstractList::Begin());
}

template<class TYPE>
inline typename SList<TYPE>::iterator SList<TYPE>::End(void) const
{
	return iterator(this, *SAbstractList::End());
}

template<class TYPE> inline
void SList<TYPE>::MakeEmpty(void)
{
	SAbstractList::MakeEmpty();
}

template<class TYPE>
void SList<TYPE>::PerformConstruct(void* base, size_t count) const
{
	BConstruct(static_cast<TYPE*>(base), count);
}

template<class TYPE>
void SList<TYPE>::PerformCopy(void* to, const void* from, size_t count) const
{
	BCopy(static_cast<TYPE*>(to), static_cast<const TYPE*>(from), count);
}

template<class TYPE>
void SList<TYPE>::PerformDestroy(void* base, size_t count) const
{
	BDestroy(static_cast<TYPE*>(base), count);
}

template<class TYPE>
void SList<TYPE>::PerformAssign(void* to, const void* from, size_t count) const
{
	BAssign(static_cast<TYPE*>(to), static_cast<const TYPE*>(from), count);
}

struct SAbstractList::_ListNode {
	_ListNode(void);
	~_ListNode(void);

	_ListNode* _next;
	_ListNode* _prev;

// This was _data[0] but the ADS compiler dose not seem to like it.
// Someone needs to verify that this has not changed the behavihor
// of List XXX--jparks
	uint8_t _data[1];
};

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif // #ifndef _SUPPORT_LIST_H
