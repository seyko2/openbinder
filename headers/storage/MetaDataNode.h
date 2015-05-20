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

#ifndef _STORAGE_METADATANODE_H
#define _STORAGE_METADATANODE_H

/*!	@file storage/MetaDataNode.h
	@ingroup CoreSupportDataModel
	@brief A base class of INode that provides all standard meta-data.
*/

#include <storage/GenericNode.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

class IDatum;

// ==========================================================================
// ==========================================================================

//!	A base class of INode that provides all standard meta-data.
/*!	This class derives from BGenericNode, adding to it an implementation
	of the basic meta-data (mimeType, creationDate, modifiedDate).
	Derived classes need only implement LookupEntry() to have a
	fully functioning INode.

	@todo Should an option be added here to allow the node to store any
	arbitrary meta-data?  Or should that functionality be provided in
	some further derived class (such as BCatalog)?  I guess this depends
	on whether we want to allow others to rely on extendable meta-data
	functionality...

	@nosubgrouping
*/
class BMetaDataNode : public BGenericNode
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
								BMetaDataNode();
								BMetaDataNode(const SContext& context);
protected:
	virtual						~BMetaDataNode();
public:

			//!	Update the modified date.
			void				TouchLocked();

	//@}

	// --------------------------------------------------------------
	/*!	@name BGenericNode Metadata API
		Implement the BGenericNode virtuals needed to support the
		basic meta-data of a node. */
	//@{

	virtual	SString				MimeTypeLocked() const;
	virtual	status_t			StoreMimeTypeLocked(const SString& value);
	virtual	nsecs_t				CreationDateLocked() const;
	virtual	status_t			StoreCreationDateLocked(nsecs_t value);
	virtual	nsecs_t				ModifiedDateLocked() const;
	virtual	status_t			StoreModifiedDateLocked(nsecs_t value);

	//@}

private:
			void				init();

			SString				m_mimeType;
			nsecs_t				m_creationDate;
			nsecs_t				m_modifiedDate;
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif // _STORAGE_METADATANODE_H
