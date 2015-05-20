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

#include "LooperPrv.h"
#include <SysThreadConcealed.h>

// -----------------------------------------------------------
// PROFILING IMPLEMENTATION
// -----------------------------------------------------------

#if SUPPORTS_BINDER_IPC_PROFILING

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

binder_call_state::binder_call_state()
{
	if (g_profileLevel.Get() > 0) {
		stack.Update(2, g_profileStackDepth.Get());
	}
}

binder_call_state::~binder_call_state()
{
}

struct binder_ipc_stats : public ipc_stats
{
	binder_ipc_stats()
		: ipc_stats(g_profileDumpPeriod.Get(), g_profileMaxItems.Get(),
					g_profileSymbols.Get(), "binder IPC")
	{
	}
	~binder_ipc_stats()
	{
	}
};

static BDebugState<binder_ipc_stats> g_stats;

void BeginBinderCall(ipc_call_state& state)
{
	if (g_profileLevel.Get() > 0) {
		binder_ipc_stats* stats = g_stats.Get();
		if (stats) stats->beginCall(state);
	}
}

void FinishBinderCall(ipc_call_state& state)
{
	if (g_profileLevel.Get() > 0) {
		binder_ipc_stats* stats = g_stats.Get();
		if (stats) stats->finishCall(state);
	}
}

void ResetBinderIPCProfile()
{
	if (g_profileLevel.Get() > 0) {
		binder_ipc_stats* stats = g_stats.Get();
		if (stats) stats->reset();
	}
}

void PrintBinderIPCProfile()
{
	if (g_profileLevel.Get() > 0) {
		binder_ipc_stats* stats = g_stats.Get();
		if (stats) stats->print();
	}
}



#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif

// ----------------------------------------------------------------
// Common looper implementation
// ----------------------------------------------------------------

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

static BProcess* g_defaultProcess = NULL;
static volatile int32_t g_haveDefaultProcess = 0;

#define DB(_x)

static inline sptr<BProcess> default_team()
{
	if ((g_haveDefaultProcess&2) != 2) {
		if (atomic_or(&g_haveDefaultProcess, 1) == 0) {
			g_defaultProcess = new BProcess(SysProcessID());
			g_defaultProcess->IncRefs(&g_defaultProcess);
			atomic_or(&g_haveDefaultProcess, 2);
		} else {
			while ((g_haveDefaultProcess&2) == 0)
				SysThreadDelay(B_MILLISECONDS(2), B_RELATIVE_TIMEOUT);
		}
	}
	
	sptr<BProcess> team;
	if (g_defaultProcess->AttemptAcquire(g_defaultProcess)) {
		team = g_defaultProcess;
		g_defaultProcess->Release(g_defaultProcess);
	}
	return team;
}

static SLocker* g_registeredLock = NULL;
static SLooper* g_registeredLoopers = NULL;

void __initialize_looper()
{
	g_registeredLock = new SLocker("Registered Loopers");
	__initialize_looper_platform();

	// Under Windows and PalmOS, we do not yet have the
	// full binder process model implemented.  If it was,
	// we would initialize the main thread as part of the
	// creation of a process.  Instead, we need to do it
	// here when we initialize the library.
#if TARGET_HOST != TARGET_HOST_BEOS
	SLooper::InitMain();
#endif

	DB(printf("* * __initialize_looper() done\n");)
}

void __stop_looper()
{
	__stop_looper_platform();
	DB(printf("* * __stop_looper() done\n");)
}

void __terminate_looper()
{
	__terminate_looper_platform();
	delete g_registeredLock;
	g_registeredLock = NULL;
	DB(printf("* * __terminate_looper() done\n");)
}

#if BINDER_BUFFER_MSGS
static int32_t g_openBuffers = 0;
#endif

SLooper::SLooper(const sptr<BProcess>& team)
	:	m_team(team),
		m_teamID(team->ID()),
		m_thid(SysCurrentThread()),
		m_priority(-1),
		m_flags(0),
		m_lastError(B_OK),
		m_nextRegistered(NULL)
{
	tls_set(TLS,this);

	// m_priority = ; LINUX_DEMO_HACK priority unimplemented
	_ConstructPlatform();

	_RegisterLooper(this);
}

SLooper::~SLooper()
{
	ExpungePackages();
	_DestroyPlatform();

	tls_set(TLS,(void*)1);
}

SLooper* SLooper::This()
{
	SLooper *me = (SLooper*)tls_get(TLS);
	if (me) return me;

	sptr<BProcess> team(default_team());
	if(team == NULL) return NULL;
	
	return new SLooper(team);
}

SLooper* SLooper::This(const sptr<BProcess>& in_team)
{
	SLooper *me = (SLooper*)tls_get(TLS);
	if (me) return me;

	sptr<BProcess> team(in_team == NULL ? default_team() : in_team);
	if(team == NULL) return NULL;

	return new SLooper(team);
}

status_t
SLooper::LastError()
{
	SLooper *me = (SLooper*)tls_get(TLS);
	status_t err;
	if (me) {
		err = me->m_lastError;
		me->m_lastError = B_OK;
	} else {
		err = B_NO_INIT;
	}
#if 0 && BUILD_TYPE == BUILD_TYPE_DEBUG
	if (err != B_OK) {
		printf("*** SLooper::LastError(): 0x%08lx (%s)\n", err, strerror(err));
	}
#endif
	return err;
}

void
SLooper::SetLastError(status_t error)
{
	SLooper *me = (SLooper*)tls_get(TLS);
	if (me && me->m_lastError == B_OK) me->m_lastError = error;
}

bool
SLooper::PrefersProcesses()
{
	if (!SupportsProcesses()) return false;
	
	char* singleproc = getenv("BINDER_SINGLE_PROCESS");
	return !(singleproc && atoi(singleproc) != 0);
}
		
int32_t 
SLooper::Thread()
{
	return This()->m_thid;
}

sptr<BProcess>
SLooper::Process()
{
	return This()->m_team;
}

int32_t 
SLooper::ProcessID()
{
	return This()->m_teamID;
}

status_t 
SLooper::InitMain(const sptr<BProcess>& team)
{
	static bool initMainCalled=false;
	#if !BINDER_DEBUG_LIB
	if (initMainCalled) {
		ErrFatalError("SLooper::InitMain() should only be called during global static initialization.\nDon't call it directly.");
		return B_DONT_DO_THAT;
	}
	#endif
	status_t err = This(team)->_InitMainPlatform();
	if (err == B_OK) {
		// Only set flag if the initialization failed.
		// We have to do this because the $#&%#&!@!
		// System Process needs a special resource bank
		// to spawn threads, so we can't initialize when
		// the library gets loaded.  Instead, we must let
		// SmooveD take care of it.
		initMainCalled = true;
	} else {
		printf("*** SLooper::InitMain() failed (in system process?)\n");
	}
	return err;
}

status_t 
SLooper::InitOther(const sptr<BProcess>& team)
{
	#if BINDER_DEBUG_LIB
	This(team);
	return B_OK;
	#else
	(void)team;
	return B_UNSUPPORTED;
	#endif
}

#if BINDER_DEBUG_LIB
struct spawn_thread_info
{
	SLooper *parent;
	thread_func func;
	void *arg;
};
#endif

SysHandle SLooper::SpawnThread(thread_func function_name, const char *thread_name, int32_t priority, void *arg)
{
	SysHandle thid;
	
//	bout << "SLooper: " << This() << " (" << find_thread(NULL) << ") SpwanThread: entering" << endl;
#if BINDER_DEBUG_LIB
	spawn_thread_info* info = (spawn_thread_info*)malloc(sizeof(spawn_thread_info));
	if (info) {
		info->parent = This();
		info->func = function_name;
		info->arg = arg;
		SysThreadCreate(NULL, thread_name, priority, sysThreadStackUI, 
		   (SysThreadEnterFunc *) _ThreadEntry, info, &thid);
	} else {
		thid = B_NO_MEMORY;
	}
#else
	// Because of the UNSIGNED nature of SysHandle, we can only return a failure sentinal or the thread ID.
	if (SysThreadCreate(NULL, thread_name, priority, sysThreadStackUI, (SysThreadEnterFunc*) function_name, arg, &thid))
		return 0;
#endif
	
//	bout << "SLooper: " << This() << " (" << find_thread(NULL) << ") SpwanThread: exiting" << endl;
	return thid;
}

void SLooper::SetThreadPriority(int32_t priority)
{
	This()->_SetThreadPriority(priority);
}

int32_t SLooper::ThreadPriority()
{
	return This()->m_priority;
}

sptr<IBinder>
SLooper::GetStrongProxyForHandle(int32_t handle)
{
	return Process()->GetStrongProxyForHandle(handle);
}

wptr<IBinder>
SLooper::GetWeakProxyForHandle(int32_t handle)
{
	return Process()->GetWeakProxyForHandle(handle);
}

status_t 
SLooper::ExpungeHandle(int32_t handle, IBinder* binder)
{
	Process()->ExpungeHandle(handle, binder);
	return B_OK;
}

void
SLooper::CatchRootObjects(catch_root_func func)
{
	This()->_CatchRootObjects(func);
}

bool 
SLooper::_ResumingScheduling()
{
	SLooper* loop = This();
	const bool resuming = !(loop->m_flags&kSchedulingResumed);
	if (resuming) loop->m_flags |= kSchedulingResumed;
	return resuming;
}

void 
SLooper::_ClearSchedulingResumed()
{
	This()->m_flags &= ~kSchedulingResumed;
}

#if BINDER_DEBUG_LIB
int32_t SLooper::_ThreadEntry(void *arg)
{
	spawn_thread_info info = *static_cast<spawn_thread_info*>(arg);
	free(arg);
	sptr<BProcess> team(info.parent ? info.parent->m_team : sptr<BProcess>());
	This(team);
	return info.func(info.arg);
}
#endif

status_t SLooper::_Loop(SLooper *parent)
{
	sptr<BProcess> team(parent ? parent->m_team : sptr<BProcess>());
	if (parent) team = parent->m_team;
	return This(team)->_LoopSelf();
}

void
SLooper::ExpungePackages()
{
	// Check to see if any add-ons got unloaded because of us
	const size_t count = m_dyingPackages.CountItems();
	if (count != 0 && m_team != NULL) {
		for (size_t i=0; i<count; i++) {
			m_team->ExpungePackage(m_dyingPackages[i]);
		}
		m_dyingPackages.MakeEmpty();
	}
}

void SLooper::_DeleteSelf(void *blooper)
{
	SLooper *me = (SLooper*)blooper;
	if (me && SLooper::_UnregisterLooper(me)) delete me;
}

void SLooper::_RegisterLooper(SLooper* who)
{
	g_registeredLock->Lock();
	who->m_nextRegistered = g_registeredLoopers;
	g_registeredLoopers = who;
	g_registeredLock->Unlock();
}

bool SLooper::_UnregisterLooper(SLooper* who)
{
	bool found = false;

	g_registeredLock->Lock();

	SLooper** last = &g_registeredLoopers;
	SLooper* cur = *last;
	while (cur != NULL && cur != who) {
		last = &cur->m_nextRegistered;
		cur = *last;
	}

	if (cur) {
		*last = cur->m_nextRegistered;
		found = true;
	}

	g_registeredLock->Unlock();

	return found;
}

void SLooper::_ShutdownLoopers()
{
	g_registeredLock->Lock();
	SLooper* cur = g_registeredLoopers;
	while (cur != NULL) {
		SLooper* next = cur->m_nextRegistered;
		// Get this looper's team to start shutting
		// down.
		cur->m_team->Shutdown();
		
		// Make sure the looper wakes up right away,
		// to expedite the shutdown process.
		cur->_Signal();
		cur = next;
	}
	g_registeredLock->Unlock();
}

void SLooper::_CleanupLoopers()
{
	g_registeredLock->Lock();
	SLooper* cur = g_registeredLoopers;
	g_registeredLoopers = NULL;
	g_registeredLock->Unlock();

	while (cur != NULL) {
		SLooper* next = cur->m_nextRegistered;
		cur->_Cleanup();
		cur = next;
	}
}

void SLooper::_Cleanup()
{
	// Note: don't unload any remaining packages, because we
	// are about to go away anyway and the system runtime may
	// have ended up unloading them for us.
	m_dyingPackages.MakeEmpty();
	delete this;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
