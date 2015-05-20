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

#include <PalmTypes.h>
#include <support/GenericCache.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*****************************************************************************/

SAbstractCache::SAbstractCache(ssize_t cacheSize)
	:	m_age(0),
		m_cacheSize(cacheSize),
		m_sizeUsed(0)
{
}

SAbstractCache::~SAbstractCache()
{
}

uint32_t SAbstractCache::tick() const
{
	return m_age += 2;
}

const void *SAbstractCache::AbstractLookup(const void* key, int32_t flags) const
{
	const SAbstractKeyedVector& e = Entries();

	bool found;
	const void* entry = e.KeyedFor(key, &found);
	if (found) {
		uint32_t& age = PerformEntryAge( entry );
		if (age & 1) {
			// cannot be removed
		} else {
			age = tick();
		}
	}
	return PerformEntryData( entry );
}

status_t SAbstractCache::AbstractAdd(const void *key, const void *data, int32_t flags)
{
	SAbstractKeyedVector& e = Entries();

	bool found;
	const void* remove = e.KeyedFor(key, &found);
	if (found) {
		m_sizeUsed -= PerformSize( PerformEntryData(remove) );
		e.RemoveKeyed(key);
	}
	
	// If this item is never removed
	// we don't count it in the overall size of the cache.
	const ssize_t dataSize = (flags & B_GENERIC_CACHE_NEVER_PURGE) ? 0 : PerformSize( data );
	const uint32_t age = tick();

	if (m_cacheSize != B_GENERIC_CACHE_SIZE_UNLIMITED) {
		// Our cache is limited in size, so try to remove some entries...

		if ((m_cacheSize < dataSize) && (flags & B_GENERIC_CACHE_DONT_PURGE)) {
			// The cache is too small for that item. But we're ask to not purge
			// the whole cache in that case. So Just do nothing...
		} else {	
			while (size_left() < dataSize) {
				uint32_t min_age = age;
				ssize_t index = -1;
				ssize_t count = e.CountItems();
				for (ssize_t i=0 ; i<count ; i++) {		
					uint32_t age_current = PerformEntryAge( e.AbstractValueAt(i) );
					if (age_current < min_age) {
						// Here we don't need to test that age_current&1 is null
						// because if it is not, age_current > age, by construction.
						min_age = age_current;
						index = i;
					}
				}
				if (index < 0) {
					// we couldn't remove anything more.
					break;
				}
				const void* remove = e.AbstractValueAt(index);
				m_sizeUsed -= PerformSize( PerformEntryData(remove) );
				e.RemoveItemsAt(index, 1);
			}
		}
	}

	if ((flags & B_GENERIC_CACHE_DONT_ADD) && (size_left() < dataSize)) {	
		// there is no space and we're asked to not add the item in that case.
		// so do nothing...
	} else {
		const uint32_t entryAge = ((flags & B_GENERIC_CACHE_NEVER_PURGE) ? 0xFFFFFFFFLU : age);
		ssize_t index = CreateEntry(key, data, entryAge);
		if (index >= 0) {
			m_sizeUsed += dataSize;
			return B_OK;
		}
		return status_t(index);
	}
	return B_NO_MEMORY;
}

status_t SAbstractCache::AbstractRemove(const void *key)
{
	SAbstractKeyedVector& e = Entries();
	bool found;
	const void* remove = e.KeyedFor(key, &found);
	if (found) {	
		if (PerformEntryAge(remove) & 1) {
			// intentionnally empty		
		} else {
			m_sizeUsed -= PerformSize( PerformEntryData(remove) );
		}
		e.RemoveKeyed(key);
		return B_OK;
	}
	return B_NAME_NOT_FOUND;
}

void SAbstractCache::Dump(const sptr<ITextOutput>& io) const
{
#if BUILD_TYPE == BUILD_TYPE_DEBUG
	const SAbstractKeyedVector& e = Entries();

#if _SUPPORTS_RTTI
	io << SPrintf("[%p (%s)]\nstatistics: %ld/%ld, left=%ld", this, typeid(*this).name(), m_sizeUsed, m_cacheSize, size_left()) << endl;
#else
	io << SPrintf("[%p (SAbstractCache)]\nstatistics: %ld/%ld, left=%ld", this, m_sizeUsed, m_cacheSize, size_left()) << endl;
#endif

	const ssize_t count = e.CountItems();
	for (ssize_t i=0 ; i<count ; i++) {
		const void* value = e.AbstractValueAt(i);
		const void* data = PerformEntryData(value);
		io << "[" << i << "] [" << PerformEntryAge(value) << "] ";
		PerformPrintItem(data, io);
		io << endl;
	}
#else
	io << SPrintf("[%p (SAbstractCache)] %ld/%ld, left=%ld", this, m_sizeUsed, m_cacheSize, size_left()) << endl;
#endif
}

/*****************************************************************************/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
