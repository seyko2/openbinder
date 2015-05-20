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

#ifndef _SUPPORT_SWAPPED_VALUE_H
#define _SUPPORT_SWAPPED_VALUE_H

/*!	@file support/SwappedValue.h
	@ingroup CoreSupportUtilities
	@brief Add data to a value that is tagged with endianness
	and pull it out and know if you need to swap.
*/

#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

// returns a SValue that is the data and an endianness
status_t create_swappable_value(uint32_t typeCode, void *data, size_t size, SValue *value);

// returns whether you should swap or not
bool get_swappable_data(const SValue &value, void **data, size_t *size);

/*!	@} */

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::render
#endif

#endif // _SUPPORT_SWAPPED_VALUE_H
