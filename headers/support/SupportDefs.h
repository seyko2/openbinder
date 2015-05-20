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

#ifndef _SUPPORT_DEFS_H
#define _SUPPORT_DEFS_H

/*!	@file support/SupportDefs.h
	@ingroup CoreSupportUtilities
	@brief Common Support Kit definitions.
*/

/*!	@addtogroup CoreSupport Support Kit
	@ingroup Core
	@brief Foundational classes of the system: utilities, types,
	containers, the Binder, the data model, etc.
*/

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/*-------------------------------------------------------------*/

#include <support/SupportBuild.h>
#include <SysThread.h>
#include <stdint.h>
/*-------------------------------------------------------------*/

#define LIBBE2 1

#if defined(_MSC_VER) && _MSC_VER
#	define B_MAKE_INT64(_x)		_x##i64
#	define B_MAKE_UINT64(_x)	_x##ui64
#else
#	define B_MAKE_INT64(_x)		_x##LL
#	define B_MAKE_UINT64(_x)	_x##ULL
#endif

#if TARGET_HOST == TARGET_HOST_WIN32
#	define B_FORMAT_INT64		"I64"
#elif TARGET_HOST == TARGET_HOST_BEOS
#	define B_FORMAT_INT64		"L"
#else
#	define B_FORMAT_INT64		"ll"
#endif

#include <support/Errors.h>

#ifndef SUPPORTS_ATOM_DEBUG
#if BUILD_TYPE == BUILD_TYPE_DEBUG
#define SUPPORTS_ATOM_DEBUG 1
#else
#define SUPPORTS_ATOM_DEBUG 0
#endif
#endif

/*-------------------------------------------------------------*/
/*----- Kernel APIs -------------------------------------------*/

// If we are building under BeOS, kernel/OS.h includes the desired kernel APIs.
// Otherwise, we use a subset placed inline here.
#if TARGET_HOST == TARGET_HOST_BEOS

#include <kernel/OS.h>
#include <kernel/image.h>

#define B_OS_NAME_LENGTH					B_OS_NAME_LENGTH
#define B_INFINITE_TIMEOUT					B_INFINITE_TIMEOUT

#define B_LOW_PRIORITY						B_LOW_PRIORITY
#define B_NORMAL_PRIORITY					B_NORMAL_PRIORITY
#define B_TRANSACTION_PRIORITY				B_NORMAL_PRIORITY
#define B_DISPLAY_PRIORITY					B_DISPLAY_PRIORITY
#define	B_URGENT_DISPLAY_PRIORITY			B_URGENT_DISPLAY_PRIORITY
#define	B_REAL_TIME_DISPLAY_PRIORITY		B_REAL_TIME_DISPLAY_PRIORITY
#define	B_URGENT_PRIORITY					B_URGENT_PRIORITY
#define B_REAL_TIME_PRIORITY				B_REAL_TIME_PRIORITY

#else

#include <fcntl.h>
#include <sys/param.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* System-wide constants */

#define B_OS_NAME_LENGTH		32

#define B_INFINITE_TIMEOUT		B_MAKE_INT64(9223372036854775807)

#define B_MICROSECONDS(n)		(B_ONE_MICROSECOND*n)
#define B_MILLISECONDS(n)		(B_ONE_MILLISECOND*n)
#define B_SECONDS(n)			(B_ONE_SECOND*n)

/* Types */

typedef int32_t	area_id;
typedef int32_t	port_id;
typedef SysHandle sem_id;
typedef int32_t thread_id;
typedef int32_t	team_id;

/* Semaphores */

/* -----
	flags for semaphore control
----- */

enum {
	B_CAN_INTERRUPT     = 1, 	/* semaphore can be interrupted by a signal */
	B_DO_NOT_RESCHEDULE = 2,	/* release() without rescheduling */
	B_CHECK_PERMISSION  = 4,	/* disallow users changing kernel semaphores */
	B_TIMEOUT           = 8,    /* honor the (relative) timeout parameter */
	//B_RELATIVE_TIMEOUT	= 8,
	//B_ABSOLUTE_TIMEOUT	= 16,	/* honor the (absolute) timeout parameter */
	B_USE_REAL_TIME     = 32,
	B_WAKE_ON_TIMEOUT   = 64
};

/* Threads */

typedef enum {
	B_THREAD_RUNNING=1,
	B_THREAD_READY,
	B_THREAD_RECEIVING,
	B_THREAD_ASLEEP,
	B_THREAD_SUSPENDED,
	B_THREAD_WAITING,

	_thread_state_force_int32 = 0x10000000
} thread_state;

#if (TARGET_HOST == TARGET_HOST_PALMOS) || (TARGET_HOST == TARGET_HOST_LINUX)

// Priorities for PalmOS, with the best user priority being 30.
#define B_LOW_PRIORITY						sysThreadPriorityLow
#define B_NORMAL_PRIORITY					sysThreadPriorityNormal
#define B_TRANSACTION_PRIORITY				sysThreadPriorityTransaction
#define B_DISPLAY_PRIORITY					sysThreadPriorityDisplay
#define	B_URGENT_DISPLAY_PRIORITY			sysThreadPriorityUrgentDisplay
#define B_REAL_TIME_PRIORITY				sysThreadPriorityHigh

#elif TARGET_HOST == TARGET_HOST_WIN32

// These are some guesses at the priority mapping to NT.
#define B_LOW_PRIORITY						-2
#define B_NORMAL_PRIORITY					-1
#define B_TRANSACTION_PRIORITY				0
#define B_DISPLAY_PRIORITY					0
#define	B_URGENT_DISPLAY_PRIORITY			1
#define	B_REAL_TIME_DISPLAY_PRIORITY		2
#define	B_URGENT_PRIORITY					16
#define B_REAL_TIME_PRIORITY				17

#else

#define B_LOW_PRIORITY						5
#define B_NORMAL_PRIORITY					10
#define B_TRANSACTION_PRIORITY				13
#define B_DISPLAY_PRIORITY					15
#define	B_URGENT_DISPLAY_PRIORITY			20
#define	B_REAL_TIME_DISPLAY_PRIORITY		100
#define	B_URGENT_PRIORITY					110
#define B_REAL_TIME_PRIORITY				120

#endif

typedef int32_t (*thread_func) (void *);


#if TARGET_HOST == TARGET_HOST_LINUX
typedef	void* image_id;
#define B_BAD_IMAGE_ID NULL
#elif TARGET_HOST == TARGET_HOST_WIN32
typedef	int32_t image_id;
#define B_BAD_IMAGE_ID -1
#else
#error "image_id -- unknown platform"
#endif

#if TARGET_HOST == TARGET_HOST_WIN32

/* Images */

typedef enum {
	B_APP_IMAGE = 1,
	B_LIBRARY_IMAGE,
	B_ADD_ON_IMAGE,
	B_SYSTEM_IMAGE,

	_image_type_force_int32 = 0x10000000
} image_type;

typedef struct {
	image_id	id;					
	image_type	type;				
	int32_t		sequence;			
	int32_t		init_order;			
	void		(*init_routine)(image_id imid);
	void		(*term_routine)(image_id imid);
	dev_t		device;				
	ino_t		node;
	char        name[MAXPATHLEN];  
	void		*text;	
	void		*data;
	int32_t		text_size;	
	int32_t		data_size;	
} image_info;

extern image_id	load_add_on(const char *path);
extern status_t	unload_add_on(image_id imid);

/* private; use the macros, below */
extern _IMPEXP_SUPPORT status_t	get_image_info(image_id image, image_info *info);
extern _IMPEXP_SUPPORT status_t	get_next_image_info(team_id team, int32_t *cookie, image_info *info);
								
#define	B_SYMBOL_TYPE_DATA		0x1
#define	B_SYMBOL_TYPE_TEXT		0x2
#define B_SYMBOL_TYPE_ANY		0x5

extern _IMPEXP_SUPPORT status_t	get_image_symbol(image_id imid, const char *name, int32_t sclass,	void **ptr);
extern _IMPEXP_SUPPORT status_t	get_nth_image_symbol(image_id imid, int32_t index, char *buf, int32_t *bufsize, int32_t *sclass, void **ptr);


#endif /* TARGET_HOST == TARGET_HOST_WIN32 */

#if TARGET_HOST != TARGET_HOST_PALMOS
static inline void* inplace_realloc (void* mem, size_t size)
{
	return NULL;
}

// XXX This was added to sys/types.h...  should it be in some PalmOS-specific
// header file instead?
#ifndef _UUID_T
#define _UUID_T
typedef struct uuid_t
{
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_version;
	uint8_t clock_seq_hi_and_reserved;
	uint8_t clock_seq_low;
	uint8_t node[6];
} uuid_t;
#endif /* uuid_T */
#endif /* TARGET_HOST != TARGET_HOST_PALMOS */

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif
//!	Perform a realloc, but only if it won't change the location of the memory.
/*!	This function is needed by some parts of the Support Kit.
	If your platform doesn't have it, you can just implement it
	to return NULL.

	@note In the Rome branch, this is an inline function here,
	while in other branches it is implemented in WindowsComaptibility.cpp
	and LinuxCompat.cpp.  The latter is probably more correct.
*/
void* inplace_realloc (void *, size_t);
#ifdef __cplusplus
}
#endif

/*-------------------------------------------------------------*/

//!	Locking operation status result.
struct lock_status_t {
	void (*unlock_func)(void* data);	//!< unlock function, NULL if lock failed
	union {
		status_t error;					//!< error if "unlock_func" is NULL
		void* data;						//!< locked object if "unlock_func" is non-NULL
	} value;
	
#ifdef __cplusplus
	//!	Constructor for successfully holding a lock.
	inline lock_status_t(void (*f)(void*), void* d)	{ unlock_func = f; value.data = d; }
	//!	Constructor for failing to lock.
	inline lock_status_t(status_t e)			{ unlock_func = NULL; value.error = e; }
	
	//!	Did the lock operation succeed?
	inline bool is_locked() const				{ return (unlock_func != NULL); }
	//!	B_OK if the lock is held, else an error code.
	inline status_t status() const				{ return is_locked() ? (status_t) B_OK : value.error; }
	//!	Call to release the lock.  May only be called once.
	inline void unlock() const					{ if (unlock_func) unlock_func(value.data); }
	
	//!	Conversion operator for status code, synonym for status().
	inline operator status_t() const			{ return status(); }
#endif
};

/*-------------------------------------------------------------*/

//!	Standard constructor flag to not initialize an object.
enum no_init_t {
	B_DO_NOT_INITIALIZE = 1
};

/*-------------------------------------------------------------*/

//!	Standard PrintToStream() flags.
enum {
	B_PRINT_STREAM_HEADER		= 0x00000001	//!< Also print type header.
};

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

/*!	@} */

#endif /* _SUPPORT_DEFS_H */
