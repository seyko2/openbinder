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

#ifndef _SUPPORT_VALUEMAPFORMAT_H
#define _SUPPORT_VALUEMAPFORMAT_H

#include <support/SupportDefs.h>
#include <support/TypeConstants.h>

#if _SUPPORTS_NAMESPACE
#if defined(__cplusplus)
namespace palmos {
namespace support {
#endif /* __cplusplus */
#endif /* _SUPPORTS_NAMESPACE */

// Perform alignment -- to 8 byte boundaries -- of value data.
#if defined(__cplusplus)
static inline uint32_t value_data_align(const uint32_t size) { return ((size+0x7)&~0x7); }
#endif /* defined(_cplusplus) */

// This is the structure of flattened data for small (<= 4 bytes)
// sizes.  Note that the minimum size is 8 bytes, even if the data
// is empty!  Also note that this very intentionally looks exactly
// like the private data inside of an SValue.
struct small_flat_data
{
	uint32_t			type;
	union {
		uint8_t			bytes[4];
		int32_t			integer;
		uint32_t		uinteger;
		void*			object;
	}					data;
};

// This is the structure of flattened data for large (> 4 bytes)
// sizes.  The type length must be B_TYPE_LENGTH_LARGE, and the
// length field here contains the actual amount of data that
// follows.
struct large_flat_header
{
	uint32_t			type;
	uint32_t			length;
};
struct large_flat_data
{
	large_flat_header	header;
	uint8_t				data[1];
};

// Basic values are written as either raw small_flat_data or
// large_flat_data structures, depending on their size.
//
// If the value is a map (B_VALUE_TYPE), then it is written
// as a value_map_header followed by a concatenation of each
// key/value pair in the value.  A value_map_header itself
// is simply a largeg_flat_header followed by some additional
// information about the map.
struct value_map_info
{
	size_t	count;		// number of key/value pairs that follow.
	int32_t	order;		// 0 if unordered, 1 for order v1.
};
struct value_map_header
{
	large_flat_header	header;
	value_map_info		info;
};

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

#if _SUPPORTS_NAMESPACE
#if defined(__cplusplus)
} }	// namespace palmos::support
#endif /* __cplusplus */
#endif /* _SUPPORTS_NAMESPACE */

#endif /* _SUPPORT_VALUEMAPFORMAT_H */
