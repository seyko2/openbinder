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

#ifndef _SETTINGS_CATALOG_H
#define _SETTINGS_CATALOG_H

#include <storage/File.h>
#include <support/Autolock.h>
#include <support/Catalog.h>
#include <support/Handler.h>
#include <support/KeyedVector.h>
#include <support/Node.h>
#include <support/Observer.h>
#include <support/Package.h>
#include <support/String.h>
#include <support/StringIO.h>
#include <support/Vector.h>

#include <xml/Parser.h>
#include <xml/Writer.h>
#include <xml/XMLParser.h>

#include <services/IInformant.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::storage;
using namespace palmos::support;
using namespace palmos::services;
#endif

class BSettingsCatalog;

class BSettingsTable : public BObserver, public BNodeObserver, public BHandler, public BCreator
{
public:
	BSettingsTable(const SContext& context, const sptr<INode>& root);
	virtual ~BSettingsTable();

	virtual void InitAtom();
	virtual void EntryCreated(const sptr<INode>& node, const SString& name, const sptr<IBinder>& entry);
	virtual status_t HandleMessage(const SMessage& msg);
	
	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);

	void Save();

	status_t AddEntry(const sptr<IBinder>& binder, const SString& name, const SValue& value, bool* modified, sptr<IBinder>* entry);
	status_t RemoveEntry(const sptr<IBinder>& binder, const SString& name);
	status_t RenameEntry(const sptr<IBinder>& binder, const SString& entry, const SString& name, sptr<IBinder>*object);
	status_t Walk(const sptr<INode>& dir, SString* path, uint32_t flags, SValue* node);
	sptr<INode> CreateNode(const sptr<INode>& directory, const SString& name, status_t* err);

	status_t EntryAtLocked(const sptr<IBinder>& binder, size_t index, uint32_t flags, SValue* key, SValue* entry);
	size_t CountEntriesLocked(const sptr<IBinder>& binder);

	
	bool HasEntry(const sptr<IBinder>& binder, const SString& name);
	inline const SContext& Context() const { return BObserver::Context(); }

protected:
	virtual void Observed(const SValue& key, const SValue& value);

private:
	enum
	{
		SAVE = 0x42,
	};

	// This class shouldn't be public, but currently has to be in
	// order to placate ADS.
public:
	class CatalogEntry : public SLightAtom
	{
	public:
		SKeyedVector<SString, sptr<IBinder> > data;
	};
private:
	
	void parse_xml_file();
	void write_xml_file();
	void save();
	void catalog_to_xml(const sptr<IBinder>& binder, const sptr<BStringIO>& string);
	
	void load_default_settings();

	void write_out_cache();

	status_t add_entry_l(const sptr<IBinder>& binder,
						 const SString& name,
						 const SValue& value,
						 bool isdir = false,
						 bool* modified = NULL,
						 sptr<IBinder>* out_entry = NULL);

	sptr<IBinder> entry_for_l(const sptr<IBinder>& binder, const SString& name);
	sptr<INode> create_directory_l(const sptr<INode>& directory, const SString& name, status_t* err);

	SLocker m_lock;
	SLocker m_databaseLock;
	SKeyedVector<IBinder*, sptr<CatalogEntry> > m_data;
	SKeyedVector<IBinder*, sptr<CatalogEntry> > m_cache;
	
	sptr<INode> m_root;
	
	// xml parsing state
	SVector<sptr<INode> > m_stack;
	sptr<INode> m_currentCatalog;
	SValue m_value;
	
	bool m_parsing;
	bool m_syncing;
	
	bool m_powerLinked;
	bool m_syncLinked;

	sptr<BFile> m_file;
};


class BSettingsCatalog : public BIndexedCatalog, public SPackageSptr
{
public:
	BSettingsCatalog();
	virtual ~BSettingsCatalog();

	static sptr<INode> CreateRoot();

	// ICatalog
	virtual status_t AddEntry(const SString& name, const SValue& value);
	virtual	status_t RemoveEntry(const SString& name);
	virtual	status_t RenameEntry(const SString& entry, const SString& name);
	virtual	sptr<INode> CreateNode(SString* name, status_t* err);
	virtual	sptr<IDatum> CreateDatum(SString* name, uint32_t flags, status_t* err);
	virtual	status_t Walk(SString* path, uint32_t flags, SValue* node);

	// BIndexedCatalog
	virtual status_t EntryAtLocked(size_t index, uint32_t flags, SValue* key, SValue* entry);
	virtual size_t CountEntriesLocked() const;
	virtual status_t LookupEntry(const SString& entry, uint32_t flags, SValue* node);

protected:
	// these static methods are not locked. m_root and m_table get initialized in CreateRoot();
	static sptr<INode> Root();
	static sptr<BSettingsTable> Table();

	static SLocker m_rootLock;
	static sptr<INode> m_root;
	static sptr<BSettingsTable> m_table;
};

#endif // _SETTINGS_CATALOG_H
