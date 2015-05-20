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

#ifndef _STORAGE_STRUCTUREDNODE_H
#define _STORAGE_STRUCTUREDNODE_H

/*!	@file storage/StructuredNode.h
	@ingroup CoreSupportDataModel
	@brief Convenience for creating data model interface to a structure.
*/

#include <storage/IndexedTableNode.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

//!	An INode containing a fixed set of named entries.
/*!
	BStructuredNode is a convenience class that helps you expose data
	as a node.  You'll want to use this class if you want a node
	with a fixed set of entries, where @e you supply the data (unlike
	BCatalog, which allocates and manages the data for each entry
	itself).

	To use this convenience class, make a subclass of it.  Into the
	constructor, pass an array of strings for the names of the entries
	and the count of those strings.  You must
	guarantee that those c-strings exist for the lifetime of your
	object.  A common way to do that is to make them static global
	constants, and to include an SPackageSptr in your subclass.  That
	technique will keep your library loaded (and therefore the strings
	around) as long as one of your objects exists.

	To return data from the class, implement ValueAtLocked().  If your
	data can be changed, you can optionally implement StoreValueAtLocked().
	These APIs operate in terms of indices into your name array.
	
	Via BBIndexedDataNode (and its base classes, BMetaDataNode,
	BIndexedIterable, and SIndexedDatumGenerator), BStructuredNode takes
	care of generating IDatums as needed, managing IIterators, and the
	rest of the functionality needed to participate in the @ref BinderDataModel.
	You can override various other methods of those classes to further
	customize the behavior of your node, and use BGenericNode::SetMimeType()
	to customize the MIME type of your node.
*/
class BStructuredNode : public BIndexedDataNode
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									BStructuredNode(const SContext &context,
													const char **entries, size_t entryCount,
													uint32_t mode = IDatum::READ_ONLY);
protected:
	virtual							~BStructuredNode();
public:

	//@}

	// --------------------------------------------------------------
	/*!	@name Implement Name Mapping
		Using array of names, implement APIs to map from names to
		data indices. */
	//@{

	//!	Find the index of an entry in your array.
	virtual	ssize_t					EntryIndexOfLocked(const SString& entry) const;
	//!	Retrieve the name of an entry at an index in your array.
	virtual	SString					EntryNameAtLocked(size_t index) const;
	//!	Return the number of entries in your array.
	virtual	size_t					CountEntriesLocked() const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Get/Set Entry Values
		Simplest functions from SIndexedDatumGenerator that must be
		implement to work with your data.  You are free to override
		other data-related functions on that class for more detailed
		control. */

	//!	Must be implemented as per SIndexedDatumGenerator::ValueAtLocked().
	virtual	SValue					ValueAtLocked(size_t index) const = 0;
	//!	Default implementation returns B_UNSUPPORTED.
	/*!	If you override this, be sure to also supply IDatum::READ_WRITE for
		the mode in your constructor. */
	virtual	status_t				StoreValueAtLocked(size_t index, const SValue& value);

	//@}

private:
									BStructuredNode(const BStructuredNode&);
			BStructuredNode&		operator=(const BStructuredNode&);

			const char**			m_entries;
			const size_t			m_entryCount;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif // _STORAGE_STRUCTUREDNODE_H
