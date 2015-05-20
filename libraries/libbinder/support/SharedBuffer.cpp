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

#include <support/SharedBuffer.h>
#include <support/StdIO.h>
#include <support/ITextStream.h>
#include <support_p/SupportMisc.h>

#include <ErrorMgr.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 32
#define MALLOC_ALIGN 8
#define ALIGN_SIZE(x) ((size_t(x)+(MALLOC_ALIGN-1))&(~(MALLOC_ALIGN-1)))

#define USED_SIZE(x) (8+sizeof(SSharedBuffer)+ALIGN_SIZE(x))

#define PRINT_POOL_METRICS 0

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// --------------------------------------------------------------------

static SysCriticalSectionType g_poolLock = sysCriticalSectionInitializer;
static const SSharedBuffer** g_pool = NULL;
static size_t g_poolSize = 0;
static size_t g_poolAvail = 0;
static size_t g_poolMemSize = 0;

static inline void lock_pool() { g_threadDirectFuncs.criticalSectionEnter(&g_poolLock); }
static inline void unlock_pool() { g_threadDirectFuncs.criticalSectionExit(&g_poolLock); }

void __terminate_shared_buffer(void)
{
	if (g_pool) {
		free(g_pool);
		g_pool = NULL;
		g_poolSize = g_poolAvail = 0;
	}
}

bool find_in_pool(const SSharedBuffer* buffer, size_t* pos)
{
	ssize_t mid, low = 0, high = ((ssize_t)g_poolSize)-1;
	while (low <= high) {
		mid = (low + high)/2;
		const int32_t cmp = buffer->Compare(g_pool[mid]);
		if (cmp < 0) {
			high = mid-1;
		} else if (cmp > 0) {
			low = mid+1;
		} else {
			*pos = mid;
			return true;
		}
	}
	
	*pos = low;
	return false;
}

bool add_to_pool(const SSharedBuffer* buffer, size_t pos)
{
	size_t newSize = g_poolSize+1;
	if (newSize > g_poolAvail) {
		size_t newAvail = ((g_poolAvail+2)*3)/2;
		void* newPool = realloc(g_pool, newAvail*sizeof(SSharedBuffer*));
		if (!newPool) return false;
		g_pool = (const SSharedBuffer**)newPool;
		g_poolAvail = newAvail;
	}
	
	if (pos < g_poolSize) {
		memmove(g_pool+pos+1, g_pool+pos, (g_poolSize-pos)*sizeof(SSharedBuffer*));
	}
	g_pool[pos] = buffer;
	g_poolSize = newSize;
#if PRINT_POOL_METRICS
	g_poolMemSize += USED_SIZE(buffer->Length());
	bout << "Buffer: " << SHexDump(buffer->Data(), buffer->Length()) << endl;
	printf("Added to shared buffer pool at %d: count=%d, mem=%d\n", pos, g_poolSize, g_poolMemSize);
#endif
	return true;
}

void remove_from_pool(size_t pos)
{
#if PRINT_POOL_METRICS
	g_poolMemSize -= USED_SIZE(g_pool[pos]->Length());
	bout << "Buffer: " << SHexDump(g_pool[pos]->Data(), g_pool[pos]->Length()) << endl;
	printf("Removed from shared buffer pool at %d: count=%d, mem=%d\n", pos, g_poolSize-1, g_poolMemSize);
#endif
	if (pos < (g_poolSize-1)) memmove(g_pool+pos, g_pool+pos+1, (g_poolSize-pos-1)*sizeof(SSharedBuffer*));
	g_pool[g_poolSize-1] = NULL;
	g_poolSize--;
}

// --------------------------------------------------------------------

struct SSharedBuffer::extended_info
{
			int32_t		pad0;
			uint32_t	bufferSize;

	inline	bool		Buffering() const { return ((bufferSize & 0x80000000) != 0) ? true : false; }
};

inline size_t align_buffer(size_t size)
{
	return ((size+(BUFFER_SIZE-1))&(~(BUFFER_SIZE-1)));
}

struct static_info
{
	SSharedBuffer::inc_ref_func m_incRef;
	SSharedBuffer::dec_ref_func m_decRef;
};

// --------------------------------------------------------------------

inline SSharedBuffer::extended_info* SSharedBuffer::get_extended_info() const
{
	if ((m_length&B_EXTENDED_BUFFER) == 0) return NULL;
	return ((extended_info*)this)-1;
}

inline void SSharedBuffer::SetBufferSize(size_t size)
{
	extended_info* ex = get_extended_info();
	if (ex) ex->bufferSize = size | 0x80000000;
}

inline void SSharedBuffer::SetLength(size_t len)
{
	m_length = (m_length&B_EXTENDED_BUFFER) | (len<<B_BUFFER_LENGTH_SHIFT);
}

bool SSharedBuffer::unpool() const
{
	lock_pool();

	// Okay, anything could have happened before we mucked with the
	// ref count and got the pool locked.  In particular, someone could
	// have come in and gotten a hold on this shared buffer before we
	// locked the pool...  if that happened, we'd better not free it!
	if ((m_users>>B_BUFFER_USERS_SHIFT) != 0) {
		unlock_pool();
		return false;
	}

	// Looks like we should indeed remove this from the pool.
	size_t pos;
	if (find_in_pool(this, &pos)) {
		remove_from_pool(pos);
		unlock_pool();
		return true;
	}

	// Gack, this shouldn't happen!  Oh well, better to leak than
	// to crash.
	DbgOnlyFatalError("SSharedBuffer: a buffer disappeared from the pool!");
	unlock_pool();
	return false;
}

void SSharedBuffer::do_delete() const
{
	extended_info* ex = const_cast<SSharedBuffer*>(this)->get_extended_info();
	if (ex)	free(ex);
	else	free(const_cast<SSharedBuffer*>(this));
}

void SSharedBuffer::IncUsers() const
{
	if ((m_users&B_STATIC_USERS) == 0) {
		SysAtomicAdd32(&m_users, 1<<B_BUFFER_USERS_SHIFT);
		return;
	}

	const static_info* si( ((const static_info*)this) - 1);
	(*si->m_incRef)();
}

void SSharedBuffer::DecUsers() const
{
	const int32_t users = m_users;

	DbgOnlyFatalErrorIf(((users&B_STATIC_USERS) == 0) && (users>>B_BUFFER_USERS_SHIFT)<1, "[SSharedBuffer::DecUsers] DecUsers() called but no users left!");

	// The common case is that that we are the only user and
	// no special modes like "static" or "pooled", so
	// optimize that to avoid a call to atomic_add().
	if (users == (1<<B_BUFFER_USERS_SHIFT)) {
		do_delete();
		return;
	}

	// Decrement reference count (if not a static buffer) and
	// delete if it goes to zero.
	if ((users&B_STATIC_USERS) == 0) {
		int32_t prev = g_threadDirectFuncs.atomicAdd32(&m_users, -(1<<B_BUFFER_USERS_SHIFT))>>B_BUFFER_USERS_SHIFT;
		DbgOnlyFatalErrorIf(prev<1, "[SSharedBuffer::DecUsers] DecUsers() called but no users left!");
		if (prev == 1) {
			if ((users&B_POOLED_USERS) != 0) {
				// This shared buffer is apparently pooled; time to unpool!
				if (!unpool()) {
					// Returns false if someone else got the buffer, so we can't
					// free it.
					return;
				}
			}

			do_delete();
		}
		return;
	}

	const static_info* si( ((const static_info*)this) - 1);
	(*si->m_decRef)();
}

SSharedBuffer* SSharedBuffer::AllocExtended(extended_info* prototype, size_t length)
{
	extended_info* ex =
		reinterpret_cast<extended_info*>(malloc(sizeof(extended_info) + sizeof(SSharedBuffer) + ALIGN_SIZE(length)));
	if (ex) {
		if (prototype) ex->bufferSize = prototype->bufferSize;
		else ex->bufferSize = 0;

		SSharedBuffer* sb = (SSharedBuffer*)(ex+1);
		sb->m_users = 1<<B_BUFFER_USERS_SHIFT;
		sb->m_length = (length<<B_BUFFER_LENGTH_SHIFT) | B_EXTENDED_BUFFER;

		return sb;
	}
	return NULL;
}

SSharedBuffer* SSharedBuffer::Alloc(size_t length)
{
	SSharedBuffer* sb =
		reinterpret_cast<SSharedBuffer*>(malloc(sizeof(SSharedBuffer) + ALIGN_SIZE(length)));
	if (sb) {
		sb->m_users = 1<<B_BUFFER_USERS_SHIFT;
		sb->m_length = length<<B_BUFFER_LENGTH_SHIFT;
	}
	return sb;
}

SSharedBuffer* SSharedBuffer::Edit(size_t newLength) const
{
	extended_info* ex = get_extended_info();

	SSharedBuffer* result;
	
	if (m_users == (1<<B_BUFFER_USERS_SHIFT)) {
		if (ex == NULL || !ex->Buffering()) {
			if (ALIGN_SIZE(Length()) == ALIGN_SIZE(newLength)) {
				const_cast<SSharedBuffer*>(this)->SetLength(newLength);
				return const_cast<SSharedBuffer*>(this);

			} else if (!ex) {
				result = reinterpret_cast<SSharedBuffer*>(
						realloc(const_cast<SSharedBuffer*>(this),
								sizeof(SSharedBuffer) + ALIGN_SIZE(newLength)));
				if (result) {
					result->SetLength(newLength);
					return result;
				}

			} else {
				ex = reinterpret_cast<extended_info*>(
						realloc(ex, sizeof(extended_info) + sizeof(SSharedBuffer) + ALIGN_SIZE(newLength)));
				if (ex) {
					result = (SSharedBuffer*)(ex+1);
					result->SetLength(newLength);
					return result;
				}
			}
			return NULL;
		}

		//printf("*** Edit buffered!\n");
		if (newLength <= BufferSize()) {
			const_cast<SSharedBuffer*>(this)->SetLength(newLength);
			return const_cast<SSharedBuffer*>(this);
		}

		size_t newBufferSize = align_buffer(newLength);

		ex = reinterpret_cast<extended_info*>(
				realloc(ex, sizeof(extended_info) + sizeof(SSharedBuffer) + newBufferSize));
		if (ex) {
			result = (SSharedBuffer*)(ex+1);
			result->SetBufferSize(newBufferSize);
			result->SetLength(newLength);
			return result;
		}
		return NULL;
	}

	// XXX The new buffer does not have extended info!!!!  We need to be better
	// about this when we do more than buffering with it...
	result = Alloc(newLength);
	if (result) {
		const size_t oldLength = Length();
		memcpy(result->Data(), Data(), newLength < oldLength ? newLength : oldLength);
		DecUsers();
		return result;
	}

	return NULL;
}

SSharedBuffer* SSharedBuffer::Edit() const
{
	return Edit(Length());
}

const SSharedBuffer* SSharedBuffer::Pool() const
{
	if (m_users&(B_STATIC_USERS|B_POOLED_USERS)) return this;

	lock_pool();

	const SSharedBuffer* pooled = this;
	if ((pooled->m_users&B_POOLED_USERS) == 0) {

		size_t pos;
		if (find_in_pool(this, &pos)) {
			pooled = g_pool[pos];
#if PRINT_POOL_METRICS
			static size_t g_savedMem = 0;
			g_savedMem += USED_SIZE(pooled->Length());
			printf("Pooling has now saved: %d\n", g_savedMem);
#endif
			pooled->IncUsers();
			DecUsers();
		} else {
			if ((m_length&B_EXTENDED_BUFFER) != 0) {
				SSharedBuffer* copy = SSharedBuffer::Alloc(pooled->Length());
				if (copy == NULL) goto outahere;
				memcpy(copy->Data(), pooled->Data(), pooled->Length());
				pooled->DecUsers();
				pooled = copy;
			}
			if (add_to_pool(pooled, pos)) {
				g_threadDirectFuncs.atomicOr32((uint32_t volatile *)&m_users, B_POOLED_USERS);
			}
		}

	}

outahere:
	unlock_pool();
	return pooled;
}

int32_t SSharedBuffer::Compare(const SSharedBuffer* other) const
{
	const size_t myLen = Length();
	const size_t otherLen = other->Length();
	if (myLen != otherLen) return myLen < otherLen ? -1 : 1;
	return memcmp(Data(), other->Data(), myLen);
}

SSharedBuffer* SSharedBuffer::Extend(size_t newLength) const
{
	extended_info* ex = get_extended_info();
	if (ex) return Edit(newLength);

	SSharedBuffer* sb = AllocExtended(NULL, newLength);
	if (sb) {
		const size_t oldLength = Length();
		memcpy(sb->Data(), Data(), newLength < oldLength ? newLength : oldLength);
		DecUsers();
	}
	return sb;
}

bool SSharedBuffer::Buffering() const
{
	extended_info* info = get_extended_info();
	if (!info) return false;
	return info->Buffering();
}

size_t SSharedBuffer::BufferSize() const
{
	extended_info* info = get_extended_info();
	if (!info) return Length();
	return (info->bufferSize & 0x7fffffff);
}

SSharedBuffer* SSharedBuffer::BeginBuffering()
{
	//printf("*** Begin buffering!\n");

	if (Buffering()) return this;

	SSharedBuffer* result = NULL;
	const size_t len = Length();

	if (len == 0)
	{
		result = Extend(BUFFER_SIZE);
		if (result) {
			result->SetLength(0);
			result->SetBufferSize(ALIGN_SIZE(BUFFER_SIZE));
		}
	}
	else
	{
		// get a copy of the current buffer. we do not want to bufferize everyone.
		const size_t newLen = (len < BUFFER_SIZE) ? BUFFER_SIZE : len;
		result = Extend(newLen);
		if (result) {
			result->SetLength(len);
			result->SetBufferSize(ALIGN_SIZE(newLen));
		}
	}
	
	return result;
}

SSharedBuffer* SSharedBuffer::EndBuffering()
{
	//printf("*** End buffering!\n");

	// trim the buffer down to length.
	extended_info* ex = get_extended_info();
	if (!ex || !(ex->bufferSize&0x80000000)) return this;
	ex->bufferSize = 0;
	return Edit(Length());
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
