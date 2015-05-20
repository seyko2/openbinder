/* binder driver
 * Copyright (C) 2005 Palmsource, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef BINDER_THREAD_H
#define BINDER_THREAD_H

#include "binder_defs.h"
#include <linux/wait.h>

typedef struct binder_thread {
	/*	These are protected by binder.c's global lock. */
	struct hlist_node	node;
	bool				attachedToThread;		/* Expecting a BINDER_THREAD_EXIT. */

	/*	These are managed by binder_proc_t.  Nothing else should
		touch them. */
	struct binder_thread *		next;			/* List of all threads */
	struct list_head			waitStackEntry;
	struct binder_thread *		pendingChild;	/* Child for bcREQUEST_ROOT_OBJECT */
	struct binder_transaction *	nextRequest;	/* Return request to waiting thread */
	bool idle;
	
	/*	Stupid hack. */
	int					returnedEventPriority;
	
	pid_t				virtualThid; /* The thid for the transaction thread group */
	atomic_t			m_primaryRefs;
	atomic_t			m_secondaryRefs;
	status_t			m_err;
	pid_t				m_thid;
	wait_queue_head_t   m_wait;
	atomic_t            m_wake_count;
	int					m_waitForReply;
	int					m_consume;
	
	struct semaphore	m_lock;
	struct binder_proc	*	m_team;	// the team we belong to
	struct binder_transaction *	m_reply;
	struct binder_transaction *	m_pendingReply;
	struct binder_transaction *	m_pendingRefResolution;
	
	/*	This is the number of primary references on our team
		that must be removed when we continue looping.  It is
		used to keep the team around while processing final
		brRELEASE and brDECREFS commands on objects inside it. */
	int						m_teamRefs;
	
	/*	Did the driver spawn this thread? */
	bool					m_isSpawned : 1;
	
	/*	Is this thread running as a looper? */
	bool					m_isLooping : 1;

	/*  For driver spawned threads: first time looping? */
	bool					m_firstLoop : 1;

	/*	Set if thread has determined an immediate reply for a
		bcATTEMPT_ACQUIRE.  In this case, 'short' is true and
		'result' is whether it succeeded. */
	bool					m_shortAttemptAcquire : 1;
	bool					m_resultAttemptAcquire : 1;
	
	/*	Set if this thread structure has been initialized to
		reply with a root object to its parent thread. */
	bool					m_pendingReplyIsRoot : 1;
	
	/*!	Set if this thread had an error when trying to
		receive a child's root reply, to return the result
		at the next Read(). */
	bool					m_failedRootReceive : 1;
	
	/*	Set if this thread tried to send a root object, but
		timed out. */
	bool					m_failedRootReply : 1;
} binder_thread_t;

int						binder_thread_GlobalCount(void);

binder_thread_t *		binder_thread_init(pid_t thid, struct binder_proc *team);
void					binder_thread_destroy(binder_thread_t *that);

void					binder_thread_Released(binder_thread_t *that);

void					binder_thread_Die(binder_thread_t *that);

BND_DECLARE_ACQUIRE_RELEASE(binder_thread);
BND_DECLARE_ATTEMPT_ACQUIRE(binder_thread);

/*	Attach parent thread to this thread.  The child is set up as if it had
	received a transaction, and the first thing it should do is send a reply
	that will go back to the parent.  This is for bcRETRIEVE_ROOT_OBJECT. */
bool					binder_thread_SetParentThread(binder_thread_t *that, binder_thread_t *replyTo);

/*	Clear the pendingChild field when we have received the reply. */
void					binder_thread_ReleasePendingChild(binder_thread_t *that);

/*	When binder_thread_SetParentThread() is used to wait for the child thread
	to send its root object, we can create a binder_thread structure that is
	not attached to a binder_proc.  This function is called when the child
	thread finally gets into the driver, to get its pre-created thread
	structure attached to its new process structure. */
void					binder_thread_AttachProcess(binder_thread_t *that, struct binder_proc *team);

/*	Calls from binder_proc_t to block until new requests arrive */
status_t				binder_thread_Snooze(binder_thread_t *that, bigtime_t wakeupTime);
status_t				binder_thread_AcquireIOSem(binder_thread_t *that);
void					binder_thread_Wakeup(binder_thread_t *that);

/*	Returning transactions -- reflections and the final reply */
void					binder_thread_Reply(binder_thread_t *that, struct binder_transaction *t);
void					binder_thread_Reflect(binder_thread_t *that, struct binder_transaction *t);

/*	Reply that the target is no longer with us. */
void					binder_thread_ReplyDead(binder_thread_t *that);

bool					binder_thread_AttemptExecution(binder_thread_t *that, struct binder_transaction *t);
void					binder_thread_FinishAsync(binder_thread_t *that, struct binder_transaction *t);
void					binder_thread_Sync(binder_thread_t *that);

#define	binder_thread_Thid(that) ((that)->m_thid)
#define	binder_thread_Team(that) ((that)->m_team)

#define	binder_thread_VirtualThid(that) ((that)->virtualThid)

#define	binder_thread_PrimaryRefCount(that) atomic_read(&(that)->m_primaryRefs)
#define	binder_thread_SecondaryRefCount(that) atomic_read(&(that)->m_secondaryRefs)

int					binder_thread_Control(binder_thread_t *that, unsigned int cmd, void *buffer);
int					binder_thread_Write(binder_thread_t *that, void *buffer, int size, signed long *consumed);
int					binder_thread_Read(binder_thread_t *that, void *buffer, int size, signed long *consumed);

#define binder_thread_Reflect(that, t) binder_thread_Reply(that, t)

#endif // BINDER_THREAD_H
