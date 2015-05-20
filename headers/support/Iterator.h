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

#ifndef _SUPPORT_ITERATOR_H
#define _SUPPORT_ITERATOR_H

/*!	@file support/Itertor.h
	@ingroup CoreSupportDataModel
	@brief Convenience for calling the IIterable and IIterator interfaces.
*/

#include <support/IIterable.h>
#include <support/IRandomIterator.h>
#include <support/Locker.h>
#include <support/Vector.h>
#include <support/Observer.h>

#include <stdint.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

B_CONST_STRING_VALUE_LARGE(BV_ITERATOR_CHANGED,		"IteratorChanged", );

//!	Convenience class for using an IIterator/IIterable.
/*!	This class provides a wrapper around IIterator::Next(),
	to hide the batched retrieval of results.  It also
	includes convenience methods for automatically retrieving
	an IIterator from an IIterable.

	@nosubgrouping
*/
class SIterator
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction.  Note that copying is not supported
		because you can't correctly share the same IIterator
		between two SIterators. */
	//@{

	SIterator();
	SIterator(const SContext& context, const SString& path, const SValue& args = B_UNDEFINED_VALUE);
	SIterator(const sptr<IIterable>& dir, const SValue& args = B_UNDEFINED_VALUE);
	SIterator(const sptr<IIterator>& it);
	SIterator(const sptr<INode>& node, const SValue& args = B_UNDEFINED_VALUE);
	~SIterator();
	
	status_t SetTo(const SContext& context, const SString& path, const SValue& args = B_UNDEFINED_VALUE);
	status_t SetTo(const sptr<IIterable>& dir, const SValue& args = B_UNDEFINED_VALUE);
	status_t SetTo(const sptr<IIterator>& it);

	status_t StatusCheck() const;

			//!	@deprecated Use StatusCheck() instead.
	status_t ErrorCheck() const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Iterating
		Step over and manipulate iterator. */
	//@{

	status_t Next(SValue* key, SValue* value, uint32_t flags = 0, size_t count = 0);
	status_t Remove();

	//@}

	static SValue ApplyFlags(const sptr<IIterable> &iterable, uint32_t flags);

private:
	SIterator(const SIterator& o);
	SIterator& operator=(const SIterator& o);

	SLocker m_lock;
	sptr<IIterator> m_iterator;
	IIterator::ValueList m_keys;
	IIterator::ValueList m_values;
	size_t m_index;
};

class BValueIterator : public BnIterator
{
public:
	BValueIterator(const SValue& value);

	virtual	SValue Options() const;
	virtual status_t Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count = 0);
	virtual status_t Remove();
	
protected:
	virtual ~BValueIterator();
	
private:
	SLocker m_lock;
	SValue m_value;
	void* m_cookie;
};

class BRandomIterator : public BnRandomIterator, public BObserver
{
public:
	BRandomIterator(const sptr<IIterator>& iter);

	virtual void	InitAtom();

	// IIterator
	virtual	SValue		Options() const;
	virtual status_t	Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count = 0);
	virtual status_t	Remove();

	// IRandomIterator
	virtual size_t		Count() const;
	virtual size_t		Position() const;
	virtual void		SetPosition(size_t value);

	// BObserver
	virtual void		Observed(const SValue& key, const SValue& value);

protected:
	virtual ~BRandomIterator();

private:
	// No default initialization and no copying!
	BRandomIterator();
	BRandomIterator(const BRandomIterator& o);
	BRandomIterator& operator=(const BRandomIterator& o);

	mutable SNestedLocker	m_lock;
	const sptr<IIterator>	m_iterator;
	size_t					m_position;
	mutable IIterator::ValueList m_keys;
	mutable IIterator::ValueList m_values;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif // _SUPPORT_ITERATOR_H
