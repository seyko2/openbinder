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

#ifndef _SUPPORT_STATIC_VALUE_H_
#define _SUPPORT_STATIC_VALUE_H_

/*!	@file support/StaticValue.h
	@ingroup CoreSupportUtilities
	@brief Optimized representations of static SValue constants.
*/

#include <support/Debug.h>
#include <support/SupportDefs.h>
#include <support/SharedBuffer.h>
#include <support/TypeConstants.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SString;
class SValue;

// the extra cast to void* stops the stu-pid ADS warning:
// Warning: C2354W: cast from ptr/ref to 'class SValue' to ptr/ref to 'struct static_large_value'; one is undefined, assuming unrelated

// These functions are provided by the Binder Glue static code.  You must
// link against it to get them; don't implement them yourself.
extern "C" void palmsource_inc_package_ref();
extern "C" void palmsource_dec_package_ref();

//	--------------------------------------------------------------------

/*!	This is a static representation of a SValue containing 4 or fewer bytes
	of data (with this data placed in-line), which can be used to create
	constant values for placement in the text section of an executable. */
struct static_small_value
{
	uint32_t		type;
	char			data[4];
	
	inline operator const SValue&() const { return *(const SValue*)(void*)this; }
};

/*!	This is a static representation of a SValue containing more than 4 bytes
	of data (pointing to its data out-of-line), which can be used to create
	constant values for placement in the text section of an executable. */
struct static_large_value
{
	uint32_t		type;
	const void*	data;
	
	inline operator const SValue&() const { return *(const SValue*)(void*)this; }
};

/*!	Special version of static_small_value for a value holding a string. */
struct static_small_string_value
{
	uint32_t		type;
	char			data[4];
	const void*	data_str;
	
	inline operator const SString&() const { return *(const SString*)(void*)&data_str; }
	inline operator const SValue&() const { return *(const SValue*)(void*)this; }
};

/*!	Special version of static_small_value for a value holding a string. */
struct static_large_string_value
{
	uint32_t		type;
	const void*	data;
	const void*	data_str;		// XXX could change SString to point to the SSharedBuffer
	
	inline operator const SString&() const { return *(const SString*)(void*)&data_str; }
	inline operator const SValue&() const { return *(const SValue*)(void*)this; }
};

/*!	This is a static representation of a SValue containing a 32-bit
	integer, which can be used to create constant values for placement
	in the text section of an executable. */
struct static_int32_value
{
	uint32_t	type;
	int32_t		value;
	
	inline operator const SValue&() const { return *(const SValue*)(void*)this; }
};

/*!	This is a static representation of a SValue containing a 32-bit
	float, which can be used to create constant values for placement
	in the text section of an executable. */
struct static_float_value
{
	uint32_t	type;
	float		value;
	
	inline operator const SValue&() const { return *(const SValue*)(void*)this; }
};

/*!	This is a static representation of a SValue containing a boolean,
	which can be used to create constant values for placement
	in the text section of an executable. */
struct static_bool_value
{
	uint32_t	type;
	// The data is not a real bool type because the size of bool
	// changes between compilers.  (For example, in ADS it is
	// 4 bytes.)
	int8_t		value;
	
	inline operator const SValue&() const { return *(const SValue*)(void*)this; }
};

//	--------------------------------------------------------------------
// These typedefs should go away, but they are used by pidgen currently.

typedef const static_small_string_value	value_csml;
typedef const static_large_string_value	value_clrg;
typedef const static_int32_value		value_cint32;
typedef const static_float_value		value_cfloat;
typedef const static_bool_value			value_cbool;

//	--------------------------------------------------------------------

#define STRING_ASSERT(x) inline void string_assert() { STATIC_ASSERT(x); }
#define PADDED_STRING_LENGTH(string) sizeof(string)+((4-(sizeof(string)%4)) & 0x3)

//!	Convenience macro for making a static SValue containing a string of 4 or fewer (including the terminating \\0) characters.
#define B_CONST_STRING_VALUE_SMALL(ident, string, prefix)				\
	const struct {														\
		STRING_ASSERT(sizeof(string)<=4);								\
		SSharedBuffer::inc_ref_func	incFunc;							\
		SSharedBuffer::dec_ref_func	decFunc;							\
		int32_t		users;												\
		size_t		length;												\
		char		data[PADDED_STRING_LENGTH(string)];					\
	} ident##str = {													\
		&palmsource_inc_package_ref,									\
		&palmsource_dec_package_ref,									\
		B_STATIC_USERS,													\
		sizeof(string)<<B_BUFFER_LENGTH_SHIFT, string					\
	};																	\
	const static_small_string_value prefix##ident = {					\
		B_PACK_SMALL_TYPE(B_STRING_TYPE, sizeof(string)), string,		\
		&ident##str.data												\
	};																	\

//!	Convenience macro for making a static SValue containing a string of	4 or fewer (including the terminating \\0) characters.
#define B_STATIC_STRING_VALUE_SMALL(ident, string, prefix)				\
	static const struct {												\
		STRING_ASSERT(sizeof(string)<=4);								\
		SSharedBuffer::inc_ref_func	incFunc;							\
		SSharedBuffer::dec_ref_func	decFunc;							\
		int32_t		users;												\
		size_t		length;												\
		char		data[PADDED_STRING_LENGTH(string)];					\
	} ident##str = {													\
		&palmsource_inc_package_ref,									\
		&palmsource_dec_package_ref,									\
		B_STATIC_USERS,													\
		sizeof(string)<<B_BUFFER_LENGTH_SHIFT, string					\
	};																	\
	static const static_small_string_value prefix##ident = {			\
		B_PACK_SMALL_TYPE(B_STRING_TYPE, sizeof(string)), string,		\
		&ident##str.data												\
	};																	\

//!	Convenience macro for making a static SValue containing a string of	5 or more (including the terminating \\0) characters.
#define B_CONST_STRING_VALUE_LARGE(ident, string, prefix)				\
	const struct {														\
		STRING_ASSERT(sizeof(string)>4);								\
		SSharedBuffer::inc_ref_func	incFunc;							\
		SSharedBuffer::dec_ref_func	decFunc;							\
		int32_t		users;												\
		size_t		length;												\
		char		data[PADDED_STRING_LENGTH(string)];					\
	} ident##str = {													\
		&palmsource_inc_package_ref,									\
		&palmsource_dec_package_ref,									\
		B_STATIC_USERS,													\
		sizeof(string)<<B_BUFFER_LENGTH_SHIFT, string					\
	};																	\
	const static_large_string_value prefix##ident = {					\
		B_PACK_LARGE_TYPE(B_STRING_TYPE), &ident##str.users,			\
		&ident##str.data												\
	};																	\

//!	Convenience macro for making a static SValue containing a string of	5 or more (including the terminating \\0) characters.
#define B_STATIC_STRING_VALUE_LARGE(ident, string, prefix)				\
	static const struct {												\
		STRING_ASSERT(sizeof(string)>4);								\
		SSharedBuffer::inc_ref_func	incFunc;							\
		SSharedBuffer::dec_ref_func	decFunc;							\
		int32_t		users;												\
		size_t		length;												\
		char		data[PADDED_STRING_LENGTH(string)];					\
	} ident##str = {													\
		&palmsource_inc_package_ref,									\
		&palmsource_dec_package_ref,									\
		B_STATIC_USERS,													\
		sizeof(string)<<B_BUFFER_LENGTH_SHIFT, string					\
	};																	\
	static const static_large_string_value prefix##ident = {			\
		B_PACK_LARGE_TYPE(B_STRING_TYPE), &ident##str.users,			\
		&ident##str.data												\
	};																	\

//!	Convenience macro for making a static SValue containing an int32_t.
#define B_CONST_INT32_VALUE(ident, val, prefix)							\
	const static_int32_value prefix##ident = {							\
		B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)), val			\
	};																	\

//!	Convenience macro for making a static SValue containing an int32_t.
#define B_STATIC_INT32_VALUE(ident, val, prefix)						\
	static const static_int32_value prefix##ident = {					\
		B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)), val			\
	};																	\

//!	Convenience macro for making a static SValue containing a float.
#define B_CONST_FLOAT_VALUE(ident, val, prefix)							\
	const static_float_value prefix##ident = {							\
		B_PACK_SMALL_TYPE(B_FLOAT_TYPE, sizeof(float)), val			\
	};																	\

//!	Convenience macro for making a static SValue containing a float.
#define B_STATIC_FLOAT_VALUE(ident, val, prefix)						\
	static const static_float_value prefix##ident = {					\
		B_PACK_SMALL_TYPE(B_FLOAT_TYPE, sizeof(float)), val			\
	};																	\

/*!	@} */

// Compatibility.  Don't use!
#define B_CONST_STRING_VALUE_4 B_CONST_STRING_VALUE_SMALL
#define B_STATIC_STRING_VALUE_4 B_STATIC_STRING_VALUE_SMALL
#define B_CONST_STRING_VALUE_8 B_CONST_STRING_VALUE_LARGE
#define B_STATIC_STRING_VALUE_8 B_STATIC_STRING_VALUE_LARGE
#define B_CONST_STRING_VALUE_12 B_CONST_STRING_VALUE_LARGE
#define B_STATIC_STRING_VALUE_12 B_STATIC_STRING_VALUE_LARGE
#define B_CONST_STRING_VALUE_16 B_CONST_STRING_VALUE_LARGE
#define B_STATIC_STRING_VALUE_16 B_STATIC_STRING_VALUE_LARGE
#define B_CONST_STRING_VALUE_20 B_CONST_STRING_VALUE_LARGE
#define B_STATIC_STRING_VALUE_20 B_STATIC_STRING_VALUE_LARGE
#define B_CONST_STRING_VALUE_24 B_CONST_STRING_VALUE_LARGE
#define B_STATIC_STRING_VALUE_24 B_STATIC_STRING_VALUE_LARGE
#define B_CONST_STRING_VALUE_28 B_CONST_STRING_VALUE_LARGE
#define B_STATIC_STRING_VALUE_28 B_STATIC_STRING_VALUE_LARGE
#define B_CONST_STRING_VALUE_32 B_CONST_STRING_VALUE_LARGE
#define B_STATIC_STRING_VALUE_32 B_STATIC_STRING_VALUE_LARGE
#define B_CONST_STRING_VALUE_36 B_CONST_STRING_VALUE_LARGE
#define B_STATIC_STRING_VALUE_36 B_STATIC_STRING_VALUE_LARGE
#define B_CONST_STRING_VALUE_40 B_CONST_STRING_VALUE_LARGE
#define B_STATIC_STRING_VALUE_40 B_STATIC_STRING_VALUE_LARGE
#define B_CONST_STRING_VALUE_44 B_CONST_STRING_VALUE_LARGE
#define B_STATIC_STRING_VALUE_44 B_STATIC_STRING_VALUE_LARGE
#define B_STATIC_STRING	B_STATIC_STRING_VALUE_LARGE
#define B_CONST_STRING	B_CONST_STRING_VALUE_LARGE

//	--------------------------------------------------------------------

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif	/* _SUPPORT_STATIC_VALUE_H_ */
