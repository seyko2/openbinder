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

#include <support_p/ValueMap.h>

#include <support/Autolock.h>
#include <support/Parcel.h>
#include <support/MemoryStore.h>
#include <support/StdIO.h>

#include <stdio.h>
#include <DebugMgr.h>

#include <support_p/WindowsCompatibility.h>

#include "ValueInternal.h"

#ifndef VALIDATES_VALUE
#define VALIDATES_VALUE 0
#endif

#define PROFILE_VALUE_MAP 0

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// Some addition private inline functions

inline void BValueMap::Construct(int32_t avail)
{
	m_users = 1;
	m_dataSize = B_ERROR;
	m_size = 0;
	m_avail = avail;
	m_editIndex = -1;
}

inline void BValueMap::Destroy()
{
	SValue* pos = (SValue*)m_maps;
	SValue* end = pos + (m_size*2);
	while (pos < end) {
		pos->~SValue();
		pos++;
	}
}

// The pool

enum { MAX_POOL_SIZE = 6 };
static const int32_t size_to_pool[] = { 0, 0, 1, 1, 2, 2, 2 };
static const int32_t pool_to_size[] = { 1, 3, MAX_POOL_SIZE };
static const int32_t max_in_pool[] = { 20, 10, 5 };

BValueMapPool::BValueMapPool()
	: m_lock("BValueMap Pool"), m_hits(0), m_misses(0)
{
}

BValueMapPool::~BValueMapPool()
{
	m_lock.LockQuick();
	for (int32_t i=0; i<NUM_POOLS; i++) {
		BValueMap* map = m_pools[i].objects;
		while (map) {
			BValueMap* next = (BValueMap*)(map->m_users);
			free(map);
			map = next;
		}
		m_pools[i].count = 0;
		m_pools[i].objects = NULL;
	}
	m_lock.Unlock();
}

BValueMap* BValueMapPool::Create(size_t initSize)
{
	if (initSize <= MAX_POOL_SIZE) {
		m_lock.LockQuick();
		pool& p = m_pools[size_to_pool[initSize]];
		BValueMap* map = p.objects;
		if (map) {
			p.objects = (BValueMap*)map->m_users;
			p.count--;
			map->m_users = 1;
			map->m_dataSize = B_ERROR;
			map->m_size = 0;
#if BUILD_TYPE == BUILD_TYPE_DEBUG && PROFILE_VALUE_MAP
			g_threadDirectFuncs.atomicInc32(&m_hits);
#endif
			m_lock.Unlock();
			return map;
		}
		m_lock.Unlock();
		initSize = pool_to_size[size_to_pool[initSize]];
	}

#if BUILD_TYPE == BUILD_TYPE_DEBUG && PROFILE_VALUE_MAP
	g_threadDirectFuncs.atomicDec32(&m_misses);
#endif

	BValueMap* map;
	if (initSize <= 1)
		map = (BValueMap*)malloc(sizeof(BValueMap));
	else
		map = (BValueMap*)malloc(sizeof(BValueMap)+(sizeof(BValueMap::pair)*(initSize-1)));
	if (map) map->Construct(initSize);
	return map;
}

void BValueMapPool::Delete(BValueMap* map)
{
	map->Destroy();

	if (map->m_avail <= MAX_POOL_SIZE) {
		m_lock.LockQuick();
		const int32_t whichPool = size_to_pool[map->m_avail];
		pool& p = m_pools[whichPool];
		if (p.count <= max_in_pool[whichPool]) {
			map->m_users = (int32_t)p.objects;
			p.objects = map;
			p.count++;
			DbgOnlyFatalErrorIf(map->m_avail != pool_to_size[whichPool], "BValueMap pool error");
			DbgOnlyFatalErrorIf(map->m_editIndex >= 0, "BValueMap pool error");
			m_lock.Unlock();
			return;
		}
		m_lock.Unlock();
	}

	free(map);
}

#if PROFILE_VALUE_MAP
static int32_t g_numMaps = 0;
static int32_t g_hits = 1;
#endif

BValueMap* BValueMap::Create(size_t initSize)
{
#if PROFILE_VALUE_MAP
	int32_t num = g_threadDirectFuncs.atomicInc32(&g_numMaps);
	if (g_threadDirectFuncs.atomicInc32(&g_hits)%1000 == 0) {
		int32_t hits = g_valueMapPool.Hits();
		int32_t misses = g_valueMapPool.Misses();
		bout << num << " BValueMap objects exist (Pool: hits=" << hits << ", misses="
			<< misses << " => " << ( (hits/float(hits+misses))*100 ) << "%)" << endl;
	}
#endif

	return g_valueMapPool.Create(initSize);
}

BValueMap* BValueMap::Create(SParcel& from, size_t avail, size_t count, ssize_t* out_size)
{
	ssize_t size;
	ssize_t remain = avail;
	size_t i = 0;
	pair* pos;
	
	BValueMap* This = Create(count);
	if (This != NULL) {
		pos = This->m_maps;
		for (i=0; i<count; i++) {
			if ((size=pos->key.unarchive_internal(from, remain)) < B_OK) {
				goto error;
			}
			remain -= size;
			if ((size=pos->value.unarchive_internal(from, remain)) < B_OK) {
				pos->key.~SValue();
				goto error;
			}
			remain -= size;
			pos++;
		}
		
		This->m_size = count;
		*out_size = This->m_dataSize = (avail-remain);
		return This;
	}

	size = B_NO_MEMORY;

error:
	*out_size = size;

	if (This) {
		This->m_size = i;
		This->DecUsers();
	}

	return NULL;
}

BValueMap* BValueMap::Clone() const
{
	BValueMap* map = Create(m_size);
	if (map) {
		map->m_dataSize = m_dataSize;
		map->m_size = m_size;
		DbgOnlyFatalErrorIf(map->m_size > map->m_avail, "BValueMap created smaller than contents");

		SValue* dst = (SValue*)map->m_maps;
		const SValue* src = (const SValue*)m_maps;
		const SValue* end = src + (m_size*2);
		while (src < end) {
			new (dst) SValue(*src);
			dst++;
			src++;
		}
	}
	return map;
}

void BValueMap::Delete()
{
#if PROFILE_VALUE_MAP
	g_threadDirectFuncs.atomicDec32(&g_numMaps);
#endif
	g_valueMapPool.Delete(this);
}

void BValueMap::SetFirstMap(const SValue& key, const SValue& value)
{
	DbgOnlyFatalErrorIf(m_size != 0, "SetFirstMap called on non-empty BValueMap");
	new (&(m_maps[0].key)) SValue(key);
	new (&(m_maps[0].value)) SValue(value);
	m_size = 1;
	m_dataSize = B_ERROR;
}

ssize_t BValueMap::ComputeArchivedSize() const
{
	const size_t N = CountMaps();
	ssize_t size = 0;
	for (size_t i=0; i<N; i++) {
		const pair& p = MapAt(i);
		size += p.key.ArchivedSize() + p.value.ArchivedSize();
	}
	return (m_dataSize = size);
}

ssize_t BValueMap::Archive(SParcel& into) const
{
	AssertEditing();
	
	ssize_t total = 0, size;
	const size_t N = CountMaps();

	for (size_t i=0; i<N; i++) {
		const pair& p = MapAt(i);
		if ((size=p.key.Archive(into)) < B_OK) return size;
		total += size;
		if ((size=p.value.Archive(into)) < B_OK) return size;
		total += size;
	}
	
	#if VALIDATES_VALUE
		if (total != ArchivedSize()) {
			bout << "Written size: " << total << ", Expected: " << ArchivedSize() << endl;
			bout << "The data so far: " << into << endl;
			ErrFatalError("Cached size wrong!");
		}
	#endif
		
	return total;
}

int32_t BValueMap::Compare(const BValueMap& o) const
{
	const int32_t N = CountMaps();
	const int32_t N2 = o.CountMaps();
	if (N != N2) return N < N2 ? -1 : 1;

	const SValue* p1 = (const SValue*)m_maps;
	const SValue* p2 = (const SValue*)o.m_maps;
	const SValue* e2 = p2 + N*2;
	while (p2 < e2) {
		const int32_t c = p1->Compare(*p2);
		if (c != 0) return c;
		p1++;
		p2++;
	}
	return 0;
}

int32_t BValueMap::LexicalCompare(const BValueMap& o) const
{
	const int32_t N1 = CountMaps();
	const int32_t N2 = o.CountMaps();
	const int32_t N = N1 < N2 ? N1 : N2;

	const SValue* p1 = (const SValue*)m_maps;
	const SValue* p2 = (const SValue*)o.m_maps;
	const SValue* e2 = p2 + N*2;
	while (p2 < e2) {
		const int32_t c = p1->Compare(*p2);
		if (c != 0) return c;
		p1++;
		p2++;
	}

	return N1 < N2 ? -1 : (N2 < N1 ? 1 : 0);
}

ssize_t BValueMap::AddNewMap(BValueMap** This, const SValue& key, const SValue& value)
{
	(*This)->AssertEditing();
	
	if (value.is_defined() && key.is_defined() && (!value.is_wild() || !key.is_wild())) {
		ssize_t index;
		if (!(*This)->GetIndexOf(key, value, reinterpret_cast<size_t*>(&index))) {
			(*This)->m_dataSize = B_ERROR;
			return AddMapAt(This, index, key, value);
		}
		return B_NAME_IN_USE;
	}
	return B_BAD_VALUE;
}

status_t BValueMap::RemoveMap(BValueMap** This, const SValue& key, const SValue& value)
{
	(*This)->AssertEditing();
	
	if (!key.is_wild() || !value.is_wild()) {
		size_t index;
		if ((*This)->GetIndexOf(key, value, &index)) {
			RemoveMapAt(This, index);
			return B_OK;
		}
	} else {
		(*This)->Destroy();
		(*This)->m_size = 0;
		(*This)->m_dataSize = B_ERROR;
	}
	
	return B_NAME_NOT_FOUND;
}

void BValueMap::RemoveMapAt(BValueMap** This, size_t index)
{
	DbgOnlyFatalErrorIf((*This)->m_size < 0 || index >= (size_t)(*This)->m_size, "Bad args to RemoveMapAt()");
	pair* p = (*This)->m_maps + index;
	p->key.~SValue();
	p->value.~SValue();

	// Time to compact?
	ssize_t size = (*This)->m_size-1;
	if ((size*4) < (*This)->m_avail && size > 0) {
		const size_t new_size = size*2;
		BValueMap* more = Create(new_size);
		if (more != NULL) {
			pair* dst = (pair*)more->m_maps;
			if ((ssize_t)index >= size) {
				BMoveBefore((SValue*)dst, (SValue*)p, size*2);
			} else {
				if (index > 0)
					BMoveBefore((SValue*)dst, (SValue*)p, index*2);
				BMoveBefore((SValue*)(dst+index), (SValue*)(p+index+1), (size-index)*2);
			}
			(*This)->m_size = 0;
			(*This)->DecUsers();
			*This = more;
			more->m_size = size;
			more->m_dataSize = B_ERROR;
			return;
		}
	}

	// Just move in-place.
	if ((ssize_t)index < size) {
		BMoveBefore((SValue*)p, (SValue*)(p+1), (size-index)*2);
	}

	(*This)->m_size = size;
	(*This)->m_dataSize = B_ERROR;
}

status_t BValueMap::RenameMap(BValueMap** This, const SValue& old_key, const SValue& new_key)
{
	(*This)->AssertEditing();
	
	if (!old_key.is_wild() && old_key.is_defined() && new_key.is_specified()) {
		ssize_t old_index, new_index;
		if ((*This)->GetIndexOf(old_key, B_WILD_VALUE, reinterpret_cast<size_t*>(&old_index))) {
			const SValue value((*This)->m_maps[old_index].value);
			if (!(*This)->GetIndexOf(new_key, value, reinterpret_cast<size_t*>(&new_index))) {
				RemoveMapAt(This, old_index);
				const ssize_t error = AddMapAt(This, new_index > old_index ? new_index-1 : new_index, new_key, value);
				(*This)->m_dataSize = B_ERROR;
				return error < B_OK ? error : B_OK;
			}
			return B_NAME_IN_USE;
		}
		return B_NAME_NOT_FOUND;
	}
	return B_BAD_VALUE;
}

SValue *BValueMap::BeginEditMapAt(BValueMap** This, size_t index)
{
	DbgOnlyFatalErrorIf((*This)->m_size < 0 || index >= (size_t)(*This)->m_size, "Bad args to RemoveMapAt()");
	ErrFatalErrorIf((*This)->m_editIndex >= 0, "Can't edit more than one item at a time.");
		
	(*This)->m_editIndex = index;
	return &((*This)->m_maps[index].value);
}

void BValueMap::EndEditMapAt(BValueMap** _This)
{
	BValueMap* This = *_This;

	ErrFatalErrorIf(This->m_editIndex < 0, "Value not being edited.");

	pair& p = This->m_maps[This->m_editIndex];
	const int32_t index = This->m_editIndex;
	This->m_editIndex = -1;
	This->m_dataSize = B_ERROR;
	
	if (!p.value.IsDefined()) RemoveMapAt(_This, index);
}

void BValueMap::Pool()
{
	const size_t N = CountMaps();

	for (size_t i=0; i<N; i++) {
		pair& p = m_maps[i];
		p.key.Pool();
		p.value.Pool();
	}
}

bool BValueMap::GetIndexOf(uint32_t type, const void* data, size_t length, size_t* index) const
{
	ssize_t mid, low = 0, high = m_size-1;
	while (low <= high) {
		mid = (low + high)/2;
		const int32_t cmp = m_maps[mid].key.compare(type, data, length);
		if (cmp > 0) {
			high = mid-1;
		} else if (cmp < 0) {
			low = mid+1;
		} else {
			*index = mid;
			return true;
		}
	}
	
	*index = low;
	return false;
}

bool BValueMap::GetIndexOf(const SValue& k, const SValue& v, size_t* index) const
{
	ssize_t mid, low = 0, high = m_size-1;
	while (low <= high) {
		mid = (low + high)/2;
		const int32_t cmp = SValue::compare_map(&m_maps[mid].key, &m_maps[mid].value, &k, &v);
		if (cmp > 0) {
			high = mid-1;
		} else if (cmp < 0) {
			low = mid+1;
		} else {
			*index = mid;
			return true;
		}
	}
	
	*index = low;
	return false;
}

ssize_t BValueMap::AddMapAt(BValueMap** This, size_t index, const SValue& key, const SValue& value)
{
	DbgOnlyFatalErrorIf((*This)->m_size < 0 || index > (size_t)(*This)->m_size, "Bad args to RemoveMapAt()");
	const ssize_t size = (*This)->m_size;
	ErrFatalErrorIf(size < 0, "gotcha!");
	pair* cur = (*This)->m_maps;
	if ((ssize_t)size >= (*This)->m_avail) {
		// Create new, larger, stronger, better BValueMap.
		const size_t new_size = ((size+1)*3)/2;
		BValueMap* more = Create(new_size);
		if (more == NULL) return B_NO_MEMORY;

		// Move into the new digs.
		pair* dst = (pair*)more->m_maps;
		if ((ssize_t)index >= size) {
			BMoveBefore((SValue*)dst, (SValue*)cur, size*2);
		} else {
			if (index > 0)
				BMoveBefore((SValue*)dst, (SValue*)cur, index*2);
			BMoveBefore((SValue*)(dst+index+1), (SValue*)(cur+index), (size-index)*2);
		}

		(*This)->m_size = 0;
		(*This)->DecUsers();

		*This = more;
		cur = dst;

	// Grow in-place.
	} else if ((ssize_t)index < size) {
		BMoveAfter((SValue*)(cur+index+1), (SValue*)(cur+index), (size-index)*2);
	}

	new (&(cur[index].key)) SValue(key);
	new (&(cur[index].value)) SValue(value);
	(*This)->m_size = size+1;
	return (ssize_t)index;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
