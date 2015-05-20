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

#ifndef _STORAGE_GENERICITERABLE_H
#define _STORAGE_GENERICITERABLE_H

/*!	@file storage/GenericIterable.h
	@ingroup CoreSupportDataModel

	@brief Common base implementations of IIterable interface.
*/

#include <support/IIterable.h>
#include <support/IRandomIterator.h>
#include <support/Locker.h>
#include <support/SortedVector.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
using namespace support;
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

//! @name Iterable Options
/*!	These are standard options that can be supplied to IIterable::NewIterator().  */
//@{

//!	Specify which columns you are interested in for IIterable::NewIterator().
/*!	This contains a set of strings specifying the columns for which you
	would like data. */
B_CONST_STRING_VALUE_LARGE(BV_ITERABLE_SELECT,		"select", );

//!	Specify the database(s) to use for IIterable::NewIterator().
/*!	This is the database portion of a SQL query.  If an iterable
	support it, it specifies which of the underlying databases (possibly
	more than one) to query.  Often this is not specified, which queries
	for the data directly under the iterable -- i.e., the iterable
	itself is a database table. */
B_CONST_STRING_VALUE_LARGE(BV_ITERABLE_DATABASE,	"database", );

//!	Specify an abstract filter of the rows you are interested in for IIterable::NewIterator().
/*!	This is like BV_ITERABLE_WHERE, except it allows you to supply a single
	string that the iterable is free to interpret in whatever way makes
	the most sense for it.  It is most often used when displaying some data
	to a user, where the user enters text to be used to filter what is
	being shown.  If nothing else, an iterable will simply filter by the
	name (key) of each item.  This can be used in conjunction with
	BV_ITERABLE_WHERE, in which case you will only see the rows for which
	both expressions match. */
B_CONST_STRING_VALUE_LARGE(BV_ITERABLE_FILTER,		"filter", );

//!	Specify which rows you are interested in for IIterable::NewIterator().
/*!	This contains a set of nested SValue mappings expressing an
	expression tree, using the BV_ITERABLE_WHERE_LHS,
	BV_ITERABLE_WHERE_RHS, and BV_ITERABLE_WHERE_OP keys. */
B_CONST_STRING_VALUE_LARGE(BV_ITERABLE_WHERE,		"where", );

//!	Specify left-hand-side of BV_ITERABLE_WHERE expression.
B_CONST_STRING_VALUE_SMALL(BV_ITERABLE_WHERE_LHS,	"lhs", );
//!	Specify right-hand-side of BV_ITERABLE_WHERE expression.
B_CONST_STRING_VALUE_SMALL(BV_ITERABLE_WHERE_RHS,	"rhs", );
//!	Specify operation between left-hand-side and right-hand-side of BV_ITERABLE_WHERE expression.
B_CONST_STRING_VALUE_SMALL(BV_ITERABLE_WHERE_OP,	"op", );

//!	Specify row ordering for IIterable::NewIterator().
/*!	This is constructed as a set of <code>{index->column_name->order}</code> mappings,
	where @a order can be a combination of B_ORDER_BY_ASCENDING, 
	B_ORDER_BY_DESCENDING, B_ORDER_BY_CASELESS, and B_ORDER_BY_CASED.  The
	@a index is used to control the order of multiple columns being sorted.
	If you are only sorting by one column, you can drop that part and simply
	specify <tt>{column_name->order}</tt>. */
B_CONST_STRING_VALUE_LARGE(BV_ITERABLE_ORDER_BY,	"order_by", );

enum {
	B_ORDER_BY_ASCENDING	= 0,
	B_ORDER_BY_DESCENDING	= 1,

	B_ORDER_BY_CASELESS		= 0,
	B_ORDER_BY_CASED		= 2
};

//!	Supply a raw SQL string for IIterable::NewIterator().
/*!	This is a back-door to the underlying database.  Never use
	unless you are intimately familiar with the iterable
	implementation. */
B_CONST_STRING_VALUE_SMALL(BV_ITERABLE_SQL,			"sql", );

//@}

// ==========================================================================
// ==========================================================================

//!	Convenience class for converting SValue queries in to SQL queries.
/*!	@nosubgrouping
*/
class SSQLBuilder
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, etc. */
	//@{
								SSQLBuilder();
	virtual						~SSQLBuilder();

	//@}

	// --------------------------------------------------------------
	/*!	@name SQL APIs
		Convenience functions for building SQL strings, and virtuals
		you can override to extend or modify its behavior. */
	//@{

			//!	Convert an SValue query to a SQL query.
			status_t			BuildSQLString(	SString* outSql,
												SValue* inoutArgs,
												const SString& dbname) const;

			//!	Lower-level version of BuildSQLQuery, to extract individual strings.
			status_t			BuildSQLSubStrings(	SString* outSelect,
													SString* outWhere,
													SString* outOrderBy,
													SValue* inoutArgs) const;

			//!	Override to map column names in the query arguments to other column(s) in the SQL string.
			/*!	The default implementation simply leaves @a inoutColumn as-is.
				You implement this to change the column name supplied by the client
				to a different column name to be used in the SQL query.
				If you change @a inoutColumn to B_NULL_VALUE, it will be silently ignored.
				If you change @a inoutColumn to an SValue containing a status code that error will be returned.
				@todo Should support mappings from one SValue column to multiple SQL columns. */
	virtual	void				MapSQLColumn(SValue* inoutColumn) const;

			//!	Override to map column names in the "order by" argument to other column(s) in the SQL string.
			/*!	The input @a inoutColumnName is the column being sorted, and @a inoutOrder is how to
				sort (these correspond to {column_name->order} in BV_ITERABLE_ORDER_BY).
				The default implementation simply calls MapSQLColumn to change the name
				of @a inoutColumnName.

				You can use this for fine-grained control of how ordering happens for a column.
				To map the input columns to multiple columns in the SQL query, change
				@a inoutColumnName to a complex mapping of the same form as BV_ITERABLE_ORDER_BY.
				In that case, @a inoutOrder will be ignored.
				
				Like MapSQLColumn(), you can set @a inoutColumnName to B_NULL_VALUE or an error
				code to skip the column or return an error, respectively. */
	virtual	void				MapSQLOrderColumn(SValue* inoutColumnName, SValue* inoutOrder) const;

			//!	Override to convert a BV_ITERABLE_FILTER into the corresponding WHERE construct.
			/*!	The default implementation simply returns B_UNDEFINED_VALUE, meaning the filters
				are not supported.  You should override to return a BV_ITERABLE_WHERE SValue
				mapping to perform the appropriate query for your iterator. */
	virtual	SValue				CreateSQLFilter(const SValue& filter) const;

	//@}

private:
								SSQLBuilder(const SSQLBuilder& o);
			SSQLBuilder&			operator=(const SSQLBuilder& o);
};

// ==========================================================================
// ==========================================================================

//!	The most generic base class implementation of the IIterable interface.
/*!	This class takes care of the construction of iterators and their
	management.  Derived classes will need to create their own subclass of
	the GenericIterator class found here, and implement NewGenericIterator()
	to return new instances of that class.

	Some additional subclasses provide for specific and convenient implementations,
	such as BIndexedIterable for an iterable over an array-like structure.  Note
	that BGenericIterable by itself only provides iterators that implement the
	raw IIterator interface -- a full IRandomIterator can not be provided here.

	@nosubgrouping
*/
class BGenericIterable : public BnIterable, public SSQLBuilder
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									BGenericIterable();
									BGenericIterable(const SContext& context);
protected:
	virtual							~BGenericIterable();
public:


			//!	Lock the iterable's state.
			/*!	To keep itself consistent, this class provides a public lock that
				is used when doing read/write operations on the class state as well
				as the actual data.  The default implemention of this method acquires
				an internal lock.  You can override it to cause all internal
				implementation to acquire some other lock.  Be sure to also override
				Unlock() if doing so. */
	virtual	lock_status_t			Lock() const;

			//!	Unlock the node's state.
	virtual	void					Unlock() const;

	//@}

	// ---------------------------------------------------------------------
	/*!	@name INode Interface
		Generic implementation of the INode interface.  */
	//@{
			//!	Returns a new iterator by calling NewGenericIterator().
			/*!	Subclasses should not override this method, using
				NewGenericIterator() instead. */
	virtual	sptr<IIterator>			NewIterator(const SValue& args = B_UNDEFINED_VALUE, status_t* error = NULL);
	//@}

	// ---------------------------------------------------------------------
	/*!	@name New Generic Iterable Virtuals
		Subclasses must override NextIteratorEntryLocked() to supply the remaining
		iterable implementation, and can use the others to customize its
		behavior. */
	//@{

			class GenericIterator;

			//!	Create and return a new iterator object.
			/*!	Derived classes need to implement this to return their own
				GenericIterator subclass. */
	virtual	sptr<GenericIterator>	NewGenericIterator(const SValue& args) = 0;

	//@}

	// --------------------------------------------------------------
	/*!	@name Iterator Management
		Helpers for dealing with active iterator objects. */
	//@{
			//!	Call-back function for ForEachIteratorLocked().
	typedef	bool					(*for_each_iterator_func)(	const sptr<BGenericIterable>& iterable,
																const sptr<GenericIterator>& iterator,
																void* cookie);

			//!	This will call @a func for each iterator that currently exists.
			void					ForEachIteratorLocked(for_each_iterator_func func, void* cookie);

			//!	Push the IteratorChanged event for all active iterators.
			void					PushIteratorChangedLocked();

	//@}

private:
									BGenericIterable(const BGenericIterable& copyFrom);
									BGenericIterable& operator=(const BGenericIterable& copyFrom);

	friend class GenericIterator;

			mutable SLocker			m_lock;
			SSortedVector<wptr<GenericIterator> >
									m_iterators;
};

// --------------------------------------------------------------------------

//!	Generic base class implementation of a BGenericIterable's iterator.
/*!	You will need to create your own concete subclass.
	@nosubgrouping
*/
class BGenericIterable::GenericIterator : public BnRandomIterator, public SSQLBuilder
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
								GenericIterator(const SContext& context, const sptr<BGenericIterable>& owner);
protected:
	virtual						~GenericIterator();
			//!	Adds the iterator to its BGenericIterable.
	virtual	void				InitAtom();
			//!	The destructor must call Lock(), so return FINISH_ATOM_ASYNC to avoid deadlocks.
	virtual	status_t			FinishAtom(const void* id);
public:

			//!	Override to only return the IIterator interface.
			/*!	A GenericIterator does not have a position, so it can only support
				the IIterator interface.  If your subclass of GenericIterator wants
				to also support IRandomIterator, you should re-implement this
				function to directly call BnRandomIterator::Inspect(). */
	virtual SValue				Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);
			//!	Use to return an error back to NewIterator().  The default implementation always returns B_OK.
	virtual	status_t			StatusCheck() const;

			//!	Return the BGenericIterable that created this iterator.
	inline	const sptr<BGenericIterable>&
								Owner() const { return m_owner; }

	//@}

	// ---------------------------------------------------------------------
	/*!	@name IIterator/IRandomIterator interfaces.
		This is a generic implementation of the iterator interface.  Some of
		these have new virtuals on this class that you should override instead
		of the base virtuals here.  */
	//@{

			//!	Return the argument options that are in effect for this iterator.
			/*!	The default implementation returns B_UNDEFINED, indicating no
				additional arguments have been handled.  Derived implementations
				should always try to implement this option; if you can't, re-implement
				this function to return only the options you do implement. */
	virtual	SValue				Options() const;
			//!	Fills in the vectors by calling back to BGenericIterable::NextIteratorEntryLocked().
	virtual status_t			Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count);
			//!	Calls back to BGenericIterable::RemoveIteratorEntryLocked().
	virtual status_t			Remove();

			//!	This base class does not support IRandomIterator, so this is stubbed to return 0.
	virtual size_t				Count() const;
			//!	This base class does not support IRandomIterator, so this is stubbed to return 0.
	virtual	size_t				Position() const;
			//!	This base class does not support IRandomIterator, so this is stubbed to do nothing.
	virtual void				SetPosition(size_t p);

	//@}

	// ---------------------------------------------------------------------
	/*!	@name SSQLBuilder Redirection
		Redirect our SSQLBuilder virtuals up to the BGenericIterable class,
		so subclasses can modify the iterable's behavior from there. */
	//@{

			//!	Call back to BGenericIterable::MapSQLColumn().
			/*!	@note If you re-implement this, you will probably need to
				re-implement MapSQLOrderColumn() as well (possibly just to
				call through to MapSQLColumn(), like
				SSQLBuilder::MapSQLOrderColumn() does). */
	virtual	void				MapSQLColumn(SValue* inoutColumn) const;
			//!	Call back to BGenericIterable::MapSQLOrderColumn().
	virtual	void				MapSQLOrderColumn(SValue* inoutColumnName, SValue* inoutOrder) const;
			//!	Call back to BGenericIterable::CreateSQLFilter().
	virtual	SValue				CreateSQLFilter(const SValue& filter) const;

	//@}

	// --------------------------------------------------------------
	/*!	@name New Functionality
		Virtuals you need to override to complete the iterator implementation,
		virtuals you can override to customize it, and helper functions. */
	//@{

			//!	Process arguments passed to NewIterator().
			/*!	The default implementation does nothing.  Derived implementation
				should try to handle as many arguments as possible, returning
				those from Options(). */
	virtual	status_t			ParseArgs(const SValue& args);

			//!	Move iterator to next item and return it.  The 'flags' come directly from IIterator::Next().
	virtual	status_t			NextLocked(uint32_t flags, SValue* key, SValue* entry) = 0;
			//!	Remove the current item from the iterable.
			/*!	The default implementation returns B_UNSUPPORTED. */
	virtual	status_t			RemoveLocked();

	//@}

private:
			const sptr<BGenericIterable>
								m_owner;		// this is effectively public.
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif // _STORAGE_GENERICITERABLE_H
