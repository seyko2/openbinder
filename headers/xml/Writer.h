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

#ifndef _XML2_B_WRITER_H
#define _XML2_B_WRITER_H

#include <support/Vector.h>
#include <support/IByteStream.h>
#include <xml/XMLOStr.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace support;
#endif

// Used for writing.  Should be private XXX
// =====================================================================
enum {
	stfLeaf		= 0x00000001,
	stfCanAddWS	= 0x00000002
};

// BOutputStream
// =====================================================================
class BOutputStream : public CXMLOStr {
public:
						BOutputStream(const sptr<IByteOutput>& stream, bool writeHeader=false);
	virtual				~BOutputStream();
	
	virtual status_t	StartTag(SString &name, SValue &attributes);
	virtual status_t	EndTag(SString &name);
	virtual status_t	TextData(const char	* data, int32_t size);
	virtual status_t	Comment(const char *data, int32_t size);

private:
	void		Indent();

	sptr<IByteOutput>	m_stream;
	int32_t				m_depth;
	int32_t				m_lastPrettyDepth;
	bool				m_isLeaf:1;
	bool				m_openStartTag:1;
	uint8_t				m_reserved:6;
};



// Consider this an inverse parser
class BWriter
{
public:
						BWriter(const sptr<IByteOutput>& data, uint32_t formattingStyle);
	virtual				~BWriter();
	
	// Stuff that goes only in DTDs
	//virtual status_t	BeginDoctype(const SString & elementName, const SString & systemID, const SString & publicID);
	//virtual status_t	EndDoctype();
	//virtual status_t	OnElementDecl(...);
	//virtual status_t	OnEntityDecl(...);
	
	// Stuff that goes only in the document part
	virtual status_t	StartTag(const SString &name, const SValue &attributes, uint32_t formattingHints=0);
	virtual status_t	EndTag(); // Will always do propper start-tag matching
	virtual status_t	TextData(const char	* data, int32_t size);
	virtual status_t	CData(const char	* data, int32_t size);
	virtual status_t	WriteEscaped(const char * data, int32_t size);
	
	// Stuff that can go anywhere
	virtual status_t	Comment(const char *data, int32_t size);
	virtual status_t	ProcessingInstruction(const SString & target, const SString & data);
	
	// Formatting Styles
	enum
	{
		BALANCE_WHITESPACE		= 0x00000001,
		SKIP_DOCTYPE_OPENER		= 0x00000002
	};
	
	// Formatting Hints
	enum
	{
		NO_EXTRA_WHITESPACE		= 0x80000002
	};
	
private:
	status_t	open_doctype();
	status_t	indent();
	
	sptr<IByteOutput>	m_stream;
	SVector<SString>	m_elementStack;
	uint32_t				m_formattingStyle;
	bool				m_openStartTag;
	bool				m_doneDOCTYPE;
	bool				m_startedInternalDTD;
	int32_t				m_depth;
	int32_t				m_lastPrettyDepth;
};

#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif

#endif // _XML2_B_WRITER_H
