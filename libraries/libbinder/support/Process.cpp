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
#include <support/Process.h>

#include <support/atomic.h>
#include <support/Autolock.h>
#include <support/Debug.h>
#include <support/Handler.h>
#include <support/InstantiateComponent.h>
#include <support/Looper.h>
#include <support/SortedVector.h>
#include <support/StdIO.h>
#include <support/String.h>
#include <support/SupportDefs.h>

#include <support_p/RBinder.h>
#include <support_p/WindowsCompatibility.h>
#include <support_p/SupportMisc.h>
#define INTRUSIVE_PROFILING 0
#include <support_p/IntrusiveProfiler.h>

#if !LIBBE_BOOTSTRAP
#include <support/IVirtualMachine.h>
#endif

#if TARGET_HOST == TARGET_HOST_WIN32
#include <support_p/WindowsCompatibility.h>
#elif TARGET_HOST == TARGET_HOST_LINUX
#	include <dlfcn.h>
#endif

#include <assert.h>
#include <new>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "LooperPrv.h"

// #undef DEBUG

#define HANDLE_DEBUG_MSGS			0
#define IMAGE_DEBUG_MSGS			0
#define COUNT_PROXIES				0

//#define TRACK_CLASS_NAME			"class BGremlinSpy"
#define TEST_TRACKED_CLASS(handle)														\
	bool tracked;																		\
	{																					\
		const remote_host_info &rhi = g_remoteHostInfo.ItemAt(handle&(~WEAK_REF));		\
		tracked = (&rhi && strcmp(rhi.name,TRACK_CLASS_NAME) == 0);						\
	}

#define INFO(_off)
//#define INFO(_on) _on

#define CHECK_INTEGRITY 0

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#if _SUPPORTS_NAMESPACE
using namespace palmos::osp;
#endif

/**************************************************************************************/

struct BProcess::ImageData
{
	// All images that are loaded.
	class ImageEntry : public SLightAtom
	{
	public:
		SConditionVariable	loading;
		wptr<ComponentImage>	image;
	};
	SKeyedVector<SValue, sptr<ImageEntry> >	images;
	
#if !LIBBE_BOOTSTRAP
	// All virtual machines that are running.
	SKeyedVector<SString, sptr<IVirtualMachine> >	vms;
#endif
};

/**************************************************************************************/
static uint32_t s_processID = ~0U;

BProcess::BProcess(team_id tid)
	: m_id(tid)
	, m_lock("BProcess access")
	, m_pendingHandlers(NULL)
	, m_nextEventTime(B_INFINITE_TIMEOUT)
	, m_maxEventConcurrency(4)
	, m_currentEventConcurrency(0)
	, m_imageLock("BProcess image access")
	, m_imageData(new ImageData)
	, m_handleRefLock("BProcess handle ref access")
	, m_remoteRefsReleased(false)

	// These are used when running without the driver.	
	, m_shutdown(false)
	, m_idleCount(0)
	, m_idleLooper(NULL)
	, m_idleTimeout(B_SECONDS(5))
	, m_loopers(0)
	, m_maxLoopers(7)
	, m_minLoopers(2)
{
	
	INFO(bout << "************* Creating BProcess " << this << " (id " << m_id << ") *************" << endl;)

	m_lock.LockQuick();
	IncrementLoopers();
	m_lock.Unlock();
}

void
BProcess::PrintBinderReferences(void)
{
}

void
BProcess::RestartMallocProfiling(void)
{
}

void
BProcess::SetMallocProfiling(bool enabled, int32_t dumpPeriod, int32_t maxItems, int32_t stackDepth)
{
}

void
BProcess::RestartVectorProfiling(void)
{
}

void
BProcess::SetVectorProfiling(bool enabled, int32_t dumpPeriod, int32_t maxItems, int32_t stackDepth)
{
	(void)enabled; (void)dumpPeriod; (void)maxItems; (void)stackDepth;
}

void
BProcess::RestartMessageIPCProfiling(void)
{
}

void
BProcess::SetMessageIPCProfiling(bool enabled, int32_t dumpPeriod, int32_t maxItems, int32_t stackDepth)
{
	(void)enabled; (void)dumpPeriod; (void)maxItems; (void)stackDepth;
}

void
BProcess::RestartBinderIPCProfiling(void)
{
}

void
BProcess::PrintBinderIPCProfiling(void)
{
}

void
BProcess::SetBinderIPCProfiling(bool enabled, int32_t dumpPeriod, int32_t maxItems, int32_t stackDepth)
{
	(void)enabled; (void)dumpPeriod; (void)maxItems; (void)stackDepth;
}

void
BProcess::CatchHandleRelease(const sptr<IBinder>& remoteObject, catchReleaseFunc callbackFunc)
{
	SNestedLocker::Autolock _l(m_handleRefLock);
	m_catchers.AddItem(remoteObject.ptr(), callbackFunc);
}

BProcess::~BProcess()
{
	INFO(bout << "************* Destroying BProcess " << this << " (id " << m_id << ") *************" << endl);
	ReleaseRemoteReferences();
	delete m_imageData;
	m_imageData = NULL;
}

void BProcess::ReleaseRemoteReferences()
{
	if (m_remoteRefsReleased) return;
	m_remoteRefsReleased = true;

	INFO(bout << "************* ReleaseRemoteReferfences BProcess " << this << " (id " << m_id << ") *************" << endl);

	size_t i;
	m_imageData->vms.MakeEmpty();


#if 0
	bout << "Remaining images:" << endl;
	i = m_imageData->images.CountItems();
	while (i > 0) {
		i--;
		bout << m_imageData->images.KeyAt(i) << ":" << indent << endl;
		m_imageData->images.ValueAt(i)->image.unsafe_ptr_access()->Report(bout);
		bout << dedent;
	}
#endif
}

int32_t BProcess::ID() const
{
	return m_id;
}

void BProcess::InitAtom()
{
}

/* ------------------------ Messaging ------------------------ */

bool BProcess::InsertHandler(BHandler **handlerP, BHandler *handler)
{
	BHandler *p = *handlerP;

	if (!p) {
		*handlerP = handler;
		handler->m_next = handler->m_prev = NULL;
		return true;
	}

	const nsecs_t msgTime = handler->NextMessageTime(NULL);
	if (p->NextMessageTime(NULL) > msgTime) {
		(*handlerP)->m_prev = handler;
		*handlerP = handler;
		handler->m_next = p;
		handler->m_prev = NULL;
	} else {
		while (p->m_next && (p->m_next->NextMessageTime(NULL) < msgTime)) p = p->m_next;
		handler->m_next = p->m_next;
		handler->m_prev = p;
		if (p->m_next) p->m_next->m_prev = handler;
		p->m_next = handler;
	}
	return false;
}

nsecs_t BProcess::GetNextEventTime() const
{
	m_lock.LockQuick();
	nsecs_t time = m_nextEventTime;
	m_lock.Unlock();
	return time;
}

void BProcess::SetHandlerConcurrency(int32_t maxConcurrency)
{
	bool doReschedule = false;

	m_lock.LockQuick();
	if (m_maxEventConcurrency != maxConcurrency) {
		doReschedule = (m_currentEventConcurrency == m_maxEventConcurrency && maxConcurrency > m_maxEventConcurrency);
		m_maxEventConcurrency = maxConcurrency;
	}

	if (doReschedule) ScheduleNextHandler();
	else m_lock.Unlock();
}

static bool FindLooper(SLooper* top, SLooper* want)
{
	while (top) {
		if (top == want) return true;
		top = top->GetNext();
	}
	return false;
}

inline void BProcess::CheckIntegrity() const
{
#if CHECK_INTEGRITY
	int32_t idle = 0;
	SLooper* next = m_idleLooper;
	
	while (next != NULL)
	{
		idle++;
		next = next->GetNext();
	}

	if (idle != m_idleCount)
	{
		printf("%d loopers on the stack with an idle count of %d\n", idle, m_idleCount);
	}
	
	ErrFatalErrorIf(idle != m_idleCount, "Looper stack and m_idleCount are inconsistent!");

	next = m_idleLooper;
	while (next) {
		ErrFatalErrorIfFindLooper(next->GetNext(), next), "Looper stack contains duplicates!");
		next = next->GetNext();
	}
#endif // CHECK_INTEGRITY
}

int32_t BProcess::MaximumLoopers() const
{
	return m_maxLoopers;
}

int32_t BProcess::MinimumLoopers() const
{
	return m_minLoopers;
}

nsecs_t BProcess::GetIdleTimeout() const
{
	return m_idleTimeout;
}

nsecs_t BProcess::GetEventDelayTime(SLooper* caller) const
{
	if (m_shutdown) return 0;

	nsecs_t timeout;
	m_lock.LockQuick();
	if (caller == m_idleLooper && m_nextEventTime != B_INFINITE_TIMEOUT) {
		// The top looper waits only until the next event
		// should be processed.
		timeout = m_nextEventTime - exact_SysGetRunTime();
		if (timeout < 0)
			timeout = 0;
	} else {
		// Looper is not on the top, or no pending events so wait forever.
		timeout = m_idleTimeout;
	}
	m_lock.Unlock();
	return timeout;
}

bool BProcess::PopLooper(SLooper* looper, nsecs_t when)
{
	bool poped = false;
	bool spawnLooper = false;
	
	// Only remove a 'looper' if the looper is at the top of the stack and
	// if the 'when' is greater than the next event time and if there are
	// more than one looper to handle incoming messages.

	m_lock.LockQuick();

	//printf("PopLooper %p: top=%p, when=%Ld, nextTime=%Ld, count=%d\n",
	//		looper, m_idleLooper, when, m_nextEventTime, m_idleCount);
	CheckIntegrity();
	
	if (m_idleLooper && (looper == m_idleLooper) && (when >= m_nextEventTime) && (m_idleCount > 1))
	{
		SLooper* pop = m_idleLooper;
		m_idleLooper = m_idleLooper->GetNext();		
		pop->SetNext(NULL);
		m_idleCount--;
		poped = true;
	}

	nsecs_t nextTime = m_pendingHandlers ? m_pendingHandlers->NextMessageTime(NULL) : B_INFINITE_TIMEOUT;
	
	if ((m_idleCount == 1 || m_loopers < m_minLoopers) && (m_loopers < m_maxLoopers) && (approx_SysGetRunTime() >= nextTime))
	{
		status_t status = SLooper::SpawnLooper();
		
		if (status == B_OK)
		{
			IncrementLoopers();
			//bout << "Just called SLooper::SpawnLooper! status = " << strerror(status) << endl;
		}
	}

	CheckIntegrity();

	m_lock.Unlock();
	
	// We have to have at least one looper around to accept transactions
	// from remote teams. Otherwise system go boom boom! Do not lock
	// while making a call to SpawnLooper!!!
	return poped;
}

void BProcess::PushLooper(SLooper* looper)
{
	m_lock.LockQuick();
	
	//printf("Pushing looper %p, top is %p\n", looper, m_idleLooper);

#if CHECK_INTEGRITY
	ErrFatalErrorIf(looper->GetNext()!= NULL, "Pushing a looper with non-NULL next!");

	// Check that looper isn't already on stack.
	SLooper* pos = m_idleLooper;
	while (pos) {
		ErrFatalErrorIf(pos == looper, "Looper pushed on idle stack twice!");
		pos = pos->GetNext();
	}
#endif

	if (m_idleLooper)
	{
		looper->SetNext(m_idleLooper);
		m_idleLooper = looper;
	}
	else
	{
		ErrFatalErrorIf(looper->GetNext() != NULL, "Pushing looper with non-NULL next!");
		m_idleLooper = looper;
	}

	m_idleCount++;

	CheckIntegrity();

	m_lock.Unlock();
}

SLooper* BProcess::UnsafePopLooper()
{
	SLooper* pop = m_idleLooper;

	if (pop != NULL)
	{	
		m_idleLooper = m_idleLooper->GetNext();		
		pop->SetNext(NULL);
		m_idleCount--;
	}

	CheckIntegrity();

	//printf("Unsafe popping looper %p\n", pop);

	return pop;
}

bool BProcess::RemoveLooper(SLooper* looper)
{
	bool removed = false;
	
	m_lock.LockQuick();
	
	if (m_shutdown || (m_loopers > m_minLoopers && m_idleLooper))
	{
		// This looper is allowed to exit.
		removed = true;
		DecrementLoopers();

		SLooper* prev = NULL;
		SLooper* pos = m_idleLooper;
	
		while (pos != NULL && pos != looper)
		{
			prev = pos;
			pos = pos->GetNext();
		}
	
		// remove references to the looper if it is on the idle list.
		if (pos)
		{
			if (prev) prev->SetNext(pos->GetNext());
			else m_idleLooper = pos->GetNext();
			pos->SetNext(NULL);
			m_idleCount--;
		}
	}

	CheckIntegrity();
	
	m_lock.Unlock();

	return removed;
}

int32_t BProcess::LooperCount() const
{
	return m_loopers;
}

inline void BProcess::IncrementLoopers()
{
	m_loopers++;
}

inline void BProcess::DecrementLoopers()
{
	m_loopers--;
}

void BProcess::Shutdown()
{
	// XXX Must not acquire lock, because this is called
	// by the looper shutdown loop!
	m_shutdown = true;
	// FIXME: make all the waiting threads unblock
}

bool BProcess::IsShuttingDown() const
{
	return m_shutdown;
}

void BProcess::UnscheduleHandler(BHandler *handler, bool lock)
{
	if (lock) m_lock.LockQuick();

	const bool hadRef =
		((m_pendingHandlers == handler) || handler->m_prev || handler->m_next);

	if (hadRef) {
		if (m_pendingHandlers == handler)	m_pendingHandlers = handler->m_next;
		if (handler->m_next)				handler->m_next->m_prev = handler->m_prev;
		if (handler->m_prev)				handler->m_prev->m_next = handler->m_next;
		handler->m_prev = handler->m_next = NULL;
	}

	if (lock) m_lock.Unlock();

	if (hadRef) handler->DecRefs(this);
}

bool BProcess::ScheduleNextEvent()
{
//	bout << "BProcess::ScheduleNextEvent (" << SysCurrentThread() << ")@" << SysGetRunTime() << endl;

	// Don't schedule the next event if it would introduce too much concurrency.
	// XXX TO DO -- need to allow scheduling the next event if all currently
	// running handlers are blocked.
	if (m_currentEventConcurrency >= m_maxEventConcurrency) {
		return false;
	}

	int32_t priority = B_NORMAL_PRIORITY;
	const nsecs_t nextTime	= m_pendingHandlers
								? m_pendingHandlers->NextMessageTime(&priority)
								: B_INFINITE_TIMEOUT;
	
	if (nextTime < m_nextEventTime) {
#if 0
		bout << "Setting next event time: from " << m_nextEventTime << " to " << nextTime
			<< " (" << (nextTime-SysGetRunTime()) << " from now)" << endl;
#endif
		m_nextEventTime = nextTime;
		return SLooper::_SetNextEventTime(m_nextEventTime, sysThreadPriorityBestUser);
	}

	return false;
}

void BProcess::ScheduleHandler(BHandler *handler)
{
//	bout << "BProcess::ScheduleHandler: (" << SysCurrentThread() << ")@" << SysGetRunTime() << endl;
	m_lock.LockQuick();
	const BHandler::scheduling s = handler->start_schedule();
	if (s != BHandler::CANCEL_SCHEDULE) {
		handler->IncRefs(this);
		UnscheduleHandler(handler,false);
		InsertHandler(&m_pendingHandlers,handler);
		handler->done_schedule();
	} else {
		UnscheduleHandler(handler,false);
	}
	
	ScheduleNextHandler();		//  releases the lock
}

void BProcess::ScheduleNextHandler()
{
	const bool reschedule = ScheduleNextEvent();
	
	SLooper* top = NULL;
	
	if (reschedule)
	{
		// we need to send a message to the looper on top of the stack.
		// we remove it then inform it that we are about to send it a 
		// message by setting m_reschedule.
		top = UnsafePopLooper();
	
		if (top)
			top->m_rescheduling = true;
	}
	
	m_lock.Unlock();

	if (top) 
	{
		top->_Signal();
		top->m_rescheduling = false;
	} 
}

void BProcess::DispatchMessage(SLooper* looper)
{
	//bout << "BProcess::DispatchMessage: (" << SysCurrentThread() << ")@" << SysGetRunTime() << endl;
	BHandler* handler = NULL;
	int32_t priority = B_NORMAL_PRIORITY;
	bool firstTime = true;
	
	m_lock.LockQuick();
	//DbgOnlyFatalErrorIf(m_currentEventConcurrency >= m_maxEventConcurrency, "We have gone past the limit on concurrent handlers!");
	if (m_currentEventConcurrency >= m_maxEventConcurrency) {
		m_lock.Unlock();
		return;
	}
	m_currentEventConcurrency++;

	// The binder driver clears its event time when processing an event,
	// so we always must inform it if there is another one to schedule.
	m_nextEventTime = B_INFINITE_TIMEOUT;
	
	nsecs_t curTime = exact_SysGetRunTime();

	while (1) {
		if ((handler = m_pendingHandlers) != NULL
				&& handler->NextMessageTime(&priority) <= curTime) {
			m_pendingHandlers = handler->m_next;
			handler->m_next = handler->m_prev = NULL;
			if (m_pendingHandlers) m_pendingHandlers->m_prev = NULL;
			handler->defer_scheduling();
		} else {
			// Nothing to do right now, time to leave.
			break;
		}
		
		// If this is the first time through the loop, we need to schedule
		// the next pending handler so that another SLooper thread can be
		// activated to execute it.
		if (firstTime) {
			ScheduleNextEvent();
			firstTime = false;
		}

		// We are now going to enter user code, don't hold the lock.
		m_lock.Unlock();
		
		if (handler->AttemptAcquire(this)) {
shortcutDispatch:
			// bout << "BProcess: Thread " << SysCurrentThread() << " dispatch to " << handler << " at pri " << priority << endl;
			looper->_SetThreadPriority(priority);
			handler->dispatch_message();
			// bout << "BProcess: Thread " << SysCurrentThread() << " returned from dispatch!" << endl;

			// Update our concept of the time.
			curTime = approx_SysGetRunTime();

			// Big shortcut (one could even call this an optimization
			// for a particular performance test).  If this handler
			// has the next message that will be processed, just do
			// it right now.
			m_lock.LockQuick();
			// We can do this if...  there is a pending message and it is next...
			const nsecs_t when = handler->NextMessageTime(&priority);
			if (m_pendingHandlers == NULL || when <= m_pendingHandlers->NextMessageTime(NULL)) {
				// And ResumeScheduling() wasn't called while processing the message...
				if (ResumingScheduling()) {
					// The above flagged that this thread called ResumeScheduling(), but it
					// really hasn't.  Psych!!
					ClearSchedulingResumed();
					// And it is time for the message.
					if (when <= curTime) {
						m_lock.Unlock();
						// Whoosh!
						// bout << "BProcess: Thread " << SysCurrentThread() << " redispatch to same handler!" << endl;
						goto shortcutDispatch;
					}
				}
			}
			m_lock.Unlock();

			handler->ResumeScheduling();
			ClearSchedulingResumed();
			handler->Release(this);
		}
		handler->DecRefs(this);

		m_lock.LockQuick();
	}

	// We have handled all available messages.  While still holding the
	// lock, reduce concurrency and schedule the next event to make sure
	// the binder is up-to-date about when it is to occur.
	m_currentEventConcurrency--;
	ScheduleNextEvent();

	m_lock.Unlock();

	// bout << "BProcess::DispatchMessage Exit: (" << SysCurrentThread() << ")@" << SysGetRunTime() << endl;

#if TARGET_HOST == TARGET_HOST_WIN32
	// Handler thread pool is implemented entirely in user space on Windows.
	looper->_SetThreadPriority(B_REAL_TIME_PRIORITY);
#endif
}

bool BProcess::ResumingScheduling()
{
	return SLooper::_ResumingScheduling();
}

void BProcess::ClearSchedulingResumed()
{
	SLooper::_ClearSchedulingResumed();
}

/* ------------------------ Binder Handles ------------------------ */

IBinder* & BProcess::BinderForHandle(int32_t handle)
{
	if (m_handleRefs.CountItems() <= (uint32_t)handle) {
		m_handleRefs.SetCapacity(handle+1);
		while (m_handleRefs.CountItems() <= (uint32_t)handle) m_handleRefs.AddItem(NULL);
	}
	return m_handleRefs.EditItemAt(handle);
}

#if COUNT_PROXIES
static int32_t maxProxy = 0;
#endif

void
BProcess::GetRemoteHostInfo(int32_t handle)
{
	(void)handle;
}

DPT(GetStrongProxyForHandle);
sptr<IBinder>
BProcess::GetStrongProxyForHandle(int32_t handle)
{
	DAP(GetStrongProxyForHandle);
	sptr<IBinder> r;

	{
		SNestedLocker::Autolock _l(m_handleRefLock);
	
#if COUNT_PROXIES
		if (handle > maxProxy) {
			maxProxy = handle;
			bout << "Now using descriptor #" << handle << endl;
		}
#endif
	
#if 0
#ifdef TRACK_CLASS_NAME
		TEST_TRACKED_CLASS(handle);
		if (tracked) {
			bout << "Pr #" << SysProcessID() << ": BProcess::GetStrongProxyForHandle(" << (void*)handle << ")" << endl;
		}
#endif
#endif

		IBinder* &b = BinderForHandle(handle);
	
		// We need to create a new BpBinder if there isn't currently one, OR we
		// are unable to acquire a weak reference on this current one.  See comment
		// in GetWeakProxyForHandle() for more info about this.
		if (b == NULL || !b->AttemptIncWeak((void*)s_processID)) {
			b = new B_NO_THROW BpBinder(handle);
			r = b;
#if HANDLE_DEBUG_MSGS
			bout << SPrintf("BProcess Creating strong BpBinder 0x%08x for handle %04x", b, handle) << endl;
#endif
		} else {
			// This little bit of nastyness is to allow us to add a primary
			// reference to the remote proxy when this team doesn't have one
			// but another team is sending the handle to us.
			b->ForceIncStrong((void*)s_processID);
			r = b;
			B_DEC_STRONG(b, (void*)s_processID);
			B_DEC_WEAK(b, (void*)s_processID); // FFB: safe
#if HANDLE_DEBUG_MSGS
			bout << SPrintf("BProcess Forcing a strong BpBinder 0x%08x for handle %04x", b, handle) << endl;
#endif
		}
	}

	return r;
}

DPT(GetWeakProxyForHandle);
wptr<IBinder>
BProcess::GetWeakProxyForHandle(int32_t handle)
{
	DAP(GetWeakProxyForHandle);
	wptr<IBinder> r;

	{
		SNestedLocker::Autolock _l(m_handleRefLock);
	
#if 0
#ifdef TRACK_CLASS_NAME
		TEST_TRACKED_CLASS(handle);
		if (tracked) {
			bout << "Pr #" << SysProcessID() << ": BProcess::GetWeakProxyForHandle(" << (void*)handle << ")" << endl;
		}
#endif
#endif

#if COUNT_PROXIES
		if (handle > maxProxy) {
			maxProxy = handle;
			bout << "Now using descriptor #" << handle << endl;
		}
#endif

		IBinder* &b = BinderForHandle(handle);
		
		// We need to create a new BpBinder if there isn't currently one, OR we
		// are unable to acquire a weak reference on this current one.  The
		// AttemptIncWeak() is safe because we know the BpBinder destructor will always
		// call ExpungeHandle(), which acquires the same lock we are holding now.
		// We need to do this because there is a race condition between someone
		// releasing a reference on this BpBinder, and a new reference on its handle
		// arriving from the driver.
		if (b == NULL || !b->AttemptIncWeak((void*)s_processID)) {
			b = new B_NO_THROW BpBinder(handle);
			r = b;
#if HANDLE_DEBUG_MSGS
			bout << SPrintf("BProcess Creating weak BpBinder 0x%08x for handle %04x", b, handle) << endl;
#endif
		} else {
			r = b;
			b->DecWeak((void*)s_processID);
#if HANDLE_DEBUG_MSGS
			bout << SPrintf("BProcess already had weak BpBinder 0x%08x for handle %04x", b, handle) << endl;
#endif
		}
	}

	return r;
}

/* Called when the last weak BpBinder disappears */
void
BProcess::ExpungeHandle(int32_t handle, IBinder* binder)
{
	SNestedLocker::Autolock _l(m_handleRefLock);
	
	IBinder* &b = BinderForHandle(handle);
#if HANDLE_DEBUG_MSGS
	bout << "BProcess Expunging BpBinder " << b << " for handle " << handle << endl;
#endif

#ifdef TRACK_CLASS_NAME
	TEST_TRACKED_CLASS(handle);
	if (tracked) {
		bout << "Pr #" << SysProcessID() << ": BProcess::ExpungeHandle(" << (void*)handle << ", " << binder << ")" << endl;
	}
#endif

	// This handle may have already been replaced with a new BpBinder
	// (if someone failed the AttemptIncWeak() above); we don't want
	// to overwrite it.
	if (b == binder) b = NULL;
}

void BProcess::StrongHandleGone(IBinder* binder)
{
	if (m_catchers.CountItems() == 0) return;
	
	SNestedLocker::Autolock _l(m_handleRefLock);
	catchReleaseFunc callbackFunc = m_catchers.ValueFor(binder);
	if (callbackFunc) {
		m_catchers.RemoveItemFor(binder);
		(*callbackFunc)(binder);
	};
}

/* ------------------------ Components ------------------------ */

// Retrieve root object from image.
void BProcess::ComponentImage::InitAtom()
{
	BSharedObject::InitAtom();
	
	status_t error = GetSymbolFor("InstantiateComponent", 0, (void**)&m_instantiate);
	if (error != B_OK || m_instantiate == NULL) {
		berr << "*** The symbol \"InstantiateComponent\" could not be found in \"" << Source() << "\"";
	}
}

// Allow this object to remain around after all strong pointers
// are gone, so we can cache it even when not currently in use.
// Instead, just enqueue a request to have the image unloaded.
status_t BProcess::ComponentImage::FinishAtom(const void*)
{
#if IMAGE_DEBUG_MSGS
	bout << "Releasing last reference on image \"" << Source() << "\" (id " << ID() << ")" << endl;
#endif
	SLooper* loop = SLooper::This();
	if (loop != NULL) {
		wptr<ComponentImage> self(this);
		if (!loop->m_dyingPackages.HasItem(self)) {
#if IMAGE_DEBUG_MSGS
			bout << "Adding image " << ID() << " to expunge list of looper " << loop << endl;
#endif
			loop->m_dyingPackages.AddItem(self);
		} else {
			// There is already a pending expunge for this looper,
			// so eat one from the expunge count.
			const int32_t oldVal = SysAtomicDec32(&m_numPendingExpunge);
			DbgOnlyFatalErrorIf(oldVal <= 2, "BProcess::ComponentImage: Bad pending expunge count!");
#if IMAGE_DEBUG_MSGS
			bout << "********************************************************" << endl;
			bout << "**** ComponentImage: Consuming expunge for image " << ID() << ", looper " << loop << endl;
			bout << "********************************************************" << endl;
#endif
		}
	} else {
		ErrFatalError("Going to leak an image!");
	}
	return B_ERROR;
}

status_t BProcess::ComponentImage::IncStrongAttempted(uint32_t flags, const void* id)
{
	if (SysAtomicOr32(&m_expunged, 0) != 0) {
		// This image has been removed from the process, so we
		// can't allow it to be used any more.
		return SAtom::IncStrongAttempted(flags, id);
	}

	// Transitioning back from zero strong references means that
	// FinishAtom() will be called again, so there will be another
	// expunge.  Keep count of that, so we don't actually unload
	// the image until the last expunge has happened.
	const int32_t oldVal = SysAtomicInc32(&m_numPendingExpunge);
#if IMAGE_DEBUG_MSGS
	bout << "IncStrongAttempted pending to " << oldVal+1 << " on image \"" << Source() << "\" (id " << ID() << ")" << endl;
#endif
	DbgOnlyFatalErrorIf(oldVal <= 0, "BProcess::ComponentImage: Bad pending expunge count!");
	
	// In this case allow the image to be re-acquired.
	return B_OK;
}

#if 0
// this code no longer applies, because libraries aren't in packages.  We'll have to
// build this back up if and when that becomes possible
sptr<BSharedObject>
BProcess::GetPackage(const SValue& file, attach_func attach)
{
	sptr<ComponentImage> image = get_shared_object(file, true, attach);
	return image.ptr();
}
#endif

sptr<BProcess::ComponentImage>
BProcess::get_shared_object(const SValue& file, const SValue& componentInfo, bool fake, attach_func attach)
{
	sptr<ComponentImage> image;
	sptr<BProcess::ImageData::ImageEntry> imageEntry;
	bool found;
	
	do {
		// See if the .so that the component is in is already loaded
		m_imageLock.LockQuick();
		{
			imageEntry = m_imageData->images.ValueFor(file, &found);
			if (found) {
				image = imageEntry->image.promote();
			} else {
				// If this image doesn't exist, insert a new entry that
				// is marked as currently loading.
				imageEntry = new BProcess::ImageData::ImageEntry;
				imageEntry->loading.Close();
				m_imageData->images.AddItem(file, imageEntry);
				image = NULL;
			}
		}
		m_imageLock.Unlock();
		
		// If the image wasn't found, load it now.
		if (!found) {
			// First expunge any images that are pending for this
			// looper.  We asume this is safe (i.e., we aren't called
			// from a destructor inside one of these images) because
			// any code that instantiates a component during its
			// destruction SHOULD DIE.  As it will very soon after
			// we return.
			if (SLooper::This()) SLooper::This()->ExpungePackages();

			if (file.IsDefined()) {
				SPackage package(componentInfo["package"].AsString());
				image = new B_NO_THROW ComponentImage(file, package, fake, attach);
			}
			m_imageLock.LockQuick();
			// If it was loaded okay, place it in our cache and
			// tell any waiting threads that it is available now.
			if (image != NULL && image->InitCheck() == B_OK) {
				imageEntry = m_imageData->images.ValueFor(file);
				if (imageEntry != NULL) {
					imageEntry->image = image;
				} else {
					image = NULL;
				}

			// If there was an error loading it, just remove the
			// entry we created.  We have to remove it now because
			// otherwise it will never get cleaned up.
			} else {
				m_imageData->images.RemoveItemFor(file);
				imageEntry = NULL;
			}
			m_imageLock.Unlock();
			if (imageEntry != NULL) imageEntry->loading.Open();
		
		// If an entry image was found, but it hasn't yet been loaded,
		// wait for whoever is doing so to complete.
		} else if (image == NULL && imageEntry != NULL) {
			imageEntry->loading.Wait();
		}
	
	// Continue until we have either found/loaded an image, or found
	// a bad entry or had an error.
	} while (image == NULL && imageEntry != NULL);

	return image;
}

/*
	While it's not possible to actually do it, it's possible to
	make manifest files that describe a circular dependency of
	one VM on another, and it's not okay to crash if some mean
	person does this.  So keep a list of vms that we're trying
	instantiate, and fail if it's cyclical.
*/
sptr<IBinder>
BProcess::DoInstantiate(	const SContext& context,
						const SValue &componentInfo,
						const SString &component,
						SVector<SString> &vmIDs,
						const SValue &args,
						status_t* outError)
{
	sptr<IBinder> b;
	SValue v;
	bool found;
	size_t i, count;
	
//#if BUILD_TYPE == BUILD_TYPE_DEBUG
//	SContext default_context = get_default_context();
//	if (default_context.AsBinder() != context.AsBinder()) {
//		// for now, all the shell commands from the bootscript are instantiated
//		// in the system context.
//		// Weed those out (and don't do the assert)
//		SValue shellInterface("org.openbinder.tools.ICommand");
//		if (shellInterface != componentInfo["interface"]) {
//			bout << "WHOA! Instantiating component in System context! (Is this okay?)" << endl;
//			bout << "... componentInfo = " << componentInfo << endl;
//		}
//	}
//	// DbgOnlyFatalErrorIf(default_context.AsBinder() != context.AsBinder(), "Why are we instantiating in System context?");
//
//#endif

	SValue virtualMachineNameValue = componentInfo["vm"];
	
	// If there is no VM specified, then it's an executable file
	if (!virtualMachineNameValue.IsDefined()) {
		
		const SValue file = componentInfo["file"];
		if (!file.IsDefined()) {
			if (outError) *outError = B_ENTRY_NOT_FOUND;
			DbgOnlyFatalError("No file found for component");
			return NULL;
		}

		// Load the image for this package.
		sptr<ComponentImage> image = get_shared_object(file, componentInfo);
		
		// And now, if we found the image, instantiate the requested component,
		// using the component name, not the full ID.
		if (image != NULL) {
#if BUILD_TYPE == BUILD_TYPE_DEBUG
			const int32_t origCount = image->StrongCount();
#endif
			sptr<IBinder> obj = image->InstantiateComponent(componentInfo["local"].AsString(), context, args);
#if BUILD_TYPE == BUILD_TYPE_DEBUG
			const int32_t newCount = image->StrongCount();
			if (obj != NULL && newCount <= origCount) {
				bout << endl
					<< "***********************************************" << endl
					<< "******************* WARNING *******************" << endl
					<< "***********************************************" << endl
					<< "A component MAY not be using SPackageSptr!!!" << endl
					<< "Original image reference count was " << origCount
					<< ", the new count is " << newCount << endl
					<< "The component info is: " << componentInfo << endl
					<< endl;
			}
#endif
			if (outError) *outError = B_OK;
			return obj;
		}
		
		if (outError) *outError = B_ERROR;
		return NULL;
	}
	// avoid end-of-non-void-function-not-reached warning
	/* else */
#if !LIBBE_BOOTSTRAP
	{
		SString virtualMachineComponent = virtualMachineNameValue.AsString();
		
		SValue vmComponentInfo;
		SString vmComponentName;
		
		context.LookupComponent(virtualMachineComponent, &vmComponentInfo);
		
		// If we found a cycle, fail
		count = vmIDs.CountItems();
		for (i=0; count; i++) {
			if (vmIDs[i] == virtualMachineComponent) {
				if (outError) *outError = B_ERROR;
				return NULL;
			}
		}
		vmIDs.AddItem(virtualMachineComponent);
		
		// Get a running vm!
		m_imageLock.LockQuick();
		sptr<IVirtualMachine> vm = m_imageData->vms.ValueFor(virtualMachineComponent, &found);
		
		if (!found) {
			// Oooh.  Recursion.  Instantiate the virtual machine.
			// There's no good way of passing args through to the vm also, so pass B_UNDEFINED_VALUE
			b = this->DoInstantiate(context, vmComponentInfo, vmComponentName, vmIDs, B_UNDEFINED_VALUE, outError);
			vm = IVirtualMachine::AsInterface(b);
			
			ErrFatalErrorIf(vm == NULL, "could not instantiate VM");
			
			m_imageData->vms.AddItem(virtualMachineComponent, vm);
			
			vm->Init();
		}
		m_imageLock.Unlock();
		
		// Have the vm instantiate the object
		return vm->InstantiateComponent(componentInfo, component, args, outError);
	}
#endif
}

sptr<IBinder>
BProcess::InstantiateComponent(	const sptr<INode>& node,
								const SValue& componentInfo,
								const SString& component,
								const SValue& args,
								status_t* outError)
{
	SContext context(node);
	SVector<SString> vmIDs;
	return this->DoInstantiate(context, componentInfo, component, vmIDs, args, outError);
}

#if !LIBBE_BOOTSTRAP
int32_t
BProcess::AtomMarkLeakReport()
{
	return SAtom::MarkLeakReport();
}

void
BProcess::AtomLeakReport(int32_t mark, int32_t last, uint32_t flags)
{
	SAtom::LeakReport(mark, last, flags);
}
#endif

bool
BProcess::ExpungePackage(const wptr<ComponentImage>& image)
{
	bool expunged = false;
	
#if IMAGE_DEBUG_MSGS
	bout << "ExpungePackage: " << image << endl;
#endif

	if (image != NULL) {
		SNestedLocker::Autolock _l(m_imageLock);
		// To avoid race conditions, we can only expunge the image if
		// at this point there are no strong pointers on it.  Any time
		// before now someone could have instantiated a component and
		// thus re-acquired a pointer.  We also need to decrement the
		// number of pending expunges in the image, and only do the
		// expunge on the last one -- this is to deal with the case
		// where the image may have been re-used and ended up on the
		// expunge list of multiple loopers.
		const int32_t oldPending = image.unsafe_ptr_access()->DecPending();
#if IMAGE_DEBUG_MSGS
		bout << "Old pending=" << oldPending << ", has strong pointers=" << image.unsafe_ptr_access()->HasStrongPointers() << endl;
#endif
		DbgOnlyFatalErrorIf(oldPending <= 0, "BProcess::ExpungePackage: bad pending expunge count");
#if IMAGE_DEBUG_MSGS
		if (oldPending > 1) {
			bout << "********************************************************" << endl;
			bout << "**** ExpungePackage: Skipping expunge for " << image << endl;
			bout << "********************************************************" << endl;
		}
#endif
		if (oldPending <= 1 && !image.unsafe_ptr_access()->HasStrongPointers()) {
			// Make sure we are removing -this- image.  It could have
			// already been expunged, and some other image loaded with
			// the same name.
			image.unsafe_ptr_access()->MakeExpunged();
			const SValue file = image.unsafe_ptr_access()->Source();
			bool found;
			sptr<BProcess::ImageData::ImageEntry> e = m_imageData->images.ValueFor(file, &found);
#if IMAGE_DEBUG_MSGS
			bout << "File=" << file << ", found=" << found << ", image=" << (e != NULL ? e->image : wptr<ComponentImage>()) << endl;
#endif
			if (found && e->image == image) {
#if IMAGE_DEBUG_MSGS
				bout << "Expunging image " << image << " (" << file << ")" << endl;
#endif
				image.unsafe_ptr_access()->Detach();
				m_imageData->images.RemoveItemFor(file);
				expunged = true;
			}
		}
	}
	
	return expunged;
}

void BProcess::BatchPutReferences()
{
	SLooper::This()->_TransactWithDriver(false);
}

#if BINDER_DEBUG_LIB
static int32_t nextFakeProcessIDDelta = 1;

static int32_t pretend_team(team_id teamid)
{
	sptr<BProcess> team(new BProcess(teamid));
	SLooper::InitMain(team);
	SLooper::SetContextObject(team->AsBinder(), B_DEFAULT_CONTEXT);
	SLooper::SetContextObject(team->AsBinder(), B_SYSTEM_CONTEXT);
	SLooper::Loop();
	return 0;
}
#endif

#if !LIBBE_BOOTSTRAP
sptr<IProcess> BProcess::Spawn(const SString& name, const SValue& env, uint32_t flags)
{
#if BINDER_DEBUG_LIB
		(void)env;
		(void)flags;
		
		team_id teamid = this_team()+atomic_add(&nextFakeProcessIDDelta,1);
		bout << "spawning pretend team " << teamid << endl;
		
		SysHandle thid = spawn_thread((thread_entry)pretend_team,"pretend_team",B_NORMAL_PRIORITY,(void *)teamid);
		ErrFatalErrorIf(thid < 0, "could not spawn pretend_team");
		
		sptr<IProcess> team
			= IProcess::AsInterface(SLooper::GetRootObject(thid, teamid));
		ErrFatalErrorIf(team == NULL, "pretend_team gave us a root object that wasn't an IProcess");
#else
		SString fullName("/binder_team");
		mkdir(fullName.String(), S_IRWXU|S_IRWXG|S_IRWXO);
		fullName += "/";
		if (name != "") fullName += name;
		else fullName += "anonymous";
		symlink("/system/servers/remote_place", fullName.String());

		sptr<IBinder> teamBinder = SpawnFile(fullName, env, flags);
		sptr<IProcess> team = IProcess::AsInterface(teamBinder);
		ErrFatalErrorIf(team == NULL && teamBinder != NULL, "remote_place did not publish an IProcess");
#endif // BINDER_DEBUG_LIB
	
	return team;
}

struct env_entry
{
	const char*	var;
	SString		buf;
};

B_IMPLEMENT_SIMPLE_TYPE_FUNCS(env_entry);

inline int32_t BCompare(const env_entry& v1, const env_entry& v2)
{
	const char* s1 = v1.var ? v1.var : v1.buf.String();
	const char* s2 = v2.var ? v2.var : v2.buf.String();
	while (*s1 != 0 && *s1 != '=' && *s2 != 0 && *s2 != '=' && *s1 == *s2) {
		s1++;
		s2++;
	}
	//printf("String %s @ %ld vs %s @ %ld\n", v1.var, s1-v1.var, v2.var, s2-v2.var);
	return int32_t(*s1) - int32_t(*s2);
}

inline bool BLessThan(const env_entry& v1, const env_entry& v2)
{
	return BCompare(v1, v2) < 0;
}

sptr<IBinder> BProcess::SpawnFile(const SString& image_path, const SValue& env, uint32_t flags)
{
	#if TARGET_HOST != TARGET_HOST_BEOS
		(void)image_path;
		(void)env;
		(void)flags;
		ErrFatalError("Not implemented for this target!");
		return NULL;
	#elif BINDER_DEBUG_LIB
		(void)image_path;
		(void)env;
		(void)flags;
		ErrFatalError("Not implemented for fake teams!");
		return NULL;
	#else
		const char *argv[1];
		argv[0] = image_path.String();
		const char** newEnv = const_cast<const char**>(environ);
		SSortedVector<env_entry> entries;
		const char** allocEnv = NULL;
		if (env.IsDefined() || (flags&B_FORGET_CURRENT_ENVIRONMENT)) {
			env_entry ent;
			if (!(flags&B_FORGET_CURRENT_ENVIRONMENT)) {
				const char** e = newEnv;
				while (e && *e) {
					ent.var = *e;
					entries.AddItem(ent);
					e++;
				}
				//for (size_t i=0; i<entries.CountItems(); i++) {
				//	bout << "Old Environment: " << entries[i].var << endl;
				//}
			}
			void* i = NULL;
			SValue k, v;
			ent.var = NULL;
			while (env.GetNextItem(&i, &k, &v) >= B_OK) {
				if (!k.IsWild()) {
					ent.buf = k.AsString();
					if (ent.buf != "") {
						ent.buf += "=";
						entries.RemoveItemFor(ent);
						if (!v.IsWild()) {
							// If v is wild, the entry is just removed.
							ent.buf += v.AsString();
							entries.AddItem(ent);
						}
					}
				}
			}
			const size_t N = entries.CountItems();
			allocEnv = static_cast<const char**>(malloc(sizeof(char*)*(N+1)));
			if (allocEnv) {
				for (size_t i=0; i<N; i++) {
					const env_entry& e = entries[i];
					allocEnv[i] = e.var ? e.var : e.buf.String();
					//bout << "New Environment: " << allocEnv[i] << endl;
				}
				allocEnv[N] = NULL;
				newEnv = allocEnv;
			}
		}
		SysHandle tid = load_image(1, argv, newEnv);
		if (allocEnv) free(allocEnv);
		if (tid < 0) {
			SString msg("coult not launch ");
			msg += image_path;
			ErrFatalError(msg.String());
		}
		return SLooper::GetRootObject(tid);
	#endif
}
#endif

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
