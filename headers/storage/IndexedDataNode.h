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

#ifndef _STORAGE_INDEXEDDATANODE_H
#define _STORAGE_INDEXEDDATANODE_H

/*!	@file storage/IndexedDataNode.h
	@ingroup CoreSupportDataModel
	@brief Helper class for implementing an INode containing an array of data.
*/

#include <storage/MetaDataNode.h>
#include <storage/IndexedIterable.h>
#include <storage/DatumGeneratorInt.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

//!	An implementation of INode that holds an array of data, identified by index.
/*!	The class derives from BMetaDataNode, providing a more concrete
	implementation for nodes that hold an indexed array of simple data.
	It implements BMetaDataNode::LookupEntry() for you.  It takes care
	of pushing the INode::NodeChanged and INode::EntryModified events,
	through use of SDatumGeneratorInt::ReportChangeAtLocked().

	The class derives from BIndexedIterable, providing index-based iterators
	over the node data.  It implements BIndexedIterable::EntryAtLocked()
	for you.  It calls BGenericIterable::PushIteratorChangedLocked() as needed
	when its data changes, through use of
	SDatumGeneratorInt::ReportChangeAtLocked().

	The class derives from SDatumGeneratorInt to assist in the implementation
	of BMetaDataNode and BIndexedIterable.

	You must still implement the methods BIndexedIterable::CountEntriesLocked(),
	SDatumGeneratorInt::ValueAtLocked(), SDatumGeneratorInt::StoreValueAtLocked(),
	and the new virtuals EntryIndexOfLocked() and EntryNameAtLocked().

	@nosubgrouping
*/
class BIndexedDataNode : public BMetaDataNode, public BIndexedIterable, public SDatumGeneratorInt
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									BIndexedDataNode(uint32_t mode = IDatum::READ_WRITE);
									BIndexedDataNode(const SContext& context, uint32_t mode = IDatum::READ_WRITE);
protected:
	virtual							~BIndexedDataNode();
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
	/*!	@name Implementation
		Provide default implementation based on new capabilities. */
	//@{

			//!	Implement using EntryIndexOfLocked() and FetchEntryAtLocked().
	virtual status_t				LookupEntry(const SString& entry, uint32_t flags, SValue* node);
			//!	Implement using EntryNameAtLocked() and FetchEntryAtLocked().
	virtual	status_t				EntryAtLocked(const sptr<IndexedIterator>& it, size_t index, uint32_t flags, SValue* key, SValue* entry);
			//!	Also push INode::NodeChanged, INode::EntryModified, and IIterator::IteratorChanged events.
	virtual	void					ReportChangeAtLocked(size_t index, const sptr<IBinder>& editor, uint32_t changes, off_t start=-1, off_t length=-1);
			//!	Use ValueAtLocked() to retrieve the entry and DatumAtLocked() if INode::REQUEST_DATA is set.
			/*!	Before retrieving the value, calls AllowDataAtLocked(), to check if it is allowed to
				return a copy of the data (instead of an IDatum) for this entry. */
			status_t				FetchEntryAtLocked(size_t index, uint32_t flags, SValue* entry);

			//!	Control whether a client will receive data for INode::REQUEST_DATA.
			/*!	The default implementation calls SizeAtLocked() to disable data
				copying if the data size > 2048.
				@note If your data at an index is actually an object,
				AllowDataAtLocked() @b must return true so that that object will
				be returned, not an IDatum wrapping it. */
	virtual	bool					AllowDataAtLocked(size_t index) const;

	//@}

	// --------------------------------------------------------------
	/*!	@name New Pure Virtuals
		New virtuals that must be implemented by derived classes, to associated
		entry names with indices. */
	//@{

			//!	Return the index for an entry name in the node.
			/*!	Return an error code if the lookup failed, usually B_ENTRY_NOT_FOUND. */
	virtual	ssize_t					EntryIndexOfLocked(const SString& entry) const = 0;
			//!	Return the name of an entry at a given index.
			/*!	This class guarantees it will not call this function with an
				invalid index. */
	virtual	SString					EntryNameAtLocked(size_t index) const = 0;

	//@}

	// --------------------------------------------------------------
	/*!	@name Other Pure Virtuals
		Purely informative definition of remaining pure virtuals from
		the base classes. */
	//@{

			//!	From BIndexedIterable::CountEntriesLocked().
	virtual	size_t					CountEntriesLocked() const = 0;
			//!	From SDatumGeneratorInt::ValueAtLocked().
	virtual	SValue					ValueAtLocked(size_t index) const = 0;
			//!	From SDatumGeneratorInt::StoreValueAtLocked().
	virtual	status_t				StoreValueAtLocked(size_t index, const SValue& value) = 0;

	//@}

private:
									BIndexedDataNode(const BIndexedDataNode&);
			BIndexedDataNode&		operator=(const BIndexedDataNode&);
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif // _STORAGE_INDEXEDDATANODE_H

