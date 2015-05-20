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

#include <support/IBinder.h>
#include <support/Package.h>
#include <support/InstantiateComponent.h>
#include <support/Memory.h>
#include <support/StdIO.h>
#include "Heap.h"

//#include <ResTrackUtil.h>

#if _SUPPORTS_NAMESPACE
using namespace ::palmos::support;
#endif

//////////////////////////////////////////////////////////////////
// BMemoryDealer
// Returns IMemories on demand and keeps the meta-data offline
//////////////////////////////////////////////////////////////////

class BMemoryDealer : public BnMemoryDealer, public SPackageSptr
{
public:

					BMemoryDealer(const SContext& ctxt, const SString& name, size_t size);
	sptr<IMemory>	Allocate(size_t size, uint32_t properties);
private:
	sptr<HeapArea>	m_heap;
};

class BMemory : public BnMemory
{
public:
	BMemory(const sptr<HeapArea>& heap, ssize_t offset, ssize_t size)
		: BnMemory(),
		 m_heap(heap), m_offset(offset), m_size(size)
	{
#if BUILD_TYPE != BUILD_TYPE_RELEASE && !defined(LINUX_DEMO_HACK)
		if( ResTrackIsEnabled() )
		{
			ResTrackAlloc("MemoryDealer", (void*)m_offset, m_size);
		}
#endif //#if BUILD_TYPE != BUILD_TYPE_RELEASE
	}

	virtual ~BMemory()
	{
#if BUILD_TYPE != BUILD_TYPE_RELEASE && !defined(LINUX_DEMO_HACK)
		if( ResTrackIsEnabled() )
		{
			ResTrackFree("MemoryDealer", (void*)m_offset);
		}
#endif //#if BUILD_TYPE != BUILD_TYPE_RELEASE
		m_heap->Free(m_offset);
	}

	virtual sptr<IMemoryHeap> GetMemory(ssize_t *offset, ssize_t *size) const {
		if (offset) *offset = m_offset;
		if (size)	*size = m_size;
		return sptr<IMemoryHeap>(m_heap->MemoryHeap().ptr());
	}

private:
	sptr<HeapArea>		m_heap;
	ssize_t				m_offset;
	ssize_t				m_size;
};


///////////////////////////////////////////////////////////////////////////////
// #pragma mark -
// #pragma mark A BMemoryDealer implementation

BMemoryDealer::BMemoryDealer(const SContext& ctxt, const SString& name, size_t size)
	: BnMemoryDealer(ctxt)
{
	m_heap = new HeapArea(name.String(), size, 32 /* the default size */, false /* not fully mapped */);
}

sptr<IMemory> BMemoryDealer::Allocate(size_t size, uint32_t properties)
{
	(void)properties;
	ssize_t offset = m_heap->Alloc(size);
	if (offset < 0) {
#if BUILD_TYPE == BUILD_TYPE_DEBUG
		DbgOnlyFatalError("[MemoryDealer] Insufficient backing RAM or address space: the system either needs more RAM, more address space for the MemoryDealer, or more likely an application has a memory leak.");
		printf("MEMORY DEALER: Unable to allocate %ld bytes!\n", size);
#endif
		return NULL;
	}
	sptr<IMemory> memory = new BMemory(m_heap, offset, size);
	if (memory == NULL) {
#if BUILD_TYPE == BUILD_TYPE_DEBUG
		DbgOnlyFatalError("CAN'T CREATE IMemory OBJECT!");
		printf("MEMORY DEALER: Unable to create IMemory object for %ld bytes!\n", size);
#endif
		m_heap->Free(offset);
	}
	return memory;
}

//////////////////////////////////////////////////////////////////


// Given a component name
// and the context it is being instantiated in, return the
// IBinder for a new instance of the component.  The "context"
// is the IContext in which this new component is to be running
// in.
sptr<IBinder> InstantiateComponent(	const SString& component,
									const SContext& context,
									const SValue& value)
{
	if (component == "") {
		SString segmentName = value["name"].AsString();
		ssize_t segmentSize = value["size"].AsInteger();
		sptr<BMemoryDealer> dealer = new BMemoryDealer(context, segmentName, segmentSize*1024);
		return dealer->AsBinder();
	}
	return NULL;
}
