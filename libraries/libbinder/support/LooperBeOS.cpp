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

#include <driver/binder2_driver.h>

#if !BINDER_DEBUG_LIB
#include "priv_syscalls.h"
#include "priv_runtime.h"
extern "C" void _init_tls(void);
extern "C" char _single_threaded;
#endif

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
	m_binderDesc = open_binder(m_team->ID());
	m_in.SetLength(0);
	m_out.SetLength(0);
	
	#warning THIS CODE SUCKS.
	// XXX: What follows is awful. This is the lamest piece of code I ever wrote.
	// I hate myself. Please help me.
	// What happens is that on_exit_thread() is called when a SLocker is used the
	// first time to do some cleanup. Unfortunately, _DeleteSelf() uses a SLocker
	// so we must assure that _DeleteSelf() is called **before** the KALCriticalSectionEnter
	// callback! See Locker.cpp/PrvGetSlotAddress().
	// To achieve this, we enter a critical section *before* registering our own callback,
	// which will install the critical section cleanup callback before ours.
	// -Jon Watte
	SLocker lock;
	lock.Lock();
	lock.Unlock();
	
	SysThreadExitCallbackID idontcare;
	SysThreadInstallExitCallback(_DeleteSelf, this, &idontcare);

#if BINDER_DEBUG_MSGS
	berr	<< "SLooper: open binder descriptor ==> " << m_binderDesc << " (" << strerror(m_binderDesc) << ")"
			<< " in thread " << m_thid << endl;
#endif
}

void
SLooper::_DestroyPlatform()
{
#if BINDER_DEBUG_MSGS
	berr	<< "SLooper: closing descriptor <== " << m_binderDesc << " (" << strerror(m_binderDesc) << ")"
			<< " in thread " << m_thid << endl;
#endif

	close_binder(m_binderDesc);
	m_binderDesc = -1;
}

status_t 
SLooper::_InitMainPlatform()
{
	const char* env;
	if ((env=getenv("BINDER_MAX_THREADS")) != NULL) {
		int32_t val = atol(env);
		ioctl_binder(m_binderDesc, BINDER_SET_MAX_THREADS, &val, sizeof(val));
	} else {
		int32_t val = 2;
		ioctl_binder(m_binderDesc, BINDER_SET_MAX_THREADS, &val, sizeof(val));
	}
	if ((env=getenv("BINDER_IDLE_PRIORITY")) != NULL) {
		int32_t val = atol(env);
		ioctl_binder(m_binderDesc, BINDER_SET_IDLE_PRIORITY, &val, sizeof(val));
	}
	if ((env=getenv("BINDER_IDLE_TIMEOUT")) != NULL) {
		int64_t val = atoll(env);
		ioctl_binder(m_binderDesc, BINDER_SET_IDLE_TIMEOUT, &val, sizeof(val));
	}
	if ((env=getenv("BINDER_REPLY_TIMEOUT")) != NULL) {
		int64_t val = atoll(env);
		ioctl_binder(m_binderDesc, BINDER_SET_REPLY_TIMEOUT, &val, sizeof(val));
	}
	m_out.WriteInt32(bcSET_THREAD_ENTRY);
#if BINDER_DEBUG_LIB
	m_out.WriteInt32((int32_t)_DriverLoop);
#else
	m_out.WriteInt32((int32_t)_DriverLoopFromKernelLand);
#endif
	m_out.WriteInt32((int32_t)this);
	return B_OK;
}


status_t SLooper::SpawnLooper()
{
	SysHandle thid;
	SysThreadCreate(NULL, "SLooper", B_NORMAL_PRIORITY, sysThreadStackUI, 
		(SysThreadEnterFunc*) _DriverLoop, This(), &thid);
	status_t status = (SysThreadStart(thid) != errNone) ? B_ERROR : B_OK;
	return status;
}

#if !BINDER_DEBUG_LIB
status_t SLooper::_DriverLoopFromKernelLand(SLooper *parent)
{
	_single_threaded = 0;
	_init_tls();
	status_t result = _DriverLoop(parent);
	_thread_do_exit_notification();
	return result;
}
#endif

status_t SLooper::_DriverLoop(SLooper *parent)
{
	sptr<BProcess> team(parent ? parent->m_team : sptr<BProcess>());
	SLooper* looper = This(team);
	looper->m_out.WriteInt32(bcREGISTER_LOOPER);

	KALThreadInfoType KALThreadInfo;
	KALThreadGetInfo(KALTSDGet(kKALTSDSlotIDCurrentThreadKeyID), &KALThreadInfo);
	looper->m_stackBottom= KALThreadInfo.stack;

	status_t status = looper->_LoopSelf();
	return status;
}

status_t SLooper::Loop()
{
	SLooper* loop = This();
	loop->m_out.WriteInt32(bcENTER_LOOPER);
	status_t result = loop->_LoopSelf();
	loop->m_out.WriteInt32(bcEXIT_LOOPER);
	loop->_TransactWithDriver(false);
	bout << "SLooper: " << loop << " (" << loop->m_thid << ") exiting: " << strerror(result) << endl;
	return (loop->m_lastError = result);
}

#if BINDER_DEBUG_MSGS
static const char *inString[] = {
	"brOK",
	"brTIMEOUT",
	"brWAKEUP",
	"brTRANSACTION",
	"brREPLY",
	"brACQUIRE_RESULT",
	"brTRANSACTION_COMPLETE",
	"brINCREFS",
	"brACQUIRE",
	"brATTEMPT_ACQUIRE",
	"brRELEASE",
	"brDECREFS",
	"brEVENT_OCCURRED",
	"brFINISHED"
};
#endif

void
SLooper::_SetNextEventTime(nsecs_t when, int32_t priority)
{
	TRACE();
	SLooper* looper = This();	
//	bout << "SLooper::_SetNextEventTime:enter (" << find_thread(NULL) << ")@" << SysGetRunTime() << " when = " << when << endl;
	
	binder_wakeup_time wakeup;
	wakeup.time = when;
	wakeup.priority = priority;
	ioctl_binder(looper->m_binderDesc, BINDER_SET_WAKEUP_TIME, &wakeup, sizeof(wakeup));
}

void SLooper::_SetThreadPriority(int32_t priority)
{
	if (m_priority != priority) {
		//printf("*** Changing priority of thread %ld from %ld to %ld\n",
		//		m_thid, m_priority, priority);
		m_priority = priority;
		set_thread_priority(m_thid, m_priority);
	}
}

status_t 
SLooper::_HandleCommand(int32_t cmd)
{
	BBinder *ptr;
	status_t result = B_OK;
	
#if BINDER_DEBUG_MSGS
	berr	<< "Thread " << find_thread(NULL)
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
			#if BINDER_REFCOUNT_MSGS
			bout << "Calling IncStrong() on " << ptr << " " << typeid(*ptr).name()
				<< " (atom " << (SAtom*)ptr << ")" << endl;
			#endif
			ptr->IncStrong(reinterpret_cast<void*>(m_teamID));
		} break;
		case brRELEASE: {
			ptr = (BBinder*)m_in.ReadInt32();
			#if BINDER_REFCOUNT_MSGS
			bout << "Calling DecStrong() on " << ptr << " " << typeid(*ptr).name()
				<< " (atom " << (SAtom*)ptr << ")" << endl;
			#endif
			#if BINDER_REFCOUNT_RESULT_MSGS
			int32_t r =
			#endif
			ptr->DecStrong(reinterpret_cast<void*>(m_teamID));
			#if BINDER_REFCOUNT_RESULT_MSGS
			bout << "DecStrong() result on " << ptr << ": " << r << endl;
			if (r > 1) ptr->Report(bout);
			#endif
		} break;
		case brATTEMPT_ACQUIRE: {
			m_priority = m_in.ReadInt32();
			ptr = (BBinder*)m_in.ReadInt32();
			#if BINDER_REFCOUNT_MSGS
			bout << "Calling AttemptIncStrong() on " << ptr << " " << typeid(*ptr).name()
				<< " (atom " << (SAtom*)ptr << ")" << endl;
			#endif
			const bool success = ptr->AttemptIncStrong(reinterpret_cast<void*>(m_teamID));
			m_out.WriteInt32(bcACQUIRE_RESULT);
			m_out.WriteInt32((int32_t)success);
			#if BINDER_REFCOUNT_RESULT_MSGS
			bout << "AttemptIncStrong() result on " << ptr << ": " << success << endl;
			#endif
		} break;
		case brINCREFS: {
			ptr = (BBinder*)m_in.ReadInt32();
			#if BINDER_REFCOUNT_MSGS
			bout << "Calling IncWeak() on " << ptr << " " << typeid(*ptr).name()
				<< " (atom " << (SAtom*)ptr << ")" << endl;
			#endif
			ptr->IncWeak(reinterpret_cast<void*>(m_teamID));
		} break;
		case brDECREFS: {
			ptr = (BBinder*)m_in.ReadInt32();
			#if BINDER_REFCOUNT_MSGS
			bout << "Calling DecWeak() on " << ptr << " " << typeid(*ptr).name()
				<< " (atom " << (SAtom*)ptr << ")" << endl;
			#endif
			#if BINDER_REFCOUNT_RESULT_MSGS
			int32_t r =
			#endif
			ptr->DecWeak(reinterpret_cast<void*>(m_teamID));
			#if BINDER_REFCOUNT_RESULT_MSGS
			bout << "DecWeak() result on " << ptr << ": " << r << endl;
			if (r > 1) ptr->Report(bout);
			#endif
		} break;
		case brTRANSACTION: {
			binder_transaction_data tr;
			ssize_t amt = m_in.Read(&tr, sizeof(tr));
			if (amt < (ssize_t)sizeof(tr)) {
				ErrFatalError("Bad read!");
				return (m_lastError = (amt < B_OK ? amt : B_BAD_VALUE));
			}
			
			SParcel buffer(tr.data.ptr.buffer, tr.data_size, _BufferFree, this);
			buffer.SetBinderOffsets(tr.data.ptr.offsets, tr.offsets_size, false);
			m_priority = tr.priority;
			
			SLooper::catch_root_func crf;
			if ((tr.flags&tfRootObject) && (crf=g_catchRootFunc) != NULL) {
				SValue value;
				buffer.GetValues(1, &value);
				sptr<IBinder> root(value.AsBinder());
				if (root != NULL) crf(root);
				else ErrFatalError("Captured transaction does not contain binder!");
			
			} else if (tr.target.ptr) {
#if BINDER_DEBUG_MSGS
				bout << "Received transaction buffer: " << buffer.Data() << endl;
#endif

#if BINDER_BUFFER_MSGS
				printf("Creating transaction buffer for %p, now have %ld open.\n",
						tr.data.ptr.buffer, atomic_add(&g_openBuffers, 1) + 1);
#endif

				sptr<BBinder> b((BBinder*)tr.target.ptr);
				SParcel reply(_BufferReply, this);
				const status_t error = b->Transact(tr.code, buffer, &reply, 0);
				if (error < B_OK) reply.Reference(NULL, error);
				reply.Reply();
				
			} else {
				ErrFatalError("No target!");
				// Just to be safe.
				SParcel reply(_BufferReply, this);
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
			result = B_IO_ERROR;
		} break;
		default: {
			result = B_ERROR;
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
		cmd = m_in.ReadInt32();
		
#if BINDER_DEBUG_MSGS
		berr << "_WaitForCompletion got : " << cmd << " " << inString[cmd] << endl;
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
			bout << "Result of bcATTEMPT_ACQUIRE: " << result << endl;
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
				} else {
					reply->Reference(NULL, *static_cast<const ssize_t*>(tr.data.ptr.buffer));
					_BufferFree(tr.data.ptr.buffer, tr.data_size, this);
				}
				setReply = true;
#if BINDER_BUFFER_MSGS
				bout	<< "Creating reply buffer for " << tr.data.ptr.buffer
						<< ", now have " << (atomic_add(&g_openBuffers, 1)+1)
						<< " open." << endl;
#endif
			} else {
#if BINDER_BUFFER_MSGS
				bout << "Immediately freeing buffer for " << tr.data.ptr.buffer << endl;
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
		bout	<< "Freeing binder buffer for " << data << ", now have "
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
	const bool needRead = m_in.Position() == m_in.Length();
	
	// How much is available for writing?  If a read is requested, but
	// we aren't going to read now, then we say 0.
	const ssize_t outAvail = (!doRead || needRead) ? m_out.Length() : 0;
	
	// This is what we'll write.
	bwr.write_size = outAvail;
	bwr.write_buffer = m_out.Data();
	//bout << "Writing: " << indent << SHexDump(bwr.write_buffer, bwr.write_size) << dedent << endl;

	// This is what we'll read.
	if (doRead && needRead) {
		bwr.read_size = m_in.Avail();
		bwr.read_buffer = m_in.ReAlloc(bwr.read_size);
	} else {
		bwr.read_size = 0;
	}
	
	// Anything to do this time?
	if (bwr.write_buffer == 0 && bwr.read_buffer == 0)
		return B_OK;
	
	// Do it!
	ssize_t amt = ioctl_binder(m_binderDesc,BINDER_WRITE_READ,&bwr,sizeof(bwr));
	if (amt >= 0) {
		//bout << "Wrote " << bwr.write_size << ", expected " << outAvail << endl;
		ErrFatalErrorIf(bwr.write_size != outAvail, "Didn't write it all!");
		// Update the write buffer.
		if (bwr.write_size > 0) {
			m_out.SetLength(outAvail-bwr.write_size);
		}
		// Update the read buffer.
		if (bwr.read_size > 0) {
			m_in.SetLength(bwr.read_size);
			m_in.SetPosition(0);
			//bout << "Read: " << indent << SHexDump(bwr.read_buffer, bwr.read_size) << dedent << endl;
		}
		return B_OK;
	}
	return amt;
}

sptr<IBinder> 
SLooper::_GetRootObject(thread_id id, team_id team)
{
	m_out.WriteInt32(bcRESUME_THREAD);
	m_out.WriteInt32(id);
	#if BINDER_DEBUG_LIB
	if (team < B_OK) {
		team = SysProcessID()+1;
	}
	m_out.WriteInt32(team);
	#else
	(void)team;
	#endif
	
	SParcel data;
	_WaitForCompletion(&data);
	SValue value;
	data.GetValues(1, &value);
	
	#if BINDER_DEBUG_MSGS
	bout << "Getting root object:" << endl << indent
			<< "Value: " << value << endl
			<< "Data: " << data << endl << dedent;
	#endif
	
	return value.AsBinder();
}

status_t 
SLooper::SetRootObject(sptr<IBinder> rootNode)
{
	SValue value(sptr<IBinder>(rootNode.ptr()));
	SParcel data;
	data.SetValues(&value, NULL);
	
	#if BINDER_DEBUG_MSGS
	bout << "Setting root object:" << endl << indent
			<< "Value: " << value << endl
			<< "Data: " << data << endl << dedent;
	#endif
	
	return This()->_Reply(tfRootObject,data);
}

void
SLooper::_CatchRootObjects(catch_root_func func)
{
	g_catchRootFunc = func;
	m_out.WriteInt32(bcCATCH_ROOT_OBJECTS);
}

// Temporary implementation until IPC arrives.
SLocker g_defaultContextAccess("g_defaultContext");
static sptr<IBinder> g_defaultContext = 0;

void
SLooper::SetRootObject(const sptr<IBinder>& context)
{
	SAutolock _l(g_defaultContextAccess.Lock());
	g_defaultContext = context;
}

sptr<IBinder>
SLooper::GetRootObject()
{
	SAutolock _l(g_defaultContextAccess.Lock());
	return g_defaultContext;
}

void
SLooper::_KillProcess()
{
	m_out.WriteInt32(bcKILL_TEAM);
	_TransactWithDriver(false);
}

void SLooper::_Signal()
{
}

int32_t SLooper::_LoopSelf()
{
	// The main thread may be calling us after doing initialization.
	// We want to unload any images now, rather than waiting for some
	// future binder event that may be a long time in coming.
	_ExpungeImages();
	
	int32_t cmd,err = B_OK;
	while (err == B_OK) {
		if ((err=_TransactWithDriver()) < B_OK) break;
		cmd = m_in.ReadInt32();
		err = _HandleCommand(cmd);
		_ExpungeImages();
	}
	return err;
}

status_t 
SLooper::IncrefsHandle(int32_t handle)
{
	TRACE();
#if BINDER_DEBUG_MSGS
	bout << "Writing increfs for handle " << handle << endl;
#endif
	m_out.WriteInt32(bcINCREFS);
	m_out.WriteInt32(handle);
	return B_OK;
}

status_t 
SLooper::DecrefsHandle(int32_t handle)
{
	TRACE();
#if BINDER_DEBUG_MSGS
	bout << "Writing decrefs for handle " << handle << endl;
#endif
	m_out.WriteInt32(bcDECREFS);
	m_out.WriteInt32(handle);
	return B_OK;
}

status_t 
SLooper::AcquireHandle(int32_t handle)
{
	TRACE();
#if BINDER_DEBUG_MSGS
	bout << "Writing acquire for handle " << handle << endl;
#endif
	m_out.WriteInt32(bcACQUIRE);
	m_out.WriteInt32(handle);
	//m_out.Flush();
	return B_OK;
}

status_t 
SLooper::ReleaseHandle(int32_t handle)
{
	TRACE();
#if BINDER_DEBUG_MSGS
	bout << "Writing release for handle " << handle << endl;
#endif
	m_out.WriteInt32(bcRELEASE);
	m_out.WriteInt32(handle);
	return B_OK;
}

status_t 
SLooper::AttemptAcquireHandle(int32_t handle)
{
	BINDER_IPC_PROFILE_STATE;
	
	TRACE();
#if BINDER_DEBUG_MSGS
	bout << "Writing attempt acquire for handle " << handle << endl;
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
	} else if (statusBuffer) {
		// Send this parcel's status through the binder.
		tr.flags |= tfStatusCode;
		*statusBuffer = data.Length();
		tr.data_size = sizeof(status_t);
		tr.data.ptr.buffer = statusBuffer;
		tr.offsets_size = 0;
		tr.data.ptr.offsets = NULL;
	} else {
		// Just return this parcel's status.
		return (m_lastError = data.ErrorCheck());
	}
	
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
	
	status_t err = _WriteTransaction(bcTRANSACTION, 0, handle, code, data);
	if (err < B_OK) {
		if (reply) reply->Reference(NULL, err);
		return err;
	}
	
	BEGIN_BINDER_CALL();
	if (reply) {
		err = _WaitForCompletion(reply);
	} else {
		SParcel fakeReply;
		err = _WaitForCompletion(&fakeReply);
	}
	FINISH_BINDER_CALL();
	
	return err;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
