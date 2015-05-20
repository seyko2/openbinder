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

#include "binder_defs.h"
#include "binder_thread.h"
#include "binder_proc.h"
#include "binder_node.h"
#include "binder_transaction.h"
#include "iobuffer.h"

#include <stdarg.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>

static void		binder_thread_Cleanup(binder_thread_t *that);

static status_t	binder_thread_WaitForReply(binder_thread_t *that, iobuffer_t *io);
static status_t	binder_thread_WaitForRequest(binder_thread_t *that, iobuffer_t *io);
static status_t	binder_thread_ReturnTransaction(binder_thread_t *that, iobuffer_t *io, binder_transaction_t *t);

// static void		binder_thread_WriteReturn(binder_thread_t *that, void *buffer, int size);

// static void		binder_thread_EnqueueTransaction(binder_thread_t *that, binder_transaction_t *t);

// Set non-zero to do the capable(CAP_SYS_ADMIN) check
#define CHECK_CAPS 0

static binder_node_t *gContextManagerNode = NULL;
static DECLARE_MUTEX(gContextManagerNodeLock);
static atomic_t g_count = ATOMIC_INIT(0);

int
binder_thread_GlobalCount()
{
	return atomic_read(&g_count);
}

binder_thread_t * binder_thread_init(int thid, binder_proc_t *team)
{
	binder_thread_t *that;

	that = (binder_thread_t*)kmem_cache_alloc(thread_cache, GFP_KERNEL);
	if (that) {
		atomic_inc(&g_count);
		that->attachedToThread = FALSE;
		that->next = NULL;
		INIT_LIST_HEAD(&that->waitStackEntry);
		that->pendingChild = NULL;
		that->nextRequest = NULL;
		that->idle = FALSE;
		that->virtualThid = 0;
		atomic_set(&that->m_primaryRefs, 0);
		atomic_set(&that->m_secondaryRefs, 0);
		atomic_set(&that->m_wake_count, 0);
		that->m_err = 0;
		init_MUTEX(&that->m_lock);
		init_waitqueue_head(&that->m_wait);
		that->m_waitForReply = 0;
		that->m_reply = NULL;
		that->m_consume = 0;
		that->m_thid = thid;
		that->m_team = team;
		if (team != NULL)
			BND_ACQUIRE(binder_proc, that->m_team, WEAK, that);
		that->m_pendingReply = NULL;
		that->m_pendingRefResolution = NULL;
		that->m_teamRefs = 0;
		that->m_isSpawned = FALSE;
		that->m_isLooping = FALSE;
		that->m_firstLoop = TRUE;
		that->m_shortAttemptAcquire = FALSE;
		that->m_pendingReplyIsRoot = FALSE;
		that->m_failedRootReceive = FALSE;
		that->m_failedRootReply = FALSE;
		DPRINTF(5, (KERN_WARNING "*** CREATING THREAD %p (%p:%d)\n", that, that->m_team, that->m_thid));
	}
	DBSHUTDOWN((KERN_WARNING "%s(%u, %p): %p\n", __func__, thid, team, that));
	return that;
}

void binder_thread_destroy(binder_thread_t *that)
{
	DBSHUTDOWN((KERN_WARNING "binder_thread_destroy(%p, %p):%d\n", that, that->m_team, that->m_thid));
	if (that->m_isLooping && that->m_team && BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
		binder_proc_FinishLooper(that->m_team, that->m_isSpawned);
		BND_RELEASE(binder_proc, that->m_team, STRONG, that);
	}
	// We don't care about process, here.
	//if (find_thread(that->m_thid, 0, TRUE) != that) {
		//DPRINTF(1, (KERN_WARNING "binder_thread_destroy(%p): couldn't find ourselves in the thread hash\n", that));
	//}
	
	binder_thread_Cleanup(that);
	
	if (that->m_team) {
		BND_RELEASE(binder_proc, that->m_team, WEAK, that);
		that->m_team = NULL;
	}
	
	atomic_dec(&g_count);
	
	// free_lock(&that->m_lock);
	kmem_cache_free(thread_cache, that);
}

void
binder_thread_Released(binder_thread_t *that)
{
	DBSHUTDOWN((KERN_WARNING "%s(%p, %p):%d\n", __func__, that, that->m_team, that->m_thid));
	binder_thread_Die(that);
}

void
binder_thread_Die(binder_thread_t *that)
{
	DBSHUTDOWN((KERN_WARNING "%s(%p) (%p:%d) in %d\n", __func__, that, that->m_team, binder_thread_Thid(that), current->pid));

	// Always do this, even if all primary references on the team
	// are gone.  This is the only way the thread list gets cleaned up.
	if (that->m_team != NULL)
		binder_proc_RemoveThread(that->m_team, that);
	
	binder_thread_Cleanup(that);

	/*
	 * Linux doesn't seem to have an equivalent to delet_sem()
	 * delete_sem(that->m_ioSem); that->m_ioSem = B_BAD_SEM_ID;
	 */
	
	DBSHUTDOWN((KERN_WARNING "Binder thread %p:%d: DEAD!\n", that->m_team, that->m_thid));
}

bool binder_thread_SetParentThread(binder_thread_t *that, binder_thread_t *replyTo)
{
	bool success;
	
	DPRINTF(4, (KERN_WARNING "binder_thread_SetParentThread(%p, %p)\n", that, replyTo));
	
	BND_LOCK(that->m_lock);
	if ((success = !that->m_failedRootReply)) {
	
		BND_ASSERT(!that->m_pendingReply, "Attaching to child thread that already has someone waiting for a reply!");
		that->m_pendingReply = binder_transaction_CreateEmpty();
		binder_transaction_SetRootObject(that->m_pendingReply, TRUE);
		that->m_pendingReply->sender = replyTo;
		that->m_pendingReplyIsRoot = TRUE;
		BND_ACQUIRE(binder_thread, replyTo, WEAK, m_pendingReply);
	
		// The thread now has the reply info, so allow it to wake up and reply.
		binder_thread_Wakeup(that);
	}
	BND_UNLOCK(that->m_lock);
	
	return success;
}

void binder_thread_ReleasePendingChild(binder_thread_t *that)
{
	binder_thread_t *child;
	BND_LOCK(that->m_lock);
	DPRINTF(4, (KERN_WARNING "binder_thread_ReleasePendingChild(%p): child=%p\n", that, that->pendingChild));
	child = that->pendingChild;
	that->pendingChild = NULL;
	BND_UNLOCK(that->m_lock);
	
	if (child) {
		forget_thread(child);
	}
}

void binder_thread_AttachProcess(binder_thread_t *that, struct binder_proc *team)
{
	bool attached = FALSE;
	
	DPRINTF(4, (KERN_WARNING "binder_thread_AttachProcess(%p, %p)\n", that, team));
	
	BND_LOCK(that->m_lock);
	
	BND_ASSERT(!that->m_team, "Child thread is already attached to its process!");
	if (that->m_team == NULL) {
		attached = TRUE;
		that->m_team = team;
		BND_ACQUIRE(binder_proc, team, WEAK, that);
	}
	
	BND_UNLOCK(that->m_lock);
	
	if (attached) {
		if(!binder_proc_AddThread(team, that)) {
			BND_ASSERT(0, "attached thread to dying process");
		}
	}
}

void
binder_thread_Cleanup(binder_thread_t *that)
{
	binder_transaction_t *cmd, *pendingRef;
	binder_transaction_t *pendingReply;
	binder_transaction_t *reply;
	binder_node_t *contextManagerNode;
	int relCount;
	bool first;

	BND_LOCK(that->m_lock);
	pendingRef = that->m_pendingRefResolution;
	that->m_pendingRefResolution = NULL;
	pendingReply = that->m_pendingReply;
	that->m_pendingReply = NULL;
	reply = that->m_reply;
	that->m_reply = NULL;
	relCount = that->m_teamRefs;
	that->m_teamRefs = 0;
	DPRINTF(0, (KERN_WARNING "%s(%p):%p,%d strong: %d, weak: %d\n", __func__, that, that->m_team, that->m_thid, that->m_primaryRefs.counter, that->m_secondaryRefs.counter));
	BND_UNLOCK(that->m_lock);
	
	while (relCount) {
		if (that->m_team)
			BND_RELEASE(binder_proc, that->m_team, STRONG, that);
		relCount--;
	}
	
	first = TRUE;
	while ((cmd = pendingRef)) {
		if (first) {
			first = FALSE;
			DPRINTF(5, (KERN_WARNING "Binder thread %p:%d: cleaning up pending ref resolution.\n", that->m_team, that->m_thid));
		}
		pendingRef = cmd->next;
		DPRINTF(5, (KERN_WARNING "Deleting transaction %p\n", cmd));
		binder_transaction_DestroyNoRefs(cmd);
	}

	first = TRUE;
	while ((cmd = pendingReply)) {
		if (first) {
			first = FALSE;
			DPRINTF(5, (KERN_WARNING "Binder thread %p:%d: cleaning up pending replies.\n", that->m_team, that->m_thid));
		}
		if (cmd->sender) {
			DPRINTF(5, (KERN_WARNING "Returning transaction %p to thread %p (%d)\n",
						cmd, cmd->sender, binder_thread_Thid(cmd->sender)));
			binder_thread_ReplyDead(cmd->sender);
		}
		pendingReply = cmd->next;
		binder_transaction_Destroy(cmd);
	}

	first = TRUE;
	while ((cmd = reply)) {
		if (first) {
			first = FALSE;
			DPRINTF(5, (KERN_WARNING "Binder thread %p:%d: cleaning up received replies.\n", that->m_team, that->m_thid));
		}
		reply = cmd->next;
		DPRINTF(5, (KERN_WARNING "Deleting transaction %p\n", cmd));
		binder_transaction_Destroy(cmd);
	}
	BND_LOCK(gContextManagerNodeLock);
	if (gContextManagerNode && (gContextManagerNode->m_team == that->m_team && that->m_team->m_threads == NULL)) {
		contextManagerNode = gContextManagerNode;
		gContextManagerNode = NULL;
	}
	else {
		contextManagerNode = NULL;
	}
	BND_UNLOCK(gContextManagerNodeLock);
	if(contextManagerNode != NULL) {
		DPRINTF(2, (KERN_WARNING "team %08lx is not longer the context manager\n", (unsigned long)that->m_team));
		binder_node_destroy(contextManagerNode);
	}

	binder_thread_ReleasePendingChild(that);
	
	// Make sure this thread returns to user space.
	binder_thread_Wakeup(that);
}

int 
binder_thread_Control(binder_thread_t *that, unsigned int cmd, void *buffer)
{
	int result = -EINVAL;
	unsigned int size = _IOC_SIZE(cmd);
	
	//ddprintf("binder -- ioctl %d, size=%d\n", cmd, size);
	
	DPRINTF(2, (KERN_WARNING "%s(%p, %d, %p): proc=%p\n", __func__, that, cmd, buffer, that->m_team));
	
	switch (cmd) {
		case BINDER_WRITE_READ:
			DPRINTF(2, (KERN_WARNING "BINDER_WRITE_READ: %p:%d\n", that->m_team, that->m_thid));
			if (size >= sizeof(binder_write_read_t)) {
				binder_write_read_t bwr;
				if (copy_from_user(&bwr, buffer, sizeof(bwr)) == 0) {
					DPRINTF(2, (KERN_WARNING " -- write %ld at %08lx\n -- read %ld at %08lx\n", bwr.write_size, bwr.write_buffer, bwr.read_size, bwr.read_buffer));
					if (bwr.write_size > 0) {
						result = binder_thread_Write(that, (void *)bwr.write_buffer, bwr.write_size, &bwr.write_consumed);
						if (result < 0) {
							bwr.read_consumed = 0;
							copy_to_user(buffer, &bwr, sizeof(bwr));
							goto getout;
						}
					}
					if (bwr.read_size > 0) {
						result = binder_thread_Read(that, (void *)bwr.read_buffer, bwr.read_size, &bwr.read_consumed);
						if (result < 0) {
							// For ERESTARTSYS, we have to propagate the fact
							// that we've already done any writes.
							//if (result != -ERESTARTSYS) {
								//bwr.read_size = result; // FIXME?
							//}
							copy_to_user(buffer, &bwr, sizeof(bwr));
							goto getout;
						}
					}
					copy_to_user(buffer, &bwr, sizeof(bwr));
					result = 0;
				}
			}
			break;
		case BINDER_SET_WAKEUP_TIME:
			if (size >= sizeof(binder_wakeup_time_t) && BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
				binder_wakeup_time_t *time = (binder_wakeup_time_t*)buffer;
				result = binder_proc_SetWakeupTime(that->m_team, time->time, time->priority);
				BND_RELEASE(binder_proc, that->m_team, STRONG, that);
			}
			break;
		case BINDER_SET_IDLE_TIMEOUT:
			if (size >= 8 && BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
				result = binder_proc_SetIdleTimeout(that->m_team, *((bigtime_t*)buffer));
				BND_RELEASE(binder_proc, that->m_team, STRONG, that);
			}
			break;
		case BINDER_SET_REPLY_TIMEOUT:
			if (size >= 8 && BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
				result = binder_proc_SetReplyTimeout(that->m_team, *((bigtime_t*)buffer));
				BND_RELEASE(binder_proc, that->m_team, STRONG, that);
			}
			break;
		case BINDER_SET_MAX_THREADS:
			if (size >= 4 && BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
				result = binder_proc_SetMaxThreads(that->m_team, *((int*)buffer));
				BND_RELEASE(binder_proc, that->m_team, STRONG, that);
			}
			break;
		case BINDER_SET_IDLE_PRIORITY:
			if (size >= 4 && BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
				result = binder_proc_SetIdlePriority(that->m_team, *((int*)buffer));
				BND_RELEASE(binder_proc, that->m_team, STRONG, that);
			}
			break;
		case BINDER_SET_CONTEXT_MGR:
			if (size >= 4 && BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
				DPRINTF(2, (KERN_WARNING "bcSET_CONTEXT_MANAGER attempt by %p\n", that->m_team));
				// LOCK
				// check for existing context
				BND_LOCK(gContextManagerNodeLock);
				if (!gContextManagerNode) {
					// check for administration rights
#if CHECK_CAPS
					if (capable(CAP_SYS_ADMIN)) {
#endif
						gContextManagerNode = binder_node_init(that->m_team, NULL, NULL);
						BND_FIRST_ACQUIRE(binder_node, gContextManagerNode, STRONG, that->m_team);
						DPRINTF(2, (KERN_WARNING "making team %08lx context manager\n", (unsigned long)that->m_team));
						result = 0;
#if CHECK_CAPS
					} else {
						DPRINTF(2, (KERN_WARNING "%p doesn't have CAP_SYS_ADMIN rights\n", that->m_team));
					}
#endif
				} else {
					DPRINTF(2, (KERN_WARNING "gContextManagerNode already set to %p by %08lx", gContextManagerNode, (unsigned long)that->m_team));
				}
				BND_UNLOCK(gContextManagerNodeLock);
				// UNLOCK
				BND_RELEASE(binder_proc, that->m_team, STRONG, that);
			}
			break;
		case BINDER_THREAD_EXIT:
			BND_RELEASE(binder_thread, that, STRONG, 0);
			result = 0;
			break;
		case BINDER_VERSION:
			if (size >= sizeof(binder_version_t)) {
				binder_version_t *vers = (binder_version_t*)buffer;
				vers->protocol_version = BINDER_CURRENT_PROTOCOL_VERSION;
				result = 0;
			}
			break;
		default:
			break;
	}
	
getout:
	DPRINTF(2, (KERN_WARNING "%s(%p, %d, %p): proc=%p: result=%d\n", __func__, that, cmd, buffer, that->m_team, -result));
	
	return result;
}

int 
binder_thread_Write(binder_thread_t *that, void *_buffer, int _size, signed long *consumed)
{
	int result, cmd, target;
	binder_node_t *n;
	iobuffer_t io;

	DPRINTF(2, (KERN_WARNING "binder_thread_Write(%p, %d)\n", _buffer, _size));
	if (that->m_err) return that->m_err;
	if (!binder_proc_IsAlive(that->m_team)) return -ECONNREFUSED;
	result = iobuffer_init(&io, (unsigned long)_buffer, _size, *consumed);
	if (result) return result;
	
	while (1) {
		if (that->m_consume) {
			that->m_consume -= iobuffer_drain(&io, that->m_consume);
			iobuffer_mark_consumed(&io);
		}
		target = -1;
		if (iobuffer_read_u32(&io, &cmd)) goto finished;
		DPRINTF(5, (KERN_WARNING "cmd: %d\n",cmd));
		switch (cmd) {
			case bcINCREFS: {
				if (iobuffer_read_u32(&io, &target)) goto finished;
				DBREFS((KERN_WARNING "bcINCREFS of %d\n", target));
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					binder_proc_RefDescriptor(that->m_team, target, WEAK);
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcINCREFS_DONE: {
				void *ptr;
				void *cookie;
				if (iobuffer_read_void(&io, &ptr)) goto finished;
				if (iobuffer_read_void(&io, &cookie)) goto finished;
				DBREFS((KERN_WARNING "bcINCREFS_DONE of %p\n", ptr));
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					if (binder_proc_Ptr2Node(that->m_team, ptr, cookie, &n, NULL, that, WEAK) == 0) {
						BND_RELEASE(binder_node, n, WEAK, that->m_team);
						BND_RELEASE(binder_node, n, WEAK, that->m_team);
					}
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcACQUIRE: {
				if (iobuffer_read_u32(&io, &target)) goto finished;
				DBREFS((KERN_WARNING "bcACQUIRE of %d\n", target));
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					binder_proc_RefDescriptor(that->m_team, target, STRONG);
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcACQUIRE_DONE: {
				void *ptr;
				void *cookie;
				if (iobuffer_read_void(&io, &ptr)) goto finished;
				if (iobuffer_read_void(&io, &cookie)) goto finished;
				DBREFS((KERN_WARNING "bcACQUIRE_DONE of %p\n", ptr));
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					if (binder_proc_Ptr2Node(that->m_team, ptr, cookie, &n, NULL, that, STRONG) == 0) {
						BND_RELEASE(binder_node, n, STRONG, that->m_team);
						BND_RELEASE(binder_node, n, STRONG, that->m_team);
					}
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcATTEMPT_ACQUIRE: {
				int priority;
				if (iobuffer_read_u32(&io, &priority)) goto finished;
				if (iobuffer_read_u32(&io, &target)) goto finished;
				DBREFS((KERN_WARNING "bcATTEMPT_ACQUIRE of %d\n", target));
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					binder_node_t *node;
					if (binder_proc_AttemptRefDescriptor(that->m_team, target, &node)) {
						DBREFS((KERN_WARNING "Immediate Success!\n"));
						BND_ASSERT(!that->m_shortAttemptAcquire, "Already have AttemptAcquire result! (now succeeding)");
						that->m_shortAttemptAcquire = TRUE;
						that->m_resultAttemptAcquire = TRUE;
					} else if (node) {
						binder_transaction_t *t;
						// Need to wait for a synchronous acquire attempt
						// on the remote node.  Note that the transaction has
						// special code to understand a tfAttemptAcquire, taking
						// ownership of the secondary reference on 'node'.
						DBREFS((KERN_WARNING "Sending off to owner!\n"));
						t = binder_transaction_CreateRef(tfAttemptAcquire, binder_node_Ptr(node), binder_node_Cookie(node), that->m_team);
						binder_transaction_SetPriority(t, (s16)priority);
						t->target = node;
						binder_transaction_SetInline(t, TRUE);
						BND_LOCK(that->m_lock);
						t->next = that->m_pendingRefResolution;
						that->m_pendingRefResolution = t;
						BND_UNLOCK(that->m_lock);
					} else {
						DBREFS((KERN_WARNING "Immediate Failure!\n"));
						BND_ASSERT(!that->m_shortAttemptAcquire, "Already have AttemptAcquire result! (now failing)");
						that->m_shortAttemptAcquire = TRUE;
						that->m_resultAttemptAcquire = FALSE;
					}
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				} else {
					DBREFS((KERN_WARNING "Team Failure!\n"));
					BND_ASSERT(!that->m_shortAttemptAcquire, "Already have AttemptAcquire result! (now team failing)");
					that->m_shortAttemptAcquire = TRUE;
					that->m_resultAttemptAcquire = FALSE;
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcACQUIRE_RESULT: {
				int result;
				binder_transaction_t *t;
				if (iobuffer_read_u32(&io, &result)) goto finished;
				iobuffer_mark_consumed(&io);
				DBREFS((KERN_WARNING "bcACQUIRE_RESULT: %d\n",result));
				t = binder_transaction_Create(0, 0, 0, 0, NULL);
				binder_transaction_SetAcquireReply(t, TRUE);
				binder_transaction_SetInline(t, TRUE);
				*(int *)t->data = result;
				BND_LOCK(that->m_lock);
				t->next = that->m_pendingRefResolution;
				that->m_pendingRefResolution = t;
				BND_UNLOCK(that->m_lock);
			} break;
			case bcRELEASE: {
				if (iobuffer_read_u32(&io, &target)) goto finished;
				DBREFS((KERN_WARNING "bcRELEASE of %d\n", target));
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					binder_proc_UnrefDescriptor(that->m_team, target, STRONG);
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcDECREFS: {
				if (iobuffer_read_u32(&io, &target)) goto finished;
				DBREFS((KERN_WARNING "bcDECREFS of %d\n", target));
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					binder_proc_UnrefDescriptor(that->m_team, target, WEAK);
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcFREE_BUFFER: {
				void *ptr;
				if (iobuffer_read_void(&io, &ptr)) goto finished;
				DPRINTF(5, (KERN_WARNING "bcFREE_BUFFER: %p\n",ptr));
				BND_LOCK(that->m_lock);
				if (that->m_pendingReply && that->m_pendingReply->map != NULL && binder_transaction_UserData(that->m_pendingReply) == ptr) {
					// Data freed before reply sent.  Remember this to free
					// the transaction when we finally get its reply.
					binder_transaction_SetFreePending(that->m_pendingReply, TRUE);
					BND_UNLOCK(that->m_lock);
				} else {
					BND_UNLOCK(that->m_lock);
					binder_proc_FreeBuffer(that->m_team, ptr);
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcRETRIEVE_ROOT_OBJECT: {
				int pid;
				binder_thread_t *child;
				if (iobuffer_read_u32(&io, &pid)) goto finished;
				DPRINTF(2, (KERN_WARNING "bcRETRIEVE_ROOT_OBJECT: process %d\n", pid));
				child = attach_child_thread((pid_t)pid, that);
				DPRINTF(2, (KERN_WARNING "bcRETRIEVE_ROOT_OBJECT: child binder_thread=%p\n", child));
				
				BND_LOCK(that->m_lock);
				if (child) {
					that->pendingChild = child;
					that->m_waitForReply++;
				} else {
					that->m_failedRootReceive = TRUE;
				}
				BND_UNLOCK(that->m_lock);
				
				iobuffer_mark_consumed(&io);
			} break;
			case bcTRANSACTION:
			case bcREPLY: {
				binder_transaction_data_t tr;

				if(cmd == bcTRANSACTION) {
					DPRINTF(5, (KERN_WARNING "bcTRANSACTION\n"));
				}
				else {
					DPRINTF(5, (KERN_WARNING "bcREPLY\n"));
				}

				if (iobuffer_read_raw(&io, &tr, sizeof(tr))) goto finished;
				if (tr.flags & tfInline) {
					// ddprintf("inline transactions not supported yet\n");
					that->m_consume = tr.data_size - sizeof(tr.data);
					iobuffer_mark_consumed(&io);
				} else if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					binder_transaction_t *t;
					iobuffer_mark_consumed(&io);
/*					
					if (tr.data_size && !is_valid_range(tr.data.ptr.buffer, tr.data_size, PROT_UWR)) {
						that->m_err = -EINVAL;
						goto finished;
					}
					if (tr.offsets_size && !is_valid_range(tr.data.ptr.offsets, tr.offsets_size, PROT_UWR)) {
						that->m_err = -EINVAL;
						goto finished;
					}
*/
					t = binder_transaction_Create(tr.code, tr.data_size, tr.data.ptr.buffer, tr.offsets_size, tr.data.ptr.offsets);
					binder_transaction_SetUserFlags(t, tr.flags);
					binder_transaction_SetPriority(t, (s16)tr.priority);
					binder_transaction_SetReply(t, cmd == bcREPLY);
					
					if (cmd == bcTRANSACTION) {
						target = tr.target.handle;
						if(target) {
							t->target = binder_proc_Descriptor2Node(that->m_team, target, t, STRONG);
							BND_ASSERT(t->target, "Failure converting target descriptor to node");
						}
						else {
							BND_LOCK(gContextManagerNodeLock);
							if (gContextManagerNode && BND_ATTEMPT_ACQUIRE(binder_node, gContextManagerNode, STRONG, t)) {
								t->target = gContextManagerNode;
							}
							else {
								DPRINTF(0, (KERN_WARNING "Failed to acquire context manager node\n"));
								t->target = NULL;
							}
							BND_UNLOCK(gContextManagerNodeLock);
						}
						DPRINTF(4, (KERN_WARNING "Transacting %p to %d(%p) in team %p\n", t, target, t->target, t->target ? t->target->m_team : NULL));
					}
					
					BND_LOCK(that->m_lock);
					t->next = that->m_pendingRefResolution;
					that->m_pendingRefResolution = t;
					if (that->m_pendingReply && binder_transaction_IsRootObject(that->m_pendingReply)) {
						BND_ASSERT(binder_transaction_IsRootObject(t), "EXPECTING ROOT REPLY!");
					} else {
						BND_ASSERT(!that->m_pendingReply || !binder_transaction_IsRootObject(t), "UNEXPECTED ROOT REPLY!");
					}
					BND_UNLOCK(that->m_lock);
					
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
			} break;
			case bcREGISTER_LOOPER: {
				DPRINTF(5, (KERN_WARNING "bcREGISTER_LOOPER for %p (%p:%d)\n", that, that->m_team, that->m_thid));
				BND_ASSERT(that->m_isSpawned == FALSE, "m_isSpawned in bcREGISTER_LOOPER");
				BND_ASSERT(that->m_isLooping == FALSE, "m_isLooping in bcREGISTER_LOOPER");
				that->m_isSpawned = TRUE;
				that->m_isLooping = TRUE;
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					binder_proc_StartLooper(that->m_team, TRUE);
					clear_bit(SPAWNING_BIT, &that->m_team->m_noop_spawner);
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcENTER_LOOPER: {
				DPRINTF(5, (KERN_WARNING "bcENTER_LOOPER for %p (%p:%d)\n", that, that->m_team, that->m_thid));
				/*	This thread is going to loop, but it's not one of the
					driver's own loopers. */
				// ASSERT(that->m_isLooping == FALSE);
				that->m_isLooping = TRUE;
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					binder_proc_StartLooper(that->m_team, FALSE);
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcEXIT_LOOPER: {
				/*	End of a looper that is not the driver's own. */
				DBSPAWN((KERN_WARNING "*** THREAD %p:%d RECEIVED bcEXIT_LOOPER\n", that->m_team, that->m_thid));
				if (binder_proc_IsAlive(that->m_team)) {
					// ASSERT(that->m_isLooping == TRUE);
					that->m_isLooping = FALSE;
					if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
						binder_proc_FinishLooper(that->m_team, FALSE);
						BND_RELEASE(binder_proc, that->m_team, STRONG, that);
					}
				}
				iobuffer_mark_consumed(&io);
			} break;
#if 0
			case bcCATCH_ROOT_OBJECTS: {
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					if (binder_proc_IsAlive(that->m_team)) {
						binder_proc_StartCapturingRootObjects(that->m_team);
					}
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
#endif
			case bcSTOP_PROCESS: {
				int now;
				if (iobuffer_read_u32(&io, &target)) goto finished;
				if (iobuffer_read_u32(&io, &now)) goto finished;
				DBREFS((KERN_WARNING "bcSTOP_PROCESS of %d\n", target));
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					binder_node_t *node = binder_proc_Descriptor2Node(that->m_team, target,that,WEAK);
					if (node != NULL && binder_node_Home(node) != NULL) {
						binder_proc_Stop(binder_node_Home(node), now ? TRUE : FALSE);
						BND_RELEASE(binder_node, node, WEAK,that);
					}
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
			case bcSTOP_SELF: {
				DPRINTF(5, (KERN_WARNING "bcSTOP_SELF\n"));
				int now;
				if (iobuffer_read_u32(&io, &now)) goto finished;
				if (BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) {
					binder_proc_Stop(that->m_team, now ? TRUE : FALSE);
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
				}
				iobuffer_mark_consumed(&io);
			} break;
			default: {
				DPRINTF(5, (KERN_WARNING "Bad command %d on binder write().\n", cmd));
			} break;
		}
	}

finished:
	DPRINTF(5, (KERN_WARNING "binder_thread_Write() finished\n"));
	*consumed = iobuffer_consumed(&io);
	return 0;
}

status_t
binder_thread_ReturnTransaction(binder_thread_t *that, iobuffer_t *io, binder_transaction_t *t)
{
	bool acquired;
	bool freeImmediately;
	binder_transaction_data_t tr;
	DPRINTF(0, (KERN_WARNING "%s(%p:%d, %p, %p)\n", __func__, that->m_team, that->m_thid, io, t));
	if (iobuffer_remaining(io) < 18) return -ENOBUFS;

	acquired = BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that);
	if (acquired) binder_transaction_ConvertFromNodes(t, that->m_team);

	freeImmediately = FALSE;
	
	if (binder_transaction_RefFlags(t)) {
		DPRINTF(5, (KERN_WARNING " -- binder_transaction_RefFlags()\n"));
		switch (binder_transaction_RefFlags(t)) {
			case tfAttemptAcquire: {
				DPRINTF(5, (KERN_WARNING " --- tfAttemptAcquire\n"));
				iobuffer_write_u32(io, brATTEMPT_ACQUIRE);
				iobuffer_write_u32(io, binder_transaction_Priority(t));
			} break;
			case tfRelease:
				DPRINTF(5, (KERN_WARNING " --- tfRelease\n"));
				iobuffer_write_u32(io, brRELEASE);
				break;
			case tfDecRefs:
				DPRINTF(5, (KERN_WARNING " --- tfDecRefs\n"));
				iobuffer_write_u32(io, brDECREFS);
				break;
		}
		DPRINTF(5, (KERN_WARNING " --- writing data pointer %p\n", t->data_ptr));
		// iobuffer_write_void(io, *((void**)binder_transaction_Data(t)));
		iobuffer_write_void(io, t->data_ptr);		// binder object token
		iobuffer_write_void(io, t->offsets_ptr);	// binder object cookie
		freeImmediately = binder_transaction_RefFlags(t) != tfAttemptAcquire;
		// Take reference on team, so it won't go away until this transaction
		// is processed.
		if (binder_transaction_TakeTeam(t, that->m_team)) {
			BND_LOCK(that->m_lock);
			that->m_teamRefs++;
			BND_UNLOCK(that->m_lock);
		}
	} else if (binder_transaction_IsAcquireReply(t)) {
		DPRINTF(5, (KERN_WARNING " -- binder_transaction_IsAcquireReply()\n"));
		iobuffer_write_u32(io, brACQUIRE_RESULT);
		// iobuffer_write_u32(io, *((int*)binder_transaction_Data(t)));
		iobuffer_write_u32(io, *(u32*)t->data);
		freeImmediately = TRUE;
	} else if (binder_transaction_IsDeadReply(t)) {
		DPRINTF(5, (KERN_WARNING " -- binder_transaction_IsDeadReply()\n"));
		if (that->pendingChild) binder_thread_ReleasePendingChild(that);
		iobuffer_write_u32(io, brDEAD_REPLY);
		freeImmediately = TRUE;
	} else {
		DPRINTF(5, (KERN_WARNING " -- else binder_transaction_IsReply(%p): %s\n", t, binder_transaction_IsReply(t) ? "true" : "false"));
		if (that->pendingChild) binder_thread_ReleasePendingChild(that);
		tr.flags = binder_transaction_UserFlags(t);
		tr.priority = binder_transaction_Priority(t);
		if (acquired) {
			tr.data_size = binder_transaction_DataSize(t);
			tr.offsets_size = binder_transaction_OffsetsSize(t);
			tr.data.ptr.buffer = binder_transaction_UserData(t);
			tr.data.ptr.offsets = binder_transaction_UserOffsets(t);
		} else {
			tr.data_size = 0;
			tr.offsets_size = 0;
			tr.data.ptr.buffer = NULL;
			tr.data.ptr.offsets = NULL;
		}

		DPRINTF(5, (KERN_WARNING "%s(%p:%d, %p, %p) tr-data %p %d tr-offsets %p %d\n", __func__, that->m_team, that->m_thid, io, t, tr.data.ptr.buffer, tr.data_size, tr.data.ptr.offsets, tr.offsets_size));

		if (binder_transaction_IsReply(t)) {
			tr.target.ptr = NULL;
			tr.code = 0;
			iobuffer_write_u32(io, brREPLY);
		} else {
			tr.target.ptr = t->target ? binder_node_Ptr(t->target) : NULL;
			tr.code = binder_transaction_Code(t);
			iobuffer_write_u32(io, brTRANSACTION);
		}
		iobuffer_write_raw(io, &tr, sizeof(tr));
	}

	if (freeImmediately) {
		DPRINTF(0, (KERN_WARNING "binder_thread_ReturnTransaction() delete %p\n",t));
		binder_transaction_Destroy(t);
	} else {
		t->receiver = that;
		BND_ACQUIRE(binder_thread, that, WEAK, t);
		if (t->sender) {
			/*	A synchronous transaction blocks this thread until
				the receiver completes. */
			DPRINTF(0, (KERN_WARNING "binder_thread %p:%d (%d): enqueueing transaction %p, pending reply %p\n", that->m_team, that->m_thid, that->virtualThid, t, that->m_pendingReply));
			BND_ASSERT(!binder_transaction_IsFreePending(t), "transaction with free pending!");
			if (that->virtualThid) {
				if (t->sender->virtualThid) {
					BND_ASSERT(t->sender->virtualThid == that->virtualThid, "Bad virtualThid from sender!");
				} else {
					BND_ASSERT(t->sender->m_thid == that->virtualThid, "My virtualThid is different than sender thid!");
				}
			}
			DPRINTF(5, (KERN_WARNING "t->sender->virtualThid: %d, that->virtualThid: %d\n", t->sender->virtualThid, that->virtualThid));
			if (t->sender->virtualThid) {
				BND_ASSERT(that->virtualThid == 0 || that->virtualThid == t->sender->virtualThid, "virtualThid not cleared!");
				that->virtualThid = t->sender->virtualThid;
				DPRINTF(0, (KERN_WARNING "Continuing virtualThid: %d\n", that->virtualThid));
			} else {
				BND_ASSERT(that->virtualThid == 0 || that->virtualThid == t->sender->m_thid, "virtualThid not cleared!");
				that->virtualThid = t->sender->m_thid;
				DPRINTF(0, (KERN_WARNING "Starting new virtualThid: %d\n", that->virtualThid));
			}
			BND_LOCK(that->m_lock);
			DPRINTF(5, (KERN_WARNING "%p:%d(%d) new reply: %p, pending reply: %p\n", that->m_team, that->m_thid, that->virtualThid, t, that->m_pendingReply));
			t->next = that->m_pendingReply;
			that->m_pendingReply = t;
			BND_UNLOCK(that->m_lock);
		} else {
			/*	A reply transaction just waits until the receiver is done with
				its data. */
			DPRINTF(0, (KERN_WARNING "binder_thread: return reply transaction %p\n", t));
			binder_proc_AddToNeedFreeList(that->m_team, t);
		}
	}

	iobuffer_mark_consumed(io);
	
	if (acquired) BND_RELEASE(binder_proc, that->m_team, STRONG, that);
	
	return 0;
}

status_t
binder_thread_WaitForReply(binder_thread_t *that, iobuffer_t *io)
{
	status_t err;
	binder_transaction_t *t = NULL;
	if (iobuffer_remaining(io) < 18) return -ENOBUFS;

	if (!BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)) return -ECONNREFUSED;
	
	if (that->m_isLooping) binder_proc_TakeMeOffYourList(that->m_team);

	// FIXME: implement reply timeouts?
	err = wait_event_interruptible(that->m_wait, atomic_read(&that->m_wake_count) > 0);
	if(err == 0)
		atomic_dec(&that->m_wake_count);
	DPRINTF(0, (KERN_WARNING "%p:%d down_interruptible() returned %08x\n", that->m_team, that->m_thid, err));

	//DBTRANSACT((KERN_WARNING "*** Thread %d received direct %p!  wait=%d, isAnyReply=%d\n", current->pid, that->m_reply, that->m_waitForReply, binder_transaction_IsAnyReply(that->m_reply)));

	/* FFB: why don't we check the err here, geh/hackbod? */
	if (that->m_isLooping) binder_proc_PutMeBackInTheGameCoach(that->m_team);

	BND_LOCK(that->m_lock);
	if ((t = that->m_reply)) {
		status_t result;
		/*	If this is a reply, handle it.  When the binder_proc_t supplies
			a reflection, it will take care of adjusting our thread
			priority at that point.  The user-space looper is responsible
			for restoring its priority when done handling the reflect. */
		if (binder_transaction_IsAnyReply(t)) that->m_waitForReply--;
		that->m_reply = t->next;
		BND_UNLOCK(that->m_lock);
		result = binder_thread_ReturnTransaction(that, io, t);
		BND_RELEASE(binder_proc, that->m_team, STRONG, that);
		return result;
	}
	BND_UNLOCK(that->m_lock);
	
	BND_RELEASE(binder_proc, that->m_team, STRONG, that);
	// We can get here if we need to spawn a looper.
	// BND_VALIDATE(err != 0, "Binder replySem released without reply available", return -EINVAL);
	return err;
}

status_t
binder_thread_WaitForRequest(binder_thread_t *that, iobuffer_t *io)
{
	binder_transaction_t *t = NULL;
	status_t err;
	if (iobuffer_remaining(io) < 18) return -ENOBUFS;
	
	err = binder_proc_WaitForRequest(that->m_team, that, &t);
	if (err == 0 && t != NULL) {
		// ASSERT(t);
		err = binder_thread_ReturnTransaction(that, io, t);
	}
	
	return err;
}

static status_t
binder_thread_WaitForParent(binder_thread_t *that)
{
	binder_thread_t *targetThread;
	struct task_struct *parentTask;
	pid_t childPid;
	bigtime_t wakeupTime;
	status_t err;
	
	DPRINTF(5, (KERN_WARNING "%s: on thread %p\n", __func__, that));
		
	// We want to support wrappers, where the real child process
	// being run may have some additional processes (such as xterms,
	// gdb sessions, etc) between it and the parent that started it.
	// In that case, the parent won't be talking directly with our
	// thread structure but instead with its immediate child, so we
	// need to go up and find it.
	
	targetThread = that;
	if (that->m_pendingReply == NULL) {
		DPRINTF(5, (KERN_WARNING "%s: PID %d: finding parent who forked us.\n", __func__, that->m_thid));
		// Parent hasn't set this thread up for a reply...  figure out
		// what is going on.
		targetThread = NULL;
		parentTask = current;
		do {
			childPid = parentTask->pid;
			parentTask =  parentTask->parent;
			if (!parentTask) break;
			targetThread = check_for_thread(parentTask->pid, FALSE);
			DPRINTF(5, (KERN_WARNING "%s: Up to parent PID %d: targetThread=%p\n", __func__, parentTask->pid, targetThread));
		} while (targetThread == NULL);
		
		// If we found a thread structure, and it is not set up to
		// send a root reply, then we hit the parent and it has not
		// yet stopped to wait for the reply.  So we'll go ahead and
		// and create the child thread structure so we can block on
		// it until the parent gets it set up.
		DPRINTF(5, (KERN_WARNING "%s: Finished search: targetThread=%p, childPid=%d\n", __func__, targetThread, childPid));
		if (targetThread && !targetThread->m_pendingReplyIsRoot) {
			targetThread = check_for_thread(childPid, TRUE);
			DPRINTF(5, (KERN_WARNING "%s: Created wrapper process thread structure: %p\n", __func__, targetThread));
		}
	}
	
	if (targetThread == NULL) {
		printk(KERN_WARNING "%s: Binder: PID %d attempting to send root reply without waiting parent\n", __func__, that->m_thid);
		return -EINVAL;
	}
	
	// Now wait for the parent to be blocked waiting for a reply.
	// Hard-coded to give the parent 10 seconds to get around to us.
	wakeupTime = 10*HZ;
	do_div(wakeupTime, TICK_NSEC);
	wakeupTime += get_jiffies_64();
	DPRINTF(0, (KERN_WARNING "%s: Process %d is about to snooze on thread %p (%d)\n", __func__, current->pid, targetThread, targetThread->m_thid));
	err = binder_thread_Snooze(targetThread, wakeupTime);
	
	// Just one more thing to deal with -- if there is a wrapper process,
	// then it is the wrapper that has been set up to reply.  We need to
	// move that state to our own process because we are the one doing
	// the reply.
	if (targetThread != that) {
		binder_transaction_t* reply;
		BND_LOCK(targetThread->m_lock);
		DPRINTF(1, (KERN_WARNING "%s: Wrapper has pendingReply=%p, isRoot=%d\n", __func__, targetThread->m_pendingReply, targetThread->m_pendingReplyIsRoot));
		reply = targetThread->m_pendingReply;
		if (reply) {
			targetThread->m_pendingReply = reply->next;
			targetThread->m_pendingReplyIsRoot = FALSE;
		}
		BND_UNLOCK(targetThread->m_lock);
		
		if (reply) {
			BND_LOCK(that->m_lock);
			reply->next = that->m_pendingReply;
			that->m_pendingReply = reply;
			that->m_pendingReplyIsRoot = TRUE;
			BND_UNLOCK(that->m_lock);
		}
		
		// The retrieval of the wrapper thread structure caused us
		// to take a reference on it.  Now release the reference,
		// removing the structure from our thread list if appropriate.
		forget_thread(targetThread);
	}
	
	if (err != 0 && that->m_pendingReply) {
		/*	If an error occurred but the pendingReply has
			also been given, then our semaphore has also been
			released.  We don't want to get out of sync. */
		DPRINTF(5, (KERN_WARNING "Thread %d: Re-acquire IO sem!\n", binder_thread_Thid(that)));
		// Note: targetThread -is- the correct one to use here, that
		// is the one we blocked on.
		binder_thread_AcquireIOSem(targetThread);
	}
						
	DPRINTF(0, (KERN_WARNING "%s: Returning: pendingReply=%p, err=%d\n", __func__, that->m_pendingReply, err));
	return that->m_pendingReply ? 0 : err;
}

int 
binder_thread_Read(binder_thread_t *that, void *buffer, int size, signed long *consumed)
{
	int origRemain;
	status_t err = 0;
	bool isRoot;
	bool isInline;
	binder_proc_t *proc;
	/* ditch these next two lines under linux, if we can */
	pid_t me = current->pid;

	binder_transaction_t *t,*replyTo;
	iobuffer_t io;
	bool acquired = FALSE;

	if (me != that->m_thid) return -EINVAL;

	DPRINTF(0, (KERN_WARNING "binder_thread_Read: %08lx (%p:%d)\n", (unsigned long)that, that->m_team, that->m_thid));
	iobuffer_init(&io, (unsigned long)buffer, size, *consumed);

	/*
	 * Write brNOOP, but don't mark it consumed.  We'll replace the brNOOP with
	 * a brSPAWN_LOOPER if we need to spawn a thread.
	 * Only do this once, in case the system call gets restarted for some reason.
	 */
	if (*consumed == 0) iobuffer_write_u32(&io, brNOOP);

	/*	Read as much data as possible, until we either have to block
		or have filled the buffer. */
	
	while (iobuffer_remaining(&io) > 8) {
		if (!binder_proc_IsAlive(that->m_team)) {
			/*	If the team is dead, write a command to say so and exit
				right now.  Do not pass go, do not collect $200. */
			DPRINTF(0, (KERN_WARNING "  binder_proc_IsAlive(%08x): false\n", (unsigned int)that->m_team));
			iobuffer_write_u32(&io, brFINISHED);
			iobuffer_mark_consumed(&io);
			err = -ECONNREFUSED;
			goto finished;
		} else if (that->m_shortAttemptAcquire) {
			/*	Return the result of a short-circuited attempt acquire. */
			DPRINTF(0, (KERN_WARNING "Thread %d already has reply!\n", that->m_thid));
			that->m_shortAttemptAcquire = FALSE;
			iobuffer_write_u32(&io, brACQUIRE_RESULT);
			iobuffer_write_u32(&io, that->m_resultAttemptAcquire);
			iobuffer_mark_consumed(&io);
			continue;
		} else if (that->m_failedRootReceive) {
			// XXX Would be nice to return a little more informative
			// error message.
			that->m_failedRootReceive = FALSE;
			iobuffer_write_u32(&io, brDEAD_REPLY);
			goto finished;
		}
		
		/*	Look for a queued transaction. */
		BND_LOCK(that->m_lock);
		if ((t=that->m_pendingRefResolution) != NULL) {
			if (iobuffer_consumed(&io) > 0 && (binder_transaction_MaxIOToNodes(t)+4) > iobuffer_remaining(&io)) {
				/*	If there is already data in the buffer, and may not be enough
					room for what this transaction could generate, then stop now. */
				DPRINTF(0, (KERN_WARNING "Aborting ConvertToNodes: consumed=%d, max=%d, remain=%d\n", iobuffer_consumed(&io), binder_transaction_MaxIOToNodes(t)+4, iobuffer_remaining(&io)));
				BND_UNLOCK(that->m_lock);
				goto finished;
			}
			that->m_pendingRefResolution = t->next;
		}
		BND_UNLOCK(that->m_lock);
		
		/*	If a transaction was found, twiddle it and send it off. */
		if (t != NULL && (acquired || (acquired=BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)))) {
			
			DPRINTF(5, (KERN_WARNING "Thread %d has pending transaction %p\n", that->m_thid, t));
				
			isRoot = (binder_transaction_IsRootObject(t));
				
			/*	Perform node conversion if not already done. */
			if (!binder_transaction_IsReferenced(t)) {
				DBREAD((KERN_WARNING "Thread %d performing ref resolution!\n", that->m_thid));
				origRemain = iobuffer_remaining(&io);
				proc = 0;
				err = 0;
				if (isRoot) {
					// If we are replying with the root object, we first need to block
					// until our parent has set us up to have somewhere to reply to.
					err = binder_thread_WaitForParent(that);

					BND_LOCK(that->m_lock);
					that->m_failedRootReply = that->m_pendingReply == NULL;
					if (that->m_failedRootReply) err = -EINVAL;
					BND_UNLOCK(that->m_lock);
				}
				/*
				 * The moment of truth.  In order to convert nodes, we have to
				 * copy the data.  In order to copy the data, we need to know
				 * the recipient of the transaction.  If the transaction has a
				 * target, the target's team becomes the recipient.  If the
				 * transaction carries a reply, use the pending reply's sending
				 * team.
				 */
				if (err == 0) {
					proc = t->target ? binder_node_Home(t->target) : that->m_pendingReply ? binder_thread_Team(that->m_pendingReply->sender) : NULL;
					err = proc ? 0 : -EINVAL;
				}
				if (!proc) {
					DPRINTF(0, (KERN_WARNING "*#*#*# NO TARGET PROCESS FOR binder_transaction_CopyTransactionData #*#*#*\n"));
					DPRINTF(0, (KERN_WARNING "t->target: %p, that->m_pendingReply: %p, m_pendingReply->sender: %p\n", t->target, that->m_pendingReply, that->m_pendingReply ? that->m_pendingReply->sender : NULL));
				}
				if (err == 0)
					err = binder_transaction_CopyTransactionData(t, proc);
				if (err == 0)
					err = binder_transaction_ConvertToNodes(t, that->m_team, &io);
				/*	If we got some error, report error to the caller so they don't wait forever. */
				if (err < 0 && !binder_transaction_IsReply(t))
					iobuffer_write_u32(&io, brDEAD_REPLY);
				iobuffer_mark_consumed(&io);
				if (err < 0 || iobuffer_remaining(&io) < 4) {
					/*	XXX Fail if we run out of room.  Do we need to deal with this
						better.  (It's only a problem if the caller is trying to read in
						to a buffer that isn't big enough, in total, for a returned
						transaction. */
					DPRINTF(0, (KERN_WARNING "Aborting transaction: err: %08x (or not enough room to return last command)\n", err));
					binder_transaction_Destroy(t);
					err = 0;
					goto finished;
				}
				
				/*	If we aren't sending anything back to the caller, we can
					deliver this transaction right away.   Otherwise, we must
					wait for the caller to process the returned data.  This
					is due to a race condition between the receiver releasing
					its references and the caller acquiring any new references
					returned by the driver. */
				if (origRemain != iobuffer_remaining(&io)) {
					DBREAD((KERN_WARNING "Transaction acquired references!  Keeping.\n"));
					BND_LOCK(that->m_lock);
					t->next = that->m_pendingRefResolution;
					that->m_pendingRefResolution = t;
					BND_UNLOCK(that->m_lock);
					t = NULL;
				}
			}
#if 0
			// FFB's broken debug code
			else {
				DPRINTF(0, (KERN_WARNING "binder_transaction_IsReferenced(%p) true -- sender: %d (vthid: %d)\n", t, t->sender->m_thid, t->sender->virtualThid));
			}
#endif
			
			/*	Send this transaction off to its target. */
			if (t != NULL) {
				DBREAD((KERN_WARNING "Thread %d delivering transaction!\n", that->m_thid));
				isInline = binder_transaction_IsInline(t);
				if (binder_transaction_IsAnyReply(t)) {
					BND_LOCK(that->m_lock);
					
					replyTo = that->m_pendingReply;
					if (replyTo) {
						that->m_pendingReply = replyTo->next;
						if (!that->m_pendingReply) {
							that->virtualThid = 0;
							DPRINTF(5, (KERN_WARNING "virtualThid reset to 0, m_waitForReply: %d\n", that->m_waitForReply));
						} else {
							DPRINTF(5, (KERN_WARNING "virtualThid: %d, m_pendingReply: %p, m_waitForReply: %d\n", that->virtualThid, that->m_pendingReply, that->m_waitForReply));
						}
						BND_UNLOCK(that->m_lock);
						
						/* If this is a successful bcATTEMPT_ACQUIRE, then take
							care of reference counts now.
						*/
						if (binder_transaction_IsAcquireReply(t) && (*(int*)t->data != 0)) {
							binder_proc_ForceRefNode(binder_thread_Team(replyTo->sender), replyTo->target, &io);
						}
						
						if (binder_transaction_IsRootObject(replyTo)) {
							BND_ASSERT(binder_transaction_IsRootObject(t), "EXPECTING ROOT REPLY!");
						} else if (binder_transaction_RefFlags(replyTo)&tfAttemptAcquire) {
							BND_ASSERT(binder_transaction_IsAcquireReply(t), "EXPECTING ACQUIRE REPLY!");
						} else {
							BND_ASSERT(!binder_transaction_IsRootObject(t) && !binder_transaction_IsAcquireReply(t), "EXPECTING REGULAR REPLY!");
						}
						
						DBTRANSACT((KERN_WARNING "*** Thread %d is replying to %p with %p!  wait=%d\n",
								that->m_thid, replyTo, t, that->m_waitForReply));
						binder_thread_Reply(replyTo->sender, t);
						if (binder_transaction_IsInline(replyTo) || binder_transaction_IsRootObject(replyTo)) {
							binder_transaction_Destroy(replyTo);
						} else {
							DPRINTF(0, (KERN_WARNING "binder_thread: finish reply request %p\n", replyTo));
							if (binder_transaction_IsFreePending(replyTo)) {
								binder_transaction_Destroy(replyTo);
							} else {
								binder_proc_AddToNeedFreeList(that->m_team, replyTo);
							}
						}
					} else {
						BND_UNLOCK(that->m_lock);
						DPRINTF(1, (KERN_WARNING "********** Nowhere for reply to go!!!!!!!!!!!\n"));
#if 0
						BND_ASSERT(binder_transaction_IsRootObject(t) || !binder_proc_IsAlive(that->m_team), "Unexpected reply!");
						if (binder_transaction_IsRootObject(t)) binder_proc_CaptureRootObject(t);
						else {
							binder_transaction_Destroy(t);
						}
#endif
					}
				} else {
					t->sender = that;
					BND_ACQUIRE(binder_thread, that, WEAK, t);
					that->m_waitForReply++;
					DPRINTF(2, (KERN_WARNING "*** Thread %d going to wait for reply to %p!  now wait=%d\n", that->m_thid, t, that->m_waitForReply));
					if (t->target) binder_node_Send(t->target, t);
					else {
						binder_thread_ReplyDead(that);
						binder_transaction_Destroy(t);
					}
				}
				if (!isInline) iobuffer_write_u32(&io, brTRANSACTION_COMPLETE);
				iobuffer_mark_consumed(&io);
			}
			
		/*	Got a transaction but team is going away.  Toss it. */
		} else if (t != NULL) {
			DPRINTF(0, (KERN_WARNING "Transaction sent to dying team, thread %d.\n", that->m_thid));
			binder_transaction_DestroyNoRefs(t);
		
		/*	If there is data available, return it now instead of
			waiting for the next transaction. */
		} else if (iobuffer_consumed(&io) > 0) {
			DPRINTF(2, (KERN_WARNING "Thread %d has %d bytes of data to return, won't wait for transaction.\n", that->m_thid, iobuffer_consumed(&io)));
			goto finished;
		
		/*	No transaction, but maybe we are waiting for a reply back? */
		} else if (that->m_waitForReply) {
			DPRINTF(2, (KERN_WARNING "Thread %d waiting for reply!\n", that->m_thid));
			if ((sizeof(binder_transaction_data_t)+8) > iobuffer_remaining(&io)) {
				/*	If there isn't enough room in the buffer to return a transaction,
					then stop now. */
				DPRINTF(0, (KERN_WARNING "Aborting read: Not enough room to return reply\n"));
				goto finished;
			}
			err = binder_thread_WaitForReply(that, &io);
			if (err == -ENOBUFS) err = 0;
			goto finished;
			
		/*	We're all out.  Just wait for something else to do. */
		} else {
			DPRINTF(2, (KERN_WARNING "Thread %d waiting for request, vthid: %d!\n", that->m_thid, that->virtualThid));
			BND_ASSERT(that->virtualThid == 0, "Waiting for transaction with vthid != 0");
			
			if (that->m_teamRefs > 0) {
				int relCount;
				BND_LOCK(that->m_lock);
				relCount = that->m_teamRefs;
				that->m_teamRefs = 0;
				BND_UNLOCK(that->m_lock);
				DPRINTF(3, (KERN_WARNING "Unlocking proc %08x %d times\n", (unsigned int)that->m_team, relCount));
				
				while (relCount) {
					BND_RELEASE(binder_proc, that->m_team, STRONG, that);
					relCount--;
				}
			}
			
			err = binder_thread_WaitForRequest(that, &io);
			if (err == -ERESTARTSYS) {
				goto finished;
			} else if (err == -EINTR) {
				goto finished;
			} else if (err == -ECONNREFUSED) {
				goto finished;
			} else if (err == -ENOBUFS) {
				err = 0;
				goto finished;
			} else if (err == REQUEST_EVENT_READY) {
				iobuffer_write_u32(&io, brEVENT_OCCURRED);
				iobuffer_write_u32(&io, that->returnedEventPriority);
				iobuffer_mark_consumed(&io);
				err = 0;
			} else if (err == -ETIMEDOUT) {
				if (that->m_isLooping) {
					if ((acquired=BND_ATTEMPT_ACQUIRE(binder_proc, that->m_team, STRONG, that)))
						binder_proc_FinishLooper(that->m_team, that->m_isSpawned);
					that->m_isLooping = FALSE;
				}
				if (that->m_isSpawned) iobuffer_write_u32(&io, brFINISHED);
				else iobuffer_write_u32(&io, brOK);
				iobuffer_mark_consumed(&io);
				err = 0;
			}
			/*
			 else if (err == B_BAD_SEM_ID) {
				iobuffer_write_u32(&io, brFINISHED);
				iobuffer_mark_consumed(&io);
			}
			*/
			else if (err < 0) {
				iobuffer_write_u32(&io, brERROR);
				iobuffer_write_u32(&io, err);
				iobuffer_mark_consumed(&io);
				err = 0;
				goto finished;
			}
		}
	}

finished:
	if (acquired) BND_RELEASE(binder_proc, that->m_team, STRONG, that);
	
	// Return number of bytes available, or the last error code
	// if there are none.  (This is so we can return -EINTR.)
	*consumed = iobuffer_consumed(&io);

	if (err != -ERESTARTSYS) {
		if (test_and_clear_bit(DO_SPAWN_BIT, &that->m_team->m_noop_spawner)) {
			DBSPAWN((KERN_WARNING "Asking %p:%d to brSPAWN_LOOPER\n", that->m_team, that->m_thid));
			// make the brNOOP into a brSPAWN_LOOPER
			// *(u32*)buffer = brSPAWN_LOOPER;
			// We call the unchecked __put_user() here because the constructor
			// for iobuffer already called access_ok().
			__put_user(brSPAWN_LOOPER, (u32*)buffer);
			if (iobuffer_consumed(&io) < sizeof(u32)) {
				iobuffer_mark_consumed(&io);
				*consumed = iobuffer_consumed(&io);
			}
		}
	}
	return err;
}

status_t
binder_thread_Snooze(binder_thread_t *that, bigtime_t timeout)
{
	status_t res = 0;

	DPRINTF(1, (KERN_WARNING "binder_thread_Snooze(%d, %lld)\n", that->m_thid, timeout));
	/*
	 * I don't know if I got the semantics correct for this.
	status_t res = acquire_sem_etc(that->m_ioSem,1,B_CAN_INTERRUPT|B_ABSOLUTE_TIMEOUT,timeout);
	*/

	if(signal_pending(current)) {
		DPRINTF(1, (KERN_WARNING "binder_thread_Snooze(%d, %lld) signal pending -- ABORT\n", that->m_thid, timeout));
		return -ERESTARTSYS;
	}

	timeout -= get_jiffies_64();
	DPRINTF(1, (KERN_WARNING "binder_thread_Snooze(%d, relative %lld)\n", that->m_thid, timeout));
	if (timeout > 0) {
#if 1
		bigtime_t check = timeout;
		do_div(check, HZ);
		if (check > 10) {
			DPRINTF(0, (KERN_WARNING "%s: timeout exceeds 10 seconds at %Ld sec\n", __func__, check));
			return -ETIMEDOUT;
		}
#endif
		DPRINTF(5, (KERN_WARNING "%s: m_wake_count: %d\n", __func__, atomic_read(&that->m_wake_count)));
		res = wait_event_interruptible_timeout(that->m_wait, atomic_read(&that->m_wake_count) > 0, timeout);
		if(res > 0)
			atomic_dec(&that->m_wake_count);
	}
	else { 
		/* Makes system lock up due to busy wait 
		 * bug temporary 
		 * when not using unlocked ioctl
		 */
		static unsigned int last_yield = 0;
		unsigned int now = jiffies;
		if ((now - last_yield) > 5*HZ) {
			last_yield = now;
			//printk(KERN_WARNING "binder_thread_Snooze(%d, %lld) yield wakeup_time thread %lld, team %lld, this %p, team->waitStack %p, team->state %x\n",
			//	that->m_thid, timeout, that->wakeupTime, that->m_team->m_wakeupTime, that, that->m_team->m_waitStack, that->m_team->m_state);
			yield();
		}
	}

	//ddprintf("Result of snooze in thread %ld: 0x%08lx\n", that->m_thid, res);
	if (res == 0) // timed out
		res = -ETIMEDOUT;
	else if (res > 0) // acquired, reports time remaining
		res = 0;
	return res;
}

status_t
binder_thread_AcquireIOSem(binder_thread_t *that)
{
	int err;
	DPRINTF(0, (KERN_WARNING "binder_thread_AcquireIOSem(%d)\n", that->m_thid));
	// while (acquire_sem_etc(that->m_ioSem,1,B_TIMEOUT,0) == -EINTR) ;
	//wait_event(that->m_wait, that->m_wake_count > 0);
	err = wait_event_interruptible(that->m_wait, atomic_read(&that->m_wake_count) > 0); // this should probably not be interruptible, but it allows us to kill the thread
	if(err == 0)
		atomic_dec(&that->m_wake_count);
	return err;
}

void
binder_thread_Wakeup(binder_thread_t *that)
{
	DIPRINTF(0, (KERN_WARNING "binder_thread_Wakeup(%d)\n", that->m_thid));
	// We use B_DO_NOT_RESCHEDULE here because Wakeup() is usually called
	// while the binder_proc_t is locked.  If the thread is a real-time
	// priority, waking up here will cause pinging between this thread
	// and its caller.  (We wake up, block on the binder_proc_t, the caller
	// continues and unlocks, then we continue.)
	// release_sem_etc(that->m_ioSem, 1, B_DO_NOT_RESCHEDULE);
	// FIXME: this may not have the do-not-reschedule semantics we want (wake_up_interruptible_sync may work for this)
	atomic_add(1, &that->m_wake_count);
	wake_up(&that->m_wait);
	//wake_up_interruptible_sync(&that->m_wait);
}

void 
binder_thread_Reply(binder_thread_t *that, binder_transaction_t *t)
{
	DBTRANSACT((KERN_WARNING "*** Thread %d (vthid %d) sending to %d (vthid %d)!  wait=%d, isReply=%d, isAcquireReply=%d\n",
				current->pid, t->sender ? t->sender->virtualThid : -1,
				that->m_thid, that->virtualThid, that->m_waitForReply, binder_transaction_IsReply(t), binder_transaction_IsAcquireReply(t)));
	BND_LOCK(that->m_lock);
	if (that->m_team && binder_proc_IsAlive(that->m_team)) {
		// BND_VALIDATE(that->m_reply == NULL, "Already have reply!", ddprintf("Current reply: %p, new reply: %p\n", that->m_reply, t));
		BND_ASSERT(that->m_waitForReply > 0, "Not waiting for a reply!");
		t->next = that->m_reply;
		that->m_reply = t;
	} else {
		BND_ASSERT(t != NULL, "binder_thread_Reply() called with NULL transaction!");
		if (t) binder_transaction_Destroy(t);
	}
	BND_UNLOCK(that->m_lock);
	atomic_add(1, &that->m_wake_count);
	wake_up(&that->m_wait);
}

void 
binder_thread_ReplyDead(binder_thread_t *that)
{
	binder_transaction_t* t = binder_transaction_CreateEmpty();
	binder_transaction_SetDeadReply(t, TRUE);
	binder_thread_Reply(that, t);
}

BND_IMPLEMENT_ACQUIRE_RELEASE(binder_thread);
BND_IMPLEMENT_ATTEMPT_ACQUIRE(binder_thread);


