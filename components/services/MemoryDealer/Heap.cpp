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

#include <stdio.h>
#include <string.h>
#include <support/StdIO.h>
#include <unistd.h> // BSD, for getpagesize()
#include <sys/mman.h> // POSIX, for mmap, munmap
#include <sys/ipc.h>
#include <sys/shm.h>
#include "Heap.h"
#include "ErrorMgr.h"
#define INTRUSIVE_PROFILING 0
#include <support_p/IntrusiveProfiler.h>
#include <errno.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

/******* Chunking *********/

Chunker::Chunker(Stash<Chunk> *stash, uint32_t size, int32_t quantum) :
	m_freeList(NULL), m_stash(stash)
{
	if (quantum < 1) quantum = 1;
	m_quantum = quantum-1;
	m_quantumMask = 0xFFFFFFFF ^ (quantum-1);
	if (size > 0) Free(0,size);
}

Chunker::~Chunker()
{
	Chunk *p=m_freeList,*n;
	while (p) {
		n = p->next;
		m_stash->Free(p);
		p = n;
	}
}

void Chunker::Reset(uint32_t size)
{
	Chunk *p=m_freeList,*n;
	while (p) {
		n = p->next;
		m_stash->Free(p);
		p = n;
	}
	m_freeList = NULL;
	if (size > 0) Free(0,size);
}

bool Chunker::Alloc(uint32_t start, uint32_t size)
{
	Chunk **p=&m_freeList,*it;

	while (*p && (((*p)->start + (*p)->size) < start)) p = &(*p)->next;

	it = *p;
	if (!it ||
		(it->start > start) ||
		((it->start + it->size) < (start+size))) {
		return false;
	}
	
	if (it->start == start) {
		if (it->size == size) {
			*p = it->next;
			m_stash->Free(it);
			return true;
		}
		
		it->size -= size;
		it->start += size;
		return true;
	}
	
	if ((it->start + it->size) == (start + size)) {
		it->size -= size;
		return true;
	}

	uint32_t freeStart = it->start;
	uint32_t freeSize = it->size;
	it->size = start - freeStart;
	it = m_stash->Alloc();
	it->start = start+size;
	it->size = freeSize - (start - freeStart) - size;
	it->next = (*p)->next;
	(*p)->next = it;
	return true;
}

uint32_t Chunker::PreAbuttance(uint32_t addr)
{
	Chunk **p=&m_freeList;
	while (*p && (((*p)->start + (*p)->size) != addr)) p = &(*p)->next;
	if (!(*p)) return 0;
	return (*p)->size;
}

uint32_t Chunker::PostAbuttance(uint32_t addr)
{
	Chunk **p=&m_freeList;
	while (*p && ((*p)->start != addr)) p = &(*p)->next;
	if (!(*p)) return 0;
	return (*p)->size;
}

uint32_t Chunker::Alloc(uint32_t &size, bool takeBest, uint32_t atLeast)
{
	Chunk **p=&m_freeList,*it,**c;
	uint32_t s=size,start;
	
	while (*p && ((*p)->size < s)) p = &(*p)->next;
	if (!(*p)) {
		if (takeBest) {
			p=&m_freeList;
			c = NULL;
			s = atLeast-1;
			while (*p) {
				if ((*p)->size > s) {
					s = (*p)->size;
					c = p;
				}
				p = &(*p)->next;
			}
			if (c) {
				size = s;
				it = *c;
				*c = (*c)->next;
				start = it->start;
				m_stash->Free(it);
				return start;
			}
		}
		size = 0;
		return NONZERO_NULL;
	}
	
	it = *p;
	s = it->size;
	start = it->start;
	if (s <= (size + m_quantum)) {
		size = s;
		*p = it->next;
		m_stash->Free(it);
	} else {
		size = (size + m_quantum) & m_quantumMask;
		it->size -= size;
		it->start += size;
	}
	
	return start;
}

Chunk const * const Chunker::Free(uint32_t start, uint32_t size)
{
	Chunk *n;
	Chunk *dummy = NULL;
	Chunk **p1=&m_freeList,**p2;

	if (!(*p1)) {
		// we have no other items in the free list
		n = m_stash->Alloc();
		n->start = start;
		n->size = size;
		n->next = m_freeList;
		m_freeList = n;
		return n;
	}

	if ((*p1)->start > start) {
		// free'd region lives ahead of the items in the free list
		p2 = p1;
		p1 = &dummy;
	} else {
		// walk the free list and find the nodes surrounding the free'd region
		p2 = &(*p1)->next;
		while (*p2) {
			if ((*p2)->start > start) break;
			p1 = p2;
			p2 = &(*p2)->next;
		}
	}
	
	// p1 points to the node left of the free'd region
	// p2 points to the node right of the free'd region
	n = *p1;
	if (n && ((n->start + n->size) == start)) {
		// free'd region follows and abutts left node
		n->size += size;
		if (n->next && (n->next->start == (n->start+n->size))) {
			// newly joined region abutts right node
			n = n->next;
			(*p1)->next = n->next;
			(*p1)->size += n->size;
			m_stash->Free(n);
		}
		return *p1;
	}

	n = *p2;
	if (!n || ((start + size) != n->start)) {
		// free'd node fits between but does not abutt either node
		n = m_stash->Alloc();
		n->start = start;
		n->size = size;
		n->next = *p2;
		*p2 = n;
		return n;
	}
	
	// free'd region preceeds and abutts left node
	n->start -= size;
	n->size += size;
	return n;
}

void Chunker::CheckSanity()
{
#if BUILD_TYPE == BUILD_TYPE_DEBUG
	Chunk *p=m_freeList;
	int32_t chunkNum=0;

	while (p->next) {
		if ((p->start + p->size) > p->next->start) {
			bout << SPrintf(	"Chunk %d end location (%08x) is greater "
								"than chunk %d start location (%08x)\n",
								chunkNum,(p->start + p->size),
								chunkNum+1,p->next->start);
			SpewChunks();
			ErrFatalError("Chunker::CheckSanity() false");
		}
		p = p->next;
		chunkNum++;
	}
#endif
}

void Chunker::SpewChunks()
{
#if BUILD_TYPE == BUILD_TYPE_DEBUG
	Chunk *p=m_freeList;
	int32_t chunkNum=0;
	bout << "-------------------------------\n";
	while (p) {
		bout << SPrintf("chunk[%d] : %08x --> %d\n", chunkNum,p->start,p->size);
		p = p->next;
		chunkNum++;
	}
	bout << "-------------------------------\n";
#endif
}

uint32_t Chunker::TotalChunkSpace()
{
	Chunk *p=m_freeList;
	uint32_t total=0;
	
	while (p) {
		total += p->size;
		p = p->next;
	}

	return total;
}

uint32_t Chunker::LargestChunk()
{
	Chunk *p=m_freeList;
	Chunk *l=p;
	
	while (p) {
		if (p->size > l->size) l = p;
		p = p->next;
	}

	return l->size;
}

/******* Hashing *********/

HashTable::HashTable(int32_t buckets)
{
	m_buckets = buckets;
	m_hashTable = (HashEntry**)malloc(sizeof(void*) * m_buckets);
	for (int32_t i=0;i<m_buckets;i++) m_hashTable[i] = NULL;
}

HashTable::~HashTable()
{
	free(m_hashTable);
}

void HashTable::Insert(uint32_t key, void *ptr)
{
	int32_t bucket;
	HashEntry *n = m_stash.Alloc();
	n->car = ptr;
	n->key = key;
	bucket = Hash(key);
	n->next = m_hashTable[bucket];
	m_hashTable[bucket] = n;
}

void * HashTable::Retrieve(uint32_t key)
{
	uint32_t bucket = Hash(key);
	HashEntry **p = &m_hashTable[bucket];
	while (*p && ((*p)->key != key)) p = &(*p)->next;
	if (!(*p)) return NULL;
	return (*p)->car;
}

void HashTable::Remove(uint32_t key)
{
	uint32_t bucket = Hash(key);
	HashEntry **p = &m_hashTable[bucket],*it;
	while (*p && ((*p)->key != key)) p = &(*p)->next;
	if (!(*p)) return;

	it = *p;
	*p = (*p)->next;
	m_stash.Free(it);
}

uint32_t HashTable::Hash(uint32_t address)
{
	return address % m_buckets;
}

/***************************/

Hasher::Hasher(Stash<Chunk> *stash)
{
	m_hashTable = (Chunk**)malloc(64 * sizeof(Chunk*));
	for (int32_t i=0;i<64;i++) m_hashTable[i] = NULL;
	m_stash = stash;
}

Hasher::~Hasher()
{
	free(m_hashTable);
}

void Hasher::RemoveAll()
{
	for (int32_t i=0;i<64;i++) {
		Chunk *p = m_hashTable[i];
		while (p) {
			Chunk *next = p->next;
			m_stash->Free(p);
			p = next;
		}
		m_hashTable[i] = NULL;
	}
}

void Hasher::Insert(uint32_t address, uint32_t size)
{
	int32_t bucket;
	Chunk *n = m_stash->Alloc();
	n->start = address;
	n->size = size;
	bucket = Hash(address);
	n->next = m_hashTable[bucket];
	m_hashTable[bucket] = n;
}

void Hasher::Retrieve(uint32_t address, uint32_t **ptrToSize)
{
	uint32_t bucket = Hash(address);
	Chunk **p = &m_hashTable[bucket];
	while (*p && ((*p)->start != address)) p = &(*p)->next;
	if (!(*p)) {
		if (ptrToSize) *ptrToSize = NULL;
		return;
	}
	
	if (ptrToSize) *ptrToSize = &(*p)->size;
}

uint32_t Hasher::Remove(uint32_t address)
{
	uint32_t bucket = Hash(address);
	Chunk **p = &m_hashTable[bucket],*it;
	while (*p && ((*p)->start != address)) p = &(*p)->next;
	if (!(*p)) return 0;
	
	it = *p;
	*p = it->next;
	bucket = it->size;
	m_stash->Free(it);
	return bucket;
}

uint32_t Hasher::Hash(uint32_t address)
{
	uint32_t h=address,r;
	h = h ^ (h >> 8);
	h = h ^ (h >> 16);
	h = h ^ (h >> 24);
	r = h & 7;
	h = ((h&0xFF)>>(r)) | ((h&0xFF)<<(8-r));
	h &= 63;

	return h;
}

/******* Areas *********/

SLocker Area::m_areaAllocLock("Area Alloc Lock");

Area::Area(const char *name, uint32_t size, bool mapped)
{
	m_areaAllocLock.Lock();
	
	char buf[64];
	int i;
	int fd = -1;
	
	if (size == 0) {
		size = 8*1024*1024;
		printf("[MemoryDealer] Defaulting to allocation of %d\n", size);
	}
	

	PAGE_SIZE = getpagesize();
	m_size = (size+getpagesize()-1)/getpagesize();
	
#if 1
	/* shm implementation */
	
	int id = shmget(IPC_PRIVATE, m_size*getpagesize(), 0666);
	if (id == -1) {
		fprintf(stderr, "[MemoryDealer] Problem obtaining shared segment: %s\n", strerror(errno));
		abort();
	}

	m_area = m_area_public_id = id;
	m_basePtr = shmat(id, NULL, 0);
	if (m_basePtr == 0) {
		fprintf(stderr, "[MemoryDealer] Failed to attach segment: %s\n", strerror(errno));
		m_area = B_NO_MEMORY;
	} else {
		shmctl(id, IPC_RMID, 0); /* Set IPC segment as 'destroyed' so that it will automatically
					    be freed when there are no more refcounts on it, instead of
					    living eternally until manually removed. */
	}
	                     
#else
	/* mmap() implementation */
	for (i=1;i<10000;i++) {
		sprintf(buf, MEMORYDEALER_NAME_TEMPLATE, getenv("PALMOS_KALNAME") ?: "", i);
		fd = open(buf, O_RDWR|O_EXCL|O_CREAT,0666);
		if (fd != -1)
			break;
	}
	
	if (i==10000) {
		fprintf(stderr, "[MemoryDealer]: unable to get filename %s, maybe /tmp needs to be cleaned up?\n", buf);
		abort();
	}
	
	printf("[MemoryDealer] Shared area file name is '%s'\n", buf);
	
	ftruncate(fd, m_size*getpagesize());
	
	m_area = m_area_public_id = i;
	m_basePtr = mmap(0, m_size*getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	
	if (m_basePtr == 0) {
		m_area = B_NO_MEMORY;
	}
#endif

	printf("[MemoryDealer] Successfully allocated area named '%s' of size %d (at address %x in owning process %d)\n", name, size, m_basePtr, m_size*getpagesize());

	if (m_area == B_NO_MEMORY) {
#if BUILD_TYPE == BUILD_TYPE_DEBUG
		bout << SPrintf("Area::Area(\"%s\",%d): Kernel doesn't agree!\n",name,size);
#endif
		m_basePtr = NULL;
		m_areaAllocLock.Unlock();
		return;
	}

	m_memoryHeap = new BMemoryHeap(m_area_public_id, m_basePtr);

	m_areaAllocLock.Unlock();
}

Area::~Area()
{
	m_areaAllocLock.Lock();
	m_basePtrLock.WriteLock();
	
#if 1
	/* shm */
	if (shmdt(m_basePtr) == -1) {
		fprintf(stderr, "[MemoryDealer] Error releasing primary memory heap with shm base %d: %s\n", m_basePtr, strerror(errno));
	}
#else
	/* mmap */
	char name[64];
	sprintf(name, MEMORYDEALER_NAME_TEMPLATE, getenv("PALMOS_KALNAME") ?: "", m_area);
	if (munmap(m_basePtr, m_size) == -1) {
		fprintf(stderr, "[MemoryDealer] Error releasing primary memory heap with mmap base %d: %s\n", m_basePtr, strerror(errno));
	}
	unlink(name);
#endif
	
	m_areaAllocLock.Unlock();
}

void Area::BasePtrLock()
{
	m_basePtrLock.ReadLock();
}

void Area::BasePtrUnlock()
{
	m_basePtrLock.ReadUnlock();
}

uint32_t Area::ResizeTo(uint32_t newSize, bool acceptBest)
{
	(void)newSize; (void)acceptBest;
	return B_UNSUPPORTED;
}

status_t Area::Map(uint32_t ptr, uint32_t size)
{
	/* Fixme: probably madvise to verify pages will be available */
	return errNone;
}

status_t Area::Unmap(uint32_t ptr, uint32_t size)
{
	/* Fixme: probably madvise to say the pages can be discarded */
	return errNone;
}

/******* Heaping *********/

uint32_t Heap::Alloc(uint32_t size)
{
	return (uint32_t)malloc(size);
}

uint32_t Heap::Realloc(uint32_t ptr, uint32_t newSize)
{
	return (uint32_t)realloc((void*)ptr,newSize);
}

void Heap::Free(uint32_t ptr)
{
	free((void*)ptr);
}

void Heap::Lock()
{
}

void Heap::Unlock()
{
}

uint32_t Heap::Offset(void *ptr)
{
	return (uint32_t)ptr;
}

void * Heap::Ptr(uint32_t offset)
{
	return (void *)offset;
}

/******************************************/

DPT(HA_HeapArea);
HeapArea::HeapArea(const char *name, uint32_t size, uint32_t quantum, bool mapped) :
	Area(name,size,mapped), m_freeList(&m_stash,this->Size(),quantum), m_allocs(&m_stash), m_size(0), m_mapped(0)
{
	DAP(HA_HeapArea);
	if (!mapped)
	{
		m_size = ((Size() / PAGE_SIZE) + 31) / 32;
		m_mapped = new uint32_t [m_size];
		for (uint32_t i = 0; i < m_size; i++) m_mapped[i] = 0;
	}
#define HEAP_TORTURE_TEST 0
#if HEAP_TORTURE_TEST > 0
	bout << SPrintf("  MapRequired(0, %lu) : %08lx", Size(), MapRequired(0, Size())) << endl;		// map it all
	bout << SPrintf("UnmapRequired(0, %lu) : %08lx", Size(), UnmapRequired(0, Size())) << endl;	// dump it all
	uint32_t addr = Alloc(Size());
	bout << SPrintf("Alloc(Size()) : %08lx (expect zero)", addr) << endl;
	Free(addr);
	// assume we have enough room to allocate PAGE_SIZE sized chunks at each of the possible 32 PAGE_SIZE boundaries.
	for (uint32_t j = 0; j < 32; j++)
	{
		uint32_t slew = NONZERO_NULL;
		if (j) slew = Alloc(j * PAGE_SIZE);
		// uint32_t p[32];
		for (uint32_t i = 0; i < 64; i++) {
			uint32_t p = Alloc((i+1) * PAGE_SIZE);
			bout << SPrintf("%04lx %04lx %08lx %08lx", j, i, (i+1)*PAGE_SIZE, p) << endl;
			Free(p);
		}
		if (j) Free(slew);
	}
#endif
}

DPT(HA_MapRequired);
status_t HeapArea::MapRequired(uint32_t ptr, uint32_t size)
{
	DAP(HA_MapRequired);
	status_t result = errNone;
	if (m_size)
	{
		uint32_t       first = ( ptr         & ~(PAGE_SIZE-1)) / PAGE_SIZE;
		uint32_t const last  = ((ptr+size-1) & ~(PAGE_SIZE-1)) / PAGE_SIZE;
		uint32_t const last_index = last / 32;
		while (first <= last)
		{
			uint32_t index = first / 32;
			uint32_t top_mask = ~((1U << (first % 32)) - 1);
			uint32_t bottom_mask = ~0;
			if (index == last_index) bottom_mask >>= 31 - (last % 32);
			uint32_t const new_pages = (top_mask & bottom_mask) & ~m_mapped[index];
			uint32_t new_bits = new_pages;
			uint32_t start = 0;
			uint32_t length = 0;
			while (new_bits)
			{
				while (!(new_bits & 1))
				{
					start++;
					new_bits >>= 1;
				}
				while (new_bits & 1)
				{
					length++;
					new_bits >>= 1;
				}
				result = Map((index * 32 + start) * PAGE_SIZE, length * PAGE_SIZE);
				if (result != errNone) goto error0;

				uint32_t mask = (length == 32) ? ~0 : ((1U << length)-1) << start;
				// bout << SPrintf("Before: m_mapped[%d] = %08lx, mask: %08lx", index, m_mapped[index], mask) << endl;
				m_mapped[index] |= mask;
				// bout << SPrintf(" After: m_mapped[%d] = %08lx", index, m_mapped[index]) << endl;
				start += length;
				length = 0;
			}
			first = (first + 32) & ~31;
		}
	}
error0:
	return result;
}

DPT(HA_UnmapRequired);
status_t HeapArea::UnmapRequired(uint32_t ptr, uint32_t size)
{
	DAP(HA_UnmapRequired);
	status_t result = errNone;
	if (m_size && size)
	{
		uint32_t       first = ( ptr         & ~(PAGE_SIZE-1)) / PAGE_SIZE;
		uint32_t const last  = ((ptr+size-1) & ~(PAGE_SIZE-1)) / PAGE_SIZE;
		uint32_t const last_index = last / 32;
		while (first <= last)
		{
			uint32_t index = first / 32;
			uint32_t top_mask = ~((1U << (first % 32)) - 1);
			uint32_t bottom_mask = ~0;
			if (index == last_index) bottom_mask >>= 31 - (last % 32);
			uint32_t const new_pages = (top_mask & bottom_mask) & m_mapped[index];
			uint32_t new_bits = new_pages;
			uint32_t start = 0;
			uint32_t length = 0;
			while (new_bits)
			{
				while (!(new_bits & 1))
				{
					start++;
					new_bits >>= 1;
				}
				while (new_bits & 1)
				{
					length++;
					new_bits >>= 1;
				}
				result = Unmap((index * 32 + start) * PAGE_SIZE, length * PAGE_SIZE);
				if (result != errNone) goto error0;

				uint32_t mask = (length == 32) ? ~0 : ((1U << length)-1) << start;
				// bout << SPrintf("Before: m_mapped[%d] = %08lx, mask: %08lx", index, m_mapped[index], mask) << endl;
				m_mapped[index] &= ~mask;
				// bout << SPrintf(" After: m_mapped[%d] = %08lx", index, m_mapped[index]) << endl;
				start += length;
				length = 0;
			}
			first = (first + 32) & ~31;
		}
	}
error0:
	return result;
}

DPT(HA_Alloc);
uint32_t HeapArea::Alloc(uint32_t size)
{
	DAP(HA_Alloc);
	m_heapLock.Lock();

	uint32_t s,p,abutt,i;
	s = size;
	p = m_freeList.Alloc(s);
	if (s < size) {
		abutt = m_freeList.PreAbuttance(Size());
		s = size - abutt;
		p = Size();
		if (s % PAGE_SIZE) s = ((s/PAGE_SIZE)+1)*PAGE_SIZE;
		i = ResizeTo(s+p);
		if (i!=(s+p)) {
			/*	Ack!  We weren't able to resize the area, even by moving it,
				to get the needed space.  Oh, well... */
#if BUILD_TYPE == BUILD_TYPE_DEBUG
			bout << SPrintf("Heap::Alloc(%d): Can't resize area!\n",size);
			bout << SPrintf("                 abutt=%d, s=%d, p=%d, i=%d\n",abutt,s,p,i);
			DumpFreeList();
#endif
			m_heapLock.Unlock();
			return NONZERO_NULL;
		}
		m_freeList.Free(p,s);
		s = size;
		p = m_freeList.Alloc(s);
		if (s < size) {
#if BUILD_TYPE == BUILD_TYPE_DEBUG
			/* There's a bug in my code, and for some reason we can't allocate
				the needed space, even through the area was resized okay. */
			bout << SPrintf("Heap::Alloc(%d): Area resized but still can't alloc!\n",size);
#endif
			m_heapLock.Unlock();
			return NONZERO_NULL;
		}
	}

	m_allocs.Insert(p,s);
	if (MapRequired(p,s) != errNone)
	{
		m_allocs.Remove(p);
		Chunk const * const region = m_freeList.Free(p,s);
		uint32_t right = (p+s+(PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
		uint32_t left = p & ~(PAGE_SIZE-1);
		if (left < p) left += PAGE_SIZE;
		if ((region->start + region->size) < right) right -= PAGE_SIZE;
		if (left < right) UnmapRequired(left, right - left);
		p = NONZERO_NULL;
	}
	m_heapLock.Unlock();
	return p;
}

DPT(HA_Realloc);
uint32_t HeapArea::Realloc(uint32_t ptr, uint32_t newSize)
{
	DAP(HA_Realloc);
	if (ptr == NONZERO_NULL) return Alloc(newSize);
	m_heapLock.Lock();
	uint32_t *size,r = NONZERO_NULL;
	m_allocs.Retrieve(ptr,&size);
	if (*size != 0) {
		uint32_t quantizedSize =	(newSize + m_freeList.m_quantum) &
								m_freeList.m_quantumMask;
		if (quantizedSize == *size) {
			r = ptr;
			goto leave;
		}

		if (quantizedSize < *size) {
			m_freeList.Free(ptr+quantizedSize,*size-quantizedSize);
			*size = quantizedSize;
		} else {
			uint32_t oldSize = m_allocs.Remove(ptr);
			m_freeList.Free(ptr,*size);
			/*	The data @ptr is still valid, because we have the heapLock */
			uint32_t newPtr = Alloc(newSize);
			BasePtrLock();
			memmove(Ptr(newPtr),Ptr(ptr),oldSize);
			BasePtrUnlock();
			r = newPtr;
		}
	}

	leave:
	m_heapLock.Unlock();
	return r;
}

DPT(HA_Free);
void HeapArea::Free(uint32_t ptr)
{
	DAP(HA_Free);
	if (ptr == NONZERO_NULL) return;
	m_heapLock.Lock();
		uint32_t size = m_allocs.Remove(ptr);
		if (size != 0)
		{
			Chunk const * const region = m_freeList.Free(ptr,size);
			uint32_t right = (ptr+size+(PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
			uint32_t left = ptr & ~(PAGE_SIZE-1);
			if (left < ptr) left += PAGE_SIZE;
			if ((region->start + region->size) < right) right -= PAGE_SIZE;
			if (left < right) UnmapRequired(left, right - left);
		}
	m_heapLock.Unlock();
}

void HeapArea::Lock()
{
	BasePtrLock();
}

void HeapArea::Unlock()
{
	BasePtrUnlock();
}

uint32_t HeapArea::Offset(void *ptr)
{
	return Ptr2Offset(ptr);
}

void * HeapArea::Ptr(uint32_t offset)
{
	return Offset2Ptr(offset);
}

HeapArea::~HeapArea()
{
	UnmapRequired(0, Size());
	delete [] m_mapped;
}

DPT(HA_Reset);
void HeapArea::Reset()
{
	DAP(HA_Reset);
	m_heapLock.Lock();
		m_freeList.Reset(Size());
		m_allocs.RemoveAll();
		UnmapRequired(0, Size());
	m_heapLock.Unlock();
}

uint32_t HeapArea::FreeSpace()
{
	return m_freeList.TotalChunkSpace();
}

uint32_t HeapArea::LargestFree()
{
	return m_freeList.LargestChunk();
}

void HeapArea::CheckSanity()
{
	m_freeList.CheckSanity();
}

void HeapArea::DumpFreeList()
{
	m_freeList.SpewChunks();
}
