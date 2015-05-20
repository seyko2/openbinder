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

#include <support/Vector.h>
#include <support/Debug.h>

#include <stdlib.h>

#ifndef SUPPORTS_VECTOR_PROFILING
#if BUILD_TYPE == BUILD_TYPE_DEBUG
#define SUPPORTS_VECTOR_PROFILING 1
#else
#define SUPPORTS_VECTOR_PROFILING 0
#endif
#endif

#if SUPPORTS_VECTOR_PROFILING

#include <support/atomic.h>
#include <support/Debug.h>
#include <stdio.h>
#include <stdlib.h>

#include <support_p/SupportMisc.h>

#if _SUPPORTS_WARNING
#warning Compiling for SVector profiling!
#endif

#define m_maxSize _reserved[0]
#define m_maxAvail _reserved[1]

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

char g_vectorProfileEnvVar[] = "VECTOR_PROFILE";
static BDebugCondition<g_vectorProfileEnvVar, 0, 0, 10000> g_profileLevel;
char g_vectorProfileDumpPeriod[] = "VECTOR_PROFILE_DUMP_PERIOD";
static BDebugInteger<g_vectorProfileDumpPeriod, 1000, 0, 10000> g_profileDumpPeriod;

struct hist_item
{
	size_t size;
	size_t num;
};

struct vector_hist
{
	size_t		count;
	hist_item*	items;
	
	vector_hist()
		:	count(0), items(NULL)
	{
	}
	vector_hist(const vector_hist& o)
		:	count(0), items(NULL)
	{
		*this = o;
	}
	~vector_hist()
	{
		if (items) free(items);
	}
	
	vector_hist& operator=(const vector_hist& o)
	{
		if (items) free(items);
		count = o.count;
		
		if (o.items) {
			items = (hist_item*)malloc(sizeof(hist_item)*count);
			if (items) memcpy(items, o.items, sizeof(hist_item)*count);
			else count = 0;
		} else {
			items = NULL;
		}
		
		return *this;
	}
	
	void accum(size_t size)
	{
	#if 1
		size_t low;
		for (low=0; low<count; low++) {
			if (items[low].size >= size) {
				if (items[low].size == size) {
					items[low].num++;
					return;
				}
				break;
			}
		}
	#else
		ssize_t mid, low = 0, high = count-1;
		while (low <= high) {
			mid = (low + high)/2;
			if (size < items[mid].size) {
				high = mid-1;
			} else if (size > items[mid].size) {
				low = mid+1;
			} else {
				items[mid].num++;
				return;
			}
		}
	#endif
	
		items = (hist_item*)realloc(items, sizeof(hist_item)*(count+1));
		if (items) {
			if (low < count)
				memmove(items+low+1, items+low, sizeof(hist_item)*(count-low));
			count++;
			items[low].size = size;
			items[low].num = 1;
		} else {
			count = 0;
		}
	}
	
	void print()
	{
		for (size_t i=0; i<count; i++) {
			printf("\t\t%ld: %ld\n", items[i].size, items[i].num);
		}
	}
};

struct vector_stats
{
	mutable int32_t		lockCount;
	mutable sem_id		lockSem;
	
	size_t				hits;
	size_t				stackToHeap;
	size_t				heapToStack;
	vector_hist			elementSize;
	vector_hist			usedCount;
	vector_hist			availCount;
	vector_hist			usedBytes;
	vector_hist			availBytes;
	vector_hist			growToCount;
	vector_hist			shrinkToCount;
	vector_hist			growToBytes;
	vector_hist			shrinkToBytes;
	
	vector_stats()
	{
		lockCount = 0;
		SysSemaphoreCreateEZ(0, &lockSem);
		hits = 0;
		stackToHeap = heapToStack = 0;
	}
	vector_stats(const vector_stats& o)
	{
		lockCount = 0;
		SysSemaphoreCreateEZ(0, &lockSem);
		o.lock();
		hits = o.hits;
		stackToHeap = o.stackToHeap;
		heapToStack = o.heapToStack;
		elementSize = o.elementSize;
		usedCount = o.usedCount;
		availCount = o.availCount;
		usedBytes = o.usedBytes;
		availBytes = o.availBytes;
		growToCount = o.growToCount;
		shrinkToCount = o.shrinkToCount;
		growToBytes = o.growToBytes;
		shrinkToBytes = o.shrinkToBytes;
		o.unlock();
	}
	~vector_stats()
	{
		SysSemaphoreDestroy(lockSem);
	}
	
	void lock() const
	{
		if (g_threadDirectFuncs.atomicInc32(&lockCount) >= 1) {
			SysSemaphoreWait(lockSem, B_WAIT_FOREVER, 0);
		}
	}
	
	void unlock() const
	{
		if (g_threadDirectFuncs.atomicDec32(&lockCount) > 1){
			SysSemaphoreSignal(lockSem);
		}
	}
	
	bool accumDestroy(size_t used, size_t avail, size_t elem)
	{
		lock();
		hits++;
		const bool print_it =
			(g_profileDumpPeriod.Get() > 0) && ((hits%g_profileDumpPeriod.Get()) == 0);
		elementSize.accum(elem);
		usedCount.accum(used);
		availCount.accum(avail);
		usedBytes.accum(used*elem);
		availBytes.accum(avail*elem);
		unlock();
		return print_it;
	}

	void accumGrow(size_t newSize, size_t elem)
	{
		lock();
		growToCount.accum(newSize);
		growToBytes.accum(newSize*elem);
		unlock();
	}

	void accumShrink(size_t newSize, size_t elem)
	{
		lock();
		shrinkToCount.accum(newSize);
		shrinkToBytes.accum(newSize*elem);
		unlock();
	}

	void accumStackToHeap()
	{
		lock();
		stackToHeap++;
		unlock();
	}
	
	void accumHeapToStack()
	{
		lock();
		heapToStack++;
		unlock();
	}
	
	// NOTE: Can't use bout here because it uses SAtom and SLocker, which
	// in turn use SVector for their debugging implementations.
	void print()
	{
		vector_stats v(*this);
		printf("Stats For Vector Usage\n");
		printf("----------------------\n");
		printf("\tTotal Vectors Destroyed: %ld\n", v.hits);
		printf("\tJumps from stack to heap: %ld / from heap to stack: %ld\n",
				v.stackToHeap, v.heapToStack);
		printf("\tVector Bytes Per Element Histogram:\n");
		v.elementSize.print();
		printf("\tVector Maximum Used Count Histogram:\n");
		v.usedCount.print();
		printf("\tVector Maximum Avail Count Histogram:\n");
		v.availCount.print();
		printf("\tVector Maximum Used Bytes Histogram:\n");
		v.usedBytes.print();
		printf("\tVector Maximum Avail Bytes Histogram:\n");
		v.availBytes.print();
		printf("\tVector Grow To Count Histogram:\n");
		v.growToCount.print();
		printf("\tVector Shrink To Count Histogram:\n");
		v.shrinkToCount.print();
		printf("\tVector Grow To Bytes Histogram:\n");
		v.growToBytes.print();
		printf("\tVector Shrink To Bytes Histogram:\n");
		v.shrinkToBytes.print();
	}
};

static BDebugState<vector_stats> g_stats;

static void AccumVectorStats(size_t used, size_t avail, size_t elem)
{
	if (g_profileLevel.Get() > 0) {
		vector_stats* stats = g_stats.Get();
		if (stats && stats->accumDestroy(used, avail, elem)) {
			stats->print();
		}
	}
}

static void AccumVectorGrow(size_t newSize, size_t elem)
{
	if (g_profileLevel.Get() > 0) {
		vector_stats* stats = g_stats.Get();
		if (stats) stats->accumGrow(newSize, elem);
	}
}

static void AccumVectorShrink(size_t newSize, size_t elem)
{
	if (g_profileLevel.Get() > 0) {
		vector_stats* stats = g_stats.Get();
		if (stats) stats->accumShrink(newSize, elem);
	}
}

static void AccumVectorStackToHeap()
{
	if (g_profileLevel.Get() > 0) {
		vector_stats* stats = g_stats.Get();
		if (stats) stats->accumStackToHeap();
	}
}

static void AccumVectorHeapToStack()
{
	if (g_profileLevel.Get() > 0) {
		vector_stats* stats = g_stats.Get();
		if (stats) stats->accumHeapToStack();
	}
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif


#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

inline const uint8_t* SAbstractVector::data() const
{
#if BUILD_TYPE == BUILD_TYPE_DEBUG
	// Verify m_base is correct.
	const uint8_t* expected = (m_avail <= m_localSpace)
		? (const uint8_t*)m_data.local : (const uint8_t*)m_data.heap;
	DbgOnlyFatalErrorIf(expected != m_base, "SAbstractVector: m_base is incorrect!");
#endif
	return m_base;
}

inline uint8_t* SAbstractVector::edit_data()
{
#if BUILD_TYPE == BUILD_TYPE_DEBUG
	// Verify m_base is correct.
	const uint8_t* expected = (m_avail <= m_localSpace)
		? (const uint8_t*)m_data.local : (const uint8_t*)m_data.heap;
	DbgOnlyFatalErrorIf(expected != m_base, "SAbstractVector: m_base is incorrect!");
#endif
	return m_base;
}

SAbstractVector::SAbstractVector(size_t element_size)
	:	m_elementSize(element_size),
		m_size(0),
		m_base(m_data.local),
		m_localSpace(sizeof(m_data)/m_elementSize),
		m_avail(sizeof(m_data)/m_elementSize)
{
#if SUPPORTS_VECTOR_PROFILING
	if (g_profileLevel.Get() > 0) {
		m_maxSize = m_size;
		m_maxAvail = m_avail;
	}
#endif
}

SAbstractVector::SAbstractVector(const SAbstractVector& o)
	:	m_elementSize(o.m_elementSize),
		m_size(0),
		m_base(m_data.local),
		m_localSpace(sizeof(m_data)/m_elementSize),
		m_avail(sizeof(m_data)/m_elementSize)
{
	if (o.m_size > 0) {
		uint8_t* dest = grow(o.m_size);
		const uint8_t* src = o.data();
		if (dest && src) {
			o.PerformCopy(dest, src, o.m_size);
			m_size = o.m_size;
		}
	}
#if SUPPORTS_VECTOR_PROFILING
	if (g_profileLevel.Get() > 0) {
		m_maxSize = m_size;
		m_maxAvail = m_avail;
	}
#endif
}

SAbstractVector::~SAbstractVector()
{
#if SUPPORTS_VECTOR_PROFILING
	AccumVectorStats(m_maxSize, m_maxAvail, m_elementSize);
#endif

	DbgOnlyFatalErrorIf(m_size != 0, "SAbstractVector: subclass must call MakeEmpty() in destructor");
}

SAbstractVector&	SAbstractVector::operator=(const SAbstractVector& o)
{
	if (m_elementSize == o.m_elementSize) {
		if (o.m_size > 0) {
			if (m_size > 0) {
				uint8_t* cur = edit_data();
				PerformDestroy(cur, m_size);
				m_size = 0;
			}
			uint8_t* dest = grow(o.m_size);
			const uint8_t* src = o.data();
			if (dest && src) {
				PerformCopy(dest, src, o.m_size);
				m_size = o.m_size;
			}
		} else {
			MakeEmpty();
		}
	
	} else {
		ErrFatalError("SAbstractVector element sizes do not match");
	}
	
#if SUPPORTS_VECTOR_PROFILING
	if (g_profileLevel.Get() > 0) {
		if (m_maxSize < (int32_t)m_size) m_maxSize = m_size;
		if (m_maxAvail < (int32_t)m_avail) m_maxAvail = m_avail;
	}
#endif

	return *this;
}

void* SAbstractVector::EditArray()
{
	if (m_size == 0) return NULL;
	return edit_data();
}

status_t SAbstractVector::SetSize(size_t total_count, const void *protoElement)
{
	if (total_count > m_size) {
		uint8_t* d = grow(total_count - m_size);
		if (d) {
			if (protoElement)
				PerformReplicate(d+(m_size*m_elementSize), protoElement, total_count - m_size);
			else
				PerformConstruct(d+(m_size*m_elementSize), total_count - m_size);
			m_size = total_count;
		
		} else {
			return B_NO_MEMORY;
		}
	}
	else {
		RemoveItemsAt(total_count, m_size - total_count);
	}
	
	return B_OK;
}

ssize_t SAbstractVector::Add(const void* newElement)
{
	uint8_t* d = grow(1);
	if (d) {
		if (newElement)
			PerformCopy(d+(m_size*m_elementSize), newElement, 1);
		else
			PerformConstruct(d+(m_size*m_elementSize), 1);
		return m_size++;
	}
	return B_NO_MEMORY;
}

ssize_t SAbstractVector::AddAt(const void* newElement, size_t index)
{
	if (index > m_size) index = m_size;
	uint8_t* d = grow(1, 3, index);
	if (d) {
		m_size++;
		if (newElement)
			PerformCopy(d+(index*m_elementSize), newElement, 1);
		else
			PerformConstruct(d+(index*m_elementSize), 1);
		return index;
	}
	return B_NO_MEMORY;
}

void* SAbstractVector::EditAt(size_t index)
{
	uint8_t* d = edit_data();
	if (d) return d+(index*m_elementSize);
	return NULL;
}

ssize_t SAbstractVector::AddVector(const SAbstractVector& o)
{
	return AddVectorAt(o, m_size);
}

ssize_t SAbstractVector::AddVectorAt(const SAbstractVector& o, size_t index)
{
	if (m_elementSize == o.m_elementSize) {
		const uint8_t* src = o.data();
		if (o.m_size > 0 && src != NULL) {
			if (index > m_size) index = m_size;
			uint8_t* d = grow(o.m_size, 3, index);
			if (d) {
				PerformCopy(d+(index*m_elementSize), src, o.m_size);
				return m_size+=o.m_size;
			}
			return B_NO_MEMORY;
		}
		return m_size;
	}
	
	ErrFatalError("SAbstractVector element sizes do not match");
	return B_BAD_TYPE;
}

ssize_t SAbstractVector::AddArray(const void* array, size_t count)
{
	return AddArrayAt(array, count, m_size);
}

ssize_t SAbstractVector::AddArrayAt(const void* array, size_t count, size_t index)
{
	const uint8_t* src = reinterpret_cast<const uint8_t*>(array);
	if (count > 0 && src != NULL) {
		if (index > m_size) index = m_size;
		uint8_t* d = grow(count, 3, index);
		if (d) {
			PerformCopy(d+(index*m_elementSize), src, count);
			return m_size+=count;
		}
		return B_NO_MEMORY;
	}
	return m_size;
}

ssize_t SAbstractVector::ReplaceAt(const void* newItem, size_t index)
{
	void* elem = EditAt(index);
	if (elem) {
		PerformAssign(elem, newItem, 1);
		return (ssize_t) index;
	}
	return B_NO_MEMORY;
}

void SAbstractVector::RemoveItemsAt(size_t index, size_t count)
{
	if (count > 0 && index < m_size) {
		if ((index+count) > m_size) count = m_size-index;
		if (count > 0) {
			PerformDestroy(edit_data()+(index*m_elementSize), count);
			shrink(count, 4, index);
			m_size -= count;
		}
	}
}

status_t SAbstractVector::MoveItems(size_t newIndex, size_t oldIndex, size_t count)
{
	if (count > 0 && oldIndex < m_size) {
		if ((oldIndex+count) > m_size) count = m_size-oldIndex;
		if ((newIndex+count) > m_size) newIndex = m_size-count;
		if (count > 0 && oldIndex != newIndex) {
			// Need a temporary buffer to move the items.
			uint8_t localBuffer[32];
			uint8_t* buffer = localBuffer;
			if ((m_elementSize*count) > sizeof(localBuffer)) {
				buffer = (uint8_t*)malloc(m_elementSize*count);
				if (buffer == NULL) return B_NO_MEMORY;
			}

			uint8_t* d = edit_data();

			// First copy the 'count' items at the old position into our buffer.
			PerformMoveBefore(buffer, d+(oldIndex*m_elementSize), count);
			// Now shift all items between the old and new indexes (except the ones
			// we copied above) to make room at the new location.
			if (newIndex < oldIndex)
				PerformMoveAfter(d+((newIndex+count)*m_elementSize), d+(newIndex*m_elementSize), oldIndex-newIndex);
			else
				PerformMoveBefore(d+(oldIndex*m_elementSize), d+((oldIndex+count)*m_elementSize), newIndex-oldIndex);
			// Finally copy the buffer back in to the new position.
			PerformMoveBefore(d+(newIndex*m_elementSize), buffer, count);

			if (buffer != localBuffer) free(buffer);
		}
	}

	return B_OK;
}

void SAbstractVector::MakeEmpty()
{
	uint8_t* d = edit_data();
	if (d) {
		if (m_size > 0)
			PerformDestroy(d, m_size);
		if (d != m_data.local) {
#if SUPPORTS_VECTOR_PROFILING
			AccumVectorHeapToStack();
#endif
			free(d);
		}
	}
	m_avail = m_localSpace;
	m_size = 0;
	m_base = m_data.local;
}

void SAbstractVector::SetCapacity(size_t total_space)
{
	if (total_space < m_size) total_space = m_size;
	PRINT(("SetCapacity: requested %ld, have %ld, using %ld, grow=%ld\n",
			total_space, m_avail, m_size, total_space-m_size));
	grow(total_space-m_size, 2);
}

size_t SAbstractVector::Capacity() const
{
	return m_avail;
}

void SAbstractVector::SetExtraCapacity(size_t extra_space)
{
	grow(extra_space, 2);
}

void SAbstractVector::MoveBefore(SAbstractVector* to, SAbstractVector* from, size_t count)
{
	memcpy(to, from, sizeof(SAbstractVector)*count);
	while (count > 0) {
		if (from->m_avail <= from->m_localSpace && from->m_size > 0) {
			from->PerformMoveBefore(to->m_data.local, from->m_data.local, from->m_size*from->m_elementSize);
		}
		count--;
		to++;
		from++;
	}
}

void SAbstractVector::MoveAfter(SAbstractVector* to, SAbstractVector* from, size_t count)
{
	memmove(to, from, sizeof(SAbstractVector)*count);
	while (count > 0) {
		count--;
		if (from[count].m_avail <= from[count].m_localSpace && from[count].m_size > 0) {
			from[count].PerformMoveBefore(to[count].m_data.local, from[count].m_data.local, from[count].m_size*from[count].m_elementSize);
		}
	}
}

void SAbstractVector::Swap(SAbstractVector& o)
{
	uint8_t buffer[sizeof(SAbstractVector)];
	memcpy(buffer, this, sizeof(SAbstractVector));
	if (m_avail <= m_localSpace && m_size > 0) {
		PerformMoveBefore(((SAbstractVector*)buffer)->m_data.local, m_data.local, 1);
	}
	memcpy(this, &o, sizeof(SAbstractVector));
	if (m_avail <= m_localSpace && m_size > 0) {
		PerformMoveBefore(m_data.local, o.m_data.local, 1);
	}
	memcpy(&o, buffer, sizeof(SAbstractVector));
	if (o.m_avail <= o.m_localSpace && o.m_size > 0) {
		PerformMoveBefore(o.m_data.local, ((SAbstractVector*)buffer)->m_data.local, 1);
	}
}

// amount is how many more elements we need
// factor is the scaling factor
// pos is where in the vector we want the new space to be 0xFFFFFFFF means at the end
// returns a pointer to the beginning of the data
uint8_t* SAbstractVector::grow(size_t amount, size_t factor, size_t pos)
{
	const size_t total_needed = m_size+amount;
	
	uint8_t* d = edit_data();

	if (total_needed > m_avail) {
		// Need to grow the available space...
		if (total_needed > m_localSpace) {
			// Figure out how much space to reserve, and allocate new heap
			// space for it.
			size_t can_use = ((total_needed+1)*factor)/2;
			can_use = (((can_use*m_elementSize)+7)&~7)/m_elementSize;
			// First see if we can just resize this block...
			uint8_t* alloc = m_avail > m_localSpace
							? static_cast<uint8_t*>(inplace_realloc(d, can_use*m_elementSize)) : NULL;
			if (alloc == NULL) {
				// Can't do in-place, so need to allocate a new block and copy
				// over into it.
				alloc = static_cast<uint8_t*>(malloc(can_use*m_elementSize));
				if (alloc == NULL) return NULL;
				if (m_size > 0) {
					// If there are existing elements, move them into the new space.
					if (pos >= m_size) {
						PRINT(("Grow heap: copying %ld entries\n", m_size));
						PerformMoveBefore(alloc, d, m_size);
					} else {
						PRINT(("Grow heap: copying %ld entries (%ld at %ld, %ld from %ld to %ld)\n",
									m_size,
									pos, 0L,
									(m_size-pos), pos, pos+amount));
						if (pos > 0)
							PerformMoveBefore(alloc, d, pos);
						PerformMoveBefore(	alloc+((pos+amount)*m_elementSize),
											d+(pos*m_elementSize),
											(m_size-pos));
					}
				}
				// Free old memory if it is on the heap.
				if (m_avail > m_localSpace) {
					free(m_data.heap);
				} else {
#if SUPPORTS_VECTOR_PROFILING
					AccumVectorStackToHeap();
#endif
				}
				m_data.heap = alloc;
				m_avail = can_use;
			
#if SUPPORTS_VECTOR_PROFILING
				AccumVectorGrow(m_avail,m_elementSize);
				if (g_profileLevel.Get() > 0) {
					if (m_maxSize < (int32_t)m_size) m_maxSize = m_size;
					if (m_maxAvail < (int32_t)m_avail) m_maxAvail = m_avail;
				}
#endif

				return (m_base=alloc);
			}

			// We resized in-place.  Fall through to insert space
			// in the middle of the existing memory, if needed.
			m_avail = can_use;

		} else {
			ErrFatalError("SAbstractVector::grow -- total_needed < m_localSpace, but total_needed > m_avail!");
		}
	}
	
	if (pos < m_size) {
		// If no memory changes have occurred, but we are growing inside
		// the vector, then just move those elements in-place.
		PRINT(("Grow: moving %ld entries (from %ld to %ld)\n",
					m_size-pos, pos, pos+amount));
		PerformMoveAfter(	d+((pos+amount)*m_elementSize),
							d+(pos*m_elementSize),
							(m_size-pos));
	}
	
#if SUPPORTS_VECTOR_PROFILING
	if (g_profileLevel.Get() > 0) {
		if (m_maxSize < (int32_t)m_size) m_maxSize = m_size;
		if (m_maxAvail < (int32_t)m_avail) m_maxAvail = m_avail;
	}
#endif

	return (m_base=d);
}

uint8_t* SAbstractVector::shrink(size_t amount, size_t factor, size_t pos)
{
	if (amount > m_size) amount = m_size;
	const size_t total_needed = m_size - amount;

	uint8_t* d = edit_data();
	
	if (total_needed <= m_localSpace) {
		// Needed size now fits in local data area...
		if (m_avail > m_localSpace) {
			// We are currently using heap storage; copy into local data.
			if (total_needed > 0) {
				if (pos >= total_needed) {
					PRINT(("Shrink to local: copying %ld entries (was %ld)\n",
								total_needed, m_size));
					PerformMoveBefore(m_data.local, d, total_needed);
				} else {
					PRINT(("Shrink to local: copying %ld entries (%ld at %ld, %ld from %ld to %ld)\n",
								total_needed,
								pos, 0L,
								(total_needed-pos), pos+amount, pos));
					if (pos > 0)
						PerformMoveBefore(m_data.local, d, pos);
					PerformMoveBefore(	m_data.local+(pos*m_elementSize),
										d+((pos+amount)*m_elementSize),
										(total_needed-pos));
				}
			}
#if SUPPORTS_VECTOR_PROFILING
			AccumVectorHeapToStack();
			AccumVectorShrink(m_localSpace,m_elementSize);
#endif
			free(d);
			m_avail = m_localSpace;
			return (m_base=m_data.local);
		}

		if (pos < total_needed) {
			// If no memory changes have occurred, but we are shrinking inside
			// the vector, then just move those elements in-place.
			PRINT(("Shrink: moving %ld entries (from %ld to %ld)\n",
						total_needed-pos, pos+amount, pos));
			PerformMoveBefore(	d+(pos*m_elementSize),
								d+((pos+amount)*m_elementSize),
								(total_needed-pos));
		}

	} else {
		if (pos < total_needed) {
			PRINT(("Shrink: moving %ld entries (from %ld to %ld)\n",
						total_needed-pos, pos+amount, pos));
			PerformMoveBefore(	d+(pos*m_elementSize),
								d+((pos+amount)*m_elementSize),
								(total_needed-pos));
		}

		if ((total_needed*factor) < m_avail) {
			// Needed size still must to be on heap, but we can reduce our
			// current heap usage...
			factor /= 2;
			if (factor < 1) factor = 1;
			size_t will_use = total_needed*factor;
			will_use = (((will_use*m_elementSize)+7)&~7)/m_elementSize;

			DbgOnlyFatalErrorIf(will_use <= m_localSpace,
				"SAbstractVector::shrink -- total_needed < m_avail*4, but m_avail < m_localSpace!");

			// First see if we can resize the block in-place.  When
			// shrinking, this will almost always work.
			uint8_t* alloc = static_cast<uint8_t*>(inplace_realloc(d, will_use*m_elementSize));
			if (alloc != NULL) {
				// That was easy!
				m_avail = will_use;
#if SUPPORTS_VECTOR_PROFILING
				AccumVectorShrink(m_avail,m_elementSize);
#endif
			} else {
				// The hard case, have to copy into a new block.
				alloc = static_cast<uint8_t*>(malloc(will_use*m_elementSize));
				// We can fail gracefully if this allocation doesn't succeed
				// by just leaving stuff as-is.
				if (alloc != NULL) {
					PRINT(("Shrink heap: resizing by copying %ld entries\n", total_needed));
					PerformMoveBefore(alloc, d, total_needed);
					free(m_data.heap);
					m_data.heap = alloc;
					m_avail = will_use;
#if SUPPORTS_VECTOR_PROFILING
					AccumVectorShrink(m_avail,m_elementSize);
#endif
					return (m_base=alloc);
				}
			}
		}
	}

	return (m_base=d);
}

// -----------------------------------------------------------------
// AsValue and SetFromValue can call
// Helper functions for simple BArrayAsValue and BArrayConstruct
// (Where we can treat the array as a blob of data, with header.)
// -----------------------------------------------------------------

struct fixed_array_header {
	uint32_t	m_type;			// type code of each array entry
	uint32_t	m_size;			// size of element item
};

SValue SAbstractVector::AsValue() const
{
	return PerformAsValue(this->data(), m_size);
}

status_t SAbstractVector::SetFromValue(const SValue& value)
{
	// Set the vector from an SValue
	// We do some poking around here to determine
	// how many entries are represented in the value.

	size_t count = 0;
	if (value.IsSimple()) {
		// If we have a simple value, then the user treated the vector 
		// elements as a blob of bits, we can calculate
		// the size based on the length and element size.
		// The byteLength was calculated as:
		// byteLength = sizeof(fixed_array_header) + (elementSize*count);
		// So the count is... 
		// (byteLength - sizeof(fixed_array_header))/elementSize = count;
		// (Notice that checking element size for match with header comes
		// in BArrayConstructHelper method.)
		size_t byteLength = value.Length();
		if (byteLength > 0) {
			count = (byteLength - sizeof(fixed_array_header))/m_elementSize;
		}
	}
	else {
		// If we have a mapping, then the user added an item for
		// each element, deduce the count from number of mappings
		count = value.CountItems();
	}
	
	this->MakeEmpty();
	// check if we have a zero length vector
	// if so, MakeEmpty is all we need to do...
	if (count == 0) {
		return B_OK;
	}

	status_t result = B_OK;
	this->SetCapacity(count);
	if (m_avail < count) {
		DbgOnlyFatalError("Unable to grow vector!");
		return B_NO_MEMORY;
	}
	void* data = this->edit_data();
	PerformConstruct(data, count);
	result = PerformSetFromValue(data, value, count);
	m_size = count;

	return result;
}

SSharedBuffer* BArrayAsValueHelper(void*& toPtr, size_t count, size_t elementSize, type_code typeCode)
{
	size_t byteLength = sizeof(fixed_array_header) + (elementSize*count);
	SSharedBuffer* buffer = SSharedBuffer::Alloc(byteLength);
	buffer = buffer->Edit();
	void* data = buffer->Data();
	fixed_array_header* header = static_cast<fixed_array_header*>(data);
	toPtr = static_cast<void*>(header+1);
	header->m_type = typeCode;
	header->m_size = elementSize;
	return buffer;
}

const void* BArrayConstructHelper(const SValue& value, size_t count, size_t elementSize, type_code typeCode)
{
	type_code valueType = value.Type();
	const void* buffer = value.Data();
	size_t valueLength = value.Length();
	size_t byteLength = sizeof(fixed_array_header) + (elementSize*count);
	if (valueType != B_FIXED_ARRAY_TYPE || valueLength != byteLength) {
		return NULL;
	}
	const fixed_array_header* header = static_cast<const fixed_array_header*>(buffer);
	if (header->m_type != typeCode || header->m_size != elementSize) {
		return NULL;
	}
	return static_cast<const void*>(header+1);
};

status_t SAbstractVector::_ReservedUntypedVector1() { return B_UNSUPPORTED; }
status_t SAbstractVector::_ReservedUntypedVector2() { return B_UNSUPPORTED; }
status_t SAbstractVector::_ReservedUntypedVector3() { return B_UNSUPPORTED; }
status_t SAbstractVector::_ReservedUntypedVector4() { return B_UNSUPPORTED; }
status_t SAbstractVector::_ReservedUntypedVector5() { return B_UNSUPPORTED; }
status_t SAbstractVector::_ReservedUntypedVector6() { return B_UNSUPPORTED; }
status_t SAbstractVector::_ReservedUntypedVector7() { return B_UNSUPPORTED; }
status_t SAbstractVector::_ReservedUntypedVector8() { return B_UNSUPPORTED; }
status_t SAbstractVector::_ReservedUntypedVector9() { return B_UNSUPPORTED; }
status_t SAbstractVector::_ReservedUntypedVector10() { return B_UNSUPPORTED; }

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
