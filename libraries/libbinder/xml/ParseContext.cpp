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

#include <xml/Parser.h>
#include <stdio.h>
#include <string.h>


#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif

// =====================================================================
BXMLParseContext::BXMLParseContext()
	:line(0),
	 column(0)
{
	
}


// =====================================================================
BXMLParseContext::~BXMLParseContext()
{
	
}


// =====================================================================
status_t
BXMLParseContext::OnDocumentBegin(uint32_t flags)
{
	(void) flags;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnDocumentEnd()
{
	return B_NO_ERROR;
}

// =====================================================================
status_t
BXMLParseContext::OnDocumentFail()
{
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnStartTag(			SString		& name,
										SValue		& attributes		)
{
	(void) name;
	(void) attributes;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnEndTag(				SString		& name				)
{
	(void) name;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnTextData(			const char	* data,
										int32_t		size				)
{
	(void) data;
	(void) size;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnCData(				const char	* data,
										int32_t		size				)
{
	(void) data;
	(void) size;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnComment(			const char	* data,
										int32_t		size				)
{
	(void) data;
	(void) size;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnDocumentTypeBegin(	SString		& name				)
{
	(void) name;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnExternalSubset(		SString		& publicID,
										SString		& systemID,
										uint32_t 		parseFlags			)
{
	(void) publicID;
	(void) systemID;
	(void) parseFlags;
	return B_NO_ERROR;
}


	
// =====================================================================
status_t
BXMLParseContext::OnInternalSubsetBegin()
{
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnInternalSubsetEnd()
{
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnDocumentTypeEnd()
{
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnProcessingInstruction(	SString		& target,
											SString		& data			)
{
	(void) target;
	(void) data;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnElementDecl(			SString		& name,
											SString		& data			)
{
	(void) name;
	(void) data;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnAttributeDecl(		SString		& element,
										SString		& name,
										SString		& type,
										SString		& behavior,
										SString		& defaultVal		)
{
	(void) element;
	(void) name;
	(void) type;
	(void) behavior;
	(void) defaultVal;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnInternalParsedEntityDecl(	SString	& name,
												SString & internalData,
												bool	parameter,
												uint32_t	parseFlags		)
{
	(void) name;
	(void) internalData;
	(void) parameter;
	(void) parseFlags;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnExternalParsedEntityDecl(	SString	& name,
												SString & publicID,
												SString & systemID,
												bool	 parameter,
												uint32_t	parseFlags		)
{
	(void) name;
	(void) publicID;
	(void) systemID;
	(void) parameter;
	(void) parseFlags;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnUnparsedEntityDecl(		SString	& name,
											SString & publicID,
											SString & systemID,
											SString & notation			)
{
	(void) name;
	(void) publicID;
	(void) systemID;
	(void) notation;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnNotationDecl(			SString	& name,
											SString	& value				)
{
	(void) name;
	(void) value;
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnGeneralParsedEntityRef(	SString	& name				)
{
	char c;
	if (name == "quot") c = '"';
	else if (name == "amp")  c = '&';
	else if (name == "lt")   c = '<';
	else if (name == "gt")   c = '>';
	else if (name == "apos") c = '\'';
	else {
		// If this doesn't get overridden, then we pretend that we just
		// got some text instead of trying to worry about it.
		SString replacement("&");
		replacement += name;
		replacement += ';';
		return OnTextData(replacement.String(), replacement.Length());
	}
	return OnTextData(&c,1);
}


// =====================================================================
status_t
BXMLParseContext::OnGeneralParsedEntityRef(	SString	& name,
											SString & replacement		)
{
	char c;
	if (name == "quot") c = '"';
	else if (name == "amp")  c = '&';
	else if (name == "lt")   c = '<';
	else if (name == "gt")   c = '>';
	else if (name == "apos") c = '\'';
	else {
		replacement = "&";
		replacement += name;
		replacement += ';';
		return B_NO_ERROR;
	}
	replacement.SetTo(c, 1);
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnParameterEntityRef(		SString	& name				)
{
	(void) name;
	
	// Don't mimic the behavior of other entity refs because there can't
	// be Text nodes in any place where a Parameter Entity Reference can happen
	return B_NO_ERROR;
}


// =====================================================================
status_t
BXMLParseContext::OnParameterEntityRef(		SString	& name,
											SString & replacement		)
{
	(void) replacement;
	
	replacement = "%";
	replacement += name;
	replacement += ';';
	return B_NO_ERROR;
}

// debugLineNo is the line number that the OnError call came from
// =====================================================================
status_t
BXMLParseContext::OnError(status_t err, bool fatal, int32_t debugLineNo,
							uint32_t code, void * data)
{
	(void) fatal;
	(void) debugLineNo;
	(void) code;
	(void) data;
	
	return err;
}



#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif
