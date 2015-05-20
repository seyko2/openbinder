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

#include <support/Handler.h>

#include <support/Autolock.h>
#include <support/Looper.h>
#include <support/Message.h>
#include <support/StdIO.h>
#include <support/Process.h>

#include <support_p/SupportMisc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <new>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {

using namespace palmos::osp;
#endif

/**************************************************************************************/

SysCriticalSectionType g_timeLock;
nsecs_t g_lastTime = 0;
int32_t g_timeRequests = 10000;

nsecs_t approx_SysGetRunTime()
{
	g_threadDirectFuncs.criticalSectionEnter(&g_timeLock);
	if ((++g_timeRequests) < 1) {
		nsecs_t time = g_lastTime;
		g_threadDirectFuncs.criticalSectionExit(&g_timeLock);
		return time;
	}

	g_threadDirectFuncs.criticalSectionExit(&g_timeLock);
	return exact_SysGetRunTime();
}

nsecs_t exact_SysGetRunTime()
{
	g_threadDirectFuncs.criticalSectionEnter(&g_timeLock);
#if TARGET_HOST == TARGET_HOST_PALMOS
	// It's just slightly faster to call this than the cover below.
	nsecs_t time = KALGetTime(B_TIMEBASE_RUN_TIME);
#else
	nsecs_t time = SysGetRunTime();
#endif
	g_lastTime = time;
	g_timeRequests = 0;
	g_threadDirectFuncs.criticalSectionExit(&g_timeLock);
	return time;
}

/**************************************************************************************/

enum {
	ghCanSchedule 		= 0x00000001,
	ghScheduled			= 0x00000002,
	ghNeedSchedule		= 0x00000004,
	ghDying				= 0x00000008,
	ghCalledSchedule	= 0x00000010
};

/**************************************************************************************/

SHandler::SHandler()
	:	m_team(SLooper::Process()), m_lock("Some SHandler"),
		m_state(ghCanSchedule), m_next(NULL), m_prev(NULL), m_externalLock(NULL)
{
}

SHandler::SHandler(SLocker* externalLock)
	:	m_team(SLooper::Process()), m_lock("Some SHandler"),
		m_state(ghCanSchedule), m_next(NULL), m_prev(NULL), m_externalLock(externalLock)
{
}

SHandler::SHandler(const SContext& /*context*/)
	:	m_team(SLooper::Process()), m_lock("Some SHandler"),
		m_state(ghCanSchedule), m_next(NULL), m_prev(NULL), m_externalLock(NULL)
{
}

SHandler::~SHandler()
{
	unschedule();
	RemoveAllMessages();
}

/**************************************************************************************/

status_t SHandler::PostMessageAtTime(	const SMessage &message,
										nsecs_t absoluteTime,
										uint32_t flags)
{
	const nsecs_t curTime = approx_SysGetRunTime();
	return EnqueueMessage(message, absoluteTime > curTime ? absoluteTime : curTime, flags);
}

status_t SHandler::PostDelayedMessage(	const SMessage &message,
										nsecs_t delay,
										uint32_t flags)
{
	return EnqueueMessage(message, exact_SysGetRunTime() + (delay >= 0 ? delay : 0), flags);
}

status_t SHandler::PostMessage(	const SMessage &message,
								uint32_t flags)
{
	return EnqueueMessage(message, approx_SysGetRunTime(), flags);
}

status_t SHandler::EnqueueMessage(const SMessage &message, nsecs_t time, uint32_t flags)
{
	flags &= (B_POST_REMOVE_DUPLICATES|B_POST_KEEP_UNIQUE);
	if (flags == 0) {
//		bout << "SHandler::Enqueue: @" << SysGetRunTime() << endl;
	
		SMessage* msg = new B_NO_THROW SMessage(message);
		if (msg != NULL) {
			bool doSchedule = false;

			m_lock.LockQuick();
			msg->SetWhen(time);
			m_msgQueue.EnqueueMessage(msg);
			if (m_msgQueue.Head() == msg) {
				doSchedule =
					((m_state & (ghCanSchedule|ghNeedSchedule|ghCalledSchedule)) == ghCanSchedule);
				m_state |= (doSchedule ? ghNeedSchedule|ghCalledSchedule : ghNeedSchedule);
			}
			m_lock.Unlock();

//			bout << "Message: " << *msg << endl << endl;
	
			if (doSchedule) m_team->ScheduleHandler(this);
			return B_OK;
		}

		return B_NO_MEMORY;
	}

	return enqueue_unique_message(message, time, flags);
}

status_t SHandler::enqueue_unique_message(const SMessage &message, nsecs_t time, uint32_t flags)
{
//	bout << "SHandler::enqueue_unique_message: @" << SysGetRunTime() << endl;
	
	SValue prevData;
	SMessageList removed;

	bool doSchedule = false;

	m_lock.LockQuick();
	SMessage* msg = (flags&B_POST_REMOVE_DUPLICATES)
		? m_msgQueue.EnqueueMessageRemoveDups(message, time, &prevData, &removed)
		: m_msgQueue.EnqueueUniqueMessage(message, time, &prevData, &removed);
	if (m_msgQueue.Head() == msg && msg != NULL) {
		doSchedule =
			((m_state & (ghCanSchedule|ghNeedSchedule|ghCalledSchedule)) == ghCanSchedule);
		m_state |= (doSchedule ? ghNeedSchedule|ghCalledSchedule : ghNeedSchedule);
	}
	m_lock.Unlock();

//	bout << "Message: " << *msg << endl << endl;
	
	removed.MakeEmpty();

	if (doSchedule) m_team->ScheduleHandler(this);
	
	return msg != NULL ? B_OK : B_NO_MEMORY;
}

status_t SHandler::HandleMessage(const SMessage &)
{
	return B_ERROR;
}

/**************************************************************************************/

void SHandler::ResumeScheduling()
{
	if (BProcess::ResumingScheduling()) {
		m_lock.LockQuick();
		const bool doSchedule =
			((m_state & (ghNeedSchedule|ghCalledSchedule)) == ghNeedSchedule);
		m_state |= (doSchedule ? ghCanSchedule|ghCalledSchedule : ghCanSchedule);
		m_lock.Unlock();
		if (doSchedule) m_team->ScheduleHandler(this);
	}
}

nsecs_t SHandler::NextMessageTime(int32_t* out_priority)
{
	SLocker::Autolock _l(m_lock);
	return m_msgQueue.OldestMessage(out_priority);
}

SMessage * SHandler::DequeueMessage(uint32_t what)
{
	bool more,doSchedule=false;

	m_lock.LockQuick();

	const SMessage *oldFirst = m_msgQueue.Head();
	SMessage *msg = m_msgQueue.DequeueMessage(what,&more);
	if (m_msgQueue.Head() != oldFirst) {
		doSchedule = ((m_state & (ghCanSchedule|ghNeedSchedule|ghCalledSchedule)) == ghCanSchedule);
		m_state |= (doSchedule ? ghNeedSchedule|ghCalledSchedule : ghNeedSchedule);
	}

	m_lock.Unlock();

	if (doSchedule) m_team->ScheduleHandler(this);
	return msg;
};

void SHandler::DispatchAllMessages()
{
	unschedule();
	while (dispatch_message() == B_OK);
};

int32_t SHandler::CountMessages(uint32_t what)
{
	SLocker::Autolock _l(m_lock);
	return m_msgQueue.CountMessages(what);
}

void SHandler::FilterMessages(	const filter_functor_base& functor,
								uint32_t flags, void* data,
								SMessageList* removed)
{
	SMessageList localRemoved;
	if (!removed) removed = &localRemoved;
	
	bool doSchedule = false;
	uint32_t state = 0;

	m_lock.LockQuick();
	
	const nsecs_t now = (flags&FILTER_FUTURE_FLAG) ? 0 : exact_SysGetRunTime();
	const SMessage* oldHead = m_msgQueue.Head();
	const SMessage* msg = (flags&FILTER_REVERSE_FLAG)
						? m_msgQueue.Tail() : m_msgQueue.Head();
	while (msg) {
		filter_action action = FILTER_KEEP;
		
		// If we are including future messages in the filter, or
		// the message time is less than now...
		if ((flags&FILTER_FUTURE_FLAG) || msg->When() <= now) {
			action = functor(msg, data);
		}
		
		if (action == FILTER_STOP)
			break;
		
		const SMessage* next = (flags&FILTER_REVERSE_FLAG)
							 ? m_msgQueue.Previous(msg) : m_msgQueue.Next(msg);
		if (action == FILTER_REMOVE) {
			SMessage* m = m_msgQueue.Remove(msg);
			if (flags&FILTER_REVERSE_FLAG) removed->AddTail(m);
			else removed->AddHead(m);
		}
		
		msg = next;
	}
	
	if (m_msgQueue.Head() != oldHead) {
		if (!m_msgQueue.IsEmpty()) {
			doSchedule =
				((m_state & (ghCanSchedule|ghNeedSchedule|ghCalledSchedule)) == ghCanSchedule);
			m_state |= (doSchedule ? ghNeedSchedule|ghCalledSchedule : ghNeedSchedule);
		} else {
			state = m_state;
			m_state &= ~ghScheduled;
		}
	}
	
	m_lock.Unlock();
	
	if (doSchedule) m_team->ScheduleHandler(this);
	else if (state & ghScheduled) m_team->UnscheduleHandler(this);
}

class function_ptr_adapter {
public:
	function_ptr_adapter(SHandler::filter_func func) : m_func(func) { }
	SHandler::filter_action adapter(const SMessage* message, void* user) const {
		return (*m_func)(message, user);
	}
private:
	SHandler::filter_func m_func;
};

void SHandler::FilterMessages(filter_func func, uint32_t flags, void* data,
							  SMessageList* removed)
{
	function_ptr_adapter adapter(func);
	filter_functor<function_ptr_adapter> f(	adapter, &function_ptr_adapter::adapter );
	FilterMessages(f, flags, data, removed);
}

static SHandler::filter_action clear_func(const SMessage* msg, void* data)
{
	return (msg->What() == (uint32_t)data) ? SHandler::FILTER_REMOVE : SHandler::FILTER_KEEP;
}

void SHandler::RemoveMessages(uint32_t what, uint32_t flags, SMessageList* removed)
{
	FilterMessages(clear_func, flags, (void*)what, removed);
}

void SHandler::RemoveAllMessages(SMessageList* outRemoved)
{
	SMessageList tmp;
	m_lock.LockQuick();
	if (outRemoved) outRemoved->Adopt(m_msgQueue);
	else tmp.Adopt(m_msgQueue);
	uint32_t state = m_state;
	m_state &= ~ghScheduled;
	m_lock.Unlock();

	tmp.MakeEmpty();
	
	if (state & ghScheduled) m_team->UnscheduleHandler(this);
};

status_t SHandler::dispatch_message()
{
//	bout << "SHandler::dispatch_message: (" << find_thread(NULL) << ")@" << SysGetRunTime() << endl;
	bool more = false;

	m_lock.LockQuick();
	SMessage *msg = m_msgQueue.DequeueMessage(B_ANY_WHAT,&more);
	if (more) m_state |= ghNeedSchedule;
	m_lock.Unlock();

	if (!msg) return B_ERROR;

	// if we have an external lock, acquire that over the HandleMessage
	if (m_externalLock) m_externalLock->Lock();
	status_t err = HandleMessage(*msg);
	if (m_externalLock) m_externalLock->Unlock();

	if (err != B_OK) {
		berr << "Error handling: " << *msg << endl;
	};

	delete msg;

	return B_OK;
}

/**************************************************************************************/

void SHandler::defer_scheduling()
{
	m_lock.LockQuick();
	m_state &= ~(ghScheduled|ghCanSchedule);
	m_lock.Unlock();
}

void SHandler::unschedule()
{
	m_lock.LockQuick();
	uint32_t state = m_state;
	m_state &= ~(ghCanSchedule|ghScheduled);
	m_state |= ghDying;
	m_lock.Unlock();

	if (state & ghScheduled) m_team->UnscheduleHandler(this);
}

SHandler::scheduling SHandler::start_schedule()
{
	SHandler::scheduling result;
	
	m_lock.LockQuick();
	m_state &= ~ghCalledSchedule;
	if ((m_state & (ghCanSchedule|ghNeedSchedule|ghDying))
				== (ghCanSchedule|ghNeedSchedule)) {
		m_state &= ~ghNeedSchedule;
		if (m_msgQueue.IsEmpty()) result = CANCEL_SCHEDULE;
		else if (m_state & ghScheduled) result = DO_RESCHEDULE;
		else result = DO_SCHEDULE;
	} else {
		result = CANCEL_SCHEDULE;
	}
	m_lock.Unlock();
	
	return result;
}

void SHandler::done_schedule()
{
	m_lock.LockQuick();
	m_state |= ghScheduled;
	m_lock.Unlock();
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
