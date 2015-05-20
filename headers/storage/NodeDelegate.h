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

#ifndef _STORAGE_NODEDELEGATE_H
#define _STORAGE_NODEDELEGATE_H

/*!	@file storage/NodeDelegate.h
	@ingroup CoreSupportDataModel
	@brief Delegation class for wrapping an existing
		INode implementation.
*/

#include <storage/GenericNode.h>

#include <support/INodeObserver.h>
#include <support/Locker.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
using namespace palmos::support;
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

//!	Provides base implementation of delegations for INode.
/*!	Given an existing object that implement the INode interfaces,
	provides a new object that delegates to the existing implementation.

	@note This class only delegates the given @a base object.  It will not
	create a wrapper for any objects retrieved from it, such as IDatums
	or other INodes.

	See BCatalogDelegate for a class that delegates the INode,
	IIterable, and ICatalog interfaces.

	@nosubgrouping
*/
class BNodeDelegate : public BGenericNode, public BnNodeObserver
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									BNodeDelegate(	const SContext& context,
													const sptr<INode>& base);
protected:
	virtual							~BNodeDelegate();
public:

			//!	Perform linking with base object.
	virtual	status_t				Link(const sptr<IBinder>& target, const SValue& bindings, uint32_t flags);
			//!	Remove linking with base object.
	virtual	status_t				Unlink(const wptr<IBinder>& target, const SValue& bindings, uint32_t flags);
			//!	Disambiguate.
	inline	SContext				Context() { return BGenericNode::Context(); }

	//@}

	// --------------------------------------------------------------
	/*!	@name INode
		Delegation to INode interface.  Uses the base implementation of
		BGenericNode for part parsing and meta-data.  Note that this default
		implementation hides all except the default meta-data entries. */
	//@{

			//!	Returns original INode object, or NULL if that interface was not available.
	inline	sptr<INode>				BaseNode() const { return m_baseNode; }

	virtual	status_t				LookupEntry(const SString& entry, uint32_t flags, SValue* node);
	virtual	SString					MimeTypeLocked() const;
	virtual	status_t				StoreMimeTypeLocked(const SString& value);
	virtual	nsecs_t					CreationDateLocked() const;
	virtual	status_t				StoreCreationDateLocked(nsecs_t value);
	virtual	nsecs_t					ModifiedDateLocked() const;
	virtual	status_t				StoreModifiedDateLocked(nsecs_t value);

	//@}

	// --------------------------------------------------------------
	/*!	@name Event Dispatching
		Handling events from the base INode object.  The default implementation
		simply pushes the same event from this object. */
	//@{

	virtual	void					NodeChanged(const sptr<INode>& node, uint32_t flags, const SValue& hints);
	virtual	void					EntryCreated(const sptr<INode>& node, const SString& name, const sptr<IBinder>& entry);
	virtual	void					EntryModified(const sptr<INode>& node, const SString& name, const sptr<IBinder>& entry);
	virtual	void					EntryRemoved(const sptr<INode>& node, const SString& name);
	virtual	void					EntryRenamed(const sptr<INode>& node, const SString& old_name, const SString& new_name, const sptr<IBinder>& entry);

	//@}

private:
									BNodeDelegate(const BNodeDelegate&);
			BNodeDelegate&			operator=(const BNodeDelegate&);

			const sptr<INode>		m_baseNode;
			bool					m_linkedToNode : 1;
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif // _STORAGE_NODEDELEGATE_H

