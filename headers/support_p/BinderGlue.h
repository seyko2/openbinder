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

#include <support/Atom.h>
#include <support/Locker.h>
#include <support/Package.h>

namespace palmos {
namespace support {

// This is the pointer to the BSharedObject created for the
// local shared library.
// Note that we MUST only hold a weak reference on it here,
// or else we would never allow the library to be unloaded.
extern SLocker __attribute__ ((visibility("hidden"))) g_sharedObjectLock;
extern wptr<BSharedObject> __attribute__ ((visibility("hidden"))) g_sharedObject;
extern volatile int32_t __attribute__ ((visibility("hidden"))) g_pendingBufferRef;

}
}
