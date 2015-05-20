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

#ifndef _PACKAGE_MANAGER_H
#define _PACKAGE_MANAGER_H

#include <services/IInformant.h>
#include <support/Catalog.h>
#include <support/Node.h>
#include <support/KeyedVector.h>
#include <package_p/ManifestParser.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace package {
using namespace palmos::services;
using namespace palmos::support;
#endif

class BPackageManager : public BCatalog
{
public:
	BPackageManager(const SContext& context);

	virtual void InitAtom();

	status_t AddQuery(const SString& dirName, const SString& property);
	void Start(bool verbose = false);

	uint32_t NextDatabaseID();

	static void UnpackPackageFileToFolder(const SString & packageFile, const SString & folder);
	static void PackFolderToPackageFile(const SString & folder, const SString & packageFile);
	static SValue ManifestForPackageFolder(const SString& packageFolder);
	static SValue ManifestForPackageFile(const SString& packageFile);

	class Package;

	// =================================================================================
	class Component : public SLightAtom
	{
	public:
		Component(const sptr<Package>& package, const SString& id, const SValue& value);
		~Component();
		const sptr<Package> GetPackage() const;
		const SString& Id() const;
		const SString& Local() const;
		uint32_t Version() const;
		time_t Modified() const;
		const SValue& Value() const;
		sptr<Component> Next() const;	// The next higher-version component
		sptr<Component> Prev() const;	// The next lower-version component
		void SetNext(const sptr<Component>& next);
		void SetPrev(const sptr<Component>& prev);
		void Remove();
	private:
		SAtom* const m_package;	// weak ref
		const SString m_id;
		const SString m_local;
		const SValue m_value;

		mutable SLocker m_lock;
		sptr<Component> m_next;
		sptr<Component> m_prev;
	};
	// =================================================================================
	class Package : public SAtom
	{
	public:
		Package(const SString& path, const SString& name);
		~Package();

		SString Path() const;
		SString Name() const;
		uint32_t DbID() const;
		ssize_t AddComponent(const sptr<Component>& component);
		void RemoveComponentAt(size_t index);
		size_t CountComponents() const;
		const sptr<Component>& ComponentAt(size_t index) const;


		void SetDbID(uint32_t dbid);

	private:
		mutable SLocker m_lock;
		SVector< sptr<Component> > m_components;
		SString m_path;
		SString m_name;
		uint32_t m_dbid;
	};
	// =================================================================================
	class Query : public SLightAtom
	{
	public:
		SString property;
		SKeyedVector<SString, SValue> entries;
		SKeyedVector<SString, wptr<IDatum> > datums;
	};
	// =================================================================================
	class Data
	{
	public:
		Data();

		void add_package_l(const sptr<BPackageManager::Package>& package);
		sptr<BPackageManager::Package> remove_package_l(const SValue& key);
		
		SLocker lock;
		SKeyedVector<sptr<IBinder>, sptr<Query> > queries;
		SVector< sptr<BPackageManager::Package> > packages;
		SKeyedVector< SString, sptr<BPackageManager::Component> > components;
		SKeyedVector< SString, sptr<BPackageManager::Component> > applications;
		SKeyedVector< SString, sptr<BPackageManager::Component> > addons;
	};
	// =================================================================================
protected:
	virtual ~BPackageManager();

private:	
	sptr<Package> scan_for_packages(const SString& packagesPath, const SString& pkgName, const SString& filePath, int32_t file, const SValue& info, bool verbose);
	uint32_t look_for_databases(const SString& dirPath, const sptr<BCatalog>& catalog);
	void register_with_informant(const sptr<IInformant>& informant);
	void run_queries();
	
	SVector<SString> m_packagesPath;
	
	uint32_t m_nextDatabaseID;
	Data m_data;
};


#if _SUPPORTS_NAMESPACE
} } // namespace palmos::package
#endif

#endif // _PACKAGE_MANAGER_H
