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

#include <support_p/WindowsCompatibility.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

static SLooper::catch_root_func g_catchRootFunc = NULL;

void __initialize_looper_platform()
{
}

void __stop_looper_platform()
{
}

void __terminate_looper_platform()
{
}

void
SLooper::_ConstructPlatform()
{
	m_next = NULL;
	m_spawnedInternally = true;
	m_rescheduling = false;
	
	// This key is used to by other loopers to wake
	// this looper up to handler pending messages. 
	m_keyID = (int32_t)CreateEvent(NULL, false, false, NULL);

	SysThreadExitCallbackID idontcare;
	SysThreadInstallExitCallback(_DeleteSelf, this, &idontcare);

#if BINDER_DEBUG_MSGS
	berr	<< "SLooper: enter new looper #" << Process()->LooperCount() << " " << this << " ==> Thread="
			<< find_thread(NULL) << ", keyId=" << m_keyID << endl;
#endif
}

void
SLooper::_DestroyPlatform()
{
#if BINDER_DEBUG_MSGS
	berr	<< "SLooper: exit new looper #" << Process()->LooperCount() << " " << this << " ==> Thread="
			<< find_thread(NULL) << ", keyId=" << m_keyID << endl;
#endif
}

status_t 
SLooper::_InitMainPlatform()
{
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

	// Get the thread pool started.
	return SpawnLooper();
}

status_t SLooper::SpawnLooper()
{
	status_t status = B_BINDER_TOO_MANY_LOOPERS;
	
	SysHandle thid;
	status_t err = SysThreadCreate(NULL, "SLooper", B_NORMAL_PRIORITY, DEFAULT_STACK_SIZE, 
		(SysThreadEnterFunc*) _EnterLoop, This(), &thid);
	
	if (err == errNone)
		status = (SysThreadStart(thid) != errNone) ? B_ERROR : B_OK;

	return status;
}

status_t SLooper::_EnterLoop(SLooper *parent)
{
	sptr<BProcess> team(parent ? parent->m_team : sptr<BProcess>());
	SLooper* looper = This(team);
	status_t status = looper->_LoopSelf();
	return status;
}

void SLooper::_SetThreadPriority(int32_t priority)
{
	if (m_priority != priority) {
		// Keep on trying to set the thread priority until one "sticks".
		// $#&%!! windows.
		while (::SetThreadPriority((HANDLE)m_thid, priority) == false && priority > THREAD_PRIORITY_IDLE) {
			priority--;
#if 0
			DWORD err = GetLastError();
			printf("Reducing requested priority of thread %ld to %ld!\n", find_thread(NULL), priority);
			LPVOID lpMsgBuf;
			if (!FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPTSTR) &lpMsgBuf,
				0,
				NULL ))
			{
				// Handle the error.
				printf("ARGH!!");
			} else {
				printf("Error: %s\n", (LPCSTR)lpMsgBuf);
				LocalFree( lpMsgBuf );
			}
#endif
		}
		//printf("Setting priority of thread %ld to %ld\n", find_thread(NULL), priority);
		m_priority = priority;
	}
}

status_t SLooper::Loop()
{
	SLooper* loop = This();
	loop->m_spawnedInternally = false;
	bout << "SLooper: " << loop << " (" << loop->m_thid << ") entering! " << endl;
	loop->_LoopSelf();
	bout << "SLooper: " << loop << " (" << loop->m_thid << ") exiting! " << endl;
	return B_OK;
}

sptr<IBinder> 
SLooper::_GetRootObject(SysHandle id, team_id team)
{
	bout << "GetRootObject() not implemented for Windows!" << endl;
	return NULL;
}

void 
SLooper::SetRootObject(const sptr<IBinder>& root)
{
	bout << "SetRootObject() not implemented for Windows!" << endl;
}

void
SLooper::_CatchRootObjects(catch_root_func func)
{
	g_catchRootFunc = func;
	bout << "CatchRootObjects() not implemented for Windows!" << endl;
}

void
SLooper::_KillProcess()
{
	bout << "KillProcess() not implemented for Windows!" << endl;
}

void SLooper::_Signal()
{
	SetEvent((HANDLE)m_keyID);
}

int32_t SLooper::_LoopSelf()
{
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
	
	bout << "SLooper::_LoopSelf(): entering " << looper << " (" << looper->m_thid << ")@" << SysGetRunTime() << endl;

	m_idleTime = 0;

	while (true)
	{
		_ExpungeImages();
	
		nsecs_t timeout = 0;

		if (atBottom) {
			// If this is the bottom thread on the stack, then we DO NOT wake up
			// for events.  This thread is not allowed to handle messages because
			// it needs to be waiting for incoming transactions.
			timeout = Process()->GetIdleTimeout();
			atBottom = false;
			//bout << "Looper " << this << " at bottom so idle wait for " << timeout << endl;

		} else {
			timeout = (int32_t)Process()->GetEventDelayTime(this);
		}

		// store the time we started to block at
		m_lastTimeISlept = SysGetRunTime();

		// Block until the timeout is reached or some one signals the event.
		int result = WaitForSingleObject((HANDLE)m_keyID, (DWORD)(timeout/B_ONE_MILLISECOND));
		//bout << "Looper " << this << " woke up!" << endl;

		if (result != WAIT_TIMEOUT) {
			// I didn't time out.  This only happens if someone raises my event,
			// which means they popped me off the stack to be rescheduled.  Push
			// back on and continue to pick up the new timeout.
			//bout << "Looper " << this << " poked!  Push and continue." << endl;
			Process()->PushLooper(looper);
			continue;
		}

		// i timed i out and some one is going to send me a message.
		// yipee! so we just want to received until they do!
		if (m_rescheduling) {
			if (!repeatReschedule) {
				repeatReschedule = true;
			} else {
				// Let others run.  Gag.
				KALCurrentThreadDelay(B_ONE_MILLISECOND, B_RELATIVE_TIMEOUT);
			}
			continue;
		}

		repeatReschedule = false;
		currentTime = SysGetRunTime();
	
		if (Process()->PopLooper(looper, currentTime)) 
		{
			// go and handle the message.
			//bout << "Looper " << this << " popped and dispatching..." << endl;
			Process()->DispatchMessage(looper);
			m_idleTime = 0;
			Process()->PushLooper(looper);
		}
		else
		{
			if (m_next == NULL) atBottom = true;

			// increment the idle time so we can eventually commit seppuku!
			m_idleTime += SysGetRunTime() - m_lastTimeISlept;
		
			//bout << "Looper " << this << " has new idle time " << m_idleTime << endl;
			
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

status_t 
SLooper::IncrefsHandle(int32_t handle)
{
	TRACE();
#if BINDER_DEBUG_MSGS
	bout << "Writing increfs for handle " << handle << endl;
#endif
	return B_OK;
}

status_t 
SLooper::DecrefsHandle(int32_t handle)
{
	TRACE();
#if BINDER_DEBUG_MSGS
	bout << "Writing decrefs for handle " << handle << endl;
#endif
	return B_OK;
}

status_t 
SLooper::AcquireHandle(int32_t handle)
{
	TRACE();
#if BINDER_DEBUG_MSGS
	bout << "Writing acquire for handle " << handle << endl;
#endif
	return B_OK;
}

status_t 
SLooper::ReleaseHandle(int32_t handle)
{
	TRACE();
#if BINDER_DEBUG_MSGS
	bout << "Writing release for handle " << handle << endl;
#endif
	return B_OK;
}

status_t 
SLooper::AttemptAcquireHandle(int32_t handle)
{
	TRACE();
#if BINDER_DEBUG_MSGS
	bout << "Writing attempt acquire for handle " << handle << endl;
#endif
	status_t result = B_ERROR;
	return result;
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
	
	bout << "Transact() not implemented for Windows" << endl;
	return B_UNSUPPORTED;
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
