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

/*!
	@file SysThread.h
	@ingroup CoreThread
	@brief Basic APIs for working with threads.
*/

#ifndef _SYSTHREAD_H_
#define _SYSTHREAD_H_

/*!	@addtogroup CoreThread Threads
	@ingroup Core
	@brief Basic system support for working with threads.

	Included here are APIs for creating and managing threads, using
	thread-specific data, synchronization between threads, fast atomic operations
	between threads, and timing services.

	Be sure to see the @ref CoreEventMgr "event manager" for some
	higher-level services, including IPC.
	@{
*/

// Include elementary types
#include <PalmTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// -------------------------------------------------------
// THREAD CONSTANTS
// -------------------------------------------------------

//! Standard thread priority levels.
/*!	ALWAYS use these constants unless you have a very, very good reason
	to use a special value. */
enum {
	sysThreadPriorityLowered		= 100,		//!< Background processing or CPU hogs.
	sysThreadPriorityNormal			= 80,		//!< Default priority for event handling.
	sysThreadPriorityBestApp		= 80,		//!< Highest priority for application process and any thread created by it.
	sysThreadPriorityRaised			= 70,		//!< Increased priority for user operations.
	sysThreadPriorityTransaction	= 65,		//!< UI transactions.
	sysThreadPriorityDisplay		= 60,		//!< Drawing to screen.
	sysThreadPriorityUrgentDisplay	= 50,		//!< Event collection and dispatching.
	sysThreadPriorityRealTime		= 40,		//!< Time critical such as audio.
	sysThreadPriorityBestUser		= 30,		//!< Highest priority for user programs.
	sysThreadPriorityBestSystem		= 5,		//!< Highest priority for system programs.

	sysThreadPriorityLow			= sysThreadPriorityLowered,	//!< Deprecated.  Use sysThreadPriorityLowered instead.
	sysThreadPriorityHigh			= sysThreadPriorityRealTime	//!< Deprecated.  Use sysThreadPriorityRealTime instead.
};

//!	Standard thread stack sizes.
enum {
	sysThreadStackBasic				= 4*1024,	//!< Minimum stack size for basic stuff.
	sysThreadStackUI				= 8*1024	//!< Minimum stack size for threads that do UI.
};

//! Timeout flags
typedef enum {
	B_WAIT_FOREVER              = 0x0000,
	B_POLL                      = 0x0300,
	B_RELATIVE_TIMEOUT          = 0x0100,
	B_ABSOLUTE_TIMEOUT          = 0x0200
} timeoutFlagsEnum_t;
typedef uint16_t timeoutFlags_t; //!< a combination of timeoutFlagsEnum_t and timebase_t flags

// -------------------------------------------------------
// THREAD SUPPORT
// -------------------------------------------------------

/*!	@name Atomic Operations */
//@{

int32_t SysAtomicInc32(int32_t volatile *ioOperandP);
int32_t SysAtomicDec32(int32_t volatile *ioOperandP);
int32_t SysAtomicAdd32(int32_t volatile *ioOperandP, int32_t iAddend);
uint32_t SysAtomicAnd32(uint32_t volatile *ioOperandP, uint32_t iValue);
uint32_t SysAtomicOr32(uint32_t volatile *ioOperandP, uint32_t iValue);
uint32_t SysAtomicCompareAndSwap32(uint32_t volatile *ioOperandP, uint32_t iOldValue,
				 uint32_t iNewValue);
//@}

/*!	@name Miscellaneous */
//@{

//!	Return the amount of time (in nanoseconds) the system has been running.
/*!	This time does \e not include time during which the device is asleep. */
nsecs_t SysGetRunTime(void);

//@]

/*!	@name Thread Specific Data */
//@{

typedef uint32_t SysTSDSlotID;
typedef void (SysTSDDestructorFunc)(void*);
enum { sysTSDAnonymous=0 }; // symbolic value for iName in unnamed slots
status_t	SysTSDAllocate(SysTSDSlotID *oTSDSlot, SysTSDDestructorFunc *iDestructor, uint32_t iName);
status_t	SysTSDFree(SysTSDSlotID tsdslot);
void		*SysTSDGet(SysTSDSlotID tsdslot);
void		SysTSDSet(SysTSDSlotID tsdslot, void *iValue);

//@}

/*!	@name Thread Finalizers */
//@{

typedef uint32_t SysThreadExitCallbackID;
typedef void (SysThreadExitCallbackFunc)(void*);
status_t	SysThreadInstallExitCallback(	SysThreadExitCallbackFunc *iExitCallbackP,
											void *iCallbackArg,
											SysThreadExitCallbackID *oThreadExitCallbackId);
status_t	SysThreadRemoveExitCallback(SysThreadExitCallbackID iThreadCallbackId);

//@}

/*!	@name Lightweight Locking */
//@{

//!	The value to use when initializing a SysCriticalSectionType.
#define sysCriticalSectionInitializer NULL
typedef void * SysCriticalSectionType;

#if (TARGET_HOST == TARGET_HOST_WIN32)
// Not needed for TARGET_HOST_WIN32
#else
#if 1 || TARGET_HOST == TARGET_HOST_PALMOS // FIXME: Linux host/target
void SysCriticalSectionEnter(SysCriticalSectionType *iCS);
void SysCriticalSectionExit(SysCriticalSectionType *iCS);
#endif //TARGET_HOST == TARGET_HOST_PALMOS
#endif // TARGET_HOST == TARGET_HOST_WIN32

/*!	@name Lightweight Condition Variables */
//@{

//!	The value to use when initializing a SysConditionVariableType.
#define sysConditionVariableInitializer 0
typedef void * SysConditionVariableType;

#if (TARGET_HOST == TARGET_HOST_WIN32)
// Not needed for TARGET_HOST_WIN32
#else
#if 1 || TARGET_HOST == TARGET_HOST_PALMOS // FIXME: Linux host/target
void SysConditionVariableWait(SysConditionVariableType *iCV, SysCriticalSectionType *iOptionalCS);
void SysConditionVariableOpen(SysConditionVariableType *iCV);
void SysConditionVariableClose(SysConditionVariableType *iCV);
void SysConditionVariableBroadcast(SysConditionVariableType *iCV);
#endif //TARGET_HOST == TARGET_HOST_PALMOS
#endif // TARGET_HOST == TARGET_HOST_WIN32

//@}

/*!	@name Counting Semaphores */
//@{

enum { sysSemaphoreMaxCount = 0xffff };
status_t	SysSemaphoreCreateEZ(uint32_t initialCount, SysHandle* outSemaphore);
status_t	SysSemaphoreCreate(	uint32_t initialCount, uint32_t maxCount, uint32_t flags,
								SysHandle* outSemaphore);
status_t	SysSemaphoreDestroy(SysHandle semaphore);
status_t	SysSemaphoreSignal(SysHandle semaphore);
status_t	SysSemaphoreSignalCount(SysHandle semaphore, uint32_t count);
status_t	SysSemaphoreWait(	SysHandle semaphore,
								timeoutFlags_t iTimeoutFlags, nsecs_t iTimeout);
status_t	SysSemaphoreWaitCount(	SysHandle semaphore,
									timeoutFlags_t iTimeoutFlags, nsecs_t iTimeout,
									uint32_t count);

//@}

// -------------------------------------------------------
// THREAD GROUPS
// -------------------------------------------------------

/*!	@name Thread Groups

	These are a convenience provided by the system
	library to wait for one or more threads to exit.  It is useful
	for unloading libraries that have spawned their own threads.
	Note that destroying a thread group implicitly waits for all
	threads in that group to exit. */
//@{

struct SysThreadGroupTag;
typedef struct SysThreadGroupTag SysThreadGroupType;
typedef SysThreadGroupType*	SysThreadGroupHandle;

SysThreadGroupHandle	SysThreadGroupCreate(void);
status_t				SysThreadGroupDestroy(SysThreadGroupHandle group);
status_t				SysThreadGroupWait(SysThreadGroupHandle group);

// Use this if you don't want the thread in a group.
#define			sysThreadNoGroup	NULL

//@}

// -------------------------------------------------------
// THREAD CREATION
// -------------------------------------------------------

/*!	@name Thread Creation and Management */
//@{

//!	Create a new thread.
/*!	@param[in] name First four letters are used for thread ID.
	@param[in] func Entry point of thread.
	@param[in] argument Data passed to above.
	@param[out] outThtread Returned thread handle.

	The thread's priority is sysThreadPriorityNormal and its
	stack size is sysThreadStackUI.

	The returned SysHandle is owned by the newly created thread, and
	we be freed when that thread exits.  It is the same SysHandle as
	the thread uses for its local identity in its TSD, so you can
	use this as a unique identity for the thread. 
*/
typedef void (SysThreadEnterFunc) (void *);
status_t	SysThreadCreateEZ(	const char *name,
								SysThreadEnterFunc *func, void *argument,
								SysHandle* outThread);

//!	Like SysThreadCreateEZ(), but with more stuff.
/*!	@param[in] group Thread group, normally sysThreadNoGroup.
	@param[in] name First four letters are used for thread ID.
	@param[in] priority Pick one of the thread priority constants.
	@param[in] stackSize Bytes available for the thread's stack.
	@param[in] func Entry point of thread.
	@param[in] argument Data passed to above.
	@param[out] outThtread Returned thread handle.
*/
status_t	SysThreadCreate(SysThreadGroupHandle group, const char *name,
							uint8_t priority, uint32_t stackSize,
							SysThreadEnterFunc *func, void *argument,
							SysHandle* outThread);

//!	Start a thread that was created above with SysThreadCreate() or SysThreadCreateEZ().
/*!	@param[in] thread Thread handle as returned by the creation function.

	If this function fails, the thread handle and all
	associated resources will be deallocated. */
status_t	SysThreadStart(SysHandle thread);

//!	Deliver a POSIX signal to a thread.
/*!	@param[in] thread Thread handle as returned by the creation function.
        @param[in] signo  Signal number

	Returns a status value the same as kill(). */
int		SysThreadKill(SysHandle thread, int signo);

//!	Return the handle for the calling thread.
SysHandle	SysCurrentThread(void);

//!	Call this function to have the current thread exit.
/*!	The thread MUST have been created with SysThreadCreate(). */
void		SysThreadExit(void);

//!	Go to sleep for the given amount of time.
status_t	SysThreadDelay(nsecs_t timeout, timeoutFlags_t flags);

#if 0 // FIXME: Missing API, not supportable with pthreads LINUX_DEMO_HACK
//!	Stop a thread from running.  Use SysThreadResume() to make it continue running.
status_t	SysThreadSuspend(SysHandle thread);
//!	Start a thread for which you had previously called SysThreadSuspend().
/*!	The thread continues execution at the point where it was suspended. */
status_t	SysThreadResume(SysHandle thread);
#endif

//!	Change a thread's priority.
status_t	SysThreadChangePriority(SysHandle thread, uint8_t priority);

//@}

#ifdef __cplusplus
}	// extern "C"
#endif

/*! @} */

#endif // _SYSTHREAD_H_
