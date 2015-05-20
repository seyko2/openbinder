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

#ifndef __SUPPORT_STATIC_CPP
#define __SUPPORT_STATIC_CPP

#include <support/StdIO.h>
#include <support/Package.h>
#include <support/Binder.h>
#include <support/KernelStreams.h>
#include <support/Looper.h>
#include <support/NullStreams.h>
#include <support/StaticValue.h>
#include <support/Process.h>
#include <support/TextStream.h>
#include <support/TLS.h>
#include <support_p/BinderKeys.h>
#include <support_p/SupportMisc.h>
#include <support_p/ValueMap.h>
#include <support_p/SignalThreadInit.h>

#include "LooperPrv.h"

#include <stdio.h>
#include <unistd.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif /* _SUPPORTS_NAMESPACE */

// allocate this dummy slot because errno is using slot 0
//int32_t g_dummySlot(tls_allocate());
int32_t BBinder::gBinderTLS(tls_allocate());
int32_t SLooper::TLS(tls_allocate());
int32_t g_syslogTLS(tls_allocate());

void __initialize_string();
void __terminate_string();

void __initialize_atom();
void __terminate_atom();

class BinderStaticPreInit {
public:


	BinderStaticPreInit()
	{
		__initialize_string();
		__initialize_atom();
	}

	~BinderStaticPreInit()
	{
		__terminate_atom();
		__terminate_string();
		__terminate_shared_buffer();
	}
};

static BinderStaticPreInit g_binderStaticPreInit;

BValueMapPool g_valueMapPool;

SignalHandlerStaticInit g_signalHandlerStaticInit;

#if SUPPORTS_BINDER_IPC_PROFILING
char g_ipcProfileEnvVar[] = "BINDER_IPC_PROFILE";
BDebugCondition<g_ipcProfileEnvVar, 0, 0, 10000> g_profileLevel;
char g_ipcProfileDumpPeriod[] = "BINDER_IPC_PROFILE_DUMP_PERIOD";
BDebugInteger<g_ipcProfileDumpPeriod, 500, 0, 10000> g_profileDumpPeriod;
char g_ipcProfileMaxItems[] = "BINDER_IPC_PROFILE_MAX_ITEMS";
BDebugInteger<g_ipcProfileMaxItems, 10, 1, 10000> g_profileMaxItems;
char g_ipcProfileStackDepth[] = "BINDER_IPC_PROFILE_STACK_DEPTH";
BDebugInteger<g_ipcProfileStackDepth, B_CALLSTACK_DEPTH, 1, B_CALLSTACK_DEPTH> g_profileStackDepth;
char g_ipcProfileSymbols[] = "BINDER_IPC_PROFILE_SYMBOLS";
BDebugInteger<g_ipcProfileSymbols, 1, 0, 1> g_profileSymbols;
#endif

SLocker	g_parcel_pool_lock;
parcel_pool_cleanup g_parcel_pool_cleanup;

#if TARGET_HOST == TARGET_HOST_PALMOS
#if BUILD_TYPE == BUILD_TYPE_DEBUG
SVector<remote_host_info> g_remoteHostInfo;
#endif
uint16_t g_weakPutQueue[4096];
uint16_t g_strongPutQueue[4096];
#endif

#if TARGET_HOST != TARGET_HOST_WIN32
// Temporary implementation until IPC arrives.
SLocker* g_binderContextAccess = NULL;
SLooper::ContextPermissionCheckFunc g_binderContextCheckFunc = NULL;
void* g_binderContextUserData = NULL;
SKeyedVector<SString, context_entry>* g_binderContext = NULL;
SLocker* g_systemDirectoryLock = NULL;
SString* g_systemDirectory = NULL;
#endif

#if TARGET_HOST != TARGET_HOST_PALMOS

const sptr<IByteInput> Stdin(new BKernelIStr(STDIN_FILENO));
const sptr<IByteOutput> Stdout(new BKernelOStr(STDOUT_FILENO));
const sptr<IByteOutput> Stderr(new BKernelOStr(STDERR_FILENO));

static const sptr<BNullStream> NullStream(new BNullStream);
const sptr<IByteInput> NullInput(NullStream.ptr());
const sptr<IByteOutput> NullOutput(NullStream.ptr());

const sptr<IByteInput>& StandardByteInput(void) { return Stdin; }
const sptr<IByteOutput>& StandardByteOutput(void) { return Stdout; }
const sptr<IByteOutput>& StandardByteError(void) { return Stderr; }
const sptr<IByteInput>& NullByteInput(void) { return NullInput; }
const sptr<IByteOutput>& NullByteOutput(void) { return NullOutput; }

sptr<ITextOutput> bout(new BTextOutput(Stdout, B_TEXT_OUTPUT_THREADED|B_TEXT_OUTPUT_FROM_ENV));
sptr<ITextOutput> berr(new BTextOutput(Stderr, B_TEXT_OUTPUT_THREADED|B_TEXT_OUTPUT_FROM_ENV));

#else 

#undef bout
#undef berr

static const sptr<IByteInput> Stdin(debugStream.ptr());
static const sptr<IByteOutput> Stdout(debugStream.ptr());
static const sptr<IByteOutput> Stderr(debugStream.ptr());
static const sptr<BNullStream> NullStream(new BNullStream);
const sptr<IByteInput> NullInput(NullStream.ptr());
const sptr<IByteOutput> NullOutput(NullStream.ptr());

const sptr<IByteInput>& StandardByteInput(void) { return Stdin; }
const sptr<IByteOutput>& StandardByteOutput(void) { return Stdout; }
const sptr<IByteOutput>& StandardByteError(void) { return Stderr; }
const sptr<IByteInput>& NullByteInput(void) { return NullInput; }
const sptr<IByteOutput>& NullByteOutput(void) { return NullOutput; }

static sptr<ITextOutput> bout(new BTextOutput(Stdout, B_TEXT_OUTPUT_THREADED|B_TEXT_OUTPUT_FROM_ENV));
static sptr<ITextOutput> berr(new BTextOutput(Stderr, B_TEXT_OUTPUT_THREADED|B_TEXT_OUTPUT_FROM_ENV));

sptr<ITextOutput>& _get_bout(void) { return bout; }
sptr<ITextOutput>& _get_berr(void) { return berr; }

#endif /* TARGET_HOST != TARGET_HOST_PALMOS */


const SValue B_FIELD_NAME("be:name");

#if !LIBBE_BOOTSTRAP
const SPackage B_NO_PACKAGE;
extern void __terminate_profile_ipc();
#endif

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif /* _SUPPORTS_NAMESPACE */

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {
using namespace palmos::support;
#endif

#if TARGET_HOST != TARGET_HOST_PALMOS

// IBinderVector<> keys
const SValue g_keyAddChild("AddChild");
const SValue g_keyAddChildAt("AddChildAt");
const SValue g_keyOverlayAttributes("OverlayAttributes");
const SValue g_keyRemoveChild("RemoveChild");
const SValue g_keyChildAt("ChildAt");
const SValue g_keyChildAt_child("child");
const SValue g_keyChildAt_attr("attr");
const SValue g_keyChildFor("ChildFor");
const SValue g_keyIndexOf("IndexOf");
const SValue g_keyNameOf("NameOf");
const SValue g_keyCount("Count");
const SValue g_keyReorder("Reorder");

#endif

class BinderStaticInit {
public:


	BinderStaticInit()
	{
	#if !LIBBE_BOOTSTRAP
	#if SUPPORTS_BINDER_IPC_PROFILING
		// Retrieve profiler settings so they get displayed to the user immediately.
		if (g_profileLevel.Get()) {
			g_profileDumpPeriod.Get();
			g_profileMaxItems.Get();
			g_profileStackDepth.Get();
			g_profileSymbols.Get();
		}
	#endif
		__initialize_looper();
	#endif
	}

	~BinderStaticInit()
	{
	#if !LIBBE_BOOTSTRAP
		__terminate_looper();
		#if BUILD_TYPE == BUILD_TYPE_DEBUG
			__terminate_profile_ipc();
		#endif
	#endif

	}
};

static BinderStaticInit g_binderStaticInit;

// These must be destroyed -before- the SLooper state
// goes away, because they can hold references to remote
// binders.
SKeyedVector<sptr<IBinder>, area_translation_info> gAreaTranslationCache;
SLocker gAreaTranslationLock("RMemory lock");

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::osp
#endif

#include "./AtomDebug.cpp"

#endif // __SUPPORT_STATIC_CPP
