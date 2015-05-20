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

#include <support/Package.h>

#include <support/ByteStream.h>
#include <support/CallStack.h>
#include <support/Debug.h>
#include <support/KernelStreams.h>
#include <support/Looper.h>
#include <support/StdIO.h>
#include <support/TextCoder.h>
#include <support/Value.h>
#include <support/StdIO.h>

#include <xml/Parser.h>
#include <xml/XML2ValueParser.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define SHOW_LOAD_UNLOAD 1
#define TRACK_SHARED_OBJECTS 0

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

B_CONST_STRING_VALUE_LARGE(key_path, "path", )
B_CONST_STRING_VALUE_LARGE(key_packages_packages, "/packages/packages", )

#if SHOW_LOAD_UNLOAD
static int32_t g_watching = -1;
static bool ShowLoadUnload()
{
	if (g_watching < 0) {
		const char* var = getenv("BINDER_SHOW_LOAD_UNLOAD");
		g_watching = var ? atoi(var) : 0;
	}
	return g_watching > 0;
}
#endif

// ========================================================================

SPackage::Pool SPackage::g_pool;

// ========================================================================

#if TRACK_SHARED_OBJECTS
static volatile int32_t g_numSharedObjects;
#endif

BSharedObject::BSharedObject(const SValue& src, const SPackage& package, attach_func attach)
	:m_source(src), m_handle(B_BAD_IMAGE_ID), m_attachFunc(attach), m_package(package)
{
	//bout << "Constructing BSharedObject: " << this << endl;
#if TRACK_SHARED_OBJECTS
	SysAtomicInc32(&g_numSharedObjects);
#endif
}

#if 0
BSharedObject::BSharedObject(image_id image)
	:m_handle(image), m_attachFunc(NULL)
{
}
#endif

void BSharedObject::InitAtom()
{
	//bout << "Initializing BSharedObject: " << this << endl;
	
	if (m_source.IsDefined()) {
		m_handle = dlopen(m_source.AsString().String(), RTLD_LAZY);
		#if SHOW_LOAD_UNLOAD
		if (ShowLoadUnload()) {
			bout << "Loaded BSharedObject " << this << " (handle " << m_handle
					<< ", path " << m_source.AsString() << endl;
		}
		#endif
		if (m_handle == B_BAD_IMAGE_ID) {
			berr << "Failed to load add-on " << this
				 << " (image " << m_handle << ", file " << m_source << ")"
				 << "error = " << dlerror() << endl;
		}
	}

	if (m_handle && !m_attachFunc) {
		m_attachFunc = (attach_func)dlsym((void*)m_handle, "set_package_image_object");
		if (m_attachFunc != NULL) {
			m_attachFunc(this);
		} else {
			berr << "*** No set_package_image_object found in " << m_handle << "!!" << endl;
		}
	}
}

image_id BSharedObject::ID() const
{
	return m_handle;
}

SValue BSharedObject::Source() const
{
	return m_source;
}

SString BSharedObject::File() const
{
	return m_source.AsString();
}


status_t BSharedObject::GetSymbolFor(const char* name, int32_t sclass, void** outPtr) const
{
#if TARGET_HOST == TARGET_HOST_PALMOS
	(void) name;
	(void) sclass;
	(void) outPtr;
	return B_UNSUPPORTED;
#elif TARGET_HOST == TARGET_HOST_WIN32
	return m_handle != B_BAD_IMAGE_ID ? get_image_symbol(m_handle, name, sclass, outPtr) : m_handle;
#elif TARGET_HOST == TARGET_HOST_LINUX
	if (m_handle != B_BAD_IMAGE_ID)
	{
		*outPtr = dlsym(m_handle, name);
		const char* errString = dlerror();
		if (errString != NULL)
		{
			bout << "Symbol('" << name << "') not found!  error=" << errString << endl;
			return B_ERROR;
		}
		return B_OK;
	}
	return B_ERROR;
#endif
}

status_t BSharedObject::GetSymbolAt(int32_t index,
		SString* outName, int32_t* outSClass, void** outPtr) const
{
#if TARGET_HOST == TARGET_HOST_WINDOWS
	int32_t size = 512;
	status_t err;
	char* buf = outName->LockBuffer(size);
	if (buf != NULL) {
		err = get_nth_image_symbol(m_handle, index, buf, &size, outSClass, outPtr);
		if (err < B_OK) size = 0;
		outName->UnlockBuffer(size);
	} else {
		err = B_NO_MEMORY;
	}
	return err;
#elif TARGET_HOST == TARGET_HOST_LINUX
	ErrFatalError("NOT IMPLEMENTED!");
	return B_ERROR;
#endif
}

void BSharedObject::Detach()
{
	if (m_attachFunc) {
		m_attachFunc(sptr<BSharedObject>(NULL));
	}
}

BSharedObject::~BSharedObject()
{
	//bout << "Destroying BSharedObject: " << this << endl;
#if TRACK_SHARED_OBJECTS
	const int32_t r = SysAtomicDec32(&g_numSharedObjects);
	bout << "Unloaded BSharedObject, " << (r-1) << " remaining." << endl;
#endif
	
#if 0
	bout << SPrintf("BSharedObject %p destructor called (area %ld)\n", this, m_handle);
	SCallStack stack;
	stack.Update();
	bout << stack << endl;
	stack.LongPrint(bout);
#endif

	//SetResourceDatabase(NULL);

	if (m_source.IsDefined() && m_handle != B_BAD_IMAGE_ID) {
		#if SHOW_LOAD_UNLOAD
		if (ShowLoadUnload()) {
			bout << "Unloading BSharedObject " << this << " (handle " << m_handle
					<< ", path " << m_source.AsString() << endl;
		}
		#endif
		dlclose(m_handle);
		m_handle = B_BAD_IMAGE_ID;
	}
}

// ========================================================================

SPackage::MemoryMap::MemoryMap(const SString& path)
	:	m_fd(-1),
		m_buffer(NULL),
		m_length(0)
{
	m_fd = open(path.String(), O_RDONLY);

	if (m_fd >= 0)
	{
		struct stat sb;
		stat(path.String(), &sb);
		m_length = (size_t)sb.st_size;
		m_buffer = (char*)mmap(NULL, m_length, PROT_READ, MAP_SHARED, m_fd, 0);
	}
}

SPackage::MemoryMap::~MemoryMap()
{
	if (m_buffer)
	{
		munmap(m_buffer, m_length);
		m_buffer = NULL;
	}

	if (m_fd >= 0)
	{
		close(m_fd);
		m_fd = -1;
	}
}

void* SPackage::MemoryMap::Buffer()
{
	return m_buffer;
}

size_t SPackage::MemoryMap::Size()
{
	return m_length;
}

// ========================================================================


SPackage::Data::Data(const SString& name, const SString& path)
	:	m_name(name),
		m_path(path)
{
	m_resourcePath = m_path;
	m_resourcePath.PathAppend("resources");
}

SPackage::Data::~Data()
{
}

SString SPackage::Data::Name()
{
	return m_name; 
}

SString SPackage::Data::Path()
{
	return m_path;
}

SString SPackage::Data::ResourcePath()
{
	return m_resourcePath;
}

char* SPackage::Data::get_buffer_for(const SString& locale)
{
	SLocker::Autolock lock(m_lock);
	sptr<SPackage::MemoryMap> map = m_maps.ValueFor(locale);

	if (map == NULL)
	{
		SString path(m_resourcePath);
		path.PathAppend(locale);
		path.PathAppend("Strings.localized");
	
		map = new SPackage::MemoryMap(path);
		m_maps.AddItem(locale, map);	
	}

	return (char*)map->Buffer(); 
}

SString SPackage::Data::LoadString(const SString& key, const SString& locale)
{
	SString result;

	char* buf = NULL;

	if (locale == SString::EmptyString())
	{
		// FIX ME! need to get the system locale
		//berr << "FIX ME! SPackage::LoadString() defaulting to enUS" << endl;
		SString where("enUS");
		buf = get_buffer_for(where);
	}
	else
	{
		buf = get_buffer_for(locale);
	}

	if (buf != NULL)
	{
		const uint32_t COUNT = *((uint32_t*)buf);

		char* it = buf + sizeof(uint32_t);
		uint32_t* indices = (uint32_t*)it;
		char* keys = it + (sizeof(uint32_t) * COUNT * 2);

		bool found = false;
		int32_t low = 0;
		int32_t high = COUNT - 1;
		int32_t mid;
		while (low <= high)
		{
			mid = (low + high) / 2;
			char* tmp = buf + indices[mid];
			int cmp = strcmp(tmp, key.String());
			if (cmp > 0)
			{
				high = mid - 1;
			}
			else if (cmp < 0)
			{
				low = mid + 1;
			}
			else
			{
				found = true;
				break;
			}
		}
	
		if (found)
		{
			result = SString(buf + indices[mid + COUNT]);
		}	
	}

	return result;
}

SString SPackage::Data::LoadString(uint32_t index, const SString& locale)
{
	return NULL;
}

// ========================================================================

SPackage::Pool::Pool()
{
}

sptr<SPackage::Data> SPackage::Pool::DataFor(const SString& pkgName)
{
	SLocker::Autolock lock(m_lock);

	sptr<SPackage::Data> data = m_packages.ValueFor(pkgName).promote();

	if (data == NULL)
	{
		SString catPath(key_packages_packages);
		catPath.PathAppend(pkgName);

		SValue value = SContext::UserContext().Lookup(catPath);
		
		SString path = value[key_path].AsString();

		struct stat sb;
		int err = stat(path.String(), &sb);

		if (err != 0)
		{
			berr << "*** Error construting package '"<< pkgName << "' err = " << strerror(errno) << endl;
		}
		else
		{
			data = new SPackage::Data(pkgName, path);
			m_packages.AddItem(pkgName, data);
		}
	}
	
	return data;
}


// ========================================================================
SPackage::SPackage()
	:	m_data(NULL)
{
}

SPackage::SPackage(const SString& packageName)
{
	m_data = g_pool.DataFor(packageName);
}

SPackage::SPackage(const SValue& value, status_t *error)
{
	if (value.Type() == B_PACKAGE_TYPE)
	{
		const void* data = value.Data();
		if (data != NULL) 
		{
			m_data = g_pool.DataFor(SString(static_cast<const char*>(data), value.Length()));
		}
	} 
	else 
	{
		status_t err;
		SString packageName = value.AsString(&err);
		if (err == B_OK) 
		{
			m_data = g_pool.DataFor(packageName);
		}
	}
		
	if (error != NULL)
	{
		*error = (m_data == NULL) ? B_BAD_VALUE : B_OK;
	}
}

SPackage::~SPackage()
{
}

SValue SPackage::AsValue()
{
	return SValue::String(Name());
} 


status_t SPackage::StatusCheck()
{
	return (m_data == NULL) ? B_NO_INIT : (m_data->Path().Length() == 0) ? B_NO_INIT : B_OK; 
}

SString SPackage::Name()
{
	return (m_data != NULL) ? m_data->Name() : SString::EmptyString();
}

SString SPackage::Path()
{
	return (m_data != NULL) ? m_data->Path() : SString::EmptyString();
}

sptr<IByteInput> SPackage::OpenResource(const SString& fileName, const SString& locale)
{
	if (m_data == NULL) return NULL;

	SString path(m_data->ResourcePath());

	if (locale != SString::EmptyString())
	{
		path.PathAppend(locale);
		struct stat sb;
		int err = stat(path.String(), &sb);

		if (err != 0)
		{
			berr << "*** Locale '" << locale << "' does not exist for package '" << Name() << "'" << endl;
			// should fall back onto the default locale

			path = m_data->Path();
		}
	}

	path.PathAppend(fileName);

	sptr<IByteInput> input = NULL;
	int fd = open(path.String(), O_RDONLY);

	if (fd != -1)
	{
		input = new BKernelIStr(fd);
	}
	else
	{
		berr << "*** Could not open file '" << path << "'" << endl;
	}

	return input;
}

SString SPackage::LoadString(const SString& key, const SString& locale)
{
	return (m_data != NULL) ? m_data->LoadString(key, locale) : SString::EmptyString(); 
}

SString SPackage::LoadString(uint32_t index, const SString& locale)
{
	return (m_data != NULL) ? m_data->LoadString(index, locale) : SString::EmptyString(); 
}



#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
