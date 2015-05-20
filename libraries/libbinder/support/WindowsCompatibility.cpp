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

#include <support_p/WindowsCompatibility.h>

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/time.h>

#include <ErrorMgr.h>
#include <LocaleMgr.h>
#include <TextMgr.h>
#include <TextMgrPrv.h>
#include <SysThreadConcealed.h>
#define _PACKED
#include <SystemPDK.h>

#if TARGET_HOST == TARGET_HOST_WIN32

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

status_t	SysThreadDelay(nsecs_t timeout, timeoutFlags_t flags)
{
	return 0;
}

uint32_t strnlen(const char *str, int32_t maxLength)
{
	uint32_t result = strlen(str);
	if(result > maxLength)
	{
		result = maxLength;
	}
	return result;
}

CharEncodingType
LmGetSystemLocale(	LmLocaleType*	oSystemLocale)
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

BOOL WINAPI DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	printf("DllMain: reason: %d\n", ul_reason_for_call);

	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
	}
    return TRUE;
}

void ErrFatalErrorInContext(const char* fileName, uint32_t lineNum, const char* errMsg)
{
	printf("%s:%lu %s", fileName, lineNum, errMsg);
	printf("Debugger: %s\n", errMsg);
	*(int32_t*)NULL = 5038;
}

// <SysThread.h>

SysHandle SysCurrentThread(void)
{
	return (SysHandle)GetCurrentThreadId();
}

const char* SysProcessName()
{
	return "[noname]";
}

uint32_t SysProcessID()
{
	return 0;
}

Boolean SysIsInstrumentationAllowed(void)
{
	return TRUE;
}

status_t SysTSDAllocate(SysTSDSlotID *oTSDSlot, SysTSDDestructorFunc *iDestructor, uint32_t iName)
{
	if (iDestructor != NULL || iName != sysTSDAnonymous)
		ErrFatalError("Destructor functions and names not supported.");
	*oTSDSlot = (SysTSDSlotID)TlsAlloc();
	return errNone;
}

status_t SysTSDFree(SysTSDSlotID tsdslot)
{
	TlsFree(tsdslot);
	return errNone;
}

void* SysTSDGet(SysTSDSlotID tsdslot)
{
	return (void*)TlsGetValue(tsdslot);
}

void SysTSDSet(SysTSDSlotID tsdslot, void *iValue)
{
	TlsSetValue(tsdslot, iValue);
}

// System stuff

void init_thread_message_queue()
{
	MSG msg;

	// create a message queue for the thread.
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
}

const char* strerror_r(int errnumber, char *buf, size_t buflen)
{
	if(buf && (buflen > strlen(strerror(errnumber))))
	{
		strncpy(buf, strerror(errnumber), buflen);
	}

	return buf;
}

status_t KALCurrentThreadDelay(nsecs_t timeout, timeoutFlags_t flags)
{
	if ((flags == B_WAIT_FOREVER) ||  (timeout == B_INFINITE_TIMEOUT))
	{
		timeout = INFINITE;
		Sleep((DWORD)timeout);
	}
	else
	{
		Sleep((DWORD)(timeout / B_ONE_MILLISECOND));
	}

	return B_OK;
}


size_t
strlcpy(char * dst, char const * src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	if (!dst || !src || !siz) {
		return 0;
	}

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}


int
rand_r(unsigned int *seed)
{
	return ((*seed = *seed * 1103515245 + 12345) & RAND_MAX);
}





// <kernel/image.h>
image_id load_add_on(const char *path)
{
	image_id imid = (image_id)LoadLibrary(path);
	return imid != 0 ? imid : B_ERROR;
}

status_t unload_add_on(image_id imid)
{
	if (imid >= 0)
		return FreeLibrary((HMODULE)imid) ? B_OK : B_ERROR;
	return imid;
}

status_t get_image_symbol(image_id imid, const char *name, int32_t sclass, void **ptr)
{
	*ptr = (void*)GetProcAddress((HMODULE)imid, name);
	return (*ptr == NULL) ? B_ERROR : B_OK;
}

status_t get_next_image_info(team_id team, int32_t *cookie, image_info *info)
{
	if (!cookie || !info)
		return B_BAD_VALUE;

	MODULEENTRY32 me;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());

	bool moreModules = true;
	bool found = false;

	int index = *cookie;

	if (index == 0)
	{
		moreModules = Module32First(snapshot, &me);
		info->id = (image_id)me.hModule;
		(*cookie)++;
		CloseHandle(snapshot);
		return B_OK;
	}

	while (moreModules && index > 0)
	{
		moreModules = Module32Next(snapshot, &me);
		index--;
	}

	CloseHandle(snapshot);

	if (index != 0)
		return B_BAD_VALUE;

	info->id = (image_id)me.hModule;
	(*cookie)++;

	return B_OK;
}

status_t get_nth_image_symbol(image_id imid, int32_t index, char *buf, int32_t *bufsize, int32_t *sclass, void **ptr)
{
	return B_ERROR;
}

// <support/atomic.h>
int32_t __declspec(naked) SysAtomicInc32(volatile int32_t* ioOperandP)
{
__asm {
	mov    edx, DWORD PTR [esp+4] // ioOperand
retry:
	mov    ecx, 1
	mov    eax, DWORD PTR [edx]
	add    ecx, eax

	lock cmpxchg DWORD PTR [edx], ecx
	jnz    retry

	ret
	}
}

int32_t __declspec(naked) SysAtomicDec32(volatile int32_t* ioOperandP)

{
__asm {
	mov    edx, DWORD PTR [esp+4] // ioOperand
retry:
	mov    ecx, -1
	mov    eax, DWORD PTR [edx]
	add    ecx, eax

	lock cmpxchg DWORD PTR [edx], ecx
	jnz    retry

	ret
	}
}

int32_t atomic_add(volatile int32_t *value, int32_t addvalue)
{
	__asm
	{
		mov ebx, value					/* load address of variable into ebx */
	atomic_add1:
		mov ecx, addvalue				/* load the count to increment by */
		mov eax, [ebx]					/* load the value of variable into eax */
		add ecx, eax					/* do the addition, result in ecx */

		lock cmpxchg [ebx], ecx
		jnz atomic_add1					/* if zf = 0, cmpxchng failed so redo it */
	}
}

int32_t atomic_and(volatile int32_t *value, int32_t andvalue)
{
	__asm
	{
		mov ebx, value					/* load address of variable into ebx */
	atomic_and1:
		mov ecx, andvalue				/* load the count to increment by */
		mov eax, [ebx]					/* load the value of variable into eax */
		and ecx, eax					/* do the AND, result in ecx */

		lock cmpxchg [ebx], ecx
		jnz atomic_and1					/* if zf = 0, cmpxchng failed so redo it */
	}
}

int32_t atomic_or(volatile int32_t *value, int32_t orvalue)
{
	__asm
	{
		mov ebx, value					/* load address of variable into ebx */
	atomic_or1:
		mov ecx, orvalue				/* load the count to increment by */
		mov eax, [ebx]					/* load the value of variable into eax */
		or ecx, eax						/* do the OR, result in ecx */

		lock cmpxchg [ebx], ecx
		jnz atomic_or1					/* if zf = 0, cmpxchng failed so redo it */
	}
}

int32_t compare_and_swap32(volatile int32_t *location, int32_t oldValue, int32_t newValue)
{
	__asm
	{
		mov	edx, location
		mov	eax, oldValue
		mov ecx, newValue
		lock cmpxchg [edx], ecx
		sete al
		and eax, 1
	}
}

__declspec(naked)
int32_t compare_and_swap64(volatile int64_t *location, int64_t oldValue, int64_t newValue)
{
	__asm
	{
		mov	edi, DWORD PTR [esp+4]
		mov	eax, DWORD PTR [esp+8]
		mov edx, DWORD PTR [esp+12]
		mov ebx, DWORD PTR [esp+16]
		mov ecx, DWORD PTR [esp+20]
		lock	cmpxchg8b	[edi]
		sete cl
		xor eax, eax
		mov al, cl
		ret
	}
}

status_t
SysSemaphoreCreate(uint32_t initialCount, uint32_t maxCount, uint32_t flags, SysHandle* outSemaphore)
{
	SysHandle semID;

	// Note that we don't pass the name through here, because on
	// Windows it is used to look up existing objects!!
	semID = (SysHandle)CreateSemaphore(NULL, initialCount, maxCount, NULL);
	if (!semID) {
		return B_ERROR;
	}

	*outSemaphore = semID;
	return B_OK;
}


status_t SysSemaphoreWaitCount(SysHandle sem, timeoutFlags_t iTimeoutFlags, nsecs_t iTimeout, uint32_t count)
{
	if (count < 1)
		return B_BAD_VALUE;

	for (int32_t i = 0 ; i < count ; i++)
	{
		int32_t result;

		do
		{
			if (iTimeoutFlags == B_WAIT_FOREVER) {
				result = WaitForSingleObject((HANDLE)sem, INFINITE);
			} else {
				if (iTimeoutFlags == B_POLL) {
					iTimeout = 0;
				}
				result = WaitForSingleObject((HANDLE)sem, (DWORD)(iTimeout / B_ONE_MILLISECOND));
				if ((result == WAIT_TIMEOUT) && (iTimeout == 0))
					return B_WOULD_BLOCK;
				else if (result == WAIT_TIMEOUT)
					return B_TIMED_OUT;
			}
			if (result == WAIT_FAILED)
				return B_BAD_VALUE;

		} while (result != WAIT_OBJECT_0);
	}

	return B_OK;
}

status_t SysSemaphoreWait(SysHandle semaphore, timeoutFlags_t iTimeoutFlags, nsecs_t iTimeout)
{
	return SysSemaphoreWaitCount(semaphore, iTimeoutFlags, iTimeout, 1);
}

status_t SysSemaphoreCreateEZ(uint32_t initialCount, SysHandle* outSemaphore)
{
	sem_id result = (sem_id)CreateSemaphore(NULL, initialCount, 100000000, NULL);
	*outSemaphore = result;
	return errNone;
}

status_t SysSemaphoreDestroy(SysHandle semaphore)
{
	return (CloseHandle((HANDLE)semaphore) ? B_OK : B_ERROR);
}


status_t SysThreadInstallExitCallback(SysThreadExitCallbackFunc * iExitCallbackP, void * iCallbackArg, SysThreadExitCallbackID * oThreadExitCallbackId)
{
	return 0;
}

status_t SysSemaphoreSignal(SysHandle semaphore) 
{
	return (ReleaseSemaphore((HANDLE)semaphore, 1, NULL) ? B_OK : B_ERROR); 
}

status_t
SysSemaphoreSignalCount(SysHandle sem, uint32_t count)
{
	if (count < 1)
		return B_BAD_VALUE;

	return (ReleaseSemaphore((HANDLE)sem, count, NULL) ? B_OK : B_ERROR);
}

nsecs_t SysGetRunTime()
{
	const DWORD ticks = GetTickCount();
	const nsecs_t time = ((nsecs_t)(unsigned long)GetTickCount()) * 1000 * 1000;
	if (time < 0) {
		printf("Bad time!  ticks=%d, time=%I64d\n", ticks, time);
		ErrFatalError("blam");
	}
	return time;
}

// <driver/binder2_driver.h>

nsecs_t	system_real_time (void)
{
	ErrFatalError("This function has not been implemented for Windows.");
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


int fsync(int fd)
{
	return 0;
	/* To be done later... */
}

ssize_t readv(int fd, const struct iovec *vector, size_t count)
{
	ssize_t size = 0;

	while (count-- > 0)
	{
		ssize_t bytes = read(fd, vector->iov_base, vector->iov_len);

		if (bytes == 0)
			break;

		if (bytes == -1)
		{
			if (size == 0) size = bytes;
			break;
		}

		size += bytes;
		vector++;
	}

	return size;
}

ssize_t writev(int fd, const struct iovec *vector, size_t count)
{
	ssize_t size = 0;

	while (count-- > 0)
	{
		ssize_t bytes = write(fd, vector->iov_base, vector->iov_len);

		if (bytes == 0)
			break;

		if (bytes == -1)
		{
			if (size == 0) size = bytes;
			break;
		}

		size += bytes;
		vector++;
	}

	return size;
}

/*
 * Convert a string to a long long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
/* LONGLONG */
int64_t
strtoll(const char* nptr, char** endptr, int base)
{
	const char *s;
	/* LONGLONG */
	int64_t acc, cutoff;
	int c;
	int neg, any, cutlim;

	/* endptr may be NULL */

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	s = nptr;
	do {
		c = (unsigned char) *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else {
		neg = 0;
		if (c == '+')
			c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for long longs is
	 * [-9223372036854775808..9223372036854775807] and the input base
	 * is 10, cutoff will be set to 922337203685477580 and cutlim to
	 * either 7 (neg==0) or 8 (neg==1), meaning that if we have
	 * accumulated a value > 922337203685477580, or equal but the
	 * next digit is > 7 (or 8), the number is too big, and we will
	 * return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = neg ? LLONG_MIN : LLONG_MAX;
	cutlim = (int)(cutoff % base);
	cutoff /= base;
	if (neg) {
		if (cutlim > 0) {
			cutlim -= base;
			cutoff += 1;
		}
		cutlim = -cutlim;
	}
	for (acc = 0, any = 0;; c = (unsigned char) *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0)
			continue;
		if (neg) {
			if (acc < cutoff || (acc == cutoff && c > cutlim)) {
				any = -1;
				acc = LLONG_MIN;
				errno = ERANGE;
			} else {
				any = 1;
				acc *= base;
				acc -= c;
			}
		} else {
			if (acc > cutoff || (acc == cutoff && c > cutlim)) {
				any = -1;
				acc = LLONG_MAX;
				errno = ERANGE;
			} else {
				any = 1;
				acc *= base;
				acc += c;
			}
		}
	}
	if (endptr != 0)
		/* LINTED interface specification */
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
}

extern "C" void
palmsource_inc_package_ref()
{
}

extern "C" void
palmsource_dec_package_ref()
{
}

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
 	0, // &SysCriticalSectionEnter,
 	0, // &SysCriticalSectionExit,
 	0, // &SysConditionVariableWait,
 	0, // &SysConditionVariableOpen,
 	0, // &SysConditionVariableClose,
 	0, // &SysConditionVariableBroadcast
};



#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif



#endif // #if TARGET_HOST == TARGET_HOST_WIN32
