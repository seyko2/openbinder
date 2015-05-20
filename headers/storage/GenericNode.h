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

#ifndef _STORAGE_GENERICNODE_H
#define _STORAGE_GENERICNODE_H

/*!	@file storage/GenericNode.h
	@ingroup CoreSupportDataModel
	@brief Common base implementations of INode interface.
*/

#include <support/INode.h>
#include <support/Locker.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
using namespace support;
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

class IDatum;

// ==========================================================================
// ==========================================================================

//!	This is the most generic base-class implementation of the INode interface.
/*!	The main purpose of this class is to deal with the mechanics of walking
	the node and basic management of attributes/meta-data.

	XXX Should all of this functionality be here?  There are some rare
	occasions where you will want a node without any meta data (that would
	be a node representing the meta data), in which case you still want
	to use BGenericNode for the other functionality; you'll need to override
	Attributes() et al to turn off the meta data functionality.  I don't
	think there should be any other time you want such a node, though, so
	it seems like BGenericNode should be what we have here, fully functional.

	@nosubgrouping
*/
class BGenericNode : public BnNode
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
								BGenericNode();
								BGenericNode(const SContext& context);
protected:
	virtual						~BGenericNode();
public:

			//!	Lock the node's state.
			/*!	To keep itself consistent, this class provides a public lock that
				is used when doing read/write operations on the class state as well
				as the actual data.  The default implemention of this method acquires
				an internal lock.  You can override it to cause all internal
				implementation to acquire some other lock.  Be sure to also override
				Unlock() if doing so. */
	virtual	lock_status_t		Lock() const;

			//!	Unlock the node's state.
	virtual	void				Unlock() const;

	//@}

	// ---------------------------------------------------------------------
	/*! @name INode Interface
		Generic implementation of the INode interface.  Note that you generally
		should never override these methods, instead customizing the new
		virtuals introduced by this class.  Some very special implementations
		may override Attributes() and all of the meta data getter/setter methods. */
	//@{
			//!	Returns a node to access this node's meta-data.
	virtual	sptr<INode>			Attributes() const;
			//!	Acquires the node lock and calls MimeTypeLocked().
	virtual	SString				MimeType() const;
			//!	Acquires the node lock and calls SetMimeTypeLocked().
	virtual	void				SetMimeType(const SString& value);
			//!	Acquires the node lock and calls CreationDateLocked().
	virtual	nsecs_t				CreationDate() const;
			//!	Acquires the node lock and calls SetCreationDateLocked().
	virtual	void				SetCreationDate(nsecs_t value);
			//!	Acquires the node lock and calls ModifiedDateLocked().
	virtual	nsecs_t				ModifiedDate() const;
			//!	Acquires the node lock and calls SetModifiedDateLocked().
	virtual	void				SetModifiedDate(nsecs_t value);
			//!	Takes care of standard node walking algorithm.
	virtual	status_t			Walk(SString* path, uint32_t flags, SValue* node);

			//!	Calls StoreMimeTypeLocked() and updates datums, pushes event.
			status_t			SetMimeTypeLocked(const SString& value);
			//!	Calls StoreCreationDateLocked() and updates datums, pushes event.
			status_t			SetCreationDateLocked(nsecs_t value);
			//!	Calls StoreModifiedDateLocked() and updates datums, pushes event.
			status_t			SetModifiedDateLocked(nsecs_t value);
	//@}
	
	// ---------------------------------------------------------------------
	/*! @name New Node Virtuals
		Subclasses can override these (instead of the raw INode APIs) to
		customize the behavior of their node. */
	//@{

			//! Used by Walk() to find a normal entry in the node.
			/*!	At this point you MUST deal correctly with the REQUEST_DATA and
				COLLAPSE_NODE flags, by either returning the actual value or an object. */
	virtual	status_t			LookupEntry(const SString& entry, uint32_t flags, SValue* node) = 0;

			//!	Called as part of walking with the CREATE_NODE flag set.
			/*!	The default implementation simply returns NULL. */
	virtual	sptr<INode>			CreateNode(SString* name, status_t* err);

			//!	Called as part of walking with the CREATE_DATUM flag set.
			/*!	The default implementation simply returns NULL. */
	virtual	sptr<IDatum>		CreateDatum(SString* name, uint32_t flags, status_t* err);

			//!	Default implementation always returns "application/vnd.palm.catalog".
	virtual	SString				MimeTypeLocked() const;
			//!	Default implementation does nothing, returning B_UNSUPPORTED.
	virtual	status_t			StoreMimeTypeLocked(const SString& value);
			//!	You must implement this to return the creation date.
	virtual	nsecs_t				CreationDateLocked() const = 0;
			//!	Default implementation does nothing, returning B_UNSUPPORTED.
	virtual	status_t			StoreCreationDateLocked(nsecs_t value);
			//!	You must implement this to return the last modification date.
	virtual	nsecs_t				ModifiedDateLocked() const = 0;
			//!	Default implementation does nothing, returning B_UNSUPPORTED.
	virtual	status_t			StoreModifiedDateLocked(nsecs_t value);

	//@}
	
	// ---------------------------------------------------------------------
	/*! @name Meta Data Virtuals
		You can override these methods to support additional meta-data in your
		node.  The default implementation of them only supports the standard
		meta-data that is common to all nodes: mimeType, creationDate, and modifiedDate.

		To implement your own meta-data, you can choose to either override all
		of the virtuals here, OR only override LookupMetaEntry() and Attributes().
		In the latter case, you are supplying your own complete custom implementation
		for meta-data through the object returned by Attributes(); LookupMetaEntry() 
		must also be implemented because it can be called by the standard Walk()
		implementation.
		
		@note This API is TEMPORARY until the complete set of data model classes
		have been implemented (it depends on them for its internal implementation,
		which will have repercussions for how these APIs operate). */
	//@{

			//! Used by Walk() and others to find a meta-data entry in the node.
			/*!	At this point you MUST deal correctly with the REQUEST_DATA flag,
				by either returning an actual value or an object. */
	virtual status_t			LookupMetaEntry(const SString& entry, uint32_t flags, SValue* node);

			//! Create a new piece of meta-data in the node.
			/*!	The default implementation returns B_UNSUPPORTED to indicate that
				it can not create any new meta-data.  Extend this to implement nodes
				that can contain additional meta-data. */
	virtual	status_t			CreateMetaEntry(const SString& name, const SValue& initialValue, sptr<IDatum>* outDatum = NULL);

			//!	Remove a piece of meta-data from the node.
			/*!	The default implementation returns B_UNSUPPORTED to indicate that it
				can not remove meta-data. */
	virtual	status_t			RemoveMetaEntry(const SString& name);

			//!	Change the name of a piece of meta-data in the node.
			/*!	The default implementation returns B_UNSUPPORTED to indicate that it
				can not rename meta-data. */
	virtual	status_t			RenameMetaEntry(const SString& old_name, const SString& new_name);

			//!	Return a meta-data entry at a particular offset in the node.
			/*!	Like LookupMetaEntry(), you must correctly respect the REQUEST_DATA flag.
				@note The index is signed, so that 0 is the start of user meta data.  The
				standard node meta-data is retrieved through negative numbers: -1 = mimeType,
				-2 = creationDate, -3 = modifiedDate; you should normally pass these through
				as-is to the base class. */
	virtual	status_t			MetaEntryAtLocked(ssize_t index, uint32_t flags, SValue* key, SValue* entry);

			//!	Return the number of meta-data entries in the node.
			/*!	@note This count is only of user meta-data.  The standard BGenericNode meta-data
				is not included in the count.  The default implementation returns 0. */
	virtual	size_t				CountMetaEntriesLocked() const;

	//@}

protected:

private:
			class				node_state;
			class				meta_datum;
			friend class		node_state;
			friend class		meta_datum;

			sptr<node_state>	get_state_l() const;
			status_t			get_meta_value_l(ssize_t index, SValue* key, SValue* value);
			status_t			datum_changed_l(size_t index, const SValue& newValue);

	mutable	SLocker				m_lock;
	mutable	wptr<node_state>	m_state;
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif // _STORAGE_GENERICNODE_H
