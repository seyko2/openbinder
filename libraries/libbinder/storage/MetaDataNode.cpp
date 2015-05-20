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

#include <storage/MetaDataNode.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

B_STATIC_STRING(kCatalogMimeType, "application/vnd.palm.catalog", );

// *********************************************************************************
// ** BMetaDataNode ****************************************************************
// *********************************************************************************

BMetaDataNode::BMetaDataNode()
{
	init();
}

BMetaDataNode::BMetaDataNode(const SContext& context)
	: BGenericNode(context)
{
	init();
}

BMetaDataNode::~BMetaDataNode()
{
}

SString BMetaDataNode::MimeTypeLocked() const
{
	return m_mimeType;
}

status_t BMetaDataNode::StoreMimeTypeLocked(const SString& value)
{
	m_mimeType = value;
	return B_OK;
}

nsecs_t BMetaDataNode::CreationDateLocked() const
{
	return m_creationDate;
}

status_t BMetaDataNode::StoreCreationDateLocked(nsecs_t value)
{
	m_creationDate = value;
	return B_OK;
}

nsecs_t BMetaDataNode::ModifiedDateLocked() const
{
	return m_modifiedDate;
}

status_t BMetaDataNode::StoreModifiedDateLocked(nsecs_t value)
{
	m_creationDate = value;
	return B_OK;
}

void BMetaDataNode::TouchLocked()
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);


	SetModifiedDateLocked(((nsecs_t)t.tv_sec * B_ONE_SECOND) + t.tv_nsec);
}

void BMetaDataNode::init()
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);

	m_creationDate = m_modifiedDate = ((nsecs_t)t.tv_sec * B_ONE_SECOND) + t.tv_nsec;
}

// =================================================================================
// =================================================================================

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
