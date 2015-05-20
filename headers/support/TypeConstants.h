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

#ifndef _SUPPORT_TYPECONSTANTS_H
#define _SUPPORT_TYPECONSTANTS_H

/*!	@file support/TypeConstants.h
	@ingroup CoreSupportUtilities
	@brief Format and standard definitions of SValue type codes.
*/

#ifdef __cplusplus
#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/*-------------------------------------------------------------*/
/*----- Data Types --------------------------------------------*/

/*!	@name Type Code Definitions
	Type codes are 32-bit integers.  The upper 24 bits are the
	the code, and the lower 8 bits are metadata.  The code is
	constructed as 3 characters.  Codes containing only the
	characters a-z and _, and codes whose last letter is not
	alphabetic (a-zA-Z), are reserved for use by the system.
	Type codes that end with the character '*' contain
	pointers to external objects.
	Type codes that end with the character '#' are in a special
	namespace reserved for SDimth units. */
//@{

//!	Type code manipulation.
enum {
	B_TYPE_CODE_MASK			= 0x7f7f7f00,	// Usable bits for the type code
	B_TYPE_CODE_SHIFT			= 8,			// Where code appears.

	B_TYPE_LENGTH_MASK			= 0x00000007,	// Usable bits for the data length
	B_TYPE_LENGTH_MAX			= 0x00000004,	// Largest length that can be encoded in type
	B_TYPE_LENGTH_LARGE			= 0x00000005,	// Value when length is > 4 bytes
	B_TYPE_LENGTH_MAP			= 0x00000007,	// For use by SValue

	B_TYPE_BYTEORDER_MASK		= 0x80000080,	// Bits used to check byte order
	B_TYPE_BYTEORDER_NORMAL		= 0x00000080,	// This bit is set if the byte order is okay
	B_TYPE_BYTEORDER_SWAPPED	= 0x80000000	// This bit is set if the byte order is swapped
};

//! Pack a small (size <= B_TYPE_LENGTH_MAX) type code from its constituent parts.
#define B_PACK_SMALL_TYPE(code, length)			(((code)&B_TYPE_CODE_MASK) | (length) | B_TYPE_BYTEORDER_NORMAL)
//! Pack a large (size > B_TYPE_LENGTH_MAX) type code from its constituent parts.
#define B_PACK_LARGE_TYPE(code)					(((code)&B_TYPE_CODE_MASK) | B_TYPE_LENGTH_LARGE | B_TYPE_BYTEORDER_NORMAL)
//!	Retrieve type information from a packed type code.
#define B_UNPACK_TYPE_CODE(type)				((type)&B_TYPE_CODE_MASK)
//!	Retrieve size information from a packaed type code.
#define B_UNPACK_TYPE_LENGTH(type)				((type)&B_TYPE_LENGTH_MASK)

//!	Build a valid code for a type code.
/*!	Ensures only correct bits are used, and shifts value into correct location. */
#define B_TYPE_CODE(code)						(((code)<<B_TYPE_CODE_SHIFT)&B_TYPE_CODE_MASK)

//!	Standard type codes.
enum {
	// Special code to match any type code in comparisons.
	B_ANY_TYPE 					= B_TYPE_CODE('any'),
	
	// Built-in value types.
	B_UNDEFINED_TYPE			= B_TYPE_CODE(0),
	B_WILD_TYPE					= B_TYPE_CODE('wld'),
	B_NULL_TYPE					= B_TYPE_CODE('nul'),
	B_VALUE_TYPE				= B_TYPE_CODE('val'),
	B_SYSTEM_TYPE				= B_TYPE_CODE('sys'),
	B_FIXED_ARRAY_TYPE			= B_TYPE_CODE('arf'),
	B_VARIABLE_ARRAY_TYPE		= B_TYPE_CODE('arv'),
	B_ERROR_TYPE				= B_TYPE_CODE('err'),
	
	// Object types.
	B_BINDER_TYPE				= B_TYPE_CODE('sb*'),
	B_BINDER_WEAK_TYPE			= B_TYPE_CODE('wb*'),
	B_BINDER_HANDLE_TYPE		= B_TYPE_CODE('sh*'),
	B_BINDER_WEAK_HANDLE_TYPE	= B_TYPE_CODE('wh*'),
	B_BINDER_NODE_TYPE			= B_TYPE_CODE('sn*'),
	B_BINDER_WEAK_NODE_TYPE		= B_TYPE_CODE('wn*'),
	B_ATOM_TYPE 				= B_TYPE_CODE('sa*'),
	B_ATOM_WEAK_TYPE 			= B_TYPE_CODE('wa*'),
	B_KEY_ID_TYPE				= B_TYPE_CODE('ky*'),
	
	// General types.
	B_BOOL_TYPE 				= B_TYPE_CODE('bol'),	// Support Kit
	B_INT8_TYPE 				= B_TYPE_CODE('i08'),
	B_INT16_TYPE 				= B_TYPE_CODE('i16'),
	B_INT32_TYPE 				= B_TYPE_CODE('i32'),
	B_INT64_TYPE 				= B_TYPE_CODE('i64'),
	B_FLOAT_TYPE 				= B_TYPE_CODE('flt'),
	B_DOUBLE_TYPE 				= B_TYPE_CODE('dbl'),
	B_STRING_TYPE 				= B_TYPE_CODE('str'),
	B_BIGTIME_TYPE 				= B_TYPE_CODE('btm'),
	B_NSECS_TYPE				= B_TYPE_CODE('nst'),
	B_URL_TYPE					= B_TYPE_CODE('url'),
	B_TIMEZONE_TYPE				= B_TYPE_CODE('zon'),
	B_ENCODED_TEXT_TYPE			= B_TYPE_CODE('etx'),
	B_STATUS_TYPE				= B_TYPE_CODE('sts'),
	B_RAW_TYPE 					= B_TYPE_CODE('raw'),
	B_BASE64_TYPE 				= B_TYPE_CODE('bas'),
	B_PACKAGE_TYPE				= B_TYPE_CODE('pkg'),

	B_POINT_TYPE 				= B_TYPE_CODE('pnt'),	// Render Kit
	B_RECT_TYPE 				= B_TYPE_CODE('rct'),
	B_INSETS_TYPE 				= B_TYPE_CODE('ist'),
	B_COLOR_32_TYPE 			= B_TYPE_CODE('icl'),
	B_COLOR_TYPE				= B_TYPE_CODE('fcl'),
	B_REGION_TYPE				= B_TYPE_CODE('rgn'),
	B_SREGION_TYPE				= B_TYPE_CODE('srg'),
	B_TRANSFORM_2D_TYPE			= B_TYPE_CODE('t2d'),
	B_TRANSFORM_COLOR_TYPE		= B_TYPE_CODE('tcl'),
	B_GRADIENT_TYPE				= B_TYPE_CODE('grd'),
	B_DIMTH_TYPE				= B_TYPE_CODE('dmt'),
	B_FONT_TYPE					= B_TYPE_CODE('fnt'),
	B_FONT_HEIGHT_TYPE			= B_TYPE_CODE('fgt'),
	B_GLYPH_MAP_TYPE			= B_TYPE_CODE('gmp'),
	B_PIXMAP_TYPE				= B_TYPE_CODE('pix'),
	B_BITMAP_TYPE				= B_TYPE_CODE('bmp'),
	B_RASTER_POINT_TYPE			= B_TYPE_CODE('rpt'),
	B_RASTER_RECT_TYPE 			= B_TYPE_CODE('rrt'),
	B_RASTER_REGION_TYPE		= B_TYPE_CODE('rrg'),
	B_SRASTER_REGION_TYPE		= B_TYPE_CODE('srr'),
	B_PALETTE_TYPE				= B_TYPE_CODE('pal'),
	B_SPATH_TYPE				= B_TYPE_CODE('spt'),

	B_UPDATE_TYPE				= B_TYPE_CODE('upd'),
	B_SUPDATE_TYPE				= B_TYPE_CODE('Upd'),
	B_CONSTRAINT_AXIS_TYPE		= B_TYPE_CODE('cax'),	// View Kit

	B_UUID_TYPE					= B_TYPE_CODE('uid'),

	// Questionable types.
	B_CHAR_TYPE 				= B_TYPE_CODE('chr'),
	B_CONSTCHAR_TYPE			= B_TYPE_CODE('cch'),
	B_WCHAR_TYPE				= B_TYPE_CODE('wch'),
	B_MIME_TYPE					= B_TYPE_CODE('mim'),
	B_OFF_T_TYPE 				= B_TYPE_CODE('oft'),
	B_SIZE_T_TYPE	 			= B_TYPE_CODE('szt'),
	B_SSIZE_T_TYPE	 			= B_TYPE_CODE('sst'),
	B_TIME_TYPE 				= B_TYPE_CODE('tim'),
	B_UINT64_TYPE				= B_TYPE_CODE('u64'),
	B_UINT32_TYPE				= B_TYPE_CODE('u32'),
	B_UINT16_TYPE 				= B_TYPE_CODE('u16'),
	B_UINT8_TYPE 				= B_TYPE_CODE('u08')
};

/*-------------------------------------------------------------*/

/*!	@} */

#ifdef __cplusplus
#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif

#endif /* _SUPPORT_TYPECONSTANTS_H */
