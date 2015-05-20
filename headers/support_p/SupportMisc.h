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

#ifndef _SUPPORT_SUPPORTMISC_H_
#define _SUPPORT_SUPPORTMISC_H_

#include <support/SupportDefs.h>
#include <support/IBinder.h>
#include <support/IMemory.h>
#include <support/KeyedVector.h>
#include <support/Locker.h>

#include <support/StaticValue.h>

// value_small_data and value_large_data are defined here.
#include <support_p/ValueMapFormat.h>

#include <stdlib.h>
#include <string.h>

#if TARGET_HOST == TARGET_HOST_PALMOS
#include <Kernel.h>
#endif

#include <SysThreadConcealed.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

enum {
	kPackedSmallAtomType = B_PACK_SMALL_TYPE(B_ATOM_TYPE, sizeof(void*)),
	kPackedSmallAtomWeakType = B_PACK_SMALL_TYPE(B_ATOM_WEAK_TYPE, sizeof(void*)),
	kPackedSmallBinderType = B_PACK_SMALL_TYPE(B_BINDER_TYPE, sizeof(void*)),
	kPackedSmallBinderWeakType = B_PACK_SMALL_TYPE(B_BINDER_WEAK_TYPE, sizeof(void*)),
	kPackedSmallBinderHandleType = B_PACK_SMALL_TYPE(B_BINDER_HANDLE_TYPE, sizeof(void*)),
	kPackedSmallBinderWeakHandleType = B_PACK_SMALL_TYPE(B_BINDER_WEAK_HANDLE_TYPE, sizeof(void*)),
};

// optimization for gcc
#if (defined(__GNUC__) && (__GNUC__ < 3) && !defined(__CC_ARM))
#define DECLARE_RETURN(x) return x
#else
#define DECLARE_RETURN(x)
#endif

// SLooper/BProcess startup and shutdown.
#if !LIBBE_BOOTSTRAP
void __initialize_looper(void);
void __stop_looper(void);
void __terminate_looper(void);
#endif

// Low-level locking primitive.
void dbg_init_gehnaphore(volatile int32_t* value);
void dbg_lock_gehnaphore(volatile int32_t* value);
void dbg_unlock_gehnaphore(volatile int32_t* value);

// Cleanup helpers.
typedef void (*binder_cleanup_func)();
void add_binder_cleanup_func(binder_cleanup_func func);
void rem_binder_cleanup_func(binder_cleanup_func func);
void call_binder_cleanup_funcs();

// Syslog madness.
void enable_thread_syslog();
void disable_thread_syslog();
extern int32_t g_syslogTLS;

// -------------------------------------------------------------------
// These are for objects stored in SValues, where we want
// to use less space (8 bytes instead of 16), so we can store the
// data inline in the SValue.

// Generic acquire and release of objects.
void acquire_object(const small_flat_data& obj, const void* who);
void release_object(const small_flat_data& obj, const void* who);

#if defined(SUPPORTS_ATOM_DEBUG) && SUPPORTS_ATOM_DEBUG
void rename_object(const small_flat_data& obj, const void* newWho, const void* oldWho);
#else
inline void rename_object(const small_flat_data&, const void*, const void*) { }
#endif

void flatten_binder(const sptr<IBinder>& binder, small_flat_data* out);
void flatten_binder(const wptr<IBinder>& binder, small_flat_data* out);
status_t unflatten_binder(const small_flat_data& flat, sptr<IBinder>* out);
status_t unflatten_binder(const small_flat_data& flat, wptr<IBinder>* out);

// -------------------------------------------------------------------
// These are for objects stored in an SParcel, where we need
// to have 8 bytes of data to store both the IBinder address and
// SAtom address (cookie) for correct interaction with the driver.

struct flat_binder_object;	// defined in support_p/binder_module.h

// Generic acquire and release of objects.
void acquire_object(const flat_binder_object& obj, const void* who);
void release_object(const flat_binder_object& obj, const void* who);

void flatten_binder(const sptr<IBinder>& binder, flat_binder_object* out);
void flatten_binder(const wptr<IBinder>& binder, flat_binder_object* out);
status_t unflatten_binder(const flat_binder_object& flat, sptr<IBinder>* out);
status_t unflatten_binder(const flat_binder_object& flat, wptr<IBinder>* out);

// -------------------------------------------------------------------
// Stuff for Memory.cpp, which must be located in Static.cpp to
// get correct ordering of static constructors/destructors.

struct area_translation_info
{
	sptr<IMemoryHeap>	heap;
	int32_t				count;
};

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {
using namespace palmos::support;
#endif

extern SKeyedVector<sptr<IBinder>, area_translation_info> gAreaTranslationCache;
extern SLocker gAreaTranslationLock;

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::osp
#endif


#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

void __terminate_shared_buffer(void);

// Static objects for Parcel.cpp.
extern SLocker	g_parcel_pool_lock;

struct parcel_pool_cleanup {
	parcel_pool_cleanup();
	~parcel_pool_cleanup();
};
extern parcel_pool_cleanup g_parcel_pool_cleanup;

extern sysThreadDirectFuncs g_threadDirectFuncs;

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif	/* _SUPPORT_SUPPORTMISC_H_ */
