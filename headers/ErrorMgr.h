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
	@file ErrorMgr.h

	@brief Error checking and reporting macros, whose behavior depends on BUILD_TYPE.
*/

#ifndef _ERRORMGR_H_
#define _ERRORMGR_H_

#include <PalmTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------
// You probably don't want to use this function unless you want show a different
//	file name and line number.
void ErrFatalErrorInContext(const char* fileName, uint32_t lineNum, const char* msg);

#ifdef __cplusplus
}
#endif

//---------------------------------------------------------------------------------------
// ErrFatalError and ErrFatalErrorIf will always compiled in.
#define ErrFatalError(msg)				ErrFatalErrorInContext(MODULE_NAME, __LINE__, msg)
#define ErrFatalErrorIf(condition, msg)	((condition) ? ErrFatalError(msg) : (void)0)

//---------------------------------------------------------------------------------------
// DbgOnlyFatalError and DbgOnlyFatalErrorIf will only be compiled in on Debug builds.
#if BUILD_TYPE != BUILD_TYPE_RELEASE
#	define DbgOnlyFatalError(msg)					ErrFatalError(msg)
#	define DbgOnlyFatalErrorIf(condition, msg)		ErrFatalErrorIf(condition, msg)
#else
#	define DbgOnlyFatalError(msg)					/* NOP */
#	define DbgOnlyFatalErrorIf(condition, msg)		/* NOP */
#endif

#endif // _ERRORMGR_H_
