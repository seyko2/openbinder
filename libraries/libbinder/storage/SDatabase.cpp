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

#include <storage/SDatabase.h>

#include <support/Autolock.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::storage;
#endif

// Only used if the table key is <type><creator> in 64-bits
// #define MAKE_TABLE_KEY(_t_, _c_) \
//	((uint64_t)(((uint64_t)_t_ << 4) | (uint64_t)_c_))

#define MAKE_TABLE_KEY(_id_) ((dmRefTableKey)(_id_))

#define DEBUGME 0
#ifdef DEBUGME
#  define MY_DEBUG_ONLY(x) x
#else
#  define MY_DEBUG_ONLY(x) 
#endif

SDatabase::PrvOpenDbAtom::PrvOpenDbAtom(DatabaseID dbID, uint16_t attr, DmOpenModeType mode, DbShareModeType share, status_t * outResult)
	: SAtom()
	, m_dbID(dbID)
	, m_dbAttr(attr)
{
	if (attr & dmHdrAttrSchema) {
		m_dbRef = DbOpenDatabase(dbID, mode, share);
	} else {
		m_dbRef = DmOpenDatabase(dbID, mode);
	}
	*outResult = (m_dbRef == 0) ? DmGetLastErr() : errNone;
}

SDatabase::PrvOpenDbAtom::~PrvOpenDbAtom()
{
	if (m_dbRef) {
		if (m_dbAttr & dmHdrAttrSchema)
			DbCloseDatabase(m_dbRef);
		else
			DmCloseDatabase(m_dbRef);
	}
}
	
SLocker SDatabase::s_dmRefTableLock("SDatabase reftable lock");
SKeyedVector<SDatabase::dmRefTableKey, wptr<SDatabase::PrvOpenDbAtom> > SDatabase::s_dmRefTable;

SDatabase::SDatabase()
	: m_database(NULL)
	, m_dbErr(sysErrNoInit)
{
}

SDatabase::SDatabase(const char *dbName, type_code dbCreator, DmOpenModeType mode, DbShareModeType share)
	: m_database(NULL)
	, m_dbErr(sysErrNoInit)
{
	DmDatabaseInfoType infoStruct;
	memset(&infoStruct, 0, sizeof(infoStruct));
	infoStruct.size = sizeof(infoStruct);
	uint16_t attrs;
	infoStruct.pAttributes = &attrs;
	DatabaseID dbID = DmFindDatabase(dbName, dbCreator, dmFindAllDB, &infoStruct);
	if (dbID) {
		m_dbErr = open_database(dbID, attrs, mode, share);
	} else {
		m_dbErr = DmGetLastErr();
//        bout << "SDatabase: Could not find database: \"" << dbName << "\"/'" <<
//            dbCreator << "': " << (void*)m_dbErr << "\n";
	}
}

SDatabase::SDatabase(DatabaseID dbID, DmOpenModeType mode, DbShareModeType share)
	: m_database(NULL)
	, m_dbErr(sysErrNoInit)
{
	DmDatabaseInfoType infoStruct;
	memset(&infoStruct, 0, sizeof(infoStruct));
	uint16_t attrs;
	infoStruct.pAttributes = &attrs;
	DmDatabaseInfo(dbID, &infoStruct);
	m_dbErr = open_database(dbID, attrs, mode, share);
}

SDatabase::SDatabase(type_code dbType, type_code dbCreator, DmOpenModeType mode, DbShareModeType share)
	: m_database(NULL)
	, m_dbErr(sysErrNoInit)
{
	DmDatabaseInfoType infoStruct;
	memset(&infoStruct, 0, sizeof(infoStruct));
	uint16_t attrs;
	infoStruct.pAttributes = &attrs;
	DatabaseID dbID = DmFindDatabaseByTypeCreator(dbType, dbCreator, dmFindAllDB, &infoStruct);

	if (dbID != NULL) {
		m_dbErr = open_database(dbID, attrs, mode, share);
	} else {
		m_dbErr = DmGetLastErr();
//        bout << "SDatabase: Could not find database: " << dbType << "/" <<
//            dbCreator << ": " << (void*)DmGetLastErr() << "\n";
	}
}

SDatabase::SDatabase(const SDatabase &orig)
	:m_database(orig.m_database),
	 m_dbErr(orig.m_dbErr)
{
}

SDatabase::~SDatabase()
{

}

SDatabase& SDatabase::operator=(const SDatabase& o)
{
	m_database = o.m_database;
	m_dbErr = o.m_dbErr;
	return *this;
}

status_t
SDatabase::open_database(DatabaseID dbID, uint16_t attrs, DmOpenModeType mode, DbShareModeType share)
{
	SAutolock(s_dmRefTableLock.Lock());

	status_t result = B_OK;
	dmRefTableKey myKey = MAKE_TABLE_KEY( dbID );

	size_t index;
	bool found = s_dmRefTable.GetIndexOf(myKey, &index);

	if (found) {
		m_database = s_dmRefTable.ValueAt(index).promote();
	}

	if (!m_database.ptr()) {
		m_database = new SDatabase::PrvOpenDbAtom(dbID, attrs, mode, share, &result);
		if (result == B_OK) {
			s_dmRefTable.AddItem(myKey, m_database);
		}
	} else {
		result = errNone;
	}
	return result;
}

status_t
SDatabase::StatusCheck() const
{
	return m_dbErr;
}

status_t
SDatabase::Status() const
{
	return StatusCheck();
}

bool
SDatabase::IsValid() const
{
	return (m_dbErr == errNone);
}

DmOpenRef
SDatabase::GetOpenRef() const
{
	if (m_database.ptr())
		return m_database->m_dbRef;
	return NULL;
}

DatabaseID
SDatabase::DbID() const
{
	if (m_database.ptr())
		return m_database->m_dbID;
	return NULL;
}
