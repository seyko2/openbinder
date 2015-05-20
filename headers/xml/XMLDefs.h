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

#ifndef _B_XML2_DEFS_H
#define _B_XML2_DEFS_H

#include <support/Errors.h>
#include <support/TypeConstants.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace support;
#endif

// If this is defined to 1, a lot of the conveninece methods are disabled
// so it has a smaller memory footprint for BeIA
// Don't just go and turn this on.  It adds tons of code.
#define _SMALL_XML_FOOTPRINT_ 1


// XML Type Code
// =====================================================================
enum
{
	B_XML_DOCUMENT_CONTENT						= 0x00000001,
	B_XML_DOCUMENT_TYPE_CONTENT					= 0x00000002,
	B_XML_ELEMENT_CONTENT						= 0x00000004,
	B_XML_ATTRIBUTE_CONTENT						= 0x00000008,
	B_XML_TEXT_CONTENT							= 0x00000010,
	B_XML_CDATA_CONTENT							= 0x00000020,
	B_XML_PROCESSING_INSTRUCTION_CONTENT		= 0x00000040,
	B_XML_COMMENT_CONTENT						= 0x00000080,
	B_XML_ATTRIBUTE_DECL_OBJECT					= 0x00000100,
	B_XML_ELEMENT_DECL_OBJECT					= 0x00000200,
	B_XML_ENTITY_DECL_OBJECT					= 0x00000400,
	B_XML_DTD_OBJECT							= 0x00000800,
	
	B_XML_ANY_CONTENT							= 0xFFFFFFFF,
	
	B_XML_BEOS_CONTENT_TYPE_MASK	= 0x0000FFFF	// Anything that masks with this
													// is reserved for use by Be. Other
													// people can use the rest for their
													// own purposes
};


#define B_XML_DTD_DIRECTORY "xml_dtds"


// XML Kit Errors
// =====================================================================
// These should go into Errors.h
// XXX NEED TO DEFINE XML ERROR CLASS
#define B_XML_ERROR_BASE			(xmlErrorClass+1)
enum {
	B_XML_ALREADY_ATTACHED			= B_XML_ERROR_BASE,
	B_XML_NOT_ATTACHED,
	B_XML_CANT_CONVERT_STRING,
	B_XML_ONE_ELEMENT_PER_DOCUMENT,
	B_XML_ONE_DOCTYPE_PER_DOCUMENT,
	B_XML_BAD_PARENT,
	B_XML_MATCH_NOT_FOUND,
	B_XML_INVALID_XPATH,
	B_XML_INVALID_XSLT_DOCUMENT,
	B_XML_BAD_ELEMENT_NESTING,
	
	B_XML_MAX_TOKEN_LENGTH_EXCEEDED,			// Names, etc. can only be 256 chars long
												// because you need to set some limit
	B_XML_BAD_NAME_CHARACTER,					// Names for elements, etc.
	B_XML_PARSE_ERROR,							// General Parse Error -- Something in the wrong place
	B_XML_ENTITY_VALUE_NO_QUOTES,				// Entity values / IDs must be surrounded by quotation marks
	B_XML_RECURSIVE_ENTITY,
	B_XML_NO_UNPARSED_PARAMETER_ENTITIES,
	B_XML_ENTITY_NOT_FOUND,
	B_XML_DECLARATION_NOT_IN_DTD,
	B_XML_NO_DTD,
	B_XML_ILLEGAL_UNPARSED_ENTITY_REF,
	B_XML_INVALID_ATTR_TYPE,
	B_XML_BAD_ATTR_BEHAVIOR,
	B_XML_AMBIGUOS_CHILDREN_PATTERN,
	B_XML_NO_EMPTY_NAMES,
	B_XML_ATTR_NAME_IN_USE,
	
	B_XML_NAMESPACE_NOT_DECLARED,
	B_XML_BAD_NAMESPACE_PREFIX,
	B_XML_NAMESPACE_PREFIX_COLLISION,
	B_XML_ATTRS_WITH_COLLIDING_NAMESPACES,
	B_XML_ALREADY_DECLARED,
	
	// Validation Errors
	B_XML_ELEMENT_NOT_DECLARED,
	B_XML_ATTRIBUTE_NOT_DECLARED,
	B_XML_ATTRIBUTE_BAD_FIXED_VALUE,
	B_XML_REQUIRED_ATTRIBUTE_MISSING,
	B_XML_ENTITY_REF_NOT_UNPARSED,
	B_XML_BAD_ENUM_VALUE,
	B_XML_ELEMENT_NOT_EMPTY,
	B_XML_CHILD_ELEMENT_NOT_ALLOWED,
	B_XML_CHILD_TEXT_NOT_ALLOWED,
	B_XML_CHILD_CDATA_NOT_ALLOWED,
	B_XML_CHILD_PATTERN_NOT_FINISHED,
	
	// These are only warnings.  The default OnParseError fails on them.
	// Maybe this behavior should be changed.
	B_XML_WARNINGS_BASE,
	
	B_XML_ENTITY_ALREADY_EXISTS,				// The first one is the one that's used.
	
	B_XML_ERRORS_END
};

enum xml_attr_behavior
{
	B_XML_ATTRIBUTE_REQUIRED,
	B_XML_ATTRIBUTE_IMPLIED,
	B_XML_ATTRIBUTE_FIXED,
	B_XML_ATTRIBUTE_OPTIONAL,
	B_XML_ATTRIBUTE_DEFAULT
};

enum
{
	B_ENUM_TYPE			= 'ENUM',
	B_NOTATION_TYPE		= 'NOTN',		// Not supported yet
	B_ID_TYPE			= 'IDYO',
	B_IDREF_TYPE		= 'IDRF',
	B_IDREFS_TYPE		= 'IRFS',
	B_ENTITY_TYPE		= 'ENTI',
	B_ENTITIES_TYPE		= 'ENTS',
	B_NMTOKEN_TYPE		= 'NTKN',
	B_NMTOKENS_TYPE		= 'NTKS'
};


// Parsing Flags
// =====================================================================
enum
{
	B_XML_COALESCE_WHITESPACE			= 0x00000001,
	B_XML_HANDLE_ATTRIBUTE_ENTITIES		= 0x00000002,
	B_XML_HANDLE_CONTENT_ENTITIES		= 0x00000004,
	B_XML_HANDLE_ALL_ENTITIES			= 0x00000006,
	B_XML_HANDLE_NAMESPACES				= 0x00000008,
	B_XML_GET_NAMESPACE_DTDS			= 0x00000010,
	B_XML_DONT_EXPAND_CHARREFS			= 0x00000020
};


#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif

#endif // _B_XML2_DEFS_H
