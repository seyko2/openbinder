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

#include <support/StringTokenizer.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

SStringTokenizer::SStringTokenizer(const SString& string, const SString& delimeters)
	:	m_string(string),
		m_delimeters(delimeters),
		m_position(0)
{
	RegularExpressionize();
}

SStringTokenizer::SStringTokenizer(const SString& string, const char* delimeters)
	:	m_string(string),
		m_delimeters(delimeters),
		m_position(0)
{
	RegularExpressionize();
}

SStringTokenizer::SStringTokenizer(const char* string, const char* delimeters)
	:	m_string(string),
		m_delimeters(delimeters),
		m_position(0)
{
	RegularExpressionize();
}

inline void SStringTokenizer::RegularExpressionize()
{
	SString exp = m_delimeters;
	exp.Prepend("[^");
	exp.Append("]+");
	m_regexp.SetTo(exp);
}

int32_t SStringTokenizer::CountTokens()
{
	int32_t start = 0;
	int32_t end = 0;
	int32_t tokens = 0;

	while (m_regexp.Search(m_string, m_position, &start, &end))
	{
		tokens++;
	}

	return tokens;
}

bool SStringTokenizer::HasMoreTokens()
{
	int32_t start;
	int32_t end;
	return m_regexp.Search(m_string, m_position, &start, &end);
}

SString SStringTokenizer::NextToken()
{
	int32_t start;
	int32_t end;

	bool found = m_regexp.Search(m_string, m_position, &start, &end);

	SString token("");
	if (found)
	{
		char* string = (char*)m_string.String();
		string += start;
		token.SetTo(string, (end - start));
		m_position = end;
	}
		
	return token;
}

SString SStringTokenizer::NextToken(const SString& delimeters)
{
	m_delimeters = delimeters;
	RegularExpressionize();
	return NextToken();
}
#if _SUPPORTS_NAMESPACE
} } // palmos::support
#endif

