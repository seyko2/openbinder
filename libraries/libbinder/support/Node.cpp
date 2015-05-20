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

#include <support/Node.h>

#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// =================================================================================

SNode::SNode()
{
}

SNode::SNode(const SContext& context, const SString& path, uint32_t node_flags)
{
	if (path.Length() > 0)
	{
		m_node = context.Root().ptr();
		if (path != "/")
		{
			// now we walk to the path. Note that we have to have
			// m_node set to the root or this walk will fail.
			SValue value = this->Walk(path, node_flags);
			m_node = INode::AsInterface(value);
		}
	}
}

SNode::SNode(const SValue& value)
	: m_value(value)
	, m_node(interface_cast<INode>(value))
{
}

SNode::SNode(const sptr<IBinder>& binder)
	: m_node(interface_cast<INode>(binder))
{
}

SNode::SNode(const sptr<INode>& node)
	: m_node(node)
{
}

SNode::SNode(const SNode& node)
	: m_value(node.m_value)
	, m_node(node.m_node)
	, m_catalog(node.m_catalog)
{
}

SNode::~SNode()
{
}

SNode& SNode::operator=(const SNode& o)
{
	m_value = o.m_value;
	m_node = o.m_node;
	m_catalog = o.m_catalog;
	return *this;
}

bool SNode::operator<(const SNode& o) const
{
	return m_node != NULL ? m_node < o.m_node : m_value < o.m_value;
}

bool SNode::operator<=(const SNode& o) const
{
	return m_node != NULL ? m_node <= o.m_node : m_value <= o.m_value;
}

bool SNode::operator==(const SNode& o) const
{
	return m_node != NULL ? m_node == o.m_node : m_value == o.m_value;
}

bool SNode::operator!=(const SNode& o) const
{
	return m_node != NULL ? m_node != o.m_node : m_value != o.m_value;
}

bool SNode::operator>=(const SNode& o) const
{
	return m_node != NULL ? m_node >= o.m_node : m_value >= o.m_value;
}

bool SNode::operator>(const SNode& o) const
{
	return m_node != NULL ? m_node > o.m_node : m_value > o.m_value;
}

status_t SNode::StatusCheck() const
{
	return m_node != NULL ? B_OK : (m_value.IsSimple() ? B_ERROR : B_OK);
}

status_t SNode::ErrorCheck() const
{
	return StatusCheck();
}

sptr<INode> SNode::Node() const
{
	return m_node;
}

SValue SNode::CollapsedNode() const
{
	return m_value;
}

status_t FindLeaf(sptr<INode> *node, SString *pathName)
{
	SValue entry;
	status_t outErr = B_OK;
	SString path;

	pathName->PathGetParent(&path);

	while (true)
	{
		entry.Undefine();
		outErr = (*node)->Walk(&path, 0, &entry);
		if (outErr != B_OK || path.Length() == 0) {
			// Success or failure, terminate and return result.
			*pathName = SString(pathName->PathLeaf());
			break;
		}
		(*node) = INode::AsInterface(entry);
		if ((*node) == NULL) {
			// We were not able to completely traverse the path,
			// but the node hierarchy didn't return an error
			// code.  This should not happen...  but if it does,
			// generate a reasonable error.
			outErr = B_ENTRY_NOT_FOUND;
			break;
		}
	}

	return outErr;
}

status_t SNode::AddEntry(const SString& name, const SValue& entry) const
{
	if (name.FindFirst(*name.PathDelimiter()) < 0) {
		if (m_catalog != NULL) return m_catalog->AddEntry(name, entry);
		get_catalog();
		return (m_catalog != NULL) ? m_catalog->AddEntry(name, entry) : B_UNSUPPORTED;
	}

	sptr<INode> node = m_node;
	SString path(name);

	if (node != NULL) {
		status_t err = FindLeaf(&node,&path);
		if (err != B_OK) return err;

		sptr<ICatalog> catalog = ICatalog::AsInterface(m_node->AsBinder());
		return (catalog != NULL) ? catalog->AddEntry(path, entry) : B_UNSUPPORTED;
	}
	
	return B_UNSUPPORTED; // We don't yet support updating values in an SValue...
}

status_t SNode::RemoveEntry(const SString& name) const
{
	if (name.FindFirst(*name.PathDelimiter()) < 0) {
		if (m_catalog != NULL) return m_catalog->RemoveEntry(name);
		get_catalog();
		return (m_catalog != NULL) ? m_catalog->RemoveEntry(name) : B_UNSUPPORTED;
	}

	sptr<INode> node = m_node;
	SString path(name);

	if (node != NULL) {
		status_t err = FindLeaf(&node,&path);
		if (err != B_OK) return err;

		sptr<ICatalog> catalog = ICatalog::AsInterface(m_node->AsBinder());
		return (catalog != NULL) ? catalog->RemoveEntry(path) : B_UNSUPPORTED;
	}
	
	return B_UNSUPPORTED; // We don't yet support removing values in an SValue...
}

status_t SNode::RenameEntry(const SString& name, const SString& rename) const
{
	if (name.FindFirst(*name.PathDelimiter()) < 0) {
		if (m_catalog != NULL) return m_catalog->RenameEntry(name, rename);
		get_catalog();
		return (m_catalog != NULL) ? m_catalog->RenameEntry(name, rename) : B_UNSUPPORTED;
	}
 
	SString parentName,parentRename;
	name.PathGetParent(&parentName);
	rename.PathGetParent(&parentRename);
	if (parentName != parentRename) {
		/*	We don't "yet" support renaming (really moving) entries
			across different nodes. */
		return B_UNSUPPORTED;		
	}

	sptr<INode> node = m_node;
	SString path(name);

	if (node != NULL) {
		status_t err = FindLeaf(&node,&path);
		if (err != B_OK) return err;

		sptr<ICatalog> catalog = ICatalog::AsInterface(m_node->AsBinder());
		return (catalog != NULL) ? catalog->RenameEntry(path, SString(rename.PathLeaf())) : B_UNSUPPORTED;
	}
	
	return B_UNSUPPORTED; // We don't yet support renaming values in an SValue...
}

SValue SNode::Walk(const SString& path, uint32_t flags) const
{
	SString tmp(path);
	status_t err;
	return Walk(&tmp, &err, flags);
}

SValue SNode::Walk(SString* path, uint32_t flags) const
{
	status_t err;
	return Walk(path, &err, flags);
}

SValue SNode::Walk(const SString& path, status_t* outErr, uint32_t flags) const
{
	SString tmp(path);
	return Walk(&tmp, outErr, flags);
}

SValue SNode::Walk(SString* path, status_t* outErr, uint32_t flags) const
{
	if (path->Length() == 0 || *path == "/")
	{
		// Requesting root node.
		*outErr = B_OK;
		if (m_node != NULL)
			return SValue::Binder(m_node->AsBinder());
		return m_value;
	}

	SValue entry;
	sptr<INode> node = m_node;

	if (node != NULL) {
walk_path:
		while (true)
		{
			entry.Undefine();
			*outErr = node->Walk(path, flags, &entry);
			if (*outErr != B_OK || path->Length() == 0) {
				// Success or failure, terminate and return result.
				break;
			}
			node = INode::AsInterface(entry);
			if (node == NULL) {
				// We were not able to completely traverse the path,
				// but the node hierarchy didn't return an error
				// code.  This should not happen...  but if it does,
				// generate a reasonable error.
				*outErr = B_ENTRY_NOT_FOUND;
				return SValue::Undefined();
			}
		}

	} else if (m_value.IsDefined()) {
		entry = m_value[SValue::String(*path)];
		if (entry.IsDefined()) {
			*outErr = B_OK;
		} else {
			// Try to walk a path.
			{
				SString name;
				path->PathRemoveRoot(&name);
				entry = m_value[SValue::String(name)];
			}

			// If this is a path with multiple names, then we can
			// try to walk into it.
			if (path->Length() > 0) {
				node = interface_cast<INode>(entry);
				if (node != NULL) {
					goto walk_path;
				}
			}

			*outErr = B_ENTRY_NOT_FOUND;
		}
	} else {
		*outErr = B_NO_INIT;
	}

	// we do not want to return an SValue that 
	// has garbage in it if we had an error
	if (*outErr != B_OK) return SValue::Undefined();
	
	return entry;
}

void SNode::get_catalog() const
{
	m_catalog = ICatalog::AsInterface(m_node->AsBinder());
}

// ==================================================================================

BNodeObserver::BNodeObserver()
{
}

BNodeObserver::BNodeObserver(const SContext& context)
	:	BnNodeObserver(context)
{
}

BNodeObserver::~BNodeObserver()
{
}

void BNodeObserver::NodeChanged(const sptr<INode>& node, uint32_t flags, const SValue& hints)
{
}

void BNodeObserver::EntryCreated(const sptr<INode>& node, const SString& name, const sptr<IBinder>& entry)
{
}

void BNodeObserver::EntryModified(const sptr<INode>& node, const SString& name, const sptr<IBinder>& entry)
{
}

void BNodeObserver::EntryRemoved(const sptr<INode>& node, const SString& name)
{
}

void BNodeObserver::EntryRenamed(const sptr<INode>& node, const SString& old_name, const SString& new_name, const sptr<IBinder>& entry)
{
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
