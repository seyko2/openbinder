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

/*!
	@file BuildDefines.h

	@brief Build variable defines for Palm OS.

	This file is included by <BuildDefaults.h>.
	It should be included by any local component that wishes
	to override any system default compile-time switches.
	For more details, refer to <BuildDefaults.h>.

	 This file supercedes the old <BuildRules.h> file.
*/
/*!	@addtogroup Core
	@{
*/

#ifndef _BUILDDEFINES_H_
#define	_BUILDDEFINES_H_


/************************************************************
 * Compilation Control Options
 *************************************************************/

// The makefile should set TRACE_OUTPUT to one of the following constants.
// Otherwise BuildDefaults.h sets it to a default value.
#define TRACE_OUTPUT_OFF	0
#define TRACE_OUTPUT_ON		1

// The makefile should set TARGET_PLATFORM to one of the following constants.
// Otherwise BuildDefaults.h sets it to a default value.
#define TARGET_PLATFORM_DEVICE_68K		0x00000001
#define TARGET_PLATFORM_DEVICE_ARM		0x00000002
#define TARGET_PLATFORM_PALMSIM_WIN32	0x00010001
#define TARGET_PLATFORM_PALMSIM_LINUX	0x00010002
#define TARGET_PLATFORM_PALMSIM_MACOS   0x00010003

#define TARGET_HOST_PALMOS				1
#define TARGET_HOST_WIN32				2
#define TARGET_HOST_BEOS				3
#define TARGET_HOST_MACOS				4
#define TARGET_HOST_LINUX				5


/* RUNTIME_MODEL_xxx defines have been removed. */

/* DAL_DEV_xxx defines have been removed. Use #if CPU_TYPE == CPU_x86, ... instead, though we do not want this in PalmOS */

/* EMULATION_xxx defines have been removed. They are obsolete. */

/* MEMORY_xxx defines have been removed. They were relevant to EMULATION_LEVELs other than NONE and are obsolete. */

/* ENVIRONMENT_xxx defines have been removed. */

/* PLATFORM_xxx defines have been removed. */

/* ERROR_CHECK_xxx have been removed. They are obsolete and have been replaced by BUILD_TYPE... */

#define BUILD_TYPE_RELEASE		100
#define BUILD_TYPE_DEBUG		300

// The makefile should set CPU_TYPE to one of the following constants.
// Otherwise BuildDefaults.h sets it to a default value.
#define	CPU_68K  				0		// Motorola 68K type
#define	CPU_x86  				1		// Intel x86 type - Used for the NT simulator only
#define	CPU_PPC  				2		// Motorola/IBM PowerPC type - currently not supported.
#define	CPU_ARM  				3		// ARM type


// The makefile should set the define CPU_ENDIAN to one of the following:
// Note: its not just a define because some processors support both.
// If CPU_ENDIAN is not defined in the makefile then a default is set
// based on the CPU_TYPE.
#define  CPU_ENDIAN_BIG       0     // Big endian
#define  CPU_ENDIAN_LITTLE    1     // Little endian


// The makefile should set the define BUS_ALIGN to one of the following:
// Note: its not just a define because some processors support both.
// If BUS_ALIGN is not defined in the makefile then a default is set
// based on the CPU_TYPE.
#define  BUS_ALIGN_16       	16     // 16-bit data must be read/written at 16-bit address
#define  BUS_ALIGN_32			32     // 32-bit data must be read/written at 32-bit address



// The makefile should set BITFIELD_LAYOUT to one of the following constants.
// Otherwise BuildDefaults.h sets it to a default value.
// Depending of the compilator used, bit fields can be mapped in
// memory from MSB to LSB, or from LSB to MSB
#define MSB_TO_LSB				0	// Maps bit fields from most significate bit to least
									// significate bit (as CodeWarrior do)
#define LSB_TO_MSB				1	// Maps bit fields from least significate bit to most
									// significate bit (as MS Visual C++ do)

/* MODEL_xxx have been removed */

/* MEMORY_FORCE_LOCK_xxx have been removed */

// The makefile should set the define DEFAULT_DATA to one of the following:
// Setting this define to USE_DEFAULT_DATA will cause the core apps to include default
// data in the build.
#define DO_NOT_USE_DEFAULT_DATA			0
#define USE_DEFAULT_DATA					1

/* USER_MODE_xxx have been removed */

/* INTERNAL_COMMANDS_xxx have been removed */

/* INCLUDE_DES_xxx have been removed */

/* CML_ENCODER_xxx have been removed */

/* LOCALE_xxx has been removed */


#define PALMOS_SDK_VERSION			0x610

#endif // _BUILDDEFINES_H_

/* @} */
