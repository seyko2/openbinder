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
	@file DebugMgr.h

	@brief Low-level Debug Manager functions.  See ErrorMgr.h for common
	debug macros.
*/

#ifndef _DEBUGMGR_H_
#define _DEBUGMGR_H_

#include <PalmTypes.h>
#include <stdarg.h>

#if defined(__cplusplus)
extern "C" {
#endif

// Retrieve a stack crawl for the current thread.  The 'outAddresses' will
// be filled in with the address of each function on the stack, from the
// immediate caller down.  The number of available slots to return is in
// 'maxResults'.  Use 'ignoreDepth' to skip functions at the top of the
// stack.  Returns the number of functions actually found.
// NOTE: ONLY IMPLEMENTED FOR PALMSIM.
int32_t DbgWalkStack(int32_t ignoreDepth, int32_t maxResults, void** outAddresses);

// Given a function address, return the raw symbol name for this
// function.
// NOTE: ONLY IMPLEMENTED FOR PALMSIM.
status_t DbgLookupSymbol(const void* addr, int32_t maxLen, char* outSymbol, void** outAddr);

// Given a symbol name, return the corresponding unmangled name.
// NOTE: ONLY IMPLEMENTED FOR PALMSIM.
status_t DbgUnmangleSymbol(char* symbol, int32_t maxLen, char* outName);

// Clear out all malloc() profiling statistics that have been collected
// so far for the current process.
void DbgRestartMallocProfiling(void);
void DbgSetMallocProfiling(Boolean enabled, int32_t dumpPeriod, int32_t maxItems, int32_t stackDepth);

// Is the debugger connected?
Boolean DbgIsPresent(void);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif  //_DEBUGMGR_H_
