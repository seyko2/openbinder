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

#include <support/Debug.h>
#include <support/List.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/* SAbstractList implementation ***********************************************/

SAbstractList::SAbstractList(const size_t itemSize)
	: m_itemSize(itemSize), m_numItems(0), m_end(new _ListNode)
{
}

SAbstractList::SAbstractList(const SAbstractList& o)
	: m_itemSize(o.ItemSize())
{
	Duplicate(o);
}

SAbstractList::~SAbstractList(void)
{
	if (m_numItems > 0) {
		char complaint[255];
		sprintf(complaint, "SAbstractList: subclass must call MakeEmpty() in destructor (%d items remain)", m_numItems);
		ErrFatalError(complaint);;
	}
	
	delete m_end;
}

SAbstractList& SAbstractList::Duplicate(const SAbstractList& o)
{
	MakeEmpty();
	
	AbstractIterator i = o.Begin();
	for ( ; i != o.End(); i++) {
		Add(i.Data());
	}

	return *this;
}

size_t SAbstractList::ItemSize(void) const
{
	return m_itemSize;
}

size_t SAbstractList::CountItems(void) const
{
	return m_numItems;
}

bool SAbstractList::IsEmpty(void) const
{
	return m_numItems == 0;
}

SAbstractList::_ListNode* SAbstractList::Add(const void* newItem)
{
	AbstractIterator end(this, m_end);

	return AddAt(newItem, end);
}

SAbstractList::_ListNode* SAbstractList::AddAt(const void* newItem, const AbstractIterator& i)
{
	ErrFatalErrorIf(i.Domain() != this, "SAbstractList: AddAt() called on an iterator from another list");

	// allocate storage for a _ListNode + an item
	_ListNode* newNode = static_cast<_ListNode*> (malloc(sizeof (_ListNode)
		+ m_itemSize));
	// construct a _ListNode at the head of the storage
	new(newNode) _ListNode;

	if (newItem) {
		// copy newItem's bytes to newNode
		PerformCopy(&newNode->_data, newItem, 1);
	} else {
		// create a new item
		PerformConstruct(&newNode->_data, 1);
	}
	
	// hook newNode up to the list
	Insert(newNode, *i);
	
	m_numItems++;
	
	return newNode;
}

SAbstractList::_ListNode *
SAbstractList::Splice(SAbstractList &subList)
{
	return SpliceAt(subList, AbstractIterator(this, m_end));
}

SAbstractList::_ListNode *
SAbstractList::SpliceAt(SAbstractList &subList, const AbstractIterator &i)
{
	ErrFatalErrorIf(i.Domain() != this, "SAbstractList: Splice() called with an iterator from another list");

	SAbstractList::AbstractIterator subListLast = subList.End();
	SAbstractList::AbstractIterator subListFirst = subList.Begin();

	if (subListLast == subListFirst) { // == subList.End()
		// splice list is already empty
		return *i;
	}
	subListLast--;
	
	SpliceNodes(*(subListFirst), *(subListLast), *i);
	
	m_numItems += subList.m_numItems;
	
	// truncate subList
	subList.m_end->_next = subList.m_end; 
	subList.m_end->_prev = subList.m_end; 
	
	subList.m_numItems = 0;
	
	return *subListFirst;
}


SAbstractList::AbstractIterator SAbstractList::Remove(AbstractIterator& i)
{
	ErrFatalErrorIf(i.Domain() != this, "SAbstractList: Remove() called with an iterator from another list");

	//disallow removal of end node
	if (i == End()) return i;

	_ListNode* killMe = *i;

	// call the stored object's destructor
	PerformDestroy(&killMe->_data, 1);
	
	// prepare to return iterator pointing to next node
	AbstractIterator next(this, killMe->_next);
	
	// unhook node from the list
	Extract(killMe);
	
	// call the _ListNode's destructor
	killMe->~_ListNode();

	// free the node's storage
	free(killMe);
	
	m_numItems--;
	
	return next;
}	

SAbstractList::AbstractIterator SAbstractList::Begin(void) const
{
	return AbstractIterator(this, m_end->_next);
}

SAbstractList::AbstractIterator SAbstractList::End(void) const
{
	return AbstractIterator(this, m_end);
}

void SAbstractList::MakeEmpty(void)
{
	AbstractIterator i = Begin();
	
	while (i != End()) {
		i = Remove(i);
	}

	if(m_numItems != 0) {
		char complaint[255];
		sprintf(complaint, "SAbstractList::MakeEmpty: internal constraint violated; removed all nodes, but numItems is still %d", m_numItems);
		ErrFatalError(complaint);
	}
}

// hook node up to list
void SAbstractList::Insert(_ListNode* node, _ListNode* before)
{
	SpliceNodes(node, NULL, before);
}

// hook many nodes up to list
void 
SAbstractList::SpliceNodes(_ListNode *sublist_head, _ListNode *sublist_tail, _ListNode *before)
{
	if (before == NULL) before = m_end;
	if (sublist_tail == NULL) sublist_tail = sublist_head;
	
	// place sublist_head before "before"
	(before->_prev)->_next = sublist_head;
	sublist_head->_prev = (before->_prev);
	
	before->_prev = sublist_tail;
	sublist_tail->_next = before;
}

// unhook node from list
void SAbstractList::Extract(_ListNode* node)
{
	node->_prev->_next = node->_next;
	node->_next->_prev = node->_prev;
}


status_t SAbstractList::_ReservedAbstractList1() { return B_UNSUPPORTED; }
status_t SAbstractList::_ReservedAbstractList2() { return B_UNSUPPORTED; }
status_t SAbstractList::_ReservedAbstractList3() { return B_UNSUPPORTED; }
status_t SAbstractList::_ReservedAbstractList4() { return B_UNSUPPORTED; }
status_t SAbstractList::_ReservedAbstractList5() { return B_UNSUPPORTED; }
status_t SAbstractList::_ReservedAbstractList6() { return B_UNSUPPORTED; }
status_t SAbstractList::_ReservedAbstractList7() { return B_UNSUPPORTED; }
status_t SAbstractList::_ReservedAbstractList8() { return B_UNSUPPORTED; }
status_t SAbstractList::_ReservedAbstractList9() { return B_UNSUPPORTED; }
status_t SAbstractList::_ReservedAbstractList10() { return B_UNSUPPORTED; }

/* SAbstractList::AbstractIterator implementation *************************************/

SAbstractList::AbstractIterator::AbstractIterator(SAbstractList const * domain)
	: m_current(NULL),
	  m_domain(domain)
{
}

SAbstractList::AbstractIterator::AbstractIterator(SAbstractList const * domain, _ListNode* node)
	: m_current(node),
	  m_domain(domain)
{
}

SAbstractList::AbstractIterator::AbstractIterator(const AbstractIterator& o)
	: m_current(o.m_current),
	  m_domain(o.m_domain)
{
}

SAbstractList::AbstractIterator::~AbstractIterator(void)
{
}

SAbstractList::_ListNode* SAbstractList::AbstractIterator::operator*(void) const
{
	return m_current;
}

SAbstractList::AbstractIterator& SAbstractList::AbstractIterator::operator++(void)
{
	m_current = m_current->_next;
	
	return *this;
}

SAbstractList::AbstractIterator SAbstractList::AbstractIterator::operator++(int)
{
	AbstractIterator tmp = *this;
	
	++*this;
	
	return tmp;
}

SAbstractList::AbstractIterator& SAbstractList::AbstractIterator::operator--(void)
{
	m_current = m_current->_prev;
	
	return *this;
}

SAbstractList::AbstractIterator SAbstractList::AbstractIterator::operator--(int)
{
	AbstractIterator tmp = *this;
	
	--*this;
	
	return tmp;
}

SAbstractList::AbstractIterator& SAbstractList::AbstractIterator::operator=(const AbstractIterator& o)
{
	m_current = o.m_current;
	m_domain = o.m_domain;
	
	return *this;
}

bool SAbstractList::AbstractIterator::operator==(const AbstractIterator& o) const
{
	// it should be impossible to have equal node pointers from different list
	// domains, so this test will be sufficient.
	return m_current == o.m_current; 
}

bool SAbstractList::AbstractIterator::operator!=(const AbstractIterator& o) const
{
	return m_current != o.m_current;
}

const void* SAbstractList::AbstractIterator::Data(void) const
{
	return &m_current->_data;
}

void* SAbstractList::AbstractIterator::EditData(void) const
{
	return &m_current->_data;
}

SAbstractList const *
SAbstractList::AbstractIterator::Domain() const
{
	return m_domain;
}


/* SAbstractList::_ListNode implementation ************************************/

SAbstractList::_ListNode::_ListNode(void)
	: _next(this), _prev(this)
{
}

SAbstractList::_ListNode::~_ListNode(void)
{
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
