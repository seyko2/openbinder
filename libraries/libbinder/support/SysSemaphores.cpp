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

// Palm OS headers
#include <SysThread.h>		
#include <CmnErrors.h>
#include <ErrorMgr.h>
#include <libpalmroot.h>
#include <support/Errors.h>

// POSIX headers
#include <pthread.h>	
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


// FIXME: Move an opaque version of this to SysThread.h
struct SysFastSemaphoreType {
  uint32_t value;
  uint32_t max;
};

// FIXME: Declare these in SysThread.h
extern "C" status_t SysFastSemaphoreInitEZ(uint32_t initialCount, SysFastSemaphoreType* outSemaphore)
{
  outSemaphore->value = initialCount;
  outSemaphore->max = sysSemaphoreMaxCount;

  return 0; // FIXME: Success
}

extern "C" status_t SysFastSemaphoreInit(uint32_t initialCount, uint32_t maxCount, uint32_t flags, SysFastSemaphoreType* outSemaphore)
{
  outSemaphore->value = initialCount;
  outSemaphore->max = maxCount;

  return 0; // FIXME: Success
}

extern "C" status_t SysFastSemaphoreDestroy(SysFastSemaphoreType* semaphore)
{
  return 0; // FIXME: Success (should we release waiters?)
}

extern "C" status_t SysFastSemaphoreSignalCount(SysFastSemaphoreType* semaphore, uint32_t count)
{
  assert(0); // FIXME: unimplemented
}

extern "C" status_t SysFastSemaphoreSignal(SysFastSemaphoreType* semaphore)
{
  return SysFastSemaphoreSignalCount(semaphore, 1);
}

extern "C" status_t SysFastSemaphoreWait(SysFastSemaphoreType* semaphore, timeoutFlags_t iTimeoutFlags, nsecs_t iTimeout)
{
  assert(0); // FIXME: unimplemented
}

extern "C" status_t SysFastSemaphoreWaitCount(SysFastSemaphoreType* semaphore, timeoutFlags_t iTimeoutFlags, nsecs_t iTimeout, uint32_t count)
{
  assert(0); // FIXME: unimplemented
}


//######################################################################################


extern "C" status_t SysSemaphoreCreateEZ(uint32_t initialCount, SysHandle* outSemaphore)
{
	return SysSemaphoreCreate(initialCount, sysSemaphoreMaxCount, 0, outSemaphore);
}


extern "C" status_t SysSemaphoreCreate(uint32_t initialCount, uint32_t maxCount, uint32_t flags,
	SysHandle* outSemaphore)
{
	// clear out return in case of error
	if (outSemaphore)
		*outSemaphore = 0;

	if (initialCount > sysSemaphoreMaxCount)
	{
		DbgOnlyFatalError("Out-of-range initialCount supplied to SysSemaphoreCreate");
		return sysErrOutOfRange;
	}

	if (maxCount > sysSemaphoreMaxCount)
	{
		DbgOnlyFatalError("Out-of-range maxCount supplied to SysSemaphoreCreate");
		return sysErrOutOfRange;
	}
	
	if (initialCount > maxCount)
	{
		DbgOnlyFatalError("initialCount greater than maxCount supplied to SysSemaphoreCreate");
		return sysErrParamErr;
	}

	// ?? note: maxCount is ignored currently!

	DbgOnlyFatalErrorIf(flags != 0, "Non-zero flags supplied to SysSemaphoreCreate");
	
	sem_t* sem = (sem_t*) malloc(sizeof(sem_t));
	if (sem == NULL)
		return sysErrNoFreeResource;
		
	int result = sem_init(sem, 0, initialCount);
	int err = errno;

	if (result == -1)
	{
		free(sem);

		if (err == EINVAL)
			return sysErrOutOfRange;

		if (err == ENOSPC)
			return sysErrNoFreeResource;

		if (err == EPERM)
			return sysErrPermissionDenied;
		
		// ?
		return sysErrPermissionDenied; //B_ERROR
	}
	
	if (outSemaphore)
		* (sem_t**) outSemaphore = sem;

	return errNone;
}


extern "C" status_t SysSemaphoreDestroy(SysHandle semaphore)
{
	sem_t* sem = (sem_t*) semaphore;
	if (sem == NULL)
		return sysErrParamErr;

	int result = sem_destroy(sem);
	int err = errno;

	if (result == -1)
	{
		if (err == EINVAL)
			return sysErrParamErr;

		if (err == EBUSY)
			return sysErrBusy; //sysErrPermissionDenied
		
		// ?
		return sysErrPermissionDenied; //B_ERROR
	}

	free(sem);
	return errNone;
}


extern "C" status_t SysSemaphoreSignal(SysHandle semaphore)
{
	sem_t* sem = (sem_t*) semaphore;
	if (sem == NULL)
		return sysErrParamErr;

	int result = sem_post(sem);
	int err = errno;

	if (result == -1)
	{
		if (err == EINVAL)
			return sysErrParamErr;

		// ?
		return sysErrPermissionDenied; //B_ERROR
	}
	
	return errNone;
}

extern "C" status_t SysSemaphoreSignalCount(SysHandle semaphore, uint32_t count)
{
	sem_t * sem = (sem_t*)semaphore;
	while (count--)
	{
		sem_post(sem);
	}
	return errNone;
}


extern "C" status_t SysSemaphoreWait(SysHandle semaphore, timeoutFlags_t iTimeoutFlags,
	nsecs_t iTimeout)
{
	//?? should be implemented as a call to SysSemaphoreWaitCount with a count of 1...

	sem_t* sem = (sem_t*) semaphore;
	if (sem == NULL)
		return sysErrParamErr;
    
    int result = 0;
    int err = 0;
      
	if (iTimeoutFlags == B_WAIT_FOREVER) 
	{
		for (;;)
		{
			result = sem_wait(sem);
			err = errno;
			
			// If debugging with gdb, we'll get EINTR when single-stepping on
			// another thread. Presumably a signal is causing this thread to
			// wake early
			if ((result == -1) && (err == EINTR))
				continue;

			break;
		}
		
		if (result == -1)
		{
			if (err == EINVAL)
				return sysErrParamErr;

			if (err == EDEADLK)
				return sysErrBusy; // sysErrPermissionDenied

			// ?
			return sysErrPermissionDenied; //B_ERROR
		}
		
		return errNone;
	}
	else if ((iTimeoutFlags == B_RELATIVE_TIMEOUT) || (iTimeoutFlags == B_ABSOLUTE_TIMEOUT))
	{
		struct timespec absTimeOut;
		
		if (iTimeoutFlags == B_RELATIVE_TIMEOUT)
		{
			clock_gettime(CLOCK_REALTIME, &absTimeOut);
			palmos::clock_timespec_add_nano(&absTimeOut, iTimeout);
		}
		else /* (iTimeoutFlags == B_ABSOLUTE_TIMEOUT) */
		{
			palmos::clock_timespec_from_nano(&absTimeOut, iTimeout);
		}

		for (;;)
		{
			result = sem_timedwait(sem, &absTimeOut);
			err = errno;
			
			// If debugging with gdb, we'll get EINTR when single-stepping on
			// another thread. Presumably a signal is causing this thread to
			// wake early
			if ((result == -1) && (err == EINTR))
				continue;

			break;
		}
		
		if (result == -1)
		{
			if (err == EINVAL)
				return sysErrParamErr;

			if (err == EDEADLK)
				return sysErrBusy; //sysErrTimeout

			// ETIMEDOUT
			return sysErrTimeout;
		}
		
		return errNone;
	} 
	else if (iTimeoutFlags == B_POLL)
	{
		result = sem_trywait(sem);
		err = errno;
		if (result == -1)
		{
			if (err == EINVAL)
				return sysErrParamErr;

			if (err == EDEADLK)
				return sysErrBusy; //sysErrTimeout

			// ETIMEDOUT
			return sysErrTimeout;
		}
		
		return errNone;
	} 
	
	DbgOnlyFatalError("Invalid TimeoutFlags supplied to SysSemaphoreWait");
	return sysErrParamErr;
}


//######################################################################################


