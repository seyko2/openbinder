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

#include <support/Memory.h>

#include <support_p/BinderKeys.h>
#include <support_p/SupportMisc.h>
#include <support/Binder.h>
#include <support/IMemory.h>
#include <support/KeyedVector.h>
#include <support/Parcel.h>
#include <support/Autolock.h>
#include <support/Locker.h>
#include <support/StdIO.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
              

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
using namespace palmos::osp;
#endif

B_STATIC_STRING_VALUE_8	(key_HeapID,	"HeapID",);
B_STATIC_STRING_VALUE_12(key_GetMemory,	"GetMemory",);
B_STATIC_STRING_VALUE_12(key_Allocate,	"Allocate",);

enum {
	kHeapID		= 'memp', // changed from 'memh' in Dialtone
	kGetMemory	= 'memg',
	kAllocate	= 'mema'
};

#define IS_VALID_HEAP_ID(_x)	((_x) != -1)
#define A_BAD_HEAP_ID			(-1)

typedef int32_t heap_id_t;

/******************************************************************************/
// The global array of IMemoryHeap

static sptr<IMemoryHeap> find_memory_heap(const sptr<IBinder>& bheap)
{
	// Look to see if we have this binder in the cache
	SAutolock locker(gAreaTranslationLock.Lock());
	bool found;
	area_translation_info& info = gAreaTranslationCache.EditValueFor(bheap, &found);
	if (found) {
		atomic_add(&info.count, 1);
		return info.heap;
	} else {
		// It doesn't exist, add the IMemoryHeap in the cache
		area_translation_info i;
		i.heap = IMemoryHeap::AsInterfaceNoInspect(bheap);
		i.count = 1;
		gAreaTranslationCache.AddItem(bheap, i);
		return i.heap;
	}
}

static void *convert_to_pointer(const sptr<IMemoryHeap>& real_heap, ssize_t offset)
{
	if (real_heap == NULL) return NULL;
	const heap_id_t id = real_heap->HeapID();
	if (!IS_VALID_HEAP_ID(id)) return NULL;
	
	void * base = real_heap->HeapBase();
	
	return ((char*)base) + offset;
}

/******************************************************************************/
// #pragma mark IMemory conveniences

// Retrieve memory address when you know the heap it exists in and offset there-in
void * IMemory::FastPointer(const sptr<IBinder>& heap, ssize_t offset) const
{
	sptr<IMemoryHeap> real_heap;

	// Look to see if we have this binder in the cache
	SAutolock locker(gAreaTranslationLock.Lock());
	bool found;
	const area_translation_info& info = gAreaTranslationCache.ValueFor(heap, &found);
	if (found)		real_heap = info.heap;
	else			real_heap = IMemoryHeap::AsInterfaceNoInspect(heap);
	return convert_to_pointer(real_heap, offset);
}

// Compute a pointer from an area_id and an offset
void *IMemory::Pointer() const
{
	ssize_t offset;
	sptr<IMemoryHeap> heap( GetMemory(&offset) );
	return convert_to_pointer(heap, offset);
}


ssize_t IMemory::Size() const
{
	ssize_t size;
	GetMemory(0, &size);
	return size;
}

/******************************************************************************/
// #pragma mark -
// #pragma mark BpMemory

class BpMemory : public BpInterface<IMemory>
{
public:
										BpMemory(const sptr<IBinder>& remote);
			virtual					~BpMemory();
			virtual sptr<IMemoryHeap>	GetMemory(ssize_t *offset = NULL, ssize_t *size = NULL) const;
private:
	mutable sptr<IMemoryHeap>	m_heap;		// We need to hold on this IMemoryHeap
	mutable ssize_t 			m_offset;
	mutable ssize_t 			m_size;
};

B_IMPLEMENT_META_INTERFACE(Memory, "org.openbinder.support.IMemory", IMemory)
; // For Eddie

BpMemory::BpMemory(const sptr<IBinder>& remote)
	: 	BpInterface<IMemory>(remote)
{
}

BpMemory::~BpMemory()
{
	if (m_heap != NULL) {
		SAutolock locker(gAreaTranslationLock.Lock());
		bool found;
		area_translation_info info = gAreaTranslationCache.EditValueFor(m_heap->AsBinder(), &found);
		if (!found) {
			// This should never happen!
			ErrFatalError("~BpMemory IMemoryHeap not cached!");
		} else {
			if (atomic_add(&info.count, -1) == 0) {
				// This IMemoryHeap is not used by anyone anymore
				info.heap = NULL; // Release the IMemoryHeap
				gAreaTranslationCache.RemoveItemFor(m_heap->AsBinder());
			}
		}
	}
}

sptr<IMemoryHeap> BpMemory::GetMemory(ssize_t *offset, ssize_t *size) const
{
	if (m_heap == NULL) {
		SParcel send;
		SParcel* reply = SParcel::GetParcel();
		if (reply != NULL) {
			status_t err = Remote()->Transact(kGetMemory, send, reply);
			if (err == B_OK) {
				// Read the return values -- IMemoryHeap's Binder, and offset.
				sptr<IBinder> bheap(reply->ReadBinder());
				m_offset = reply->ReadInt32();	// XXX should check for error.
				m_size = reply->ReadInt32();	// XXX should check for error.
				// Look to see if we have the IMemoryHeap in the cache
				m_heap = find_memory_heap(bheap);
			}
			SParcel::PutParcel(reply);
		}
	}

	if (offset)	*offset = m_offset;
	if (size)	*size = m_size;
	return m_heap;
}

static SValue memory_hook_GetMemory(const sptr<IInterface>& This)
{
	ssize_t offset, size;
	sptr<IMemoryHeap> heap = static_cast<IMemory*>(This.ptr())->GetMemory(&offset, &size);
	SValue v;
	v.JoinItem(SValue::Int32(0), SValue(heap->AsBinder()));
	v.JoinItem(SValue::Int32(1), SValue::Int32(offset));
	v.JoinItem(SValue::Int32(2), SValue::Int32(size));
	return v;
}

static const struct effect_action_def memory_actions[] = {
	{	sizeof(effect_action_def), &key_GetMemory, NULL, NULL, memory_hook_GetMemory, NULL }
};


status_t BnMemory::HandleEffect(	const SValue &in,
									const SValue &inBindings,
									const SValue &outBindings,
									SValue *out)
{
	return execute_effect(	sptr<IInterface>(this),
							in, inBindings, outBindings, out,
							memory_actions, sizeof(memory_actions)/sizeof(memory_actions[0]));
}

status_t BnMemory::Transact(	uint32_t code,
								SParcel& data,
								SParcel* reply,
								uint32_t flags)
{
	if (code == kGetMemory) {
		ssize_t offset, size;
		sptr<IMemoryHeap> heap = GetMemory(&offset, &size);
		reply->WriteBinder(heap->AsBinder());
		reply->WriteInt32(offset);
		reply->WriteInt32(size);
		return B_OK;
	}
	return BBinder::Transact(code, data, reply, flags);
}

///////////////////////////////////////////////////////////////////////////////
// #pragma mark -
// #pragma mark BpMemoryHeap

class BpMemoryHeap : public BpInterface<IMemoryHeap>
{
public:
								BpMemoryHeap(const sptr<IBinder>& remote);
		virtual				~BpMemoryHeap();
		virtual	heap_id_t	HeapID() const;
		virtual void *		HeapBase() const;
private:
		mutable heap_id_t	m_heap_id;
		mutable void *		m_heap_base;
		mutable uint32_t 	m_heap_size;
};

B_IMPLEMENT_META_INTERFACE(MemoryHeap, "org.openbinder.support.IMemoryHeap", IMemoryHeap)
; // For Eddie

BpMemoryHeap::BpMemoryHeap(const sptr<IBinder>& remote)
	: 	BpInterface<IMemoryHeap>(remote),
		m_heap_id(A_BAD_HEAP_ID)
{
}

BpMemoryHeap::~BpMemoryHeap()
{
	// unmap the segment?
	if (IS_VALID_HEAP_ID(m_heap_id) && m_heap_base) {
#if 1
		/* shm */
		if (shmdt(m_heap_base) == -1) {
			fprintf(stderr, "[MemoryDealer] Error releasing proxy memory heap in process %d with shm id %d: %s\n", getpid(), m_heap_id, strerror(errno));
		}
#else
		if (munmap(m_heap_base, m_heap_size) == -1) {
			fprintf(stderr, "[MemoryDealer] Error releasing proxy memory heap in process %d with id %d: %s\n", getpid(), m_heap_id, strerror(errno));
		}
		m_heap_base = NULL;
		m_heap_size = 0;
#endif

	}
}

heap_id_t BpMemoryHeap::HeapID() const
{
	if (!IS_VALID_HEAP_ID(m_heap_id)) {
		// Here we need to clone the memory, because a remote process request it
		SParcel send;
		SParcel* reply = SParcel::GetParcel();
		if (reply == NULL) return B_NO_MEMORY;
		status_t err = Remote()->Transact(kHeapID, send, reply);
		if (err != B_OK) {
			SParcel::PutParcel(reply);
			return err;
		}
		
		m_heap_id = reply->ReadInt32();


#if 1
		/* shm */
		
		shmid_ds buf;
		
		m_heap_base = shmat(m_heap_id, NULL, 0);
		if (!m_heap_base) {
			fprintf(stderr, "[MemoryDealer] Proxy had failure attaching shm id %d\n", m_heap_id);
			abort();
		}
		if (shmctl(m_heap_id, IPC_STAT, &buf) == -1) {
			fprintf(stderr, "[MemoryDealer] Proxy had failure retrieving shm id %d info\n", m_heap_id);
			abort();
		}
		m_heap_size = buf.shm_segsz;
		
		if (!m_heap_base) {
			m_heap_size = 0;
		}

#else
		/* mmap */
		char name[64];
		sprintf(name, MEMORYDEALER_NAME_TEMPLATE, getenv("PALMOS_KALNAME") ?: "", heapID);

		int fd = open(name, O_RDWR);
		if (fd != -1) {
		  
			int filesize = lseek(fd, 0, SEEK_END);
			lseek(fd, 0, SEEK_SET);

			m_heap_base = mmap(0, filesize, PROT_READ|PROT_WRITE, MAP_SHARED, 0, fd);
			m_heap_size = filesize;

			printf("[MemoryDealer] Proxy memory heap in process %d mapped area %d of size (%d) at address %x\n", getpid(), heapID, m_heap_size, m_heap_base);
		
			if (!m_heap_base) {
				m_heap_size = 0;
			}
		} else {
			printf("[MemoryDealer] Proxy memory heap in process %d had failure retrieving mapped area %d of size (%d) at address %x\n", getpid(), heapID, m_heap_size, m_heap_base);
			abort();
		}
#endif

		SParcel::PutParcel(reply);
	}
	
	return m_heap_id;
}

void * BpMemoryHeap::HeapBase() const
{
	HeapID();
	return m_heap_base;
}

static SValue memoryheap_hook_HeapID(const sptr<IInterface>& This)
{
	const int32_t id = (int32_t)(static_cast<IMemoryHeap*>(This.ptr())->HeapID());
	SValue v(SValue::Int32(0), SValue::Int32(id));
	return v;
}

static const struct effect_action_def memoryheap_actions[] = {
	{	sizeof(effect_action_def), &key_HeapID, NULL, NULL, memoryheap_hook_HeapID, NULL }
};


status_t BnMemoryHeap::HandleEffect(	const SValue &in,
										const SValue &inBindings,
										const SValue &outBindings,
										SValue *out)
{
	return execute_effect(	sptr<IInterface>(this),
							in, inBindings, outBindings, out,
							memoryheap_actions, sizeof(memoryheap_actions)/sizeof(memoryheap_actions[0]));
}

status_t BnMemoryHeap::Transact(	uint32_t code,
									SParcel& data,
									SParcel* reply,
									uint32_t flags)
{
	if (code == kHeapID) {
		const int32_t id = (int32_t)HeapID();
		reply->WriteInt32(id);
		return B_OK;
	}
	return BBinder::Transact(code, data, reply, flags);
}


///////////////////////////////////////////////////////////////////////////////
// #pragma mark -
// #pragma mark BpMemoryDealer

class BpMemoryDealer : public BpInterface<IMemoryDealer>
{
public:
				BpMemoryDealer(const sptr<IBinder>& remote);
	virtual	sptr<IMemory> Allocate(size_t size, uint32_t properties);
};

/******************************************************************************/

BpMemoryDealer::BpMemoryDealer(const sptr<IBinder>& remote)
	: BpInterface<IMemoryDealer>(remote)
{
}

sptr<IMemory> BpMemoryDealer::Allocate(size_t size, uint32_t properties)
{
	sptr<IMemory> result;

	SParcel* send = SParcel::GetParcel();
	SParcel* reply = SParcel::GetParcel();
	if (send != NULL && reply != NULL) {
		send->WriteInt32(size);
		send->WriteInt32(properties);
		status_t err = Remote()->Transact(kAllocate, *send, reply);
		if (err == B_OK) {
			result = IMemory::AsInterfaceNoInspect(reply->ReadBinder());
		}
	}
	SParcel::PutParcel(send);
	SParcel::PutParcel(reply);

	return result;
}

B_IMPLEMENT_META_INTERFACE(MemoryDealer, "org.openbinder.support.IMemoryDealer", IMemoryDealer)
; // For Eddie

static SValue memorydealer_hook_Allocate(const sptr<IInterface>& This, const SValue& args)
{
	const SValue size(args[SValue::Int32(0)]);
	const SValue properties(args[SValue::Int32(1)]);
	if (!size.IsDefined() || !properties.IsDefined()) return SValue::Int32(B_BINDER_MISSING_ARG);	
	return SValue((static_cast<IMemoryDealer*>(This.ptr()))->Allocate(size.AsInt32(), properties.AsInt32())->AsBinder());
}

static const struct effect_action_def memorydealer_actions[] = {
	{	sizeof(effect_action_def), &key_Allocate, NULL, NULL, NULL, memorydealer_hook_Allocate }
};

status_t BnMemoryDealer::HandleEffect(	const SValue &in,
										const SValue &inBindings,
										const SValue &outBindings,
										SValue *out)
{
	return execute_effect(	sptr<IInterface>(this),
							in, inBindings, outBindings, out,
							memorydealer_actions, sizeof(memorydealer_actions)/sizeof(memorydealer_actions[0]));
}

status_t BnMemoryDealer::Transact(	uint32_t code,
									SParcel& data,
									SParcel* reply,
									uint32_t flags)
{
	if (code == kAllocate) {
		size_t size = data.ReadInt32();
		uint32_t properties = data.ReadInt32();
		sptr<IMemory> mem = Allocate(size, properties);
		reply->WriteBinder(mem->AsBinder());
		return B_OK;
	}
	return BBinder::Transact(code, data, reply, flags);
}


///////////////////////////////////////////////////////////////////////////////
// #pragma mark -
// #pragma mark A BMemoryHeap implementation

BMemoryHeap::BMemoryHeap(int32_t area, void * basePtr)
	:	m_area(area), m_basePtr(basePtr)
{
}

BMemoryHeap::~BMemoryHeap()
{
}

int32_t BMemoryHeap::HeapID() const
{
	return m_area;
}

void * BMemoryHeap::HeapBase() const
{
	return m_basePtr;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
