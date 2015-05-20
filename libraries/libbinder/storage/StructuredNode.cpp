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

#include <storage/StructuredNode.h>

BStructuredNode::BStructuredNode(const SContext &context, const char **entries, size_t entryCount, uint32_t mode)
	: BIndexedDataNode(context, mode)
	, m_entries(entries)
	, m_entryCount(entryCount)
{
}

BStructuredNode::~BStructuredNode()
{
}

ssize_t BStructuredNode::EntryIndexOfLocked(const SString& entry) const
{
	const size_t N = m_entryCount;
	for (size_t i=0; i<N; i++) {
		if (strcmp(entry.String(), m_entries[i]) == 0) return i;
	}
	return B_ENTRY_NOT_FOUND;
}

SString BStructuredNode::EntryNameAtLocked(size_t index) const
{
	return SString(m_entries[index]);
}

size_t BStructuredNode::CountEntriesLocked() const
{
	return m_entryCount;
}

status_t BStructuredNode::StoreValueAtLocked(size_t index, const SValue& value)
{
	return B_UNSUPPORTED;
}
