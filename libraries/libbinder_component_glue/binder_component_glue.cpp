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
#include <support/StdIO.h>

// XXX we are including these all together as
// a quick and dirty way to prevent set_package_image_object()
// from being stripped by the linker.
#include "binder_glue.cpp"

using namespace palmos::support;

// This must be exported from a shared library implementing a Binder package.
// It is called by the package manager (in BProcess) to tell a newly
// loading package shared library about the shared object associated with it.
extern "C" void set_package_image_object(const sptr<BSharedObject>& image)
{
	g_sharedObjectLock.Lock();
	wptr<BSharedObject> oldPackage = g_sharedObject;	// avoid deadlocks.
	g_sharedObject = image;
	if (g_pendingBufferRef) {
		//bout << "Transfering pending buffer ref!" << endl;
		image->IncStrong((const void*)&g_sharedBufferRefs);
		g_pendingBufferRef = 0;
	}
	g_sharedObjectLock.Unlock();
}
