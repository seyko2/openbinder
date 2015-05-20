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
#include <SysThread.h>
#include <SysThreadConcealed.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <syslog.h>
#include <ErrorMgr.h>
#include <support/CallStack.h>
#include <support_p/DebugLock.h>
#include <support_p/binder_module.h>
#include <support_p/RBinder.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#define DSHUTDOWN(x) //_x

#define LOOPER_STACK_SIZE (8*1024)
#define SYSTEM_DIRECTORY "/opt/palmos"
#define BINDER_VM_SIZE (8*1024*1024)

#if CPU_TYPE == CPU_x86
#	define CPU_TYPE_STRING "LINUX_X86"
#elif CPU_TYPE == CPU_ARM
#	define CPU_TYPE_STRING "LINUX_ARM"
#endif

#if BUILD_TYPE == BUILD_TYPE_DEBUG
#	define BUILD_TYPE_STRING "Debug"
#elif BUILD_TYPE == BUILD_TYPE_RELEASE
#	define BUILD_TYPE_STRING "Release"
#endif

#define ioctl_binder(fd, op, ptr, size) ioctl(fd, op, ptr)

static SLooper::catch_root_func g_catchRootFunc = NULL;
static bool s_managesContexts = false;
static int32_t s_binderDesc = -1;
static void *s_vmstart = MAP_FAILED;
static int32_t g_openBuffers = 0;

#if BINDER_DEBUG_MSGS
static const char *inString[] = {
	"brOK",
	"brTIMEOUT",
	"brWAKEUP",
	"brTRANSACTION",
	"brREPLY",
	"brACQUIRE_RESULT",
	"brDEAD_REPLY",
	"brTRANSACTION_COMPLETE",
	"brINCREFS",
	"brACQUIRE",
	"brRELEASE",
	"brDECREFS",
	"brATTEMPT_ACQUIRE",
	"brEVENT_OCCURRED",
	"brNOOP",
	"brSPAWN_LOOPER",
	"brFINISHED"
};
#endif


bool
SLooper::ProcessManagesContexts(void)
{
	return s_managesContexts;
}

bool
SLooper::BecomeContextManager(ContextPermissionCheckFunc checkFunc, void* userData)
{
	if (!s_managesContexts) {
		g_binderContextAccess->Lock();
		g_binderContextCheckFunc = checkFunc;
		g_binderContextUserData = userData;
		if (s_binderDesc >= 0) {
			int dummy = 0;
			status_t result = ioctl_binder(s_binderDesc, BINDER_SET_CONTEXT_MGR, &dummy, sizeof(dummy));
			if (result == 0) {
				s_managesContexts = true;
			} else if (result == -1) {
				g_binderContextCheckFunc = NULL;
				g_binderContextUserData = NULL;
				berr << "binder ioctl to become context manager failed: " << strerror(errno) << endl;
			}
		} else {
			// If there is no driver, our only world is the local
			// process so we can always become the context manager there.
			s_managesContexts = true;
		}
		g_binderContextAccess->Unlock();
	}
	printf("BecomeContextManager(): %s\n", s_managesContexts ? "true" : "false");
	return s_managesContexts;
}

static int32_t g_haveVersion = 0;

static int32_t open_binder(int ignored)
{
	int32_t fd = open("/dev/binder", O_RDWR);
	if (fd >= 0) {
		fcntl(fd, F_SETFD, FD_CLOEXEC);
		if (!g_haveVersion) {
			binder_version_t vers;
			status_t result = ioctl_binder(fd, BINDER_VERSION, &vers, sizeof(vers));
			if (result == -1) {
				berr << "binder ioctl to obtain version failed: " << strerror(errno) << endl;
				close(fd);
				fd = -1;
			}
			if (result != 0 || vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION) {
				ErrFatalError("Binder driver protocol does not match user space protocol!");
				close(fd);
				fd = -1;
			}
		}
		
	} else {
		berr << "Opening '/dev/binder' failed: " << strerror(errno) << endl;
	}
	return fd;
}

void __initialize_looper_platform()
{
	//printf("__initialize_looper_platform()\n");
	g_binderContextAccess = new SLocker("LooperLinux.cpp:g_binderContext");
	g_binderContext = new SKeyedVector<SString, context_entry>();
	
	g_systemDirectoryLock = new SLocker("g_systemDirectory");
	g_systemDirectory = new SString();

	s_binderDesc = open_binder(0);
	if (s_binderDesc >= 0) {
		// mmap the binder, providing a chunk of virtual address space to receive transactions.
		s_vmstart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, s_binderDesc, 0);
		if (s_vmstart == MAP_FAILED) {
			// *sigh*
			berr << "Using /dev/binder failed: unable to mmap transaction memory." << endl;
			close(s_binderDesc);
			s_binderDesc = -1;
		}
	} else {
		// Prime the thread pool.
		SLooper::SpawnLooper();
	}
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

	// Stop all loopers.  Forcibly free any other SLooper objects
	// for threads we don't own.
	SLooper::Shutdown();

	// No more need for this thing.
	delete g_binderContextAccess;
	g_binderContextAccess = NULL;
	
	delete g_systemDirectoryLock;
	g_systemDirectoryLock = NULL;
	
	delete g_systemDirectory;
	g_systemDirectory = NULL;

	if (s_vmstart != MAP_FAILED) munmap(s_vmstart, BINDER_VM_SIZE);
	if (s_binderDesc >= 0) close(s_binderDesc);
}

SContext get_default_context()
{
	return SContext::UserContext();
}

SContext get_system_context()
{
	return SContext::SystemContext();
}

SString get_system_directory()
{
	if (*g_systemDirectory == "")
	{
		SLocker::Autolock lock(*g_systemDirectoryLock);
		if (*g_systemDirectory == "")
		{
			char buf[1024];
			int buflen = readlink("/proc/self/exe", buf, sizeof(buf)-1);
			buf[(buflen > 0) ? buflen : 0] = '\0';
			g_systemDirectory->PathSetTo(buf);
			
			// remove bin/smooved
			g_systemDirectory->PathGetParent(g_systemDirectory);
			g_systemDirectory->PathGetParent(g_systemDirectory);
		
			if (strcmp(g_systemDirectory->PathLeaf(), "palmos") != 0)
			{
				char* top = getenv("TOP");
				if (top != NULL)
				{
					g_systemDirectory->PathSetTo(top);
					g_systemDirectory->PathAppend("build");
					g_systemDirectory->PathAppend(CPU_TYPE_STRING);
					g_systemDirectory->PathAppend(BUILD_TYPE_STRING);
					g_systemDirectory->Append(SYSTEM_DIRECTORY);

					struct stat stbuf;
					if (stat(g_systemDirectory->String(), &stbuf) != 0 || !S_ISDIR(stbuf.st_mode))
					{
						g_systemDirectory->PathSetTo(SYSTEM_DIRECTORY);
					}
				}
			}
		}
		// berr << "Setting system directory to be '" << g_systemDirectory->String() << "'" << endl;
	}


	return *g_systemDirectory;
}


// -------------------- LINUX IMPLEMENTATION --------------------

void SLooper::Stop()
{
	DSHUTDOWN(printf("* * SLooper::Stop() is stopping all teams...\n"));
	SLooper::_ShutdownLoopers();
	DSHUTDOWN(printf("* * SLooper::Stop() is expunging this looper's images...\n"));
	SLooper* me = SLooper::This();
	DSHUTDOWN(printf("* * SLooper::Stop() releasing remote references...\n"));
	if (me) 
	{
		me->ExpungePackages();
	}
	DSHUTDOWN(printf("* * SLooper::Stop() is done!\n"));
}

void SLooper::Shutdown()
{
	DSHUTDOWN(printf("* * SLooper::Shutdown() is stopping all teams...\n"));
	SLooper::_ShutdownLoopers();
	DSHUTDOWN(printf("* * SLooper::Shutdown() is waiting for threads to exit...\n"));
	DSHUTDOWN(printf("* * SLooper::Shutdown() is cleaning the remains...\n"));
	SLooper::_CleanupLoopers();
	DSHUTDOWN(printf("* * SLooper::Shutdown() is done!\n"));
}

void SLooper::_ConstructPlatform()
{
	m_in.Reserve(128);
	m_in.SetLength(0);
	m_out.SetLength(0);
	
	m_next = NULL;
	m_spawnedInternally = true;
	m_rescheduling = false;
	m_signaled = 0;
	
	SysThreadExitCallbackID idontcare;
	SysThreadInstallExitCallback(_DeleteSelf, this, &idontcare);

	pthread_cond_init(&m_condition, NULL);
	pthread_mutex_init(&m_mutex, NULL);
	
#if BINDER_DEBUG_MSGS
	// berr	<< "SLooper: enter new looper #" << Process()->LooperCount() << " " << this << /* " ==> Thread=" << find_thread(NULL) << ", keyId=" << m_keyID << */ endl;
#endif
}

void SLooper::_DestroyPlatform()
{
	int dummy = 0;
	status_t result = B_OK;
	if (s_binderDesc >= 0)
		ioctl_binder(s_binderDesc, BINDER_THREAD_EXIT, &dummy, sizeof(dummy));
#if BINDER_DEBUG_MSGS
	// berr	<< "SLooper: exit new looper #" << Process()->LooperCount() << " " << this << /* " ==> Thread=" << find_thread(NULL) <<  ", keyId=" << m_keyID << */ endl;
#endif
}

status_t SLooper::_InitMainPlatform()
{
	return SpawnLooper();
}

status_t SLooper::SpawnLooper()
{
	SysHandle thid;
	status_t err = SysThreadCreate(NULL, "SLooper", B_NORMAL_PRIORITY, LOOPER_STACK_SIZE, (SysThreadEnterFunc*) _EnterLoop, This(), &thid);

	if (err == B_OK)
	{
		err = (SysThreadStart(thid) != B_OK) ? B_ERROR : B_OK;
	}
	// syslog(LOG_DEBUG, "SpawnLooper started thread %08x\n", thid);
	
	return err;
}

static bool g_singleProcess = false;
static bool g_knowSingleProcess = false;
bool SLooper::SupportsProcesses()
{
	return s_binderDesc >= 0;
	
	if (!g_knowSingleProcess) {
		char* singleproc = getenv("BINDER_SINGLE_PROCESS");
		if (singleproc && atoi(singleproc) != 0) {
			g_singleProcess = true;
		}
		g_knowSingleProcess = true;
	}
	
	return !g_singleProcess;
}

status_t SLooper::_EnterLoop(SLooper *parent)
{
	// parent exists vestigially from the BeOS run-in-user-space hack
	(void)parent;
	SLooper* looper = This(0);
	status_t status = B_ERROR;
	// This(0) can return NULL if called during shutdown
	if (looper) {
		looper->m_out.WriteInt32(bcREGISTER_LOOPER);
		status = looper->_LoopSelf();
	}
	return status;
}

status_t SLooper::Loop(bool isMain)
{
	SLooper* loop = This();
	loop->m_spawnedInternally = false;
	// berr << "SLooper: " << loop << " (" << loop->m_thid << ") entering! " << endl;
	loop->m_out.WriteInt32(bcENTER_LOOPER);
	status_t result;
	do {
		result = loop->_LoopSelf();
		//berr << "Looper " << loop << (isMain ? "(main)" : "(not main)") << " returned with: " << (-result) << endl;
	} while (isMain && !loop->m_signaled && result != -ECONNREFUSED);
	loop->m_out.WriteInt32(bcEXIT_LOOPER);
	loop->_TransactWithDriver(false);
	// berr << "SLooper: " << loop << " (" << loop->m_thid << ") exiting! " << endl;
	return B_OK;
}

bool SLooper::_SetNextEventTime(nsecs_t when, int32_t priority)
{
	SLooper* looper = This();	
	
	binder_wakeup_time wakeup;
	// FFB: specifying a relative time makes the kernel driver much simpler, for now.
	wakeup.time = when - SysGetRunTime();
	wakeup.priority = priority;
	if (s_binderDesc >= 0) {
		ioctl_binder(s_binderDesc, BINDER_SET_WAKEUP_TIME, &wakeup, sizeof(wakeup));
	
		// return false to indicate that the driver will take care
		// of scheduling the next event.
		return false;
	}
	
	// let the user-space scheduling code run.
	return true;
}

void SLooper::_SetThreadPriority(int32_t priority)
{
}

void SLooper::StopProcess(const sptr<IBinder>& rootObject, bool now)
{
	SLooper* looper = This();
	if (!looper) return;
	
	BpBinder* remote = rootObject->RemoteBinder();
	if (remote) {
		looper->m_out.WriteInt32(bcSTOP_PROCESS);
		looper->m_out.WriteInt32(remote->Handle());
		looper->m_out.WriteInt32(now ? 1 : 0);
		looper->_TransactWithDriver(false);
	} else if (rootObject == looper->Process()->AsBinder()) {
		looper->m_out.WriteInt32(bcSTOP_SELF);
		looper->m_out.WriteInt32(now ? 1 : 0);
		looper->_TransactWithDriver(false);
	}
}

void SLooper::_CatchRootObjects(catch_root_func func)
{
	g_catchRootFunc = func;
	berr << "CatchRootObjects() not implemented for Linux!" << endl;
}

sptr<IBinder> SLooper::ReceiveRootObject(pid_t process)
{
	BINDER_IPC_PROFILE_STATE;
	
	m_out.WriteInt32(bcRETRIEVE_ROOT_OBJECT);
	m_out.WriteInt32((int32_t)process);
	
	BEGIN_BINDER_CALL();

	SParcel data;
	_WaitForCompletion(&data);
	sptr<IBinder> object = data.ReadBinder();
	
	FINISH_BINDER_CALL();

	#if BINDER_DEBUG_MSGS
	bout << "Getting root object: object=" << object << endl;
	#endif
	
	return object;
}

status_t SLooper::SendRootObject(const sptr<IBinder>& rootNode)
{
	SParcel data;
	data.WriteBinder(rootNode);
	
	#if BINDER_DEBUG_MSGS
	bout << "Setting root object: " << data << endl;
	#endif
	
	return This()->_Reply(tfRootObject,data);
}

void SLooper::SetContextObject(	const sptr<IBinder>& object, const SString& name)
{
	context_entry entry;
	entry.context = SContext(interface_cast<INode>(object));
	entry.contextObject = object;
	
	g_binderContextAccess->Lock();
	g_binderContext->AddItem(name, entry);
	g_binderContextAccess->Unlock();
}

sptr<IBinder> SLooper::GetContextObject(const SString& name, const sptr<IBinder>& caller)
{
	g_binderContextAccess->Lock();
	sptr<IBinder> object(g_binderContext->ValueFor(name).contextObject);
	g_binderContextAccess->Unlock();
	
	if (object != NULL) return object;

	// Don't attempt to retrieve contexts if we manage them
	if (s_managesContexts) {
		berr << "GetContextObject(" << name << ") failed, but we manage the contexts!" << endl;
		return NULL;
	}
	
	object = _RetrieveContext(name, caller);
	if (object != NULL) SetContextObject(object, name);
	return object;
}

SContext SLooper::GetContext(const SString& name, const sptr<IBinder>& caller)
{
	g_binderContextAccess->Lock();
	SContext context(g_binderContext->ValueFor(name).context);
	g_binderContextAccess->Unlock();

	if (context.InitCheck() == B_OK) return context;
	
	GetContextObject(name, caller);
	
	g_binderContextAccess->Lock();
	context = g_binderContext->ValueFor(name).context;
	g_binderContextAccess->Unlock();
	
	return context;
}

sptr<IBinder> SLooper::_RetrieveContext(const SString& name, const sptr<IBinder>& caller)
{
	// berr << SPrintf("_RetrieveContext(%d)", which) << endl;
	/* We talk to the context manager by Transact()ing with descriptor 0. */
	sptr<IBinder> context;
	SParcel data, reply;
	data.WriteValue(SValue::String(name));
	data.WriteValue(SValue::Binder(caller));
	status_t result = This()->Transact(0 /*magic*/, 0, data, &reply, 0);
	if (result == B_OK) {
		context = reply.ReadValue().AsBinder();
	}
	// berr << SPrintf("_RetrieveContext(%d) returns: ", which ) << context << endl;
	return context;
}

/*
int32_t SLooper::_LoopTransactionSelf()
{
	return B_ERROR;
}
*/

void SLooper::_Signal()
{
	// Do we need the locks?
	m_signalLock.Lock();
	m_signaled++;
	int result = pthread_cond_broadcast(&m_condition);
	m_signalLock.Unlock();
}

int32_t SLooper::_LoopSelf()
{
	if (s_binderDesc >= 0) {
		// Loop style when working with the driver.
		// berr << "SLooper::_LoopSelf(): entering " << this << " (" << m_thid << ")@" << system_time() << endl;
		// The main thread may be calling us after doing initialization.
		// We want to unload any images now, rather than waiting for some
		// future binder event that may be a long time in coming.
		ExpungePackages();
	
		int32_t cmd,err = B_OK;
		while (err == B_OK) {
			//if ((err=_TransactWithDriver()) < B_OK) break;
			err = _TransactWithDriver();
			//bout << "Looper " << this << " transaction result: " << (-err) << ", errno=" << (errno) << endl;
			if (err < B_OK) break;
			if (m_signaled) break;
			err = m_in.Length();
			if (err < 0) break;
			if (err == 0) continue;
			cmd = m_in.ReadInt32();
			err = _HandleCommand(cmd);
			ExpungePackages();
		}
		//bout << "Looper " << this << " returning with: " << (-err) << endl;
		return err;
	}
	
	// Here we put ourself into the team's looper stack, and then
	// wait for the next event time to elapse by blocking on the
	// gehnaphore key with a timeout.  When we wake up, we need
	// to either:
	// (1) Call DispatchMessage() if we have reached g_nextEventTime
	//     AND this thread is at the top of the stack (if not at the
	//     top, need to make sure the top has the correct timeout);
	// (2) Reblock with the new timeout; or
	// (3) Exit if the total time spent waiting is greater then the
	//     maximum thread wait time (i.e., this thread is no longer
	//     needed.
	// In cases 1 and 2, we must first remove this looper from the
	// team's stack, and add it back when we return to wait for the
	// next event.

	SLooper* looper = This();
	m_team->PushLooper(looper);
	
	nsecs_t currentTime;
	bool atBottom = false;
	bool repeatReschedule = false;
	
	//berr << "SLooper::_LoopSelf(): entering " << looper << " (" << looper->m_thid << ")@" << SysGetRunTime() << endl;

	m_idleTime = 0;

	while (true)
	{
		ExpungePackages();
	
		nsecs_t timeout = 0;

		if (atBottom)
		{
			// If this is the bottom thread on the stack, then we DO NOT wake up
			// for events.  This thread is not allowed to handle messages because
			// it needs to be waiting for incoming transactions.
			timeout = Process()->GetIdleTimeout();
			
			atBottom = false;
			//berr << "Looper " << this << " at bottom so idle wait for " << timeout << endl;
		} 
		else
		{
			timeout = Process()->GetEventDelayTime(this);
		}


		// store the time we started to block at
		m_lastTimeISlept = SysGetRunTime();


		struct timespec abstime;
		clock_gettime(CLOCK_REALTIME, &abstime);
		
		// residue of nanoseconds: nanoseconds fragment from realtime, with
		// total timeout (in nanoseconds): we have to carry seconds into 
		// the realtime seconds
		nsecs_t part_abs_nano = abstime.tv_nsec + timeout;

		abstime.tv_sec += part_abs_nano / B_ONE_SECOND;
		abstime.tv_nsec = part_abs_nano % B_ONE_SECOND;
		if (abstime.tv_nsec < 0) 
		{ 
			// requires tv_nsec to be signed field
			// the residue of a negative timeout can result
			// in a negative nanoseconds field. Since it will
			// be abolutely less than one second, we can compensate
			// simply by borrowing back one second
			abstime.tv_sec -= 1;
			abstime.tv_nsec += B_ONE_SECOND;
		}
		
		// By the time we get here, a negative timeout has been converted
		// into an absolute timeout. It is in the past, but should still be
		// a legal timespec
	
		int result = 0;
		m_signalLock.Lock();
		if (m_signaled == 0)
		{
			m_signalLock.Unlock();
			pthread_mutex_lock(&m_mutex);
			result = pthread_cond_timedwait(&m_condition, &m_mutex, &abstime);
			pthread_mutex_unlock(&m_mutex);
			m_signalLock.Lock();
		}
		m_signalLock.Unlock();

		//berr << "Looper " << this << " woke up! " << result << endl;
		
		if (result != ETIMEDOUT)
		{
			if (result == B_OK)
			{
				// I didn't time out.  This only happens if someone raises my event,
				// which means they popped me off the stack to be rescheduled.  Push
				// back on and continue to pick up the new timeout.
		//		berr << "Looper " << this << " poked!  Push and continue. GetNext() = " << looper->GetNext() << " result = " << result << endl;
				Process()->PushLooper(looper);
				m_signalLock.Lock();
				m_signaled--;
				m_signalLock.Unlock();
			}
			else
			{
				berr << "[SLooper]: pthread_cond_timedwait returned: " << strerror(result) << endl;
			  	if (result == EINVAL)
				{
					berr << indent << SPrintf("abstime = (%d,%d)", abstime.tv_sec, abstime.tv_nsec) << endl;  
				}

				DbgOnlyFatalError("pthread_cond_timedwait returned an unusual status");
			}
			continue;
		}

		// i timed i out and some one is going to send me a message.
		// yipee! so we just want to received until they do!
		if (m_rescheduling)
		{
			if (!repeatReschedule)
			{
				repeatReschedule = true;
			}
			else
			{
				// Let others run.  Gag.
				SysThreadDelay(B_ONE_MILLISECOND, B_RELATIVE_TIMEOUT);
			}
			
			continue;
		}

		repeatReschedule = false;
		currentTime = SysGetRunTime();
		if (Process()->PopLooper(looper, currentTime)) 
		{
			DbgOnlyFatalErrorIf(looper->GetNext() != NULL, "Poped a looper with non-NULL next!");
			
			// go and handle the message.
			//berr << "Looper " << this << " popped and dispatching..." << endl;
			Process()->DispatchMessage(looper);
			m_idleTime = 0;
			Process()->PushLooper(looper);
		}
		else
		{
			if (m_next == NULL) atBottom = true;

			// increment the idle time so we can eventually commit seppuku!
			m_idleTime += SysGetRunTime() - m_lastTimeISlept;
		
			//berr << "Looper " << this << " has new idle time " << m_idleTime << endl;
			
			if (currentTime < Process()->GetNextEventTime() && m_idleTime >= Process()->GetIdleTimeout() && m_spawnedInternally)
			{
				// we have out lasted our usefullness. WE MUST DIE!
				if (Process()->RemoveLooper(looper))
					break;
			}
		}
	}
	
	return B_OK;
}

status_t SLooper::IncrefsHandle(int32_t handle)
{
#if BINDER_REFCOUNT_MSGS
	berr << "Writing increfs for handle " << handle << endl;
#endif
	m_out.WriteInt32(bcINCREFS);
	m_out.WriteInt32(handle);
	return B_OK;
}

status_t 
SLooper::DecrefsHandle(int32_t handle)
{
#if BINDER_REFCOUNT_MSGS
	berr << "Writing decrefs for handle " << handle << endl;
#endif
	m_out.WriteInt32(bcDECREFS);
	m_out.WriteInt32(handle);
	return B_OK;
}

status_t SLooper::AcquireHandle(int32_t handle)
{
#if BINDER_REFCOUNT_MSGS
	berr << "Writing acquire for handle " << handle << endl;
#endif
	m_out.WriteInt32(bcACQUIRE);
	m_out.WriteInt32(handle);
	return B_OK;
}

/* Called when the last strong BpBinder disappears */
status_t SLooper::ReleaseHandle(int32_t handle)
{
#if BINDER_REFCOUNT_MSGS
	berr << "Writing release for handle " << handle << endl;
#endif
	m_out.WriteInt32(bcRELEASE);
	m_out.WriteInt32(handle);
	return B_OK;
}

status_t SLooper::AttemptAcquireHandle(int32_t handle)
{
	BINDER_IPC_PROFILE_STATE;
	
#if BINDER_REFCOUNT_MSGS
	berr << "Writing attempt acquire for handle " << handle << endl;
#endif
	m_out.WriteInt32(bcATTEMPT_ACQUIRE);
	m_out.WriteInt32(m_priority);
	m_out.WriteInt32(handle);
	status_t result = B_ERROR;
	
	BEGIN_BINDER_CALL();
	_WaitForCompletion(NULL, &result);
	FINISH_BINDER_CALL();
	
	return result;
}

status_t 
SLooper::_HandleCommand(int32_t cmd)
{
	BBinder *ptr;
	SAtom *atom;
	status_t result = B_OK;
	
#if BINDER_DEBUG_MSGS
	if (cmd != brNOOP)
		berr	<< "Thread " << this << SPrintf(" in %d ", getpid())
				<< " got command: " << cmd << " " << inString[cmd] << endl;
#endif
	switch (cmd) {
		case brERROR: {
			result = m_in.ReadInt32();
			berr << "*** Binder driver error during read: " << strerror(result) << endl;
		} break;
		case brOK: {
		} break;
		case brACQUIRE: {
			ptr = (BBinder*)m_in.ReadInt32();
			atom = (SAtom*)m_in.ReadInt32();
			//fprintf(stderr, "brACQUIRE: ptr=%p, atom=%p\n", ptr, atom);
			//if (ptr != atom) {
			//	fprintf(stderr, "Failing with bad cookie: got %p, expected %p, object %p\n", atom, static_cast<SAtom*>(ptr), ptr);
			//}
			DbgOnlyFatalErrorIf(ptr != atom, "SLooper: Driver brACQUIRE returned Binder address with incorrect SAtom cookie!");
			#if BINDER_REFCOUNT_MSGS
			berr << "Calling IncStrong() on " << ptr << " " << typeid(*ptr).name()
				<< " (atom " << atom << ")" << endl;
			#endif
			#if BINDER_REFCOUNT_RESULT_MSGS
			int32_t r =
			#endif
			atom->IncStrong(reinterpret_cast<void*>(m_teamID));
			#if BINDER_REFCOUNT_RESULT_MSGS
			berr << "IncStrong() result on " << ptr << ": " << r << endl;
			#endif
			m_out.WriteInt32(bcACQUIRE_DONE);
			m_out.WriteInt32((int32_t)ptr);
			m_out.WriteInt32((int32_t)atom);
		} break;
		case brRELEASE: {
			ptr = (BBinder*)m_in.ReadInt32();
			atom = (SAtom*)m_in.ReadInt32();
			//fprintf(stderr, "brRELEASE: ptr=%p, atom=%p\n", ptr, atom);
			//if (ptr != atom) {
			//	fprintf(stderr, "Failing with bad cookie: got %p, expected %p, object %p\n", atom, static_cast<SAtom*>(ptr), ptr);
			//}
			DbgOnlyFatalErrorIf(ptr != atom, "SLooper: Driver brRELEASE returned Binder address with incorrect SAtom cookie!");
			#if BINDER_REFCOUNT_MSGS
			berr << "Calling DecStrong() on " << ptr << " " << typeid(*ptr).name()
				<< " (atom " << atom << ")" << endl;
			#endif
			#if BINDER_REFCOUNT_RESULT_MSGS
			berr << "References on " << ptr << " before DecStrong(): " << atom->StrongCount() << endl;
			atom->Report(berr);
			#endif
			atom->DecStrong(reinterpret_cast<void*>(m_teamID));
		} break;
		case brATTEMPT_ACQUIRE: {
			m_priority = m_in.ReadInt32();
			ptr = (BBinder*)m_in.ReadInt32();
			atom = (SAtom*)m_in.ReadInt32();
			#if BINDER_REFCOUNT_MSGS
			berr << "Calling AttemptIncStrong() on " << ptr << " " << typeid(*ptr).name()
				<< " (atom " << atom << ")" << endl;
			#endif
			const bool success = atom->AttemptIncStrong(reinterpret_cast<void*>(m_teamID));
			//fprintf(stderr, "brATTEMPT_ACQUIRE: ptr=%p, atom=%p\n", ptr, atom);
			//if (success && ptr != atom) {
			//	fprintf(stderr, "Failing with bad cookie: got %p, expected %p, object %p\n", atom, static_cast<SAtom*>(ptr), ptr);
			//}
			DbgOnlyFatalErrorIf(success && ptr != atom, "SLooper: Driver brATTEMPT_ACQUIRE returned Binder address with incorrect SAtom cookie!");
			m_out.WriteInt32(bcACQUIRE_RESULT);
			m_out.WriteInt32((int32_t)success);
			#if BINDER_REFCOUNT_RESULT_MSGS
			berr << "AttemptIncStrong() result on " << ptr << ": " << success << endl;
			#endif
		} break;
		case brINCREFS: {
			ptr = (BBinder*)m_in.ReadInt32();
			atom = (SAtom*)m_in.ReadInt32();
			#if BINDER_REFCOUNT_MSGS
			berr << "Calling IncWeak() on " << ptr << " " << typeid(*ptr).name()
				<< " (atom " << atom << ")" << endl;
			#endif
			#if BINDER_REFCOUNT_RESULT_MSGS
			int32_t r =
			#endif
			atom->IncWeak(reinterpret_cast<void*>(m_teamID));
			#if BINDER_REFCOUNT_RESULT_MSGS
			berr << "IncWeak() result on " << ptr << ": " << r << endl;
			#endif
			m_out.WriteInt32(bcINCREFS_DONE);
			m_out.WriteInt32((int32_t)ptr);
			m_out.WriteInt32((int32_t)atom);
		} break;
		case brDECREFS: {
			ptr = (BBinder*)m_in.ReadInt32();
			atom = (SAtom*)m_in.ReadInt32();
			#if BINDER_REFCOUNT_MSGS
			berr << "Calling DecWeak() on " << ptr << " " << typeid(*ptr).name()
				<< " (atom " << atom << ")" << endl;
			#endif
			#if BINDER_REFCOUNT_RESULT_MSGS
			berr << "References on " << ptr << " before DecWeak(): " << atom->WeakCount() << endl;
			atom->Report(berr);
			#endif
			atom->DecWeak(reinterpret_cast<void*>(m_teamID));
		} break;
		case brTRANSACTION: {
			binder_transaction_data tr;
			ssize_t amt = m_in.Read(&tr, sizeof(tr));
			if (amt < (ssize_t)sizeof(tr)) {
				ErrFatalError("Bad read!");
				return (m_lastError = (amt < B_OK ? amt : B_BAD_VALUE));
			}
			
			SParcel buffer(tr.data.ptr.buffer, tr.data_size, _BufferFree, this);
			// syslog(LOG_DEBUG, "brTRANSACTION data: %d, offsets: %d\n", tr.data_size, tr.offsets_size);
			buffer.SetBinderOffsets(tr.data.ptr.offsets, tr.offsets_size, false);
			m_priority = tr.priority;
			
#if BINDER_TRANSACTION_MSGS
			bout << "Receiving transaction " << STypeCode(tr.code) << " to " << tr.target.ptr
				<< ": " << buffer << endl;
#endif

			SLooper::catch_root_func crf;
			if ((tr.flags&tfRootObject) && (crf=g_catchRootFunc) != NULL) {
				SValue value;
				buffer.GetValues(1, &value);
				sptr<IBinder> root(value.AsBinder());
				if (root != NULL) crf(root);
				else ErrFatalError("Captured transaction does not contain binder!");
			
			} else if (tr.target.ptr) {
#if BINDER_DEBUG_MSGS
				berr << "Received transaction buffer: " << buffer.Data() << endl;
#endif

#if BINDER_BUFFER_MSGS
				printf("Creating transaction buffer for %p, now have %d open.\n",
						tr.data.ptr.buffer, atomic_add(&g_openBuffers, 1) + 1);
#endif

				sptr<BBinder> b((BBinder*)tr.target.ptr);
				SParcel reply(_BufferReply, this);
				const status_t error = b->Transact(tr.code, buffer, &reply, 0);
				if (error < B_OK) reply.Reference(NULL, error);
#if BINDER_TRANSACTION_MSGS
				bout << "Replying with: " << reply << endl;
#endif
				reply.Reply();
				
			} else {
				// Transactions against the NULL binder always go to the context manager.
				SParcel reply(_BufferReply, this);
				if (s_managesContexts) {
					SString name(buffer.ReadValue().AsString());
					sptr<IBinder> caller(buffer.ReadValue().AsBinder());
					if (g_binderContextCheckFunc(name, caller, g_binderContextUserData)) {
						g_binderContextAccess->Lock();
						reply.WriteValue(SValue::Binder(g_binderContext->ValueFor(name).contextObject));
						g_binderContextAccess->Unlock();
					} else {
						reply.WriteValue(SValue::Status(B_PERMISSION_DENIED));
					}
				} else {
					berr << "Aieee! We got B_*_CONTEXT, but we don't manage contexts!" << endl;
				}
#if BINDER_BUFFER_MSGS
				berr << "Handled B_*_CONTEXT with " << reply << endl;
#endif
#if BINDER_TRANSACTION_MSGS
				bout << "Replying with: " << reply << endl;
#endif
				reply.Reply();
			}
			
			buffer.Free();
			
		} break;
		case brEVENT_OCCURRED: {
			m_priority = m_in.ReadInt32();
			if (m_team != NULL) {
				m_team->DispatchMessage(this);
			}
		} break;
		case brFINISHED: {
			result = -ETIMEDOUT;
		} break;
		case brNOOP: {
		} break;
		case brSPAWN_LOOPER: {
			SpawnLooper();
		} break;
		default: {
			berr << "********* Bad command: " << (void*) cmd << ", read buffer is: " << m_in << endl;
			result = B_ERROR;
			//DbgOnlyFatalError("Boom");
		} break;
	}
	
	return (m_lastError = result);
}

status_t
SLooper::_WaitForCompletion(SParcel *reply, status_t *acquireResult)
{
	int32_t cmd;
	int32_t err = B_OK;
	bool setReply = false;

	// Remember current thread priority, so any transactions executed
	// during this time don't disrupt it.
	const int32_t curPriority = m_priority;
	
	while (1) {
		if ((err=_TransactWithDriver()) < B_OK) break;
		err = m_in.Length();
		if (err < 0) break;
		if (err == 0) continue;
		err = 0;
		cmd = m_in.ReadInt32();
		
#if BINDER_DEBUG_MSGS
		if (cmd != brNOOP)
			berr << "Thread " << this << SPrintf(" in %d ", getpid()) << " _WaitForCompletion got : " << cmd << " " << inString[cmd] << endl;
#endif
		if (cmd == brTRANSACTION_COMPLETE) {
			if (!reply && !acquireResult) break;
		} else if (cmd == brDEAD_REPLY) {
			// The target is gone!
			err = B_BINDER_DEAD;
			if (acquireResult) *acquireResult = B_BINDER_DEAD;
			else if (reply) reply->Reference(NULL, B_BINDER_DEAD);
			break;
		} else if (cmd == brACQUIRE_RESULT) {
			int32_t result = m_in.ReadInt32();
#if BINDER_BUFFER_MSGS
			berr << "Result of bcATTEMPT_ACQUIRE: " << result << endl;
#endif
			if (acquireResult) {
				*acquireResult = result ? B_OK : B_NOT_ALLOWED;
				break;
			}
			ErrFatalError("Unexpected brACQUIRE_RESULT!");
		} else if (cmd == brREPLY) {
			binder_transaction_data tr;
			ssize_t amt = m_in.Read(&tr, sizeof(tr));
			if (amt < (ssize_t)sizeof(tr)) {
				ErrFatalError("Bad read!");
				if (amt >= B_OK) amt = B_BAD_VALUE;
				if (reply) reply->Reference(NULL, amt);
				return (m_lastError = amt);
			}
			if (reply) {
				if ((tr.flags&tfStatusCode) == 0) {
					reply->Reference(	tr.data.ptr.buffer, tr.data_size,
										_BufferFree, this);
					reply->SetBinderOffsets(tr.data.ptr.offsets, tr.offsets_size, false);
				} else {
					reply->Reference(NULL, *static_cast<const ssize_t*>(tr.data.ptr.buffer));
					_BufferFree(tr.data.ptr.buffer, tr.data_size, this);
				}
				setReply = true;
#if BINDER_BUFFER_MSGS
				berr	<< "Creating reply buffer for " << tr.data.ptr.buffer
						<< ", now have " << (atomic_add(&g_openBuffers, 1)+1)
						<< " open." << endl;
				berr << "brREPLY: " << *reply << endl;
#endif
			} else {
#if BINDER_BUFFER_MSGS
				berr << "Immediately freeing buffer for " << tr.data.ptr.buffer << endl;
#endif
				ErrFatalErrorIf(tr.data.ptr.buffer == NULL, "Sending NULL bcFREE_BUFFER!");
				m_out.WriteInt32(bcFREE_BUFFER);
				m_out.WriteInt32((int32_t)tr.data.ptr.buffer);
			}
			break;
		} else if ((err = _HandleCommand(cmd))) break;
	}
	
	// Restore last thread priority.
	_SetThreadPriority(curPriority);
	
	// Propagate binder errors into reply status.
	if (err < B_OK && !setReply && reply) {
		reply->Reference(NULL, err);
	}
	
	return (m_lastError = err);
}

status_t 
SLooper::_BufferReply(const SParcel& buffer, void* context)
{
	return reinterpret_cast<SLooper*>(context)->_Reply(0, buffer);
}

void
SLooper::_BufferFree(const void* data, ssize_t /*len*/, void* context)
{
	if (data) {
#if BINDER_DEBUG_MSGS
		berr << "Free transaction buffer: " << data << endl;
#endif
#if BINDER_BUFFER_MSGS
		berr	<< "Freeing binder buffer for " << data << ", now have "
				<< (atomic_add(&g_openBuffers, -1) - 1) << " open." << endl;
#endif
		ErrFatalErrorIf(data == NULL, "Sending NULL bcFREE_BUFFER!");
		reinterpret_cast<SLooper*>(context)->m_out.WriteInt32(bcFREE_BUFFER);
		reinterpret_cast<SLooper*>(context)->m_out.WriteInt32((int32_t)data);
	} else {
		ErrFatalError("NULL _BufferFree()!");
	}
}

status_t 
SLooper::_Reply(uint32_t flags, const SParcel& data)
{
	status_t err;
	status_t statusBuffer;
	err = _WriteTransaction(bcREPLY, flags, -1, 0, data, &statusBuffer);
	if (err < B_OK) return err;
	
	return _WaitForCompletion();
}

status_t
SLooper::_TransactWithDriver(bool doRead)
{
	if (s_binderDesc < 0)
		return B_UNSUPPORTED;	// XXX Should have a better error code.
	
	// The goal here is to perform as few binder transactions as possible.
	// - If doRead is false, then the caller only wants to flush the current
	//   write buffer.  We will never try to read anything, and only write
	//   if there is actually data available.
	// - If doRead is true, then the caller is going to be reading data
	//   immediately afterward.  In this case, we want to attempt to perform
	//   writes and reads together as much as possible.  To do this, we only
	//   read when the read buffer is empty, and only write at that time.
	
	binder_write_read bwr;
	
	// Is the read buffer empty?
	const bool needRead = m_in.Position() >= m_in.Length();
	
	// How much is available for writing?  If a read is requested, but
	// we aren't going to read now, then we say 0.
	const ssize_t outAvail = (!doRead || needRead) ? m_out.Length() : 0;
	
	// This is what we'll write.
	bwr.write_size = outAvail;
	bwr.write_buffer = (long unsigned int)m_out.Data();
#if BINDER_BUFFER_MSGS
	berr << "Thread " << getpid() << " Writing: " << indent << SHexDump((void*)bwr.write_buffer, bwr.write_size) << dedent << endl;
#endif

	// This is what we'll read.
	if (doRead && needRead) {
		bwr.read_size = m_in.Avail();
		bwr.read_buffer = (long unsigned int)m_in.ReAlloc(bwr.read_size);
	} else {
		bwr.read_size = 0;
	}
	
	// Anything to do this time?
	if ((bwr.write_size == 0) && (bwr.read_size == 0)) {
#if BINDER_BUFFER_MSGS
		printf("doRead: %s, needRead: %s\n", doRead ? "true" : "false", needRead ? "true" : "false"); 
		printf("m_in.Length(): %d, .Position(): %d\n", m_in.Length(), m_in.Position());
#endif
		return B_OK;
	}
	
	// Do it!
	bwr.write_consumed = 0;
	bwr.read_consumed = 0;
	ssize_t err = -EINTR;
	while (!m_signaled && err == -EINTR) {
		if (ioctl_binder(s_binderDesc,BINDER_WRITE_READ,&bwr,sizeof(bwr)) >= 0)
			err = B_OK;
		else
			err = -errno;
		//if (err == -EINTR)
		//	berr << SPrintf("%s:ioctl_binder returned EINTR\n\t\t%u written\n\t\t%u read\n", __func__, bwr.write_consumed, bwr.read_consumed);
	}
	if (err >= 0) {
		//berr << "Wrote " << bwr.write_size << ", expected " << outAvail << endl;
		ErrFatalErrorIf(bwr.write_size != outAvail, "Didn't write it all!");
		// Update the write buffer.
		if (bwr.write_consumed > 0) {
			// FFB: why do we bother with this?  We loose anything we didn't
			// write, and then we never try to send the remaining data.
			m_out.SetLength(outAvail-bwr.write_consumed);
		}
		// Update the read buffer.
		m_in.SetLength(bwr.read_consumed);
		m_in.SetPosition(0);
#if BINDER_BUFFER_MSGS
		if (bwr.read_consumed > 0) {
			berr << "Thread " << getpid() << " Read: " << indent << SHexDump((void*)bwr.read_buffer, bwr.read_consumed) << dedent << endl;
		} else {
			berr << SPrintf("Thread %d Read: %d", getpid(), bwr.read_consumed) << endl;
		}
#endif
		return B_OK;
	}
#if BINDER_BUFFER_MSGS
	else {
		berr << "_TransactWithDriver(" << SString(doRead ? "true" : "false") << ") failed with: " << SString(strerror(errno)) << endl;
	}
#endif
	return err;
}

status_t
SLooper::_WriteTransaction(int32_t cmd, uint32_t binderFlags, int32_t handle, uint32_t code,
	const SParcel& data, status_t* statusBuffer)
{
	binder_transaction_data tr;

	tr.target.handle = handle;
	tr.code = code;
	tr.flags = binderFlags&~tfInline;
	tr.priority = m_priority;
	if (static_cast<ssize_t>((tr.data_size=data.Length())) >= 0) {
		// Send this parcel's data through the binder.
		tr.data.ptr.buffer = data.Data();
		tr.offsets_size = data.BinderOffsetsLength();
		tr.data.ptr.offsets = data.BinderOffsetsData();
#if BINDER_BUFFER_MSGS
		berr << SPrintf("_WriteTransaction() %d sending buffer data", getpid()) << endl;
		berr << SPrintf("  -- data_size: %d, offsets_size: %d", tr.data_size, tr.offsets_size) << endl;
		berr << SPrintf("  -- data_ptr: %p, offsets_ptr: %p", tr.data.ptr.buffer, tr.data.ptr.offsets) << endl;
#endif
	} else if (statusBuffer) {
		// Send this parcel's status through the binder.
		tr.flags |= tfStatusCode;
		*statusBuffer = data.Length();
		tr.data_size = sizeof(status_t);
		tr.data.ptr.buffer = statusBuffer;
		tr.offsets_size = 0;
		tr.data.ptr.offsets = NULL;
#if BINDER_BUFFER_MSGS
		berr << SPrintf("_WriteTransaction() %d sending buffer status", getpid()) << endl;
#endif
	} else {
		// Just return this parcel's status.
#if BINDER_BUFFER_MSGS
		berr << SPrintf("_WriteTransaction() %d returning status", getpid()) << endl;
#endif
		return (m_lastError = data.ErrorCheck());
	}
	
	//bout << "Sending transaction: " << data << endl;
	
	m_out.WriteInt32(cmd);
	m_out.Write(&tr, sizeof(tr));
	
	return (m_lastError = B_OK);
}

status_t
SLooper::Transact(int32_t handle, uint32_t code, const SParcel& data,
	SParcel* reply, uint32_t /*flags*/)
{
	if (data.ErrorCheck() != B_OK) {
		// Reflect errors back to caller.
		if (reply) reply->Reference(NULL, data.Length());
		return (m_lastError = data.ErrorCheck());
	}
	
	BINDER_IPC_PROFILE_STATE;
	
#if BINDER_TRANSACTION_MSGS
	bout << "Sending transaction " << STypeCode(code) << " to " << (void*)handle
		<< ": " << data << endl;
#endif

	status_t err = _WriteTransaction(bcTRANSACTION, 0, handle, code, data);
	if (err < B_OK) {
		if (reply) reply->Reference(NULL, err);
		return err;
	}
	
	BEGIN_BINDER_CALL();
	if (reply) {
		err = _WaitForCompletion(reply);
		if (err == errNone) err = reply->ErrorCheck();
	} else {
		SParcel fakeReply;
		err = _WaitForCompletion(&fakeReply);
		if (err == errNone) err = fakeReply.ErrorCheck();
	}
	FINISH_BINDER_CALL();
	
#if BINDER_TRANSACTION_MSGS
	if (reply)
		bout << "Received reply: " << *reply << endl;
#endif

	return err;
}

SLooper* SLooper::GetNext()
{
	return m_next;
}

void SLooper::SetNext(SLooper* next)
{
	m_next = next;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

