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

#ifndef _STORAGE_CATALOGDELEGATE_H
#define _STORAGE_CATALOGDELEGATE_H

/*!	@file storage/CatalogDelegate.h
	@ingroup CoreSupportDataModel
	@brief Delegation class for wrapping an existing
		INode/IIterable/ICatalog implementation.
*/

#include <storage/GenericIterable.h>
#include <storage/NodeDelegate.h>

#include <support/ICatalog.h>
#include <support/Locker.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

//!	Provides base implementation of delegations for INode/IIterable/ICatalog.
/*!	Given an existing object that implements one or more of the INode,
	IIterable, and ICatalog interfaces, provides a new object that delegates
	to the existing implementation.  This gives you a base class from which
	you can implement modifications to the data supplied by existing data
	objects.

	@note This class only delegates the given @a base object.  It will not
	create a wrapper for any objects retrieved from it, such as IDatums
	or other INodes.

	@nosubgrouping
*/
class BCatalogDelegate : public BNodeDelegate, public BGenericIterable, public BnCatalog
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									BCatalogDelegate(	const SContext& context,
														const sptr<IBinder>& base);
protected:
	virtual							~BCatalogDelegate();
public:

			//!	Disambiguate.
	virtual	lock_status_t			Lock() const;
			//!	Disambiguate.
	virtual	void					Unlock() const;
			//!	Disambiguate.
	inline	SContext				Context() { return BnNode::Context(); }
			//!	Make INode, IIterable, and ICatalog interfaces visible based on delegate.
			/*!	This will allow you to cast to the same interfaces that are supported by
				the base object, but returning the new delegate implementations. */
	virtual	SValue					Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);

	//@}

	// --------------------------------------------------------------
	/*!	@name IIterable
		Delegation to IIterable interface. */
	//@{

			class IteratorDelegate;

			//!	Returns original IIterable object, or NULL if that interface was not available.
	inline	sptr<IIterable>			BaseIterable() const { return m_baseIterable; }

	virtual	sptr<GenericIterator>	NewGenericIterator(const SValue& args);

	//@}

	// --------------------------------------------------------------
	/*!	@name ICatalog
		Delegation to ICatalog interface. */
	//@{

			//!	Returns original ICatalog object, or NULL if that interface was not available.
	inline	sptr<ICatalog>			BaseCatalog() const { return m_baseCatalog; }

	virtual	status_t				AddEntry(const SString& name, const SValue& entry);
	virtual	status_t				RemoveEntry(const SString& name);
	virtual	status_t				RenameEntry(const SString& entry, const SString& name);
	
	virtual	sptr<INode>				CreateNode(SString* name, status_t* err);
	virtual	sptr<IDatum>			CreateDatum(SString* name, uint32_t flags, status_t* err);

	//@}

private:
									BCatalogDelegate(const BCatalogDelegate&);
			BCatalogDelegate&		operator=(const BCatalogDelegate&);

			const sptr<IIterable>	m_baseIterable;
			const sptr<ICatalog>	m_baseCatalog;
};

// --------------------------------------------------------------------------

//!	Generic base class implementation of a BGenericIterable's iterator.
/*!	You will need to create your own concete subclass.
	@nosubgrouping
*/
class BCatalogDelegate::IteratorDelegate : public BGenericIterable::GenericIterator
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
								IteratorDelegate(	const SContext& context,
													const sptr<BGenericIterable>& owner);
protected:
	virtual						~IteratorDelegate();
public:

			//!	Return IIterator, and IRandomIterator if the base supports it.
	virtual SValue				Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);
			//!	Propagate error code back from base NewIterator() call.
	virtual	status_t			StatusCheck() const;

	//@}

			//!	Return options from base iterator.
	virtual	SValue				Options() const;
			//!	Call through to pass iterator.
			/*!	To modify the results, you can override this, call through
				to this implementation, and the munge the data in the
				@a keys and/or @a values vectors.  If you want more control
				over the results being generated, you can reimplement this to
				call to the normal GenericIterator::Next() implementation and
				generate results in NextLock(). */
	virtual status_t			Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count);
			//!	Calls through to base iterator.
	virtual status_t			Remove();

			//!	Calls through to base iterator.
	virtual size_t				Count() const;
			//!	Calls through to base iterator.
	virtual	size_t				Position() const;
			//!	Calls through to base iterator.
	virtual void				SetPosition(size_t p);

			//!	Create the delegate iterator with the given arguments.
	virtual	status_t			ParseArgs(const SValue& args);

			//!	Stubbed out because Next() just calls to base iterator.
	virtual	status_t			NextLocked(uint32_t flags, SValue* key, SValue* entry);

private:
			status_t				m_baseError;
			sptr<IIterator>			m_baseIterator;
			sptr<IRandomIterator>	m_baseRandomIterator;
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif // _STORAGE_CATALOGDELEGATE_H

