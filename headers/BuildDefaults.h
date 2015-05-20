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

#ifndef _BUILDDEFAULTS_H_
#define	_BUILDDEFAULTS_H_

#include <BuildDefines.h>

// To override build options in a local component, include <BuildDefines.h>
// first, then define switches as need, and then include <PalmTypes.h>.

// Some projects used to have a local copy of a file called "AppBuildRules.h"
// or "AppBuildRulesMSC.h", which was automatically included by <BuildRules.h>
// to override certain system default compile-time switches.  These local
// "prefix" files can still be used.  The project source files should be changed
// to include <BuildDefines.h>, then "AppBuildRules.MSC.h", then <PalmTypes.h>
// instead of the previous <Pilot.h>


/************************************************************
 * Settings that can be overriden in the makefile
 *************************************************************/

// Must be defined (-d or prefix file) before using.  See comment in <BuildDefines.h>.
// Since it's really important, we cannot simply use a default value for it.
#if !defined(BUILD_TYPE)
#	error BUILD_TYPE MUST be defined.
#endif

/*
ERROR_CHECK_LEVEL is now obsolete.
*/
#ifdef 	ERROR_CHECK_LEVEL
	#error "ERROR_CHECK_LEVEL is an obsolete build flag! - See BUILD_TYPE instead"
#endif

#ifndef DEFAULT_LIB_ENTRIES
	#define DEFAULT_LIB_ENTRIES	12			// space for 12 libraries in library table
#endif

// Default to allow access to internal data structures exposed in system/ui header files.
// If you want to verify that your app does not access data structure internals then define
// DO_NOT_ALLOW_ACCESS_TO_INTERNALS_OF_STRUCTS before including this file.
#ifndef DO_NOT_ALLOW_ACCESS_TO_INTERNALS_OF_STRUCTS

#	define ALLOW_ACCESS_TO_INTERNALS_OF_CLIPBOARDS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_CONTROLS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_FIELDS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_FINDPARAMS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_FORMS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_LISTS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_MENUS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_PROGRESS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_SCROLLBARS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_TABLES

#	define ALLOW_ACCESS_TO_INTERNALS_OF_BITMAPS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_FONTS
#	define ALLOW_ACCESS_TO_INTERNALS_OF_WINDOWS

#endif

/************************************************************
 *	The following defines provide a way to use compiler extensions that are
 *	implemented on different compilers using different keywords or mechanisms
 *	These are automatically defined and must not be changed
 *************************************************************/

#	ifndef CPU_TYPE
#		if defined(__i386__) || defined(_M_IX86)
#			define  CPU_TYPE	CPU_x86
#		elif defined(__POWERPC__) || defined(__powerpc__)
#			define  CPU_TYPE	CPU_PPC
#		elif defined(__arm__) || defined(__arm)
#			define  CPU_TYPE	CPU_ARM
#		else
#			error "Unable to automatically define CPU_TYPE from existing predefined macros."
#		endif
#	endif // CPU_TYPE

#if CPU_TYPE != CPU_ARM && CPU_TYPE != CPU_x86 && CPU_TYPE != CPU_PPC
#	error "Unsupported CPU_TYPE - Only CPU_ARM and CPU_x86 are supported in this release."
#endif

#ifndef CPU_ENDIAN
#	if (CPU_TYPE == CPU_x86) || (CPU_TYPE == CPU_ARM)
#		define CPU_ENDIAN	CPU_ENDIAN_LITTLE
#	else
#		define CPU_ENDIAN	CPU_ENDIAN_BIG
#	endif
#endif

#	ifndef BUS_ALIGN
#		if (CPU_TYPE == CPU_ARM) || (CPU_TYPE == CPU_x86) || (CPU_TYPE == CPU_PPC)
#			define BUS_ALIGN	BUS_ALIGN_32
#		else
#			define BUS_ALIGN	BUS_ALIGN_16
#		endif
#	endif



#	if (BUS_ALIGN == BUS_ALIGN_16)
		// if we are on a 16-bit bus, we want the 16-bit
		// structures (the 32-bit ones will always be defined
		// but not used on the 16-bit devices)
#		define INCLUDE_ALIGN_16_STRUCT
#	endif


// Bit field layout
// for MS Visual C++ (used for CPU_x86), BITFIELD_LAYOUT must be set to LSB_TO_MSB
// for the ARM tools, when compiling little endian target, layout is also LSB_TO_MSB
#ifndef BITFIELD_LAYOUT
#	if (CPU_TYPE == CPU_x86) || (CPU_TYPE == CPU_ARM)
#		define BITFIELD_LAYOUT	LSB_TO_MSB
#	else
#		define BITFIELD_LAYOUT	MSB_TO_LSB
#	endif
#endif

// The macro expansion for the filename is different on the ARM compiler
// The ARM compiler expands __FILE__ to the full path (too much for PalmOS)
// so this define allows us to use the shorter form on the ARM compiler.
#if defined (__CC_ARM)
	#define MODULE_NAME				__MODULE__
#else
	#define MODULE_NAME				__FILE__
#endif

// The inline keyword differs from compiler to compiler.
// (or some compilers may not even support inlining)
#ifdef __ARMCC_VERSION
	#define INLINE_FNC				__inline
#elif defined (_MSC_VER)
	#define INLINE_FNC				__inline
#elif defined (__MWERKS__)
	#define INLINE_FNC				inline
#elif defined (_PACC_VER)
	#define INLINE_FNC				__inline
#elif defined (__GNUC__)
#  ifdef __cplusplus
	#define INLINE_FNC				inline		// C++ syntax
#  else
	#define INLINE_FNC				static inline	// C99 syntax
#  endif
#else
	// Define it to nothing
	#warning No __inline available; linkage errors may arise
	#define INLINE_FNC
#endif
#define INLINE_FUNCTION			INLINE_FNC

// The inline keyword differs from compiler to compiler.
// (or some compilers may not even support inlining)
#ifdef __ARMCC_VERSION
	#define PURE_FUNCTION			__pure
#elif defined (_MSC_VER)
	#define PURE_FUNCTION
#elif defined (__MWERKS__)
	#define PURE_FUNCTION
#elif defined (_PACC_VER)
	#define PURE_FUNCTION			__pure
#elif defined (__GNUC__)
	#define PURE_FUNCTION
#else
	// Define it to nothing
	#warning No __pure available; linkage errors may arise
	#define PURE_FUNCTION
#endif

// The method of specifying structure alignment differs from compiler
// to compiler.  This define is used to force 1-byte alignment.
// (note that, when using MSVC, you must also add '#pragma pack(1)'
// to your source file)
#ifdef __ARMCC_VERSION
	#define PACKED						__packed
#elif defined(_MSC_VER)  // MS VC++ Compiler
	#define PACKED
	#pragma warning( disable : 4103 )  // To remove "used #pragma pack to change alignment" warning
#else
	#define PACKED
	#if defined (__GNUC__) && defined (__UNIX__)
		#pragma pack(1)
	#endif
#endif

// Some PalmOS structs need to be packed on 16-bit boundaries.
// This define/pragma is used to ensure that they are.
#ifdef __ARMCC_VERSION
	#define PACKED16					__packed
#elif defined(_MSC_VER)  // MS VC++ Compiler
	#define PACKED16
	#pragma warning( disable : 4103 )  // To remove "used #pragma pack to change alignment" warning
#else
	#define PACKED16
	#if defined (__GNUC__) && defined (__UNIX__)
		#pragma pack(2)
	#endif
#endif

// Use this to work around ADS1.1 compiler bugs. For example:
// #ifdef ADS11_COMPILER_BUG
// #pragma Ono_peephole
// #endif
#if defined (__ARMCC_VERSION) && (__ARMCC_VERSION / 10000 == 11)
#	define ADS11_COMPILER_BUG	1
#endif

#if defined (__ARMCC_VERSION) && (__ARMCC_VERSION / 10000 == 12)
#	define ADS12_COMPILER_BUG	1
#endif

#ifndef TARGET_PLATFORM
#	if defined(WIN32)
#		define TARGET_PLATFORM	TARGET_PLATFORM_PALMSIM_WIN32
#	elif defined(__arm__) || defined(__arm) || defined(__ARMCC_VERSION)
#		define TARGET_PLATFORM	TARGET_PLATFORM_DEVICE_ARM
#   elif defined(MACOS)
#		define TARGET_PLATFORM TARGET_PLATFORM_PALMSIM_MACOS
#	elif defined(LINUX)
#		define TARGET_PLATFORM TARGET_PLATFORM_PALMSIM_LINUX
#	endif
#endif

#ifndef TARGET_PLATFORM
#	error TARGET_PLATFORM "is not specified in your project file and it's not possible to set a default value."
#endif

#ifndef TARGET_HOST
#define TARGET_HOST TARGET_HOST_PALMOS
#endif


/**************************************************************************
 * Kernel functions accessed via traps.  In ARM, traps are SWIs and
 * the compiler has syntax for giving a SWI a function prototype (useful
 * for additional type checking but not really necessary).
 * In other compilers, this might not be possible so the KAL might need
 * to be different.  In Win32, we just provide these traps as stubs
 * that use Win32 SendMessage.
 **************************************************************************/
#if !defined(LINUX_DEMO_HACK) && TARGET_PLATFORM == TARGET_PLATFORM_DEVICE_ARM
#	ifdef __ARMCC_VERSION
#		define KERNEL_TRAP(x) __swi(x)
#		define KERNEL_TRAP_INDIRECT(x) __swi_indirect(x)
#		define RETURN_STRUCT __value_in_regs
#	else
#		error Unsupported ARM compiler
#	endif
#else
#	define KERNEL_TRAP(x)
#	define KERNEL_TRAP_INDIRECT(x)
#	define RETURN_STRUCT
#endif

// If not already specified, try to determine whether we can
// use C++ namespaces with this compiler.
#ifndef _SUPPORTS_NAMESPACE
#if defined(__CC_ARM) || (defined(_MSC_VER) && _MSC_VER < 1300)
// ADS doesn't support namespaces at all.
// MSC 6.0 has broken namespace support.
#define _SUPPORTS_NAMESPACE 0
#else
#define _SUPPORTS_NAMESPACE 1
#endif
#endif

// GNUC and RVDS requires namespaces for std namespace things (like
// type_info).  We must use namespace without turning on our
// use of namespaces in our own code.
#if defined(__GNUC__) || defined(__EDG_RUNTIME_USES_NAMESPACES)
#define _REQUIRES_NAMESPACE 1
#else
#define _REQUIRES_NAMESPACE 0
#endif

// Convenience macro for new (nothrow) when nothrow may or may
// not be in a namespace.
#if _SUPPORTS_NAMESPACE || _REQUIRES_NAMESPACE
#define B_NO_THROW (std::nothrow)
#else
#define B_NO_THROW (nothrow)
#endif

// Convenience macro to conditionalize on whether namespaces
// are supported. (std namespace only)
#if _SUPPORTS_NAMESPACE || _REQUIRES_NAMESPACE
#define B_SNS(x) x
#else
#define B_SNS(x)
#endif

// Convenience macro to conditionalize on whether namespaces
// are supported.
#if _SUPPORTS_NAMESPACE
#define BNS(x) x
#else
#define BNS(x)
#endif

// If not already specified, determine whether RTTI is supported
#ifndef _SUPPORTS_RTTI
#if TARGET_HOST == TARGET_HOST_PALMOS
#define _SUPPORTS_RTTI 0
#else
#define _SUPPORTS_RTTI 1
#endif
#endif

// Convenience macro to conditionalize on whether RTTI is supported.
#if _SUPPORTS_RTTI
#define B_TYPEPTR(x) (&typeid(x))
#define B_TYPENAME(x) typeid(x).name()
#else
#define B_TYPEPTR(x) ((const type_info*)NULL)
#define B_TYPENAME(x) ""
#endif

// Safety checks. Several people spent a lot of time figuring out this kind of problems...
// A few files does not REQUIRE them so I cannot simply put them as a rule
/*
#ifdef __ARMCC_VERSION
#	if !defined(__APCS_ROPI)
#		error ARM versions must be compiled with compiler option -apcs /ropi.
#	endif
#	if !defined(__APCS_RWPI)
#		error ARM versions must be compiled with compiler option -apcs /rwpi.
#	endif
#	if !defined(__APCS_INTERWORK)
#		error ARM versions must be compiled with compiler option -apcs /interwork.
#	endif
#	if !defined(__APCS_SHL)
#		error ARM versions must be compiled with compiler option -apcs /shl.
#	endif
#endif
*/


// Provide macro covers for __declspec(), which is needed when performing
// a native Windows build.
#if (TARGET_HOST == TARGET_HOST_WIN32)
#define	_EXPORT		__declspec(dllexport)
#define	_IMPORT		__declspec(dllimport)
#else
#define	_EXPORT
#define	_IMPORT
#endif


/*
Disable most frequent VS warnings so we can still build in Warning level 4.

warning C4200: nonstandard extension used : zero-sized array in struct/union
warning C4214: nonstandard extension used : bit field types other than int
warning C4201: nonstandard extension used : nameless struct/union
warning C4100: 'xxx' : unreferenced formal parameter
warning C4152: nonstandard extension, function/data pointer conversion in expression
warning C4218: nonstandard extension used : must specify at least a storage class or a type
warning C4127: conditional expression is constant
warning C4204: nonstandard extension used : non-constant aggregate initializer
warning C4711: function 'BmpFindShadowEntryFor68K' selected for automatic inline expansion
warning C4054: 'type cast' : from function pointer 'void (__cdecl *)(struct EmulStateTag *)' to data pointer 'void *'
warning C4068: unknown pragma

Warnings 4152 and 4218 are appearing in Startup.c generated by PalmDefComp.
The 4152 could probably be removed by generating an explicit cast to void*
Warning 4127 is caused by the do { } while(0) in ErrFatalError macros

DO NOT REMOVE: 4244, 4244
These warnings may reveal bugs and SHOULD be fixed anyway
*/
#ifdef _MSC_VER
#pragma warning(disable : 4200)
#pragma warning(disable : 4201)
#pragma warning(disable : 4100)
#pragma warning(disable : 4152)
#pragma warning(disable : 4218)
#pragma warning(disable : 4204)
//#pragma warning(disable : 4711)
#pragma warning(disable : 4127)
#pragma warning(disable : 4054)
#pragma warning(disable : 4214)
#pragma warning(disable : 4068)
#endif

/************************************************************
 * Obsolete Options
 *************************************************************/

#ifdef 	MEMORY_FORCE_LOCK
	#error "MEMORY_FORCE_LOCK is an obsolete build flag!"
#endif

#ifdef 	MODEL
	#error "MODEL is an obsolete build flag!"
#endif

#ifdef 	USER_MODE
	#error "USER_MODE is an obsolete build flag!"
#endif

#ifdef 	INTERNAL_COMMANDS
	#error "INTERNAL_COMMANDS is an obsolete build flag!"
#endif

#ifdef 	INCLUDE_DES
	#error "INCLUDE_DES is an obsolete build flag!"
#endif

#ifdef 	RESOURCE_FILE_PREFIX
	#error "RESOURCE_FILE_PREFIX is an obsolete build flag!"
#endif

#ifdef 	SHELL_COMMAND_DB
	#error "SHELL_COMMAND_DB is an obsolete build flag!"
#endif

#ifdef 	SHELL_COMMAND_UI
	#error "SHELL_COMMAND_UI is an obsolete build flag!"
#endif

#ifdef 	SHELL_COMMAND_APP
	#error "SHELL_COMMAND_APP is an obsolete build flag!"
#endif

#ifdef 	SHELL_COMMAND_EMULATOR
	#error "SHELL_COMMAND_EMULATOR is an obsolete build flag!"
#endif

#ifdef 	CML_ENCODER
	#error "CML_ENCODER is an obsolete build flag!"
#endif

#ifdef	EMULATION_LEVEL
	#error "EMULATION_LEVEL is an obsolete build flag!"
#endif

#ifdef	PLATFORM_TYPE
	#error "PLATFORM_TYPE is an obsolete build flag!"
#endif

#ifdef 	USE_TRAPS
	#error "USE_TRAPS is an obsolete build flag!"
#endif

#ifdef ENVIRONMENT
	#error "ENVIRONMENT is an obsolete build flag!"
#endif

#ifdef MODEL
	#error "MODEL is an obsolete build flag!"
#endif

#ifdef 	DISABLE_HAL_TRAPS
	#error "DISABLE_HAL_TRAPS is an obsolete build flag!"
#endif

#ifdef 	_DONT_USE_FP_TRAPS_
	#error "_DONT_USE_FP_TRAPS_ is an obsolete build flag!"
#endif

#ifdef 	_DONT_USE_FP_TRAPSE_
	#error "_DONT_USE_FP_TRAPSE_ is an obsolete build flag!"
#endif

#ifdef 	RUNTIME_MODEL
	#error "RUNTIME_MODEL is an obsolete build flag!"
#endif

#ifdef 	MEMORY_TYPE
	#error "MEMORY_TYPE is an obsolete build flag!"
#endif

#ifdef 	DAL_DEVELOPMENT
	#error "DAL_DEVELOPMENT is an obsolete build flag!"
#endif

#ifdef 	DYN_MEM_SIZE_MAX
	#error "DYN_MEM_SIZE_MAX is an obsolete build flag!"
#endif

#ifdef 	SMALL_ROM_SIZE
	#error "SMALL_ROM_SIZE is an obsolete build flag!"
#endif

#ifdef 	CONSOLE_SERIAL_LIB
	#error "CONSOLE_SERIAL_LIB is an obsolete build flag!"
#endif

#ifdef 	PILOT_SERIAL_MGR
	#error "PILOT_SERIAL_MGR is an obsolete build flag!"
#endif

#ifdef 	MEMORY_VERSION
	#error "MEMORY_VERSION is an obsolete build flag!"
#endif

#ifdef 	GRAPHICS_VERSION
	#error "GRAPHICS_VERSION is an obsolete build flag!"
#endif

#ifdef 	HW_TARGET
	#error "HW_TARGET is an obsolete build flag!"
#endif

#ifdef 	HW_REV
	#error "HW_REV is an obsolete build flag!"
#endif

#ifdef 	RMP_LIB_INCLUDE
	#error "RMP_LIB_INCLUDE is an obsolete build flag!"
#endif

#ifdef 	OEM_PRODUCT
	#error "OEM_PRODUCT is an obsolete build flag!"
#endif

#ifdef 	LANGUAGE
	#error "LANGUAGE is an obsolete build flag!"
#endif

#ifdef 	COUNTRY
	#error "COUNTRY is an obsolete build flag!"
#endif

#ifdef 	LOCALE
	#error "LOCALE is an obsolete build flag!"
#endif


/************************************************************
 *	Settings that can be overriden in the makefile
 *	These needs to be here because it uses previous section defines
 *************************************************************/
// Default Palm Reporter trace policy
#ifndef TRACE_OUTPUT
#	if	BUILD_TYPE == BUILD_TYPE_DEBUG
#		define TRACE_OUTPUT TRACE_OUTPUT_ON
#	endif
#endif

/* NO_RUNTIME_SHARED_LIBRARIES is defined by the jamfile */

/************************************************************
 *************************************************************/

#endif // __BUILDDEFAULTS_H__
