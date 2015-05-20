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

#ifndef _SUPPORT_DEBUGLOCK_H
#define _SUPPORT_DEBUGLOCK_H

#include <BuildDefines.h>

#ifndef SUPPORTS_LOCK_DEBUG

// Lock debugging is not specified.  Turn it on if this is a debug build.
#if BUILD_TYPE == BUILD_TYPE_DEBUG
#define SUPPORTS_LOCK_DEBUG 1
#else
#define SUPPORTS_LOCK_DEBUG 0
#endif

#endif

#if !SUPPORTS_LOCK_DEBUG

static inline void AssertNoLocks() { }

#else	// !SUPPORTS_LOCK_DEBUG

#include <support/SupportDefs.h>
#include <support/StdIO.h>
#include <support/String.h>
#include <support/StringIO.h>

#include <new>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

class SCallStack;

static int32_t LockDebugLevel();
static bool LockDebugStackCrawls();

// Assert if the calling thread is currently holding any
// locks that are instrumented by lock debugging.
void AssertNoLocks();

class StaticStringIO : public sptr<BStringIO>
{
public:
	inline StaticStringIO() : sptr<BStringIO>(new BStringIO) { }
	inline ~StaticStringIO() { }
};

struct block_links;

enum {
	/*
	 * These flags can be passed in to the lock constructor.
	 */
	
	// Set to allow the lock to be deleted by a thread that is holding it.
	LOCK_CAN_DELETE_WHILE_HELD		= 0x00000001,
	
	// Set to allow the lock to always be deleted, even if another thread holds it.
	LOCK_ANYONE_CAN_DELETE			= 0x00000002,
	
	/*
	 * These flags can be passed in to the lock constructor or Lock() function.
	 */
	
	// Set to not perform deadlock checks -- don't add this acquire to deadlock graph.
	LOCK_SKIP_DEADLOCK_CHECK		= 0x00010000,
	
	// Set to not remember for future deadlock checks that this lock is being held.
	LOCK_DO_NOT_REGISTER_HELD		= 0x00020000,
	
	// Set to allow a threads besides the current owner to perform an unlock.
	LOCK_ANYONE_CAN_UNLOCK			= 0x00040000
};

class DebugLockNode
{
public:
								DebugLockNode(	const char* type,
												const void* addr,
												const char* name,
												uint32_t debug_flags = 0);
			
	virtual	void				Delete();
			
			int32_t				IncRefs() const;
			int32_t				DecRefs() const;
			
			const char*			Type() const;
			const void*			Addr() const;
			const char*			Name() const;
			uint32_t			GlobalFlags() const;
			
			const SCallStack*	CreationStack() const;
	
			nsecs_t				CreationTime() const;

			bool				LockGraph() const;
			void				UnlockGraph() const;
			bool				AddToGraph();
			void				RegisterAsHeld();
			void				UnregisterAsHeld();
	
			void				SetMaxContention(int32_t c);
			int32_t				MaxContention() const;
	
			int32_t				BlockCount() const;

			void				RemoveFromContention();
			void				RemoveFromBlockCount();

	virtual	void				PrintToStream(const sptr<ITextOutput>& io) const;
	virtual	void				PrintStacksToStream(sptr<ITextOutput>& io) const;
	static	void				PrintContentionToStream(const sptr<ITextOutput>& io);
	
	// Private implementation.
	inline	const block_links*	Links() const	{ return m_links; }
	
protected:
	virtual						~DebugLockNode();
	virtual	void				PrintSubclassToStream(const sptr<ITextOutput>& io) const;
	
private:
	mutable	int32_t				m_refs;			// instance reference count
	
	// Lock-specific information.  Does not change after creation.
			SString const		m_type;			// descriptive type of this object
			const void* const	m_addr;			// user-visible address of lock
			SString const		m_name;			// descriptive name of lock
			uint32_t const		m_globalFlags;	// overall operating flags
			SCallStack* const	m_createStack;	// where the lock was created
			nsecs_t const		m_createTime;	// when the lock was created
	
	// Lock linkage information.  Protected by the global graph lock.
			block_links*		m_links;		// linkage to other locks

	// Lock contention tracking.  Protected by global contention lock.
			int32_t				m_maxContention;// maximum number of waiting threads
			bool				m_inContention;	// in the high contention list?
			int32_t				m_blockCount;	// number of times a thread has blocked here.
			bool				m_inBlockCount;	// in the high block count list?
};

inline const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const DebugLockNode& db)
{
	db.PrintToStream(io);
	return io;
}

class DebugLock : public DebugLockNode
{
public:
								DebugLock(	const char* type,
											const void* addr,
											const char* name,
											uint32_t debug_flags = 0);
	
	virtual	void				Delete();
	
	virtual	void				PrintStacksToStream(sptr<ITextOutput>& io) const;

			status_t			Lock(	uint32_t flags = B_TIMEOUT,
										nsecs_t timeout = B_INFINITE_TIMEOUT,
										uint32_t debug_flags = 0);
			status_t			Unlock();
			bool				IsLocked() const;

			const SCallStack*	OwnerStack() const;

			// For use by SConditionVariable::Wait().
			void				RestoreOwnership();
			volatile int32_t*				RemoveOwnership();
			
protected:
	virtual						~DebugLock();
	virtual	void				PrintSubclassToStream(const sptr<ITextOutput>& io) const;
	
private:
			status_t			do_lock(uint32_t flags, nsecs_t timeout,
										uint32_t debug_flags, bool restoreOnly = false);
			status_t			do_unlock(bool removeOnly = false);

	// Lock-specific information.  Protected by the lock itself.
			volatile int32_t		m_gehnaphore;	// implementation of this lock
			sem_id				m_sem;			// alternate implementation
			int32_t				m_held;			// lock implementation validation
			SysHandle			m_owner;		// who currently holds the lock
			int32_t				m_ownerFlags;	// passed in from Lock()
			SCallStack*			m_ownerStack;	// stack of last lock accessor
			int32_t				m_contention;	// current waiting threads
			bool				m_deleted;		// no longer valid?
};


extern int32_t gLockDebugLevel;
int32_t LockDebugLevelSlow();
static inline int32_t LockDebugLevel() {
	if (gLockDebugLevel >= 0) return gLockDebugLevel;
	return LockDebugLevelSlow();
}

extern bool gLockDebugStackCrawls;
static inline bool LockDebugStackCrawls() {
	if (gLockDebugLevel < 0) LockDebugLevelSlow();
	return gLockDebugStackCrawls;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif	// !SUPPORTS_LOCK_DEBUG

#endif	// _SUPPORT_DEBUGLOCK_H
