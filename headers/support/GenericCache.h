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

#ifndef SUPPORT_GENERIC_CACHE_H
#define SUPPORT_GENERIC_CACHE_H

/*!	@file support/GenericCache.h
	@ingroup CoreSupportUtilities
	@brief Helper class for implementing RAM-based caches.
*/

#include <PalmTypes.h>
#include <support/Atom.h>
#include <support/Locker.h>
#include <support/KeyedVector.h>
#include <support/StdIO.h>
#if _SUPPORTS_RTTI
#include <typeinfo>
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class ITextOutput;

/*****************************************************************************/

enum {
	B_GENERIC_CACHE_SIZE_UNLIMITED = 0			//!< pass to constructor so the cache uncontrolably grows, until system ressources are exhausted
};

enum {
	B_GENERIC_CACHE_NEVER_PURGE	= 0x00000001,	//!< pass to Add() so the added item will never be purged
	B_GENERIC_CACHE_DONT_ADD	= 0x00000002,	//!< pass to Add() so this item is not added if there is no more space (cache size will not grow)
	B_GENERIC_CACHE_DONT_PURGE	= 0x00000004	//!< pass to Add() so the cache is not purged if the item is bigger than the cache size
};

class SAbstractCache
{
public:
	const void*		AbstractLookup(const void *key, int32_t flags) const;
	status_t		AbstractAdd(const void *key, const void *data, int32_t flags);
	status_t		AbstractRemove(const void *key);
	void			Dump(const sptr<ITextOutput>& io) const;
protected:
					SAbstractCache(ssize_t cacheSize);
	virtual			~SAbstractCache();
private:
	virtual void							PerformPrintItem(const void* data, const sptr<ITextOutput>& io) const = 0;
	virtual ssize_t							PerformSize(const void* data) const = 0;
	virtual uint32_t&						PerformEntryAge(const void* entry) const = 0;
	virtual const void*						PerformEntryData(const void* entry) const = 0;
	virtual const SAbstractKeyedVector&		Entries() const = 0;
	virtual SAbstractKeyedVector&			Entries() = 0;
	virtual status_t						CreateEntry(const void* key, const void* data, uint32_t age) = 0;
	inline uint32_t tick() const;
	inline ssize_t size_left() const { return m_cacheSize-m_sizeUsed; }
	mutable uint32_t	m_age;
	ssize_t				m_cacheSize;
	ssize_t				m_sizeUsed;
};

/*****************************************************************************/

template<class KEY, class TYPE>
class SGenericCache : private SAbstractCache
{
public:
	inline						SGenericCache(ssize_t cacheSize);
	virtual inline				~SGenericCache();

	inline	TYPE				Lookup(const KEY& key, int32_t flags = 0) const;
	inline	status_t			Add(const KEY& key, const TYPE& data, int32_t flags = 0);
	inline	status_t			Remove(const KEY& key);
	inline void					Dump(const sptr<ITextOutput>& io) const;

private:
	virtual void							PerformPrintItem(const void* data, const sptr<ITextOutput>& io) const;
	virtual ssize_t							PerformSize(const void* data) const;
	virtual uint32_t&						PerformEntryAge(const void* entry) const;
	virtual const void*						PerformEntryData(const void* entry) const;
	virtual const SAbstractKeyedVector&		Entries() const;
	virtual SAbstractKeyedVector&			Entries();
	virtual status_t						CreateEntry(const void* key, const void* data, uint32_t age);
	template<class ENTTYPE> struct entry_t {
		mutable uint32_t	age;	// if 0xFFFFFFFF, value cannot be expunged
		ENTTYPE				data;
		entry_t() {}
		entry_t(const entry_t& from)
			:	age(from.age),
				data(from.data)
		{}
	};
	SKeyedVector<KEY, entry_t<TYPE> >		m_entries;
	mutable SLocker	m_lock;
};

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

template<class TYPE> inline
ssize_t PGenericCacheSize(const TYPE& data)
{
	return data->Size();
}


template<class TYPE> inline
const sptr<ITextOutput>& PGenericCachePrintItem(const sptr<ITextOutput>& io, const TYPE& data)
{
#if _SUPPORTS_RTTI
	io << &data << " (" << typeid(data).name() << ") size=" << PGenericCacheSize(data);
#else
	io << &data << " size=" << PGenericCacheSize(data);
#endif
	return io;
}

// ---------------------------------------------------------------------

template<class KEY, class TYPE> inline
SGenericCache<KEY, TYPE>::SGenericCache(ssize_t cacheSize)
	: SAbstractCache(cacheSize), m_lock("SGenericCache::m_lock")
{
}

template<class KEY, class TYPE> inline
SGenericCache<KEY, TYPE>::~SGenericCache()
{
}

// ---------------------------------------------------------------------

template<class KEY, class TYPE> inline
void SGenericCache<KEY, TYPE>::Dump(const sptr<ITextOutput>& io) const
{
	SLocker::Autolock _l(m_lock);
	SAbstractCache::Dump(io);
}

template<class KEY, class TYPE> inline
TYPE SGenericCache<KEY, TYPE>::Lookup(const KEY& key, int32_t flags) const
{
	SLocker::Autolock _l(m_lock);
	return *(static_cast< const TYPE* >( AbstractLookup(&key, flags) ));
}

template<class KEY, class TYPE> inline
status_t SGenericCache<KEY, TYPE>::Add(const KEY& key, const TYPE& data, int32_t flags)
{
	SLocker::Autolock _l(m_lock);
	return AbstractAdd(&key, &data, flags);
}

template<class KEY, class TYPE> inline
status_t SGenericCache<KEY, TYPE>::Remove(const KEY& key)
{
	SLocker::Autolock _l(m_lock);
	return AbstractRemove(&key);
}

// ---------------------------------------------------------------------

template<class KEY, class TYPE> inline
void SGenericCache<KEY, TYPE>::PerformPrintItem(const void* data, const sptr<ITextOutput>& io) const
{
	PGenericCachePrintItem(io, *static_cast< const TYPE* >(data));
}

template<class KEY, class TYPE> inline
ssize_t SGenericCache<KEY, TYPE>::PerformSize(const void *data) const
{
	return PGenericCacheSize(*static_cast< const TYPE* >(data));
}

template<class KEY, class TYPE> inline
uint32_t& SGenericCache<KEY, TYPE>::PerformEntryAge(const void* entry) const
{
	return static_cast<const entry_t<TYPE>*>(entry)->age;
}

template<class KEY, class TYPE> inline
const void* SGenericCache<KEY, TYPE>::PerformEntryData(const void* entry) const
{
	return &(static_cast<const entry_t<TYPE>*>(entry)->data);
}

// ---------------------------------------------------------------------

template<class KEY, class TYPE> inline
const SAbstractKeyedVector& SGenericCache<KEY, TYPE>::Entries() const
{
	return *(static_cast<const SAbstractKeyedVector*>((const void*)&m_entries));
}

template<class KEY, class TYPE> inline
SAbstractKeyedVector& SGenericCache<KEY, TYPE>::Entries()
{
	return *(static_cast<SAbstractKeyedVector*>((void*)&m_entries));
}

template<class KEY, class TYPE> inline
status_t SGenericCache<KEY, TYPE>::CreateEntry(const void* key, const void* data, uint32_t age)
{
	entry_t<TYPE> entry;
	entry.age = age;
	entry.data = *(static_cast< const TYPE* >(data));
	return m_entries.AddItem(*(static_cast<const KEY*>(key)), entry);
}

/*****************************************************************************/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif
