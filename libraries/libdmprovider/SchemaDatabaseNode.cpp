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

#include <dmprovider/SchemaDatabaseNode.h>

#include <support/Autolock.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace dmprovider {
#endif

B_CONST_STRING_VALUE_LARGE(kRefPath,		"os:refPath", );

// *********************************************************************************
// ** BSchemaDatabaseNode **********************************************************
// *********************************************************************************

BSchemaDatabaseNode::BSchemaDatabaseNode(const SContext& context, const SString& dbname, uint32_t creator,
	DmOpenModeType mode, DbShareModeType share, database_missing_func missing, const SValue& args)
	: BIndexedDataNode(context)
	, BnReferable(context)
	, m_dbname(dbname)
	, m_creator(creator)
	, m_mode(mode)
	, m_share(share)
	, m_dbMissing(missing)
	, m_refPath(args[kRefPath].AsString())
	, m_status(B_NO_INIT)
	, m_tableNameToIndex(B_ENTRY_NOT_FOUND)
{
}

BSchemaDatabaseNode::BSchemaDatabaseNode(const SContext& context, const SDatabase &database, const SValue& args)
	: BIndexedDataNode(context)
	, BnReferable(context)
	, m_dbname()
	, m_creator(0)
	, m_mode(0)
	, m_share(0)
	, m_dbMissing(NULL) // not useful with this ctor
	, m_refPath(args[kRefPath].AsString())
	, m_database(database)
	, m_tableNameToIndex(B_ENTRY_NOT_FOUND)
{
	m_status = m_database.Status();
}

BSchemaDatabaseNode::~BSchemaDatabaseNode()
{
}

void BSchemaDatabaseNode::InitAtom()
{
	BIndexedDataNode::InitAtom();
	if (m_status == B_NO_INIT) open_database();
	if (m_status == B_OK) retrieve_tables();
}

SValue BSchemaDatabaseNode::Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
{
	SValue res(BIndexedDataNode::Inspect(caller, which, flags));
	if (m_refPath != "") res.Join(BnReferable::Inspect(caller, which, flags));
	return res;
}

status_t BSchemaDatabaseNode::StatusCheck()
{
	return m_status;
}

size_t BSchemaDatabaseNode::CountEntriesLocked() const
{
	return m_tables.CountItems();
}

ssize_t BSchemaDatabaseNode::EntryIndexOfLocked(const SString& entry) const
{
	return m_tableNameToIndex[entry];
}

SString BSchemaDatabaseNode::EntryNameAtLocked(size_t index) const
{
	return m_tables[index]->TableName();
}

SValue BSchemaDatabaseNode::ValueAtLocked(size_t index) const
{
	return SValue::Binder((BnNode*)(m_tables[index].ptr()));
}

status_t BSchemaDatabaseNode::StoreValueAtLocked(size_t index, const SValue& value)
{
	return B_NOT_ALLOWED;
}

sptr<BSchemaTableNode> BSchemaDatabaseNode::TableAtLocked(size_t index) const
{
	return m_tables[index];
}

sptr<BSchemaTableNode> BSchemaDatabaseNode::TableForLocked(const SString& name) const
{
	const ssize_t i = m_tableNameToIndex[name];
	if (i >= 0) return TableAtLocked(i);
	return NULL;
}

SDatabase BSchemaDatabaseNode::DatabaseLocked() const
{
	return m_database;
}

sptr<BSchemaTableNode> BSchemaDatabaseNode::NewSchemaTableNodeLocked(const SString& table, const SString& refPath)
{
	return new BSchemaTableNode(Context(), m_database, table, SString(), refPath);
}

SString BSchemaDatabaseNode::Reference() const
{
	return m_refPath;
}

void BSchemaDatabaseNode::open_database()
{
	for (size_t i=0; i<2; i++) {
		m_database = SDatabase(m_dbname.String(), m_creator, m_mode, m_share);
		if (m_database.GetOpenRef() != NULL) {
			m_status = B_OK;
			return;
		}
		m_status = m_database.Status();
		if (i == 0 && m_dbMissing) {
			m_dbMissing(this); // maybe they'll create it
			continue;
		}
		DbgOnlyFatalError("[BSchemaDatabaseNode]: failed to open database!");
	}
}

void BSchemaDatabaseNode::retrieve_tables()
{
	SAutolock _l(Lock());

	const DmOpenRef ref(m_database.GetOpenRef());

	uint32_t N;
	status_t err = DbNumTables(ref, &N);
	if (err == errNone) {
		for (uint32_t i=0; i<N && err == errNone; i++) {

			char tableName[dbDBNameLength]; 
			err = DbGetTableName(ref, i, tableName);
			SString name(tableName);

			if (err == errNone && name != "") {
				SString tableRef(m_refPath);
				if (tableRef != "") tableRef.PathAppend(name);
				sptr<BSchemaTableNode> table = NewSchemaTableNodeLocked(name, tableRef);
				if (table != NULL) {
					ssize_t idx = m_tables.AddItem(table);
					if (idx >= 0) m_tableNameToIndex.AddItem(name, idx);
				}
			}
		}
	}
	m_status = err;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
