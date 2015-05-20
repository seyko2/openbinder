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

#ifndef _SUPPORT_NODE_H
#define _SUPPORT_NODE_H

/*!	@file support/Node.h
	@ingroup CoreSupportDataModel
	@brief Helper class for calling the INode interface.
*/

#include <support/Binder.h>
#include <support/ICatalog.h>
#include <support/INode.h>
#include <support/INodeObserver.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

B_CONST_STRING_VALUE_LARGE(BV_ENTRY_CREATED,		"EntryCreated", );
B_CONST_STRING_VALUE_LARGE(BV_ENTRY_MODIFIED,		"EntryModified", );
B_CONST_STRING_VALUE_LARGE(BV_ENTRY_REMOVED,		"EntryRemoved", );
B_CONST_STRING_VALUE_LARGE(BV_ENTRY_RENAMED,		"EntryRenamed", );

//!	Convenience class for operating on the INode interface.
/*!	This class provides some convenience methods for making
	calls on an INode, to provide a more convenience API.  In
	general you will create an SNode wrapper around an INode
	you have and call the SNode methods instead of making
	direct calls on the INode.  For example:

@code
	void get_something(const sptr<INode>& node)
	{
		SNode n(node).
		SValue v = n.Walk(SString("some/path"));
	}
@endcode

	When using the constructor that takes an SValue, the
	class will automatically try to retrieve an INode from
	the value.  If that fails, and the SValue contains
	mappings, then Walk() will look up the mappings inside
	of it.  This allows you to use INode::COLLAPSE_CATALOG without
	worrying about whether or not the returned item is
	collapsed.

	@todo Need to clean up the Walk() methods to get rid of
	amiguities between them.

	@nosubgrouping
*/
class SNode
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, copying, comparing, etc. */
	//@{
			//!	Create a new, empty node.
						SNode();
			//!	Retrieve a node from the given @a path in @a context.
			/*!	The @a node_flags are as per the INode::Walk() flags. */
						SNode(const SContext& context, const SString& path, uint32_t node_flags=0);
			//!	Retrieve a node from a generic SValue.
			/*!	If the SValue contains an INode object, that will be used
				directly.  Otherwise, if the SValue contains mappings,
				Walk() will perform a lookup on it.  This allows convenient
				manipulation of INode::COLLAPSE_NODE results. */
						SNode(const SValue& value);
			//!	Retrieve a node from an IBinder, casting to an INode interface.
						SNode(const sptr<IBinder>& binder);
			//!	Initialize directly from an INode.
						SNode(const sptr<INode>& node);
			//!	Copy from another SNode.
						SNode(const SNode& node);
			//!	Release reference on INode.
						~SNode();

			//!	Replace this SNode with @a o.
	SNode&				operator=(const SNode& o);

	bool				operator<(const SNode& o) const;
	bool				operator<=(const SNode& o) const;
	bool				operator==(const SNode& o) const;
	bool				operator!=(const SNode& o) const;
	bool				operator>=(const SNode& o) const;
	bool				operator>(const SNode& o) const;

			//!	Returns B_OK if we hold a value INode or SValue of mappings.
	status_t			StatusCheck() const;

			//!	Retrieve the INode object being used.
	sptr<INode>			Node() const;

			//!	Retrieve the SValue mappings being used.
	SValue				CollapsedNode() const;

			//!	@deprecated Use StatusCheck() instead.
	status_t			ErrorCheck() const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Path Walking
		Call INode::Walk() to resolve a path.  If this SNode contains
		a collapsed SValue, walk through that instead.  These functions
		take care of repeatedly calling INode::Walk() until the path
		is fully resolved or an error occurs. */
	//@{

	SValue				Walk(const SString& path, uint32_t flags = INode::REQUEST_DATA) const;
	SValue				Walk(SString* path, uint32_t flags = INode::REQUEST_DATA) const;
	SValue				Walk(const SString& path, status_t* outErr, uint32_t flags = INode::REQUEST_DATA) const;
	SValue				Walk(SString* path, status_t* outErr, uint32_t flags = INode::REQUEST_DATA) const;

	//@}

	// --------------------------------------------------------------
	/*!	@name ICatalog
		If this is also an ICatalog, call the appropriate methods.
		@todo Should be moved to a new SCatalog class. */
	//@{

	status_t			AddEntry(const SString& name, const SValue& entry) const;
	status_t			RemoveEntry(const SString& name) const;
	status_t			RenameEntry(const SString& entry, const SString& name) const;
	
	//@}

private:
	void				get_catalog() const;

	SValue				m_value;
	sptr<INode>			m_node;
	mutable sptr<ICatalog> m_catalog;	// todo: remove?
};

class BNodeObserver : public BnNodeObserver
{
public:
	BNodeObserver();
	BNodeObserver(const SContext& context);	

	virtual ~BNodeObserver();

	virtual void NodeChanged(const sptr<INode>& node, uint32_t flags, const SValue& hints);
	virtual void EntryCreated(const sptr<INode>& node, const SString& name, const sptr<IBinder>& entry);
	virtual void EntryModified(const sptr<INode>& node, const SString& name, const sptr<IBinder>& entry);
	virtual void EntryRemoved(const sptr<INode>& node, const SString& name);
	virtual void EntryRenamed(const sptr<INode>& node, const SString& old_name, const SString& new_name, const sptr<IBinder>& entry);
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif // _SUPPORT_NODE_H
