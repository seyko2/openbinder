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

#ifndef	_SUPPORT_STRINGIO_H
#define	_SUPPORT_STRINGIO_H

/*!	@file support/StringIO.h
	@ingroup CoreSupportDataModel
	@brief Binder IO stream for creating C strings.
*/

#include <support/TextStream.h>
#include <support/ByteStream.h>
#include <support/MemoryStore.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

/*-------------------------------------------------------------------*/
/*------- BStringIO Class -------------------------------------------*/

class BStringIO : public BMallocStore, public BTextOutput
{
	public:

							BStringIO();
		virtual				~BStringIO();
		
		const char *		String();
		size_t				StringLength() const;
		void				Clear(off_t to);
		void				Reset();
		void				PrintAndReset(const sptr<ITextOutput>& io);
};

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_STRINGIO_H */
