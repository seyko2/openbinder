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

#include <support/ConditionVariable.h>

#include <support/atomic.h>
#include <support/Locker.h>
#include <support/StdIO.h>

#include <support_p/DebugLock.h>
#include <support_p/SupportMisc.h>

#include <stdio.h>
#include <stdlib.h>

#if TARGET_HOST == TARGET_HOST_WIN32
#include <support_p/WindowsCompatibility.h>
#include <stdio.h>
#endif

#include <SysThread.h>

// -----------------------------------------------------------------
// -----------------------------------------------------------------
// -----------------------------------------------------------------

#if !SUPPORTS_LOCK_DEBUG

//#pragma mark -

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

static inline void init_cv(volatile int32_t* value)
{
	*value = sysConditionVariableInitializer;
}

static inline void init_cv(volatile int32_t* value, const char* /*name*/)
{
	*value = sysConditionVariableInitializer;
}

static inline void fini_cv(volatile int32_t* value)
{
	(void)value;
}

static inline void wait_cv(volatile int32_t* value, volatile int32_t* critSec)
{
	g_threadDirectFuncs.conditionVariableWait((SysConditionVariableType*)value, (SysCriticalSectionType*)critSec);
}

static inline void open_cv(volatile int32_t* value)
{
	g_threadDirectFuncs.conditionVariableOpen((SysConditionVariableType*)value);
}

static inline void close_cv(volatile int32_t* value)
{
	g_threadDirectFuncs.conditionVariableClose((SysConditionVariableType*)value);
}

static inline void broadcast_cv(volatile int32_t* value)
{
	g_threadDirectFuncs.conditionVariableBroadcast((SysConditionVariableType*)value);
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

// -----------------------------------------------------------------

#else

#include <stdlib.h>
#include <new>

//#pragma mark -

// No debugging implemented yet.  Boo hoo!

static inline void init_cv(volatile int32_t* value)
{
	*value = sysConditionVariableInitializer;
}

static inline void init_cv(volatile int32_t* value, const char* /*name*/)
{
	*value = sysConditionVariableInitializer;
}

static inline void fini_cv(volatile int32_t* value)
{
	(void)value;
}

static inline void wait_cv(volatile int32_t* value, volatile int32_t* critSec)
{
	SysConditionVariableWait((SysConditionVariableType*)value, (SysCriticalSectionType*)critSec);
}

static inline void open_cv(volatile int32_t* value)
{
	SysConditionVariableOpen((SysConditionVariableType*)value);
}

static inline void close_cv(volatile int32_t* value)
{
	SysConditionVariableClose((SysConditionVariableType*)value);
}

static inline void broadcast_cv(volatile int32_t* value)
{
	SysConditionVariableBroadcast((SysConditionVariableType*)value);
}

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif

// -----------------------------------------------------------------
// ---------------------------- SConditionVariable ----------------------------
// -----------------------------------------------------------------

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

//#pragma mark -

SConditionVariable::SConditionVariable()
{
	init_cv(&m_var);
}

SConditionVariable::SConditionVariable(const char* name)
{
	init_cv(&m_var, name);
}

SConditionVariable::~SConditionVariable()
{
	fini_cv(&m_var);
}

void SConditionVariable::Wait()
{
	wait_cv(&m_var, NULL);
}

void SConditionVariable::Wait(SLocker& locker)
{
	// Need to do a little magic here to not trip
	// over the lock debugging code -- let it know
	// that we (may) no longer hold the lock.
	wait_cv(&m_var, locker.RemoveOwnership());
	locker.RestoreOwnership();
}

void SConditionVariable::Open()
{
	open_cv(&m_var);
}

void SConditionVariable::Close()
{
	close_cv(&m_var);
}

void SConditionVariable::Broadcast()
{
	broadcast_cv(&m_var);
}


#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
