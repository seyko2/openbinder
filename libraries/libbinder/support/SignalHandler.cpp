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

#include <support/KeyedVector.h>
#include <support/Locker.h>
#include <support/ConditionVariable.h>
#include <support/SignalHandler.h>
#include <support_p/SignalThreadInit.h>
#include <support/Thread.h>
#include <support/Vector.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
              
#include <signal.h>
#include <unistd.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#define NPTL 0 // 1 for code that only works properly on NPTL

#undef DEBUGSIG
//#define DEBUGSIG 1

struct chld_indirect {
	void * ucontext;
	int pid;
	int status;
	struct rusage * usage;
};



// ==================================================================================
// ==================================================================================
// ==================================================================================

class GlobalChildSignalHandler : public SChildSignalHandler
{
public:
	GlobalChildSignalHandler() 
	{
	}
	
	virtual bool OnChildSignal(int32_t sig, const siginfo_t* si, const void* ucontext, int pid, int status, const struct rusage * usage)
	{
#if BUILD_TYPE == BUILD_TYPE_DEBUG		
		if (WIFSTOPPED(status))
		{
			fprintf(stderr, "[SIGCHLD handler] ALERT: process %d has detected that child process %d was stopped by signal %d (%s)\n", getpid(), pid, WTERMSIG(status), strsignal(WSTOPSIG(status)));
		}
#ifdef WIFCONTINUED
		else if (WIFCONTINUED(status))
		{
			fprintf(stderr, "[SIGCHLD handler] ALERT: process %d has detected that child process %d was continued by SIGCONT\n", getpid(), pid, WTERMSIG(status), strsignal(WSTOPSIG(status)));
		}
#endif
		else if (WIFSIGNALED(status))
		{
			fprintf(stderr, "[SIGCHLD handler] ERROR: process %d has detected that child process %d was terminated by signal %d (%s)\n", getpid(), pid, WTERMSIG(status), strsignal(WTERMSIG(status)));
#ifdef WCOREDUMP
			if (WCOREDUMP(status))			
				fprintf(stderr, "[SIGCHLD handler] ALERT: A coredump was produced for child process %d\n", pid);
#endif
		}
		else if (WIFEXITED(status))
		{
			fprintf(stderr, "[SIGCHLD handler] process %d has detected that child process %d exited normally with exit value %d\n", getpid(), pid, WEXITSTATUS(status));
		}
		else
		{
			fprintf(stderr, "[SIGCHLD handler] process %d has detected that child process %d has some something incomprehensible\n", getpid(), pid);
		}
#endif		
		return true; // finish SIGCHLD handling with this handler
	}
};

#if NPTL

// ==================================================================================
// ==================================================================================
// ==================================================================================

class SignalHandlerThread : public SThread
{
public:
	static sptr<SignalHandlerThread> Instance()
	{
#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] Retrieving instance, g_thread=%d\n", g_thread);
#endif
		if (!g_thread)
		{
#ifdef DEBUGSIG
			fprintf(stderr, "[SignalHandler] Constructing new instance, g_thread=%d\n", g_thread);
#endif
			SignalHandlerThread * newThread = new SignalHandlerThread();
			newThread->IncStrong(&g_thread);
			g_thread = newThread;
		}

		return g_thread;
	}
	
	virtual void InitAtom (void)
	{
		SThread::InitAtom();
		
		m_blockChild = 0;
		
		// First, record the original signal handling state
		sigset_t orignalState;
		sigprocmask(SIG_SETMASK, NULL, &orignalState);

		// Next, block off asynchronous signals before we start spawning threads. To
		// properly handle any of these, we'll need a worker thread for them.
		// for now, we'll leave the interrupt signals enabled, as we need to be
		// able to tear down the process, even if we aren't tearing it down cleanly.
		sigset_t blocksigs;
		sigemptyset(&blocksigs);

		sigaddset(&blocksigs, SIGPIPE);
		sigaddset(&blocksigs, SIGALRM);
		sigaddset(&blocksigs, SIGUSR1);
		sigaddset(&blocksigs, SIGUSR2);
		sigaddset(&blocksigs, SIGCHLD);
		sigprocmask(SIG_BLOCK, &blocksigs, NULL);
		
		// Then, set up a signal mask to use during signal handling which will mask
		// off all but the synchronous signals. Yes, this makes the handlers quite
		// synchronous, but this should reduce problems with handler-unsafe code.
		
		sigset_t mask;
		
		sigfillset(&mask);
		sigdelset(&mask, SIGABRT);
		sigdelset(&mask, SIGINT);
		sigdelset(&mask, SIGTRAP);
		sigdelset(&mask, SIGSEGV);
		sigdelset(&mask, SIGBUS);
		sigdelset(&mask, SIGFPE);
		sigdelset(&mask, SIGILL);
		
		m_defaultAction.sa_mask = mask;
		m_defaultAction.sa_flags = SA_SIGINFO;
		m_defaultAction.sa_sigaction = &OnSignal;

		// Finally, actually set up our signal handler on signals we are interested
		// in.
		
		sigemptyset(&m_interestedSignals);
		//sigaddset(&m_interestedSignals, SIGUSR2);

		//sigaction(SIGUSR2, &m_defaultAction, &m_originalHandlers[SIGUSR2]);
		
		// FIXME: decide whether we should install OnSignal as handler for all
		// signals right now, or do it on demand as the code does now.
		
		m_blockCondition.Close();

#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] In InitAtom (this==%p)\n", this);
#endif
	}
	
	virtual	void RequestExit()
	{
		SLocker::Autolock lock(g_thread->m_lock);
		
		SThread::RequestExit();
		
		Kick();
		Wait(g_thread->m_lock);
		
		g_thread = NULL;
	}
	
	static void Kick()
	{
#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] kicking\n");
#endif
		g_thread->m_loopCondition.Close();
		g_thread->m_blockCondition.Open();
		//SysThreadKill(g_thread->m_thread, SIGUSR2);
	}
	
	static void Wait(SLocker & lock)
	{
#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] waiting\n");
#endif
		g_thread->m_loopCondition.Wait(lock);
	}
	

	static status_t Register(const sptr<SSignalHandler>& handler, int32_t sig)
	{
		SLocker::Autolock lock(g_thread->m_lock);

		SVector< wptr<SSignalHandler> >* vector = g_thread->m_handlers.ValueFor(sig);

		if (vector == NULL)
		{
			vector = new SVector< wptr<SSignalHandler> >();
			g_thread->m_handlers.AddItem(sig, vector);
		}
		
		ssize_t added = vector->AddItem(handler);
		
		if (added < 0)
			return added;

		if (vector->CountItems() == 1) 
		{
			sigaddset(&g_thread->m_interestedSignals, sig);
			
			sigaction(sig, &g_thread->m_defaultAction, &g_thread->m_originalHandlers[sig]);
			
			Kick();
		}
		
		Wait(g_thread->m_lock);
		
		return B_OK;
	}
	
	static status_t Unregister(const sptr<SSignalHandler>& handler, int32_t sig)
	{
		SLocker::Autolock lock(g_thread->m_lock);
		SVector< wptr<SSignalHandler> >* vector = g_thread->m_handlers.ValueFor(sig);

		if (vector != NULL)
		{
			const size_t COUNT = vector->CountItems();
			
			if (COUNT == 0)
				return B_ENTRY_NOT_FOUND;
				
			for (size_t i = 0 ; i < COUNT ; i++)
			{
				if (vector->ItemAt(i) == handler)
				{
					vector->RemoveItemsAt(i);
					
					break;
				}
			}

			if (vector->CountItems() == 0) 
			{
				sigdelset(&g_thread->m_interestedSignals, sig);
				
				sigaction(sig, &g_thread->m_originalHandlers[sig], NULL);

				Kick();
			}
			
			Wait(g_thread->m_lock);
			
		}
		return B_ENTRY_NOT_FOUND;
	}
	
	static void BlockSIGCHLD()
	{
		SLocker::Autolock lock(g_thread->m_lock);

		g_thread->m_blockChild++;
	
		if (g_thread->m_blockChild == 1)
		{
#ifdef DEBUGSIG
			fprintf(stderr, "[SignalHandler] Removing SIGCHLD from interesting signals\n");
#endif
			sigdelset(&g_thread->m_interestedSignals, SIGCHLD);

			// Only kick the paused thread if we just incremented the count
			// to block SIGCHLD. If blockChild > 1 then we have already done
			// this and there is not reason to do it again.
			Kick();
		}

		// we need to wait until the main signal loop has reblocked
		// signals, including SIGCHLD, before we allow the caller
		// to proceed.
		Wait(g_thread->m_lock);

#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] SIGCHLD is now blocked\n");
#endif
	}

	static void UnblockSIGCHLD()
	{
		SLocker::Autolock lock(g_thread->m_lock);
		
		if (g_thread->m_blockChild == 0)
		{
#ifdef DEBUGSIG
			fprintf(stderr, "[SignalHandler] Mismatched UnblockSIGCHLD\n");
#endif
			return;
		}
		
		g_thread->m_blockChild--;

		if (g_thread->m_blockChild == 0)
		{
			
#ifdef DEBUGSIG
			fprintf(stderr, "[SignalHandler] Restoring SIGCHLD to interesting signals\n");
#endif
			sigaddset(&g_thread->m_interestedSignals, SIGCHLD);

			// Kick the loop out of pause(), so that it can see the
			// new signal state
			Kick();

#ifdef DEBUGSIG
			fprintf(stderr, "[SignalHandler] SIGCHLD is now unblocked\n");
#endif
		}
	}

	static void OnSignal(int sig, siginfo_t* si, void* ucontext)
	{
		SLocker::Autolock lock(g_thread->m_lock);
		
		// FIXME: Do we have to have the lock held during the entire signal
		// processing loop? If we had a request queue system, we could pull
		// a given request out and then release the lock before handling it.
		
#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] Received signal %d (%s)\n", sig, strsignal(sig));
#endif
		
		void * orig_ucontext = ucontext;		
		SVector< wptr<SSignalHandler> >* vector = g_thread->m_handlers.ValueFor(sig);
		if (vector != NULL)
		{
			struct chld_indirect ci;
			
			do
			{
				// For SIGCHLD, retrieve death notice
				if (sig == SIGCHLD)
				{
					struct rusage usage;
					int status;
					int pid = wait4(-1, &status, WNOHANG|WUNTRACED, &usage);
					
#ifdef DEBUGSIG
					fprintf(stderr, "[SignalHandler] On SIGCHLD, waitpid()=%d\n", pid);
#endif
					// We call wait4 with WNOHANG, so it won't block if there aren't
					// any death notices
					if (pid == 0 || (pid == -1 && errno == ECHILD))
					{
						return;
					}

					if (pid == -1)
					{
						fprintf(stderr, "[SignalHandler] On SIGCHLD wait4 failed with error %d (%s)\n", errno, strerror(errno));
						return;
					}
				
					ci.ucontext = orig_ucontext;
					ci.pid = pid;
					ci.status = status;
					ci.usage = &usage;
					ucontext = &ci;
				}
		
				const size_t COUNT = vector->CountItems();
				
				for (size_t i = COUNT ; i > 0 ; i--)
				{
					sptr<SSignalHandler> handler = vector->ItemAt(i-1).promote();
				
					if (handler != NULL)
					{
#ifdef DEBUGSIG
						fprintf(stderr, "[SignalHandler] Invoking handler for signal %d\n", sig);
#endif
						if (handler->OnSignal(sig, si, ucontext))
						{
							break;
						}
					}
				}
			
				// For SIGCHLD, continue processing until there aren't any more
				// zombies or notices
			} while(sig == SIGCHLD);
		}
	}

	
protected:
	SignalHandlerThread()
	{
#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] in construction (this==%p)\n", this);
#endif
	}

	virtual ~SignalHandlerThread()
	{
#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] in destruction (this==%p)\n", this);
#endif
	}

	virtual status_t AboutToRun(SysHandle thread)
	{
#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] m_thread (%p) is set (%d) in this (%p)\n", &m_thread, thread, this);
#endif
		m_thread = thread;	
		
		return B_OK;
	}


	virtual bool ThreadEntry()
	{

		// Note the inverse loop sense of the lock -- but that's OK,
		// cause this loop will spend the majority of its time
		// in pause().
		
#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] Entering service thread\n");
#endif
		SLocker::Autolock lock(m_lock);	
		
		while (!ExitRequested())
		{
			// capture global state for this loop
			sigset_t interested;

			interested = m_interestedSignals;
			
			m_lock.Unlock();

#ifdef DEBUGSIG
			fprintf(stderr, "[SignalHandler] Unblocking signals and waiting\n");
#endif

			pthread_sigmask(SIG_UNBLOCK, &interested, NULL);

			m_blockCondition.Wait();

#ifdef DEBUGSIG
			fprintf(stderr, "[SignalHandler] Got block condition, looping\n");
#endif
			
			pthread_sigmask(SIG_BLOCK, &interested, NULL);
			
			
			m_lock.Lock();
			
			m_blockCondition.Close();

			m_loopCondition.Open();
		}		

		return false;
	}

private:

	static SignalHandlerThread * g_thread;

	SLocker m_lock;
	SConditionVariable m_loopCondition, m_blockCondition;
	SKeyedVector<int32_t, SVector< wptr<SSignalHandler> >* > m_handlers;
	int32_t m_blockChild;
	SysHandle m_thread;
	sigset_t m_interestedSignals;

	struct sigaction m_originalHandlers[_NSIG];
	struct sigaction m_defaultAction;
};

// Avoid using sptr<> for global objects in any module other than Statics.o,
// to prevent construction order problems.
SignalHandlerThread * SignalHandlerThread::g_thread;

SSignalHandler * GlobalSIGCHLD;

#else /* !NPTL */

SSignalHandler * GlobalSIGCHLD;

void SIGCHLD_handler(int sig, siginfo_t * si, void * ucontext)
{
	struct chld_indirect ci;
	void * orig_ucontext = ucontext;

#ifdef DEBUGSIG
	fprintf(stderr, "[SignalHandler] In handler, sig=%d\n", sig);
#endif
	
	
	if (sig == SIGCHLD)
	{
		struct rusage usage;
		int status;
		int pid = wait4(si->si_pid, &status, WNOHANG|WUNTRACED, &usage);
					
#ifdef DEBUGSIG
		fprintf(stderr, "[SignalHandler] On SIGCHLD, waitpid(%d)=%d\n", si->si_pid, pid);
#endif
		// We call wait4 with WNOHANG, so it won't block if there aren't
		// any death notices
		if (pid == 0 || (pid == -1 && errno == ECHILD))
		{
			return;
		}

		if (pid == -1)
		{
			fprintf(stderr, "[SignalHandler] On SIGCHLD wait4 failed with error %d (%s)\n", errno, strerror(errno));
			return;
		}
				
		ci.ucontext = orig_ucontext;
		ci.pid = pid;
		ci.status = status;
		ci.usage = &usage;
		ucontext = &ci;
	}
	
	GlobalSIGCHLD->OnSignal(sig, si, ucontext);
}

#endif


SignalHandlerStaticInit::SignalHandlerStaticInit()
{
#if NPTL
	SignalHandlerThread::Instance()->Run("signal_handler", B_NORMAL_PRIORITY, 4*1024);
#ifdef DEBUGSIG
	fprintf(stderr, "[SignalHandler] Registering global SIGCHLD handler\n");
#endif
	GlobalSIGCHLD = new GlobalChildSignalHandler();
	GlobalSIGCHLD->IncStrong(&GlobalSIGCHLD);
	SignalHandlerThread::Register(GlobalSIGCHLD, SIGCHLD);
#else

	GlobalSIGCHLD = new GlobalChildSignalHandler();
	GlobalSIGCHLD->IncStrong(&GlobalSIGCHLD);
	
	/* Install a SIGCHLD handler that will be present in all threads */
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = &SIGCHLD_handler;
	sigaction(SIGCHLD, &sa, NULL);

#ifdef DEBUGSIG
	fprintf(stderr, "[SignalHandler] Registering global SIGCHLD handler for all threads\n");
#endif
	
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
#endif
}

SignalHandlerStaticInit::~SignalHandlerStaticInit()
{
#if NPTL
	SignalHandlerThread::Instance()->RequestExit();
	GlobalSIGCHLD->DecStrong(&GlobalSIGCHLD);
#else
	GlobalSIGCHLD->DecStrong(&GlobalSIGCHLD);
#endif
}


// ==================================================================================
// ==================================================================================
// ==================================================================================

bool SChildSignalHandler::OnSignal(int32_t sig, const siginfo_t* si, const void *ucontext)
{
	struct chld_indirect * ci;
	ci = (chld_indirect *)ucontext;
		
	return OnChildSignal(sig, si, ci->ucontext, ci->pid, ci->status, ci->usage);
}

// ==================================================================================
// ==================================================================================
// ==================================================================================

SSignalHandler::SSignalHandler()
{
}

SSignalHandler::~SSignalHandler()
{
}


status_t SSignalHandler::RegisterFor(int32_t sig)
{
#if NPTL
	return SignalHandlerThread::Instance()->Register(this, sig);
#else
	return B_ERROR;
#endif
}

status_t SSignalHandler::UnregisterFor(int32_t sig)
{
#if NPTL
	return SignalHandlerThread::Instance()->Unregister(this, sig);
#else
	return B_ERROR;
#endif
}

SChildSignalHandler::SChildSignalHandler()
{
}

SChildSignalHandler::~SChildSignalHandler()
{
}

void SChildSignalHandler::BlockChildHandling()
{
#if NPTL
	SignalHandlerThread::Instance()->BlockSIGCHLD();
#endif
}

void SChildSignalHandler::UnblockChildHandling()
{
#if NPTL
	SignalHandlerThread::Instance()->UnblockSIGCHLD();
#endif
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

