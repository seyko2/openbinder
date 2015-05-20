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

#include <storage/GenericIterable.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

// *********************************************************************************
// ** LOCAL DEFINITIONS ************************************************************
// *********************************************************************************

// *********************************************************************************
// ** SSQLBuilder ******************************************************************
// *********************************************************************************

static status_t add_order_value(SString* query, bool* started, SValue* newArgs, const SSQLBuilder& builder, const SValue& order);

static status_t add_order_item(SString* query, bool* started, SValue* newArgs, const SSQLBuilder& builder, const SValue& key, const SValue& value)
{
	SValue columnValue(key), orderValue(value);
	builder.MapSQLOrderColumn(&columnValue, &orderValue);
	if (columnValue == B_NULL_VALUE) return B_OK;
	if (columnValue.StatusCheck() != B_OK) return columnValue.StatusCheck();

	if (!columnValue.IsSimple()) {
		// Inserting multiple order columns at this point!
		status_t err = add_order_value(query, started, NULL, builder, columnValue);
		if (err != B_OK) return err;

	} else {
		const SString column = columnValue.AsString();
		const int32_t order = orderValue.AsInt32();
		if (column == "") return B_ERROR;

		if (!*started) {
			query->Append("ORDER BY ");
			*started = true;
		} else {
			query->Append(", ");
		}
		query->Append(column);
		query->Append((order&B_ORDER_BY_DESCENDING) ? " DESC" : " ASC");
		query->Append((order&B_ORDER_BY_CASED) ? " CASED" : " CASELESS");
	}

	if (newArgs) newArgs->JoinItem(key, SSimpleValue<int32_t>(value.AsInt32()&3));
	return B_OK;
}

static status_t add_order_value(SString* query, bool* started, SValue* newArgs, const SSQLBuilder& builder, const SValue& order)
{
	SValue k, v, orderArgs;
	void* cookie = NULL;
	while (order.GetNextItem(&cookie, &k, &v) == B_OK) {
		if (k.Type() == B_INT32_TYPE) {
			// Ordered; 'v' contains the column and order info.
			SValue tk(k), tv(v), subArgs;
			void* c = NULL;
			while (tv.GetNextItem(&c, &k, &v) == B_OK) {
				status_t err = add_order_item(query, started, newArgs ? &subArgs : NULL, builder, k, v);
				if (err != B_OK) return err;
			}
			if (newArgs) newArgs->JoinItem(tk, subArgs);
		} else {
			status_t err = add_order_item(query, started, newArgs, builder, k, v);
			if (err != B_OK) return err;
		}
	}
	return B_OK;
}

static void parse_where(const SValue& where, const SSQLBuilder& builder, SString* result)
{	
	SValue lhs = where[BV_ITERABLE_WHERE_LHS];
	
	// do the lhs recursion first
	if (lhs[BV_ITERABLE_WHERE_LHS].IsDefined())
	{
		result->Append("(");
		parse_where(lhs, builder, result);
		result->Append(") ");
	}
	else
	{
		result->Append(lhs.AsString());
	}
	
	result->Append(" ");
	result->Append(where[BV_ITERABLE_WHERE_OP].AsString());
	result->Append(" ");
	
	SValue rhs = where[BV_ITERABLE_WHERE_RHS];
	
	if (rhs[BV_ITERABLE_WHERE_LHS].IsDefined())
	{
		result->Append("(");
		parse_where(rhs, builder, result);
		result->Append(")");
	}
	else
	{
		bool isString = (rhs.Type() == B_STRING_TYPE);
		if (isString) result->Append("\"");
		result->Append(rhs.AsString());
		if (isString) result->Append("\"");
	}
}

SSQLBuilder::SSQLBuilder()
{
}

SSQLBuilder::~SSQLBuilder()
{
}

/*!	Takes an SValue query (containing the mappings BV_ITERABLE_SELECT, BV_ITERABLE_FILTER,
	BV_ITERABLE_WHERE, and BV_ITERABLE_ORDER_BY) and generates the corresponding SQL query string.
	@param[out] outSql The resulting SQL string to be handed off to the database.
	@param[in,out] inoutArgs Incoming, this is the iterator's arguments as provider to
		ParseArgs().  Outgoing, it is the actual arguments used in constructing
		the SQL string to be returned by Options().
	@param[in] dbname Database name to be used in SQL string.
	@return B_OK if all is well, else an error code.
*/
status_t SSQLBuilder::BuildSQLString(SString* outSql, SValue* inoutArgs, const SString& dbname) const
{
	SString selectStr, whereStr, orderByStr;
	SValue tmp = (*inoutArgs)[BV_ITERABLE_SQL];

	if (tmp != SValue::Undefined())
	{
		// XXX Should we try to insert the database name in
		// to the sql query?
		SString query = tmp.AsString();
		outSql->Append(query);
		*inoutArgs = SValue(BV_ITERABLE_SQL, SValue::String(query));
	}
	else
	{
		status_t err = BuildSQLSubStrings(&selectStr, &whereStr, &orderByStr, inoutArgs);
		if (err != B_OK) return err;

		if (selectStr != "") {
			outSql->Append(selectStr);
			outSql->Append(" FROM ");
		}

		outSql->Append(dbname);

		if (whereStr != "") {
			outSql->Append(" ");
			outSql->Append(whereStr);
		}

		if (orderByStr != "") {
			outSql->Append(" ");
			outSql->Append(orderByStr);
		}
	}

	return B_OK;
}

status_t SSQLBuilder::BuildSQLSubStrings(SString* outSelect, SString* outWhere, SString* outOrderBy, SValue* inoutArgs) const
{
	SValue newArgs;

	if (outSelect) {
		const SValue select = (*inoutArgs)[BV_ITERABLE_SELECT];
		if (select.IsDefined())
		{
			outSelect->Append("SELECT ");

			SValue selectResult;
			SValue k, v;
			void* cookie = NULL;
			bool started = false;
			while (select.GetNextItem(&cookie, &k, &v) == B_OK) {
				SValue col(v);
				MapSQLColumn(&col);
				if (col == B_NULL_VALUE) continue;
				if (col.StatusCheck() != B_OK) return col.StatusCheck();
				selectResult.Join(v);
				v = col;

				SString selectStr(v.AsString());
				DbgOnlyFatalErrorIf(selectStr.FindSetFirst(" \n\t\r") >= 0, "SELECT string clauses with multiple columns no longer supported");
				if (selectStr == "") return B_ERROR;
				if (started) outSelect->Append(" ");
				started = true;
				outSelect->Append("\"");
				outSelect->Append(selectStr);
				outSelect->Append("\"");
			}
			if (!started) return B_ERROR;
		}
	}

	if (outWhere) {
		SValue filter = (*inoutArgs)[BV_ITERABLE_FILTER];
		if (filter.IsDefined()) {
			SValue origFilter(filter);
			filter = CreateSQLFilter(filter);
			if (filter.IsDefined()) {
				newArgs.JoinItem(BV_ITERABLE_FILTER, origFilter);
			}
		}

		SValue where = (*inoutArgs)[BV_ITERABLE_WHERE];
		if (filter.IsDefined()) {
			if (where.IsDefined()) {
				where = SValue(BV_ITERABLE_WHERE_LHS, where);
				where.JoinItem(BV_ITERABLE_WHERE_OP, SValue::String("AND"));
				where.JoinItem(BV_ITERABLE_WHERE_RHS, filter);
			} else {
				where = filter;
			}
		}

		if (where.IsDefined())
		{
			// XXX TAKE ME OUT SOME TIME SOON!!!!!!
			DbgOnlyFatalErrorIf(where.AsString() != "", "WHERE string clauses no longer supported.");
			outWhere->Append("WHERE ");
			parse_where(where, *this, outWhere);
			newArgs.JoinItem(BV_ITERABLE_WHERE, where);
		}
	}

	if (outOrderBy) {
		SValue tmp = (*inoutArgs)[BV_ITERABLE_ORDER_BY];
		if (tmp.IsDefined()) {
			SValue orderArgs;
			bool started = false;
			status_t err = add_order_value(outOrderBy, &started, &orderArgs, *this, tmp);
			if (err != B_OK) return err;
			newArgs.JoinItem(BV_ITERABLE_ORDER_BY, orderArgs);
		}
	}

	*inoutArgs = newArgs;
	return B_OK;
}

void SSQLBuilder::MapSQLColumn(SValue* /*inoutColumn*/) const
{
}

void SSQLBuilder::MapSQLOrderColumn(SValue* inoutColumnName, SValue* inoutOrder) const
{
	MapSQLColumn(inoutColumnName);
}

SValue SSQLBuilder::CreateSQLFilter(const SValue& filter) const
{
	return SValue::Undefined();
}

// *********************************************************************************
// ** BGenericIterable *************************************************************
// *********************************************************************************

BGenericIterable::BGenericIterable()
	: m_lock("BGenericIterable::m_lock")
{
}

BGenericIterable::BGenericIterable(const SContext& context)
	: BnIterable(context)
	, m_lock("BGenericIterable::m_lock")
{
}

BGenericIterable::~BGenericIterable()
{
}

lock_status_t BGenericIterable::Lock() const
{
	return m_lock.Lock();
}

void BGenericIterable::Unlock() const
{
	m_lock.Unlock();
}

sptr<IIterator> BGenericIterable::NewIterator(const SValue& args, status_t* error)
{
	status_t err = B_NO_MEMORY;
	sptr<GenericIterator> iter = NewGenericIterator(args);

	if (iter != NULL) {
		err = iter->StatusCheck();
		if (err == B_OK) {
			err = iter->ParseArgs(args);
		}
		if (err != B_OK) iter = NULL;
	}

	if (error) *error = err;
	return iter.ptr();
}

void BGenericIterable::ForEachIteratorLocked(for_each_iterator_func func, void* cookie)
{
	// Avoid repeatedly acquiring/releasing a reference.
	const sptr<BGenericIterable> me(this);

	const size_t N = m_iterators.CountItems();
	for (size_t i = 0 ; i < N ; i++)
	{
		sptr<GenericIterator> iter = m_iterators[i].promote();
		if (iter != NULL) {
			if (!func(me, iter, cookie))
				break;
		}
	}
}

void BGenericIterable::PushIteratorChangedLocked()
{
	const size_t N = m_iterators.CountItems();
	for (size_t i = 0 ; i < N ; i++)
	{
		sptr<GenericIterator> iter = m_iterators[i].promote();
		if (iter != NULL) {
			iter->PushIteratorChanged(iter.ptr());
		}
	}
}

// =================================================================================
// =================================================================================
// =================================================================================

BGenericIterable::GenericIterator::GenericIterator(const SContext& context, const sptr<BGenericIterable>& owner)
	: BnRandomIterator(context)
	, m_owner(owner)
{
}

void BGenericIterable::GenericIterator::InitAtom()
{
	SAutolock _l(m_owner->Lock());
	m_owner->m_iterators.AddItem(this);
}

status_t BGenericIterable::GenericIterator::FinishAtom(const void* )
{
	return FINISH_ATOM_ASYNC;
}

BGenericIterable::GenericIterator::~GenericIterator()
{
	SAutolock _l(m_owner->Lock());
	m_owner->m_iterators.RemoveItemFor(this);
}

SValue BGenericIterable::GenericIterator::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	// XXX This is a copy of the code pidgen generates for BnIterable.  I feel so dirty!
	return SValue(which * SValue(IIterator::Descriptor(), SValue::Binder(this)));
}

status_t BGenericIterable::GenericIterator::StatusCheck() const
{
	return B_OK;
}

SValue BGenericIterable::GenericIterator::Options() const
{
	return SValue::Undefined();
}

status_t BGenericIterable::GenericIterator::Next(IIterator::ValueList* keys, IIterator::ValueList* values, uint32_t flags, size_t count)
{
	// Before adding items to the end of what they might currently have
	// we need to make sure they are empty
	keys->MakeEmpty();
	values->MakeEmpty();
	
	status_t err = B_OK;
	SValue key;
	SValue value;
	
	// Avoid repeatedly acquiring/releasing references.
	const sptr<GenericIterator> me(this);

	SAutolock _l(m_owner->Lock());

	const size_t limit = (BINDER_IPC_LIMIT > count && count != 0) ? count : BINDER_IPC_LIMIT;
	for (size_t i = 0 ; i < limit ; i++)
	{
		err = NextLocked(flags, &key, &value);
		if (err != B_OK)
		{
			// we don't want to send out B_END_OF_DATA when we
			// have items in the vectors.
			if (err == B_END_OF_DATA && i > 0)
				err = B_OK;
			break;
		}
		if (keys) keys->AddItem(key);
		if (values) values->AddItem(value);
	}

	return err;
}

status_t BGenericIterable::GenericIterator::Remove()
{
	SAutolock _l(m_owner->Lock());
	return RemoveLocked();
}

size_t BGenericIterable::GenericIterator::Count() const
{
	return 0;
}

size_t BGenericIterable::GenericIterator::Position() const
{
	return 0;
}

void BGenericIterable::GenericIterator::SetPosition(size_t /*p*/)
{
}

void BGenericIterable::GenericIterator::MapSQLColumn(SValue* inoutColumn) const
{
	m_owner->MapSQLColumn(inoutColumn);
}

void BGenericIterable::GenericIterator::MapSQLOrderColumn(SValue* inoutColumnName, SValue* inoutOrder) const
{
	m_owner->MapSQLOrderColumn(inoutColumnName, inoutOrder);
}

SValue BGenericIterable::GenericIterator::CreateSQLFilter(const SValue& filter) const
{
	return m_owner->CreateSQLFilter(filter);
}

status_t BGenericIterable::GenericIterator::ParseArgs(const SValue& /*args*/)
{
	return B_OK;
}

status_t BGenericIterable::GenericIterator::RemoveLocked()
{
	return B_UNSUPPORTED;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
