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

#ifndef _SUPPORT_DIRECTORYMIRROR_H
#define _SUPPORT_DIRECTORYMIRROR_H

/*!	@file support/CatalogMirror.h
	@ingroup CoreSupportDataModel
	@brief Create a new catalog entry based on modifications to an existing one.
*/

#include <support/ICatalogPermissions.h>
#include <support/ICatalog.h>
#include <support/IDatum.h>
#include <support/IIterable.h>
#include <support/IIterator.h>
#include <support/INode.h>
#include <support/Locker.h>
#include <support/SortedVector.h>
#include <support/KeyedVector.h>
#include <support/Observer.h>
#include <support/String.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

//! CatalogMirror is an ICatalogPermissions + ICatalog
/*!	A CatalogMirror is a combination of ICatalogPermissions
 *	and ICatalog.  When placed in the namespace, clients
 *	can access the mirror as if they are accessing the
 *	catalog itself.  Except that the mirror can have
 *	restrictions, and items hidden or overlayed.
 *
 *	When you create an CatalogMirror, you must give it
 *	the ICatalog to mirror.  Do that with either
 *	<tt>\@{catalog&nbsp;->&nbsp;icatalog_binder}</tt> or
 *	<tt>\@{catalog_path&nbsp;->&nbsp;/catalog_path}</tt>.
*/
class BCatalogMirror : public BnCatalogPermissions, public BnCatalog, public BnNode, public BnIterable, public BObserver
{
public:
	BCatalogMirror(const SContext& context, const SValue& args);

	// BCatalog overrides
	virtual	status_t			AddEntry(const SString& name, const SValue& entry);
	virtual	status_t			RemoveEntry(const SString& name);
	virtual	status_t			RenameEntry(const SString& entry, const SString& name);
	
	virtual	sptr<INode>			CreateNode(SString* name, status_t* err);
	virtual	sptr<IDatum>		CreateDatum(SString* name, uint32_t flags, status_t* err);
	virtual	sptr<IIterator>		NewIterator(const SValue& args, status_t* error = NULL);
	
	virtual	status_t			Walk(SString* path, uint32_t flags, SValue* node);

	// INode implementation
	virtual	sptr<INode> Attributes() const;
	
	virtual	SString MimeType() const;
	virtual	void SetMimeType(const SString& value);
	
	virtual	nsecs_t CreationDate() const;
	virtual	void SetCreationDate(nsecs_t value);
		
	virtual	nsecs_t ModifiedDate() const;
	virtual	void SetModifiedDate(nsecs_t value);
	
	// ICatalogPermissions implementation
	virtual	bool				Writable() const;
	virtual	void				SetWritable(bool value);
	
	virtual	status_t			HideEntry(const SString& name);
	virtual	status_t			ShowEntry(const SString& name);
	
	virtual	status_t			OverlayEntry(const SString& location, const SValue& item);
	virtual	status_t			RestoreEntry(const SString& location);
	
	virtual status_t			Merge(const sptr<ICatalog>& catalog);

	// BObserver implementation
	virtual void Observed(const SValue& key, const SValue& value);
	
	// IBinder implementation
	virtual SValue				Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags);
	virtual status_t			Link(const sptr<IBinder>& target, const SValue& bindings, uint32_t flags);
	virtual status_t			Unlink(const wptr<IBinder>& target, const SValue& bindings, uint32_t flags);

protected:
	virtual ~BCatalogMirror();

private:
	void						init_from_args(const SContext& context, const SValue& args);
	bool						is_writable(const SString& name) const;

	sptr<INode> 							m_node;
	sptr<ICatalog>							m_catalog;
	sptr<IIterable>							m_iterable;

	// NOTE: m_permissionsLock must not be held when calling in to the
	// underlying node/catalog/iterable!  This is because we use a sync
	// link to the node, and acquire this lock while processing events
	// from that link.
	mutable SLocker							m_permissionsLock;
	SSortedVector<SString>					m_hidden;
	SKeyedVector<SString, SValue>			m_overlays;
	bool									m_writable : 1;
	bool									m_linkedToNode : 1;

	class IteratorWrapper;
	friend class IteratorWrapper;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif // _SUPPORT_DIRECTORYMIRROR_H
