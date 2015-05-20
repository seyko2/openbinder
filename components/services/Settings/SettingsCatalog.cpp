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

#include <storage/ValueDatum.h>
#include <support/KeyedVector.h>
#include <support/Looper.h>
#include <support/StdIO.h>
#include <xml/Value2XML.h>
#include <xml/DataSource.h>
#include <xml/XML2ValueParser.h>
#include <xml/Parser.h>

#if !defined(OPENBINDER_SETTINGS_BUILD)
#include <services/IPowerManagement.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "Legacy.h"
#include "SettingsCatalog.h"

#if _SUPPORTS_NAMESPACE
using namespace palmos::storage;
using namespace palmos::support;
using namespace palmos::xml;
#endif

#define DEBUG_SETTINGS_CATALOG	0

B_CONST_STRING_VALUE_LARGE	(key_name,	 				"name",);
B_CONST_STRING_VALUE_LARGE	(key_legacy, 				"legacy",);
B_CONST_STRING_VALUE_LARGE	(key_system, 				"system",);
B_CONST_STRING_VALUE_LARGE	(key_date_format, 			"date_format",);
B_CONST_STRING_VALUE_LARGE	(key_entry_created, 		"EntryCreated",);
B_CONST_STRING_VALUE_LARGE	(key_timezone, 				"timezone",);

// ##### static members ############################################################

SLocker BSettingsCatalog::m_rootLock("settings_directory_root_lock");
sptr<INode> BSettingsCatalog::m_root = NULL;
sptr<BSettingsTable> BSettingsCatalog::m_table = NULL;
 
// ##### Table #####################################################################

BSettingsTable::BSettingsTable(const SContext& context, const sptr<INode>& root)
	:	BObserver(context),
		BNodeObserver(context),
		BHandler(context),
		m_lock("settings_table_lock"),
		m_databaseLock("settings_table_databaseLock"),
		m_root(root),
		m_currentCatalog(NULL),
		m_parsing(false),
		m_syncing(false),
		m_powerLinked(false),
		m_syncLinked(false)
{
}

BSettingsTable::~BSettingsTable()
{
}

void BSettingsTable::InitAtom()
{
	BHandler::InitAtom();

	SString dir = get_system_directory();
	dir.PathAppend("data");
	
	SString filename = dir;
	filename.PathAppend("settings.xml");

	m_file = new BFile();
	status_t err = m_file->SetTo(filename.String(), O_RDWR | O_CREAT);

	if (err != B_OK)
	{
		mkdir(dir.String(), 0777);
		err = m_file->SetTo(filename.String(), O_RDWR | O_CREAT);

		ErrFatalErrorIf(err != B_OK, "[settings]: could not create 'settings.xml'");
	}

	parse_xml_file();
	load_default_settings();

#if !defined(OPENBINDER_SETTINGS_BUILD)
	sptr<IBinder> power = Context().LookupService(SString("power"));
	sptr<IBinder> sync = Context().LookupService(SString("sync"));

	if (power != NULL)
	{
		power->Link((BObserver*)this, SValue(SValue::String("SleepNotify"), SValue::String("SleepNotify")), B_WEAK_BINDER_LINK);
		power->Link((BObserver*)this, SValue(SValue::String("softReset"), SValue::String("softReset")), B_WEAK_BINDER_LINK);
		m_powerLinked = true;
	}
	
	if (sync != NULL)
	{
		sync->Link((BObserver*)this, SValue(SValue::String("syncing"), SValue::String("syncing")), B_WEAK_BINDER_LINK);
		m_syncLinked = true;
	}
	
	if (!m_powerLinked || !m_syncLinked)
	{
		// link the services directory if the services were not found
		sptr<INode> node = interface_cast<INode>(Context().Lookup(SString("/services")).AsBinder());
		if (node != NULL) {
			SValue link(BV_ENTRY_CREATED, BV_ENTRY_CREATED);
			node->AsBinder()->Link((BNodeObserver*)this, link, B_WEAK_BINDER_LINK);
		}
	}
#endif
}

status_t BSettingsTable::AddEntry(const sptr<IBinder>& binder, const SString& name, const SValue& value, bool* modified, sptr<IBinder>* out_entry)
{
	SLocker::Autolock lock(m_lock);
	return add_entry_l(binder, name, value, false, modified, out_entry);
}

status_t BSettingsTable::add_entry_l(const sptr<IBinder>& binder, const SString& name, const SValue& value, bool isdir, bool* modified, sptr<IBinder>* out_entry)
{
	status_t err = B_OK;
	// if we are sync'ing create and the valuse to the cache.
	sptr<CatalogEntry> record;
	
	if (m_syncing)
	{
		record = m_cache.ValueFor(binder.ptr());
		if (record == NULL)
		{
			record = new CatalogEntry();
			m_cache.AddItem(binder.ptr(), record);
		}
	}
	else
	{
		record = m_data.ValueFor(binder.ptr());
		if (record == NULL)
		{
			record = new CatalogEntry();
			m_data.AddItem(binder.ptr(), record);
		}
	}
	
	sptr<IBinder> entry = record->data.ValueFor(name, modified);
	if (entry == NULL)
	{
		if (isdir)
		{
			err = record->data.AddItem(name, value.AsBinder());
		}
		else
		{
			sptr<IDatum> datum = new BValueDatum(value);
			err = record->data.AddItem(name, datum->AsBinder());
		}
	}
	else
	{
		if (!isdir)
		{
			// if we are syncing and there is already a binder here
			// we don't want to place the value in that binder. 
			// if we do then when the xml is parsed it will overwrite 
			// the value in the datum by what was in the xml file.
			// in write_out_cache we call the SetValue.
			if (m_syncing)
			{
				sptr<IDatum> datum = new BValueDatum(value);
				err = record->data.AddItem(name, datum->AsBinder());
			}
			else
			{
				sptr<IDatum> datum = IDatum::AsInterface(entry);
				if (datum != NULL) datum->SetValue(value);
			}
		}
	}

	// now just set the out entry to the entry for name.
	if (out_entry != NULL)
		*out_entry = record->data.ValueFor(name);

	// save only if we are not sync'ing		
	if (!m_syncing) Save();

	return (err >= 0) ? B_OK : B_ERROR;
}

status_t BSettingsTable::RemoveEntry(const sptr<IBinder>& binder, const SString& name)
{
	SLocker::Autolock lock(m_lock);

	status_t err = B_NAME_NOT_FOUND;
	bool found = false;
	
	sptr<CatalogEntry> record = m_data.ValueFor(binder.ptr(), &found);
	if (record != NULL)
	{
		ssize_t index = record->data.RemoveItemFor(name);
		err = (index >= 0) ? B_OK : B_NAME_NOT_FOUND;
	}
	
	if (m_syncing)
	{
		record = m_cache.ValueFor(binder.ptr(), &found);
		if (record != NULL)
		{
			record->data.RemoveItemFor(name);
			err = B_OK;
		}
	}
	else
	{
		Save();
	}
	
	return err;
}

status_t BSettingsTable::RenameEntry(const sptr<IBinder>& binder, const SString& entry, const SString& name, sptr<IBinder>* object)
{
	SLocker::Autolock lock(m_lock);

	status_t err = B_NAME_NOT_FOUND;
	bool found = false;
	
	sptr<CatalogEntry> record;

	record = m_data.ValueFor(binder.ptr());
	if (record != NULL)
	{
		ssize_t index = record->data.IndexOf(entry);
		if (index >= 0) {
			*object = record->data.ValueAt(index);
			record->data.RemoveItemsAt(index);
			if (!m_syncing)
			{
				record->data.AddItem(name, *object);
			}
			err = B_OK;
		} else {
			err = B_NAME_NOT_FOUND;
		}
	}
	
	if (m_syncing)
	{
		record = m_cache.ValueFor(binder.ptr());
		if (record != NULL)
		{
			ssize_t index = record->data.IndexOf(entry);
			if (index >= 0) {
				*object = record->data.ValueAt(index);
				record->data.RemoveItemsAt(index);
				record->data.AddItem(name, *object);
			}
		}
	}
	else
	{
		Save();
	}
	
	return err;
}

status_t BSettingsTable::Walk(const sptr<INode>& directory, SString* path, uint32_t flags, SValue* node)
{
	SLocker::Autolock lock(m_lock);

	// XXX add path checking here!!!!
	sptr<INode> currentDir = directory;
	status_t err;
	
	do
	{
		SString name;
		path->PathRemoveRoot(&name);		
		
		sptr<IBinder> binder = entry_for_l(currentDir->AsBinder(), name);
		
		if (binder != NULL)
		{
			if (path->Length() > 0)
			{
				currentDir = interface_cast<INode>(binder);
			}
			else if ((flags & INode::REQUEST_DATA))
			{
				sptr<IDatum> datum = IDatum::AsInterface(binder);
				if (datum != NULL)
				{
					*node = datum->Value();
				}
				else
				{
					*node = SValue::Binder(binder);
				}
			}
			else
			{
				*node = SValue::Binder(binder);
			}
			err = B_OK;
		}
		else
		{
			// check the flags
			if (flags & INode::CREATE_CATALOG)
			{
				currentDir = create_directory_l(currentDir, name, &err);
				*node = SValue::Binder(currentDir->AsBinder().ptr());
				err = B_OK;
			}
			else
			{
				err = B_ENTRY_NOT_FOUND;
			}
		}
	} while (err == B_OK && currentDir != NULL && path->Length() > 0);

	return err;
}

sptr<INode> BSettingsTable::CreateNode(const sptr<INode>& directory, const SString& name, status_t* err)
{
	SLocker::Autolock lock(m_lock);
	return create_directory_l(directory, name, err);
}

sptr<INode> BSettingsTable::create_directory_l(const sptr<INode>& directory, const SString& name, status_t* err)
{
	sptr<IBinder> binder = entry_for_l(directory->AsBinder(), name);
	if (binder != NULL)
	{
		*err = B_ENTRY_EXISTS;
		return NULL;
	}
	
	sptr<INode> dir = new BSettingsCatalog();
	if (dir == NULL)
	{
		*err = B_NO_MEMORY;
		return NULL;
	}
	
	*err = add_entry_l(directory->AsBinder(), name, SValue::Binder(dir->AsBinder()), true);
	if (*err != B_OK)
	{
		*err = B_NO_MEMORY;
		return NULL;
	}
	
	return dir;
}

bool BSettingsTable::HasEntry(const sptr<IBinder>& binder, const SString& name)
{
	SLocker::Autolock lock(m_lock);
	sptr<IBinder> datum = entry_for_l(binder, name);
	return (binder != NULL) ? true : false;
}

status_t BSettingsTable::EntryAtLocked(const sptr<IBinder>& binder, size_t index, uint32_t flags, SValue* key, SValue* entry)
{
	SLocker::Autolock lock(m_lock);

	bool found = false;
	sptr<CatalogEntry> record;
	if (m_syncing)
	{
		record = m_cache.ValueFor(binder.ptr(), &found);
	}

	if (!found)
	{
		record = m_data.ValueFor(binder.ptr(), &found);
	}

	status_t err = B_END_OF_DATA;
	if (found && (record != NULL) && (record->data.CountItems() > index))
	{
		*key = SValue::String(record->data.KeyAt(index));
		*entry = record->data.ValueAt(index);
		
		if ((flags & INode::REQUEST_DATA))
		{
			sptr<IDatum> datum = IDatum::AsInterface(*entry);
			if (datum != NULL)
			{
				*entry = datum->Value();
			}
		}
		err = B_OK;
	}

	return err;
}

size_t BSettingsTable::CountEntriesLocked(const sptr<IBinder>& binder)
{
	SLocker::Autolock lock(m_lock);

	bool found = false;
	sptr<CatalogEntry> record;
	if (m_syncing)
	{
		record = m_cache.ValueFor(binder.ptr(), &found);
	}

	if (!found)
	{
		record = m_data.ValueFor(binder.ptr(), &found);
	}
	size_t count = 0;
	if (found && (record != NULL))
		count = record->data.CountItems();
	return count;
}


sptr<IBinder> BSettingsTable::entry_for_l(const sptr<IBinder>& binder, const SString& name)
{
	bool found = false;
	sptr<IBinder> entry;

	// if we are sync'ing then check to see if it is in the cache
	if (m_syncing)
	{
		sptr<CatalogEntry> record = m_cache.ValueFor(binder.ptr(), &found);
		
		if (found && (record != NULL))
		{
			entry = record->data.ValueFor(name, &found);
		}
	}
	
	// we are not sync'ing or it was not found in the cache
	if (!found)
	{
		sptr<CatalogEntry> record = m_data.ValueFor(binder.ptr(), &found);
		if (found && (record != NULL))
		{
			entry = record->data.ValueFor(name, &found);
		}
	}
	return entry;
}

void BSettingsTable::load_default_settings()
{
	// reload the timezone information
	int fd = open("/etc/timezone", O_RDONLY);

	if (fd != -1)
	{
		SValue value;
		SString path(key_system);
		status_t err = this->Walk(m_root, &path, INode::CREATE_CATALOG, &value);

		if (err == B_OK)
		{
			char buf[1024];
			ssize_t amount = read(fd, buf, 1024);

			if (amount >= 0)
			{
				buf[amount - 1] = '\0';
				add_entry_l(value.AsBinder(), key_timezone, SValue::String(buf));
			}

		}
	}
}

void BSettingsTable::parse_xml_file()
{
	// Serialize database access.
	SLocker::Autolock dblock(m_databaseLock);

	size_t size = m_file->Size();
	if (size > 0) {
		char* buffer = (char*)malloc(size);
		ssize_t count = m_file->ReadAt(0, buffer, (size_t)size);

		if (count > 0)
		{
			m_parsing = true;
			ParseXML(this, new BXMLBufferSource(buffer, size), B_XML_DONT_EXPAND_CHARREFS);
			m_parsing = false;
			
			free(buffer);
		}
	}
}

void BSettingsTable::write_xml_file()
{
	// Serialize database access.
	SLocker::Autolock dblock(m_databaseLock);

	sptr<BStringIO> string = new BStringIO();

	{
		// Hold the data lock while building the new XML data, so
		// nobody can modify it while we do so.  Note that we MUST
		// NOT hold this lock later when calling in to the Data
		// Manager, because it may end up calling back into us to
		// retrieve timezone information.
		SLocker::Autolock lock(m_lock);

		sptr<ITextOutput> xml = (BTextOutput*)string.ptr();
		
		xml << "<settings>" << endl;
		catalog_to_xml(m_root->AsBinder(), string);
		xml << "</settings>" << endl;
	}

#if DEBUG_SETTINGS_CATALOG
	bout << "[BSettingsCatalog]: writing xml: " << endl;
	bout << string->String() << endl;
#endif

	if (m_file != NULL) {
		m_file->SetSize(string->StringLength());
		m_file->WriteAt(0, string->String(), string->StringLength());
	}
}

void BSettingsTable::catalog_to_xml(const sptr<IBinder>& binder, const sptr<BStringIO>& string)
{
	sptr<ITextOutput> xml = (BTextOutput*)string.ptr();
	sptr<IByteOutput> out = (BnByteOutput*)string.ptr();

	const sptr<CatalogEntry>& record = m_data.ValueFor(binder.ptr());
	if (record == NULL) return;
	
	size_t size = record->data.CountItems();
	for (size_t i = 0 ; i < size ; i++)
	{
		SString key = record->data.KeyAt(i);
		sptr<IBinder> binder = record->data.ValueAt(i);
		if (binder == NULL) continue;

		sptr<ICatalog> directory = ICatalog::AsInterface(binder);
		if (directory == NULL)
		{
			sptr<IDatum> datum = IDatum::AsInterface(binder);
			if (datum == NULL)
			{
				ErrFatalError("datum should not be null!");
				continue;
			}

			SValue joined(SValue::String(key), datum->Value());
			status_t err = ValueToXML(out, joined);
			xml << endl;
		}
		else
		{
			// recursively parse the catalog.
			xml << "<catalog name=\"" << key << "\">" << endl;
			catalog_to_xml(binder.ptr(), string);
			xml << "</catalog>" << endl;
		}
	}
}

status_t BSettingsTable::OnStartTag(SString& tag, SValue& attributes, sptr<BCreator>& newCreator)
{
	if (tag == "settings")
	{
		m_currentCatalog = m_root;
	}
	else if (tag == "catalog")
	{
		SString name = attributes[key_name].AsString();
		
		SString path = name;
		SValue value;
		status_t err = this->Walk(m_currentCatalog, &path, INode::CREATE_CATALOG, &value);
	
		sptr<INode> dir = interface_cast<INode>(value);
		m_stack.Push(m_currentCatalog);
		m_currentCatalog = dir;
	}
	else if (tag == "value")
	{
		newCreator = new BXML2ValueCreator(m_value, attributes);
	}

	return B_OK;
}

status_t BSettingsTable::OnEndTag(SString& tag)
{
	if (tag == "catalog")
	{
		if (m_stack.CountItems() > 0)
		{
			m_currentCatalog = m_stack.Top();
			m_stack.Pop();
		}
	}
	else if (tag == "value")
	{
		SLocker::Autolock _l(m_lock);

		SValue key;
		SValue value;
		void* cookie = NULL;
		while (m_value.GetNextItem(&cookie, &key, &value) == B_OK)
		{
			add_entry_l(m_currentCatalog->AsBinder(), key.AsString(), value, false);
		}
		
		m_value = SValue::Undefined();
	}
	
	return B_OK;
}

status_t BSettingsTable::OnText(SString& name)
{
	return B_OK;
}

void BSettingsTable::EntryCreated(const sptr<INode>& node, const SString& name, const sptr<IBinder>& binder)
{
#if !defined(OPENBINDER_SETTINGS_BUILD)
	if (name == "power")
	{
		binder->Link((BObserver*)this, SValue(SValue::String("SleepNotify"), SValue::String("SleepNotify")), B_WEAK_BINDER_LINK);
		binder->Link((BObserver*)this, SValue(SValue::String("softReset"), SValue::String("softReset")), B_WEAK_BINDER_LINK);
		m_powerLinked = true;
	}
	else if (name == "sync")
	{
		binder->Link((BObserver*)this, SValue(SValue::String("syncing"), SValue::String("syncing")), B_WEAK_BINDER_LINK);
		m_syncLinked = true;
	}
	
	if (m_powerLinked && m_syncLinked)
	{
		// now unlink the services directory
		sptr<IBinder> directory = Context().Lookup(SString("/services")).AsBinder();
		SValue link(BV_ENTRY_CREATED, BV_ENTRY_CREATED);
		directory->Unlink((BNodeObserver*)this, link, B_WEAK_BINDER_LINK);
	}
#endif
}

void BSettingsTable::Observed(const SValue& key, const SValue& value)
{
	if (key == SValue::String("SleepNotify") ||
		key == SValue::String("softReset"))
	{
		RemoveMessages(BSettingsTable::SAVE, B_FILTER_FUTURE_FLAG);
		// if we have anything in the cache then write it out.
		m_syncing = false;
		write_out_cache();
		write_xml_file();
	}
	else if (key == SValue::String("syncing"))
	{
		m_lock.Lock();
		m_syncing = value.AsBool();
		m_lock.Unlock();
		
		if (m_syncing)
		{
			// starting a sync write out the current settings.
			RemoveMessages(BSettingsTable::SAVE, B_FILTER_FUTURE_FLAG);
			write_xml_file();

			// XXX HACK ALERT XXX
			// I really don't have any idead how to do this right now
			// but i need to save the current calibration data so it
			// does not get overwritten by restore. In the future I
			// would like to have an attribute set on the directory
			// saying not to restore it.
	
			SValue calibrate;
			SValue pressure;
			SString path;
			
			path = "system/calibrate";
			status_t err = m_root->Walk(&path, 0, &calibrate);
			if (err != B_OK) return;
			
			path = "system/calibrate/pressure";
			err = m_root->Walk(&path, 0, &pressure);
			if (err != B_OK) return;

			size_t size;
			sptr<CatalogEntry> caliRecord = m_data.ValueFor(calibrate.AsBinder().ptr());
			sptr<CatalogEntry> nucali = new CatalogEntry();
			size = caliRecord->data.CountItems();
			for (size_t i = 0 ; i < size ; i++)
			{
				sptr<IDatum> datum = IDatum::AsInterface(caliRecord->data.ValueAt(i));
				if (datum != NULL)
				{
					sptr<IDatum> nudatum = new BValueDatum(datum->Value());
					nucali->data.AddItem(caliRecord->data.KeyAt(i), nudatum->AsBinder());
				}
			}
			
			if (size > 0)
			{
				m_cache.AddItem(calibrate.AsBinder().ptr(), nucali);
			}
			
			sptr<CatalogEntry> presRecord = m_data.ValueFor(pressure.AsBinder().ptr());
			sptr<CatalogEntry> nupres = new CatalogEntry();
			size = presRecord->data.CountItems();
			for (size_t i = 0 ; i < size ; i++)
			{
				sptr<IDatum> datum = IDatum::AsInterface(presRecord->data.ValueAt(i));
				if (datum != NULL)
				{
					sptr<IDatum> nudatum = new BValueDatum(datum->Value());
					nupres->data.AddItem(presRecord->data.KeyAt(i), nudatum->AsBinder());
				}
			}
			
			if (size > 0)
			{
				m_cache.AddItem(pressure.AsBinder().ptr(), nupres);
			}
		}
		else
		{
			parse_xml_file();
			write_out_cache();
			write_xml_file();
		}
	}
}

void BSettingsTable::write_out_cache()
{
	size_t cacheSize = m_cache.CountItems();
	for (size_t i = 0 ; i < cacheSize ; i++)
	{
		sptr<IBinder> binder = m_cache.KeyAt(i);
		sptr<CatalogEntry> cacheRecord = m_cache.ValueAt(i);
		sptr<CatalogEntry> record = m_data.ValueFor(binder.ptr());

		// if a directory entry exists for a directory then loop through
		// either adding the datums or if a datum already exists the just
		// filling in the blanks. else just added the directory entry.
		if (record != NULL)
		{
			size_t size = cacheRecord->data.CountItems();

			for (size_t j = 0 ; j < size ; j++)
			{
				sptr<IBinder> entry = record->data.ValueFor(cacheRecord->data.KeyAt(j));

				if (entry == NULL)
				{
					record->data.AddItem(cacheRecord->data.KeyAt(j), cacheRecord->data.ValueAt(j));
				}
				else
				{
					sptr<IDatum> to = IDatum::AsInterface(entry);
					sptr<IDatum> from = IDatum::AsInterface(cacheRecord->data.ValueAt(j));
					if (to != NULL && from != NULL) to->SetValue(from->Value());	
				}
			}
		}
		else
		{
			m_data.AddItem(binder.ptr(), cacheRecord);
		}
	}

	m_cache.MakeEmpty();
}

void BSettingsTable::Save()
{
	if (!m_parsing)
	{
		if (CountMessages(BSettingsTable::SAVE) == 0)
		{
			PostDelayedMessage(SMessage(BSettingsTable::SAVE), B_MILLISECONDS(500));
		}
	}
}

status_t BSettingsTable::HandleMessage(const SMessage& msg)
{
	switch (msg.What())
	{
		case BSettingsTable::SAVE:
		{
			write_xml_file();
			break;
		}		
	}

	return B_OK;
}

// ##### SettingsCatalog #########################################################

BSettingsCatalog::BSettingsCatalog()
{	
}

BSettingsCatalog::~BSettingsCatalog()
{	
}

sptr<INode> BSettingsCatalog::CreateRoot()
{
	SLocker::Autolock lock(m_rootLock);
	if (m_root == NULL)
	{
		m_root = new BSettingsCatalog();
		m_table = new BSettingsTable(get_default_context(), m_root); 
	}

	return m_root;
}

sptr<BSettingsTable> BSettingsCatalog::Table()
{
	return m_table;
}

status_t BSettingsCatalog::AddEntry(const SString& name, const SValue& value)
{
	bool modified = false;
	sptr<IBinder> entry;
	status_t err = BSettingsCatalog::Table()->AddEntry((BnNode*)this, name, value, &modified, &entry);

	// send either the modified or creation event
	if (modified)
		PushEntryModified(this, name, entry);
	else
		PushEntryCreated(this, name, entry);
	
	return err;
}

status_t BSettingsCatalog::RemoveEntry(const SString& name)
{
	status_t err = BSettingsCatalog::Table()->RemoveEntry((BnNode*)this, name);
	PushEntryRemoved(this, name);
	return err;
}

status_t BSettingsCatalog::RenameEntry(const SString& entry, const SString& name)
{
	sptr<IBinder> object;
	status_t err = BSettingsCatalog::Table()->RenameEntry((BnNode*)this, entry, name, &object);
	if (err == B_OK) {
		PushEntryRenamed(this, entry, name, object);
	}
	return err;
}

sptr<INode> BSettingsCatalog::CreateNode(SString* name, status_t* err)
{
	sptr<INode> dir = BSettingsCatalog::Table()->CreateNode(this, *name, err);	
	if (dir != NULL) {
		PushEntryCreated(this, *name, dir->AsBinder());
	}
	return dir;
}

sptr<IDatum> BSettingsCatalog::CreateDatum(SString* name, uint32_t flags, status_t* err)
{
	*err = B_UNSUPPORTED;
	return NULL;
}

status_t BSettingsCatalog::EntryAtLocked(size_t index, uint32_t flags, SValue* key, SValue* entry)
{
	return BSettingsCatalog::Table()->EntryAtLocked((BnNode*)this, index, flags, key, entry);
}

size_t BSettingsCatalog::CountEntriesLocked() const
{
	return BSettingsCatalog::Table()->CountEntriesLocked((BnNode*)this);
}

status_t BSettingsCatalog::LookupEntry(const SString& entry, uint32_t flags, SValue* node)
{
	return B_UNSUPPORTED;
}

status_t BSettingsCatalog::Walk(SString* path, uint32_t flags, SValue* node)
{
	return BSettingsCatalog::Table()->Walk(this, path, flags, node);
}
