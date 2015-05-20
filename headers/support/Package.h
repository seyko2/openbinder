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

#ifndef _SUPPORT_PACKAGE_H
#define _SUPPORT_PACKAGE_H

/*!	@file support/Package.h
	@ingroup CoreSupportBinder
	@brief Interface to an executable image representing a Binder package.
*/

#include <support/KeyedVector.h>
#include <support/Locker.h>
#include <support/String.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportBinder
	@{
*/

#if (defined(__GNUC__))
#define ATTRIBUTE_VISIBILITY_HIDDEN __attribute__ ((visibility("hidden")))
#else
#define ATTRIBUTE_VISIBILITY_HIDDEN
#endif

#if TARGET_HOST == TARGET_HOST_PALMOS
#include <CmnLaunchCodes.h>
#include <DataMgr.h>
#else
#define sysPackageLaunchAttachImage				71
#endif

class BSharedObject;

struct SysPackageLaunchAttachImageType
{
      size_t                  size;
      sptr<BSharedObject>     image;
};

typedef void	(*attach_func)(const sptr<BSharedObject>& image);


/*---------------------------------------------------------------*/
/*----- SPackage ------------------------------------------------*/
//! A convenience class for getting information about a package,
//! and doing useful things with it.
class SPackage 
{
public:
	SPackage();
	SPackage(const SString& name);
	SPackage(const SValue& value, status_t* err=NULL);
	SValue AsValue();

	~SPackage();

	status_t StatusCheck();
	SString Name();
	SString Path();
			
	sptr<IByteInput> OpenResource(const SString& fileName, const SString& locale = SString::EmptyString());
	
	SString LoadString(const SString& key, const SString& locale = SString::EmptyString());
	SString LoadString(uint32_t index, const SString& locale = SString::EmptyString());

private:
	class MemoryMap : public virtual SAtom
	{
	public:
		MemoryMap(const SString& path);

		void* Buffer();
		size_t Size();
	protected:
		~MemoryMap();

	private:
		int m_fd;
		void* m_buffer;	
		size_t m_length;
	};

	class Data : public virtual SAtom
	{
	public:
		Data(const SString& name, const SString& path);
		
		SString Name();
		SString Path();
		SString ResourcePath();
		
		SString LoadString(const SString& key, const SString& locale);
		SString LoadString(uint32_t index, const SString& locale);

	protected:
		virtual ~Data();

	private:	
		char* get_buffer_for(const SString& locale);
		
		SLocker m_lock;
		SString m_name;
		SString m_path;
		SString m_resourcePath;

		SKeyedVector<SString, sptr<SPackage::MemoryMap> > m_maps;
	};

	class Pool
	{
	public:
		Pool();
		
		sptr<SPackage::Data> DataFor(const SString& pkgName);

	private:
		SLocker m_lock;
		SKeyedVector<SString, wptr<SPackage::Data> > m_packages;
	};


	static SPackage::Pool g_pool;
	sptr<Data> m_data;
};

extern const SPackage B_NO_PACKAGE;


/*---------------------------------------------------------------*/
/*----- BSharedObject -------------------------------------------*/
//!	A representation of a loaded executable image.  
class BSharedObject : public virtual SAtom
{
public:
			//!	Create a new BSharedObject, loading \a file as an add-on.
			/*!	The image will be unloaded when the BSharedObject is destoyed. */
							BSharedObject(const SValue& src, const SPackage &package, attach_func attach = NULL);
			//!	Create a new BSharedObject for \a image.
			/*!	The given image will never be unloaded. */
			//				BSharedObject(const image_id image);

			//!	Return the image_id this object is for.
			image_id		ID() const;
			//! Return source of this image.
			SValue			Source() const;
			//!	Return the filename the image came from.
			//!	Deprecated -- used Source() instead.
			SString			File() const;

			//!	Look up a symbol in the image by name.
			/*!	\sa get_image_symbol() */
			status_t		GetSymbolFor(	const char* name,
											int32_t sclass,
											void** outPtr) const;
			//!	Look up a symvol in the image by index.
			/*!	\sa get_nth_image_symbol() */
			status_t		GetSymbolAt(	int32_t index,
											SString* outName,
											int32_t* outSClass,
											void** outPtr) const;

			//!	Detach this BSharedObject from the image it loaded.
			/*!	This makes it so that any future SPackageSptr instantations
				in the image's code will fail, so that (eventually) all
				weak references on the image can go away. */
			void			Detach();

			inline SPackage	Package() { return m_package; }

protected:
	virtual					~BSharedObject();
	virtual	void			InitAtom();

private:
			void			close_resource();

			const SValue	m_source;
			SValue			m_extra;
			image_id		m_handle;
			attach_func		m_attachFunc;
			SPackage		m_package;

private:

	// copy constructor not supported
	BSharedObject( const BSharedObject& rhs );

	// operator= not supported
	BSharedObject& operator=( const BSharedObject& rhs );
};


/*-----------------------------------------------------------*/
/*----- SPackageSptr ----------------------------------------*/
//!	Keep your local executable image loaded.
/*! Instantiating this class adds a reference to the package, so that
	its code will not be unloaded until the SPackageSptr is destroyed.
	This allows you to ensure that the code stays around as long as any
	of its components exist (by having those components inherit from
	SPackageSptr or included it as a member), or as long as any threads
	you manually spawn still exist (by putting the SPackageSptr on that
	thread's stack).

	All of your exported classes should inherit from this, so that when
	all references to it are deleted, your add-on will be unloaded.  The
	code for this class is linked in to each PRC.  This means the
	implementation here can pretty much change at will, without any
	binary compatibility issues; references to SPackageSptr objects should
	never pass outside the confines of the PRC in which they are
	created.
	
	@note The visibility control on this class is very important -- every
	shared library must get its own copy of the implementation of the
	class, so that it can correctly track references on that code and not
	get confused with other shared libraries.
*/
class SPackageSptr
{
public:
	//!	Acquire a reference on your package.
	/*!	This method is included as part of the glue code that your
		application links with, so that it can retrieve the BSharedObject
		associated with your executable. */
									SPackageSptr()
									 ATTRIBUTE_VISIBILITY_HIDDEN
									 ;
	//!	Note that the destructor is very intentionally NOT virtual.
	/*!	It wouldn't cause problems if it was virtual, but there is
		no reason to do so and it causes unneeded code to be generated. */
	inline							~SPackageSptr()
									 ATTRIBUTE_VISIBILITY_HIDDEN
									 ;

	//!	Retrieve your package object.
	inline	const sptr<BSharedObject>&	SharedObject() const ATTRIBUTE_VISIBILITY_HIDDEN;
	inline	SPackage					Package() const ATTRIBUTE_VISIBILITY_HIDDEN;

	//!	Convenience to retrieve resource data from your package via SPackage::OpenResource().
	sptr<IByteInput> OpenResource(const SString& fileName, const SString& locale = SString::EmptyString()) const ATTRIBUTE_VISIBILITY_HIDDEN;
	
	//! Convenience to retrieve a string from your package via SPackage::LoadString().
	SString LoadString(const SString& key, const SString& locale = SString::EmptyString()) const ATTRIBUTE_VISIBILITY_HIDDEN;
	//! Convenience to retrieve a string from your package via SPackage::LoadString().
	SString LoadString(uint32_t index, const SString& locale = SString::EmptyString()) const ATTRIBUTE_VISIBILITY_HIDDEN;
	
	// XXX This are old APIs from Cobalt.
	
	//!	Convenience to retrieve resource data from your package.
	inline	sptr<IByteInput>		OpenResource(uint32_t type, int32_t id) const ATTRIBUTE_VISIBILITY_HIDDEN;
	//!	Convenience to retrieve a string from your package.
	inline	SString					LoadString(int32_t id, int32_t index = 0) const ATTRIBUTE_VISIBILITY_HIDDEN;

private:
			sptr<BSharedObject>		m_so;
};

inline SPackageSptr::~SPackageSptr()
{
}

inline const sptr<BSharedObject>& SPackageSptr::SharedObject() const
{
	return m_so;
}
inline SPackage SPackageSptr::Package() const
{
	return m_so->Package();
}

inline sptr<IByteInput> SPackageSptr::OpenResource(const SString& fileName, const SString& locale) const
{
	return m_so->Package().OpenResource(fileName, locale);
}

inline SString SPackageSptr::LoadString(const SString& key, const SString& locale) const
{
	return m_so->Package().LoadString(key, locale);
}

inline SString SPackageSptr::LoadString(uint32_t index, const SString& locale) const
{
	return m_so->Package().LoadString(index, locale);
}

inline sptr<IByteInput> SPackageSptr::OpenResource(uint32_t type, int32_t id) const
{
	return NULL;
}
inline SString SPackageSptr::LoadString(int32_t id, int32_t index) const
{
	return SString();
}

typedef SPackageSptr SDontForgetThis;

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif
