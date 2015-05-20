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

#include <xml/StringUtils.h>
#include <support/Value.h>
#include <ctype.h>
#include <stdio.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif

// Return the next string in str, starting after pos, in split.
// split will start in the next character after any whitespace happening
// at pos, and continue up until, but not including any whitespace.
// =====================================================================
bool
SplitStringOnWhitespace(const SString & str, SString & split, int32_t * pos)
{
	if (*pos >= str.Length())
		return false;
	
	int count = 0;
	const char * p = str.String() + *pos;
	
	// Use up any preceeding whitespace
	while (*p && isspace(*p))
		p++;
	if (*p == '\0')
		return false;
		
	const char * s = p;
	
	// Go until the end
	while (*p && !isspace(*p))
	{
		count++;
		p++;
	}
	
	split.SetTo(s, count);
	*pos = p - str.String();
	return true;
}



#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif
