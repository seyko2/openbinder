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

#include <storage/BDatabaseStore.h>

#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

BDatabaseStore::BDatabaseStore(DatabaseID db, int32_t restype, DmResourceID resid) :
	m_db(db, dmModeReadOnly, dbShareNone)
{
	init();
	open_resource(restype, resid);
}

BDatabaseStore::BDatabaseStore(int32_t type, int32_t creator, int32_t restype, DmResourceID resid) :
	m_db(type, creator, dmModeReadOnly, dbShareNone)
{
	init();
	open_resource(restype, resid);
}

status_t BDatabaseStore::ErrorCheck() const
{
	status_t dbStatus = m_db.Status();
	return m_mem == NULL ? (dbStatus == errNone ? B_ERROR : dbStatus) : B_OK;
}

BDatabaseStore::~BDatabaseStore()
{
	if (m_mem) DmHandleUnlock(m_handle);
	if (m_handle) DmReleaseResource(m_handle);
}

void BDatabaseStore::init()
{
	m_handle = NULL;
	m_mem = NULL;
	m_length = 0;
}

void BDatabaseStore::open_resource(int32_t restype, DmResourceID resid)
{
	if (m_db.IsValid()) {
		m_handle = DmGetResource(m_db.GetOpenRef(), restype, resid);
		m_mem = m_handle ? DmHandleLock(m_handle) : NULL;
		if (m_mem != NULL) {
			m_length = DmHandleSize(m_handle);
			SetSize(m_length);
		}
	}
}

void * BDatabaseStore::MoreCore(void *oldBuf, size_t newsize)
{
	if (newsize <= m_length) return m_mem;
	return NULL;
}

DatabaseID BDatabaseStore::DbID() const
{
	return m_db.DbID();
}

#if _SUPPORTS_NAMESPACE
} }
#endif
