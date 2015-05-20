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

#ifndef _B_XML2_PARSER_H
#define _B_XML2_PARSER_H

#include <support/Atom.h>
#include <support/IByteStream.h>
#include <support/Binder.h>
#include <support/Vector.h>

#include <xml/IXMLOStr.h>
#include <xml/XMLDefs.h>
#include <xml/DataSource.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace support;
#endif

// Forward References in namespace palmos::xml
// =====================================================================
class BXMLParseContext;
class BParser;
class BCreator;


// BXMLParseContext -- Hook Functions for what the parser encounters
// =====================================================================
class BXMLParseContext
{
public:
				BXMLParseContext();
	virtual		~BXMLParseContext();
	
	// These fields are always set to the current line and column (or as
	// close of an approximation as possible).  They are probably most
	// useful for error messages.
	int32_t line;
	int32_t column;
	
						// Called at the beginning of the document parsing process.
	virtual status_t	OnDocumentBegin(uint32_t flags);
	
						// Called at the end of the document parsing process.
						// If you get this call, then you know that parsing was successful.
	virtual status_t	OnDocumentEnd();
						// Called at the end of the document parsing process.
						// You get this call if you don't get OnDocumentEnd().
	virtual status_t	OnDocumentFail();
	
	// The following functions are fairly self explanitory.
	// Whenever there's a SString that isn't const, you are allowed to use the
	// SString::Adopt function to take the string buffer, and leave the original
	// string empty.  This is just a performance optimization.
	
	virtual status_t	OnStartTag(				SString		& name,
												SValue		& attributes		);
									
	virtual status_t	OnEndTag(				SString		& name				);
	
	virtual status_t	OnTextData(				const char	* data,
												int32_t		size				);
	
	virtual status_t	OnCData(				const char	* data,
												int32_t		size				);
	
	virtual status_t	OnComment(				const char	* data,
												int32_t		size				);
	
	virtual status_t	OnDocumentTypeBegin(	SString		& name				);
	
	virtual status_t	OnExternalSubset(		SString		& publicID,
												SString		& systemID,
												uint32_t 		parseFlags			);
	
	virtual status_t	OnInternalSubsetBegin();
	
	virtual status_t	OnInternalSubsetEnd();
	
	virtual status_t	OnDocumentTypeEnd();
	
	virtual status_t	OnProcessingInstruction(SString		& target,
												SString		& data				);
	
	virtual	status_t	OnElementDecl(			SString		& name,
												SString		& data				);
	
	virtual status_t	OnAttributeDecl(		SString		& element,
												SString		& name,
												SString		& type,
												SString		& behavior,
												SString		& defaultVal		);
	
	virtual status_t	OnInternalParsedEntityDecl(	SString	& name,
													SString & internalData,
													bool	parameter,
													uint32_t	parseFlags			);
	
	virtual status_t	OnExternalParsedEntityDecl(	SString	& name,
													SString & publicID,
													SString & systemID,
													bool	 parameter,
													uint32_t	parseFlags			);
	
	virtual status_t	OnUnparsedEntityDecl(		SString	& name,
													SString & publicID,
													SString & systemID,
													SString & notation			);
	
	virtual status_t	OnNotationDecl(				SString	& name,
													SString	& value				);
	
						// This is a hook function to notify the subclass that we
						// encountered a PE in a text section.  Subclasses might
						// either look up replacement text and insert it, or look
						// parsed objects and insert them.
	virtual status_t	OnGeneralParsedEntityRef(	SString	& name				);
	
						// This is a hook function to find out the replacement text
						// for a general entity when it occurs in an attribute.  The
						// value is then substituted into the attribute as if it
						// had never been there.  If you want this behavior, you must
						// set the B_XML_HANDLE_ATTRIBUTE_ENTITIES flag.
	virtual status_t	OnGeneralParsedEntityRef(	SString	& name,
													SString & replacement		);
	
						// This is a hook function to notify the subclass when an 
						// entity occurred in the DTD, but in a place where it would
						// be better for the subclass to just insert its objects into
						// the stream than to send back the replacement text, and 
						// have this parser have to reparse it each time it occurs.
	virtual status_t	OnParameterEntityRef(		SString	& name				);
	
						// This is a hook function to find the replacement text for
						// a parameter entity.  It will then be parsed, and the normal
						// other functions will be called.
	virtual status_t	OnParameterEntityRef(		SString	& name,
													SString & replacement		);

						// An error occurred.  If you would like to ignore the error,
						// and continue processing, then return B_OK from this
						// function, and processing will continue.
						// If fatal is true, a fatal error occurred, and we're not
						// going to be able to recover, no matter what you return.
						// debugLineNo is the line number from which OnError was called
						// XXX Should debugLineNo be made public?
						// The code and data parameters are currently unused.  I have
						// visions of using them to help recover from errors 
						// (for example, pass in potentially corrupted structures, and
						// allow the OnError function to have a go at correcting them)
	virtual status_t	OnError(status_t error, bool fatal, int32_t debugLineNo,
									uint32_t code = 0, void * data = NULL);

};


class BCreator : public virtual SAtom
{
public:
						BCreator();	
	
	virtual 			~BCreator();
	
	virtual status_t	OnStartTag(				SString			& name,
												SValue			& attributes,
												sptr<BCreator>	& newCreator	);
									
	virtual status_t	OnEndTag(				SString			& name			);
	
	virtual status_t	OnText(					SString			& data			);
	
	virtual status_t	OnComment(				SString			& name			);
	
	virtual status_t	OnProcessingInstruction(SString			& target,
												SString			& data			);
												
	virtual status_t	Done();
};


// ParseXML -- Do some parsing.
// =====================================================================
status_t	_IMPEXP_SUPPORT ParseXML(BXMLParseContext *context, BXMLDataSource *data, uint32_t flags = 0);
status_t	_IMPEXP_SUPPORT ParseXML(const sptr<BCreator>& creator, BXMLDataSource *data, uint32_t flags = 0);


#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif

#endif // _B_XML2_PARSER_H
