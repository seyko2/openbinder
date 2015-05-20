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

#ifndef _SUPPORT_BYTEORDER_H
#define _SUPPORT_BYTEORDER_H
#define _BYTEORDER_H

/*!	@file support/ByteOrder.h
	@ingroup CoreSupportUtilities
	@brief Definitions and routines for managing byte order differences.
*/

#include <support/SupportDefs.h>
#include <support/TypeConstants.h>	/* For convenience */

/*!	@addtogroup CoreSupportUtilities
	@{
*/

#ifdef __cplusplus
extern "C" {
#endif 

/*!	@name Byte Order Functions and Macros */
//@{

/*----------------------------------------------------------------------*/
/*------- Swap-direction constants, and swapping functions -------------*/

typedef enum {
	B_SWAP_HOST_TO_LENDIAN,
	B_SWAP_HOST_TO_BENDIAN,
	B_SWAP_LENDIAN_TO_HOST,
	B_SWAP_BENDIAN_TO_HOST,
	B_SWAP_ALWAYS
} swap_action;

extern _IMPEXP_SUPPORT status_t swap_data(	type_code type,
							void *data,
							size_t length,
							swap_action action);

extern _IMPEXP_SUPPORT int is_type_swapped(type_code type);
 
 
/*-----------------------------------------------------------------------*/
/*----- Private implementations -----------------------------------------*/

double _swap_double(double arg);
float  _swap_float(float arg);
uint64_t _swap_int64(uint64_t uarg);
uint32_t _swap_int32(uint32_t uarg);

/* This one is now always inlined */
INLINE_FNC uint16_t _swap_int16(uint16_t uarg)
{
	return (uarg>>8)|((uarg&0xFF)<<8);
}

/*-------------------------------------------------------------*/
/*--------- Enforced Swapping  --------------------------------*/

#define B_BYTE_SWAP_DOUBLE(arg)			_swap_double(arg)
#define B_BYTE_SWAP_FLOAT(arg)			_swap_float(arg)
#define B_BYTE_SWAP_INT64(arg)			_swap_int64(arg)
#define B_BYTE_SWAP_INT32(arg)			_swap_int32(arg)
#define B_BYTE_SWAP_INT16(arg)			_swap_int16(arg)

/*-------------------------------------------------------------*/
/*--------- Host is Little  -----------------------------------*/

#if BYTE_ORDER == LITTLE_ENDIAN
#define B_HOST_IS_LENDIAN 1
#define B_HOST_IS_BENDIAN 0

/*--------- Host Native -> Little  --------------------*/

#define B_HOST_TO_LENDIAN_DOUBLE(arg)	(double)(arg)
#define B_HOST_TO_LENDIAN_FLOAT(arg)	(float)(arg)
#define B_HOST_TO_LENDIAN_INT64(arg)	(uint64_t)(arg)
#define B_HOST_TO_LENDIAN_INT32(arg)	(uint32_t)(arg)
#define B_HOST_TO_LENDIAN_INT16(arg)	(uint16_t)(arg)

/*--------- Host Native -> Big  ------------------------*/
#define B_HOST_TO_BENDIAN_DOUBLE(arg)	_swap_double(arg)
#define B_HOST_TO_BENDIAN_FLOAT(arg)	_swap_float(arg)
#define B_HOST_TO_BENDIAN_INT64(arg)	_swap_int64(arg)
#define B_HOST_TO_BENDIAN_INT32(arg)	_swap_int32(arg)
#define B_HOST_TO_BENDIAN_INT16(arg)	_swap_int16(arg)

/*--------- Little -> Host Native ---------------------*/
#define B_LENDIAN_TO_HOST_DOUBLE(arg)	(double)(arg)
#define B_LENDIAN_TO_HOST_FLOAT(arg)	(float)(arg)
#define B_LENDIAN_TO_HOST_INT64(arg)	(uint64_t)(arg)
#define B_LENDIAN_TO_HOST_INT32(arg)	(uint32_t)(arg)
#define B_LENDIAN_TO_HOST_INT16(arg)	(uint16_t)(arg)

/*--------- Big -> Host Native ------------------------*/
#define B_BENDIAN_TO_HOST_DOUBLE(arg)	_swap_double(arg)
#define B_BENDIAN_TO_HOST_FLOAT(arg)	_swap_float(arg)
#define B_BENDIAN_TO_HOST_INT64(arg)	_swap_int64(arg)
#define B_BENDIAN_TO_HOST_INT32(arg)	_swap_int32(arg)
#define B_BENDIAN_TO_HOST_INT16(arg)	_swap_int16(arg)

#else /* __LITTLE_ENDIAN */


/*-------------------------------------------------------------*/
/*--------- Host is Big  --------------------------------------*/

#define B_HOST_IS_LENDIAN 0
#define B_HOST_IS_BENDIAN 1

/*--------- Host Native -> Little  -------------------*/
#define B_HOST_TO_LENDIAN_DOUBLE(arg)	_swap_double(arg)
#define B_HOST_TO_LENDIAN_FLOAT(arg)	_swap_float(arg)
#define B_HOST_TO_LENDIAN_INT64(arg)	_swap_int64(arg)
#define B_HOST_TO_LENDIAN_INT32(arg)	_swap_int32(arg)
#define B_HOST_TO_LENDIAN_INT16(arg)	_swap_int16(arg)

/*--------- Host Native -> Big  ------------------------*/
#define B_HOST_TO_BENDIAN_DOUBLE(arg)	(double)(arg)
#define B_HOST_TO_BENDIAN_FLOAT(arg)	(float)(arg)
#define B_HOST_TO_BENDIAN_INT64(arg)	(uint64_t)(arg)
#define B_HOST_TO_BENDIAN_INT32(arg)	(uint32_t)(arg)
#define B_HOST_TO_BENDIAN_INT16(arg)	(uint16_t)(arg)

/*--------- Little -> Host Native ----------------------*/
#define B_LENDIAN_TO_HOST_DOUBLE(arg)	_swap_double(arg)
#define B_LENDIAN_TO_HOST_FLOAT(arg)	_swap_float(arg)
#define B_LENDIAN_TO_HOST_INT64(arg)	_swap_int64(arg)
#define B_LENDIAN_TO_HOST_INT32(arg)	_swap_int32(arg)
#define B_LENDIAN_TO_HOST_INT16(arg)	_swap_int16(arg)

/*--------- Big -> Host Native -------------------------*/
#define B_BENDIAN_TO_HOST_DOUBLE(arg)	(double)(arg)
#define B_BENDIAN_TO_HOST_FLOAT(arg)	(float)(arg)
#define B_BENDIAN_TO_HOST_INT64(arg)	(uint64_t)(arg)
#define B_BENDIAN_TO_HOST_INT32(arg)	(uint32_t)(arg)
#define B_BENDIAN_TO_HOST_INT16(arg)	(uint16_t)(arg)

#endif /* __LITTLE_ENDIAN */


/*-------------------------------------------------------------*/
/*--------- Just-do-it macros ---------------------------------*/
#define B_SWAP_DOUBLE(arg)   _swap_double(arg)
#define B_SWAP_FLOAT(arg)    _swap_float(arg)
#define B_SWAP_INT64(arg)    _swap_int64(arg)
#define B_SWAP_INT32(arg)    _swap_int32(arg)
#define B_SWAP_INT16(arg)    _swap_int16(arg)

//@}

#ifdef __cplusplus
}
#endif /* __cplusplus */

/*!	@} */

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

#endif /* _BYTEORDER_H */
