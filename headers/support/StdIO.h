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

#ifndef	_SUPPORT_STDIO_H
#define	_SUPPORT_STDIO_H

/*!	@file support/StdIO.h
	@ingroup CoreSupportDataModel
	@brief Binder-based standard IO streams.
*/

#include <support/Value.h>
#include <support/IByteStream.h>
#include <support/ITextStream.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

/**************************************************************************************/
/*
This is defined here to be at the same location where btrace is defined.
Thus using traces does not require to include several headers.
Another approach could be to put this in TraceMgr.h and include this header here,
but you would need to add several path to the search dir (and reduce the build time).
*/
#if defined(TRACE_OUTPUT) && TRACE_OUTPUT == TRACE_OUTPUT_ON
#	define TO(x)	x
#else
#	define TO(x)
#endif

// The new function names for raw byte streams.
extern _IMPEXP_SUPPORT const sptr<IByteInput>& StandardByteInput(void);
extern _IMPEXP_SUPPORT const sptr<IByteOutput>& StandardByteOutput(void);
extern _IMPEXP_SUPPORT const sptr<IByteOutput>& StandardByteError(void);
extern _IMPEXP_SUPPORT const sptr<IByteInput>& NullByteInput(void);
extern _IMPEXP_SUPPORT const sptr<IByteOutput>& NullByteOutput(void);

#if TARGET_HOST != TARGET_HOST_PALMOS

// Raw byte streams for the standard C files.
extern _IMPEXP_SUPPORT const sptr<IByteInput> Stdin;
extern _IMPEXP_SUPPORT const sptr<IByteOutput> Stdout;
extern _IMPEXP_SUPPORT const sptr<IByteOutput> Stderr;

// Formatted text streams for C standard out, standard error,
// and the debug serial port.
extern _IMPEXP_SUPPORT sptr<ITextOutput> bout;
extern _IMPEXP_SUPPORT sptr<ITextOutput> berr;

#else 

extern _IMPEXP_SUPPORT sptr<ITextOutput>& _get_bout(void);
extern _IMPEXP_SUPPORT sptr<ITextOutput>& _get_berr(void);

#define bout _get_bout()
#define berr _get_berr()

#endif

/**************************************************************************************/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_STDIO_H */
