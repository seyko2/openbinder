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

#ifndef _DMPROVIDER_SCHEMATAROWIDJOIN_H
#define _DMPROVIDER_SCHEMATAROWIDJOIN_H

/*!	@file dmprovider/SchemaRowIDJoin.h
	@ingroup CoreDataManagerProvider
	@brief Join operation between two BSchemaTableNode objects based on row ID.
*/

#include <dmprovider/SchemaTableNode.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace dmprovider {
using namespace palmos::storage;
#endif

/*!	@addtogroup CoreDataManagerProvider
	@{
*/

// ==========================================================================
// ==========================================================================

//!	Join operation between two BSchemaTableNode objects based on row ID.
/*!	This class provides a simple client-side join of two schema tables.
	It results in a new table node with an entry for each of the entries
	in the @a join table, which are then mapped back to a corresponding
	row id in the @a primary through @a joinIDCol.  In other words, it
	supports a 1:N relationship between the primary:join, using Data
	Manager row IDs to map between them.

	There are a number of limitations with the current implementation:

	- Filtering and sorting can only happen on the @a join table.
	- It assumes that every row in @a join will have a corresponding row
	  in @a primary.  If that is not the case, then this row will still
	  show up in the join, just without any @a primary data.
	
	@note These classes are not a part of the OpenBinder build, but included
	as an example for other similar implementations.
*/
class BSchemaRowIDJoin : public BMetaDataNode, public BGenericIterable
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
			//!	Create a new node containing a join between two BSchemaTableNode objects.
			/*!	@param[in] context Hosting context (not needed by this class).
				@param[in] primary The primary table of the join, containing the
					uniqueRecords.
				@param[in] join The join table, which will be mapped back to the
					primary table through:
				@param[in] joinIDCol Column in the join table that has the row ID of the
					entry in the corresponding primary table.
				@param[in] primaryColumnName If set, the original primary table node
					will be available through a column with this name.  If not set
					(the default) all primary columns will be joined directly into
					the new join table.
			*/
									BSchemaRowIDJoin(	const SContext& context,
														const sptr<BSchemaTableNode>& primary,
														const sptr<BSchemaTableNode>& join,
														const SString& joinIDCol,
														const SString& primaryColumnName = SString());
protected:
			//! Clean up all resources.
	virtual							~BSchemaRowIDJoin();
public:

			//!	Disambiguate.
	virtual	lock_status_t			Lock() const;
			//!	Disambiguate.
	virtual	void					Unlock() const;
			//!	Disambiguate.
	inline	SContext				Context() { return BMetaDataNode::Context(); }
			//!	Make INode and IIterable accessible.
	virtual	SValue					Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);
			//!	Return initialization status -- B_OK if all is well, else an error code.
			status_t				StatusCheck() const;
	//@}

	// --------------------------------------------------------------
	/*!	@name Configuration
		Functions for customizing how the join is exposed. */
	//@{

			//!	By default, clients are not allowed to modify the MIME type.
			/*!	To use the normal BMetaDataNode implementation, override
				this to call directly to BGenericNode::SetMimeType(). 
				To change the MIME type yourself, you can call
				SetMimeTypeLocked(). */
	virtual	void					SetMimeType(const SString& value);

			//!	Retrieve the MIME type of entries (row nodes).
			SString					RowMimeTypeLocked() const;

			//!	Set the MIME type that all entries (row nodes) will report.
			status_t				StoreRowMimeTypeLocked(const SString& mimeType);

	//@}

	// --------------------------------------------------------------
	/*!	@name Data Model Implementation
		Provide default implementation based on new capabilities. */
	//@{

	class RowNode;

			//!	Implement by parsing name into primary and join row names and then calling NodeForLocked().
	virtual status_t				LookupEntry(const SString& entry, uint32_t flags, SValue* node);

			//!	Return INode object for a particular table row ID.
			sptr<RowNode>			NodeForLocked(	const sptr<BSchemaTableNode::RowNode>& priNode,
													const sptr<BSchemaTableNode::RowNode>& joinNode);

	//@}

	//  ------------------------------------------------------------------
	/*!	@name Object Generation */
	//@{

	class JoinIterator;

	virtual	sptr<GenericIterator>	NewGenericIterator(const SValue& args);

			//!	Called when a new row object needs to be created.
			/*!	The default implementation instantiates and returns a
				new IndexedDatum object. */
	virtual	sptr<RowNode>			NewRowNodeLocked(	const sptr<BSchemaTableNode::RowNode>& priNode,
														const sptr<BSchemaTableNode::RowNode>& joinNode);

	//@}
private:
									BSchemaRowIDJoin(const BSchemaRowIDJoin&);
			BSchemaRowIDJoin&		operator=(const BSchemaRowIDJoin&);

	friend class RowNode;
	friend class JoinIterator;

	const	sptr<BSchemaTableNode>					m_primaryTable;
	const	sptr<BSchemaTableNode>					m_joinTable;
	const	SString									m_joinIDColumn;
	const	SString									m_primaryColumnName;

			ssize_t									m_joinIDIndex;

			SString									m_rowMimeType;

			sptr<BSchemaTableNode::DataAccessor>	m_primaryData;
			sptr<BSchemaTableNode::DataAccessor>	m_joinData;

			struct node_key {
				sptr<BSchemaTableNode::RowNode> primary;
				sptr<BSchemaTableNode::RowNode> join;

				bool operator<(const node_key& o) const { return primary < o.primary || join < o.join; }
			};

			SKeyedVector<node_key, wptr<RowNode> >	m_nodes;
};

// --------------------------------------------------------------------------

//!	Node object generated for a row in a BSchemaRowIDJoin.
/*!	This object provides read/write access to a particular row in join.

	@todo Deal with changes in the columns of the underlying tables.
	@todo Push appropriate events when the underlying table change.

	@nosubgrouping
*/
class BSchemaRowIDJoin::RowNode : public BMetaDataNode, public BIndexedIterable
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									RowNode(	const SContext& context,
												const sptr<BSchemaRowIDJoin>& owner,
												const sptr<BSchemaTableNode::RowNode>& primary,
												const sptr<BSchemaTableNode::RowNode>& join);
protected:
	virtual							~RowNode();
			//!	The destructor must call Lock(), so return FINISH_ATOM_ASYNC to avoid deadlocks.
	virtual	status_t				FinishAtom(const void* id);
			//!	Make this object stay around while there are links on it.
	virtual	bool					HoldRefForLink(const SValue& binding, uint32_t flags);
public:

			//!	Reimplemented to use the BSchemaRowIDJoin lock.
	virtual	lock_status_t			Lock() const;
			//!	Reimplemented to use the BSchemaRowIDJoin lock.
	virtual	void					Unlock() const;
			//!	Disambiguate.
	inline	SContext				Context() { return BMetaDataNode::Context(); }
			//!	Make both INode and IIterable accessible.
	virtual	SValue					Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);

	//@}

	// --------------------------------------------------------------
	/*!	@name Implementation
		Make it do the right thing. */
	//@{

			//!	Implement.
	virtual status_t				LookupEntry(const SString& entry, uint32_t flags, SValue* node);
			//!	Implement.
	virtual	status_t				EntryAtLocked(const sptr<IndexedIterator>& it, size_t index, uint32_t flags, SValue* key, SValue* entry);
			//!	Return BSchemaRowIDJoin::RowMimeTypeLocked().
	virtual	SString					MimeTypeLocked() const;
			//!	Return B_UNSUPPORTED (clients can not modify the MIME type).
	virtual	status_t				StoreMimeTypeLocked(const SString& value);
			//!	Return number of entries in the join table.
	virtual	size_t					CountEntriesLocked() const;

	//@}

private:
									RowNode(const RowNode&);
			RowNode&				operator=(const RowNode&);

	const	sptr<BSchemaRowIDJoin>			m_owner;
	const	sptr<BSchemaTableNode::RowNode>	m_primary;
	const	sptr<BSchemaTableNode::RowNode>	m_join;
};

// --------------------------------------------------------------------------

//!	Iterator over a BSchemaTableNode.
/*!	Custom iterator implementation that uses a cursor to step over the
	results of a query.

	@nosubgrouping
*/
class BSchemaRowIDJoin::JoinIterator : public BGenericIterable::GenericIterator
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									JoinIterator(const SContext& context, const sptr<BSchemaRowIDJoin>& owner);
protected:
	virtual							~JoinIterator();
			//! We support both IIterator and IRandomIterator.
	virtual SValue					Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);
public:
			//!	Return FINISH_ATOM_ASYNC, same as GenericIterator::FinishAtom().
	virtual	status_t				FinishAtom(const void* id);

	//@}

	virtual	SValue					Options() const;
	virtual status_t				Remove();
	virtual size_t					Count() const;
	virtual	size_t					Position() const;
	virtual void					SetPosition(size_t p);

	virtual	status_t				ParseArgs(const SValue& args);

	virtual	status_t				NextLocked(uint32_t flags, SValue* key, SValue* entry);

private:
									JoinIterator(const JoinIterator&);
			JoinIterator&			operator=(const JoinIterator&);

	const	sptr<BSchemaRowIDJoin>					m_owner;
			sptr<BSchemaTableNode::DataAccessor>	m_primaryData;
			sptr<BSchemaTableNode::QueryIterator>	m_joinIterator;
			SValue									m_args;
			bool									m_includePrimaryNode;
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::dmprovider
#endif

#endif // _DMPROVIDER_SCHEMATAROWIDJOIN_H
