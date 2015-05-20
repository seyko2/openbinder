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

#ifndef _STORAGE_INDEXEDTABLENODE_H
#define _STORAGE_INDEXEDTABLENODE_H

/*!	@file storage/IndexedTableNode.h
	@ingroup CoreSupportDataModel
	@brief Helper class for implementing an INode containing a complex table structure.
*/

#include <storage/IndexedDataNode.h>

#include <support/ITable.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

//!	An implementation of INode that holds a 2d array of data, identified by index.
/*!	The class derives from BMetaDataNode, providing a more concrete
	implementation for nodes that hold an array of items, which themselves have
	a common array of items (a table structure).  It implements BMetaDataNode::LookupEntry()
	for you, returning an INode of the data at that row in the table and also
	taking care of the COLLAPSE_NODE flag to return the data as an SValue mapping.
	It does not push any node events, since this node does not itself contain actual
	data.

	The class derives from BIndexedIterable, providing index-based iterators
	over the table rows.  It implements BIndexedIterable::EntryAtLocked()
	for you, like BMetaDataNode::LookupEntry().  It calls
	BGenericIterable::PushIteratorChangedLocked() if any of the table data changes...
	though it is not clear if this is really the correct behavior (an iterator here
	will only change if it is using COLLAPSE_NODE to retrieve items).

	The class also implements ITable, but at this point only so it can push the
	CellModified event.  You will need to implement the other ITable methods
	if you want their functionality.

	You must implement the following methods for a concrete class.  Providing
	information about rows: BIndexedIterable::CountEntriesLocked(), EntryIndexOfLocked(),
	EntryNameAtLocked().  Providing information about columns: CountColumnsLocked(),
	ColumnIndexOfLocked(), ColumnNameAtLocked().  Providing table data:
	ValueAtLocked(), StoreValueAtLocked().

	@nosubgrouping
*/
class BIndexedTableNode : public BMetaDataNode, public BIndexedIterable, public BnTable
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									BIndexedTableNode(uint32_t mode = IDatum::READ_WRITE);
									BIndexedTableNode(const SContext& context, uint32_t mode = IDatum::READ_WRITE);
protected:
	virtual							~BIndexedTableNode();
public:

			//!	Disambiguate.
	virtual	lock_status_t			Lock() const;
			//!	Disambiguate.
	virtual	void					Unlock() const;
			//!	Disambiguate.
	inline	SContext				Context() { return BMetaDataNode::Context(); }
			//!	Make both INode and IIterable accessible.
	virtual	SValue					Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);

	//@}

	// --------------------------------------------------------------
	/*!	@name Implement Pure Virtuals
		Provide default implementation based on new capabilities. */
	//@{

			//!	Implement using EntryIndexOfLocked() and FetchEntryAtLocked().
	virtual status_t				LookupEntry(const SString& entry, uint32_t flags, SValue* node);
			//!	Implement using EntryNameAtLocked() and FetchEntryAtLocked().
	virtual	status_t				EntryAtLocked(const sptr<IndexedIterator>& it, size_t index, uint32_t flags, SValue* key, SValue* entry);
			//!	Retrieve row entry, taking care of COLLAPSE_NODE flag if needed.
			/*!	@note The iterator @a it will be NULL when not called from an iterator. */
			status_t				FetchEntryAtLocked(const sptr<IndexedIterator>& it, size_t rowIndex, uint32_t flags, SValue* entry);
			//!	Use ValueAtLocked() to retrieve the entry and DatumAtLocked() if it needs to be wrapped in an IDatum.
			/*!	The @a rowNode is only needed if the REQUEST_DATA flag is NOT set.  It is passed in here
				instead of looking it up inside for better performance when this method is called
				many times in a row, such as to implement COLLAPSE_NODE. */
			status_t				FetchValueAtLocked(size_t row, const sptr<BIndexedDataNode>& rowNode, size_t column, uint32_t flags, SValue* entry);

	//@}

	// --------------------------------------------------------------
	/*! @name ITable Implementation */
	//@{

			//!	Returns all column names.
	virtual	SValue					ColumnNames() const;
			//!	Returns column schema info.
	virtual	SValue					Schema() const;
			//!	Returns B_UNSUPPORTED.
	virtual	status_t				CreateRow(SString* inoutName, const SValue& columns, uint32_t flags = 0, sptr<INode>* createdRow = NULL);
			//!	Returns B_UNSUPPORTED.
	virtual	status_t				RemoveRow(const sptr<INode>& row);
			//!	Returns B_UNSUPPORTED.
	virtual	status_t				AddColumn(const SString& name, uint32_t type, size_t maxSize, uint32_t flags, const SValue& extras);
			//!	Returns B_UNSUPPORTED.
	virtual	status_t				RemoveColumn(const SString& name);

	// --------------------------------------------------------------
	/*!	@name Meta-data
		Customization and control of meta-data. */
	//@{

			//!	By default, clients are not allowed to modify the MIME type.
			/*!	To use the normal BMetaDataNode implementation, override
				this to call directly to BGenericNode::SetMimeType(). 
				To change the MIME type yourself, you can call
				SetMimeTypeLocked(). */
	virtual	void					SetMimeType(const SString& value);

			//!	Retrieve the MIME type of entries (row nodes).
			SString					EntryMimeTypeLocked() const;

			//!	Set the MIME type that all entries (row nodes) will report.
			status_t				StoreEntryMimeTypeLocked(const SString& mimeType);

			//!	Perform selection on available columns.
	virtual	void					CreateSelectionLocked(SValue* inoutSelection, SValue* outCookie) const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Structural Virtuals
		New virtuals that must be implemented by derived classes, to associated
		entry names with indices and otherwise manage the table structure. */
	//@{

			//!	Return the index for an row entry name in the node.
			/*!	Return an error code if the lookup failed, usually B_ENTRY_NOT_FOUND. */
	virtual	ssize_t					EntryIndexOfLocked(const SString& entry) const = 0;
			//!	Return the name of a row at a given index.
			/*!	This class guarantees it will not call this function with an
				invalid index. */
	virtual	SString					EntryNameAtLocked(size_t index) const = 0;
			//!	From BIndexedIterable::CountEntriesLocked().
	virtual	size_t					CountEntriesLocked() const = 0;

			//!	Return the index for a column entry name in the node.
			/*!	Return an error code if the lookup failed, usually B_ENTRY_NOT_FOUND. */
	virtual	ssize_t					ColumnIndexOfLocked(const SString& entry) const = 0;
			//!	Return the name of a column at a given index.
			/*!	This class guarantees it will not call this function with an
				invalid index. */
	virtual	SString					ColumnNameAtLocked(size_t index) const = 0;

			//!	Return the total number of columns in the table.
	virtual	size_t					CountColumnsLocked() const = 0;

	//@}

	//  ------------------------------------------------------------------
	/*!	@name Row/Column Management
		Manage the mapping of indices to nodes/datums. */
	//@{

			//!	Retrieve a node of a given row.
			/*!	Returns an existing node if one is currently active for the index.
				Otherwise, calls NewRowNodeLocked() to create a new node and associates
				it with this index. */
			sptr<BIndexedDataNode>	NodeAtLocked(size_t index);

			//!	Retrieve node for a given row only if it already exists.
			/*!	This is like NodeAtLocked(), but doesn't create the node object if
				it doesn't already exist. */
			sptr<BIndexedDataNode>	ActiveNodeAtLocked(size_t index);

			//!	Adjust all active row nodes due to items being added/removed.
			/*!	Your subclass must call this when it changes its set of row items. */
			void					UpdateEntryIndicesLocked(size_t index, ssize_t delta);

			//!	Adjust all active column datums due to items being added/removed.
			/*!	Your subclass must call this when it changes its set of column items. */
			void					UpdateColumnIndicesLocked(size_t index, ssize_t delta);

	//@}

	//  ------------------------------------------------------------------
	/*!	@name Value-based Structure Access
		These are like the corresponding virtuals on BStreamDatum. */
	//@{

			//!	Convenience function for changing a value.
			/*!	The implementation takes care of calling ReportChangeAtLocked()
				for you if the value has changed.  This is semantically the same
				as BStreamDatum::SetValue(). */
			void					SetValueAtLocked(size_t row, size_t col, const SValue& value);

			//!	The default implementation returns ValueAtLocked().Type().
	virtual	uint32_t				ValueTypeAtLocked(size_t row, size_t col) const;
			//!	The default implementation uses ValueAtLocked() and StoreValueAtLocked() to change the type.
	virtual	status_t				StoreValueTypeAtLocked(size_t row, size_t col, uint32_t type);
			//!	The default implementation returns ValueAtLocked().Length().
	virtual	off_t					SizeAtLocked(size_t row, size_t col) const;
			//!	The default implementation uses ValueAtLocked() and StoreValueAtLocked() to change the size.
	virtual	status_t				StoreSizeAtLocked(size_t row, size_t col, off_t size);
			//!	Must be implemented by subclasses to return the current value at an index.
	virtual	SValue					ValueAtLocked(size_t row, size_t col) const = 0;
			//!	Must be implemented by subclasses change the current value at an index.
	virtual	status_t				StoreValueAtLocked(size_t row, size_t col, const SValue& value) = 0;

			//!	Similar to BStreamDatum::ReportChangeLocked(), using same change flags.
			/*!	Also pushes the ITable change events. */
	virtual	void					ReportChangeAtLocked(size_t row, size_t col, const sptr<IBinder>& editor, uint32_t changes, off_t start=-1, off_t length=-1);

	//@}

	//  ------------------------------------------------------------------
	/*!	@name Stream Virtuals
		These are like the corresponding virtuals on BStreamDatum. */
	//@{

			//!	The default implementation return uses ValueAtLocked() to retrieve the data.
	virtual	const void*				StartReadingAtLocked(	size_t row, size_t col, off_t position,
															ssize_t* inoutSize, uint32_t flags) const;
			//!	The default implementation does nothing.
	virtual	void					FinishReadingAtLocked(	size_t row, size_t col, const void* data) const;
			//!	The default implementation uses ValueAtLocked() to create a temporary buffer.
	virtual	void*					StartWritingAtLocked(	size_t row, size_t col, off_t position,
															ssize_t* inoutSize, uint32_t flags);
			//!	The default implementation uses StoreValueAtLocked() to write the data.
	virtual	void					FinishWritingAtLocked(	size_t row, size_t col, void* data);

	//@}

	//  ------------------------------------------------------------------
	/*!	@name Node Generation */
	//@{

	class RowNode;

			//!	Called when a new row object needs to be created.
			/*!	The default implementation instantiates and returns a
				new IndexedDatum object. */
	virtual	sptr<RowNode>			NewRowNodeLocked(size_t index, uint32_t mode);

	//@}

private:
									BIndexedTableNode(const BIndexedTableNode&);
			BIndexedTableNode&				operator=(const BIndexedTableNode&);

	friend class RowNode;

	const	uint32_t				m_mode;
			SString					m_entryMimeType;
			SKeyedVector<size_t, wptr<RowNode> >*	m_nodes;
	mutable	SValue					m_tmpValue;
			size_t					m_writeLen;
};

// --------------------------------------------------------------------------

//!	Node object generated for a row in BIndexedTableNode.
/*!
	@nosubgrouping
*/
class BIndexedTableNode::RowNode : public BIndexedDataNode
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									RowNode(const SContext& context, const sptr<BIndexedTableNode>& owner, size_t rowIndex, uint32_t mode);
protected:
	virtual							~RowNode();
			//!	The destructor must call Lock(), so return FINISH_ATOM_ASYNC to avoid deadlocks.
	virtual	status_t				FinishAtom(const void* id);
			//!	Make this object stay around while there are links on it.
	virtual	bool					HoldRefForLink(const SValue& binding, uint32_t flags);
public:

			//!	Reimplemented to use the BIndexedTableNode lock.
	virtual	lock_status_t			Lock() const;
			//!	Reimplemented to use the BIndexedTableNode lock.
	virtual	void					Unlock() const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Implementation
		Redirect the row node implementation back up to the owning
		BIndexedTableNode. */
	//@{

			//!	Return BIndexedTableNode::EntryMimeTypeLocked().
	virtual	SString					MimeTypeLocked() const;
			//!	Return B_UNSUPPORTED (clients can not modify the MIME type).
	virtual	status_t				StoreMimeTypeLocked(const SString& value);
			//!	Return BIndexedTableNode::CountColumnsLocked().
	virtual	size_t					CountEntriesLocked() const;
			//!	Return BIndexedTableNode::ColumnIndexOfLocked().
	virtual	ssize_t					EntryIndexOfLocked(const SString& entry) const;
			//!	Return BIndexedTableNode::ColumnNameAtLocked().
	virtual	SString					EntryNameAtLocked(size_t index) const;
			//!	Return BIndexedTableNode::ValueTypeAtLocked(m_index, index).
	virtual	uint32_t				ValueTypeAtLocked(size_t index) const;
			//!	Call BIndexedTableNode::StoreValueTypeAtLocked(m_index, index, ...).
	virtual	status_t				StoreValueTypeAtLocked(size_t index, uint32_t type);
			//!	Return BIndexedTableNode::SizeAtLocked(m_index, index).
	virtual	off_t					SizeAtLocked(size_t index) const;
			//!	Call BIndexedTableNode::StoreSizeAtLocked(m_index, index, ...).
	virtual	status_t				StoreSizeAtLocked(size_t index, off_t size);
			//!	Return BIndexedTableNode::ValueAtLocked(m_index, index).
	virtual	SValue					ValueAtLocked(size_t index) const;
			//!	Call BIndexedTableNode::StoreValueAtLocked(m_index, index, ...).
	virtual	status_t				StoreValueAtLocked(size_t index, const SValue& value);
			//!	Call BIndexedTableNode::ReportChangeAtLocked(m_index, index, ...).
	virtual	void					ReportChangeAtLocked(size_t index, const sptr<IBinder>& editor, uint32_t changes, off_t start=-1, off_t length=-1);
			//!	Call BIndexedTableNode::StartReadingAtLocked(m_index, index, ...).
	virtual	const void*				StartReadingAtLocked(	size_t index, off_t position,
															ssize_t* inoutSize, uint32_t flags) const;
			//!	Call BIndexedTableNode::FinishReadingAtLocked(m_index, index, ...).
	virtual	void					FinishReadingAtLocked(	size_t index, const void* data) const;
			//!	Call BIndexedTableNode::StartWritingAtLocked(m_index, index, ...).
	virtual	void*					StartWritingAtLocked(	size_t index, off_t position,
															ssize_t* inoutSize, uint32_t flags);
			//!	Call BIndexedTableNode::FinishWritingAtLocked(m_index, index, ...).
	virtual	void					FinishWritingAtLocked(	size_t index, void* data);

	//@}

			const sptr<BIndexedTableNode>	m_owner;
			ssize_t					m_index;

private:
									RowNode(const RowNode&);
			RowNode&				operator=(const RowNode&);
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif // _STORAGE_INDEXEDTABLENODE_H
