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

#ifndef _SUPPORT_STRINGTOKENIZER_H
#define _SUPPORT_STRINGTOKENIZER_H

/*!	@file support/StringTokenizer.h
	@ingroup CoreSupportUtilities
	@brief Helper for parsing strings.
*/

#include <support/RegExp.h>
#include <support/String.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SStringTokenizer
{
public:
	SStringTokenizer(const SString& string, const SString& delimeters);
	SStringTokenizer(const SString& string, const char* delimeters = " \t\n\r\f");
	SStringTokenizer(const char* string, const char* delimeters = " \t\n\r\f");

	int32_t CountTokens();
	bool HasMoreTokens();
	SString NextToken();
	SString NextToken(const SString& delimeters);
	
private:
	void RegularExpressionize();

	SRegExp m_regexp;
	SString m_string;
	SString m_delimeters;
	int32_t m_position;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // palmos::support
#endif

#endif // _SUPPORT_STRINGTOKENIZER_H
