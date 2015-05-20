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

#ifndef _SUPPORTKIT_STRINGUTILS_H_INCLUDED_
#define _SUPPORTKIT_STRINGUTILS_H_INCLUDED_

/*!	@file support/StringUtils.h
	@ingroup CoreSupportUtilities
	@brief Additional string utilities.
*/

#include <support/String.h>
#include <support/Vector.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/*!	@name String Utilities */
//@{

            //! Fills the strList vector with the list of string created by splitting the string on splitOn.
            /*! Splits the string into a vector of string that are seperated by the splitOn string.  If the
                append parameter is set to true then the strList parameter will be appended.  If the append
                parameter is false then MakeEmpty is call on strList before it is filled with the split apart
                strings.
                */
status_t    StringSplit(const SString& srcStr, const SString& splitOn, SVector<SString>* strList, bool append = false);

            //! Fills the strList vector with the list of string created by splitting the string on splitOn.
            /*! Splits the string into a vector of strings that are seperated by the splitOn string.  If the
                append parameter is set to true then the strList parameter will be appended.  If the append
                parameter is false then MakeEmpty is call on strList before it is filled with the split apart
                strings.  The splitOnLen parameter is the length of the splitOn string not including the
                \0 null terminating zero.  You can use strlen to get the correct length to pass for this 
                parameter.
                */
status_t    StringSplit(const char* srcStr, int32_t srcStrLen, const char* splitOn, int32_t splitOnLen, SVector<SString>* strList, bool append = false);

//@}

/*!	@} */

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif //_SUPPORTKIT_STRINGUTILS_H_INCLUDED_