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

#ifndef _XMLParserCore_P_H
#define _XMLParserCore_P_H


#include <xml/Parser.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {	
#endif

/*
**
** This class is the core class for two different flavors of parsers.
** This core class parses data that is fed to it via ProcessInputBuffer.
** It forms the base class for parsers that either act as a data recipient
** or use a data source.
*/
class XMLParserCore
{
public:
	XMLParserCore(BXMLParseContext * context, bool dtdOnly, uint32_t flags);
	~XMLParserCore(void);

	status_t ProcessBegin(void);
	status_t ProcessInputBuffer(size_t dataSize, const uint8_t * dataBuffer, bool& errorExit);
	status_t ProcessEnd(bool isComplete);

private:
	// =====================================================================
	typedef enum
	{
		PARSER_PE_DECL,
		PARSER_GE_DECL
	}decl_type;

	// =====================================================================
	typedef enum
	{
		PARSER_IN_UNKNOWN,
		PARSER_IN_UNKNOWN_MARKUP,					// <		encountered
		PARSER_IN_UNKNOWN_MARKUP_GT_E,				// <!		encountered
		PARSER_IN_PROCESSING_INSTRUCTION_TARGET,	// <?		encountered
		PARSER_IN_PROCESSING_INSTRUCTION,			// <?...S	encountered
		PARSER_IN_ELEMENT_START_NAME,				// <...		encountered
		PARSER_IN_ELEMENT_START_TAG,
		PARSER_IN_ELEMENT_END_TAG,
		PARSER_IN_CDATA,
		PARSER_IN_COMMENT,
		PARSER_IN_DOCTYPE,
		PARSER_IN_ELEMENT_DECL,
		PARSER_IN_ATTLIST_DECL,
		PARSER_IN_ENTITY_DECL,
		PARSER_IN_NOTATION_DECL,
		PARSER_IN_GE_REF,
		PARSER_IN_PE_REF
	}parser_state;


	// =====================================================================
	typedef enum
	{
		PARSER_NORMAL,
		PARSER_NEARING_END_1
	}forward_looking_state;


	// =====================================================================
	typedef enum
	{
		PARSER_SUB_IN_UNKNOWN,
		PARSER_SUB_IN_WHITESPACE,
		PARSER_SUB_IN_NAME,
		PARSER_SUB_READY_FOR_VALUE,
		PARSER_SUB_IN_VALUE,
		PARSER_SUB_IN_ELEMENT,
		PARSER_SUB_IN_READY_FOR_WS_1,
		PARSER_SUB_IN_WHITESPACE_1,
		PARSER_SUB_IN_WHITESPACE_2,
		PARSER_SUB_IN_WHITESPACE_3,
		PARSER_SUB_IN_WHITESPACE_4,
		PARSER_SUB_IN_WHITESPACE_5,
		PARSER_SUB_IN_WHITESPACE_7,
		PARSER_SUB_IN_WHITESPACE_8,
		PARSER_SUB_IN_WHITESPACE_9,
		PARSER_SUB_IN_WHITESPACE_10,
		PARSER_SUB_IN_DOCTYPE_NAME,
		PARSER_SUB_IN_EXTERNAL_ID,
		PARSER_SUB_IN_INTERNAL_PARSED_VALUE,
		PARSER_SUB_IN_READIING_NDATA,
		PARSER_SUB_IN_READING_PUBLIC_ID,
		PARSER_SUB_IN_READING_SYSTEM_ID,
		PARSER_SUB_IN_SPEC,
		PARSER_SUB_IN_READING_NOTATION,
		PARSER_SUB_IN_ENTITY_REF
	}parser_sub_state;


	// Private methods.
	void initializeState();
	void finalizeState();

	status_t handle_attribute_decl(SString & element, SString & data);
	status_t handle_entity_decl(bool parameter, SString & name, SString & value, uint32_t flags, bool doctypeBeginOnly);
	status_t handle_element_start(SString & name, SValue & attributes, uint32_t flags);
	status_t expand_char_ref(const SString & entity, SString & entityVal);
	status_t	expand_char_refs(SString & str);
	status_t expand_entities(SString & str, char delimiter);



	// Members
protected:
	BXMLParseContext * m_context;
private:
	bool m_dtdOnly;
	uint32_t m_flags;
	// Characters remaining from the last iteration
	uint8_t	* m_remainingChars;
	int32_t	m_remainingCharsSize;
	
	uint8_t	* m_parseText;
	int32_t	m_parseTextLength;
	
	int32_t	m_characterSize;
	
	parser_state			m_state;
	forward_looking_state	m_upcomming;
	parser_sub_state		m_subState;
	
	// The current token, until it has been completed, and we're ready to
	// move on to the next one.
	SString		m_currentToken;
	
	// Current Name meaning element, target, notation, or any of those other thigns
	SString		m_currentName;
	SString		m_savedName;
	SString		m_currentSubName;
	SString		m_entityValue;
	uint8_t		m_delimiter;
	
	// Mapping of name/values.  Use for attributes, everything.
	SValue		m_stringMap;

	uint8_t		* m_longStringData ;
	uint8_t		m_carryoverLongData[4];
	uint8_t		m_someChars[3];
	
	bool		m_inDTD;
	decl_type	m_declType;
	
	SString		m_emptyString;


};

#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif

#endif // #define _XMLParserCore_P_H
