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

#ifndef	_STORAGE_BDATABASE_STORE_H
#define	_STORAGE_BDATABASE_STORE_H

#include <support/MemoryStore.h>
#include <storage/SDatabase.h>

#include <DataMgr.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

class BDatabaseStore : public BMemoryStore
{
public:

						BDatabaseStore(DatabaseID db, int32_t restype, DmResourceID resid);
						BDatabaseStore(int32_t type, int32_t creator, int32_t restype, DmResourceID resid);

			status_t	ErrorCheck() const;

	DatabaseID			DbID() const;

protected:
	virtual				~BDatabaseStore();

private:
						BDatabaseStore(const BDatabaseStore&);

			void		init();
			void		open_resource(int32_t restype, DmResourceID resid);

	virtual	void*		MoreCore(void *oldBuf, size_t newsize);

			SDatabase	m_db;
			MemHandle	m_handle;
			MemPtr		m_mem;
			size_t		m_length;
};

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif /* _STORAGE_BDATABASE_STORE_H */
