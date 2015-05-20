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

#include <xml/XMLParser.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif

/*********************************************************************/

BXMLParser::BXMLParser(const sptr<IXMLOStr>& stream) : m_stream(stream)
{
}

BXMLParser::~BXMLParser()
{
}

status_t 
BXMLParser::OnDocumentBegin(uint32_t)
{
	return B_OK;
}

status_t 
BXMLParser::OnDocumentEnd()
{
	return B_OK;
}

status_t 
BXMLParser::OnDocumentFail()
{
	return B_OK;
}

status_t 
BXMLParser::OnStartTag(SString &name, SValue &attributes)
{
	return m_stream->StartTag(name,attributes);
}

status_t 
BXMLParser::OnEndTag(SString &name)
{
	return m_stream->EndTag(name);
}

status_t 
BXMLParser::OnTextData(const char *data, int32_t size)
{
	return m_stream->Content(data,size);
}

status_t 
BXMLParser::OnCData(const char *data, int32_t size)
{
	return m_stream->Content(data,size);
}

status_t 
BXMLParser::OnGeneralParsedEntityRef(SString &name)
{
	char c;
	if (name == "quot") c = '"';
	else if (name == "amp")  c = '&';
	else if (name == "lt")   c = '<';
	else if (name == "gt")   c = '>';
	else if (name == "apos") c = '\'';
	else return B_XML_ENTITY_NOT_FOUND;

	return OnTextData(&c,1);
}

status_t 
BXMLParser::OnGeneralParsedEntityRef(SString &name, SString &replacement)
{
	char c;
	if (name == "quot") c = '"';
	else if (name == "amp")  c = '&';
	else if (name == "lt")   c = '<';
	else if (name == "gt")   c = '>';
	else if (name == "apos") c = '\'';
	else return B_XML_ENTITY_NOT_FOUND;
	
	replacement.SetTo(c,1);
	
	return B_OK;
}

#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif
