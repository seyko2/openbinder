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

#ifndef	_XML2_IXMLOSTR_H
#define	_XML2_IXMLOSTR_H

#include <support/IInterface.h>
#include <support/String.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace support;
#endif

/**************************************************************************************/

class IXMLOStr : public IInterface
{
	public:

		//the following could be B_DECLARE_META_INTERFACE(), but we've no RXMLOStr
		B_DECLARE_META_INTERFACE(XMLOStr)

		virtual status_t	StartTag(SString &name, SValue &attributes) = 0;
		virtual status_t	EndTag(SString &name) = 0;
		virtual status_t	Content(const char	*data, int32_t size) = 0;
		virtual status_t	Comment(const char *data, int32_t size);
};

/**************************************************************************************/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::xml
#endif

#endif /* _XML2_XMLOSTR_H */
