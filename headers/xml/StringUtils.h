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

#ifndef _B_XML2_STRINGUTILS_H
#define _B_XML2_STRINGUTILS_H

#include <support/Vector.h>
#include <support/String.h>
#include <support/SupportDefs.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace support;
#endif

// Functions
// =====================================================================
// Functions that do some fun stuff to strings
bool SplitStringOnWhitespace(const SString & str, SString & split, int32_t * pos);

#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif

#endif // _B_XML2_STRINGUTILS_H
