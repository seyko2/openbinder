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

#ifndef _XML2_XMLPARSER_H
#define _XML2_XMLPARSER_H

#include <xml/IXMLOStr.h>
#include <xml/Parser.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace support;
#endif

/**************************************************************************************/

class BXMLParser : public BXMLParseContext
{
	public:

								BXMLParser(const sptr<IXMLOStr>& stream);
		virtual					~BXMLParser();
	
		virtual status_t		OnDocumentBegin(uint32_t flags);
		virtual status_t		OnDocumentEnd();
		virtual status_t		OnDocumentFail();
	
		virtual status_t		OnStartTag(SString &name, SValue &attributes);
		virtual status_t		OnEndTag(SString &name);
		virtual status_t		OnTextData(const char *data, int32_t size);
		virtual status_t		OnCData(const char *data, int32_t size);
	
		virtual status_t		OnGeneralParsedEntityRef(SString &name);
		virtual status_t		OnGeneralParsedEntityRef(SString &name, SString &replacement);
	
	private:
	
		sptr<IXMLOStr> 			m_stream;
};

/**************************************************************************************/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::xml
#endif

#endif // _B_XML2_PARSER_H
