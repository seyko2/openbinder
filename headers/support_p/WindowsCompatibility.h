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

#ifndef _WINDOWS_COMPATIBILITY_H
#define _WINDOWS_COMPATIBILITY_H

#include <BuildDefines.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

#if TARGET_HOST == TARGET_HOST_WIN32

uint32_t strnlen(const char *str, int32_t maxLength);

#define atoll _atoi64

#ifdef __cplusplus
extern "C" {
#endif

void ErrFatalErrorInContext(const char* fileName, uint32_t lineNum, const char* errMsg);
void init_thread_message_queue();
const char* strerror_r(int errnumber, char *buf, size_t buflen);

size_t strlcpy(char * dst, char const * src, size_t size);

int rand_r(unsigned int * seed);

#ifndef snprintf
#define snprintf _snprintf
#endif

#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif

// ########################################################################################## //
// # NOTE THAT THE FOLLOWING FILES ARE SAFE TO INCLUDE!										# //
// ########################################################################################## //

#if !defined(_68K_) && !defined(_MPPC_) && !defined(_X86_) && !defined(_IA64_) && !defined(_AMD64_) && defined(_M_IX86)
#define _X86_
#endif

// THIS MAY CAUSE A PROBLEM!
#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include <winver.h>
#include <winnt.h>
#include <dbghelp.h>
#include <tlhelp32.h>

// DispatchMessage is defined to be DispatchMessageA which reaplaces BHandlers DispatchMessage!
#undef DispatchMessage
// This interferes with SLocker::Yield
#undef Yield


#ifdef __cplusplus
}

#endif

#define ErrFatalOption_IgnoreAllowed	1

#define DbgOnlyFatalError(errMsg) ErrFatalErrorInContext(MODULE_NAME, __LINE__, errMsg)
#define DbgOnlyFatalErrorIf(condition, errMsg) ((condition) ? DbgOnlyFatalError(errMsg) : (void)0)


#endif // TARGET_HOST == TARGET_HOST_WIN32

#endif // _WINDOWS_COMPATIBILITY_H
