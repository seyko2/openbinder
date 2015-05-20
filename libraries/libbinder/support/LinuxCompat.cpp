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
#include <support/CallStack.h>
#include <support/Errors.h>
#include <support/StdIO.h>
#include <support/String.h>
#include <support/SupportDefs.h>
#include <support/TLS.h>

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <ctype.h>

#include <ErrorMgr.h>
#include <LocaleMgr.h>
#include <TextMgr.h>
#include <SysThreadConcealed.h>

#if TARGET_HOST == TARGET_HOST_LINUX

extern "C" {

CharEncodingType LmGetSystemLocale(LmLocaleType* oSystemLocale)
{
	//?? FIX ME!

	if (oSystemLocale != NULL)
	{
		oSystemLocale->language = lEnglish;
		oSystemLocale->country = cUnitedStates;
	}

	return charEncodingPalmLatin; // maybe UTF-8 now?
}

size_t TxtSetNextChar(char* text, size_t offset, wchar32_t replaceMe)
{
	text[offset] = (char)replaceMe;
	return sizeof(char);
}

status_t TxtDeviceToUTF32Lengths(const uint8_t* srcTextP, size_t srcBytes,
						wchar32_t* dstTextP, uint8_t* charLengths,
						size_t dstLength)
{
	size_t i;
	for (i = 0 ; i < srcBytes ; i++)
	{
		if (i >= dstLength) break;

		dstTextP[i] = srcTextP[i];
		charLengths[i] = 1;
	}
	
	return i;
}

status_t TxtConvertEncoding(Boolean newConversion, TxtConvertStateType* ioStateP,
			const char* srcTextP, size_t* ioSrcBytes, CharEncodingType srcEncoding,
			char* dstTextP, size_t* ioDstBytes, CharEncodingType dstEncoding,
			const char* substitutionStr, size_t substitutionLen)
{
    /* XXXX - Linux Hack, don't do conversion, but fill in some values */
	if (ioSrcBytes) {
		*ioSrcBytes = strlen(srcTextP);
	}
	if (ioDstBytes) {
		*ioDstBytes = strlen(srcTextP);
	}
	if (srcTextP) {
		if (dstTextP) {
			strcpy(dstTextP, srcTextP);
	    }
	    return errNone;
	}

	//?? FIX ME!
	return -1;
}

extern "C" void
ErrFatalErrorInContext(const char* file, uint32_t line, const char* errMsg)
{
	// LINUX_DEMO_HACK should also dump to syslog and the alert driver
	fprintf(stderr, "\n======================================================================\n");
	if (file && line) {
		fprintf(stderr, "FATAL ERROR in %s(%d) process %d:\n", file, line, getpid());
	}
	else if (file) {
		fprintf(stderr, "FATAL ERROR in %s process %d:\n", file, getpid());
	}
	else if (line) {
		fprintf(stderr, "FATAL ERROR in (%d) process %d:\n", line, getpid());
	}
	else {
		fprintf(stderr, "FATAL ERROR in process %d:\n", getpid());
	}
	fprintf(stderr, "%s\n", errMsg);
	fprintf(stderr, "----------------------------------------------------------------------\n");
	SCallStack crawl;
	crawl.Update();
	crawl.LongPrint(berr);
	fprintf(stderr, "======================================================================\n\n");
	
	const char* dbgMode = getenv("FAULT_ON_FATAL_ALERT");
	if ((dbgMode != NULL) && (strcmp(dbgMode, "1") == 0)) {
		*(int*)3 = 4;
	}
	
	// FIXME: as above, should go alert driver (which might itself be camping out
    // on the console)
	if (isatty(fileno(stdin))) {
	
        fprintf(stderr, "Abort, or Ignore and continue? [Ai] ");
		fflush(stderr);

		char buf[6];

		if (fgets(buf, sizeof(buf), stdin) && tolower(buf[0]) == 'i') {
			fprintf(stderr, "- Ignoring fatal error, will continue -\n");
			return;
		} 
    }

	fprintf(stderr, "- Aborting on fatal error -\n");
	abort();

#if !LINUX_DEMO_HACK
	// dump the message to syslog
	if (i_am_smooved()) {
		// reset the whole device
	} else {
		// put the appropriate message to the driver
		// abort -- which will break into the debugger if it's attached
#endif
}

const char* SysProcessName()
{
	return "[noname]";
}

uint32_t SysProcessID()
{
	return (uint32_t)getpid();
}

#if 0 // TSD is in libpalmroot
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
#endif

// System stuff

#if 0 // SysThreadDelay() is in libpalmroot
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
#else
status_t KALCurrentThreadDelay(nsecs_t timeout, timeoutFlags_t flags)
{
	return SysThreadDelay(timeout, flags);
}
#endif


// <kernel/image.h>
image_id load_add_on(const char *path)
{
	ErrFatalError("load_add_on not implemented for Darwin!");
	return B_BAD_IMAGE_ID;
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
#if 0 // Now implemented in libpalmroot
int32_t SysAtomicInc32(volatile int32_t* location)
{
	return atomic_add(location, 1);
}

int32_t SysAtomicDec32(volatile int32_t* location)
{
	return atomic_add(location, -1);
}
#endif

#if 0 // Now implemented in libpalmroot
int32_t atomic_add(volatile int32_t* location, int32_t addvalue)
{
	int32_t res;

	asm volatile
	(
		"atomic_add1:"
		"mov %2, %%ecx;"
		"mov (%1), %0;"
		"add %0, %%ecx;"
		"lock; cmpxchg %%ecx, (%1);"
		"jnz atomic_add1;"
		: "=&a"(res)
		: "r"(location), "r"(addvalue)
		: "%ecx"
	);
	
	return res;
}

int32_t atomic_and(volatile int32_t* location, int32_t andvalue)
{
	int32_t res;

	asm volatile
	(
		"atomic_and1:"
		"mov %2, %%ecx;"
		"mov (%1), %0;"
		"and %0, %%ecx;"
		"lock; cmpxchg %%ecx, (%1);"
		"jnz atomic_and1;"
		: "=&a"(res)
		: "r"(location), "r"(andvalue)
		: "%ecx"
	);

	return res;
}

int32_t atomic_or(volatile int32_t* location, int32_t orvalue)	
{
	int32_t res;

	asm volatile
	(
		"atomic_or1:"
		"mov %2, %%ecx;"
		"mov (%1), %0;"
		"or %0, %%ecx;"
		"lock; cmpxchg %%ecx, (%1);"
		"jnz atomic_or1;"
		: "=&a"(res)
		: "r"(location), "r"(orvalue)
		: "%ecx"
	);

	return res;

}

int32_t compare_and_swap32(volatile int32_t *location, int32_t oldValue, int32_t newValue)
{
	int32_t success;
	asm volatile
	(
	 	"lock; cmpxchg %%ecx, (%%edx); sete %%al; andl $1, %%eax"
		: "=a" (success) 
		: "a" (oldValue), "c" (newValue), "d" (location)
	);
	return success;
}
#else

// Use the Sys routines, in libpalmroot

int32_t atomic_add(volatile int32_t* location, int32_t addvalue)
{
	return SysAtomicAdd32(location, addvalue);
}

int32_t atomic_and(volatile int32_t* location, int32_t andvalue)
{
	return SysAtomicAnd32((volatile uint32_t*)location, andvalue);
}

int32_t atomic_or(volatile int32_t* location, int32_t orvalue)	
{
	return SysAtomicOr32((volatile uint32_t*)location, orvalue);
}

int32_t compare_and_swap32(volatile int32_t *location, int32_t oldValue, int32_t newValue)
{
	return !SysAtomicCompareAndSwap32((volatile uint32_t*)location, oldValue, newValue);
}
#endif


int32_t compare_and_swap64(volatile int64_t *location, int64_t oldValue, int64_t newValue)
{
	ErrFatalError("compare_and_swap64 not implemented for Darwin!");
	return 0;
}

// <kernel/OS.h>
#if 0 // Now implemented in libpalmroot
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
#endif


// <driver/binder2_driver.h>

nsecs_t	system_real_time (void)
{
	return SysGetRunTime();
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

#if 0
DmOpenRef DmOpenDatabase(DatabaseID dbID, DmOpenModeType mode)
{
	return 0;
}

DmOpenRef DmOpenDBNoOverlay(DatabaseID dbID, DmOpenModeType mode)
{
	return 0;
}

DmOpenRef DmOpenDatabaseByTypeCreator(uint32_t type, uint32_t creator, DmOpenModeType mode)
{
	return 0;
}

status_t DmCloseDatabase(DmOpenRef dbRef)
{
	return 0;
}

status_t DmGetLastErr(void)
{
	return 0;
}

MemPtr DmHandleLock(MemHandle handle)
{
	return 0;
}

status_t DmHandleUnlock(MemHandle handle)
{
	return 0;
}

uint32_t DmHandleSize(MemHandle handle)
{
	return 0;
}

status_t DmCreateDatabaseFromImage(MemPtr pImage, DatabaseID* pDbID)
{
	return 0;
}

DatabaseID DmFindDatabase(const char* nameP, uint32_t creator, DmFindType find, DmDatabaseInfoPtr databaseInfoP)
{
	return 0;
}

status_t DmDatabaseInfo(DatabaseID dbID, DmDatabaseInfoPtr pDatabaseInfo)
{
	return 0;
}

DatabaseID DmFindDatabaseByTypeCreator(uint32_t type, uint32_t creator, DmFindType find, DmDatabaseInfoPtr databaseInfoP)
{
	return 0;
}

status_t DmGetOpenInfo(DmOpenRef dbRef, DatabaseID*	pDbID, uint16_t* pOpenCount, DmOpenModeType* pOpenMode, Boolean* pResDB)
{
	return 0;
}

MemHandle DmGetResource(DmOpenRef dbRef, DmResourceType resType, DmResourceID resID)
{
	return 0;
}

status_t DmReleaseResource(MemHandle hResource)
{
	return 0;
}

status_t DmPtrUnlock(MemPtr p)
{
	return 0;
}

DmOpenRef DbOpenDatabase(DatabaseID			dbID, DmOpenModeType		mode, DbShareModeType	share)
{
	return 0;
}

DmOpenRef DbOpenDatabaseByName(uint32_t			creator, const char		* name, DmOpenModeType	mode, DbShareModeType	share)
{
	return 0;
}

status_t DbCloseDatabase(DmOpenRef dbRef)
{
	return 0;
}

status_t DbCursorOpen(DmOpenRef			dbRef, const char		* sql, uint32_t			flags, uint32_t			* cursorID)
{
	return 0;
}

status_t DbCursorClose(uint32_t cursorID)
{
	return 0;
}

uint32_t DbCursorGetRowCount(uint32_t cursorID)
{
	return 0;
}

status_t DbCursorMove(uint32_t cursorID, int32_t offset, DbFetchType fetchType)
{
	return 0;
}

status_t DbGetColumnValues(DmOpenRef dbRef, uint32_t  rowID, uint32_t  numColumns, const uint32_t columnIDs[], DbSchemaColumnValueType** columnValuesPP)
{
	return 0;
}

status_t DbAddSortIndex(DmOpenRef dbRef, const char * orderBy)
{
	return 0;
}

status_t DbReleaseStorage(DmOpenRef dbRef, void * ptr)
{
	return 0;
}

status_t SysThreadCreate(SysThreadGroupHandle group, const char *name, uint8_t priority, uint32_t stackSize, SysThreadEnterFunc *func, void *argument, SysHandle* outThread)
{
	return 0;
}

status_t SysThreadCreateEZ(const char *name, SysThreadEnterFunc *func, void *argument, SysHandle* outThread)
{
	return 0;
}

status_t SysThreadStart(SysHandle thread)
{
	return 0;
}
#endif

uint32_t SysGetRefNum(void)
{
	return 0;
}

double _swap_double(double arg)
{
	return arg;
	/* To be done later... */
}

float _swap_float(float arg)
{
	return arg;
	/* To be done later... */
}

uint64_t _swap_int64(uint64_t uarg)
{
	uint64_t swapped;
	uint32_t const *s = (uint32_t const *)&uarg;
	uint32_t *d = (uint32_t *)&swapped;
	d[0] = _swap_int32(s[1]);
	d[1] = _swap_int32(s[0]);
	return swapped;
}

uint32_t _swap_int32(uint32_t uarg)
{
	// this is is actually the best way to do this. This should result in 4
	// instruction on ARM (8 in thumb), but since the compiler is being
	// stupid (it doesn't generate ROR), it doesn't.
	uint32_t swapped = uarg ^ ((uarg<<16)|(uarg>>16));
	swapped &= 0xFF00FFFF;
	uarg = (uarg>>8)|(uarg<<24);
	return (swapped >> 8) ^ uarg;
}



}

#if LIBBE_BOOTSTRAP
extern "C" void
palmsource_inc_package_ref()
{
}

extern "C" void
palmsource_dec_package_ref()
{
}
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

sysThreadDirectFuncs g_threadDirectFuncs = 
{
	sysThreadDirectFuncsCount,
	&SysAtomicInc32,
	&SysAtomicDec32,
	&SysAtomicAdd32,
	&SysAtomicAnd32,
	&SysAtomicOr32,
	&SysAtomicCompareAndSwap32,
 	&SysTSDGet,
 	&SysTSDSet,
 	&SysCriticalSectionEnter,
 	&SysCriticalSectionExit,
 	&SysConditionVariableWait,
 	&SysConditionVariableOpen,
 	&SysConditionVariableClose,
 	&SysConditionVariableBroadcast
};

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif // 
