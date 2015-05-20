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

#include <support/Locker.h>

#include <support/atomic.h>
#include <support/StdIO.h>
#include <support/TLS.h>

#include <support_p/DebugLock.h>
#include <support_p/SupportMisc.h>

#include <stdio.h>
#include <stdlib.h>

#if TARGET_HOST == TARGET_HOST_WIN32
#include <support_p/WindowsCompatibility.h>
#include <stdio.h>
#endif

#if 0 && TARGET_HOST != TARGET_HOST_PALMOS // FIXME: Shutting off for Linux, 'cause KALTSD ain't it

// This code was stolen from KALTSD.c in the kernel client code.
// It is written to match that code as closely as possible, NOT
// for the absolute best performance.

#include <Kernel.h>
#include <KeyIDs.h>

static volatile int32_t gLockBaseIndex = -1;
static volatile int32_t gLockBaseAllocated = 0;

#define KALThreadWakeup SysSemaphoreSignal

static void delete_lock_state(void *state)
{
	uint32_t *tsdBase = (uint32_t*)state;
	if (state) {
		SysSemaphoreDestroy(tsdBase[kKALTSDSlotIDCurrentThreadKeyID]);
		
		free(tsdBase);
	}
}

static uint32_t* PrvGetSlotAddress(int32_t)
{
	// Retrieve critical section node for this thread.
	int32_t baseIndex = gLockBaseIndex;
	if (baseIndex == -1) {
		if (!atomic_or(&gLockBaseAllocated, 1)) {
			baseIndex = tls_allocate();
			tls_set(baseIndex, NULL);
			gLockBaseIndex = baseIndex;
		} else {
			while ((baseIndex=gLockBaseIndex) == -1) 
				KALCurrentThreadDelay(B_MILLISECONDS(2), B_RELATIVE_TIMEOUT);
		}
	}

	uint32_t* slot = (uint32_t*)tls_get(baseIndex);
	if (slot == NULL) {
		slot = (uint32_t*)malloc(sizeof(uint32_t)*(kKALTSDSlotIDCriticalSection+1));
		SysSemaphoreCreateEZ(0, &slot[kKALTSDSlotIDCurrentThreadKeyID]);
		slot[kKALTSDSlotIDCriticalSection] = 0;
		tls_set(baseIndex, slot);
		SysThreadExitCallbackID idontcare;
		SysThreadInstallExitCallback(delete_lock_state, slot, &idontcare);
	}

	return slot;
}

void
SysCriticalSectionEnter(SysCriticalSectionType *iCS)
{
	uint32_t *tsdBase;
	uint32_t old;
	uint32_t retry;

	tsdBase= (uint32_t*)PrvGetSlotAddress(0);

	do {
		old= *((uint32_t*)iCS);
		tsdBase[kKALTSDSlotIDCriticalSection]= old;

		retry= SysAtomicCompareAndSwap32(
						(volatile uint32_t *)iCS,
						(uint32_t)old,
						(uint32_t)tsdBase
					);
	} while(retry);

	if (old) {
		SysSemaphoreWait(tsdBase[kKALTSDSlotIDCurrentThreadKeyID], B_WAIT_FOREVER, B_INFINITE_TIMEOUT);
		
	}
}

void
SysCriticalSectionExit(SysCriticalSectionType *iCS)
{
	uint32_t *tsdBase;
	uint32_t *curr;
	uint32_t **prev;
	uint32_t retry;

	tsdBase= (uint32_t*)PrvGetSlotAddress(0);

	do {
		prev= (uint32_t **)(iCS);
		curr= *prev;

		while(curr!= tsdBase) {
			prev= (uint32_t**)(curr+kKALTSDSlotIDCriticalSection);
			curr= *prev;
		}

		if(prev!= (uint32_t**)iCS) {
			/*
			 * easy part, we are not the head of the queue
			 * so we can freely fix the queue without
			 * concurrency issues.
			 */
			uint32_t *otherTSD;

			otherTSD= ((uint32_t*)prev)-kKALTSDSlotIDCriticalSection;

			*prev= 0;
			KALThreadWakeup(otherTSD[kKALTSDSlotIDCurrentThreadKeyID]);
			retry= 0;
		} else {
			/*
			 * this is the tricky part, we are the only thread
			 * in the list so prev is aliased to iCS... we need
			 * to do an atomic operation since some may be trying
			 * to enter the critical section.
			 *
			 * Note that this retry business is not particulary
			 * expensive, since the inner loop took only 1
			 * iteration in the first pass of the outer one, and
			 * the second pass will involve no atomic operations
			 * at all.
			 *
			 * Note also that if the CompareAndSwap succeeds
			 * there is no thread to wake up.
			 */
			retry= SysAtomicCompareAndSwap32(
							(volatile uint32_t *)iCS,
							(uint32_t)tsdBase,
							0
						);
		}
	} while(retry);
}

#define sysConditionVariableOpened 1

void
SysConditionVariableWait(SysConditionVariableType *iCV, SysCriticalSectionType *iOptionalCS)
{
	uint32_t *tsdBase;
	uint32_t old;
	uint32_t retry;

	tsdBase= (uint32_t*)PrvGetSlotAddress(0);

	// If the condition is not opened, push this thread
	// on to the list of those waiting.
	do {
		old= *((uint32_t volatile *)iCV);
		if (old == sysConditionVariableOpened) break;

		tsdBase[kKALTSDSlotIDCriticalSection]= old;

		retry= SysAtomicCompareAndSwap32(
						(volatile uint32_t *)iCV,
						(uint32_t)old,
						(uint32_t)tsdBase
					);
	} while(retry);

	// Now wait for the condition to be opened if it
	// previously wasn't.
	if (old != sysConditionVariableOpened) {
		if (iOptionalCS) SysCriticalSectionExit(iOptionalCS);
		SysSemaphoreWait(tsdBase[kKALTSDSlotIDCurrentThreadKeyID], B_WAIT_FOREVER, B_INFINITE_TIMEOUT);
		if (iOptionalCS) SysCriticalSectionEnter(iOptionalCS);
	}
}

static void
wake_up_condition(uint32_t *curr)
{
	uint32_t *next;

	// Iterate through the list, clearing it and waking
	// up its threads.
	while (curr != sysConditionVariableInitializer) {
		next = (uint32_t*)(curr[kKALTSDSlotIDCriticalSection]);
		curr[kKALTSDSlotIDCriticalSection] = sysConditionVariableInitializer;
		KALThreadWakeup(curr[kKALTSDSlotIDCurrentThreadKeyID]);
		curr = next;
	}
}

void
SysConditionVariableOpen(ConditionVariableType *iCV)
{
	uint32_t *curr;
	uint32_t retry;

	// Atomically open the condition and retrieve the list of
	// waiting threads.
	do {
		curr= *(uint32_t * volatile *)(iCV);
		if (curr == (uint32_t *)sysConditionVariableOpened) {
			curr = (uint32_t *)sysConditionVariableInitializer;
			break;
		}

		retry= SysAtomicCompareAndSwap32(
						(volatile uint32_t *)iCV,
						(uint32_t)curr,
						(uint32_t)sysConditionVariableOpened
					);
	} while(retry);

	wake_up_condition(curr);
}

void
SysConditionVariableClose(ConditionVariableType *iCV)
{
	uint32_t curr;
	uint32_t retry;

	// Atomically transition the condition variable
	// from opened to closed.  If it is not currently
	// opened, leave it as-is.
	do {
		curr= *(volatile uint32_t *)iCV;
		if (curr != sysConditionVariableOpened) {
			break;
		}

		retry= SysAtomicCompareAndSwap32(
						(volatile uint32_t *)iCV,
						curr,
						(uint32_t)sysConditionVariableInitializer
					);
	} while(retry);
}

void
SysConditionVariableBroadcast(ConditionVariableType *iCV)
{
	uint32_t *curr;
	uint32_t retry;

	// Atomically retrieve and clear the list of
	// waiting threads.
	do {
		curr= *(uint32_t * volatile *)(iCV);

		retry= SysAtomicCompareAndSwap32(
						(volatile uint32_t *)iCV,
						(uint32_t)curr,
						(uint32_t)kKALConditionVariableInitializer
					);
	} while(retry);

	if (curr != (uint32_t *)sysConditionVariableOpened) {
		wake_up_condition(curr);
	}
}

#undef KALThreadWakeup

#endif

// -----------------------------------------------------------------
// -----------------------------------------------------------------
// -----------------------------------------------------------------

#if !SUPPORTS_LOCK_DEBUG

//#pragma mark -

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

static inline void init_gehnaphore(volatile int32_t* value)
{
	*value = (int32_t)sysCriticalSectionInitializer;
}

static inline void init_gehnaphore(volatile int32_t* value, const char*)
{
	*value = (int32_t)sysCriticalSectionInitializer;
}

static inline void fini_gehnaphore(volatile int32_t* value)
{
	(void)value;
}

static inline void lock_gehnaphore(volatile int32_t* value)
{
	g_threadDirectFuncs.criticalSectionEnter((SysCriticalSectionType*)value);
}

static inline void unlock_gehnaphore(volatile int32_t* value)
{
	g_threadDirectFuncs.criticalSectionExit((SysCriticalSectionType*)value);
}

inline void restore_ownership_gehnaphore(volatile int32_t* /*value*/)
{
}

inline volatile int32_t* remove_ownership_gehnaphore(volatile int32_t* value)
{
	return value;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

// -----------------------------------------------------------------

#else

#include <stdlib.h>
#include <new>

//#pragma mark -

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

inline void init_gehnaphore(volatile int32_t* value, const char* name = "gehnaphore")
{
	if (!LockDebugLevel()) *value = (int32_t)sysCriticalSectionInitializer;
	else *value = reinterpret_cast<int32_t>(new(B_SNS(std::) nothrow) DebugLock("SLocker", (const void*)value, name));
}

inline void fini_gehnaphore(volatile int32_t* value)
{
	if (!LockDebugLevel()) ;
	else if (*value) reinterpret_cast<DebugLock*>(*value)->Delete();
}

inline void lock_gehnaphore(volatile int32_t* value)
{
	if (!LockDebugLevel()) SysCriticalSectionEnter((SysCriticalSectionType*)value);
	else if (*value) reinterpret_cast<DebugLock*>(*value)->Lock();
}

inline void unlock_gehnaphore(volatile int32_t* value)
{
	if (!LockDebugLevel()) SysCriticalSectionExit((SysCriticalSectionType*)value);
	else if (*value) reinterpret_cast<DebugLock*>(*value)->Unlock();
}

inline void restore_ownership_gehnaphore(volatile int32_t* value)
{
	if (LockDebugLevel() && *value) reinterpret_cast<DebugLock*>(*value)->RestoreOwnership();
}

inline volatile int32_t* remove_ownership_gehnaphore(volatile int32_t* value)
{
	if (!LockDebugLevel()) return value;
	else if (*value) return reinterpret_cast<DebugLock*>(*value)->RemoveOwnership();
	return NULL;
}

// We export these for lock debugging, so it can use
// gehnaphores as well.
extern void dbg_init_gehnaphore(volatile int32_t* value);
extern void dbg_lock_gehnaphore(volatile int32_t* value);
extern void dbg_unlock_gehnaphore(volatile int32_t* value);

void dbg_init_gehnaphore(volatile int32_t* value)
{
	*value = (int32_t)sysCriticalSectionInitializer;
}

void dbg_lock_gehnaphore(volatile int32_t* value)
{
	SysCriticalSectionEnter((SysCriticalSectionType*)value);
}

void dbg_unlock_gehnaphore(volatile int32_t* value)
{
	SysCriticalSectionExit((SysCriticalSectionType*)value);
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif

// -----------------------------------------------------------------
// ---------------------------- SLocker ----------------------------
// -----------------------------------------------------------------

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

//#pragma mark -

SLocker::SLocker()
{
	init_gehnaphore(&m_lockValue);
}

SLocker::SLocker(const char* name)
{
	init_gehnaphore(&m_lockValue, name);
}

SLocker::~SLocker()
{
	fini_gehnaphore(&m_lockValue);
}

lock_status_t SLocker::Lock()
{
	lock_gehnaphore(&m_lockValue);
	return lock_status_t((void (*)(void*))unlock_gehnaphore, (void*)&m_lockValue);
}

void SLocker::LockQuick()
{
	lock_gehnaphore(&m_lockValue);
}

bool SLocker::IsLocked() const
{
#if !SUPPORTS_LOCK_DEBUG
	return (m_lockValue != (int32_t)sysCriticalSectionInitializer);
#else
	return reinterpret_cast<DebugLock*>(m_lockValue)->IsLocked();
#endif
}

void SLocker::Yield()
{
	if (IsLocked()) {
		Unlock();
		Lock();
	}
}

void SLocker::Unlock()
{
	unlock_gehnaphore(&m_lockValue);
}

void SLocker::RestoreOwnership()
{
	restore_ownership_gehnaphore(&m_lockValue);
}

volatile int32_t* SLocker::RemoveOwnership()
{
	return remove_ownership_gehnaphore(&m_lockValue);
}

// -----------------------------------------------------------------
// ------------------------- SNestedLocker -------------------------
// -----------------------------------------------------------------

//#pragma mark -

SNestedLocker::SNestedLocker()
{
	init_gehnaphore(&m_lockValue);
	m_owner = B_NO_INIT;
	m_ownerCount = 0;
}

SNestedLocker::SNestedLocker(const char* name)
{
	init_gehnaphore(&m_lockValue, name);
	m_owner = B_NO_INIT;
	m_ownerCount = 0;
}

SNestedLocker::~SNestedLocker()
{
	fini_gehnaphore(&m_lockValue);
}

lock_status_t SNestedLocker::Lock()
{
	SysHandle me = SysCurrentThread();
	if (me == m_owner) {
		m_ownerCount++;
	} else {
		lock_gehnaphore(&m_lockValue);
		m_ownerCount = 1;
		m_owner = me;
	}
	return lock_status_t((void (*)(void*))_UnlockFunc, this);
}

void SNestedLocker::LockQuick()
{
	SysHandle me = SysCurrentThread();
	if (me == m_owner) {
		m_ownerCount++;
	} else {
		lock_gehnaphore(&m_lockValue);
		m_ownerCount = 1;
		m_owner = me;
	}
}

void SNestedLocker::Yield()
{
	if (m_lockValue != 0 && m_owner == SysCurrentThread()) {
		const int32_t count = m_ownerCount;
		m_ownerCount = 1;
		Unlock();
		Lock();
		m_ownerCount = count;
	}
}

void SNestedLocker::_UnlockFunc(SNestedLocker* l)
{
	if (--l->m_ownerCount) return;
	l->m_owner = B_NO_INIT;
	unlock_gehnaphore(&l->m_lockValue);
}

void SNestedLocker::Unlock()
{
	_UnlockFunc(this);
}

int32_t SNestedLocker::NestingLevel() const
{
	if (m_owner == SysCurrentThread()) {
		return m_ownerCount;
	}
	return 0;
}

#if TARGET_HOST != TARGET_HOST_WIN32

// -----------------------------------------------------------------
// ----------------------- SReadWriteLocker ------------------------
// -----------------------------------------------------------------

//#pragma mark -

SReadWriteLocker::SReadWriteLocker()
{
	pthread_rwlock_init(&m_lock, NULL);
}

SReadWriteLocker::~SReadWriteLocker()
{
	pthread_rwlock_destroy(&m_lock);
}

lock_status_t SReadWriteLocker::ReadLock()
{
	pthread_rwlock_rdlock(&m_lock);
	return lock_status_t((void (*)(void*))pthread_rwlock_unlock, (void*)&m_lock);
}

void SReadWriteLocker::ReadLockQuick()
{
	pthread_rwlock_rdlock(&m_lock);
}

void SReadWriteLocker::ReadUnlock()
{
	pthread_rwlock_unlock(&m_lock);
}

lock_status_t SReadWriteLocker::WriteLock()
{
	pthread_rwlock_wrlock(&m_lock);
	return lock_status_t((void (*)(void*))pthread_rwlock_unlock, (void*)&m_lock);
}

void SReadWriteLocker::WriteLockQuick()
{
	pthread_rwlock_wrlock(&m_lock);
}

void SReadWriteLocker::WriteUnlock()
{
	pthread_rwlock_unlock(&m_lock);
}
#else

void readUnlocker(void* data)
{
	SysSemaphoreSignal((SysHandle) data);
}

void writeUnlocker(void* data)
{
	SysSemaphoreSignalCount((SysHandle) data, 0xffffffff);
}



SReadWriteLocker::SReadWriteLocker()
{
	if (SysSemaphoreCreate(0x0000ffff, 0x0000ffff, 0, &m_sem) != B_OK) {
		DbgOnlyFatalError("Couldn't create semaphore");
	}
}

SReadWriteLocker::~SReadWriteLocker()
{
	SysSemaphoreDestroy(m_sem);
}

lock_status_t SReadWriteLocker::ReadLock()
{
	SysSemaphoreWait(m_sem, B_WAIT_FOREVER, 0);
	return lock_status_t((void (*)(void*))readUnlocker, (void*)m_sem);
}

void SReadWriteLocker::ReadUnlock()
{
	SysSemaphoreSignal(m_sem);
}

lock_status_t SReadWriteLocker::WriteLock()
{
	SysSemaphoreWaitCount(m_sem, B_WAIT_FOREVER, 0, 0xffffffff);
	return lock_status_t((void (*)(void*))writeUnlocker, (void*)m_sem);
}

void SReadWriteLocker::WriteUnlock()
{
	SysSemaphoreSignalCount(m_sem, 0xffffffff);
}

#endif // TARGET_HOST != TARGET_HOST_WIN32

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
