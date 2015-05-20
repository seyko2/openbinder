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

//#define DEBUG 1
#include "LooperPrv.h"

#include <support/Autolock.h>
#include <support/StopWatch.h>
#include <support/Handler.h>
#include <support/INode.h>
#include <ServiceMgr.h>
#include <Kernel.h>
#include <KeyIDs.h>
#include <SysThread.h>
#include <SysThreadConcealed.h>
#include <SecurityServices.h>

#include <ErrorMgr.h>

#include <support_p/DebugLock.h>
#include <support_p/SupportMisc.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#define OP_FLAGS_USER_CODE		1

#define OP_ATTEMPT_INC_STRONG	1
#define OP_TRANSACT				2
#define OP_REPLY				3
#define OP_ERROR				4		// opcode2 is the (int32_t) error code.
#define OP_GET_CONTEXT			5		// opcode2 is which context.

#define STACK_META_DATA			1
#define MAX_TRANSACTION_SIZE	4096
#define META_DATA_SIZE			512

// Minimum amount of stack we will allow before dispatching
// a binder transaction
#define MIN_STACK_SPACE			1024

#define TRACE_TRANSACTIONS		0
#define TRACE_LOOPERS			0
#define TRACE_THREADS			0
#define TRACE_CONCURRENCY		0
#define TRACE_EVENT_SCHEDULING	0
#define SHOW_LOOPER_INFO		0
#define DEBUG_BINDER_OPS		0
#define TRACE_THREAD_TIMEOUTS	0
#define TRACE_DECREFS			0
#define TRACE_BATCH_PUTS		0
#define BINDER_DEBUG_MSGS		0
#define DEBUG_LOSTOBJ			0
#define DEBUG_HANDLER_DISPATCH	0
#define DEBUG_PARCEL_POOL		0

#if SHOW_LOOPER_INFO
#define INFO(_on) _on
#else
#define INFO(_off)
#endif

// Some magic from Manuel.
extern "C" status_t KALCurrentHardenExitCallback(ThreadExitCallbackID iThreadExitCallbackId);

#define LOOPER_STACK_SIZE (16*1024)

// -------------------- GLOBALS --------------------

#if BUILD_TYPE == BUILD_TYPE_DEBUG
#include <typeinfo>
#define OP_GET_HOST_BINDER_INFO	11
#include <AppMgrPrv.h>
#endif
static uint32_t s_processID = ~0U;

static SLooper::catch_root_func g_catchRootFunc = NULL;

static SysThreadGroupHandle g_threadGroup = NULL;

// Can't clean this up because it is in the BProcess.
// (If we ever want to work again to debug with
// virtual teams, there is no way to get back to the
// correct team.)
static KeyID g_SystemBinderKey = kKALKeyIDNull;

#if TRACE_CONCURRENCY
static volatile int32_t g_runningSpawns = 0, g_maxSpawns = 0;
static volatile int32_t g_runningHandlers = 0, g_maxHandlers = 0;
static volatile int32_t g_runningDecRefs = 0, g_maxDecRefs = 0;
static volatile int32_t g_runningLostObjs = 0, g_maxLostObjs = 0;
static volatile int32_t g_runningCalls = 0, g_maxCalls = 0;
static volatile int32_t g_runningAttemptIncs = 0, g_maxAttemptIncs = 0;
static volatile int32_t g_runningTransacts = 0, g_maxTransacts = 0;
static volatile int32_t g_runningAny = 0, g_maxAny = 0;

#define ENTER_CONCURRENCY(cond, vars, what, who)									\
	if (cond) {																		\
		int32_t _curCon = g_threadDirectFuncs.atomicAdd32(&g_running##vars, 1)+1;	\
		int32_t _maxCon = g_max##vars;												\
		while (_curCon > _maxCon) {													\
			if (KALAtomicCompareAndSwap32(											\
					(volatile uint32_t*)&g_max##vars, _maxCon, _curCon) == 0) {		\
				_maxCon = _curCon;													\
				bout << "Process " << (void*)s_processID							\
					<< what << _maxCon << who << endl;								\
			} else {																\
				_maxCon = g_max##vars;												\
			}																		\
		}																			\
	}																				\

#define EXIT_CONCURRENCY(cond, vars)												\
	if (cond) { g_threadDirectFuncs.atomicAdd32(&g_running##vars, -1); }			\

#else

#define ENTER_CONCURRENCY(cond, vars, what, who)
#define EXIT_CONCURRENCY(cond, vars)

#endif

void __initialize_looper_platform()
{
	g_threadGroup = SysThreadGroupCreate();
	g_binderContextAccess = new SLocker("g_binderContext");
	g_realContextAccess = new SLocker("g_realContext");
	g_binderContext = new SVector<sptr<IBinder> >;
	g_realContext = new SVector< sptr<INode> >;
	g_keyContext = new SVector<KeyID>;

#if BUILD_TYPE == BUILD_TYPE_DEBUG
	status_t huh = SysAppGetProcessID(kKeyIDProcessIdentity, &s_processID);
	if (huh != errNone) bout << SPrintf("SysAppGetProcessID(%04x) failed: %08x", kKALKeyIDProcess, huh) << endl;
	if (s_processID == 0) s_processID = 0x1000;
#endif

}

void __stop_looper_platform()
{
	// This causes all thread pools to be stopped, but doesn't
	// yet leave us in an unstable state.
	SLooper::Stop();
}

void __terminate_looper_platform()
{
	// Remove reference on default context.  Must do this BEFORE
	// shutting down everything, as this will cause transactions
	// to be sent back to the owning process, which requires
	// we have working SLooper objects.
	delete g_binderContext;
	g_binderContext = NULL;
	delete g_realContext;
	g_realContext = NULL;
	delete g_keyContext;
	g_keyContext = NULL;

	// Stop all loopers.  Forcibly free any other SLooper objects
	// for threads we don't own.
	SLooper::Shutdown();

	// No more need for this thing.
	delete g_binderContextAccess;
	g_binderContextAccess = NULL;
	delete g_realContextAccess;
	g_realContextAccess = NULL;
}

SContext get_default_context()
{
	return get_context(B_DEFAULT_CONTEXT);
}

SContext get_system_context()
{
	// LINUX_DEMO_HACK -- fixed in multiproc --joeo
	return get_default_context();
}

SContext get_context(uint32_t which, KeyID permissionKey)
{
	SAutolock lock(g_realContextAccess->Lock());
	if (g_realContext->CountItems() > which) {
		const sptr<INode>& context = g_realContext->ItemAt(which);
		if (context != NULL) return context;
	} else {
		g_realContext->SetSize(which+1);
	}

	sptr<IBinder> root = SLooper::GetRootObject(which, permissionKey);
	sptr<INode> context = INode::AsInterface(root);
	g_realContext->ReplaceItemAt(context, which);
	// Ignore early errors in the data manager.
	if (context == NULL && which == B_DEFAULT_CONTEXT && SysProcessID() != 1) {
		printf("Unable to retrieve context in process %s (#%ld) from root object %p\n", SysProcessName(), SysProcessID(), root.ptr());
	}
	if (root != NULL)
	{
//		bout << "**** root = " << SValue::Binder(root) << " Inspect() = " << root->Inspect(root, B_WILD_VALUE) << endl;
	}
	if (root != NULL)
	{
		bout << "**** root = " << SValue::Binder(root) << " Inspect() = " << root->Inspect(root, B_WILD_VALUE) << endl;
	}
	return SContext(context);
}

// -------------------- PALMOS IMPLEMENTATION --------------------

void SLooper::Stop()
{
	INFO(printf("* * SLooper::Stop() is stopping all teams...\n"));
	SLooper::_ShutdownLoopers();
	INFO(printf("* * SLooper::Stop() is waiting for threads to exit...\n"));
	SysThreadGroupWait(g_threadGroup);
	INFO(printf("* * SLooper::Stop() is expunging this looper's images...\n"));
	SLooper* me = SLooper::This();
	INFO(printf("* * SLooper::Stop() releasing remote references...\n"));
	if (me) {
		me->m_team->ReleaseRemoteReferences();
		me->ExpungePackages();
		//bout << "Remaining references on BProcess:" << endl;
		//me->m_team->Report(bout);
	}
	INFO(printf("* * SLooper::Stop() is done!\n"));
}

void SLooper::Shutdown()
{
	INFO(printf("* * SLooper::Shutdown() is stopping all teams...\n"));
	SLooper::_ShutdownLoopers();
	INFO(printf("* * SLooper::Shutdown() is waiting for threads to exit...\n"));
	SysThreadGroupDestroy(g_threadGroup);
	INFO(printf("* * SLooper::Shutdown() is cleaning the remains...\n"));
	SLooper::_CleanupLoopers();
	INFO(printf("* * SLooper::Shutdown() is done!\n"));
}

void
SLooper::_ConstructPlatform()
{
	// Initialize pool of keys for incoming transactions.
	for (int32_t i = 0; i < MAX_KEY_TRANSFER; i++) {
		m_keys[i] = kKALKeyIDNull;
	}

	KALCurrentThreadInstallExitCallback(_DeleteSelf, this, &m_exitCallback);

	// Modify the above callback so that it will not crash if called
	// after this library is unloaded.  We need to do this because
	// we attach this looper state to any threads that make use of
	// the binder -- not just those we have spawned.  For those threads,
	// there is no way to control when the exit or get them to remove
	// the callback, so this is the best we can do.
	KALCurrentHardenExitCallback(m_exitCallback);

	KALThreadInfoType ti;
	KALThreadGetInfo(KeyID(KALTSDGet(kKALTSDSlotIDCurrentThreadKeyID)), kKALThreadInfoTypeCurrentVersion, &ti);

	m_stackBottom= (uint32_t)ti.stack;

#if SHOW_LOOPER_INFO || BINDER_DEBUG_MSGS
	KALResourceBankInfoType rbit;
	
	KALResourceBankGetInfo(kKeyIDResourceBank, kKALResourceBankInfoTypeCurrentVersion, &rbit);
	
	int maxThreads = rbit.maxThreads;
	int freeThreads = rbit.freeThreads;

	berr	<< "SLooper: enter new looper #" /* << Process()->LooperCount() */ << " " << this << " ==> Thread="
			<< SPrintf("%8x", SysCurrentThread())
			<< ", thread count=" << (maxThreads - freeThreads) << endl;
#endif
}

void
SLooper::_DestroyPlatform()
{
	// Free the pool of keys keys
	for (int32_t i = 0; i < MAX_KEY_TRANSFER; i++) {
		if (m_keys[i] != kKALKeyIDNull) {
			m_team->FreeKey(m_keys[i]);
		}
	}

#if SHOW_LOOPER_INFO || BINDER_DEBUG_MSGS
	berr	<< "SLooper: exit new looper #" /* << Process()->LooperCount() */ << " " << this << " ==> Thread="
			<< SPrintf("%8x", SysCurrentThread())
			<< endl;
#endif
}

status_t
SLooper::_InitMainPlatform()
{
	/*
	const char* env;
	if ((env=getenv("BINDER_MAX_THREADS")) != NULL) {
		int32_t val = atol(env);
	}
	if ((env=getenv("BINDER_IDLE_PRIORITY")) != NULL) {
		int32_t val = atol(env);
	}
	if ((env=getenv("BINDER_IDLE_TIMEOUT")) != NULL) {
		int64_t val = atoll(env);
	}
	if ((env=getenv("BINDER_REPLY_TIMEOUT")) != NULL) {
		int64_t val = atoll(env);
	}
	*/

	status_t err;

	ENTER_CONCURRENCY(true, Spawns, " now spawning ", " new threads");

	err = _SpawnTransactionLooper('I');
	if (err) {
		m_team->TransactionThreadForceRemove();
	}
	return err;
}

static int32_t g_transSeq = 1;

status_t SLooper::_SpawnTransactionLooper(char code)
{
	status_t status = B_BINDER_TOO_MANY_LOOPERS;
	
	const int32_t seq = g_threadDirectFuncs.atomicInc32(&g_transSeq);
	char name[32]; // smooved server threads get lowercase initial letters
	sprintf(name, "%03ld%c Transaction #%ld\n", seq%1000, code, seq);

	INFO(printf("Spawning a transaction looper!  %s\n", name));

	sptr<BProcess> team = Process();
	team->LooperEnter();

	KeyID thid;
	// Start it at the highest possible priority, so we can get in
	// to handle the next transaction without someone getting in
	// the way.
	status = SysThreadCreate(g_threadGroup, name, sysThreadPriorityBestSystem, LOOPER_STACK_SIZE,
							(SysThreadEnterFunc*)_EnterTransactionLoop, team.ptr(), &thid);
	if (status == errNone) {
		B_INC_STRONG(team, team.ptr());
		status = SysThreadStart(thid);
	}
	if (status != errNone) {
		team->LooperForceRemove();
		DbgOnlyFatalError("Unable to spawn a new thread for the Binder thread pool.  "
							"This is probably because someone is creating too many -- or leaking -- threads.  "
							"It is most likely NOT a bug in the thread pool itself.");
	}

	return status;
}

status_t SLooper::_EnterTransactionLoop(BProcess* team)
{
	SLooper* looper = This(team);
	if (team) B_DEC_STRONG(team, team);
	INFO(printf("Transaction Looper %p entering team %p\n", looper, team));
	status_t status = looper->_LoopTransactionSelf();

#if TRACE_THREADS
	bout << "Exiting binder thread in " << team->ID() << endl;
#endif

	INFO(printf("Transaction Looper %p exiting team %p with status %ld\n", looper, team, status));
	return status;
}

status_t SLooper::SpawnLooper()
{
	TRACE();
	return B_ERROR;
}

status_t SLooper::_EnterLoop(BProcess* team)
{
	TRACE();
	(void)team;
	return B_ERROR;
}

status_t SLooper::Loop()
{
	bout << "SLooper::Loop(): Don't call this function.  I'll now spin forever..." << endl;
	while (1) KALCurrentThreadDelay(B_ONE_SECOND, B_RELATIVE_TIMEOUT);

	return B_DONT_DO_THAT;
}

void
SLooper::_SetNextEventTime(nsecs_t when, int32_t priority)
{
	//TRACE();
//	bout << "SLooper::_SetNextEventTime:enter (" << SysCurrentThread() << ")@" << SysGetRunTime() << " when = " << when << endl;
	
	if (when != B_INFINITE_TIMEOUT)
	{
		const nsecs_t now = approx_SysGetRunTime();
#if TRACE_EVENT_SCHEDULING
		bout << "Looper in " << (void*)s_processID << " setting next event time to " << (when-now) << "ns from now" << endl;
#endif
		// bout << " now: " << now << endl << "next: " << when << endl;
		if (now >= when)
			KALBinderTimeout(kKALKeyIDProcess, B_POLL, 0, (uint8_t)priority);
		else
			KALBinderTimeout(kKALKeyIDProcess, B_ABSOLUTE_TIMEOUT, when, (uint8_t)priority);
	}
	else {
#if TRACE_EVENT_SCHEDULING
		bout << "Looper in " << (void*)s_processID << " setting next event time to INFINITE" << endl;
#endif
		KALBinderTimeout(kKALKeyIDProcess, B_WAIT_FOREVER, 0, (uint8_t)priority);
	}
}

void SLooper::_SetThreadPriority(int32_t priority)
{
	if (m_priority != priority) {
		//printf("*** Changing priority of thread %ld from %ld to %ld\n",
		//		m_thid, m_priority, priority);
		if (KALThreadChangePriority(m_thid, (uint8_t)priority) != errNone) {
			printf("Error priority of thread %8lx to %ld!\n", m_thid, priority);
			priority = B_NORMAL_PRIORITY;
			KALThreadChangePriority(m_thid, (uint8_t)priority);
		}
		//printf("Setting priority of thread %ld to %ld\n", SysCurrentThread(), priority);
		m_priority = priority;
	}
}

void
SLooper::_CatchRootObjects(catch_root_func func)
{
	g_catchRootFunc = func;
	bout << "CatchRootObjects() not implemented for PalmOS!" << endl;
}

void
SLooper::SetRootObject(const sptr<IBinder>& context, uint32_t which, KeyID permissionKey)
{
	g_binderContextAccess->Lock();
	if (g_binderContext->CountItems() <= which) g_binderContext->SetSize(which+1);
	g_binderContext->ReplaceItemAt(context, which);
	if (g_keyContext->CountItems() <= which) g_keyContext->SetSize(which+1);
	g_keyContext->ReplaceItemAt(permissionKey, which);
	g_binderContextAccess->Unlock();

	g_realContextAccess->Lock();
	if (g_realContext->CountItems() <= which) g_realContext->SetSize(which+1);
	g_realContext->ReplaceItemAt(INode::AsInterface(context.ptr()), which);
	g_realContextAccess->Unlock();
}

sptr<IBinder> SLooper::GetRootObject(uint32_t which, KeyID permissionKey)
{
	SAutolock _l(g_binderContextAccess->Lock());
	if (g_binderContext->CountItems() > which) {
		const sptr<IBinder>& obj = g_binderContext->ItemAt(which);
		if (obj != NULL) return obj;
	} else {
		g_binderContext->SetSize(which+1);
	}
	g_binderContext->ReplaceItemAt(_RetrieveContext(which, permissionKey), which);
	return g_binderContext->ItemAt(which);
}

sptr<IBinder>
SLooper::_RetrieveContext(uint32_t which, KeyID permissionKey)
{
	sptr<BProcess> team = Process();
	g_SystemBinderKey = team->AllocateKey();
	if (g_SystemBinderKey == kKALKeyIDNull)
	{
//#if BUILD_TYPE == BUILD_TYPE_DEBUG
		static const char* txt = "Failure allocating key variable for SystemBinder";
		ErrFatalError(txt);
//#endif
	}
	status_t err = SvcGetKey("SystemBinder", g_SystemBinderKey);
	if (err != errNone)
	{
//#if BUILD_TYPE == BUILD_TYPE_DEBUG
		printf("SvcGetKey(SystemBinder) failed in process %s (#%ld): %s\n", SysProcessName(), SysProcessID(), strerror(err));
//#endif
		team->FreeKey(g_SystemBinderKey);
		return NULL;
	}

	// Start with a fresh IPC packet
	KALTimedBinderIPCArgsType ioSndArgs;
	memset(&ioSndArgs, 0, sizeof(ioSndArgs));
	ioSndArgs.args.opcode1 = OP_GET_CONTEXT;
	ioSndArgs.args.opcode2 = which;
	if (permissionKey != kKALKeyIDNull) {
		ioSndArgs.args.numKeys = 1;
		ioSndArgs.args.keys[0] = permissionKey;
	}
	// XXX remove me soon
	ioSndArgs.args.binder_op = kKALBinderOp_Call;
	ioSndArgs.timeout_flags = B_WAIT_FOREVER;
	// make the call
	err = KALBinderCall(g_SystemBinderKey, &ioSndArgs);
	if (err != errNone)
	{
//#if BUILD_TYPE == BUILD_TYPE_DEBUG
		char msg[128];
		sprintf(msg, "KALBinderCall(SystemBinder) failed in process %s (#%ld): %s", SysProcessName(), SysProcessID(), strerror(err));
		ErrFatalError(msg);
//#endif

		return NULL;
	}
	SParcel *replyBuffer = SParcel::GetParcel();
	uint8_t * const meta = (uint8_t *)malloc(META_DATA_SIZE);
	if (replyBuffer == NULL || meta == NULL)
	{
//#if BUILD_TYPE == BUILD_TYPE_DEBUG
		static const char* txt = "Out of memory while trying to retrieve SystemBinder";
		ErrFatalError(txt);
//#endif

		return NULL;
	}

	replyBuffer->Reserve(MAX_TRANSACTION_SIZE);
	memset(&ioSndArgs, 0, sizeof(ioSndArgs));
	ioSndArgs.args.data1 = meta;
	ioSndArgs.args.sizeData1 = META_DATA_SIZE;
	ioSndArgs.args.data2 = replyBuffer->EditData();
	ioSndArgs.args.sizeData2 = MAX_TRANSACTION_SIZE; // XXX
	err = KALBinderReceive(&ioSndArgs.args, B_WAIT_FOREVER, 0);
	if (err != errNone) {
//#if BUILD_TYPE == BUILD_TYPE_DEBUG
		char msg[128];
		sprintf(msg, "KALBinderReceive(SystemBinder) failed in process %s (#%ld): %s", SysProcessName(), SysProcessID(), strerror(err));
		ErrFatalError(msg);
//#endif

		return NULL;
	}

	team->UnpackBinderArgs(&ioSndArgs.args, 0, 0, *replyBuffer);
	free(meta);

	sptr<IBinder> result = replyBuffer->ReadBinder();
	// B_DEFAULT_CONTEXT should always be found, B_SYSTEM_CONTEXT
	// might fail because of security
	if (result == NULL && which == B_DEFAULT_CONTEXT) {
		static const char* txt = "SystemBinder not found in reply parcel";
		ErrFatalError(txt);
	}

	SParcel::PutParcel(replyBuffer);
	return result;
}

void
SLooper::_KillProcess()
{
	printf("*** SLooper::_KillProcess() not implemented for PalmOS\n");
}

int32_t SLooper::_LoopTransactionSelf()
{
	// Start with a fresh IPC packet
	KALBinderIPCArgsType ioSndArgs;
#if STACK_META_DATA
	uint8_t meta[META_DATA_SIZE];
#else
	uint8_t * const meta = (uint8_t *)malloc(META_DATA_SIZE);
	if (!meta) return B_NO_MEMORY;
#endif
	SParcel *replyBuffer = SParcel::GetParcel();
	if (!replyBuffer) {
#if STACK_META_DATA
#else
		free(meta);
#endif
		return B_NO_MEMORY;
	}
	memset(&ioSndArgs, 0, sizeof(ioSndArgs));
	ioSndArgs.data1 = meta;
	int32_t result = _HandleResponse(&ioSndArgs, meta, replyBuffer, true);
	SParcel::PutParcel(replyBuffer);
#if STACK_META_DATA
#else
	free(meta);
#endif
	return result;
}

void SLooper::_Signal()
{
	m_team->Shutdown();
	// kTimeoutPoll == 0 == immediate timeout
	KALBinderTimeout(kKALKeyIDProcess, B_POLL, 0, sysThreadPriorityNormal);
}

int32_t SLooper::_LoopSelf()
{
	TRACE();
	return B_ERROR;
}

status_t
SLooper::IncrefsHandle(int32_t handle)
{
	B_PERFORMANCE_NODE(B_PERFORMANCE_WEIGHT_LIGHT, "SLooper::IncrefsHandle()");

#if BINDER_DEBUG_MSGS
	bout << "Writing increfs for handle " << handle << endl;
#else
	//unused parameter
	(void) handle;
#endif
	return B_OK;
}

/* Called when the last weak BpBinder goes away */
status_t
SLooper::DecrefsHandle(int32_t handle)
{
	B_PERFORMANCE_NODE(B_PERFORMANCE_WEIGHT_LIGHT, "SLooper::DecrefsHandle()");

#if BINDER_DEBUG_MSGS
	bout << "Writing decrefs for handle " << handle << endl;
#endif
	// Tell the kernel we don't need this any more
	Process()->DisposeReference(handle, true);
	return B_OK;
}

status_t
SLooper::AcquireHandle(int32_t handle)
{
	B_PERFORMANCE_NODE(B_PERFORMANCE_WEIGHT_EMPTY, "SLooper::AcquireHandle()");

#if BINDER_DEBUG_MSGS
	bout << "Writing acquire for handle " << handle << endl;
#else
	//unused parameter
	(void) handle;
#endif
	return B_OK;
}

/* Called when the last strong BpBinder disappears */
status_t
SLooper::ReleaseHandle(int32_t handle)
{
#if BINDER_DEBUG_MSGS
	bout << "Writing release for handle " << handle << endl;
#endif
	// Tell the kernel we don't need this any more
	Process()->DisposeReference(handle, false);
	return B_OK;
}

status_t
SLooper::AttemptAcquireHandle(int32_t handle)
{
	B_PERFORMANCE_NODE(B_PERFORMANCE_WEIGHT_LIGHT, "SLooper::AttemptAcquireHandle()");

#if BINDER_DEBUG_MSGS
	bout << "Writing attempt acquire for handle " << handle << endl;
#else
	//unused parameter
	(void) handle;
#endif
	uint32_t found;
	nsecs_t timeout = B_MICROSECONDS(10);
	const nsecs_t maxTimeout = B_MILLISECONDS(10);
	while ((found = Process()->RecoverStrongReference((uint16_t)handle)) == BProcess::INC_STRONG_PENDING) {
		// yield
		SysThreadDelay(timeout, B_RELATIVE_TIMEOUT);
		if (timeout < maxTimeout)
		{
			timeout <<= 1;
		}
	}
	if (found == BProcess::RECOVERED_STRONG) return B_OK;
	// at this point, found == BProcess::ATTEMPT_INC_STRONG
	// make a transaction to ATTEMPT_INC_STRONG
	SParcel *parcel = SParcel::GetParcel();
	status_t result;
	if (parcel) {
		// NOTE: only try to strengthen WEAK handles
		result = Transact(handle | WEAK_REF, OP_ATTEMPT_INC_STRONG, *parcel, 0, OP_FLAGS_USER_CODE);
		SParcel::PutParcel(parcel);
	} else {
		result = B_NO_MEMORY;
	}
	Process()->FinishStrongRecovery((uint16_t)handle);
	return result;
}

status_t
SLooper::_HandleResponse(KALBinderIPCArgsType *ioSndArgs, uint8_t *meta, SParcel *replyBuffer, bool spawnLoopers)
{
	BProcess* team = m_team.ptr();		// for fast access.
	uint32_t code;
	status_t result = B_OK;
	bool notReceivedReply = true;
#if DEBUG_HANDLER_DISPATCH
	uint32_t dh_spawns = 0, dh_dispatch = 0;
	uint32_t tr_spawns = 0, tr_dispatch = 0;
	nsecs_t thread_life = SysGetRunTime();
#endif
#if TRACE_CONCURRENCY
	bool firstTime = true;
#endif
#if DEBUG
	bool waiting_for_inc_strong = ioSndArgs->opcode1 == OP_ATTEMPT_INC_STRONG;
	bool unpacked_args = false;
	bool has_binders = false;
#endif

#if TRACE_TRANSACTIONS
	printf("_HandleResponse(..., %s)\n", spawnLoopers ? "true":"false");
#endif
	SParcel *remoteReply = NULL;
	while (notReceivedReply)
	{
		IBinder *target = 0;
		bool needSpawn = false;
		ASSERT(!has_binders || unpacked_args);
		if (remoteReply) {
			remoteReply->Reset();
			ASSERT(remoteReply->EditData());
		}
		// ensure replyBuffer has enough space
		replyBuffer->Reserve(MAX_TRANSACTION_SIZE);
		ASSERT(replyBuffer->EditData());
		// no need to memset -- we are filling in all args.
		//memset(ioSndArgs, 0, sizeof(*ioSndArgs));
		ioSndArgs->data1 = meta;
		ioSndArgs->sizeData1 = META_DATA_SIZE; // XXX
		ioSndArgs->data2 = replyBuffer->EditData();
		ioSndArgs->sizeData2 = MAX_TRANSACTION_SIZE; // XXX
		ioSndArgs->numKeys = MAX_KEY_TRANSFER;
		ioSndArgs->numRefs = 0;
		{
			int i = MAX_KEY_TRANSFER;
			KeyID *keys = m_keys;
			KeyID *argKeys = ioSndArgs->keys;
			do {
				if (*keys == kKALKeyIDNull) *keys = team->AllocateKey();
				*argKeys++ = *keys++;
			} while (--i);
		}

restartKALBinderReceive:
#if 0
		if (team->m_refCounts.flag) bout << SPrintf("* * * * %04x:%08x: Pending PutRefs before KALBinderReceive()", s_processID, SysCurrentThread()) << endl;
#endif
		team->BatchPutReferences();
		if (spawnLoopers) {
			team->TransactionThreadReady();
		}
#if TRACE_TRANSACTIONS
		bout << "Thread " << SPrintf("%8x", SysCurrentThread()) << "/" << team << ": Waiting for a new transaction..." << endl;
#endif
		if (spawnLoopers) {
#if TRACE_CONCURRENCY
			if (firstTime) {
				EXIT_CONCURRENCY(true, Spawns);
				firstTime = false;
			}
#endif
			result = KALBinderReceive(  ioSndArgs, B_RELATIVE_TIMEOUT,
			                            B_SECONDS_TO_NANOSECONDS(1<<4) );
		} else {
			result = KALBinderReceive(ioSndArgs, B_WAIT_FOREVER, 0);
			// XXX THIS IS BAD!
			// There are two situations that can happen here, one is
			// the target object dying and the other is the kernel
			// having to return a timeout for some reason.  Unfortunately
			// it sends a timeout error code in both cases.  There are
			// situations we are seeing where a dead object result
			// should be returned, so we'll assume that if some error
			// code is returned that it is dead and treat it that way.
			// Whatever.
			if (result != B_OK) {
				printf("*****************************************\n");
				printf("UNEXPECTED RECEIVE RESULT: %p\n", result);
				result = B_BINDER_DEAD;
				// if someone called us from Transact(), then give them the bad news
				if (!spawnLoopers) break;

				// We belong to the thread pool.  Return the bad news to the remote invoker.
				goto sendReplyError;
			}
			//DbgOnlyFatalErrorIf(result != errNone, "Error in middle of transaction!");
		}

		ENTER_CONCURRENCY(spawnLoopers, Any, " now running ", " threads");
#if DEBUG
		unpacked_args = false;
		has_binders = ioSndArgs->numRefs != 0;
#endif

		//The receiving transaction for debugging.
		//KALBinderIPCArgsType receiveArgs(*ioSndArgs);

		// We don't have any idea what priority the thread is now running at.
		m_priority = -1;
#if TRACE_TRANSACTIONS
		bout << "Thread " << SPrintf("%8x", SysCurrentThread()) << "/" << team << ": Returned from KALBinderReceive: "
			<< (void*)result << ", opcode " << ioSndArgs->binder_op << endl << indent
			<< "numRefs: " << ioSndArgs->numRefs << ", sizeData1: " << ioSndArgs->sizeData1
			<< ", sizeData2: " << ioSndArgs->sizeData2 << dedent << endl;
#endif
		if (spawnLoopers) {
			needSpawn = team->TransactionThreadBusy();
		}
		if (result == kalErrReceiveTimeout) {
#if TRACE_TRANSACTIONS || TRACE_THREAD_TIMEOUTS
			bout << SPrintf("%04x:%08x:Timeout received, needSpawn: %ld\n", s_processID, SysCurrentThread(), (int32_t)needSpawn);
#endif
			if (spawnLoopers) {
				if (!team->LooperTimeout() || (needSpawn && !team->IsShuttingDown())) {
					/*
					* take a cheap shot, if BProcess says we need more loopers and we
					* are about to die, just requeue ourselves, rather than spawning
					* a new one to take our place while we die...
					*/
					EXIT_CONCURRENCY(spawnLoopers, Any);
					goto restartKALBinderReceive;
				}
			}
#if TRACE_TRANSACTIONS || TRACE_THREAD_TIMEOUTS
			bout << SPrintf("%04x:%08x:Timing out of _HandleResponse()\n", s_processID, SysCurrentThread());
#endif
			result = B_OK;
			EXIT_CONCURRENCY(spawnLoopers, Any);
			break; // out of while loop and clean up the keys
		}

		// ensure we have at least one thread waiting
		if (spawnLoopers && needSpawn) {
#if DEBUG_HANDLER_DISPATCH
			if (result == kalErrBinderPoolTimeout) {
				dh_spawns++;
			} else {
				tr_spawns++;
			}
#endif
#if TRACE_THREADS
			bout << "Spawning binder thread #" << g_transSeq << " in " << team->ID() << " from "
				<< (result == kalErrBinderPoolTimeout ? "handler" : "transaction")
				<< ", " << team->TransactionThreadWaitingCount() << " waiting" << endl;
#endif
			char code = 'U';
#if BUILD_TYPE == BUILD_TYPE_DEBUG
			if (result == kalErrBinderPoolTimeout) code = 'H';
			if (ioSndArgs->binder_op == kKALBinderOp_Call) {
				if (ioSndArgs->opcode1 == OP_ATTEMPT_INC_STRONG) code = 'A';
				else if (ioSndArgs->opcode1 == OP_TRANSACT) code = 'T';
				else if (ioSndArgs->opcode1 == OP_ERROR) code = 'E';
				else if (ioSndArgs->opcode1 == OP_GET_CONTEXT) code = 'C';
				else if (ioSndArgs->opcode1 == OP_GET_HOST_BINDER_INFO) code = 'I';
			} else if (ioSndArgs->binder_op == kKALBinderOp_Reply) code = 'R';
			else if (ioSndArgs->binder_op == kKALBinderOp_DecRef) code = 'D';
			else if (ioSndArgs->binder_op == kKALBinderOp_LostObj) code = 'L';
#endif
			ENTER_CONCURRENCY(spawnLoopers, Spawns, " now spawning ", " new threads");
			if (_SpawnTransactionLooper(code) != B_OK) {
				// This is really bad, but let's not make it worse.
				// We thought there would be a thread ready for more
				// transactions but it looks like that isn't the case.
#if TRACE_THREADS
				bout << "*** FAILURE SPAWNING TRANSACTION LOOPER!  Trying to clean up..." << endl;
#endif
				EXIT_CONCURRENCY(spawnLoopers, Spawns);
				team->TransactionThreadForceRemove();
			}
		}

		if (result == kalErrBinderPoolTimeout) {
			DbgOnlyFatalErrorIf(!spawnLoopers, "Event timeout in a nested binder thread!");
#if TRACE_EVENT_SCHEDULING
			bout << "Looper in " << (void*)s_processID << " has received a kalErrBinderPoolTimeout!" << endl;
#endif
			if (!team->IsShuttingDown()) {
#if DEBUG_HANDLER_DISPATCH
				dh_dispatch++;
#endif
				ENTER_CONCURRENCY(spawnLoopers, Handlers, " now running ", " handlers");
				team->DispatchMessage(this);
				AssertNoLocks();
				EXIT_CONCURRENCY(spawnLoopers, Handlers);
				EXIT_CONCURRENCY(spawnLoopers, Any);
				goto restartKALBinderReceive;
			}
#if TRACE_THREAD_TIMEOUTS
			bout << "kalErrBinderPoolTimeout and team->IsShuttingDown()" << endl;
#endif
			// wake up the next dormant thread
			KALBinderTimeout(kKALKeyIDProcess, B_POLL, 0, sysThreadPriorityNormal);
			result = B_OK;
			EXIT_CONCURRENCY(spawnLoopers, Any);
			break; // out of while loop and clean up the keys
		}
		ASSERT(result == errNone);

#if DEBUG_HANDLER_DISPATCH
		tr_dispatch++;
#endif
		switch (ioSndArgs->binder_op)
		{
#if 1
			case kKALBinderOp_Nop:
				bout << "*** kKALBinderOp_Nop issued" << endl;
				goto skip_reply;
				break;
			case kKALBinderOp_GetRefs:
				bout << "*** kKALBinderOp_GetRefs issued" << endl;
				goto skip_reply;
				break;
			case kKALBinderOp_PutRefs:
				bout << "*** kKALBinderOp_PutRefs issued" << endl;
				// XXX The references in question have ceased to exist in all remote processes.
				goto skip_reply;
				break;
#endif
			case kKALBinderOp_Call:

				// Verify we have enough stack.
				if (_AvailStack() < MIN_STACK_SPACE) {
					DbgOnlyFatalError("Not enough stack to handle binder call.");
					result = B_BINDER_OUT_OF_STACK;
					goto sendReplyError;
				}

				// handle what we got back
				team->UnpackBinderArgs(ioSndArgs, &target, &code, *replyBuffer);
#if DEBUG
				unpacked_args = true;
#endif
				// forget any key we put into the parcel
				{
					int i = ioSndArgs->numKeys;
					if (i) {
						KeyID *keys = m_keys;
						do { *keys++ = kKALKeyIDNull; } while (--i);
					}
				}

				switch (ioSndArgs->opcode1)
				{
					case OP_ATTEMPT_INC_STRONG: {
						ENTER_CONCURRENCY(spawnLoopers, AttemptIncs, " now executing ", " OP_ATTEMPT_INC_STRONG");
						//memset(ioSndArgs, 0, sizeof(*ioSndArgs));
						ioSndArgs->opcode1 = OP_ATTEMPT_INC_STRONG;
						ioSndArgs->sizeData1 = 0;
						ioSndArgs->sizeData2 = 0;
						ioSndArgs->numKeys = 0;
						ioSndArgs->numRefs = 0;
						team->m_handleRefLock.Lock();
						result = B_BINDER_INC_STRONG_FAILED;
						if (target) {
							uint16_t ref = team->ReferenceForLocalBinder((uint32_t)target);
							if (ref < BAD_REF) {
								const BProcess::handleInfo *hi = team->m_handleRefs[ref];
								if (hi->strong == 0) {
									ASSERT(hi->atom);
									result = B_ATTEMPT_INC_STRONG(hi->atom, (void*)s_processID) ? B_OK : B_BINDER_INC_STRONG_FAILED;
								}
								else result = B_OK;
								if (result == B_OK) {
									ioSndArgs->numRefs = 2;
									ioSndArgs->refs[0] = ref;
									ioSndArgs->refs[1] = team->m_binderRef;
									team->IncrementReferenceCounts(ref, false, true);
									result = B_OK;
#if 0
									bout << SPrintf("ref, key: %d,%d", ioSndArgs->refs[0], ioSndArgs->refs[1]) << endl;
#endif
								}
							}
						}
						ioSndArgs->opcode2 = result;
						team->m_handleRefLock.Unlock();
#if 0
						bout << SPrintf("%04x:%08x OP_ATTEMPT_INC_STRONG : %08x", s_processID, SysCurrentThread(), result) << endl;
#endif
						EXIT_CONCURRENCY(spawnLoopers, AttemptIncs);
						goto sendReplyNoPacking;
					} break;
					case OP_TRANSACT:
						if (!remoteReply) {
							remoteReply = SParcel::GetParcel();
							if (remoteReply == NULL) {
								result = B_NO_MEMORY;
								goto sendReplyError;
							}
						}
						ENTER_CONCURRENCY(spawnLoopers, Transacts, " now executing ", " OP_TRANSCACT");
						// call local target with code and *replyBuffer
						result = target ?
							target->Transact(code, *replyBuffer, remoteReply) :
							B_BINDER_DEAD;
						EXIT_CONCURRENCY(spawnLoopers, Transacts);
						break;
					case OP_ERROR:
						if (ioSndArgs->sizeData2 >= sizeof(status_t)) {
							result = *(status_t*)ioSndArgs->data2;
						} else {
							result = B_ERROR;
						}
						break;
					case OP_GET_CONTEXT:
					{
						result = B_PERMISSION_DENIED;	// we are pessimistic

						// is this a valid context?
						if (ioSndArgs->opcode2 < g_binderContext->CountItems())
						{
							// do we require permission for this context?
							KeyID key = g_keyContext->ItemAt(ioSndArgs->opcode2);
							if (key == kKALKeyIDNull || (ioSndArgs->keys[0] != kKALKeyIDNull && KALKeyCompare(key, ioSndArgs->keys[0])))
							{
								result = B_OK;
							}
							else
							{
								SecSvcsDeviceSettingEnum securityLevel;
								status_t err = SecSvcsGetDeviceSetting(&securityLevel);
								if (err == B_OK && securityLevel == SecSvcsDeviceSecurityNone)
								{
									// if the security level is set to none - then allow access
									result = B_OK;
								}
							}
					
							// if authorized send the context. 
							if (result == B_OK)
							{
								if (!remoteReply) {
									remoteReply = SParcel::GetParcel();
									if (remoteReply == NULL) {
										result = B_NO_MEMORY;
										goto sendReplyError;
									}
								}
								remoteReply->WriteBinder(g_binderContext->ItemAt(ioSndArgs->opcode2));
							}
						}
					} break;
#if BUILD_TYPE == BUILD_TYPE_DEBUG
					case OP_GET_HOST_BINDER_INFO: {
						if (!remoteReply) {
							remoteReply = SParcel::GetParcel();
							if (remoteReply == NULL) {
								result = B_NO_MEMORY;
								goto sendReplyError;
							}
						}
						remoteReply->WriteInt32(s_processID);
						remoteReply->WriteInt32(ioSndArgs->refs[0]);
						remoteReply->WriteInt32((int32_t)target);
						const char *name = 0;
						size_t namelen = 0;
						if (target) {
#if _SUPPORTS_RTTI
							name = typeid(*target).name();
#else
							name = "no typeid info";
#endif
							namelen = strlen(name);
						}
						remoteReply->WriteInt32(namelen);
						remoteReply->Write(name, namelen);
					} break;
#endif
					default:
						// XXX Uh, ooops.
						DbgOnlyFatalError("kKALBinderOp_Call with unknown opcode1 issued");
						break;
				}
				break;
			case kKALBinderOp_Reply:

				notReceivedReply = false;
				switch (ioSndArgs->opcode1)
				{
					case OP_ATTEMPT_INC_STRONG:
						ASSERT(waiting_for_inc_strong);
						result = ioSndArgs->opcode2;
						if (result == B_OK) {
							team->RefsReceived(ioSndArgs, false);
							// mark the local handle as having a strong reference
							team->StashRemoteBinderRef(ioSndArgs->refs[0], ioSndArgs->refs[1]);
							team->RefsReceived(ioSndArgs, true);
						}
						break;
					case OP_ERROR:
						// Handle returned errors.
						if (ioSndArgs->sizeData2 >= sizeof(status_t)) {
							result = *(status_t*)ioSndArgs->data2;
						} else {
							result = B_ERROR;
						}
						replyBuffer->Reference(NULL, result);
						break;
					default:
						// XXX check for errors
						// handle what we got back
						team->UnpackBinderArgs(ioSndArgs, 0, &code, *replyBuffer);
#if DEBUG
						unpacked_args = true;
#endif
						// forget any key we put into the parcel
						{
							int i = ioSndArgs->numKeys;
							if (i) {
								KeyID *keys = m_keys;
								do { *keys++ = kKALKeyIDNull; } while (--i);
							}
						}
						result = B_OK;
				}
				EXIT_CONCURRENCY(spawnLoopers, Any);
				continue;
				break;
			case kKALBinderOp_DecRef:
				ENTER_CONCURRENCY(spawnLoopers, DecRefs, " now executing ", " kKALBinderOp_DecRef");
				// The last external copies of these reference have disappeared.
#if DEBUG_BINDER_OPS || TRACE_DECREFS || TRACE_BATCH_PUTS
				bout << SPrintf("*** kKALBinderOp_DecRef in %04x:%08x issued for %d references.", s_processID, SysCurrentThread(), ioSndArgs->numRefs) << endl;
#endif
				{ // !@#$^ VC6 scoping rules
					for (int i = 0; i < ioSndArgs->numRefs; i++)
					{
#if DEBUG_BINDER_OPS || TRACE_DECREFS || TRACE_BATCH_PUTS
						bout << SPrintf("** %04x", ioSndArgs->refs[i]) << endl;
#endif
						team->DecrementReferenceCount(ioSndArgs->refs[i], 1);
					}
				}
#if DEBUG_BINDER_OPS || TRACE_DECREFS || TRACE_BATCH_PUTS
				bout << endl;
#endif
				EXIT_CONCURRENCY(spawnLoopers, DecRefs);
#if DEBUG
				unpacked_args = true;
#endif
				goto skip_reply;
				break;
			case kKALBinderOp_LostObj:
				ENTER_CONCURRENCY(spawnLoopers, LostObjs, " now executing ", " kKALBinderOp_LostObj");
				// The remote app that hosted these references has disappeared
#if DEBUG_LOSTOBJ
				bout << SPrintf("*** kKALBinderOp_LostObj in %04x:%08x issued for %d references.", s_processID, SysCurrentThread(), ioSndArgs->numRefs) << endl;
				bout << " **";
				for (int i = 0; i < ioSndArgs->numRefs; i++) bout << " " << ioSndArgs->refs[i];
				bout << endl;
#endif
				for (int i = 0; i < ioSndArgs->numRefs; i++) team->Loose(ioSndArgs->refs[i]);
				EXIT_CONCURRENCY(spawnLoopers, LostObjs);
#if DEBUG
				unpacked_args = true;
#endif
				goto skip_reply;
				break;
			default:
				DbgOnlyFatalError("Unknown Binder opcode received!");
				goto skip_reply;
				break;
		}

		//memset(ioSndArgs, 0, sizeof(*ioSndArgs));
		ioSndArgs->data1 = meta;
		// Use BAD_REF for the reply target, since we don't know and don't care
		m_team->_Lock();
		result = team->PackBinderArgsLocked(BAD_REF, result, *remoteReply, ioSndArgs);
		m_team->_Unlock();
		if (result == B_OK) {
			ioSndArgs->opcode1 = OP_REPLY;
		} else {
			ioSndArgs->opcode1 = OP_ERROR;
		}

sendReplyNoPacking:
#if 0
		if (team->m_refCounts.flag) bout << SPrintf("* * * * %04x:%08x: Pending PutRefs before KALBinderReply()", s_processID, SysCurrentThread()) << endl;
#endif
		// XXX Putting this here causes bperf --test-remote-same-binder not to leak weak refs!
		// replyBuffer->Reserve(0);

		// FFB XXX
		// team->BatchPutReferences();
		// send the response
		// XXX shouldn't need this next line after Manuel fixes the kernel
		ioSndArgs->binder_op = kKALBinderOp_Reply;
		ioSndArgs->refsBump = 0;
#if TRACE_TRANSACTIONS
		bout << "Thread " << SPrintf("%8x", SysCurrentThread()) << "/" << team;
		if (remoteReply) bout << " returning reply: " << *remoteReply;
		bout << endl;
#endif
		result = KALBinderReply(ioSndArgs, B_WAIT_FOREVER, 0);
		// ASSERT(result == errNone);
		// cleanup any bumped references
		// We do this before checking for an error to keep the reference counts accurate.
		team->BumpAfterSend(ioSndArgs);
		if (result != errNone)
		{
			// the other side probally died! so if we are the top level
			// thread then we can just continue. other wise set that we
			// did not recevie the reply and get the hell out of dodge.
			if (spawnLoopers)
				result = B_OK;
		}

		// If something has happened to this parcel such that we can
		// not re-use it, let the parcel cache get rid of it and then
		// we will get a new parcel next time we need it.
		if (remoteReply && !remoteReply->IsCacheable()) {
			SParcel::PutParcel(remoteReply);
			remoteReply = NULL;
		}

skip_reply:
		if (spawnLoopers) AssertNoLocks();
		EXIT_CONCURRENCY(spawnLoopers, Any);
		continue;

sendReplyError:
		ioSndArgs->numRefs = 0;
		ioSndArgs->numKeys = 0;
		ioSndArgs->opcode1 = OP_ERROR;
		ioSndArgs->sizeData1 = 0;
		ioSndArgs->data1 = NULL;
		ioSndArgs->sizeData2 = sizeof(status_t);
		ioSndArgs->data2 = &result;
		goto sendReplyNoPacking;
	}

	if (remoteReply)
		SParcel::PutParcel(remoteReply);

	if (!notReceivedReply && spawnLoopers) {
		DbgOnlyFatalError("Transaction thread exiting its top-level loop with something besides a timeout.");
	}

dead_in_da_water:

#if DEBUG_HANDLER_DISPATCH
	if (spawnLoopers) {
		bout << SPrintf("%04x:%08x Handler Dispatch Stats", s_processID, SysCurrentThread()) <<
			" spawns: " << dh_spawns <<
			" dispatch: " << dh_dispatch << endl;
		bout <<     "          Transaction Dispatch Stats" <<
			" spawns: " << tr_spawns <<
			" dispatch: " << tr_dispatch << endl;
		bout << "Thread Lifetime: " << (double)(SysGetRunTime() - thread_life) / (double)B_ONE_SECOND << endl;
	}
#endif

	// take the opportunity to clean up on the way out
	team->BatchPutReferences();
	return result;
}

#if DEBUG_PARCEL_POOL
uint32_t dg_needParcel = 0;
uint32_t dg_transactions = 0;
#endif
status_t
SLooper::Transact(int32_t handle, uint32_t code, const SParcel& data,
	SParcel* localReply, uint32_t flags)
{
	B_PERFORMANCE_NODE(B_PERFORMANCE_WEIGHT_HEAVY, "SLooper::Transact()");
	BINDER_IPC_PROFILE_STATE;

	status_t result = data.ErrorCheck();
	if (result != B_OK) {
		// Reflect errors back to caller.
		if (localReply) localReply->Reference(NULL, data.Length());
		SetLastError(result);
		return result;
	}

	// shortcut determining if the remote host died.
	if (UNDECORATED_REF(handle) >= BAD_REF || m_team->Lost((uint16_t)handle)) {
		SetLastError(B_BINDER_DEAD);
		return B_BINDER_DEAD;
	}
#if STACK_META_DATA
	uint8_t meta[META_DATA_SIZE];
#else
	uint8_t * const meta = (uint8_t *)malloc(META_DATA_SIZE);
	if (!meta) {
		SetLastError(B_NO_MEMORY);
		return B_NO_MEMORY;
	}
#endif

	BEGIN_BINDER_CALL();

	// get a few things for chatting with the kernel
#if TRACE_TRANSACTIONS
	bout << "Thread " << SPrintf("%8x", SysCurrentThread()) << "/" << m_team << ": Get remote binder key." << endl;
#endif


	m_team->_Lock();
		KeyID key = m_team->GetRemoteBinderKeyLocked((uint16_t)handle);
		bool deleteReply = localReply == 0;

#if DEBUG_PARCEL_POOL
		// NOT THREAD SAFE! (but we don't care in this case)
		if (deleteReply) dg_needParcel++;
		dg_transactions++;
#endif
		// Start with a fresh IPC packet
		KALTimedBinderIPCArgsType ioSndArgs;
		ioSndArgs.args.data1 = meta;
		ioSndArgs.timeout_flags = B_WAIT_FOREVER;

#if 0
		if (handle == 17)
			bout << SPrintf("Transact(%04x) begins", handle) << endl;
#endif
		// Let our BProcess fill in the IPC args for us, since it requires lots of team-specific info
		result = m_team->PackBinderArgsLocked((uint16_t)handle, code, data, &ioSndArgs.args);
	m_team->_Unlock();

	if (result == B_OK)
	{
		ioSndArgs.args.opcode1 = ((flags == OP_FLAGS_USER_CODE) ? code : OP_TRANSACT);
		// XXX Temporary -- use a timeout here, to work around a bug where we won't
		// wake up in certain situations with an infinite timeout.
		ioSndArgs.timeout_flags = B_RELATIVE_TIMEOUT;
		ioSndArgs.timeout = B_S2NS(1);
		// clean up any outstanding PutRefs
		// ?? m_team->BatchPutReferences();
		// make the call
		ioSndArgs.args.binder_op = kKALBinderOp_Call;
		ioSndArgs.args.refsBump = 0;
#if TRACE_TRANSACTIONS
		bout << "Thread " << SPrintf("%8x", SysCurrentThread()) << "/" << m_team << ": KALBinderCall." << endl;
#endif
		result = KALBinderCall(key, &ioSndArgs);
		//DbgOnlyFatalErrorIf(result != errNone, "Error returned from call!");

#if 0
		if (handle == 17)
			bout << SPrintf("Transact(%04x) returns %08x", handle, result) << endl;
#endif
		// account for incrementing references
		m_team->BumpAfterSend(&ioSndArgs.args);

		if (result == errNone)
		{
			// prepare to get the result
			if (deleteReply) localReply = SParcel::GetParcel();

#if TRACE_TRANSACTIONS
			bout << "Thread " << SPrintf("%8x", SysCurrentThread()) << "/" << m_team << ": Getting respose from transaction..." << endl;
#endif
			if (localReply) {
				result = _HandleResponse(&ioSndArgs.args, meta, localReply);
				//DbgOnlyFatalErrorIf(result != errNone, "Bad response!");
				if (deleteReply) SParcel::PutParcel(localReply);
			} else {
				result = B_NO_MEMORY;
			}
		}
		else
			result = B_BINDER_DEAD;
	}
	
	// clean up any outstanding PutRefs
	// _HandleResponse() called this before it returned
	// m_team->BatchPutReferences();

	FINISH_BINDER_CALL();

#if STACK_META_DATA
#else
	// clean up meta buffer
	free(meta);
#endif
	if (result >= B_OK) return B_OK;
	// An inc strong failure is not a looper IPC failure.
	if (result != B_BINDER_INC_STRONG_FAILED) SetLastError(result);
	return result;
}

ptrdiff_t
SLooper::_AvailStack(void) const
{
	return ptrdiff_t(KALPrivGetStackPointer() - VAddr(m_stackBottom));
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
