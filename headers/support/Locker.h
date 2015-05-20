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

#ifndef	_SUPPORT_LOCKER_H
#define	_SUPPORT_LOCKER_H

/*!	@file support/Locker.h
	@ingroup CoreSupportUtilities
	@brief Lightweight mutex class.
*/

#include <SysThread.h>
#include <support/SupportDefs.h>

#if TARGET_HOST != TARGET_HOST_WIN32
#include <pthread.h>
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SConditionVariable;

/*-------------------------------------------------------------*/

//!	Lightweight mutex.
class SLocker
{
public:
						SLocker();
						SLocker(const char* name);
						~SLocker();

		lock_status_t	Lock();
		void			LockQuick();
		void			Yield();
		void			Unlock();
		bool			IsLocked() const;

		class Autolock
		{
		public:
			inline Autolock(SLocker& lock);
			inline ~Autolock();
			inline void Unlock();
		private:
			SLocker* m_lock;
		};

private:
		friend class	SConditionVariable;

		// For use by SConditionVariable.
		void			RestoreOwnership();
		volatile int32_t*			RemoveOwnership();

		// Blocker can't be copied
						SLocker(const SLocker&);
		SLocker& 		operator = (const SLocker&);

		volatile int32_t			m_lockValue;
};

/*-------------------------------------------------------------*/
/*----- SNestedLocker class -----------------------------------*/

//!	A version of SLocker that allows nesting of Lock() calls.
/*!	@note If you find yourself needing to use this, it is
	probably because you are not being careful enough with
	your locking.  In general you should avoid this class;
	using it is usually the cliff after which locking becomes
	completely uncontrollable.
*/
class SNestedLocker
{
public:
						SNestedLocker();
						SNestedLocker(const char* name);
						~SNestedLocker();

		lock_status_t	Lock();
		void			LockQuick();
		void			Yield();
		void			Unlock();

		/*! Return the number of levels that this thread has
			nested the lock; if the caller isn't holding the
			lock, 0 is returned.
		*/
		int32_t			NestingLevel() const;

		class Autolock
		{
		public:
			inline Autolock(SNestedLocker& lock);
			inline ~Autolock();
			inline void Unlock();
		private:
			SNestedLocker* m_lock;
		};

private:
		// SNestedLocker can't be copied
						SNestedLocker(const SNestedLocker&);
		SNestedLocker& 	operator = (const SNestedLocker&);

static	void			_UnlockFunc(SNestedLocker* l);

		volatile int32_t			m_lockValue;
		int32_t			m_owner;
		int32_t			m_ownerCount;
};

//!	@note Do not use, may be broken.
class SReadWriteLocker
{
public:
						SReadWriteLocker();
						~SReadWriteLocker();

		lock_status_t	ReadLock();
		void			ReadLockQuick();
		void			ReadUnlock();

		lock_status_t	WriteLock();
		void			WriteLockQuick();
		void			WriteUnlock();

private:
		// SReadWriteLocker can't be copied
						SReadWriteLocker(const SReadWriteLocker&);
		SReadWriteLocker&	operator = (const SReadWriteLocker&);

#if TARGET_HOST == TARGET_HOST_WIN32
		SysHandle		m_sem;
#else
		pthread_rwlock_t m_lock;
#endif
};

/*!	@} */

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

inline SLocker::Autolock::Autolock(SLocker& lock)
	:	m_lock(&lock)
{
	lock.LockQuick();
}

inline SLocker::Autolock::~Autolock()
{
	if (m_lock) m_lock->Unlock();
}

inline void SLocker::Autolock::Unlock()
{
	if (m_lock) m_lock->Unlock();
	m_lock = NULL;
}

inline SNestedLocker::Autolock::Autolock(SNestedLocker& lock)
	:	m_lock(&lock)
{
	lock.LockQuick();
}

inline SNestedLocker::Autolock::~Autolock()
{
	if (m_lock) m_lock->Unlock();
}

inline void SNestedLocker::Autolock::Unlock()
{
	if (m_lock) m_lock->Unlock();
	m_lock = NULL;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif /* _SUPPORT_LOCKER_H */
