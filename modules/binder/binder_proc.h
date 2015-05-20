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

#ifndef BINDER_PROC_H
#define BINDER_PROC_H

#include <linux/timer.h>
#include <linux/types.h>
#include <linux/rbtree.h>
#include <linux/list.h>
#include "binder_defs.h"
#include "binder_thread.h"
#include "iobuffer.h"

// This "error" is returned by WaitForRequest() when a timed event
// is scheduled to happen.
enum {
	REQUEST_EVENT_READY = 1
};

typedef struct descriptor {
	struct binder_node *node;
	s32 priRef;
	s32 secRef;
} descriptor_t;

typedef struct reverse_mapping {
	struct reverse_mapping *next;
	struct binder_node *node;
	s32 descriptor;
} reverse_mapping_t;

typedef struct local_mapping {
	struct local_mapping *next;
	void *ptr;		// Unique token identifying this object (supplied by user space)
	void *cookie;	// Arbitrary data for user space to associate with the object/token
	struct binder_node *node;
} local_mapping_t;

typedef struct range_map {
	unsigned long	start;	// inclusive
	unsigned long	end;	// non-inclusive
	struct page	*page;
	struct range_map* next; // next in the chain of free buffers
	struct rb_node	rm_rb;
	struct binder_proc *team;
} range_map_t;

enum {
	btEventInQueue	= 0x00000002,
	btDying			= 0x00000004,
	btDead			= 0x00000008,
	btCleaned		= 0x00000010,
	btFreed			= 0x00000020
};

typedef struct binder_proc {
	atomic_t		m_primaryRefs;
	atomic_t 		m_secondaryRefs;
	volatile unsigned long	m_noop_spawner;
#define SPAWNING_BIT 0
#define DO_SPAWN_BIT 1
	struct semaphore	m_lock;
	spinlock_t          m_spin_lock;
	struct semaphore	m_map_pool_lock;
	u32			m_state;
	struct binder_thread *	m_threads;
	struct list_head m_waitStack;
	int              m_waitStackCount;
	bigtime_t		m_wakeupTime;
	s32			m_wakeupPriority;
	struct timer_list  m_wakeupTimer;
	struct timer_list  m_idleTimer;
	bigtime_t		m_idleTimeout;
	bigtime_t		m_replyTimeout;
	s32			m_syncCount;
	s32			m_freeCount;
	struct binder_transaction *	m_head;
	struct binder_transaction **	m_tail;
	struct binder_transaction *	m_needFree;
	struct binder_transaction *	m_eventTransaction;
	local_mapping_t *	m_localHash[HASH_SIZE];
	struct binder_node *	m_rootObject;	// only use for comparison!!
	s32			m_rootStopsProcess;	
	s32			m_numRemoteStrongRefs;
	reverse_mapping_t *	m_reverseHash[HASH_SIZE];
	descriptor_t *		m_descriptors;
	s32			m_descriptorCount;
	s32			m_nonblockedThreads;
	s32			m_waitingThreads;
	s32			m_maxThreads;
	s32			m_idlePriority;
	atomic_t		m_loopingThreads;
	// s32			m_spawningThreads;
	unsigned long	m_mmap_start;	// inclusive
	struct rb_root		m_rangeMap;
	struct rb_root		m_freeMap;
	range_map_t		*m_pool;
	size_t			m_pool_active;
} binder_proc_t;


binder_proc_t *		new_binder_proc(void);
#if 0
binder_proc_t *		new_binder_proc_with_parent(pid_t id, pid_t mainThid, struct binder_thread *parent);
#endif
void			binder_proc_destroy(binder_proc_t *that);

#define 		binder_proc_IsAlive(that) ((that->m_state&(btDying|btDead)) == 0)
// bool			binder_proc_IsAlive(binder_proc_t *that) const;
void 			binder_proc_Released(binder_proc_t *that);

void			binder_proc_Die(binder_proc_t *that, bool locked /* = false */);

BND_DECLARE_ACQUIRE_RELEASE(binder_proc);
BND_DECLARE_ATTEMPT_ACQUIRE(binder_proc);

void			binder_proc_SetRootObject(binder_proc_t *that, struct binder_node *node);

void			binder_proc_Stop(binder_proc_t *that, bool now);

bool			binder_proc_AddThread(binder_proc_t *that, binder_thread_t *t);
void			binder_proc_RemoveThread(binder_proc_t *that, struct binder_thread *t);

status_t		binder_proc_WaitForRequest(binder_proc_t *that,	struct binder_thread* who, struct binder_transaction **t);

/*	Call when a thread receives its bcREGISTER_LOOPER command. */
void			binder_proc_StartLooper(binder_proc_t *that, bool driver_spawned);
/*	Call when exiting a thread who has been told bcREGISTER_LOOPER. */
void			binder_proc_FinishLooper(binder_proc_t *that, bool driverSpawned);

status_t		binder_proc_SetWakeupTime(binder_proc_t *that, bigtime_t time, s32 priority);
status_t		binder_proc_SetIdleTimeout(binder_proc_t *that, bigtime_t timeDelta);
status_t		binder_proc_SetReplyTimeout(binder_proc_t *that, bigtime_t timeDelta);
status_t		binder_proc_SetMaxThreads(binder_proc_t *that, s32 num);
status_t		binder_proc_SetIdlePriority(binder_proc_t *that, s32 pri);

/*	Call to place a transaction in to this team's queue. */
status_t		binder_proc_Transact(binder_proc_t *that, struct binder_transaction *t);

/*	Management of transactions that are waiting to be deallocated.
	These are safe to call with only a secondary reference on the
	team.
*/
status_t		binder_proc_AddToNeedFreeList(binder_proc_t *that, struct binder_transaction *t);
status_t		binder_proc_FreeBuffer(binder_proc_t *that, void *p);

bool			binder_proc_RefDescriptor(binder_proc_t *that, s32 descriptor, s32 type);
bool			binder_proc_UnrefDescriptor(binder_proc_t *that, s32 descriptor, s32 type);
bool			binder_proc_RemoveLocalMapping(binder_proc_t *that, void *ptr, struct binder_node *node);

/*	Called by binder_node when its last strong reference goes away, for the process to
	do the appropriate bookkeeping. */
void			binder_proc_RemoveLocalStrongRef(binder_proc_t *that, struct binder_node *node);

/*	Called by binder_proc_ForceRefNode() if it is restoring the first strong reference
	back on to the node. */
void			binder_proc_AddLocalStrongRef(binder_proc_t *that, struct binder_node *node);

/*	Attempt to acquire a primary reference on the given descriptor.
	The result will be true if this succeeded, in which case you
	can just continue with it.  If the result is false, then
	'out_target' may be set to the binder_node_t the you are making
	the attempt on.  You can execute a transaction to the node
	to attempt the acquire on it, and -must- release a SECONDARY
	reference on the node which this function acquired. */
bool			binder_proc_AttemptRefDescriptor(binder_proc_t *that, s32 descriptor, struct binder_node **out_target);

/*	Forcibly increment the primary reference count of the given,
	in response to a successful binder_proc_AttemptAcquire(). */
void			binder_proc_ForceRefNode(binder_proc_t *that, struct binder_node *node, iobuffer_t *io);

s32			binder_proc_Node2Descriptor(binder_proc_t *that, struct binder_node *node, bool ref /* = true */, s32 type /* = PRIMARY */);
struct binder_node * 	binder_proc_Descriptor2Node(binder_proc_t *that, s32 descriptor, const void* id, s32 type /* = PRIMARY */);
status_t 		binder_proc_Ptr2Node(binder_proc_t *that, void *ptr, void *cookie, struct binder_node **n, iobuffer_t *io, const void* id, s32 type /* = PRIMARY */);

status_t		binder_proc_TakeMeOffYourList(binder_proc_t *that);
status_t		binder_proc_PutMeBackInTheGameCoach(binder_proc_t *that);

bool			binder_proc_ValidTransactionAddress(binder_proc_t *that, unsigned long address, struct page **pageptr);
range_map_t *		binder_proc_AllocateTransactionBuffer(binder_proc_t *that, size_t size);
void			binder_proc_FreeTransactionBuffer(binder_proc_t *that, range_map_t *buffer);
range_map_t *		binder_proc_free_map_insert(binder_proc_t *that, range_map_t *buffer);
#endif // BINDER_PROC_H
