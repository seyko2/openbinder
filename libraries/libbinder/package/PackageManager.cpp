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

#include <package_p/PackageManager.h>

#include <storage/ValueDatum.h>

#include <support/Iterator.h>
#include <support/KernelStreams.h>
#include <support/Looper.h>
#include <support/MemoryStore.h>
#include <support/Package.h>
#include <support/RegExp.h>
#include <support/StdIO.h>
#include <support/String.h>
#include <xml/Value2XML.h>
#include <xml/Parser.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
              
#if TARGET_HOST == TARGET_HOST_LINUX
#include <dirent.h>
#include <sys/mman.h>
#endif

#if _SUPPORTS_NAMESPACE
using namespace palmos::package;
using namespace palmos::services;
using namespace palmos::support;
#endif

B_CONST_STRING_VALUE_LARGE(key_file, "file", )
B_CONST_STRING_VALUE_LARGE(key_type, "type", )
B_CONST_STRING_VALUE_LARGE(key_local, "local", )
B_CONST_STRING_VALUE_LARGE(key_package, "package", )
B_CONST_STRING_VALUE_LARGE(key_version, "version", )
B_CONST_STRING_VALUE_LARGE(key_modified, "modified", )
B_CONST_STRING_VALUE_LARGE(key_entry_created, "EntryCreated", )
B_CONST_STRING_VALUE_LARGE(key_packagedir, "packagedir", )
B_CONST_STRING_VALUE_LARGE(key_components, "components", )
B_CONST_STRING_VALUE_LARGE(key_path, "path", )
B_CONST_STRING_VALUE_LARGE(key_dbtype, "dbtype", )
B_CONST_STRING_VALUE_LARGE(key_dbcreator, "dbcreator", )
B_CONST_STRING_VALUE_LARGE(key_dbID, "dbID", )

B_CONST_STRING_VALUE_LARGE(BV_APPLICATIONS, "applications", )
B_CONST_STRING_VALUE_LARGE(BV_COMPONENTS, "components", )
B_CONST_STRING_VALUE_LARGE(BV_DATABASES, "databases", )
B_CONST_STRING_VALUE_LARGE(BV_PACKAGES, "packages", )
B_CONST_STRING_VALUE_LARGE(BV_ADDONS, "addons", )

B_CONST_STRING_VALUE_LARGE(BV_MANIFEST, "Manifest.xml", )

struct cached_file_info
{
	SString path;			// must be set by caller.
	
	SString lastRelFile;
	SString lastPackage;
	SString lastAbsFile;
	time_t lastModTime;
};

static bool make_file_absolute(cached_file_info* ci, SValue* info)
{	
	SString file = (*info)[key_file].AsString();
	SString package = (*info)[key_package].AsString();
	SString filePath(ci->path);
	
	if (file.Length()>0 && file.ByteAt(0) != '/')
	{
		// First, check to see if we already know about this file.
		int code = 0;
		if (ci->lastRelFile != file || ci->lastPackage != package)
		{
			filePath.PathAppend(package);
			filePath.PathAppend(file);

			struct stat info;
			code = stat(filePath.String(), &info);

			if (code < 0) {
				filePath.Append(".so");
				// now check to make sure that the file actually exists
				code = stat(filePath.String(), &info);
			}
			if (code >= 0)
			{
				ci->lastAbsFile = filePath;
				ci->lastModTime = info.st_mtime;
				ci->lastRelFile = file;
				ci->lastPackage = package;
			}
		}
		
		if (code >= 0)
		{
			info->Overlay(SValue(key_file, SValue::String(ci->lastAbsFile)));
			info->Overlay(SValue(key_modified, SValue::Int32(ci->lastModTime)));
			return true;
		}
	}

#if BUILD_TYPE != BUILD_TYPE_RELEASE
	berr << "[PackageManager]: Failed to find '" << filePath << "'. Local file = '" << file << "'" << endl;
#endif

	return false;
}

// ==================================================================================
// ==================================================================================
// ==================================================================================

class QueryCatalog : public BMetaDataCatalog
{
public:
	QueryCatalog(BPackageManager::Data* data);

	virtual	status_t AddEntry(const SString& name, const SValue& entry);
	virtual	status_t RemoveEntry(const SString& name);
	virtual	status_t RenameEntry(const SString& entry, const SString& name);
	virtual	sptr<ICatalog> CreateCatalog(SString* name, status_t* err);
	virtual	sptr<IDatum> CreateDatum(SString* name, uint32_t flags, status_t* err);
	virtual status_t EntryAtLocked(size_t index, uint32_t flags, SValue* key, SValue* entry);
	virtual size_t CountEntriesLocked() const;
	virtual status_t LookupEntry(const SString& entry, uint32_t flags, SValue* node);
private: 
	BPackageManager::Data* m_data;
};

QueryCatalog::QueryCatalog(BPackageManager::Data* data)
	:	BMetaDataCatalog(SContext())
	,	m_data(data)
{
}

// ==================================================================================
// ==================================================================================
// ==================================================================================

status_t QueryCatalog::AddEntry(const SString& name, const SValue& entry)
{
	return B_UNSUPPORTED;
}

status_t QueryCatalog::RemoveEntry(const SString& name)
{
	SLocker::Autolock lock(m_data->lock);
	sptr<BPackageManager::Query> query = m_data->queries.ValueFor((BnNode*)this);
	if (query == NULL) return B_ERROR;
	query->datums.RemoveItemFor(name);
	ssize_t size = query->entries.RemoveItemFor(name);
	return (size >= 0) ? B_OK : B_ENTRY_NOT_FOUND;
}

status_t QueryCatalog::RenameEntry(const SString& entry, const SString& name)
{
	return B_UNSUPPORTED;
}

sptr<ICatalog> QueryCatalog::CreateCatalog(SString* name, status_t* err)
{
	*err = B_UNSUPPORTED;
	return NULL;
}

sptr<IDatum> QueryCatalog::CreateDatum(SString* name, uint32_t flags, status_t* err)
{
	*err = B_UNSUPPORTED;
	return NULL;
}
status_t QueryCatalog::EntryAtLocked(size_t index, uint32_t flags, SValue* key, SValue* entry)
{
	SLocker::Autolock data_lock(m_data->lock);
	sptr<BPackageManager::Query> query = m_data->queries.ValueFor((BnNode*)this);
	
	if (query == NULL) return B_END_OF_DATA;
	if (index >= query->entries.CountItems()) return B_END_OF_DATA;
	
	*key = SValue::String(query->entries.KeyAt(index));
	*entry = query->entries.ValueAt(index);

	if (!(flags & INode::REQUEST_DATA))
	{
		sptr<IDatum> datum = query->datums.ValueFor(key->AsString()).promote();
		if (datum == NULL)
		{
			datum = new BValueDatum(*entry, IDatum::READ_ONLY);
			query->datums.AddItem(key->AsString(), datum);
		}
		*entry = SValue::Binder(datum->AsBinder());
	}
	
	return B_OK;
}

size_t QueryCatalog::CountEntriesLocked() const
{
	sptr<BPackageManager::Query> query = m_data->queries.ValueFor((BnNode*)this);
	if (query == NULL) return 0;
	return query->entries.CountItems();
}

status_t QueryCatalog::LookupEntry(const SString& entry, uint32_t flags, SValue* node)
{
	SLocker::Autolock lock(m_data->lock);
	
	sptr<BPackageManager::Query> query = m_data->queries.ValueFor((BnNode*)this);
	if (query == NULL) return B_ENTRY_NOT_FOUND;

	bool found = false;
	*node = query->entries.ValueFor(entry, &found);
//	bout << "***BPM::QD::Walk() name = " << name << endl;
	if (!(flags & INode::REQUEST_DATA))
	{
		// look up the datum
		sptr<IDatum> datum = query->datums.ValueFor(entry).promote();
		if (datum == NULL)
		{
			datum = new BValueDatum(*node, IDatum::READ_ONLY);
			query->datums.AddItem(entry, datum);
		}
		*node = SValue::Binder(datum->AsBinder());
	}
	
	return (found) ? B_OK : B_ENTRY_NOT_FOUND;
}

// ==================================================================================
// ==================================================================================
// ==================================================================================

class PackageManifestParser : public SManifestParser
{
public:
	/* Read manifest data into internal catalog */
	PackageManifestParser(const SString& path,
							  BPackageManager::Data* data,
							  const sptr<BPackageManager::Package>& package,
							  const sptr<BPackageManager>& manager,
							  const SValue& metadata = SValue::Undefined(),
							  bool verbose = false);

	/* Read manifest data into an SValue, for the caller. This does
	   not read any data into the internal catalog, an so can be used to probe
	   packages that aren't yet registered. */
	PackageManifestParser(SValue& output, bool verbose = false);
		
	
	virtual void OnDeclareAddon(const SValue &info);
	virtual void OnDeclareApplication(const SValue &info);
	virtual void OnDeclareComponent(const SValue &info);
	
private:
	BPackageManager::Data* m_data;
	sptr<BPackageManager::Package> m_package;
	sptr<BPackageManager> m_manager;
	SValue m_metadata;
	bool m_verbose;
	
	cached_file_info m_ci;
};

PackageManifestParser::PackageManifestParser(
			const SString& path,
			BPackageManager::Data* data,
		    const sptr<BPackageManager::Package>& package,
			const sptr<BPackageManager>& manager,
			const SValue& metadata,
			bool verbose)
	:	m_data(data),
		m_package(package),
		m_manager(manager),
		m_metadata(metadata),
		m_verbose(verbose)
{
	m_ci.path = path;
}

PackageManifestParser::PackageManifestParser(
			SValue& output,
			bool verbose)
	:	m_data(NULL),
		m_package(NULL),
		m_manager(NULL),
		m_metadata(SValue::Undefined()),
		m_verbose(verbose)
{
}

struct component_version_info
{
	component_version_info()
	{
	}

	component_version_info(const sptr<BPackageManager::Component>& component)
	{
		version = component->Version();
		localLength = component->Local().Length();
		modified = component->Modified();
	}

	int32_t compare_strict(const component_version_info& o) const
	{
		if (version != o.version) return version > o.version ? 1 : -1;
		if (localLength != o.localLength) return localLength > o.localLength ? 1 : -1;
		return 0;
	}

	int32_t compare(const component_version_info& o) const
	{
		const int32_t res = compare_strict(o);
		if (res) return res;
		if (modified != o.modified) return modified > o.modified ? 1 : -1;
		return 0;
	}

	uint32_t version;
	int32_t localLength;
	time_t modified;
};


void PackageManifestParser::OnDeclareComponent(const SValue &info)
{
	if (info.IsDefined() && !m_package.is_null())
	{
		SValue componentInfo(m_metadata);
		componentInfo.Overlay(info);
		
		SString local(info[key_local].AsString());
		SString id(info[key_package].AsString());
		
		if (local.Length() != 0) {
			id.Append('.', 1);
			id.Append(local);
		}

		if (make_file_absolute(&m_ci, &componentInfo) == false) {
			berr << "Skipping component " << id << " because its implementation couldn't be found." << endl;
			return;
		}
		
		// bout << "componentInfo=" << componentInfo << endl;
		
		id.Pool();
		componentInfo.Pool();

		sptr<BPackageManager::Component> new_component = new BPackageManager::Component(m_package, id, componentInfo);
		//bout << "Adding component " << id << ": " << new_component->Version() << endl;
		m_package->AddComponent(new_component);

		// XXX MOVE THIS INTO THE QUERY!

		{
			SLocker::Autolock(m_data->lock);
			sptr<BPackageManager::Component> old_component = m_data->components.ValueFor(id);
			if (old_component != NULL)
			{

				/*
					If "existingLocal" is shorter than "local" then the new one is a
					"less specific" one, and we should use whatever is already there.
					
					Example:
						id:				org.openbinder.widgets.Button
						local:			widgets.Button	(therefore in package: palmos)
						existingLocal:	Button			(therefore in package: org.openbinder.widgets)
					... or ...
						id:				org.openbinder.widgets.Button
						local:							(therefore in package: org.openbinder.widgets.Button)
						existingLocal:	Button			(therefore in package: org.openbinder.widgets)
						Use the new one because it's package is more specific
					
					We know this will work because for the ids to match the names must
					also match.
				*/
				const component_version_info newVersion(new_component);
				component_version_info oldVersion(old_component);

				// Find where to put the component based on its version.
				sptr<BPackageManager::Component> pos = old_component;
				sptr<BPackageManager::Component> prev_pos;
				bool found_dup = false;
				while (pos != NULL && newVersion.compare(oldVersion) < 0) {
					if (newVersion.compare_strict(oldVersion) == 0) {
						if (!found_dup) {
							printf("*** Package manager: Ambiguous component '%s'\n", id.String());
							sptr<BPackageManager::Package> p = pos->GetPackage();
							if (p != NULL) {
								printf("*** Package manager: Using '%s' for component.\n", p->Name().String());
							}
							found_dup = true;
						} else {
							sptr<BPackageManager::Package> p = pos->GetPackage();
							if (p != NULL) {
								printf("*** Package manager: Skipping '%s' for component.\n", p->Name().String());
							}
						}
					}
					prev_pos = pos;
					pos = pos->Prev();
					if (pos != NULL) {
						oldVersion = component_version_info(pos);
					}
				}

#if 0
				if (pos != NULL || prev_pos != NULL) {
					printf("*** Package manager: Component %s has duplicates.\n", id.String());
					printf("*** Package manager: new ts=%d, ver=%d, name=%s.\n", m_package->Modified(), m_package->DatabaseVersion(), m_package->Name().String());
					sptr<BPackageManager::Package> oldp = pos != NULL ? pos->GetPackage() : prev_pos->GetPackage();
					if (oldp != NULL) {
						printf("*** Package manager: old ts=%d, ver=%d, name=%s.\n", oldp->Modified(), oldp->DatabaseVersion(), oldp->Name().String());
					}
				}
#endif

				if (pos != NULL) {
					// Not at end of list.
					new_component->SetPrev(pos);
					pos->SetNext(new_component);

					if (found_dup) {
						printf("*** Package manager: Skipping '%s' for component.\n", m_package->Name().String());
					}
				}
				if (prev_pos != NULL) {
					// Not at front of list.
					new_component->SetNext(prev_pos);
					prev_pos->SetPrev(new_component);
					if (newVersion.compare_strict(oldVersion) == 0) {
						if (!found_dup) {
							printf("*** Package manager: Ambiguous component '%s'\n", id.String());
							printf("*** Package manager: Using '%s' for component.\n", m_package->Name().String());
							sptr<BPackageManager::Package> p = prev_pos->GetPackage();
							if (p != NULL) {
								printf("*** Package manager: Skipping '%s' for component.\n", p->Name().String());
							}
							found_dup = true;
						} else {
							printf("*** Package manager: Skipping '%s' for component.\n", m_package->Name().String());
						}
					}
				} else {
					// at front -- we want the new component when we look at the entries
					m_data->components.AddItem(id, new_component);
				}
			}
			else
			{
				// there wasn't an old component so just add it.
				m_data->components.AddItem(id, new_component);
			}
		}
		
		if (m_verbose) bout << "Adding component " << id << ": " << componentInfo << endl;
		
	}
}

void PackageManifestParser::OnDeclareApplication(const SValue &info)
{
	if (info.IsDefined() && !m_package.is_null())
	{
		SValue componentInfo(m_metadata);
		componentInfo.Overlay(info);
		
		SString local(info[key_local].AsString());
		SString id(info[key_package].AsString());
		
		if (make_file_absolute(&m_ci, &componentInfo) == false) return;
		// bout << "componentInfo=" << componentInfo << endl;
		
		if (local.Length() != 0) {
			id.Append('.', 1);
			id.Append(local);
		}

		id.Pool();
		componentInfo.Pool();

		sptr<BPackageManager::Component> new_component = new BPackageManager::Component(m_package, id, componentInfo);
		//bout << "Adding application " << id << ": " << new_component->Version() << endl;
		m_data->applications.AddItem(id, new_component);
	}
}

void PackageManifestParser::OnDeclareAddon(const SValue& info)
{
	if (info.IsDefined() && !m_package.is_null())
	{
		SValue componentInfo(m_metadata);
		componentInfo.Overlay(info);
		
		SString local(info[key_local].AsString());
		SString id(info[key_package].AsString());
		
		if (make_file_absolute(&m_ci, &componentInfo) == false) return;
		// bout << "componentInfo=" << componentInfo << endl;
		
		if (local.Length() != 0) {
			id.Append('.', 1);
			id.Append(local);
		}
		
		id.Pool();
		componentInfo.Pool();

		sptr<BPackageManager::Component> new_component = new BPackageManager::Component(m_package, id, componentInfo);
		m_data->addons.AddItem(id, new_component);
	}
}


// ==================================================================================
// ==================================================================================
// ==================================================================================

BPackageManager::Component::Component(const sptr<BPackageManager::Package>& package, const SString& id, const SValue& value)
	:	m_package(package.ptr()),
		m_id(id),
		m_local(value[key_local].AsString()),
		m_value(value),
		m_lock("Component Lock"),
		m_next(NULL),
		m_prev(NULL)
{
	//bout << "Hello component: " << m_id << endl;
	if (m_package) m_package->IncWeak(this);
}

BPackageManager::Component::~Component()
{
	if (m_package) m_package->DecWeak(this);
	//bout << "Goodbye component: " << m_id << endl;
}

const sptr<BPackageManager::Package> BPackageManager::Component::GetPackage() const
{
	sptr<BPackageManager::Package> p;
	if (m_package && m_package->AttemptIncStrong(this)) {
		p = static_cast<BPackageManager::Package*>(m_package);
		m_package->DecStrong(this);
	}
	return p;
}

const SString& BPackageManager::Component::Id() const
{
	return m_id;
}

const SString& BPackageManager::Component::Local() const
{
	return m_local;
}

uint32_t BPackageManager::Component::Version() const
{
	return m_value[key_version].AsInt32();
}

time_t BPackageManager::Component::Modified() const
{
	return m_value[key_modified].AsInt32();
}

const SValue& BPackageManager::Component::Value() const
{
	return m_value;
}

sptr<BPackageManager::Component> BPackageManager::Component::Next() const
{
	SLocker::Autolock lock(m_lock);
	return m_next;
}

sptr<BPackageManager::Component> BPackageManager::Component::Prev() const
{
	SLocker::Autolock lock(m_lock);
	return m_prev;
}

void BPackageManager::Component::SetNext(const sptr<BPackageManager::Component>& next)
{
	SLocker::Autolock lock(m_lock);
	m_next = next;
}

void BPackageManager::Component::SetPrev(const sptr<BPackageManager::Component>& prev)
{
	SLocker::Autolock lock(m_lock);
	m_prev = prev;
}

void BPackageManager::Component::Remove()
{
	if (m_next != NULL) m_next->SetPrev(m_prev);
	if (m_prev != NULL) m_prev->SetNext(m_next);
	
	m_next = NULL;
	m_prev = NULL;
}

// ==================================================================================
// ==================================================================================
// ==================================================================================

BPackageManager::Package::Package(const SString& path, const SString& name)
	:	m_path(path),
		m_name(name),
		m_dbid(0)
{
}

BPackageManager::Package::~Package()
{
	//bout << "Goodbye package: " << m_name << endl;

	// Components have circular references, so we need to
	// manually break them. :(

	const size_t N = m_components.CountItems();
	for (size_t i=0; i<N; i++) {
		sptr<BPackageManager::Component> component = m_components[i];
		//bout << "Unlinking component: " << component->Id() << endl;
		if (component != NULL) {
			if (component->Next() != NULL) {
				component->Next()->SetPrev(component->Prev());
			}
			if (component->Prev() != NULL) {
				component->Prev()->SetNext(component->Next());
			}
			component->SetNext(NULL);
			component->SetPrev(NULL);
		}
	}
}

SString BPackageManager::Package::Path() const
{
	return m_path;
}

SString BPackageManager::Package::Name() const
{
	return m_name;
}

uint32_t BPackageManager::Package::DbID() const
{
	return m_dbid;
}

ssize_t BPackageManager::Package::AddComponent(const sptr<BPackageManager::Component>& component)
{
	SLocker::Autolock lock(m_lock);

	if (component->Value()["package"].AsString() != m_name)
	{
#if BUILD_TYPE != BUILD_TYPE_RELEASE
		bout << "Gack: " << component->Value() << endl;
#endif
		ErrFatalError("Adding a component that is not from this package!");
		return B_ERROR;
	}

	return m_components.AddItem(component);
}

void BPackageManager::Package::RemoveComponentAt(size_t index)
{
	SLocker::Autolock lock(m_lock);
	m_components.RemoveItemsAt(index);
}

size_t BPackageManager::Package::CountComponents() const
{
	SLocker::Autolock lock(m_lock);
	return m_components.CountItems();
}

const sptr<BPackageManager::Component>& BPackageManager::Package::ComponentAt(size_t index) const
{
	SLocker::Autolock lock(m_lock);
	return m_components.ItemAt(index);
}

void BPackageManager::Package::SetDbID(uint32_t dbid)
{
	m_dbid = dbid;
}

// ==================================================================================
// ==================================================================================
// ==================================================================================

BPackageManager::Data::Data()
{
}

void BPackageManager::Data::add_package_l(const sptr<BPackageManager::Package>& package)
{
	packages.AddItem(package);
}

sptr<BPackageManager::Package> BPackageManager::Data::remove_package_l(const SValue& info)
{
	return NULL;
}

// ==================================================================================
// ==================================================================================
// ==================================================================================

BPackageManager::BPackageManager(const SContext& context)
	:	BCatalog(context),
		m_nextDatabaseID(1)
{
}

BPackageManager::~BPackageManager()
{
}

void BPackageManager::InitAtom()
{
	BCatalog::InitAtom();

	AddQuery(SString("interfaces"), SString("interface"));

	const char* paths = getenv("BINDER_PACKAGE_PATH");
	if (paths) {
		const char* end;
		do {
			end = strchr(paths, ':');
			SString dir(paths, end ? end-paths : strlen(paths));
			if (dir != "") {
				m_packagesPath.AddItem(dir);
				bout << "[PackageManager]: adding package directory '" << dir << "'" << endl;
			}
			paths = end+1;
		} while (end);
	} else {
		SString dir(get_system_directory()); //opt/palmos
		dir.PathAppend("packages");
		m_packagesPath.AddItem(dir);
		bout << "[PackageManager]: setting package directory '" << dir << "'" << endl;
	}
}

uint32_t BPackageManager::NextDatabaseID()
{
	uint32_t prev = (uint32_t)SysAtomicInc32((volatile int32_t*)&m_nextDatabaseID);
	DbgOnlyFatalErrorIf(prev&0xff000000, "WARNING: BPackageManager::m_nextDatabaseID wrapped");
	return (prev & 0x00ffffff) | 0x80000000;
}

status_t BPackageManager::AddQuery(const SString& dirName, const SString& property)
{
	SLocker::Autolock lock(m_data.lock);
	sptr<QueryCatalog> dir = new QueryCatalog(&m_data);
	sptr<Query> query = new Query();
	query->property = property;
	m_data.queries.AddItem((BnNode*)dir.ptr(), query);
	AddEntry(dirName, SValue::Binder((BnNode*)dir.ptr()));
	return B_OK;
}

sptr<BPackageManager::Package> BPackageManager::scan_for_packages(const SString& packagesPath, const SString& pkgName, const SString& filePath, int32_t file, const SValue& info, bool verbose)
{
	sptr<BPackageManager::Package> package;

	// create a package with the the leaf as the package name. 
	// i.e /opt/palmsource/package/org.openbinder.foo.bar would be package palm.foo.bar
	package = new BPackageManager::Package(info[key_packagedir].AsString(), pkgName);
	if (package == NULL)
	{
		berr << "[PackageManager]: Could not create package for '" << pkgName << "'" << endl;
		return NULL;
	}

	sptr<IByteInput> input = new BKernelIStr(file);
	SManifestParser::ParseManifest(filePath, new PackageManifestParser(packagesPath, &m_data, package, this, info), input, B_NO_PACKAGE, pkgName);

	// now add the package
	m_data.lock.Lock();
	m_data.add_package_l(package);
	m_data.lock.Unlock();
	
	return package;
}

struct collect_properties_args
{
	sptr<BPackageManager::Query> query;
	SString component;
	SString* path;
};

static void collect_properties(const collect_properties_args& args, const SValue& info)
{
	SString name;
	args.path->PathRemoveRoot(&name);

	if (name == "*") {
		// Wildcard means to collect everything found here.
		name = *args.path;
		SValue key, value;
		void* cookie = NULL;
		while (info.GetNextItem(&cookie, &key, &value) == B_OK)
		{
			collect_properties(args, value);
			// The collect_properties() call eats up the path,
			// so we need to set it back to the original value
			// in case of further matches.
			*args.path = name;
		}
		return;
	}

	SValue prop(info[name]);
	if (!prop.IsDefined()) {
		// Whoops, that's it.
		return;
	}

	if ((*args.path) != "") {
		// Need to dig deeper into the value structure.
		name = "";
		collect_properties(args, prop);
		return;
	}

	// Hit the end, collect up all of the properties.
	SValue key, value;
	void* cookie = NULL;
	while (prop.GetNextItem(&cookie, &key, &value) == B_OK)
	{
		SString name = value.AsString();
		if (name != "")
		{
			bool found = false;
			name.ReplaceAll('/', ':');	// names can't have a '/' in them.
			SValue& list = args.query->entries.EditValueFor(name, &found);
			if (found) list.Join(SValue::String(args.component));
			else args.query->entries.AddItem(name, SValue::String(args.component));
		}
	}
}

void build_db_entry(const sptr<BCatalog>& databaseCatalog, uint32_t dbID, const SString& file)
{
	char entryName[9];
	sprintf(entryName, "%08x", dbID);
	
	SValue v(key_dbID, SValue::Int32(dbID));
	v.JoinItem(key_file, SValue::String(file));

	databaseCatalog->AddEntry(SString(entryName), v);
}

uint32_t BPackageManager::look_for_databases(const SString& dirpath, const sptr<BCatalog>& databaseCatalog)
{
	DIR* dir = opendir(dirpath.String());

	uint32_t result = 0;

	if (dir != NULL)
	{
		struct dirent* dent = readdir(dir);

		SRegExp regexp(".*.[bo]?prc$");

		while (dent != NULL)
		{
			SString path(dirpath);
			path.PathAppend(dent->d_name);

			if (regexp.Matches(dent->d_name))
			{
				result = NextDatabaseID();
//				bout << "*** Adding prc '" << path << "' result = " << (void*)result << endl;
				build_db_entry(databaseCatalog, result, path);
			}
			else
			{
				if (dent->d_name[0] != '.' || (dent->d_name[1] != '\0' && dent->d_name[1] != '.'))
				{
					struct stat sb;
					int err = stat(path.String(), &sb);

					if (S_ISDIR(sb.st_mode))
					{
						look_for_databases(path, databaseCatalog);
					}
				}
			}
			dent = readdir(dir);
		}

		closedir(dir);
	}

	return result;
}

void BPackageManager::run_queries()
{
	SLocker::Autolock lock(m_data.lock);

	// build up the /packages/components and /packages/packages

	sptr<BCatalog> appCatalog = new BCatalog();
	AddEntry(BV_APPLICATIONS, SValue::Binder((BnNode*)appCatalog.ptr()));

	sptr<BCatalog> componentCatalog = new BCatalog();
	AddEntry(BV_COMPONENTS, SValue::Binder((BnNode*)componentCatalog.ptr()));
	
	sptr<BCatalog> databaseCatalog = new BCatalog();
	AddEntry(BV_DATABASES, SValue::Binder((BnNode*)databaseCatalog.ptr()));
	
	sptr<BCatalog> pkgCatalog = new BCatalog();
	AddEntry(BV_PACKAGES, SValue::Binder((BnNode*)pkgCatalog.ptr()));
	
	sptr<BCatalog> addonCatalog = new BCatalog();
	AddEntry(BV_ADDONS, SValue::Binder((BnNode*)addonCatalog.ptr()));


	const size_t PACKAGES = m_data.packages.CountItems();
	for (size_t i = 0 ; i < PACKAGES ; i++)
	{
		sptr<Package> pkg = m_data.packages.ItemAt(i);

		SValue value;
		value.JoinItem(key_path, SValue::String(pkg->Path()));

#if BUILD_TYPE != BUILD_TYPE_RELEASE
		// For debug builds add a list of componets to the package entry
		SValue components;
		const size_t count = pkg->CountComponents();
		for (size_t j = 0 ; j < count ; j++)
		{
			components.Join(SValue::String(pkg->ComponentAt(j)->Local()));
		}
		value.JoinItem(key_components, components);
#endif

		SString path(pkg->Path());
		path.PathAppend("resources");
//		bout << "look for resource in '" << path << "'" << endl;
		uint32_t dbid = look_for_databases(path, databaseCatalog);
		pkg->SetDbID(dbid);

		pkgCatalog->AddEntry(pkg->Name(), value);
	}
	
	
	const size_t COMPONENTS = m_data.components.CountItems();
	for (size_t i = 0 ; i < COMPONENTS ; i++)
	{
		sptr<Component> component = m_data.components.ValueAt(i);
		SValue value = component->Value();
		componentCatalog->AddEntry(component->Id(), value);
	}

	const size_t APPS = m_data.applications.CountItems();
	for (size_t i = 0 ; i < APPS ; i++)
	{
		sptr<Component> component = m_data.applications.ValueAt(i);
		SValue value = component->Value();
		
		SString path(value[key_packagedir].AsString());
		path.PathAppend("resources");
		
		uint32_t dbid = component->GetPackage()->DbID();
		value.JoinItem(key_dbID, SSimpleValue<uint32_t>(dbid));

//		bout << component->Id() << " default dbid = " << (void*)dbid << endl;
		appCatalog->AddEntry(component->Id(), value);
	}
	
	const size_t PLUGINS = m_data.addons.CountItems();
	for (size_t i = 0 ; i < PLUGINS ; i++)
	{
		sptr<Component> component = m_data.addons.ValueAt(i);
		SValue value = component->Value();
		addonCatalog->AddEntry(component->Id(), value);
	}

	SString prcPath(get_system_directory());
	prcPath.PathAppend("PRC");
	look_for_databases(prcPath, databaseCatalog);

	const size_t QUERIES = m_data.queries.CountItems();
	size_t i, j;

	// Erase current data in queries!  It would be much preferrable
	// to be smarter when a component is removed and only change
	// the things it actually impacts...  but for now, we'll live.
	for (j = 0 ; j < QUERIES ; j++)
	{
		sptr<Query> query = m_data.queries.ValueAt(j);
		query->entries.MakeEmpty();
		// XXX When re-running a query, we shouldn't remove
		// any active datums for entries that don't change.
		// oooops...
		query->datums.MakeEmpty();
	}

	collect_properties_args args;
	for (i = 0 ; i < COMPONENTS ; i++)
	{
		args.component = m_data.components.KeyAt(i);
		SValue info = m_data.components.ValueAt(i)->Value();

		for (j = 0 ; j < QUERIES ; j++)
		{
			args.query = m_data.queries.ValueAt(j);
			SString pathcpy(args.query->property);
			args.path = &pathcpy;
			collect_properties(args, info);
		}
	}
}

void BPackageManager::Start(bool verbose)
{
	// build up a list of directories to search

	size_t N = m_packagesPath.CountItems();
	for (size_t i=0; i<N; i++) {
		DIR* dir = opendir(m_packagesPath[i].String());
		if (dir != NULL)
		{
			struct dirent* outerDent = readdir(dir);
		
			while (outerDent != NULL)
			{
				if (outerDent->d_name[0] == '.' || outerDent->d_name[1] == '.')
				{
					outerDent = readdir(dir);
					continue;
				}

				SString filePath(m_packagesPath[i]);
				filePath.PathAppend(outerDent->d_name);
				SString packagePath = filePath;
				filePath.PathAppend(BV_MANIFEST);
						
				int file = open(filePath.String(), O_RDONLY);
				if (file != -1)
				{
					SString name(outerDent->d_name);
					int32_t index = name.FindLast(".");
					if (index >= 0)
					{
						name.Remove(0, index+1);
					}
				
					SValue info;
					info.JoinItem(key_file, SValue::String(name));
					info.JoinItem(key_packagedir, SValue::String(packagePath));
					info.Pool();

					scan_for_packages(m_packagesPath[i], SString(outerDent->d_name), filePath, file, info, verbose);
					close(file);
				}
				else
				{
					berr << "[PackageManager]: could not open '" << filePath << "'" << endl;
				}
					
				outerDent = readdir(dir);
			}
			closedir(dir);
		}
		else
		{
			berr << "[PackageManager]: Could not open directory '" << m_packagesPath[i] << "'." << endl; 
		}
	}
	
	run_queries();
}

// FIXME: There are some serious problems with SIGCHLD handling: really, we should have a single
// thread that is always handling them, even for waitpid() purposes.

/* Take a package file (.kpg aka a cramfs image) and unpack it to a folder. That folder should
   already exist, as should the file. The results of doing this should be "good enough" for using
   and executing from that package folder, and similar to mounting the image.
 */
void BPackageManager::UnpackPackageFileToFolder(const SString & packageFile, const SString & folder)
{
	char folderStr[1024];
	char packageFileStr[1024];

	strcpy(folderStr, folder.String());
	strcpy(packageFileStr, packageFile.String());
	
	int child_pid;
	
	sigset_t chld;
	sigemptyset(&chld);
	sigaddset(&chld, SIGCHLD);

	sigprocmask(SIG_BLOCK, &chld, &chld);
	
	if ((child_pid = fork()) == 0) {
		int devnull=open("/dev/null", O_RDWR);
		if (devnull != 0) dup2(devnull, 0);
		if (devnull != 1) dup2(devnull, 1);
		if (devnull != 2) dup2(devnull, 2);		
		int result = execlp("cramfsck", "cramfsck", "-x", folderStr, packageFileStr, NULL);
		_exit(result);
	}
	
	int status;
	waitpid(child_pid, &status, 0);
	
	sigprocmask(SIG_SETMASK, &chld, NULL);
}

/* Take a package folder and compress it down into a file suitable for transport off of
   the system (a .kpg aka cramfs image), or passing back to UnpackPackageFileToFolder. */
void BPackageManager::PackFolderToPackageFile(const SString & folder, const SString & packageFile)
{
	char folderStr[1024];
	char packageFileStr[1024];
	char manifestFileStr[1024];
	
	SString manifestFile = folder;
	manifestFile.PathAppend(BV_MANIFEST);

	strcpy(manifestFileStr, manifestFile.String());
	strcpy(folderStr, folder.String());
	strcpy(packageFileStr, packageFile.String());
	
	int child_pid;

	sigset_t chld;
	sigemptyset(&chld);
	sigaddset(&chld, SIGCHLD);

	sigprocmask(SIG_BLOCK, &chld, &chld);
	
	if ((child_pid = fork()) == 0) {
		int devnull=open("/dev/null", O_RDWR);
		if (devnull != 0) dup2(devnull, 0);
		if (devnull != 1) dup2(devnull, 1);
		if (devnull != 2) dup2(devnull, 2);		
		int result = execlp("mkcramfs", "mkcramfs", "-i", manifestFileStr, folderStr, packageFileStr, NULL);
		_exit(result);
	}
	
	int status;
	waitpid(child_pid, &status, 0);
	
	sigprocmask(SIG_SETMASK, &chld, NULL);
}

/* Return an SValue containing manifest information for an arbitrary package folder. This
   does not have any connection to loaded packages, it is reading the manifest from that folder. */
SValue BPackageManager::ManifestForPackageFolder(const SString& packageFolder)
{
	SString filePath = packageFolder;
	filePath.PathAppend(BV_MANIFEST);

	SValue manifestData;
	int fd = open(filePath.String(), O_RDONLY);
	sptr<IByteInput> input = new BKernelIStr(fd);
	SManifestParser::ParseManifest(filePath, new PackageManifestParser(manifestData), input, B_NO_PACKAGE, B_EMPTY_STRING);
	close(fd);

	return manifestData;
}

/* Return an SValue containing manifest information for an arbitrary package file. This
   does not have any connection to loaded packages, it is reading the manifest from that file.
   The read process is efficient, and is pulling the XML out of the image header of the cramfs
   image -- it does not need to mount the image, unpack it, or even have the entire header
   available. */
SValue BPackageManager::ManifestForPackageFile(const SString& packageFile)
{
	SValue manifestData;
	int fd = open(packageFile.String(), O_RDONLY);
	sptr<IByteInput> input = new BKernelIStr(fd);
	SManifestParser::ParseManifestFromPackageFile(packageFile, new PackageManifestParser(manifestData), input, B_NO_PACKAGE, B_EMPTY_STRING);
	close(fd);

	return manifestData;
}

#if 0
sptr<BPackageManager::Package> BPackageManager::ImportPackageFile(int fd, ssize_t length)
{
	/* read data from fd until EOF or length bytes if length >= 0 */
	
	/* It will contain a .kpg, which consists of a cramfs image containing
	   a manifest. We'll need to read the manifest to determine the package name.
	   (We don't want to rely on external information supplied with the package,
	   purely the package contents.) */
	   
	/* We'll place the .kpg in a package path, and update the package list to
	   include it. */
	
	/* Scan new path for packages, and update queries */
	
	return package; /* new package, if it was loaded */
}


sptr<BPackageManager::Package> BPackageManager::ImportPackageFile(const SString & localFile)
{
	int fd = open(localFile.String(), O_RDONLY);
	
	ssize_t length = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	
	return ImportPackageFile(fd, length);
}

void BPackageManager::ExportPackageFile(const sptr<BPackageManager::Package>& package, const SString & target)
{
	/* if package is a folder: */
	char buf[1024];
	sprintf(buf, "mkcramfs %s %s", package.Path().String(), target.String());
	system(buf);
	/* else if package is a file: 
	   sendfile();
	 */
}
#endif


