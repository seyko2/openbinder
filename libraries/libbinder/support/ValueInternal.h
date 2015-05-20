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

// Part of the SValue implementation that BValueMap would like to use.
#ifndef _VALUEINTERNAL_H_
#define _VALUEINTERNAL_H_

#include <support/Value.h>
#include <support/SharedBuffer.h>

#include <math.h>
#include <ctype.h>
#include <float.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

// Some useful compound type constants.

enum {
	// Note -- undefined is 0, NOT a valid packed type.
	kUndefinedTypeCode = B_UNDEFINED_TYPE,

	kWildTypeCode = B_PACK_SMALL_TYPE(B_WILD_TYPE, 0),
	kNullTypeCode = B_PACK_SMALL_TYPE(B_NULL_TYPE, 0),
	kErrorTypeCode = B_PACK_SMALL_TYPE(B_ERROR_TYPE, 4),
	kMapTypeCode = B_TYPE_BYTEORDER_NORMAL | B_TYPE_LENGTH_MAP | B_VALUE_TYPE
};

#define CHECK_IS_SMALL_OBJECT(type) (((type)&(B_TYPE_LENGTH_MASK|0x00007f00)) == (sizeof(void*)|('*'<<B_TYPE_CODE_SHIFT)))
#define CHECK_IS_LARGE_OBJECT(type) (((type)&(B_TYPE_LENGTH_MASK|0x00007f00)) == (B_TYPE_LENGTH_LARGE|('*'<<B_TYPE_CODE_SHIFT)))

#define VALIDATE_TYPE(type) DbgOnlyFatalErrorIf(((type)&~B_TYPE_CODE_MASK) != 0, "Type codes can only use bits 0x7f7f7f00!");

// Low-level inline funcs.

inline bool SValue::is_defined() const
{
	return m_type != kUndefinedTypeCode;
}
inline bool SValue::is_wild() const
{
	return m_type == kWildTypeCode;
}
inline bool SValue::is_null() const
{
	return m_type == kNullTypeCode;
}
inline bool SValue::is_error() const
{
	return m_type == kErrorTypeCode;
}
inline bool SValue::is_final() const
{
	return is_wild() || is_error();
}
inline bool SValue::is_simple() const
{
	return B_UNPACK_TYPE_LENGTH(m_type) != B_TYPE_LENGTH_MAP;
}
inline bool SValue::is_map() const
{
	return B_UNPACK_TYPE_LENGTH(m_type) == B_TYPE_LENGTH_MAP;
}
inline bool SValue::is_object() const
{
	return CHECK_IS_SMALL_OBJECT(m_type);
}
inline bool SValue::is_specified() const
{
	return is_defined() && !is_wild();
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif /* _VALUEINTERNAL_H_ */
