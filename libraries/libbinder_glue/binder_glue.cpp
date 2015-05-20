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

#include <support_p/BinderGlue.h>

#include <support/InstantiateComponent.h>
#include <support/Package.h>
#include <support/Locker.h>
#include <support/Looper.h>
#include <support/Process.h>
#include <support/Value.h>
#include <support/StdIO.h>

#include <stdio.h>

// This file contains per-executable-image glue code needed by the
// Binder runtime.  None of the symbols defined here must ever be
// exported from a shared library -- each executable image needs to
// get its own copy of them.

namespace palmos {
namespace support {
SLocker g_sharedObjectLock("g_sharedObject");
wptr<BSharedObject> g_sharedObject;
volatile int32_t g_pendingBufferRef = 0;
}
}

using namespace palmos::support;

static volatile int32_t g_sharedBufferRefs = 0;

void __attribute__ ((visibility("hidden")))
palmsource_inc_package_ref()
{
	//bout << "g_sharedBufferRefs: inc from " << g_sharedBufferRefs << endl;
	if (SysAtomicAdd32(&g_sharedBufferRefs, 1) > 0) {
		return;
	} else {
		// Note that there is not a race condition here because this
		// thread owns this reference, so we know the reference count
		// can't go back to zero until this thread returns.
		SPackageSptr package;
		g_sharedObjectLock.Lock();
		//bout << "Inc package ref: " << package.SharedObject() << endl;
		if (package.SharedObject() != NULL)
			package.SharedObject()->IncStrong((const void*)&g_sharedBufferRefs);
		else
			g_pendingBufferRef = 1;
		g_sharedObjectLock.Unlock();
	}
}

void __attribute__ ((visibility("hidden")))
palmsource_dec_package_ref()
{
	//bout << "g_sharedBufferRefs: dec from " << g_sharedBufferRefs << endl;
	if (SysAtomicAdd32(&g_sharedBufferRefs, -1) > 1) {
		return;
	} else {
		SPackageSptr package;
		g_sharedObjectLock.Lock();
		//bout << "Dec package ref: " << package.SharedObject() << endl;
		if (package.SharedObject() != NULL)
			package.SharedObject()->DecStrong((const void*)&g_sharedBufferRefs);
		else
			g_pendingBufferRef = 0;
		g_sharedObjectLock.Unlock();
	}
}

// The magical constructor for SPackageSptr that gives you the shared object
// of whatever executable code is creating it.  See libbinder_component_glue
// for where g_sharedObject is set.
SPackageSptr::SPackageSptr()
{
	g_sharedObjectLock.Lock();
	m_so = g_sharedObject.promote();
	g_sharedObjectLock.Unlock();


#if 0
	if (m_so != NULL) return;

// this code no longer applies, because libraries aren't in packages.  We'll have to
// build this back up if and when that becomes possible

#if TARGET_HOST == TARGET_HOST_PALMOS
	// Create a new package pointing to the PRC containing this
	// code.  Note that this ONLY works if the code is in resource 0.
	DatabaseID dbID;
	if (SysGetModuleDatabase(SysGetRefNum(), &dbID, NULL) == errNone) {
		// Load this PRC as a package, even though it (currently)
		// isn't being used that way.
		sptr<BProcess> team = SLooper::Process();
		if (team != NULL) {
			m_so = team->GetPackage(SValue::Int32(dbID), set_package_image_object);
		}
	}
#endif
#endif
}
