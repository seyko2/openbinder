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

#ifndef _SUPPORT_LOOPER_H
#define _SUPPORT_LOOPER_H

/*!	@file support/Looper.h
	@ingroup CoreSupportBinder
	@brief The Binder's per-thread state and utilities.
*/

#include <support/Atom.h>
#include <support/ConditionVariable.h>
#include <support/SupportDefs.h>
#include <support/IBinder.h>
#include <support/Context.h>
#include <support/Parcel.h>
#if !LIBBE_BOOTSTRAP

#endif
#include <support/Process.h>
#include <support/SortedVector.h>
#include <stddef.h>

#if TARGET_HOST == TARGET_HOST_LINUX
#include <pthread.h>
#endif

#ifndef BINDER_DEBUG_LIB
#define BINDER_DEBUG_LIB 0
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif /* _SUPPORTS_NAMESPACE */

class SParcel;
class BSharedObject;
struct handle_refs;

//!	Old compatibility API.  Use SContext::UserContext() instead.
SContext get_default_context();
//!	Old compatibility API.  Use SContext::SystemContext() instead.
SContext get_system_context();

SString get_system_directory();
//@}

/*----------------------------------------------------------------*/
/*----- SLooper class --------------------------------------------*/

//!	The Binder's per-thread state and thread pool utilities.
class SLooper
{
	
	public:

		static	SLooper *				This();
		static	SLooper *				This(const sptr<BProcess>& team);
		
		static	void					SetThreadPriority(int32_t priority);
		static	int32_t					ThreadPriority();
		
		static	status_t				LastError();
		static	void					SetLastError(status_t error);
		
		static	status_t				Loop(bool isMain);
		
		//!	spawn_thread for binder debugging.
		/*!	This is just like spawn_thread(), except it takes care of propagating
			simulated team information when debugging the binder driver.
		*/
		static	SysHandle				SpawnThread(	thread_func		function_name, 
														const char 		*thread_name, 
														int32_t			priority, 
														void			*arg);
		
		static	status_t				SpawnLooper();
		
		static	int32_t					Thread();
		static	sptr<BProcess>			Process();
		static	int32_t					ProcessID();

		//!	Returns true if the current environment supports multiple processes.
		static	bool					SupportsProcesses();
		
		//!	Returns true if the user would like us to use multiple processes.
		/*!	Currently, true is returned if SupportsProcess() is true and the
			environment variable BINDER_SINGLE_PROCESS is not set. */
		static	bool					PrefersProcesses();
		
		//!	Cause the process associated with the give root object to stop.
		/*!	If 'now' is false (the default), the process will stop when all
			remote strong references on its root object are released.  If
			'now' is true, it will stop immediately.
			
			To stop the local process, pass in your own IProcess object (or
			root object if you have a different root).  If your process had
			published a root object, then the behavior is as described above.
			Otherwise, it will only be stopped if you set 'now' to true. */
		static	void					StopProcess(const sptr<IBinder>& rootObject, bool now=false);
	
		typedef status_t (*catch_root_func)(const sptr<IBinder>& node);
		static	void					CatchRootObjects(catch_root_func func);
#if TARGET_HOST == TARGET_HOST_LINUX
				sptr<IBinder>			ReceiveRootObject(pid_t process);
#endif // #if TARGET_HOST == TARGET_HOST_LINUX
				status_t				SendRootObject(const sptr<IBinder>& rootNode);

		// Special boot-strap for the current environment.
		static	void					SetContextObject(const sptr<IBinder>& object, const SString& name);
		static	sptr<IBinder>			GetContextObject(const SString& name, const sptr<IBinder>& caller);
		static	SContext				GetContext(const SString& name, const sptr<IBinder>& caller);

		static	sptr<IBinder>			GetStrongProxyForHandle(int32_t handle);
		static	wptr<IBinder>			GetWeakProxyForHandle(int32_t handle);

				status_t				Transact(	int32_t handle,
													uint32_t code, const SParcel& data,
													SParcel* reply, uint32_t flags);
		
		// Only called by the main thread during static initialization
		static	status_t				InitMain(const sptr<BProcess>& team = NULL);
		static	status_t				InitOther(const sptr<BProcess>& team = NULL);

		// For use only by IBinder proxy object
				status_t				IncrefsHandle(int32_t handle);
				status_t				AcquireHandle(int32_t handle);
				status_t				ReleaseHandle(int32_t handle);
				status_t				DecrefsHandle(int32_t handle);
				status_t				AttemptAcquireHandle(int32_t handle);
		static	status_t				ExpungeHandle(int32_t handle, IBinder* binder);

		// Special magic cleanup code for internal use.
		static	void					Stop();
		static	void					Shutdown();

				void					ExpungePackages();

#if TARGET_HOST == TARGET_HOST_LINUX
		static	bool					ProcessManagesContexts(void);
		
		typedef bool (*ContextPermissionCheckFunc)(const SString& name, const sptr<IBinder>& caller, void* userData);
		static	bool					BecomeContextManager(ContextPermissionCheckFunc checkFunc, void* userData);
#endif
#if TARGET_HOST == TARGET_HOST_WIN32 || TARGET_HOST == TARGET_HOST_LINUX
				SLooper*				GetNext();
				void					SetNext(SLooper* next);
		static	int32_t					Loopers();
#endif

	private:
	
		friend	class BProcess;
		friend	class BProcess::ComponentImage;

				// --------------------------------------------------------------------
				// The following methods have generic implementations for all platforms
				// --------------------------------------------------------------------

										SLooper(const sptr<BProcess>& team = NULL);
										~SLooper();

		static	void					_DeleteSelf(void *blooper);
		static	bool					_ResumingScheduling();
		static	void					_ClearSchedulingResumed();
		static	int32_t					_ThreadEntry(void *info);
		static	int32_t					_Loop(SLooper *parent);
				void					_SetThreadPriority(int32_t priority);

		static	void					_RegisterLooper(SLooper* who);
		static	bool					_UnregisterLooper(SLooper* who);
		static	void					_ShutdownLoopers();
		static	void					_CleanupLoopers();

		static	int32_t					TLS;

				sptr<BProcess>			m_team;
				team_id					m_teamID;

				int32_t					m_thid;
				int32_t					m_priority;
				int32_t					m_flags;
				status_t				m_lastError;
				uint32_t				m_stackBottom;
				SLooper*				m_nextRegistered;

				SSortedVector< wptr<BProcess::ComponentImage> >	m_dyingPackages;

				// --------------------------------------------------------------------
				// The following methods have platform-specific implementations.
				// --------------------------------------------------------------------

				void					_ConstructPlatform();
				void					_DestroyPlatform();
				status_t				_InitMainPlatform();

				void					_Signal();
				void					_Cleanup();

				int32_t					_LoopSelf();

				sptr<IBinder>			_GetRootObject(SysHandle id, team_id team);
				void					_CatchRootObjects(catch_root_func func);

		static	bool					_SetNextEventTime(nsecs_t when, int32_t priority);

#if TARGET_HOST == TARGET_HOST_PALMOS
				ptrdiff_t				_AvailStack(void) const;
#endif
				// --------------------------------------------------------------------
				// Special methods and state for the BeOS platform.
				// --------------------------------------------------------------------

#if TARGET_HOST == TARGET_HOST_BEOS
		static	int32_t					_DriverLoop(SLooper *parent);
#if !BINDER_DEBUG_LIB
		static	int32_t					_DriverLoopFromKernelLand(SLooper *parent);
#endif /* !BINDER_DEBUG_LIB */
		static	status_t				_BufferReply(const SParcel& buffer, void* context);
		static	void					_BufferFree(const void* data, ssize_t len, void* context);
		
				status_t				_HandleCommand(int32_t cmd);
				status_t				_WaitForCompletion(	SParcel *reply = NULL,
															status_t *acquireResult = NULL);
				status_t				_WriteTransaction(	int32_t cmd, uint32_t binderFlags,
															int32_t handle, uint32_t code,
															const SParcel& data,
															status_t* statusBuffer = NULL);
				status_t				_Reply(uint32_t flags, const SParcel& reply);
				status_t				_TransactWithDriver(bool doRead = true);

				int32_t					m_binderDesc;
				SParcel					m_in;
				SParcel					m_out;

				// --------------------------------------------------------------------
				// Special methods and state for the PalmOS platform.
				// --------------------------------------------------------------------

#elif TARGET_HOST == TARGET_HOST_PALMOS
		static	status_t				_SpawnTransactionLooper(char code);
		static	int32_t					_EnterTransactionLoop(BProcess *team);
				int32_t					_LoopTransactionSelf();
		static	int32_t					_EnterLoop(BProcess *team);
				status_t				_HandleResponse(KALBinderIPCArgsType *ioSndArgs, uint8_t *meta, SParcel *reply, bool spawnLoopers = false);
		static	sptr<IBinder>			_RetrieveContext(uint32_t which, KeyID permissionKey);
				ThreadExitCallbackID	m_exitCallback;
				KeyID					m_keys[MAX_KEY_TRANSFER];

#elif TARGET_HOST == TARGET_HOST_WIN32
				// --------------------------------------------------------------------
				// Special methods and state for the Windows platform.
				// --------------------------------------------------------------------

		static	int32_t					_EnterLoop(SLooper *parent);

				bool					m_spawnedInternally : 1;
				bool					m_rescheduling : 1;
				nsecs_t					m_idleTime;
				nsecs_t					m_lastTimeISlept;
				SLocker					m_teamLock;
				SLooper*				m_next;			// This pointer is owned by the team.
				uint32_t				m_signaled;
				SLocker					m_signalLock;

#elif TARGET_HOST == TARGET_HOST_LINUX

		static	int32_t					_EnterLoop(SLooper *parent);
		static	status_t				_BufferReply(const SParcel& buffer, void* context);
		static	void					_BufferFree(const void* data, ssize_t len, void* context);
		static	sptr<IBinder>			_RetrieveContext(const SString& name, const sptr<IBinder>& caller);
		
				status_t				_HandleCommand(int32_t cmd);
				status_t				_WaitForCompletion(	SParcel *reply = NULL,
															status_t *acquireResult = NULL);
				status_t				_WriteTransaction(	int32_t cmd, uint32_t binderFlags,
															int32_t handle, uint32_t code,
															const SParcel& data,
															status_t* statusBuffer = NULL);
				status_t				_Reply(uint32_t flags, const SParcel& reply);
				status_t				_TransactWithDriver(bool doRead = true);

				SParcel					m_in;
				SParcel					m_out;


				bool					m_spawnedInternally : 1;
				bool					m_rescheduling : 1;
				pthread_cond_t			m_condition;
				pthread_mutex_t			m_mutex;
				nsecs_t					m_idleTime;
				nsecs_t					m_lastTimeISlept;
				SLocker					m_teamLock;
				SLooper*				m_next;			// This pointer is owned by the team.
				uint32_t				m_signaled;
				SLocker					m_signalLock;

#endif
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif /* _SUPPORTS_NAMESPACE */

#endif /* _SUPPORT_LOOPER_H */
