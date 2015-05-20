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

/*!	@file PalmTypes.h
	@ingroup Core
	@brief Elementary Palm OS data types.  Included by all Palm OS components.
*/

#ifndef _PALMTYPES_H_
#define _PALMTYPES_H_

/*!	@addtogroup Core Core Palm OS APIs
	@brief Fundamental system services, including memory, data, threads, and events.
	@{
*/

/************************************************************
 * Environment configuration
 *************************************************************/
// <BuildDefaults.h> must be included here, rather than in <PalmOS.h>
// because they must be included for ALL builds.
// Not every build includes <PalmOS.h>.

// To override build options in a local component, include <BuildDefines.h>
// first, then define switches as need, and THEN include <PalmTypes.h>.
// This new mechanism supercedes the old "AppBuildRules.h" approach.
// More details available in <BuildDefaults.h>.
#include <BuildDefaults.h>


/********************************************************************
 * Elementary data types
 ********************************************************************/
// force the Palm OS 6 version of sys/types.h
#include <stdint.h>
#include <stddef.h>
#if TARGET_HOST == TARGET_HOST_PALMOS
#	include <sys/types.h>
	/*-------------------------------------------------------------*/
	/* PalmOS types and definitions */
	/*-------------------------------------------------------------*/

	/* Descriptive formats */
	typedef int32_t					status_t;
	typedef int64_t					nsecs_t;
	typedef uint32_t				type_code;
	typedef uint32_t				perform_code;

	/* IOS */
	typedef int		FDCELL;
	typedef void*		PCELL;
	typedef int32_t	pl_t;

#	ifndef uuid_t
	typedef struct uuid_t
	{
		uint32_t time_low;
		uint16_t time_mid;
		uint16_t time_hi_and_version;
		uint8_t clock_seq_hi_and_reserved;
		uint8_t clock_seq_low;
		uint8_t node[6];
	} uuid_t;
#	endif
#elif (TARGET_HOST == TARGET_HOST_MACOS) || (TARGET_HOST == TARGET_HOST_LINUX)
	typedef int32_t					status_t;
	typedef int64_t					nsecs_t;
	typedef uint32_t				type_code;
	typedef uint32_t				perform_code;
	/* IOS */
	typedef int		FDCELL;
	typedef void*		PCELL;
	typedef int32_t	pl_t;
#else
// Windows native tools
#	include <sys/types.h>
#endif

// Logical data types
typedef uint16_t			wchar16_t;
typedef uint32_t			wchar32_t;

typedef int16_t				Coord;		// screen/window coordinate
typedef float 				fcoord_t;	// floating pt for the GcApis

typedef float				coord;		// XXX must be removed

typedef unsigned char		Boolean;

// for Boolean values
#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

// force enum size
typedef uint8_t				Enum8;
typedef uint16_t			Enum16;
typedef int8_t				SignedEnum8;
typedef int16_t				SignedEnum16;

typedef void *				MemPtr;		// global pointer
typedef struct _opaque *	MemHandle;	// global handle

typedef uint32_t			VAddr;		// Virtual Address
typedef uint32_t			SysHandle;	// Virtual Address

typedef int32_t (*ProcPtr)();


/************************************************************
 * Useful Macros 
 *************************************************************/
#ifndef ErrConvertFrom68k
#define ErrConvertFrom68k(x) ( (((status_t)(x))&0xFFFF) | 0x80000000 )
#endif

#ifndef ErrConvertTo68k
#define ErrConvertTo68k(x) ((uint16_t)(((uint32_t)(x))&0xFFFF))
#endif

// Nanosecond units
#define B_ONE_NANOSECOND		((nsecs_t)1)
#define B_ONE_MICROSECOND		(B_ONE_NANOSECOND*1000)
#define B_ONE_MILLISECOND		(B_ONE_MICROSECOND*1000)
#define B_ONE_SECOND			(B_ONE_MILLISECOND*1000)

// Conversions to nsecs_t (nanoseconds)
#define B_US2NS(us)                             ((nsecs_t)(us) * 1000)
#define B_MS2NS(ms)                             ((nsecs_t)(ms) * 1000000)
#define B_S2NS(s)                               ((nsecs_t)(s) * 1000000000)
#define B_MICROSECONDS_TO_NANOSECONDS(us)       B_US2NS(us)
#define B_MILLISECONDS_TO_NANOSECONDS(ms)       B_MS2NS(ms)
#define B_SECONDS_TO_NANOSECONDS(s)             B_S2NS(s)

// Conversions from nsecs_t (nanoseconds)
#define B_NS2US(ns)                             ((int64_t)(ns) / 1000)
#define B_NS2MS(ns)                             ((int64_t)(ns) / 1000000)
#define B_NS2S(ns)                              ((int64_t)(ns) / 1000000000)
#define B_NANOSECONDS_TO_MICROSECONDS(ns)		B_NS2US(ns)
#define B_NANOSECONDS_TO_MILLISECONDS(ns)		B_NS2MS(ns)
#define B_NANOSECONDS_TO_SECONDS(ns)			B_NS2S(ns)

// macros can be used to get at high and lo parts of 64 bit doubles 	
#define _FLB_FIRST32(x) *((uint32_t *) &x)
#define _FLB_SECOND32(x) *((uint32_t *) &x + 1)
#define _FLB_ALL32(x) *((uint32_t *) &x)

#if CPU_ENDIAN == CPU_ENDIAN_LITTLE
#define _FLB_LO32(x) *((uint32_t *) &x)
#define _FLB_HI32(x) *((uint32_t *) &x + 1)

#else
#define _FLB_HI32(x) *((uint32_t *) &x)
#define _FLB_LO32(x) *((uint32_t *) &x + 1)

#endif

/************************************************************
 * Common constants
 *************************************************************/
#ifndef NULL
#define NULL	0
#endif	// NULL

// Include the following typedefs if types.h wasn't read.
#ifdef __MWERKS__
	#if !__option(bool)
		#ifndef true
			#define true			1
		#endif
		#ifndef false
			#define false			0
		#endif
	#endif
#else
  #ifndef __cplusplus
	  #ifndef true
			enum {false, true};
	  #endif
   #endif
#endif




/************************************************************
 * Misc
 *************************************************************/

// Basic macros for swapping bytes in words or dwords to switch between
// big endian byte order and little endian byte order
#define EndianSwap16(n)	(((((uint16_t) n) << 8) & 0xFF00) | \
                         ((((uint16_t) n) >> 8) & 0x00FF))

#define EndianSwap32(n)	(((((uint32_t) n) << 24) & 0xFF000000) |	\
                         ((((uint32_t) n) <<  8) & 0x00FF0000) |	\
                         ((((uint32_t) n) >>  8) & 0x0000FF00) |	\
                         ((((uint32_t) n) >> 24) & 0x000000FF))

// Macros to convert a resource from BigEndian to LittleEndian
// when running on a LittleEndian device.

#if CPU_ENDIAN == CPU_ENDIAN_BIG
// Resources are always aligned to 16-bit boundaries so we can read 16-bits at a time.
#define RsrcEndianSwap16(x) (x)

// Note, the source MUST be 32-bit aligned on most machines.
#define RsrcEndianSwap32(x) (x)

#elif CPU_ENDIAN == CPU_ENDIAN_LITTLE

// Resources are always aligned to 16-bit boundaries so
// we can read 16-bits at a time.
#define RsrcEndianSwap16(x) EndianSwap16(*(uint16_t*)&(x))

// Note, the source MUST be 32-bit aligned on most machines.
#define RsrcEndianSwap32(x) EndianSwap32(*(uint32_t*)&(x))

#endif


/************************************************************
 * ARM specific
 *************************************************************/
#if defined(NO_RUNTIME_SHARED_LIBRARIES) && defined(__arm)
 	// prevent r9 (sb) from being used in PalmOS for Monolithic builds
    // so we can use it as a process global for heapBase
 	__global_reg(6)   uint32_t sb;
#endif	

// Use this storage class modifier macro to guarantee that objects are x bytes aligned in ARM.
#if defined(__arm) && defined(__ARMCC_VERSION) && (__ARMCC_VERSION < 130000)
#define _STACK_ALIGN(x)    __align(x)
#else
#define _STACK_ALIGN(x)
#endif

/*!	@} */

#endif //_PALMTYPES_H_
