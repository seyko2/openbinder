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

#ifndef STORAGE_PDATABASE_H_
#define STORAGE_PDATABASE_H_

#include <support/Atom.h>
#include <support/Locker.h>
#include <support/KeyedVector.h>

#include <DataMgr.h>
#include <SchemaDatabases.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
using namespace palmos::support;
#endif

//!	Convenience class for opening and managing reference on a Data Manager database.
/*!
 * \c SDatabase is a local-only (non-Binder) convenience class that helps
 * applications manage one or more references to a single open database.
 * The underlying open database is a per-process singleton.  Multiple
 * instances of \c SDatabase in the same process which refer to the same
 * open database will increment a reference count on that database.
 * When the last \c SDatabase instance referring to a particular
 * database is freed, that database is closed again.  
 *
 * \par
 * Use GetOpenRef() to recover a \c DmOpenRef with which standard Data
 * Manager routines may be used.
 *
 * \note You may open a database dmModeWrite or \c dmModeReadWrite if you
 * wish; however, <em>no reader-writer locking of any kind</em> is
 * provided by \c SDatabase, so you'll need to control concurrent access
 * yourself.  \c SDatabase is purely a resource-management tool.
 *
 * \see DataMgr.h (for Data Manager routines), BDatabaseStore (a
 * slightly more complex database wrapper for reading resources).
 *
 * \todo Develop a concurrent version, \c SConcurrentDatabase, which
 * can perform reader-writer locking.
 */
class SDatabase
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, etc.

		The non-trivial constructors will open the database requested,
		leaving it open until all SDatabase instances referring to it
		are destroyed.

		Note that there is no default mode for DatabaseID database access,
		to prevent ambiguity with the type/creator constructor signature */
	//@{
						SDatabase();

						//!	Open a database by DatabaseID.
						SDatabase(DatabaseID dbID,
								DmOpenModeType mode, DbShareModeType share);
						
						//!	Open a database by name and creator.  
						/*!	@see DmFindDatabase() */
						SDatabase(const char *dbName, type_code dbCreator,
								DmOpenModeType mode, DbShareModeType share);
						
						//!	Open a database by type and creator.
						//*	@see DmFindDatabaseByTypeCreator() */
						SDatabase(type_code dbType, type_code dbCreator,
								DmOpenModeType mode, DbShareModeType share);

						//!	Increments the refcount on the database
						SDatabase(const SDatabase &orig);

						~SDatabase();

						//!	Copy another SDatabase into this one.
			SDatabase&	operator=(const SDatabase& o);

						//! Data Mgr error, if any, associated with opening the database.
			status_t	StatusCheck() const;
						//! Equivalent to <tt>(StatusCheck()==errNone)</tt>.
			bool		IsValid() const;

						//!	@deprecated Use StatusCheck() instead.
			status_t	Status() const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Database Access
		Retrieve the database that was opened. */
	//@{

						//! Retrieve the \c DmOpenRef for the open database.
			DmOpenRef	GetOpenRef() const;
						//! Get the database id for it
			DatabaseID	DbID() const;

	//@}

private:
	status_t			open_database(DatabaseID dbID, uint16_t attr, DmOpenModeType mode, DbShareModeType share);
	
	class PrvOpenDbAtom : public virtual SAtom
	{
		public:
						PrvOpenDbAtom(DatabaseID dbID, uint16_t attr, DmOpenModeType mode, DbShareModeType share, status_t * outResult);
			virtual		~PrvOpenDbAtom();
			DatabaseID	m_dbID;
			DmOpenRef 	m_dbRef;
			uint16_t	m_dbAttr;
	};

	sptr<PrvOpenDbAtom> m_database;
	status_t	m_dbErr;

	typedef DatabaseID dmRefTableKey;
	static SLocker s_dmRefTableLock;
	static SKeyedVector<dmRefTableKey, wptr<PrvOpenDbAtom> > s_dmRefTable;
};

#if _SUPPORTS_NAMESPACE
} } // palmos::storage
#endif

#endif // STORAGE_PDATABASE_H_
