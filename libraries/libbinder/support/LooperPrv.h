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

#ifndef __SUPPORT_LOOPERPRV_H
#define __SUPPORT_LOOPERPRV_H

#include <support/Looper.h>

#include <support/Debug.h>
#include <support/ITextStream.h>
#include <support/Locker.h>
#include <support/Parcel.h>
#include <support/Process.h>
#include <support/StdIO.h>
#include <support/TLS.h>
#include <support/Value.h>
#include <support/Vector.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------
// INTERNAL DEBUGGING OPTIONS
// -----------------------------------------------------------

#ifdef  BINDER_REFCOUNT_MSGS
#undef  BINDER_REFCOUNT_MSGS
#endif

#define BINDER_REFCOUNT_MSGS 0

#ifdef  BINDER_REFCOUNT_RESULT_MSGS
#undef  BINDER_REFCOUNT_RESULT_MSGS
#endif

#define BINDER_REFCOUNT_RESULT_MSGS 0

#define BINDER_TRANSACTION_MSGS 0
#define BINDER_BUFFER_MSGS 0
#define BINDER_DEBUG_MSGS 0

// -----------------------------------------------------------
// PUBLIC PROFILING OPTION
// -----------------------------------------------------------

#ifndef SUPPORTS_BINDER_IPC_PROFILING
#if BUILD_TYPE == BUILD_TYPE_DEBUG
#define SUPPORTS_BINDER_IPC_PROFILING 1
#else
#define SUPPORTS_BINDER_IPC_PROFILING 0
#endif
#endif

#if SUPPORTS_BINDER_IPC_PROFILING

#include "ProfileIPC.h"

#if _SUPPORTS_WARNING
#warning Compiling for binder IPC profiling!
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

struct binder_call_state : public ipc_call_state
{
	binder_call_state();
	~binder_call_state();
};

void BeginBinderCall(ipc_call_state& state);
void FinishBinderCall(ipc_call_state& state);
void ResetBinderIPCProfile();
void PrintBinderIPCProfile();

#define BINDER_IPC_PROFILE_STATE binder_call_state __ipc_profile_state__
#define BEGIN_BINDER_CALL() BeginBinderCall(__ipc_profile_state__)
#define FINISH_BINDER_CALL() FinishBinderCall(__ipc_profile_state__)
#define RESET_BINDER_IPC_PROFILE() ResetBinderIPCProfile()
#define PRINT_BINDER_IPC_PROFILE() PrintBinderIPCProfile()

extern char g_ipcProfileEnvVar[];
extern BDebugCondition<g_ipcProfileEnvVar, 0, 0, 10000> g_profileLevel;
extern char g_ipcProfileDumpPeriod[];
extern BDebugInteger<g_ipcProfileDumpPeriod, 500, 0, 10000> g_profileDumpPeriod;
extern char g_ipcProfileMaxItems[];
extern BDebugInteger<g_ipcProfileMaxItems, 10, 1, 10000> g_profileMaxItems;
extern char g_ipcProfileStackDepth[];
extern BDebugInteger<g_ipcProfileStackDepth, B_CALLSTACK_DEPTH, 1, B_CALLSTACK_DEPTH> g_profileStackDepth;
extern char g_ipcProfileSymbols[];
extern BDebugInteger<g_ipcProfileSymbols, 1, 0, 1> g_profileSymbols;

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#else

#define BINDER_IPC_PROFILE_STATE
#define BEGIN_BINDER_CALL()
#define FINISH_BINDER_CALL()
#define RESET_BINDER_IPC_PROFILE()
#define PRINT_BINDER_IPC_PROFILE()

#endif

// -----------------------------------------------------------
// COMMON PRIVATE API
// -----------------------------------------------------------

enum {
	kSchedulingResumed	= 0x00000001
};

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

extern void __initialize_looper_platform(void);
extern void __stop_looper_platform(void);
extern void __terminate_looper_platform(void);

// -----------------------------------------------------------
// TEAM GLOBALS
// -----------------------------------------------------------

#if TARGET_HOST == TARGET_HOST_PALMOS
#if BUILD_TYPE == BUILD_TYPE_DEBUG
#include <AppMgrPrv.h>
struct remote_host_info {
	int32_t	processID;
	int32_t	ref;
	int32_t	target;
	int32_t	namelen;
	char	name[32];
};
extern SVector<remote_host_info> g_remoteHostInfo;
#endif
extern uint16_t g_weakPutQueue[4096];
extern uint16_t g_strongPutQueue[4096];
#endif

// Temporary implementation until IPC arrives.
extern SLocker* g_binderContextAccess;
#if TARGET_HOST == TARGET_HOST_LINUX
extern SLooper::ContextPermissionCheckFunc g_binderContextCheckFunc;
#endif // #if TARGET_HOST == TARGET_HOST_LINUX
extern void* g_binderContextUserData;
struct context_entry
{
	SContext context;
	sptr<IBinder> contextObject;
	
};
extern SKeyedVector<SString, context_entry>* g_binderContext;
extern SLocker* g_systemDirectoryLock;
extern SString* g_systemDirectory;

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif
