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

/*

I'm sorry this function is so big.  There's really no better way.
You could also look at it as an efficiency gain.

I wrote it basically by staring at the XML spec for a while, and
then coding it.  If you ever have to maintain this code, make sure
you understand the intricacies of the productions in the spec. Most
of it's relatively easy, except for the Entity stuff.

Entities are either PARSED or UNPARSED, either INTERNAL or EXTERNAL,
and either PARAMETER or GENERAL.

- All entities are defined in DTDs
- GENERAL Entities are NEVER referenced in DTDs, except in attribute
  default values, where they are included as literal entities,
  and their values are not expanded.
        <!ENTITY Moo "This is the value">
- PARAMETER Entities are referenced only in DTDs. Put in a % to
  signify parameter entities:
        <!ENTITY % Moo "This is the value">
- INTERNAL Entities have the replacement text given:
        <!ENTITY Moo "This is the value">
- EXTERNAL Entities have the replacement text looked up somewhere else:
        <!ENTITY Moo SYSTEM "myfile.entity">
        <!ENTITY Moo PUBLIC "//formal-id-blah/" "myfile.entity">
- PARSED Entities have their text parsed by the processor, and their
  data goes directly into the document.  The data is XML.
- UNPARSED Entities do not have their text parsed by the processor.
  There data is not included in the document.  The data may or may
  not be XML (there is an errata in the XML Spec about this).  It
  may be presented along with the document (the sort of common 
  example of this is an image), but it is not actually part of the
  XML.

-joe

*/


#include <xml_p/XMLParserCore.h>
#include <stdlib.h>
#include <support/StdIO.h>

#ifdef _DESKTOP
	#ifdef _DEBUG
	#undef THIS_FILE
	static char THIS_FILE[]=__FILE__;
	#define new DEBUG_NEW
	#endif // _DEBUG
#endif // _DESKTOP

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {	
#endif

// Forward References


// From XML-2.3
#define IS_WHITESPACE(x) ((x)==0x20||(x)==0x09||(x)==0x0d||(x)==0x0a)


// Shamelessly taken from String.cpp
// =====================================================================
inline int32_t
UTF8CharLen(uint8_t ch)
{
	return ((0xe5000000 >> ((ch >> 3) & 0x1e)) & 3) + 1;
}


// XML 1.0 ss 2.3 (Production 4)
// =====================================================================
inline status_t
CheckForValidNameChar(uint8_t ch)
{
	(void) ch;
	return B_OK;
	return B_XML_BAD_NAME_CHARACTER;
}


// XML 1.0 ss 2.3 (Production 5)
// =====================================================================
inline status_t
CheckForValidFirstNameChar(uint8_t ch)
{
	(void) ch;
	return B_OK;
	return B_XML_BAD_NAME_CHARACTER;
}





XMLParserCore::XMLParserCore(BXMLParseContext * par_context, bool par_dtdOnly, uint32_t par_flags)
: m_context(par_context)
, m_dtdOnly(par_dtdOnly)
, m_flags(par_flags)
, m_inDTD(par_dtdOnly)
, m_remainingChars(0)
, m_parseText(0)
, m_longStringData(0)
{
};


XMLParserCore::~XMLParserCore(void)
{
	if(m_parseText)
	{
		free(m_parseText);
		m_parseText = NULL;
	}
	if(m_remainingChars)
	{
		free(m_remainingChars);
		m_remainingChars = NULL;
	}
};


status_t
XMLParserCore::ProcessBegin(void)
{
	initializeState();

	// Start the line numbers at 1.  Column numbers are handled later.
	m_context->line = 1;
	
	// Yeah dude!
	m_context->OnDocumentBegin(m_flags);
	return B_OK;
}

status_t
XMLParserCore::ProcessEnd(bool isComplete)
{
	status_t err = B_OK;

	if(isComplete)
	{
		// We won!
		err = m_context->OnDocumentEnd();
		if (err != B_OK)
		{
			err = m_context->OnError(err, false, __LINE__);
		}							
	}
	else
	{
		// We Lost!
		err = m_context->OnDocumentFail();
		if (err != B_OK)
		{
			err = m_context->OnError(err, false, __LINE__);
		}							
	}

	finalizeState();
	return err;
}

void 
XMLParserCore::initializeState()
{

	
	// Characters remaining from the last iteration
	if(m_remainingChars)
	{
		free(m_remainingChars);
		m_remainingChars = NULL;
	}
	m_remainingCharsSize = 0;
	
	if(m_parseText)
	{
		free(m_parseText);
		m_parseText = NULL;
	}
	m_parseTextLength = 0;
	
	m_characterSize = 0;
	
	
	m_state = PARSER_IN_UNKNOWN;
	m_upcomming = PARSER_NORMAL;
	m_subState = PARSER_SUB_IN_UNKNOWN;
	
	// The current token, until it has been completed, and we're ready to
	// move on to the next one.
	m_currentToken = "";
	
	// Current Name meaning element, target, notation, or any of those other thigns
	m_currentName = "";
	m_savedName = "";
	m_currentSubName = "";
	m_entityValue = "";
	m_delimiter = '\0';
	
	// Mapping of name/values.  Use for attributes, everything.
	m_stringMap.Undefine();

	m_longStringData = NULL;
	m_carryoverLongData[0] = '\0';
	m_carryoverLongData[1] = '\0';
	m_carryoverLongData[2] = '\0';
	m_carryoverLongData[3] = '\0';
	m_someChars[0] = '\0';
	m_someChars[1] = '\0';
	m_someChars[2] = '\0';
	
	m_inDTD = false;
	m_declType = PARSER_GE_DECL;
	
	m_emptyString = "";
}

void 
XMLParserCore::finalizeState()
{
}

// =====================================================================
status_t
XMLParserCore::handle_element_start(SString & name, SValue & attributes, uint32_t flags)
{
	status_t err;
	void * cookie;
	SValue key, value;
	
	if (flags & B_XML_HANDLE_ATTRIBUTE_ENTITIES)
	{
		// Go through each of the attributes, looking for entities.
		// If you find an entity, go ask for the replacement text,
		// and fill it in.
		
		cookie = NULL;
		while (B_OK == attributes.GetNextItem(&cookie, &key, &value))
		{
			SString v = value.AsString();
			
			err = expand_entities(v, '&');
			if (err != B_OK)
			{
				if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
					return err;
			}							
			
			if (flags & B_XML_COALESCE_WHITESPACE)
				v.Mush();
			
			attributes.JoinItem(key, SValue::String(v));
		}
		
		err = m_context->OnStartTag(name, attributes);
		if (err != B_OK)
		{
			if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
				return err;
		}							
		return B_OK;
	}
	else if (flags & B_XML_COALESCE_WHITESPACE)
	{

		cookie = NULL;
		while (B_OK == attributes.GetNextItem(&cookie, &key, &value))
		{
			SString v = value.AsString();
			v.Mush();
			attributes.JoinItem(key, SValue::String(v));
		}

		
		err = m_context->OnStartTag(name, attributes);
		if (err != B_OK)
		{
			if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
				return err;
		}							
		return B_OK;
	}
	else
	{
		err = m_context->OnStartTag(name, attributes);
		if (err != B_OK)
		{
			if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
				return err;
		}							
		return B_OK;
	}
}



// =====================================================================
status_t
XMLParserCore::handle_entity_decl(bool parameter, SString & name,
					SString & value, uint32_t flags, bool doctypeBeginOnly)
{
	status_t err;
	
	// This second pass is required because it's just easier to expand
	// the entities while reading, and then process the results of that.
	
	// Determine what type of declaration it is.  The report
	// it with the correct event.
	const char * p = value.String();
	parser_sub_state subState = PARSER_SUB_IN_WHITESPACE_1;
	SString currentToken;
	SString publicID, systemID, internalData, spec, notation;
	bool foundInternalData = false;
	while (*p)
	{
		switch(subState)
		{
			case PARSER_SUB_IN_WHITESPACE_1:
			{
				if (*p == '\"')
				{
					// Internal Parsed
					subState = PARSER_SUB_IN_INTERNAL_PARSED_VALUE;
				}
				else if (*p == 'S' || *p == 'P')
				{
					// Either SYSTEM or PUBLIC
					spec.SetTo(*p,1);
					subState = PARSER_SUB_IN_SPEC;
				}
				else if (!IS_WHITESPACE(*p))
				{
					err = B_XML_PARSE_ERROR;
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						return err;
				}
			}
			break;
			
			
			// ====== Internal Parsed ==========================
			case PARSER_SUB_IN_INTERNAL_PARSED_VALUE:
			{
				if (*p == '\"')
				{
					subState = PARSER_SUB_IN_WHITESPACE_4;
					foundInternalData = true;
				}
				else
					internalData += *p;
			}
			break;
			case PARSER_SUB_IN_WHITESPACE_4:
			{
				// At this point, all we're allowed is whitespace.
				// We could ignore this and be forgiving, but instead,
				// we will be anal.
				if (!IS_WHITESPACE(*p))
				{
					err = B_XML_PARSE_ERROR;
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						return err;
				}
			}
			break;
			
			
			// ====== Choose ID Type ===========================
			case PARSER_SUB_IN_SPEC:
			{
				if (spec == "PUBLIC")
				{
					subState = PARSER_SUB_IN_WHITESPACE_5;
				}
				else if (spec == "SYSTEM")
				{
					subState = PARSER_SUB_IN_WHITESPACE_7;
				}
				else if (IS_WHITESPACE(*p))
				{
					err = B_XML_PARSE_ERROR;
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						return err;
				}
				else
				{
					spec += *p;
				} 
			}
			break;
			
			
			// ====== Public ID ================================
			case PARSER_SUB_IN_WHITESPACE_5:
			{
				if (*p == '\"')
					subState = PARSER_SUB_IN_READING_PUBLIC_ID;
				else if (!IS_WHITESPACE(*p))
				{
					err = B_XML_ENTITY_VALUE_NO_QUOTES;
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						return err;
				}
			}
			break;
			case PARSER_SUB_IN_READING_PUBLIC_ID:
			{
				if (*p == '\"')
				{
					// Now look for a systemID (WHITESPACE_6 got optimized out)
					subState = PARSER_SUB_IN_WHITESPACE_7;
				}
				else
					publicID += *p;
			}
			break;
			
			
			// ====== System ID ================================
			case PARSER_SUB_IN_WHITESPACE_7:
			{
				if (*p == '\"')
					subState = PARSER_SUB_IN_READING_SYSTEM_ID;
				else if (!IS_WHITESPACE(*p))
				{
					err = B_XML_ENTITY_VALUE_NO_QUOTES;
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						return err;
				}
			}
			break;
			case PARSER_SUB_IN_READING_SYSTEM_ID:
			{
				if (*p == '\"')
				{
					spec.Truncate(0);
					subState = PARSER_SUB_IN_WHITESPACE_8;
				}
				else
					systemID += *p;
			}
			break;
			
			
			// ====== End or NDATA =============================
			case PARSER_SUB_IN_WHITESPACE_8:
			{
				if (*p == 'N')
				{
					spec += *p;
					subState = PARSER_SUB_IN_READIING_NDATA;
				}
				else if (!IS_WHITESPACE(*p))
				{
					err = B_XML_PARSE_ERROR;
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						return err;
				}
			}
			break;
			case PARSER_SUB_IN_READIING_NDATA:
			{
				if (spec == "NDATA")
				{
					subState = PARSER_SUB_IN_WHITESPACE_9;
				}
				else if (IS_WHITESPACE(*p))
				{
					err = B_XML_PARSE_ERROR;
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						return err;
				}
				else
					spec += *p;
			}
			break;
			case PARSER_SUB_IN_WHITESPACE_9:
			{
				if (!IS_WHITESPACE(*p))
				{
					notation += *p;
					subState = PARSER_SUB_IN_READING_NOTATION;
				}
			}
			break;
			case PARSER_SUB_IN_READING_NOTATION:
			{
				if (IS_WHITESPACE(*p))
					subState = PARSER_SUB_IN_WHITESPACE_10;
				else
					notation += *p;
			}
			break;
			case PARSER_SUB_IN_WHITESPACE_10:
			{
				if (!IS_WHITESPACE(*p))
				{
					err = B_XML_PARSE_ERROR;
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						return err;
				}
			}
			break;
			
			default:
				; // error?
		}
		
		++p;
	}
	
	if (subState == PARSER_SUB_IN_WHITESPACE_1
			|| subState == PARSER_SUB_IN_SPEC
			|| subState == PARSER_SUB_IN_WHITESPACE_5
			|| subState == PARSER_SUB_IN_READING_PUBLIC_ID
			|| subState == PARSER_SUB_IN_READING_SYSTEM_ID
			|| subState == PARSER_SUB_IN_READIING_NDATA
			|| subState == PARSER_SUB_IN_WHITESPACE_9 )
	{
		err = B_XML_PARSE_ERROR;
		if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
			return err;
	}
	if (doctypeBeginOnly)
	{
		// Technically, there can't be an empty systemID, but we're going to allow it because
		// we will fail later if it can't find the publicID and they don't want to try looking
		// for one.  --joeo
		//else if (systemID == "")
		//	return B_XML_PARSE_ERROR;
		if (foundInternalData || notation != "")
		{
			err = B_XML_PARSE_ERROR;
			if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
				return err;
		}
		err = m_context->OnExternalSubset(publicID, systemID, flags);
		if (err != B_OK)
		{
			if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
				return err;
		}							
		return B_OK;
	}
	else
	{
		if (foundInternalData)
		{
			err = m_context->OnInternalParsedEntityDecl(name, internalData, parameter, flags);
			if (err != B_OK)
			{
				if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
					return err;
			}							
			return B_OK;
		}
		// Technically, there can't be an empty systemID, but we're going to allow it because
		// we will fail later if it can't find the publicID and they don't want to try looking
		// for one.  --joeo
		//else if (systemID == "")
		//	return B_XML_PARSE_ERROR;
		else if (notation != "")
		{
			if (parameter)
				err = B_XML_NO_UNPARSED_PARAMETER_ENTITIES;
			else
				err = m_context->OnUnparsedEntityDecl(name, publicID, systemID, notation);
			if (err != B_OK)
			{
				if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
					return err;
			}
			return B_OK;
		}
		else
		{
			err = m_context->OnExternalParsedEntityDecl(name, publicID, systemID, parameter, flags);
			if (err != B_OK)
			{
				if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
					return err;
			}
			return B_OK;
		}
	}
	// Will never get here.  Just make gcc happy
	return B_ERROR;
}


// =====================================================================
status_t
XMLParserCore::handle_attribute_decl(SString & element, SString & data)
{
	status_t err = B_ERROR;
	SString name;
	SString type;
	SString mode;
	SString def;
	SString elementTemp;
	bool okayToEnd = false;
	enum {BEGIN, NAME, FIRST_TYPE, TYPE_ENUM, TYPE, MODE_FIRST, MODE, DEF_FIRST, DEF, EAT_SPACE} state = NAME;
	data.Mush();
	const char * p = data.String();
	while (true)
	{
		switch (state)
		{
			case BEGIN:
				if (*p)
					okayToEnd = false;
				if (!IS_WHITESPACE(*p))
					state = NAME;
				else
					break;
			// Fall through
			case NAME:
				if (IS_WHITESPACE(*p))
					state = FIRST_TYPE;
				else
					name += *p;
				break;
			case FIRST_TYPE:
				if (*p == '(')
				{
					type.SetTo('(',1);
					state = TYPE_ENUM;
				}
				else
				{
					type.SetTo(*p,1);
					state = TYPE;
				}
				break;
			case TYPE_ENUM:
				if (*p == ')')
				{
					type += ')';
					state = EAT_SPACE;
				}
				else
					type += *p;
				break;
			case EAT_SPACE:
				if (!IS_WHITESPACE(*p))
				{
					state = MODE_FIRST;
					goto MODE_FIRST_1;
				}
				break;
			case TYPE:
				if (IS_WHITESPACE(*p))
					state = MODE_FIRST;
				else
					type += *p;
				break;
			case MODE_FIRST:
MODE_FIRST_1:
				if (*p == '#')
					state = MODE;
				else if (*p == '\"')
					state = DEF;
				else
					goto DONE_WITH_DECL;
				break;
			case MODE:
				if (IS_WHITESPACE(*p) || !*p)
				{
					if (mode == "REQUIRED" || mode == "IMPLIED")
						goto DONE_WITH_DECL;
					else
						state = DEF_FIRST;
				}
				else
					mode += *p;
				break;
			case DEF_FIRST:
				if (*p == '\"')
					state = DEF;
				else
				{
					err = B_XML_PARSE_ERROR;
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						return err;
				}
				break;
			case DEF:
				if (*p == '\"')
				{
DONE_WITH_DECL:
					// Temporary copy for API consistency, things expect to be able 
					// to adopt the storage of the BStrings to avoid a copy.
					elementTemp = element;
					
					// Can only affect enum types. Make it easier to parse later.
					type.StripWhitespace();
					
					// FINISHED Attribute Decl
					err = m_context->OnAttributeDecl(elementTemp, name, type, mode, def);
					if (err != B_OK)
					{
						if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
							return err;
					}
					
					// Clean everything up for the next trip around
					state = BEGIN;
					name.Truncate(0);
					type.Truncate(0);
					mode.Truncate(0);
					def.Truncate(0);
					okayToEnd = true;
				}
				else
					def += *p;
				break;
		}
		if (!*p)
		{
			if (okayToEnd)
				return B_OK;
			else
			{
				if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
					return err;
			}
		}
		p++;
	}
	
	return B_OK;
}


static char
hex2dec(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else
		return 0;
}


// =====================================================================
status_t
XMLParserCore::expand_char_ref(const SString & entity, SString & entityVal)
{
	uint32_t character = 0;

	entityVal.Truncate(0);
	size_t last = entity.Length();
	if (entity.ByteAt(1) == 'x')
	{
		// Hex
		for (size_t i=2; i<last; i++)
		{
			character <<= 4;
			character |= hex2dec(entity[i]);
		}
	}
	else
	{
		// Decimal
		character = atoi(entity.String()+1);
	}
	
	entityVal.AppendChar(character);
	return B_NO_ERROR;
}


// =====================================================================
status_t
XMLParserCore::expand_char_refs(SString & str)
{
	char delimiter = '&';
	status_t err;
	
	SString entity;
	SString entityVal;
	SString newValue("");
	
	int32_t offset = 0, oldOffset = 0;
	int32_t end = -1;
	
	while (true)
	{
		oldOffset = end + 1;
		offset = str.FindFirst(delimiter, oldOffset);
		if (offset < 0)
			break;
		end = str.FindFirst(';', offset);
		if (end < 0)
			break;
		
		newValue.Append(str.String() + oldOffset, offset-oldOffset);
		str.CopyInto(entity, offset+1, end-offset-1);
		
		if (entity.ByteAt(0) == '#')
		{
			err = expand_char_ref(entity, entityVal);
			if (err != B_OK)
				return err;
		}
		
		newValue.Append(entityVal);
	}
	
	newValue.Append(str.String() + oldOffset, str.Length()-oldOffset);
	str = newValue;
	return B_OK;
}


// =====================================================================
status_t
XMLParserCore::expand_entities(SString & str, char delimiter)
{
	status_t err;
	
	SString entity;
	SString entityVal;
	SString newValue("");
	
	int32_t offset = 0, oldOffset = 0;
	int32_t end = -1;
	
	while (true)
	{
		oldOffset = end + 1;
		offset = str.FindFirst(delimiter, oldOffset);
		if (offset < 0)
			break;
		end = str.FindFirst(';', offset);
		if (end < 0)
			break;
		
		newValue.Append(str.String() + oldOffset, offset-oldOffset);
		str.CopyInto(entity, offset+1, end-offset-1);
		
		if (entity.ByteAt(0) == '#' && delimiter == '&')
		{
			err = expand_char_ref(entity, entityVal);
			if (err != B_OK)
			{
				if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
					return err;
			}
		}
		else
		{
			if (delimiter == '%')
				err = m_context->OnParameterEntityRef(entity, entityVal);
			else
				err = m_context->OnGeneralParsedEntityRef(entity, entityVal);
			if (err != B_OK)
			{
				if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
					return err;
			}
		}
		
		newValue.Append(entityVal);
	}
	
	newValue.Append(str.String() + oldOffset, str.Length()-oldOffset);
	str = newValue;
	return B_OK;
}




status_t 
XMLParserCore::ProcessInputBuffer(size_t dataSize, const uint8_t * dataBuffer, bool& errorExit)
{
	status_t err = B_OK;
	errorExit = true; // default

	// don't ever try to parse text less than 3 bytes long.  It screws
	// up the stuff where you tell comments are ending
	if (dataSize + m_remainingCharsSize <= 3)
	{
		m_parseText = (uint8_t *) malloc(m_remainingCharsSize + dataSize + 1);
		if (m_remainingCharsSize > 0)
		{
			memcpy(m_parseText, m_remainingChars, m_remainingCharsSize);
			free(m_remainingChars);
			m_remainingChars = NULL;
		}
		memcpy(m_parseText+m_remainingCharsSize, dataBuffer, dataSize);
		m_remainingCharsSize = m_remainingCharsSize + dataSize;
		m_remainingChars = m_parseText;
		m_parseText = NULL;
		m_remainingChars[m_remainingCharsSize] = '\0';
		errorExit = false;
		return err;
	}
	
	m_parseText = (uint8_t *) malloc(m_remainingCharsSize + dataSize + 1);
	if (m_remainingCharsSize > 0)
	{
		memcpy(m_parseText, m_remainingChars, m_remainingCharsSize);
		free(m_remainingChars);
		m_remainingChars = NULL;
	}
	memcpy(m_parseText+m_remainingCharsSize, dataBuffer, dataSize);
	m_parseTextLength = m_remainingCharsSize + dataSize;
	m_parseText[m_parseTextLength] = '\0';
	m_remainingCharsSize = 0;
	
	// m_longStringData, at this point might be dangling pointer, because we sent
	// the data to the m_context and then deleted it, however, we haven't found
	// the end of the section that it's in.  So set it to the next text.
	if (m_longStringData) 
	{
		m_longStringData = m_parseText;
	}
	
	// printf("--[%s]-- ", m_parseText);
	
	m_characterSize = 0;
	uint8_t	* pParseChar = m_parseText;
	while (pParseChar < (m_parseText + m_parseTextLength) && *pParseChar)
	{
		// Nice error handling
		if (*pParseChar == '\n' || *pParseChar == '\r' && pParseChar[1] != '\n')
		{
			++m_context->line;
			m_context->column = 1;
		}
		else
		{
			++m_context->column;
		}
		
		m_characterSize = UTF8CharLen(*pParseChar);
		
		// If we don't have the entire character here in the buffer,
		// defer it to the next loop (UTF-8)
		// This is because it might be passed into a m_longStringData section
		if (pParseChar + m_characterSize > m_parseText + m_parseTextLength)
		{
			;
		}
		
		// Else, handle this character
		else
		{
			switch (m_state)
			{
				case PARSER_IN_UNKNOWN:
				{
					if (*pParseChar == '<')
					{
						if (m_longStringData)
						{
							// FINISHED Text Section
							err = m_context->OnTextData((char *)m_longStringData, pParseChar-m_longStringData);
							if (err != B_OK)
							{
								if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
									goto ERROR_2;
							}							
							m_longStringData = NULL;
						}
						
						m_state = PARSER_IN_UNKNOWN_MARKUP;
					}
					else if (!m_dtdOnly && m_inDTD && *pParseChar == ']')
					{
						// FINISHED End Internal Subset
						err = m_context->OnInternalSubsetEnd();
						if (err != B_OK)
						{
							if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
								goto ERROR_2;
						}							
					}
					else if (m_inDTD && *pParseChar == '>')
					{
						// FINISHED End Doctype Declaration
						err = m_context->OnDocumentTypeEnd();
						if (err != B_OK)
						{
							if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
								goto ERROR_2;
						}							
						
						m_inDTD = false;
					}
					else if (m_inDTD && *pParseChar == '%')
					{
						m_currentName.Truncate(0);
						m_state = PARSER_IN_PE_REF;
					}
					else if ((m_dtdOnly || !m_inDTD) && *pParseChar == '&')
					{
						if (m_longStringData)
						{
							// FINISHED Text Section
							err = m_context->OnTextData((char *)m_longStringData, pParseChar-m_longStringData);
							if (err != B_OK)
							{
								if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
									goto ERROR_2;
							}							
							
							m_longStringData = NULL;
						}
						m_currentName.Truncate(0);
						m_state = PARSER_IN_GE_REF;
					}
					else if ((m_dtdOnly || !m_inDTD) && !m_longStringData)
					{
						m_longStringData = pParseChar;
					}
				}
				break;
				case PARSER_IN_UNKNOWN_MARKUP:
				{
					if (*pParseChar == '/')
					{
						m_state = PARSER_IN_ELEMENT_END_TAG;
						m_currentName.Truncate(0);
					}
					else if (*pParseChar == '!')
					{
						m_state = PARSER_IN_UNKNOWN_MARKUP_GT_E;
						m_currentToken.Truncate(0);
					}
					else if (*pParseChar == '?')
					{
						m_state = PARSER_IN_PROCESSING_INSTRUCTION_TARGET;
						m_currentToken.Truncate(0);
					}
					else
					{
						m_state = PARSER_IN_ELEMENT_START_NAME;
						
						err = CheckForValidFirstNameChar(*pParseChar);
						if (err != B_OK)
						{
							if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
								goto ERROR_2;
						}							
						
						m_currentName.Truncate(0);
						m_currentName += *pParseChar;
						m_stringMap.Undefine();

					}
				}
				break;
				case PARSER_IN_UNKNOWN_MARKUP_GT_E:
				{
					// Begin Comment
					if (m_currentToken == "--")
					{
						// This is the first character of a comment
						m_longStringData = pParseChar;
						m_state = PARSER_IN_COMMENT;
						m_someChars[0] = '\0';
						m_someChars[1] = '\0';
						m_someChars[2] = '\0';
					}
					
					// Begin CDATA Section
					else if (m_currentToken == "[CDATA[")
					{
						// This is the first character of a CData section
						m_longStringData = pParseChar;
						m_state = PARSER_IN_CDATA;
						m_someChars[0] = '\0';
						m_someChars[1] = '\0';
						m_someChars[2] = '\0';
					}
					
					// Begin INCLUDE Section
					// else if (m_currentToken == "[INCLUDE[")
					// {
					// 	
					// }
					// 
					// Begin IGNORE Section
					// else if (m_currentToken == "[IGNORE[")
					// {
					// 	
					// }
					
					// Begin DOCTYPE Declaration
					else if (m_currentToken == "DOCTYPE")
					{
						if (!IS_WHITESPACE(*pParseChar))
						{
							err = B_XML_PARSE_ERROR;
							if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
								goto ERROR_2;
						}
						
						m_state = PARSER_IN_DOCTYPE;
						m_subState = PARSER_SUB_IN_WHITESPACE_1;
					}
					
					// Begin ELEMENT Declaration
					else if (m_currentToken == "ELEMENT")
					{
						m_state = PARSER_IN_ELEMENT_DECL;
						m_subState = PARSER_SUB_IN_WHITESPACE_1;
					}
					
					// Begin ATTLIST Declaration
					else if (m_currentToken == "ATTLIST")
					{
						m_state = PARSER_IN_ATTLIST_DECL;
						m_subState = PARSER_SUB_IN_WHITESPACE_1;
					}
					
					// Begin ENTITY Declaration
					else if (m_currentToken == "ENTITY")
					{
						m_state = PARSER_IN_ENTITY_DECL;
						m_subState = PARSER_SUB_IN_WHITESPACE_1;
					}
					
					// Begin NOTATION Section
					else if (m_currentToken == "NOTATION")
					{
						m_state = PARSER_IN_NOTATION_DECL;
						m_subState = PARSER_SUB_IN_WHITESPACE_1;
					}
					
					// No tokens that can be here are longer than 9 chars.
					// Signal an error.
					else if (m_currentToken.Length() > 9)
					{
						err = B_XML_PARSE_ERROR;
						goto ERROR_2;
					}
					// Build up m_currentToken
					else
					{
						m_currentToken += *pParseChar;
					}
				}
				break;
				case PARSER_IN_PROCESSING_INSTRUCTION_TARGET:
				{
					if (IS_WHITESPACE(*pParseChar))
					{
						m_currentName = m_currentToken;
						m_currentToken.Truncate(0);
						m_state = PARSER_IN_PROCESSING_INSTRUCTION;
					}
					else
					{
						m_currentToken += *pParseChar;
					}
				}
				break;
				case PARSER_IN_PROCESSING_INSTRUCTION:
				{
					// Skip whitespace until we find something
					if (IS_WHITESPACE(*pParseChar) && m_currentToken.Length() == 0)
						;
					// When we find ?> end
					else if (*pParseChar == '?')
						m_upcomming = PARSER_NEARING_END_1;
					else if (*pParseChar == '>' && m_upcomming == PARSER_NEARING_END_1)
					{
						// Expand Character Refs
						err = expand_char_refs(m_currentToken);
						if (err != B_OK)
							goto ERROR_2;
						
						// FINISHED Processing Instruction
						err = m_context->OnProcessingInstruction(m_currentName, m_currentToken);
						if (err != B_OK)
							goto ERROR_2;
						
						m_currentName.Truncate(0);
						m_currentToken.Truncate(0);
						m_state = PARSER_IN_UNKNOWN;
						m_upcomming = PARSER_NORMAL;
					}
					else
					{
						m_upcomming = PARSER_NORMAL;
						m_currentToken += *pParseChar;
					}
				}
				break;
				case PARSER_IN_ELEMENT_START_NAME:
				{
					// End of Element Tag
					if (*pParseChar == '/')
					{
						m_state = PARSER_IN_ELEMENT_START_TAG;
						m_upcomming = PARSER_NEARING_END_1;
					}
					else if (*pParseChar == '>')
					{
						// FINISHED Element Start Tag
						err = handle_element_start(m_currentName, m_stringMap, m_flags);
						if (err != B_OK)
							goto ERROR_2;
						
						m_state = PARSER_IN_UNKNOWN;
					}
					// End of the Element Name
					else if (IS_WHITESPACE(*pParseChar))
					{
						m_currentToken.Truncate(0);
						m_state = PARSER_IN_ELEMENT_START_TAG;
						m_subState = PARSER_SUB_IN_WHITESPACE;
					}
					else
					{
						m_currentName += *pParseChar;
					}
				}
				break;
				case PARSER_IN_ELEMENT_START_TAG:
				{
					if (m_subState != PARSER_SUB_IN_VALUE && *pParseChar == '/')
					{
						m_upcomming = PARSER_NEARING_END_1;
					}
					else if (*pParseChar == '>')
					{
						// FINISHED Element Start Tag
						m_savedName = m_currentName;
						err = handle_element_start(m_currentName, m_stringMap, m_flags);
						if (err != B_OK)
							goto ERROR_2;
						
						if (m_upcomming == PARSER_NEARING_END_1)
						{
							// FINISHED Implicit Element End Tag
							err = m_context->OnEndTag(m_savedName);
							if (err != B_OK)
								goto ERROR_2;
						}
						
						m_currentName.Truncate(0);
						m_currentToken.Truncate(0);
						m_state = PARSER_IN_UNKNOWN;
						m_upcomming = PARSER_NORMAL;
					}
					else
					{
						if (m_upcomming == PARSER_NEARING_END_1)
						{
							m_upcomming = PARSER_NORMAL;
							m_currentToken += '/';
						}
						
						// Do Attributes
						switch (m_subState)
						{
							case PARSER_SUB_IN_WHITESPACE:
							{
									if (!IS_WHITESPACE(*pParseChar))
									m_subState = PARSER_SUB_IN_NAME;
									m_currentToken.Truncate(0);
									m_currentToken += *pParseChar;
							}
							break;
							case PARSER_SUB_IN_NAME:
							{
								if (IS_WHITESPACE(*pParseChar))
									m_subState = PARSER_SUB_IN_WHITESPACE_1;
								else if (*pParseChar == '=')
									m_subState = PARSER_SUB_READY_FOR_VALUE;
								else
									m_currentToken += *pParseChar;
							}
							break;
							case PARSER_SUB_IN_WHITESPACE_1:
							{
								if (IS_WHITESPACE(*pParseChar))
									; // nothing
								else if (*pParseChar == '=')
									m_subState = PARSER_SUB_READY_FOR_VALUE;
									else
									{
									err = B_XML_PARSE_ERROR;
									goto ERROR_2;
									}
							}
							break;
							case PARSER_SUB_READY_FOR_VALUE:
							{
									if (*pParseChar == '\'')
									m_delimiter = '\'';
									else if (*pParseChar == '\"')
									m_delimiter = '\"';
									else if (IS_WHITESPACE(*pParseChar))
									m_delimiter = '\0'; // nothing
									else
									{
									err = B_XML_PARSE_ERROR;
									goto ERROR_2;
									}
									if (m_delimiter != '\0')
									{
										m_currentSubName = m_currentToken;
										m_currentToken.MakeEmpty();
										m_subState = PARSER_SUB_IN_VALUE;
									}
							}
							break;
							case PARSER_SUB_IN_VALUE:
							{
								if (*pParseChar == m_delimiter)
								{
									if (m_stringMap[SValue(m_currentSubName)].IsDefined())
									{
										bout << "m_stringMap[" << m_currentSubName << "] = " << m_stringMap[SValue(m_currentSubName)] << endl;
										err = B_XML_ATTR_NAME_IN_USE;
										goto ERROR_2;
									}
									m_stringMap.JoinItem(SValue::String(m_currentSubName), SValue::String(m_currentToken));
									m_currentSubName.Truncate(0);
									m_currentToken.Truncate(0);
									m_subState = PARSER_SUB_IN_WHITESPACE;
								}
								else
									m_currentToken += *pParseChar;
							}
							break;
							default:
								;
						}
						
					}
				}
				break;
				case PARSER_IN_ELEMENT_END_TAG:
				{
					// End of the Element Name
					if (*pParseChar == '>')
					{
						// FINISHED Element End Tag
						err = m_context->OnEndTag(m_currentName);
						if (err != B_OK)
						{
							if (B_OK != m_context->OnError(err, false, __LINE__))
								goto ERROR_2;
						}
						
						m_currentName.Truncate(0);
						m_state = PARSER_IN_UNKNOWN;
						m_upcomming = PARSER_NORMAL;
					}
					else if (IS_WHITESPACE(*pParseChar))
					{
						m_upcomming = PARSER_NEARING_END_1;
					}
					else
					{
						if (m_upcomming == PARSER_NORMAL)
							m_currentName += *pParseChar;
					}
				}
				break;
				case PARSER_IN_COMMENT:
				{
					m_someChars[0] = m_someChars[1];
					m_someChars[1] = m_someChars[2];
					m_someChars[2] = *pParseChar;
					
					// See comment in PARSER_IN_CDATA
					if (m_carryoverLongData[0])
					{
						// See the comment for this in the CData section handler
						
						// FINISHED Comment Section
						err = m_context->OnComment((char *) m_carryoverLongData, 3);
						if (err != B_OK)
						{
							if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
								goto ERROR_2;
						}							
						
						m_carryoverLongData[0] = '\0';
					}
					
					if (m_someChars[0] == '-' && m_someChars[1] == '-' && m_someChars[2] == '>')
					{
						// need to strip off the trailing "--"
						// this is okay because we have a minimum of 3 chars in the buffer
						// this line is why we have that restriction.
						*(pParseChar-2) = '\0';
						
						// FINISHED Comment Section
						err = m_context->OnComment((char *) m_longStringData, pParseChar-m_longStringData);
						if (err != B_OK)
						{
							if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
								goto ERROR_2;
						}							
						
						m_state = PARSER_IN_UNKNOWN;
						m_longStringData = NULL;
					}
				}
				break;
				case PARSER_IN_CDATA:
				{
					
					m_someChars[0] = m_someChars[1];
					m_someChars[1] = m_someChars[2];
					m_someChars[2] = *pParseChar;
					
					// These three lonely characters get outputted whenever
					// they crossed the buffer boundary.  The idea is that
					// it's faster to just send them out as three lonely
					// characters in their own CData section than it is to
					// memcpy around the next one.
					if (m_carryoverLongData[0])
					{
						// FINISHED CData Section
						err = m_context->OnCData((char *) m_carryoverLongData, 3);
						if (err != B_OK)
						{
							if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
								goto ERROR_2;
						}							
						
						m_carryoverLongData[0] = '\0';
					}
					
					if (m_someChars[0] == ']' && m_someChars[1] == ']' && m_someChars[2] == '>')
					{
						// See comment for this same thing in the comment handler
						*(pParseChar-2) = '\0';
						
						// FINISHED CData Section
						err = m_context->OnCData((char *) m_longStringData, pParseChar-m_longStringData);
						if (err != B_OK)
						{
							if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
								goto ERROR_2;
						}							
						
						m_state = PARSER_IN_UNKNOWN;
						m_longStringData = NULL;
					}
				}
				break;
				case PARSER_IN_DOCTYPE:
				{
					// doctypeName
					switch (m_subState)
					{
						case PARSER_SUB_IN_WHITESPACE_1:
						{
							if (!IS_WHITESPACE(*pParseChar))
							{
								m_currentName.Truncate(0);
								m_currentName += *pParseChar;
								m_subState = PARSER_SUB_IN_DOCTYPE_NAME;
							}
						}
						break;
						case PARSER_SUB_IN_DOCTYPE_NAME:
						{
							if (*pParseChar == '[')
							{
								// FINISHED Begin Doctype Declaration
								// No ExternalID
								err = m_context->OnDocumentTypeBegin(m_currentName);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								m_emptyString = "";
								
								// FINISHED Begin Internal Subset
								err = m_context->OnInternalSubsetBegin();
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							

								m_state = PARSER_IN_UNKNOWN;
								m_subState = PARSER_SUB_IN_UNKNOWN;
								m_inDTD = true;
								m_longStringData = NULL;
							}
							else if (*pParseChar == '>')
							{
								// FINISHED Begin Doctype Declaration
								// No ExternalID
								err = m_context->OnDocumentTypeBegin(m_currentName);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								m_emptyString = "";
								
								// FINISHED End Doctype Declaration
								err = m_context->OnDocumentTypeEnd();
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								
								m_state = PARSER_IN_UNKNOWN;
								m_inDTD = false;
							}
							else if (IS_WHITESPACE(*pParseChar))
								m_subState = PARSER_SUB_IN_WHITESPACE_2;
							else
								m_currentName += *pParseChar;
						}
						break;
						case PARSER_SUB_IN_WHITESPACE_2:
						{
							if (*pParseChar == '[')
							{
								// FINISHED Begin Doctype Declaration
								// No ExternalID
								err = m_context->OnDocumentTypeBegin(m_currentName);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								m_emptyString = "";
								
								// FINISHED Begin Internal Subset
								err = m_context->OnInternalSubsetBegin();
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							

								m_state = PARSER_IN_UNKNOWN;
								m_subState = PARSER_SUB_IN_UNKNOWN;
								m_inDTD = true;
								m_longStringData = NULL;
							}
							else if (*pParseChar == '>')
							{
								// FINISHED Begin Doctype Declaration
								// No ExternalID
								err = m_context->OnDocumentTypeBegin(m_currentName);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								m_emptyString = "";
								
								// FINISHED End Doctype Declaration
								err = m_context->OnDocumentTypeEnd();
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								
								m_state = PARSER_IN_UNKNOWN;
								m_inDTD = false;
							}
							else if (!IS_WHITESPACE(*pParseChar))
							{
								m_subState = PARSER_SUB_IN_EXTERNAL_ID;
								m_currentToken.Truncate(0);
								m_currentToken += *pParseChar;
							}
						}
						break;
						case PARSER_SUB_IN_EXTERNAL_ID:
						{
							if (*pParseChar == '[')
							{
								// FINISHED Begin Doctype Declaration
								// With ExternalID
								err = m_context->OnDocumentTypeBegin(m_currentName);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								
								// Do the External Subset
								err = handle_entity_decl(false, m_emptyString, m_currentToken, m_flags, true);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								
								// FINISHED Begin Internal Subset
								err = m_context->OnInternalSubsetBegin();
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							

								m_state = PARSER_IN_UNKNOWN;
								m_subState = PARSER_SUB_IN_UNKNOWN;
								m_inDTD = true;
								m_longStringData = NULL;
							}
							else if (*pParseChar == '>')
							{
								// FINISHED Begin Doctype Declaration
								// With ExternalID
								err = m_context->OnDocumentTypeBegin(m_currentName);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
									
								// Do the External Subset
								err = handle_entity_decl(false, m_emptyString, m_currentToken, m_flags, true);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								
								// FINISHED End Doctype Declaration
								err = m_context->OnDocumentTypeEnd();
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								
								m_state = PARSER_IN_UNKNOWN;
								m_inDTD = false;
							}
							else
								m_currentToken += *pParseChar;
						}
						break;
						default:
							; // maybe should signal some type of error.
					}
					
				}
				break;
				case PARSER_IN_ELEMENT_DECL:
				{
					switch (m_subState)
					{
						case PARSER_SUB_IN_WHITESPACE_1:
						{
							if (!IS_WHITESPACE(*pParseChar))
							{
								m_currentName.Truncate(0);
								m_currentName += *pParseChar;
								m_subState = PARSER_SUB_IN_NAME;
							}
						}
						break;
						case PARSER_SUB_IN_NAME:
						{
							if (IS_WHITESPACE(*pParseChar))
								m_subState = PARSER_SUB_IN_WHITESPACE_2;
							else
								m_currentName += *pParseChar;
						}
						break;
						case PARSER_SUB_IN_WHITESPACE_2:
						{
							if (*pParseChar == '%')
							{
								m_currentToken.Truncate(0);
								m_subState = PARSER_SUB_IN_ENTITY_REF;
								m_currentSubName.Truncate(0);
							}
							else if (!IS_WHITESPACE(*pParseChar))
							{
								m_currentToken.Truncate(0);
								m_currentToken += *pParseChar;
								m_subState = PARSER_SUB_IN_SPEC;
							}
						}
						break;
						case PARSER_SUB_IN_SPEC:
						{
							if (*pParseChar == '%')
							{
								m_subState = PARSER_SUB_IN_ENTITY_REF;
								m_currentSubName.Truncate(0);
							}
							else if (*pParseChar == '>')
							{
								// FINISHED Element Decl
								err = m_context->OnElementDecl(m_currentName, m_currentToken);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								
								m_state = PARSER_IN_UNKNOWN;
								m_subState = PARSER_SUB_IN_UNKNOWN;
							}
							else
								m_currentToken += *pParseChar;
						}
						break;
						case PARSER_SUB_IN_ENTITY_REF:
						{
							if (*pParseChar == ';')
							{
								err = m_context->OnParameterEntityRef(m_currentSubName, m_entityValue);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								m_currentToken += m_entityValue;
								m_subState = PARSER_SUB_IN_SPEC;
							}
							else
								m_currentSubName += *pParseChar;
						}
						break;

						default:
							; // maybe should signal some type of error.
					}
				}
				break;
				
				case PARSER_IN_ATTLIST_DECL:
				{
					switch (m_subState)
					{
						case PARSER_SUB_IN_WHITESPACE_1:
						{
							if (!IS_WHITESPACE(*pParseChar))
							{
								m_currentName.Truncate(0);
								m_currentName += *pParseChar;
								m_subState = PARSER_SUB_IN_NAME;
							}
						}
						break;
						case PARSER_SUB_IN_NAME:
						{
							if (IS_WHITESPACE(*pParseChar))
								m_subState = PARSER_SUB_IN_WHITESPACE_2;
							else
								m_currentName += *pParseChar;
						}
						break;
						case PARSER_SUB_IN_WHITESPACE_2:
						{
							if (*pParseChar == '%')
							{
								m_currentToken.Truncate(0);
								m_subState = PARSER_SUB_IN_ENTITY_REF;
								m_currentSubName.Truncate(0);
							}
							else if (!IS_WHITESPACE(*pParseChar))
							{
								m_currentToken.Truncate(0);
								m_currentToken += *pParseChar;
								m_subState = PARSER_SUB_IN_SPEC;
							}
						}
						break;
						case PARSER_SUB_IN_SPEC:
						{
							if (*pParseChar == '%')
							{
								m_subState = PARSER_SUB_IN_ENTITY_REF;
								m_currentSubName.Truncate(0);
							}
							else if (*pParseChar == '>')
							{
								// FINISHED Attlist Decl
								err = handle_attribute_decl(m_currentName, m_currentToken);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								
								m_state = PARSER_IN_UNKNOWN;
								m_subState = PARSER_SUB_IN_UNKNOWN;
							}
							else
								m_currentToken += *pParseChar;
						}
						break;
						case PARSER_SUB_IN_ENTITY_REF:
						{
							if (*pParseChar == ';')
							{
								err = m_context->OnParameterEntityRef(m_currentSubName, m_entityValue);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								m_currentToken += m_entityValue;
								m_subState = PARSER_SUB_IN_SPEC;
							}
							else
								m_currentSubName += *pParseChar;
						}
						break;
						default:
							; // maybe should signal some type of error.
					}
				}
				break;
				
				case PARSER_IN_ENTITY_DECL:
				{
					switch (m_subState)
					{
						case PARSER_SUB_IN_WHITESPACE_1:
						{
							if (!IS_WHITESPACE(*pParseChar))
							{
								
								if (*pParseChar == '%')
								{
									m_declType = PARSER_PE_DECL;
									m_subState = PARSER_SUB_IN_WHITESPACE_2;
								}
								else
								{
									m_subState = PARSER_SUB_IN_NAME;
									m_currentName.Truncate(0);
									m_currentName += *pParseChar;
									m_declType = PARSER_GE_DECL;
								}
							}
						}
						break;
						case PARSER_SUB_IN_WHITESPACE_2:
						{
							if (!IS_WHITESPACE(*pParseChar))
							{
								m_subState = PARSER_SUB_IN_NAME;
								m_currentName.Truncate(0);
								m_currentName += *pParseChar;
							}
						}
						break;
						case PARSER_SUB_IN_NAME:
						{
							if (IS_WHITESPACE(*pParseChar))
								m_subState = PARSER_SUB_IN_WHITESPACE_3;
							else
								m_currentName += *pParseChar;
						}
						break;
						case PARSER_SUB_IN_WHITESPACE_3:
						{
							if (!IS_WHITESPACE(*pParseChar))
							{
								m_currentToken.Truncate(0);
								m_currentToken += *pParseChar;
								m_subState = PARSER_SUB_IN_SPEC;
							}
						}
						break;
						case PARSER_SUB_IN_SPEC:
						{
							if (*pParseChar == '%')
							{
								m_subState = PARSER_SUB_IN_ENTITY_REF;
								m_currentSubName.Truncate(0);
							}
							else if (*pParseChar == '>')
							{
								// FINISHED Entity Decl
								err = handle_entity_decl(m_declType == PARSER_PE_DECL, m_currentName, m_currentToken, m_flags, false);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								
								m_state = PARSER_IN_UNKNOWN;
								m_subState = PARSER_SUB_IN_UNKNOWN;
							}
							else
								m_currentToken += *pParseChar;
						}
						break;
						case PARSER_SUB_IN_ENTITY_REF:
						{
							if (*pParseChar == ';')
							{
								// You don't have to worry about recursive parameter entities
								// because, by parsing the document in one pass, we enforce
								// at total ordering on the entities, therefore, it will fail
								// if an entity references itself either directly or indirectly
								// because it has not been declared yet.  It gets declared a few
								// lines above (FINISHED Entity Decl), which always happens
								// after right now.
								err = m_context->OnParameterEntityRef(m_currentSubName, m_entityValue);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								m_currentToken += m_entityValue;
								m_subState = PARSER_SUB_IN_SPEC;
							}
							else
								m_currentSubName += *pParseChar;
						}
						break;
						default:
							; // maybe should signal some type of error.
					}
				}
				break;
				
				case PARSER_IN_NOTATION_DECL:
				{
					switch (m_subState)
					{
						case PARSER_SUB_IN_WHITESPACE_1:
						{
							if (!IS_WHITESPACE(*pParseChar))
							{
								m_currentName.Truncate(0);
								m_currentName += *pParseChar;
								m_subState = PARSER_SUB_IN_NAME;
							}
						}
						break;
						case PARSER_SUB_IN_NAME:
						{
							if (IS_WHITESPACE(*pParseChar))
								m_subState = PARSER_SUB_IN_WHITESPACE_2;
							else
								m_currentName += *pParseChar;
						}
						break;
						case PARSER_SUB_IN_WHITESPACE_2:
						{
							if (!IS_WHITESPACE(*pParseChar))
							{
								m_currentToken.Truncate(0);
								m_currentToken += *pParseChar;
								m_subState = PARSER_SUB_IN_SPEC;
							}
						}
						break;
						case PARSER_SUB_IN_SPEC:
						{
							if (*pParseChar == '>')
							{
								// FINISHED Attlist Decl
								err = m_context->OnNotationDecl(m_currentName, m_currentToken);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
								
								m_state = PARSER_IN_UNKNOWN;
								m_subState = PARSER_SUB_IN_UNKNOWN;
							}
							else
								m_currentToken += *pParseChar;
						}
						break;
						default:
							; // maybe should signal some type of error.
					}
				}
				break;
				
				case PARSER_IN_GE_REF:
				{
					if (*pParseChar == ';')
					{
						if (m_currentName.ByteAt(0) == '#')
						{
							SString entityVal;
							if (m_flags & B_XML_DONT_EXPAND_CHARREFS)
							{
								entityVal = "&";
								entityVal += m_currentName;
								entityVal += ";";
							}
							else
							{
								err = expand_char_ref(m_currentName, entityVal);
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
							}
							if (entityVal.Length() > 0)
							{
								err = m_context->OnTextData(entityVal.String(), entityVal.Length());
								if (err != B_OK)
								{
									if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
										goto ERROR_2;
								}							
							}
						}
						else
						{
							err = m_context->OnGeneralParsedEntityRef(m_currentName);
							if (err != B_OK)
							{
								if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
									goto ERROR_2;
							}							
						}
						m_state = PARSER_IN_UNKNOWN;
					}
					else
						m_currentName += *pParseChar;
				}
				break;
				
 				case PARSER_IN_PE_REF:
				{
					if (*pParseChar == ';')
					{
						err = m_context->OnParameterEntityRef(m_currentName);
						if (err != B_OK)
						{
							if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
								goto ERROR_2;
						}							
						m_state = PARSER_IN_UNKNOWN;
					}
					else
						m_currentName += *pParseChar;
				}
				break;
				
				default:
					err = B_ERROR;
					m_context->OnError(err, true, __LINE__);
					goto ERROR_2;
			}
			
		}
		
		// XXX Should this be pParseChar++????
		// pParseChar += m_characterSize;
		// I think it should, because Sebastian is having problems with multibyte characters
		// in attributes.  In text data, it works because it just skips over it.  Attributes,
		// and most other things are stored in temporary BStrings (just not long text or CData
		// sections).  Therefore, I'm going to change it so it just does pParseChar++.
		pParseChar++;
		
	}
	
	if (pParseChar == (m_parseText + m_parseTextLength))
	{
		m_remainingChars = NULL;
		m_remainingCharsSize = 0;
	}
	else
	{
		m_remainingChars = (uint8_t *) strdup((char *) pParseChar - m_characterSize);
		m_remainingCharsSize = strlen((char *) m_remainingChars);
		// fprintf(stderr, "Using Remaining Characters (length %d): %s\n", (int) m_remainingCharsSize, m_remainingChars);
	}
	
	if (m_longStringData)
	{
		// if m_state == PARSER_IN_UNKNOWN then we we want to output all the characters
		// because that m_state ends when another begins, not at a terminator string.
		// Therefore, just send them.  m_parseText is automatically NULL terminated, so
		// there's no need to do it here.
		if (m_state != PARSER_IN_UNKNOWN)
		{
			// This okay because we guaranteed before that we never
			// process less than 3 characters at a time
			m_carryoverLongData[0] = m_parseText[m_parseTextLength-3];
			m_carryoverLongData[1] = m_parseText[m_parseTextLength-2];
			m_carryoverLongData[2] = m_parseText[m_parseTextLength-1];
			
			m_parseText[m_parseTextLength-3] = '\0';
		}
		
		// Long Text Sections -- CData, Comment, Text can be split across
		// buffer boundaries.  But, it's okay to generate them as separate
		// events.
		switch (m_state)
		{
			case PARSER_IN_CDATA:
				err = m_context->OnCData((char *) m_longStringData, pParseChar-m_longStringData);
				if (err != B_OK)
				{
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						goto ERROR_2;
				}							
				break;
			case PARSER_IN_COMMENT:
				err = m_context->OnComment((char *) m_longStringData, pParseChar-m_longStringData);
				if (err != B_OK)
				{
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						goto ERROR_2;
				}							
				break;
			default:
				err = m_context->OnTextData((char *)m_longStringData, pParseChar-m_longStringData);
				if (err != B_OK)
				{
					if (B_OK != (err = m_context->OnError(err, false, __LINE__)))
						goto ERROR_2;
				}							
				break;
		}
	}
	
	errorExit = false;
	
ERROR_2:

	if(m_parseText)
	{
		free(m_parseText);
		m_parseText = NULL;
	}


	return err;
	
}



#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif
