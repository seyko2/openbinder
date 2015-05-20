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

#include <support/atomic.h>
#include <support/ByteOrder.h>
#include <support/Errors.h>
#include <support/StdIO.h>
#include <support/String.h>
#include <support/SupportDefs.h>
#include <support/TLS.h>

#include <PalmTypesCompatibility.h>

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#include <ErrorMgr.h>
#include <LocaleMgr.h>
#include <TextMgr.h>
#include <TextMgrPrv.h>
#include <SysThreadConcealed.h>

#include <sys/kernel.h>

#if TARGET_HOST == TARGET_HOST_MACOS

extern "C" {

nsecs_t SysGetRunTime()
{
	return time(NULL) * B_ONE_SECOND;
}

CharEncodingType LmGetSystemLocale(LmLocaleType* oSystemLocale)
{
	//Crash because this is not implemented
	(*((int*)NULL)) = 5;
	return 0;
}

size_t TxtSetNextChar(char* iTextP, size_t iOffset, wchar32_t iChar)
{
	//Crash because this is not implemented
	(*((int*)NULL)) = 5;
	return 0;
}

status_t TxtDeviceToUTF32Lengths(const uint8_t* srcTextP, size_t srcBytes,
						wchar32_t* dstTextP, uint8_t* srcCharLengths,
						size_t dstLength)
{
	//Crash because this is not implemented
	(*((int*)NULL)) = 5;
	return 0;
}

status_t TxtConvertEncoding(Boolean newConversion, TxtConvertStateType* ioStateP,
			const char* srcTextP, size_t* ioSrcBytes, CharEncodingType srcEncoding,
			char* dstTextP, size_t* ioDstBytes, CharEncodingType dstEncoding,
			const char* substitutionStr, size_t substitutionLen)
{
	//Crash because this is not implemented
	(*((int*)NULL)) = 5;
	return 0;
}

void ErrFatalErrorInContext(const char* fileName, uint32_t lineNum, const char* errMsg)
{
	printf("%s:%lu %s", fileName, lineNum, errMsg);
	debugger(errMsg);
}

// <SysThread.h>

SysHandle SysCurrentThread(void)
{
	return (SysHandle)pthread_self();
}

const char* SysProcessName()
{
	return "[noname]";
}

uint32_t SysProcessID()
{
	return 0;
}

status_t SysTSDAllocate(SysTSDSlotID *oTSDSlot, SysTSDDestructorFunc *iDestructor, uint32_t iName)
{
	status_t err = pthread_key_create((pthread_key_t*)oTSDSlot, iDestructor);
	return err;
}

status_t SysTSDFree(SysTSDSlotID tsdslot)
{
	return pthread_key_delete(tsdslot);
}

void* SysTSDGet(SysTSDSlotID tsdslot)
{
	void* data = pthread_getspecific(tsdslot);
	return data;
}

void SysTSDSet(SysTSDSlotID tsdslot, void* data)
{
	status_t err = pthread_setspecific(tsdslot, data);
}

// System stuff

status_t KALCurrentThreadDelay(nsecs_t timeout, timeoutFlags_t flags)
{
	if (timeout == B_INFINITE_TIMEOUT)
	{
		printf("[snooze]: B_INFINITE_TIMEOUT is unsported on Darwin!");
		return B_ERROR;
	}
	
	struct timespec rqtp;
	rqtp.tv_sec = timeout / B_ONE_SECOND;
	rqtp.tv_nsec = (rqtp.tv_sec == 0) ? timeout : timeout - (rqtp.tv_sec * B_ONE_SECOND);

	struct timespec rmtp;
	rmtp.tv_sec = 0;
	rmtp.tv_nsec = 0;

	status_t err = nanosleep(&rqtp, &rmtp);
	while (err == B_ERROR && errno == EINTR)
	{
		err = nanosleep(&rmtp, &rmtp);
	}

	return err;
}


// <kernel/image.h>
image_id load_add_on(const char *path)
{
	ErrFatalError("load_add_on not implemented for Darwin!");
	return B_ERROR;
}

status_t unload_add_on(image_id imid)
{
	ErrFatalError("unload_add_on not implemented for Darwin!");
	return B_ERROR;
}

status_t get_image_symbol(image_id imid, const char *name, int32_t sclass, void **ptr)
{
	ErrFatalError("get_image_symbol not implemented for Darwin!");
	return B_ERROR;
}

#if 0
status_t get_next_image_info(team_id team, int32_t *cookie, image_info *info)
{
	ErrFatalError("Not implemented for Darwin!");
	return B_ERROR;
}
#endif

status_t get_nth_image_symbol(image_id imid, int32_t index, char *buf, int32_t *bufsize, int32_t *sclass, void **ptr)
{
	ErrFatalError("get_nth_image_symbol not implemented for Darwin!");
	return B_ERROR;
}


// <support/atomic.h>
int32_t SysAtomicInc32(volatile int32_t* location)
{
	return atomic_add(location, 1);
}

int32_t SysAtomicDec32(volatile int32_t* location)
{
	return atomic_add(location, -1);
}

int32_t atomic_add(volatile int32_t* location, int32_t addvalue)
{
	int32_t res;

	asm volatile
	(
		"1:\t"
		"lwarx   %0,0,%1\n"
		"add%I2  %0,%0,%2\n"
		"stwcx.  %0,0,%1\n"
		"bne-    1b"
		: "=&r" (res)
		: "r" (location), "r" (addvalue)
		: "cr0", "memory"
	);

	
	return res;
}

int32_t atomic_and(volatile int32_t* location, int32_t andvalue)
{
	int32_t res;
	int32_t newValue;

	asm volatile
	(
		"1:\t"
		"lwarx   %0,0,%1\n"
		"and	 %3,%0,%2\n"
		"stwcx.  %3,0,%1\n"
		"bne-    1b"
		: "=&r" (res)
		: "r" (location), "r" (andvalue), "r" (newValue)
		: "cr0", "memory"
	);

	
	return res;
}

int32_t atomic_or(volatile int32_t* location, int32_t orvalue)	
{
	int32_t res;
	int32_t newValue;
	
	asm volatile
	(
		"1:\t"
		"lwarx   %0,0,%1\n"
		"or		 %3,%0,%2\n"
		"stwcx.  %3,0,%1\n"
		"bne-    1b"
		: "=&r" (res)
		: "r" (location), "r" (orvalue), "r" (newValue)
		: "cr0", "memory"
	);
	
	return res;
}

int32_t compare_and_swap32(volatile int32_t *location, int32_t oldValue, int32_t newValue)
{
	int32_t res;
    
	asm volatile
	(
		"loop:\t"
		"lwarx    %0,0,%1 \n\t"
		"cmpw     %3,%0 \n\t"
		"bne-     exit \n\t"
		"stwcx.   %2,0,%1 \n\t"
		"bne-     loop\n\t"
		"exit:\t"
		: "=&r"(res)
		: "r"(location), "r"(newValue), "r"(oldValue)
		: "cr0", "memory");
	
	return (*location == oldValue) ? 0 : 1;
}

int32_t compare_and_swap64(volatile int64_t *location, int64_t oldValue, int64_t newValue)
{
	ErrFatalError("compare_and_swap64 not implemented for Darwin!");
	return 0;
}

// <kernel/OS.h>
status_t SysSemaphoreWaitCount(SysHandle sem, timeoutFlags_t iTimeoutFlags, nsecs_t iTimeout, uint32_t count)
{
	return B_ERROR;
}

status_t SysSemaphoreWait(SysHandle semaphore, timeoutFlags_t iTimeoutFlags, nsecs_t iTimeout)
{
	return SysSemaphoreWaitCount(semaphore, iTimeoutFlags, iTimeout, 1);
}

status_t SysSemaphoreCreateEZ(uint32_t initialCount, SysHandle* outSemaphore)
{
	return B_ERROR;
}

status_t SysSemaphoreDestroy(SysHandle semaphore)
{
	
	return B_ERROR;
}

status_t SysThreadInstallExitCallback(SysThreadExitCallbackFunc * iExitCallbackP, void * iCallbackArg, SysThreadExitCallbackID * oThreadExitCallbackId)
{
	return 0;
}

status_t SysSemaphoreSignal(SysHandle semaphore) 
{
	return B_ERROR;
}

void* inplace_realloc (void *, size_t)
{
	return NULL;
}

// <driver/binder2_driver.h>

nsecs_t	system_real_time (void)
{
	ErrFatalError("system_real_time function has not been implemented for BSD.");
	return 0;
}

int open_binder(int teamID)
{
	return 0;
	/* To be done later... */
}

status_t close_binder(int desc)
{
	return 0;
	/* To be done later... */
}

status_t ioctl_binder(int desc, int cmd, void *data, int32_t len)
{
	return 0;
	/* To be done later... */
}

ssize_t read_binder(int desc, void *data, size_t numBytes)
{
	return 0;
	/* To be done later... */
}

ssize_t write_binder(int desc, void *data, size_t numBytes)
{
	return 0;
	/* To be done later... */
}

}

#endif // 
