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

#include <SysThread.h>
#include <CmnErrors.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <SysThreadConcealed.h>
#include <cxxabi.h>
#include <sys/time.h>
#include <sys/resource.h>


pthread_key_t currentThreadRecord = 0; // Process global key to thread specific storage for current thread record

namespace palmos {


// Fetch current thread record, which will cause initialization of
// primary thread record and install an exit handler.

struct libpalmroot_constructor  {
	libpalmroot_constructor() { SysCurrentThread(); }
}  global_libpalmroot_constructor; // __attribute__((init_priority(10000))); // Higher priority then normal global ctors


extern void cleanupNamedTSDKeys(void); // TSD.cpp

} // namespace palmos

struct our_thread_group_record;

const int our_thread_exit_record_MAGIC = 0x2439714;
struct our_thread_exit_record
{
	int magic; // should be our_thread_exit_record_MAGIC;
	struct our_thread_exit_record * next;
	
	void (*exit_func)(void*);
	void * exit_func_arg;
};

const int our_thread_init_record_MAGIC = 0x12934126;
struct our_thread_init_record
{
	int magic; // should be our_thread_init_record_MAGIC;
	struct our_thread_exit_record * exit_records; // same offset as next in our_thread_exit_record

	pthread_t thread;
	pid_t tid;	// The linux thread ID. There is no API to retrieve this from a pthread ID, so we must have the new thread retrieve it.
	sem_t preroll;
	sem_t startup;
	sem_t blockThread; // used for critical sections
	our_thread_init_record * nextWaitingThread; // linked list of waiting threads, for critical sections
	
	char * name;
	our_thread_group_record * group;
	void (*startup_func)(void*);
	void * startup_func_arg;
	
	void * stack_base;
};

our_thread_init_record *  SetupPrimaryThread();
void FreePrimaryThread(void *);
bool PrimaryThreadFreed = false; // false
bool PrimaryThreadSetup = false; // false

static pid_t get_our_tid()
{
	long int tid = syscall(__NR_gettid);
	if(tid < 0)
	{
		printf("syscallx%d (gettid) failed\n", __NR_gettid);
	}
	return tid;
}

our_thread_init_record *  SetupPrimaryThread()
{
	our_thread_init_record * threadRecord = (our_thread_init_record*)malloc(sizeof(our_thread_init_record)); 
	
	memset(threadRecord, '\0', sizeof(*threadRecord));
	
	threadRecord->magic = our_thread_init_record_MAGIC;
	threadRecord->exit_records = NULL;
	threadRecord->tid = get_our_tid();

	threadRecord->name = "Primary";
	threadRecord->group = NULL;
	
	sem_init(&threadRecord->blockThread, 0, 0);
	
	pthread_setspecific(currentThreadRecord, threadRecord);
	
	return threadRecord;
}

void FreePrimaryThread(void * threadP)
{
	our_thread_init_record * thread = (our_thread_init_record*)threadP;
	
	assert(thread);
	
	assert(thread->magic == our_thread_init_record_MAGIC);

	while (thread->exit_records) {
		our_thread_exit_record * e = thread->exit_records;
		
		assert(e->magic == our_thread_exit_record_MAGIC);
		
		thread->exit_records = e->next;
		
		if (e->exit_func)
			e->exit_func(e->exit_func_arg);
		
		free(e);
	}

	pthread_setspecific(currentThreadRecord, 0);
	
	//if (thread->group)
	//	group_remove(thread->group, thread);

	free(thread);
	
	currentThreadRecord = 0;
	PrimaryThreadFreed = true;
}


extern "C" status_t SysThreadInstallExitCallback(SysThreadExitCallbackFunc *iExitCallbackP,
					void *iCallbackArg,
					SysThreadExitCallbackID *oThreadExitCallbackId)
{
	our_thread_init_record * thread = (our_thread_init_record*)SysCurrentThread();
	
	assert(thread->magic == our_thread_init_record_MAGIC);

	struct our_thread_exit_record * e = (our_thread_exit_record*)malloc(sizeof(our_thread_exit_record));
	
	// FIXME: Check !e
	
	e->magic = our_thread_exit_record_MAGIC;
	
	e->exit_func = iExitCallbackP;
	e->exit_func_arg = iCallbackArg;
	
	e->next = thread->exit_records;
	thread->exit_records = e;
	
	if (oThreadExitCallbackId)
		*oThreadExitCallbackId = (SysThreadExitCallbackID)e;
	
	return 0; // FIXME: Success
}

extern "C" status_t SysThreadRemoveExitCallback(SysThreadExitCallbackID iThreadCallbackId)
{
	our_thread_init_record * thread = (our_thread_init_record*)SysCurrentThread();
	
	our_thread_exit_record * remove_ptr = (our_thread_exit_record*)iThreadCallbackId;
	
	assert(thread->magic == our_thread_init_record_MAGIC);
	assert(remove_ptr->magic == our_thread_exit_record_MAGIC);

	// cheat with linked-list removal by sharing structure layout
	assert(offsetof(our_thread_init_record, exit_records) == offsetof(our_thread_exit_record, next));
	
	our_thread_exit_record * prior_ptr = (our_thread_exit_record*)thread;
	for (our_thread_exit_record * ptr = prior_ptr->next; ptr; prior_ptr = ptr, ptr=ptr->next)
	{
		assert(ptr->magic == our_thread_exit_record_MAGIC);
		if (ptr == remove_ptr) {
			prior_ptr->next = ptr->next;
			free(ptr);
			
			break;
		}
	}
	
	return 0; // FIXME: Success
}



void group_add(struct our_thread_group_record *, struct our_thread_init_record *);
void group_remove(struct our_thread_group_record *, struct our_thread_init_record *);


void cleanup(void * arg)
{
	our_thread_init_record * thread = (our_thread_init_record*)arg;
	
	assert(thread->magic == our_thread_init_record_MAGIC);

	palmos::cleanupNamedTSDKeys();
	
	while (thread->exit_records) {
		our_thread_exit_record * e = thread->exit_records;
		
		assert(e->magic == our_thread_exit_record_MAGIC);
		
		thread->exit_records = e->next;
		
		if (e->exit_func)
			e->exit_func(e->exit_func_arg);
		
		free(e);
	}

	pthread_setspecific(currentThreadRecord, 0);
	
	sem_destroy(&thread->blockThread);
	sem_destroy(&thread->startup);
	sem_destroy(&thread->preroll);
	
	if (thread->group)
		group_remove(thread->group, thread);

	if (thread->name)
		free((void*)thread->name);
	
	free(thread);
}


void * our_thread_startup(void * arg)
{
	our_thread_init_record * thread = (our_thread_init_record*)arg;
	void *result = 0;
	
	assert(thread->magic == our_thread_init_record_MAGIC);
	
	thread->stack_base = (void*)&thread; // stack address
	
	pthread_setspecific(currentThreadRecord, arg);

	thread->tid = get_our_tid();
	sem_post(&thread->preroll);
	
	pthread_cleanup_push(cleanup, arg);

		while ((sem_wait(&thread->startup) == -1) && (errno == EINTR))
			; // Loop while sem_wait has been interrupted
		
		thread->startup_func(thread->startup_func_arg);
	
	pthread_cleanup_pop(1);
	
	return result;
}

const int group_rec_MAGIC = 0x9811232;
struct group_rec {
	int magic; // should be group_rec_MAGIC;
	group_rec * next;
	our_thread_init_record * thread;
};

const int our_thread_group_record_MAGIC = 0x12734123;
struct our_thread_group_record
{
	int magic; // should be our_thread_group_record_MAGIC;
	group_rec * threads; // must be at same offset as next in group_rec
	pthread_mutex_t mutex;
};



// precondition: thread is not already in group
void group_add(our_thread_group_record * group, our_thread_init_record * thread)
{
	assert(group->magic == our_thread_group_record_MAGIC);
	
	pthread_mutex_lock(&group->mutex);
	
	group_rec * r = (group_rec*)malloc(sizeof(group_rec));
	
	//FIXME: Check !r
	
	r->magic = group_rec_MAGIC;
	r->next = group->threads;
	r->thread = thread;
	group->threads = r;
	
	pthread_mutex_unlock(&group->mutex);
}

// precondition: thread is in group zero or one times
void group_remove(our_thread_group_record * group, our_thread_init_record * thread)
{
	assert(group->magic == our_thread_group_record_MAGIC);

	pthread_mutex_lock(&group->mutex);
	
	// cheat with linked-list removal by sharing structure layout
	assert(offsetof(our_thread_group_record, threads) == offsetof(group_rec, next));
	
	group_rec * prior_ptr = (group_rec*)group;
	for (group_rec * ptr = prior_ptr->next; ptr; prior_ptr = ptr, ptr=ptr->next)
	{
		assert(ptr->magic == group_rec_MAGIC);
		
		if (ptr->thread == thread) {
			prior_ptr->next = ptr->next;
			free(ptr);
			
			break;
		}
	}
	
	pthread_mutex_unlock(&group->mutex);
}

extern "C" SysThreadGroupHandle SysThreadGroupCreate(void)
{
	our_thread_group_record * group = (our_thread_group_record*)malloc(sizeof(our_thread_group_record));
	
	// FIXME: Check !group;
	
	group->magic = our_thread_group_record_MAGIC;
	
	group->threads = 0;
	pthread_mutex_init(&group->mutex, NULL);
	
	return (SysThreadGroupHandle)group;
}

extern "C" status_t SysThreadGroupDestroy(SysThreadGroupHandle h)
{
	SysThreadGroupWait(h);

	our_thread_group_record * group = (our_thread_group_record*)h;

	assert(group->magic == our_thread_group_record_MAGIC);

	free(group);
	
	return 0; // FIXME: Success
}

extern "C" status_t SysThreadGroupWait (SysThreadGroupHandle h)
{
	our_thread_group_record * group = (our_thread_group_record*)h;

	assert(group->magic == our_thread_group_record_MAGIC);
	
	for (;;) {
		pthread_mutex_lock(&group->mutex);
		
		group_rec * ptr = group->threads;
		
		if (ptr)
			group->threads = ptr->next;
			
		pthread_mutex_unlock(&group->mutex);
		
		if (!ptr)
			break;

		assert(ptr->magic == group_rec_MAGIC);
		
		void * result;
		pthread_join(ptr->thread->thread, &result);
	}

	return 0; // FIXME: Success
}

extern "C" status_t SysThreadCreateEZ(const char * name, 
				SysThreadEnterFunc *func, void *argument,
				SysHandle* outThread)
{
	return SysThreadCreate( sysThreadNoGroup,
				name,
				sysThreadPriorityNormal,
				sysThreadStackBasic,
				func,
				argument,
				outThread);
}
                                                                 

extern "C" status_t SysThreadCreate(
	SysThreadGroupHandle group,
	const char *name,
	uint8_t priority,
	uint32_t stackSize,
	SysThreadEnterFunc *func,
	void *argument,
        SysHandle* outThread)
{
	our_thread_init_record * threadRecord;
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, stackSize*10); // FIXME: stack*10 for safety

	threadRecord = (our_thread_init_record*)malloc(sizeof(our_thread_init_record));
	
	if (!threadRecord)
		return -1; // FIXME: Error

	memset(threadRecord, '\0', sizeof(our_thread_init_record));
	
	sem_init(&threadRecord->preroll, 0, 0);
	sem_init(&threadRecord->startup, 0, 0);
	sem_init(&threadRecord->blockThread, 0, 0);
	
	threadRecord->magic = our_thread_init_record_MAGIC;
	threadRecord->group = NULL;
	threadRecord->startup_func = func;
	threadRecord->startup_func_arg = argument;

	// FIXME: decide if we should do something else with name
	if (name)
		threadRecord->name = strdup(name);
	else
		threadRecord->name = NULL;
	
	int err = pthread_create((pthread_t*)&threadRecord->thread, &attr, our_thread_startup, threadRecord);
	
	if (err) {
		sem_destroy(&threadRecord->preroll);
		sem_destroy(&threadRecord->startup);
		sem_destroy(&threadRecord->blockThread);
		free(threadRecord);
		printf("THREAD FAILED TO BE CREATED!\n");
		return err;
	}

	if (group) {
		group_add((our_thread_group_record*)group, threadRecord);
		threadRecord->group = (our_thread_group_record*)group;
	} else {
		// this thread does not belong to a group.  put it in the detached
		// state so the memory resources used by the thread will be freed
		// automatically on exit.
		pthread_detach(threadRecord->thread);
	}	
	
	*outThread = (SysHandle)threadRecord;
	sem_wait(&threadRecord->preroll); // wait for the new thread to store its thread ID
	SysThreadChangePriority(*outThread, priority);

	return (status_t)err;
}

VAddr KALPrivGetStackPointer(void)
{
	int stack = 0;
	void * stack_ptr = &stack;
	return (VAddr)stack_ptr;
}

extern "C" status_t SysThreadStart(SysHandle thread)
{
	int value;
	our_thread_init_record * threadRecord = (our_thread_init_record*)thread;

	assert(threadRecord->magic == our_thread_init_record_MAGIC);

	sem_post(&threadRecord->startup);
	
	// FIXME: decide whether this is worth the effort
	sem_getvalue(&threadRecord->startup, &value);
	if (value > 1) {
		sem_wait(&threadRecord->startup);
		return -1; // Error, already started
	}
	
	return 0; // Success
}


extern int __dso_handle;

extern "C" SysHandle SysCurrentThread(void)
{
	if (!PrimaryThreadSetup) {
		pthread_key_create(&currentThreadRecord, NULL);
		our_thread_init_record * primaryThread = SetupPrimaryThread();
		PrimaryThreadSetup = true;
		//FIXME: register for destruction. Right now this doesn't work, as
		//secondary thread cleanup appears to happen after the primary global destructor chain
		//abi::__cxa_atexit (&FreePrimaryThread, (void*)primaryThread, &__dso_handle); 
	}
	assert(!PrimaryThreadFreed);
	
	return (SysHandle)pthread_getspecific(currentThreadRecord);
}

extern "C" void SysThreadExit(void)
{
	pthread_exit(0);
} 

extern "C" int SysThreadKill(SysHandle thread, int signo)
{
	our_thread_init_record * threadRecord = (our_thread_init_record*)thread;

	assert(threadRecord->magic == our_thread_init_record_MAGIC);
	
	return pthread_kill(threadRecord->thread, signo);
}

extern "C" pthread_t SysThreadPthread(SysHandle thread)
{
	our_thread_init_record * threadRecord = (our_thread_init_record*)thread;

	assert(threadRecord->magic == our_thread_init_record_MAGIC);
	
	return threadRecord->thread;
}

extern "C" status_t  SysThreadChangePriority(SysHandle thread, uint8_t priority)
{
	our_thread_init_record * threadRecord = (our_thread_init_record*)thread;

	assert(threadRecord->magic == our_thread_init_record_MAGIC);

	sched_param sched;
	
	// PalmOS priorities go from 0-100, with lower values indicating more favorable scheduling.
	// pthreads priorities go from 1-99, with higher values indicating more favorable scheduling.
	// 'nice' values go from -20 to 19, with lower values indicating more favorable scheduling
	// Pthread-priorities are only effective when the scheduling policy is SCHED_FIFO or SCHED_RR.
	//
	// Here's how we map Palm OS priorities to linux priorities and nice values:
	//
	// Palm OS               Linux                                  Comment
	// 100-80                nice value 19 to 0, normal thread      Low to Normal priority
	// 79-41                 nice value -1 to -20, normal thread    Normal to Realtime priority
	// 40-0                  Priority 0-20, realtime thread         Realtime and better priority
	//
	//
	// Alternatively, you can choose the following mapping, which uses only nice values:
	// Palm OS               Linux                                  Comment
	// 100-80                nice value 19 to 0, normal thread      Low to Normal priority
	// 79-0                  nice value -1 to -20, normal thread    Normal to Realtime priority

	if(priority >= 80)
	{
		// Normal to low priority
		// map 80..100 to 0..19
		int newprio = priority - 80;
		if(newprio > 19)
			newprio = 19;
		sched.sched_priority = 0;
		sched_setscheduler(threadRecord->tid, SCHED_OTHER, &sched);
		int ret = setpriority(PRIO_PROCESS,threadRecord->tid, newprio);
	}
#if 0
	else if(priority >=41)
	{
		// Normal to realtime priority
		// map 41..79 to -20..-1
		int newprio = priority-1 - 80;
		newprio /= 2;
		sched.sched_priority = 0;
		sched_setscheduler(threadRecord->tid, SCHED_OTHER, &sched);
		int ret = setpriority(PRIO_PROCESS,threadRecord->tid, newprio);
	}
	else
	{
		// Realtime priority or better
		// map 0..40 to 40..0
		int newprio = 40-priority;
		sched.sched_priority = newprio;
		sched_setscheduler(threadRecord->tid, SCHED_RR, &sched);
	}
#else
	else
	{
		// Normal priority or better
		// map 0..79 to -20..-1
		int newprio = priority-3 - 80;
		newprio /= 4;
		sched.sched_priority = 0;
		sched_setscheduler(threadRecord->tid, SCHED_OTHER, &sched);
		int ret = setpriority(PRIO_PROCESS,threadRecord->tid, newprio);
	}

#endif

	return errNone;
}


extern "C" void
SysCriticalSectionInit(SysCriticalSectionType *iCS)
{
	iCS = sysCriticalSectionInitializer;
}

extern "C" void
SysCriticalSectionDestroy(SysCriticalSectionType *iCS)
{
	(void)iCS;
}

extern "C" void
SysCriticalSectionEnter(SysCriticalSectionType *iCS)
{
	our_thread_init_record * ourselves;
	our_thread_init_record * old;
	uint32_t retry;
	uint32_t zero;
	
	SysCurrentThread(); // Make sure thread record for primary thread has been created

	ourselves = (our_thread_init_record*)pthread_getspecific(currentThreadRecord);
	
	assert(ourselves);
	assert(ourselves->magic == our_thread_init_record_MAGIC);

	do {
		old= (our_thread_init_record*)*((uint32_t volatile *)iCS);
		ourselves->nextWaitingThread = old;

		retry= SysAtomicCompareAndSwap32(
						(volatile uint32_t *)iCS,
						(uint32_t)old,
						(uint32_t)ourselves
					);
	} while(retry);

	if(old) {
		zero= 0;
		int result;
		while ((result = sem_wait(&ourselves->blockThread)) == -1 && errno == EINTR)
			;
	}
}

extern "C" void
SysCriticalSectionExit(SysCriticalSectionType *iCS)
{
	our_thread_init_record * ourselves;
	our_thread_init_record *curr;
	uint32_t * volatile *prev;
	uint32_t retry;
	
	SysCurrentThread();

	ourselves = (our_thread_init_record*)pthread_getspecific(currentThreadRecord);

	assert(ourselves);
	assert(ourselves->magic == our_thread_init_record_MAGIC);


	do {
		prev= (uint32_t * volatile *)(iCS);
		curr= (our_thread_init_record*)*prev;

		while(curr != ourselves) {
		
			assert(curr->magic == our_thread_init_record_MAGIC);
			
			prev= (uint32_t**)(&curr->nextWaitingThread);
			curr= (our_thread_init_record*)*prev;
		}

		if(prev!= (uint32_t* volatile *)iCS) {
			/*
			 * easy part, we are not the head of the queue
			 * so we can freely fix the queue without
			 * concurrency issues.
			 */
			our_thread_init_record *otherThread;

			otherThread= (our_thread_init_record*)(((uint32_t)prev)-offsetof(our_thread_init_record,nextWaitingThread));

			assert(otherThread->magic == our_thread_init_record_MAGIC);

			*prev= 0;
			sem_post(&otherThread->blockThread);
			retry= 0;
		} else {
			/*
			 * this is the tricky part, we are the only thread
			 * in the list so prev is aliased to iCS... we need
			 * to do an atomic operation since some may be trying
			 * to enter the critical section.
			 *
			 * Note that this retry business is not particulary
			 * expensive, since the inner loop took only 1
			 * iteration in the first pass of the outer one, and
			 * the second pass will involve no atomic operations
			 * at all.
			 *
			 * Note also that if the CompareAndSwap succeeds
			 * there is no thread to wake up.
			 */
			retry= SysAtomicCompareAndSwap32(
							(volatile uint32_t *)iCS,
							(uint32_t)ourselves,
							0
						);
		}
	} while(retry);
}



#define sysConditionVariableOpened 1

extern "C" void
SysConditionVariableWait(SysConditionVariableType *iCV, SysCriticalSectionType *iOptionalCS)
{
	our_thread_init_record * ourselves;
	our_thread_init_record * old;
	uint32_t retry;

	SysCurrentThread();

	ourselves = (our_thread_init_record*)pthread_getspecific(currentThreadRecord);

	assert(ourselves);
	assert(ourselves->magic == our_thread_init_record_MAGIC);

	/*
	 * If the condition is not opened, push this thread
	 * on to the list of those waiting.
	 */
	do {
		old= (our_thread_init_record*)*((uint32_t volatile *)iCV);

		if ((uint32_t)old== sysConditionVariableOpened) {
			break;
		}

		ourselves->nextWaitingThread = old;

		retry= SysAtomicCompareAndSwap32(
						(volatile uint32_t *)iCV,
						(uint32_t)old,
						(uint32_t)ourselves
					);
	} while(retry);

	/*
	 * Now wait for the condition to be opened if it
	 * previously wasn't.
	 */
	if((uint32_t)old!= sysConditionVariableOpened) {
		if(iOptionalCS) {
			SysCriticalSectionExit(iOptionalCS);
		}

		int result;
		while ((result = sem_wait(&ourselves->blockThread)) == -1 && errno == EINTR)
			;

		if (iOptionalCS) {
			SysCriticalSectionEnter(iOptionalCS);
		}
	}
}

static void
wake_up_condition(our_thread_init_record *curr)
{
	our_thread_init_record *next;

	/*
	 * Iterate through the list, clearing it and waking
	 * up its threads.
	 */
	while(curr != sysConditionVariableInitializer) {

		assert(curr->magic == our_thread_init_record_MAGIC);

		next = curr->nextWaitingThread;
		curr->nextWaitingThread = (our_thread_init_record*)sysConditionVariableInitializer;
		sem_post(&curr->blockThread);
		curr = next;
	}
}

extern "C" void
SysConditionVariableOpen(SysConditionVariableType *iCV)
{
	our_thread_init_record *curr;
	uint32_t retry;

	/*
	 * Atomically open the condition and retrieve the list of
	 * waiting threads.
	 */
	do {
		curr= (our_thread_init_record*)*(uint32_t * volatile *)(iCV);
		if ((uint32_t)curr == sysConditionVariableOpened) {
			curr = (our_thread_init_record*)sysConditionVariableInitializer;
			break;
		}

		retry= SysAtomicCompareAndSwap32(
						(volatile uint32_t *)iCV,
						(uint32_t)curr,
						(uint32_t)sysConditionVariableOpened
					);
	} while(retry);

	wake_up_condition(curr);
}

extern "C" void
SysConditionVariableClose(SysConditionVariableType *iCV)
{
	uint32_t curr;
	uint32_t retry;

	/*
	 * Atomically transition the condition variable
	 * from opened to closed.  If it is not currently
	 * opened, leave it as-is.
	 */
	do {
		curr= *(volatile uint32_t *)iCV;
		if (curr != sysConditionVariableOpened) {
			break;
		}

		retry= SysAtomicCompareAndSwap32(
						(volatile uint32_t *)iCV,
						curr,
						(uint32_t)sysConditionVariableInitializer
					);
	} while(retry);
}

extern "C" void
SysConditionVariableBroadcast(SysConditionVariableType *iCV)
{
	uint32_t *curr;
	uint32_t retry;

	/*
	 * Atomically retrieve and clear the list of
	 * waiting threads.
	 */
	do {
		curr= *(uint32_t * volatile *)(iCV);

		retry= SysAtomicCompareAndSwap32(
						(volatile uint32_t *)iCV,
						(uint32_t)curr,
						(uint32_t)sysConditionVariableInitializer
					);
	} while(retry);

	if(curr!= (uint32_t *)sysConditionVariableOpened) {
		wake_up_condition((our_thread_init_record*)curr);
	}
}

#if 0
/*
 * Once flag are a mix up between barriers and critical
 * sections. We could do without them except that for
 * ARMs EABI we only have four bytes to play with, hence
 * we need a new primitive type.
 *
 * OnceFlags should be initialized with sysOnceFlagInitializer.
 *
 * A thread is allowed to test the flag for equality to
 * sysOnceFlagDone instead of calling directly into
 * SysOnceFlagTest().
 *
 * SysOnceFlagTest() returns either sysOnceFlagDone,
 * or sysOnceFlagPending. If a thread receives a
 * return of sysOnceFlagPending it is guaranteed to
 * have exclusive access to the object controlled
 * by the flag, and should call SysOnceFlagSignal().
 *
 * The argument to SysOnceFlagSignal() indicates if
 * the initialization succeeded or failed and
 * should be retried at a later time.
 */
uint32_t
SysOnceFlagTest(SysOnceFlagType *iOF)
{
	uint32_t *tsdBase;
	uint32_t old;
	uint32_t zero;

	tsdBase= (uint32_t*)&tsdStore[0];
	zero   = 0;

	do {
		/*
		 * If the condition is not opened, push this thread
		 * on to the list of those waiting.
		 */
		do {
			old= *iOF;

			if(old== sysOnceFlagDone) {
				/*
				 * some did it while we were trying to queue ourselves
				 */
				tsdBase[kKALTSDSlotIDCriticalSection]= sysOnceFlagInitializer;
				return sysOnceFlagDone;
			}

			tsdBase[kKALTSDSlotIDCriticalSection]= old;

		} while(KALAtomicCompareAndSwap32(iOF, old, (uint32_t)tsdBase));

		/*
		 * Now wait for the flag to be signaled if we are not the
		 * first in the queue.
		 */
		if(old!= sysOnceFlagPending) {
			KALCurrentThreadTimedSleep((uint64_t)(zero), B_WAIT_FOREVER);
		}
	} while(old!= sysOnceFlagPending);

	return sysOnceFlagPending;
}

uint32_t
SysOnceFlagSignal(SysOnceFlagType *iOF, uint32_t result)
{
	uint32_t *curr;

#if BUILD_TYPE== BUILD_TYPE_DEBUG
	if(*iOF<= sysOnceFlagDone) {
		/* error here */
	}
	if(result> sysOnceFlagDone) {
		/* error here */
	}
#endif


	/*
	 * Atomically remove all thread in the queue
	 */
	do {
		curr= *(uint32_t**)iOF;
	} while(KALAtomicCompareAndSwap32(iOF, (uint32_t)curr, result));


	/*
	 * Iterate through the list, clearing it and waking
	 * up its threads. In the case the result is sysOnceFlagPending
	 * we can create a herd... but contention is expected to be low
	 * and creating a herd can be beneficious if one of the threads
	 * is a high priority thread (the herd as a side effect reduces
	 * priority inversions.)
	 */
	while(curr) {
		uint32_t *next;

		next= (uint32_t*)(curr[kKALTSDSlotIDCriticalSection]);

#if BUILD_TYPE== BUILD_TYPE_DEBUG
		if(!next) {
			if(curr!= (uint32_t*)tsdStore) {
				/* error here */
			}
		}
#endif

		curr[kKALTSDSlotIDCriticalSection]= sysOnceFlagInitializer;
		KALThreadWakeup(curr[kKALTSDSlotIDCurrentThreadKeyID]);

		curr= next;
	}

	return errNone;
}

#endif

extern sysThreadDirectFuncs g_threadDirectFuncs;
status_t SysThreadGetDirectFuncs(sysThreadDirectFuncs* ioFuncs)
{
	*ioFuncs = g_threadDirectFuncs;
	return 0;
}
