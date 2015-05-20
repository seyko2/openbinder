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

#include <SysThreadConcealed.h>
#include <support_p/SupportMisc.h>
#include <support_p/RBinder.h>

#include <support/SupportDefs.h>
#include <support/Atom.h>
#include <support/Autolock.h>
#include <support/IBinder.h>
#include <support/Looper.h>
#include <support/TypeConstants.h>

#include <ErrorMgr.h>

// Binder driver definitions.
#include <support_p/binder_module.h>

#if TARGET_HOST == TARGET_HOST_PALMOS
#include <support/KeyID.h>
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#if _SUPPORTS_NAMESPACE
using namespace palmos::osp;
#endif

static team_id g_team = B_ERROR;

int32_t this_team()
{
	if (g_team >= B_OK) return g_team;
	
	return (g_team=SysProcessID());
}

static SLocker g_cleanupLock;
static SVector<binder_cleanup_func> g_cleanupFuncs;

void add_binder_cleanup_func(binder_cleanup_func func)
{
	SAutolock _l(g_cleanupLock.Lock());
	g_cleanupFuncs.AddItem(func);
}

void rem_binder_cleanup_func(binder_cleanup_func func)
{
	SAutolock _l(g_cleanupLock.Lock());
	size_t N = g_cleanupFuncs.CountItems();
	for (size_t i=0; i<N; i++) {
		if (g_cleanupFuncs[i] == func) {
			g_cleanupFuncs.RemoveItemsAt(i);
			i--;
			N--;
		}
	}
}

void call_binder_cleanup_funcs()
{
	SVector<binder_cleanup_func> funcs;
	{
		SAutolock _l(g_cleanupLock.Lock());
		funcs = g_cleanupFuncs;
		g_cleanupFuncs.MakeEmpty();
	}

	size_t N = funcs.CountItems();
	while (N-- > 0) {
		funcs[N]();
	}
}

// ---------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------

void acquire_object(const small_flat_data& obj, const void* who)
{
	switch (obj.type) {
		case kPackedSmallAtomType:
			if (obj.data.object) B_INC_STRONG(static_cast<SAtom*>(obj.data.object), who);
			return;
		case kPackedSmallAtomWeakType: {
			if (obj.data.object) static_cast<SAtom::weak_atom_ptr*>(obj.data.object)->Increment(who);
			return;
		}
		case kPackedSmallBinderType:
			if (obj.data.object) B_INC_STRONG(static_cast<IBinder*>(obj.data.object), who);
			return;
		case kPackedSmallBinderWeakType:
			if (obj.data.object) static_cast<SAtom::weak_atom_ptr*>(obj.data.object)->Increment(who);
			return;
#if !LIBBE_BOOTSTRAP
		case kPackedSmallBinderHandleType: {
			const sptr<IBinder> b = SLooper::GetStrongProxyForHandle(obj.data.uinteger);
			if (b != NULL) B_INC_STRONG(b, who);
			return;
		}
		case kPackedSmallBinderWeakHandleType: {
			const wptr<IBinder> b = SLooper::GetWeakProxyForHandle(obj.data.uinteger);
			if (b != NULL) b.inc_weak(who);
			return;
		}
#endif
	}

	DbgOnlyFatalError("Bad type supplied to acquire_object()");
}

void release_object(const small_flat_data& obj, const void* who)
{
	switch (obj.type) {
		case kPackedSmallAtomType:
			if (obj.data.object) B_DEC_STRONG(static_cast<SAtom*>(obj.data.object), who);
			return;
		case kPackedSmallAtomWeakType: {
			if (obj.data.object) static_cast<SAtom::weak_atom_ptr*>(obj.data.object)->Decrement(who);
			return;
		}
		case kPackedSmallBinderType:
			if (obj.data.object) B_DEC_STRONG(static_cast<IBinder*>(obj.data.object), who);
			return;
		case kPackedSmallBinderWeakType:
			if (obj.data.object) static_cast<SAtom::weak_atom_ptr*>(obj.data.object)->Decrement(who);
			return;
#if !LIBBE_BOOTSTRAP
		case kPackedSmallBinderHandleType: {
			const sptr<IBinder> b = SLooper::GetStrongProxyForHandle(obj.data.uinteger);
			if (b != NULL) B_DEC_STRONG(b, who);
			return;
		}
		case kPackedSmallBinderWeakHandleType: {
			const wptr<IBinder> b = SLooper::GetWeakProxyForHandle(obj.data.uinteger);
			if (b != NULL) b.dec_weak(who);
			return;
		}
#endif
	}

	DbgOnlyFatalError("Bad type supplied to release_object()");
}

#if SUPPORTS_ATOM_DEBUG
void rename_object(const small_flat_data& obj, const void* newWho, const void* oldWho)
{
	SAtom* atom = NULL;
	bool matched = false;
	
	switch (obj.type) {
		case kPackedSmallAtomType:
		{
			atom = static_cast<SAtom*>(obj.data.object);
			matched = true;
		} break;
		case kPackedSmallAtomWeakType:
		{
			if (obj.data.object != NULL) atom = static_cast<SAtom::weak_atom_ptr*>(obj.data.object)->atom;
			matched = true;
		} break;
		case kPackedSmallBinderType:
		{
			atom = static_cast<SAtom*>(static_cast<IBinder*>(obj.data.object));
			matched = true;
		} break;
		case kPackedSmallBinderWeakType:
		{
			if (obj.data.object != NULL) atom = static_cast<SAtom::weak_atom_ptr*>(obj.data.object)->atom;
			matched = true;
		} break;
#if !LIBBE_BOOTSTRAP
		case kPackedSmallBinderHandleType: {
			const sptr<IBinder> b = SLooper::GetStrongProxyForHandle(obj.data.uinteger);
			atom = static_cast<SAtom*>(b.ptr());
			matched = true;
		} break;
		case kPackedSmallBinderWeakHandleType: {
			const wptr<IBinder> b = SLooper::GetWeakProxyForHandle(obj.data.uinteger);
			atom = b.get_weak_atom_ptr()->atom;
			matched = true;
		} break;
#endif
	}
	
	if (atom) atom->RenameOwnerID(newWho, oldWho);
	else if (!matched) DbgOnlyFatalError("Bad type supplied to rename_object()");
}
#endif

void flatten_binder(const sptr<IBinder>& binder, small_flat_data* out)
{
	if (binder != NULL) {
		//printf("Flattening for local %p...\n", binder.ptr());
		IBinder *local = binder->LocalBinder();
		if (!local) {
			//printf("Flattening for remote %p...\n", binder.ptr());
			BpBinder *proxy = binder->RemoteBinder();
			ErrFatalErrorIf(!proxy, "Binder is neither local nor remote");
			const int32_t handle = proxy->Handle();
			out->type = kPackedSmallBinderHandleType;
			out->data.object = reinterpret_cast<void*>(handle);
		} else {
			out->type = kPackedSmallBinderType;
			out->data.object = reinterpret_cast<void*>(local);
		}
	} else {
		out->type = kPackedSmallBinderType;
		out->data.object = NULL;
	}
}

void flatten_binder(const wptr<IBinder>& binder, small_flat_data* out)
{
	if (binder != NULL) {
		sptr<IBinder> real = binder.promote();
		if (real != NULL) {
			IBinder *local = real->LocalBinder();
			if (!local) {
				BpBinder *proxy = real->RemoteBinder();
				ErrFatalErrorIf(!proxy, "Binder is neither local nor remote");
				const int32_t handle = proxy->Handle();
				out->type = kPackedSmallBinderWeakHandleType;
				out->data.integer = handle;
			} else {
				out->type = kPackedSmallBinderWeakType;
				out->data.object = binder.get_weak_atom_ptr();
			}
		} else {
			// This is bad!  In order to build a SValue from a binder,
			// we need to probe it for information, which requires a primary
			// reference...  but we don't have one.  As something of a hack,
			// we will do a direct dynamic_cast<> on the binder, since we
			// know an BpBinder doesn't get destroyed until all weak references
			// are removed.
			
			SAtom::weak_atom_ptr* weak = binder.get_weak_atom_ptr();
			IBinder* local = static_cast<IBinder*>(weak->cookie);
			BpBinder* proxy = local->RemoteBinder();
			if (proxy != NULL) {
				// Found a proxy, so use it.
				const int32_t handle = proxy->Handle();
				out->type = kPackedSmallBinderWeakHandleType;
				out->data.integer = handle;
			} else {
				// Couldn't cast as a proxy, so assume this is a local binder.
				out->type = kPackedSmallBinderWeakType;
				out->data.object = weak;
			}
		}
	} else {
		out->type = kPackedSmallBinderType;
		out->data.object = NULL;
	}
}

status_t unflatten_binder(const small_flat_data& flat, sptr<IBinder>* out)
{
	switch (flat.type) {
		case kPackedSmallBinderType:
			*out = static_cast<IBinder*>(flat.data.object);
			return B_OK;
#if !LIBBE_BOOTSTRAP
		case kPackedSmallBinderHandleType:
			*out = SLooper::Process()->GetStrongProxyForHandle(flat.data.integer);
			return B_OK;
#endif
	}
	return B_BAD_TYPE;
}

status_t unflatten_binder(const small_flat_data& flat, wptr<IBinder>* out)
{
	switch (flat.type) {
		case kPackedSmallBinderType:
		{
			*out = static_cast<IBinder*>(flat.data.object);
			return B_OK;
		}
		case kPackedSmallBinderWeakType:
		{
			SAtom::weak_atom_ptr* weak = static_cast<SAtom::weak_atom_ptr*>(flat.data.object);
			out->set_weak_atom_ptr(weak);
			return B_OK;
		}
#if !LIBBE_BOOTSTRAP
		case kPackedSmallBinderHandleType:
		case kPackedSmallBinderWeakHandleType:
			*out = SLooper::Process()->GetWeakProxyForHandle(flat.data.integer);
			return B_OK;
#endif
	}
	return B_BAD_TYPE;
}

// ---------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------

void acquire_object(const flat_binder_object& obj, const void* who)
{
	switch (obj.type) {
		case kPackedLargeBinderType:
			if (obj.binder) B_INC_STRONG(static_cast<SAtom*>(obj.cookie), who);
			return;
		case kPackedLargeBinderWeakType:
			if (obj.binder) B_INC_WEAK(static_cast<SAtom*>(obj.cookie), who);
			return;
#if !LIBBE_BOOTSTRAP
		case kPackedLargeBinderHandleType: {
			const sptr<IBinder> b = SLooper::GetStrongProxyForHandle(obj.handle);
			if (b != NULL) B_INC_STRONG(b, who);
			return;
		}
		case kPackedLargeBinderWeakHandleType: {
			const wptr<IBinder> b = SLooper::GetWeakProxyForHandle(obj.handle);
			if (b != NULL) b.inc_weak(who);
			return;
		}
#endif
	}

	DbgOnlyFatalError("Bad type supplied to acquire_object()");
}

void release_object(const flat_binder_object& obj, const void* who)
{
	switch (obj.type) {
		case kPackedLargeBinderType:
			if (obj.binder) B_DEC_STRONG(static_cast<SAtom*>(obj.cookie), who);
			return;
		case kPackedLargeBinderWeakType:
			if (obj.binder) B_DEC_WEAK(static_cast<SAtom*>(obj.cookie), who);
			return;
#if !LIBBE_BOOTSTRAP
		case kPackedLargeBinderHandleType: {
			const sptr<IBinder> b = SLooper::GetStrongProxyForHandle(obj.handle);
			if (b != NULL) B_DEC_STRONG(b, who);
			return;
		}
		case kPackedLargeBinderWeakHandleType: {
			const wptr<IBinder> b = SLooper::GetWeakProxyForHandle(obj.handle);
			if (b != NULL) b.dec_weak(who);
			return;
		}
#endif
	}

	DbgOnlyFatalError("Bad type supplied to release_object()");
}

void flatten_binder(const sptr<IBinder>& binder, flat_binder_object* out)
{
	out->length = sizeof(flat_binder_object) - sizeof(small_flat_data);
	if (binder != NULL) {
		//printf("Flattening for local %p...\n", binder.ptr());
		IBinder *local = binder->LocalBinder();
		if (!local) {
			//printf("Flattening for remote %p...\n", binder.ptr());
			BpBinder *proxy = binder->RemoteBinder();
			ErrFatalErrorIf(!proxy, "Binder is neither local nor remote");
			const int32_t handle = proxy->Handle();
			out->type = kPackedLargeBinderHandleType;
			out->handle = handle;
		} else {
			out->type = kPackedLargeBinderType;
			out->binder = local;
			out->cookie = static_cast<SAtom*>(local);
		}
	} else {
		out->type = kPackedLargeBinderType;
		out->binder = NULL;
	}
}

void flatten_binder(const wptr<IBinder>& binder, flat_binder_object* out)
{
	out->length = sizeof(flat_binder_object) - sizeof(small_flat_data);
	if (binder != NULL) {
		sptr<IBinder> real = binder.promote();
		if (real != NULL) {
			IBinder *local = real->LocalBinder();
			if (!local) {
				BpBinder *proxy = real->RemoteBinder();
				ErrFatalErrorIf(!proxy, "Binder is neither local nor remote");
				const int32_t handle = proxy->Handle();
				out->type = kPackedLargeBinderWeakHandleType;
				out->handle = handle;
			} else {
				out->type = kPackedLargeBinderWeakType;
				out->binder = binder.get_weak_atom_ptr()->cookie;
				out->cookie = binder.get_weak_atom_ptr()->atom;
			}
		} else {
			// This is bad!  In order to build a SValue from a binder,
			// we need to probe it for information, which requires a primary
			// reference...  but we don't have one.  As something of a hack,
			// we will do a direct dynamic_cast<> on the binder, since we
			// know an BpBinder doesn't get destroyed until all weak references
			// are removed.
			
			SAtom::weak_atom_ptr* weak = binder.get_weak_atom_ptr();
			IBinder* local = static_cast<IBinder*>(weak->cookie);
			BpBinder* proxy = local->RemoteBinder();
			if (proxy != NULL) {
				// Found a proxy, so use it.
				const int32_t handle = proxy->Handle();
				out->type = kPackedLargeBinderWeakHandleType;
				out->handle = handle;
			} else {
				// Couldn't cast as a proxy, so assume this is a local binder.
				out->type = kPackedLargeBinderWeakType;
				out->binder = weak->cookie;
				out->cookie = weak->atom;
			}
		}
	} else {
		out->type = kPackedLargeBinderType;
		out->binder = NULL;
	}
}

status_t unflatten_binder(const flat_binder_object& flat, sptr<IBinder>* out)
{
	switch (flat.type) {
		case kPackedLargeBinderType:
			*out = static_cast<IBinder*>(flat.binder);
			return B_OK;
#if !LIBBE_BOOTSTRAP
		case kPackedLargeBinderHandleType:
			*out = SLooper::Process()->GetStrongProxyForHandle(flat.handle);
			return B_OK;
#endif
	}
	return B_BAD_TYPE;
}

status_t unflatten_binder(const flat_binder_object& flat, wptr<IBinder>* out)
{
	switch (flat.type) {
		case kPackedLargeBinderType:
		{
			*out = static_cast<IBinder*>(flat.binder);
			return B_OK;
		}
		case kPackedLargeBinderWeakType:
		{
			if (flat.binder != NULL) {
				SAtom::weak_atom_ptr* weak = static_cast<SAtom*>(flat.cookie)->CreateWeak(flat.binder);
				out->set_weak_atom_ptr(weak);
			} else {
				*out = NULL;
			}
			return B_OK;
		}
#if !LIBBE_BOOTSTRAP
		case kPackedLargeBinderHandleType:
		case kPackedLargeBinderWeakHandleType:
			*out = SLooper::Process()->GetWeakProxyForHandle(flat.handle);
			return B_OK;
#endif
	}
	return B_BAD_TYPE;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
