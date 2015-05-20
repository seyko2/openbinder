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

#include <linux/delay.h>
// #include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/hash.h>

#include "binder_defs.h"
#include "binder_proc.h"
#include "binder_thread.h"
#include "binder_node.h"
#include "binder_transaction.h"
#include "iobuffer.h"

#define BND_PROC_MAX_IDLE_THREADS 3

static inline unsigned long calc_order_from_size(unsigned long size)
{
#if 0
	unsigned long order = 0;
	if (size) {
		size -= 1;
		size *= 2;
	}
	size >>= PAGE_SHIFT+1;
	while (size) {
		order++;
		size >>= 1;
	}
	return order;
#else
	return size ? get_order(size) : 0;
#endif
}

static void binder_proc_init(binder_proc_t *that);
static void binder_proc_spawn_looper(binder_proc_t *that);
static void binder_proc_wakeup_timer(unsigned long);
static void binder_proc_idle_timer(unsigned long);

static void set_thread_priority(pid_t thread, int nice)
{
	// Do I just poke into nice value of the thread structure?
	// Punt for now.
}


void binder_proc_init(binder_proc_t *that)
{
	int i;
	atomic_set(&that->m_primaryRefs, 0);
	atomic_set(&that->m_secondaryRefs, 0);
	init_MUTEX(&that->m_lock);
	spin_lock_init(&that->m_spin_lock);
	init_MUTEX(&that->m_map_pool_lock);
	that->m_threads = NULL;
	INIT_LIST_HEAD(&that->m_waitStack);
	that->m_waitStackCount = 0;
	that->m_wakeupTime = B_INFINITE_TIMEOUT;
	that->m_wakeupPriority = 10;
	init_timer(&that->m_wakeupTimer);
	that->m_wakeupTimer.function = &binder_proc_wakeup_timer;
	that->m_wakeupTimer.data = (unsigned long)that;
	init_timer(&that->m_idleTimer);
	that->m_idleTimer.function = &binder_proc_idle_timer;
	that->m_idleTimer.data = (unsigned long)that;
	that->m_idleTimeout = 5*HZ;
	that->m_replyTimeout = 5*HZ;
	//that->m_idleTimeout = 5*60*HZ;
	//that->m_replyTimeout = 5*60*HZ;
	that->m_syncCount = 0;
	that->m_freeCount = 0;
	that->m_head = NULL;
	that->m_tail = &that->m_head;
	that->m_needFree = NULL;
	that->m_state = 0;
	for (i=0;i<HASH_SIZE;i++) {
		that->m_localHash[i] = NULL;
		that->m_reverseHash[i] = NULL;
	}
	that->m_numRemoteStrongRefs = 0;
	that->m_rootObject = NULL;
	that->m_rootStopsProcess = 0;
	that->m_descriptors = NULL;
	that->m_descriptorCount = 0;
	that->m_waitingThreads = 0;
	that->m_nonblockedThreads = 0;
	that->m_maxThreads = 5;
	that->m_idlePriority = B_REAL_TIME_PRIORITY;
	atomic_set(&that->m_loopingThreads, 0);
#if 0
	that->m_spawningThreads = 0;
#endif
	that->m_rangeMap = RB_ROOT;
	that->m_freeMap = RB_ROOT;
	BND_FIRST_ACQUIRE(binder_proc, that, STRONG, that);
	that->m_eventTransaction = binder_transaction_CreateEmpty();
	binder_transaction_SetEvent(that->m_eventTransaction, TRUE);
	that->m_pool = NULL;
	that->m_pool_active = 0;
}

binder_proc_t *
new_binder_proc()
{
	// allocate a binder_proc_t from the slab allocator
	binder_proc_t *that = (binder_proc_t*)kmalloc(sizeof(binder_proc_t), GFP_KERNEL);
	BND_ASSERT(that != NULL, "failed to allocate binder_proc");
	if(that == NULL)
		return NULL;
	binder_proc_init(that);
	DPRINTF(2, (KERN_WARNING "************* Creating binder_proc %p *************\n", that));
	return that;
}

void
binder_proc_destroy(binder_proc_t *that)
{
	binder_transaction_t *cmd;
	range_map_t *r;
	struct rb_node *n;
	int i;
	
	DPRINTF(2, (KERN_WARNING "************* Destroying binder_proc %p *************\n", that));

	BND_ASSERT(that->m_state & btCleaned, "binder_proc_Die wns not done");
	BND_ASSERT(!(that->m_state & btFreed), "already free");
	if(that->m_state & btFreed)
		return;
	
	while ((cmd = that->m_needFree)) {
		that->m_needFree = cmd->next;
		binder_transaction_Destroy(cmd);
	}
	
	for (i=0; i<HASH_SIZE; i++) {
		BND_ASSERT(that->m_localHash[i] == NULL, "Leaking some local mappings!");
		BND_ASSERT(that->m_reverseHash[i] == NULL, "Leaking some reverse mappings!");
	}
	
	// Free up any items in the transaction data pool.
	BND_LOCK(that->m_map_pool_lock);
	n = rb_first(&that->m_rangeMap);
	while (n) {
		r = rb_entry(n, range_map_t, rm_rb);
		n = rb_next(n);

		rb_erase(&r->rm_rb, &that->m_rangeMap);
		//__free_pages(r->page, calc_order_from_size(r->end - r->start));
		kmem_cache_free(range_map_cache, r);
	}
	n = rb_first(&that->m_freeMap);
	while (n) {
		r = rb_entry(n, range_map_t, rm_rb);
		n = rb_next(n);
		rb_erase(&r->rm_rb, &that->m_rangeMap);
		kmem_cache_free(range_map_cache, r);
	}
	BND_UNLOCK(that->m_map_pool_lock);

	// free_lock(&that->m_lock);
	that->m_state |= btFreed;
	kfree(that);
}

void
binder_proc_SetRootObject(binder_proc_t *that, struct binder_node *node)
{
	BND_LOCK(that->m_lock);
	if (that->m_rootObject == NULL) that->m_rootObject = node;
	BND_UNLOCK(that->m_lock);
}

void
binder_proc_Stop(binder_proc_t *that, bool now)
{
	bool goodbye;
	
	DBLOCK((KERN_WARNING "binder_proc_Stop() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);
	
	that->m_rootStopsProcess = TRUE;
	goodbye = now || that->m_rootObject == (binder_node_t*)-1;
	
	BND_UNLOCK(that->m_lock);
	
	if (goodbye) binder_proc_Die(that, FALSE);
}

bool
binder_proc_AddThread(binder_proc_t *that, binder_thread_t *t)
{
	BND_FIRST_ACQUIRE(binder_thread, t, STRONG, 0);
	BND_LOCK(that->m_lock);
	if (binder_proc_IsAlive(that)) {
		t->next = that->m_threads;
		that->m_threads = t;
		BND_UNLOCK(that->m_lock);
	} else {
		BND_UNLOCK(that->m_lock);
		BND_RELEASE(binder_thread, t, STRONG, that);
		t = NULL;
	}
	DBSHUTDOWN((KERN_WARNING "%s(%p): %p\n", __func__, that, t));
	return t != NULL;
}

void
binder_proc_RemoveThread(binder_proc_t *that, binder_thread_t *t)
{
	binder_thread_t **thread;
	DBSHUTDOWN((KERN_WARNING "%s(%p): %p\n", __func__, that, t));
	DBLOCK((KERN_WARNING "RemoveThread() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);
	for (thread = &that->m_threads; *thread && *thread != t; thread = &(*thread)->next)
		;
	if (*thread) {
		*thread = (*thread)->next;
	} else {
		DPRINTF(5, (KERN_WARNING "binder_team %p: RemoveThread of %d does not exist\n", that, t->m_thid));
	}
	
	// If this is the last thread, the team is dead.
	if (!that->m_threads) binder_proc_Die(that, TRUE);
	else BND_UNLOCK(that->m_lock);
}

void 
binder_proc_Released(binder_proc_t *that)
{
	DBSHUTDOWN((KERN_WARNING "%s(%p)\n", __func__, that));
	binder_proc_Die(that, FALSE);
}

void 
binder_proc_Die(binder_proc_t *that, bool locked)
{
	binder_transaction_t *cmd;
	local_mapping_t *lm;
	reverse_mapping_t *rm;
	binder_node_t *n;
	binder_thread_t *thr;
	descriptor_t *descriptors;
	bool dying;
	bool first;
	binder_transaction_t *cmdHead;
	s32 descriptorCount;
	binder_thread_t *threads;
	int i;
	local_mapping_t *localMappings;
	reverse_mapping_t *reverseMappings;
	bool acquired; 

	DBSHUTDOWN((KERN_WARNING "*****************************************\n"));
	DBSHUTDOWN((KERN_WARNING "**** %s(%p, %s)\n", __func__, that, locked ? "locked" : "unlocked"));
	
	// Make sure our destructor doesn't get called until Die() is done.
	BND_ACQUIRE(binder_proc, that, WEAK, that);
	
	// Make sure that Released() doesn't get called if we are dying
	// before all primary references have been removed.
	acquired = BND_ATTEMPT_ACQUIRE(binder_proc, that, STRONG, that);
	
	if (!locked) {
		DBLOCK((KERN_WARNING "%s() going to lock %p in %d\n", __func__, that, current->pid));
		BND_LOCK(that->m_lock);
	}
	dying = that->m_state&btDying;
	that->m_state |= btDying;
	BND_UNLOCK(that->m_lock);
	
	if (dying) {
		DBSHUTDOWN((KERN_WARNING "racing to kill %p\n", that));
		while (!(that->m_state&btDead)) msleep(10);
		BND_RELEASE(binder_proc, that, WEAK, that);
		if (acquired) BND_RELEASE(binder_proc, that, STRONG, that);
		DBSHUTDOWN((KERN_WARNING "race finished\n"));
		return;
	}
	
	/*
	DPRINTF(5, (KERN_WARNING "Binder team %p: removing from driver.\n", that));
	remove_team(that->tgid);
	delete_sem(that->m_spawnerSem);
	that->m_spawnerSem = B_BAD_SEM_ID;
	*/
	
	DBLOCK((KERN_WARNING "%s() #2 going to lock %p in %d\n", __func__, that, current->pid));
	BND_LOCK(that->m_lock);

	// Remove team associated with pending free transactions, so
	// they no longer hold a secondary reference on us.  We must
	// keep them around because there could still be loopers using
	// the data, if they kept the parcel after replying.
	first = TRUE;
	cmd = that->m_needFree;
	while (cmd) {
		if (first) {
			first = FALSE;
			DBSHUTDOWN((KERN_WARNING "Binder team %p: detaching free transactions.\n", that));
		}
		DBSHUTDOWN((KERN_WARNING "Detaching transaction %p from thread %p (%d) to thread %p (%d) node %p\n",
					cmd, cmd->sender, cmd->sender ? binder_thread_Thid(cmd->sender) : -1,
					cmd->receiver, cmd->receiver ? binder_thread_Thid(cmd->receiver) : -1,
					cmd->target));
		binder_transaction_ReleaseTeam(cmd);
		cmd = cmd->next;
	}

	// Now collect everything we have to clean up.  We don't want to
	// do stuff on these until after our own lock is released, to avoid
	// various horrible deadlock situations.

	del_timer_sync(&that->m_wakeupTimer);
	del_timer_sync(&that->m_idleTimer);
	
	cmdHead = that->m_head;
	that->m_head = NULL;
	that->m_tail = &that->m_head;
	cmd = cmdHead;
	while (cmd) {
		// If a pending transaction is the event transaction, remove
		// our global pointer so that nobody else tries to use it.
		if (cmd == that->m_eventTransaction) that->m_eventTransaction = NULL;
		cmd = cmd->next;
	}
	
	//DPRINTF(5, (KERN_WARNING "Binder team %p: collecting mappings.\n", that));
	lm = localMappings = NULL;
	rm = reverseMappings = NULL;
	for (i=0;i<HASH_SIZE;i++) {
		if (that->m_localHash[i]) {
			// mark the front of the list
			if (!localMappings) lm = localMappings = that->m_localHash[i];
			// or tack this chain on the end
			else lm->next = that->m_localHash[i];
			// run to the end of the chain
			while (lm->next) lm = lm->next;
			// mark this chain handled
			that->m_localHash[i] = NULL;
		}
		if (that->m_reverseHash[i]) {
			// ditto for reverse mappings
			if (!reverseMappings) rm = reverseMappings = that->m_reverseHash[i];
			else rm->next = that->m_reverseHash[i];
			while (rm->next) rm = rm->next;
			that->m_reverseHash[i] = NULL;
		}
	}

	descriptors = that->m_descriptors;
	descriptorCount = that->m_descriptorCount;
	that->m_descriptors = NULL;
	that->m_descriptorCount = 0;
	
	threads = that->m_threads;
	that->m_threads = NULL;
	for (thr = threads; thr != NULL; thr = thr->next) BND_ACQUIRE(binder_thread, thr, WEAK, that);
	
	that->m_state |= btDead;
	
	BND_UNLOCK(that->m_lock);
	
	// Now do all the cleanup!
	
	first = TRUE;
	while ((thr = threads)) {
		if (first) {
			first = FALSE;
			DBSHUTDOWN((KERN_WARNING "Binder team %p: removing remaining threads.\n", that));
		}
		threads = thr->next;
		DBSHUTDOWN((KERN_WARNING "Killing thread %p (%d)\n", thr, binder_thread_Thid(thr)));
		binder_thread_Die(thr);
		BND_RELEASE(binder_thread, thr, WEAK, that);
	}

	first = TRUE;
	while ((cmd = cmdHead)) {
		if (first) {
			first = FALSE;
			DBSHUTDOWN((KERN_WARNING "Binder team %p: cleaning up pending commands.\n", that));
		}
		if (cmd->sender) {
			DBSHUTDOWN((KERN_WARNING "Returning transaction %p to thread %p (%d)\n", cmd, cmd->sender, binder_thread_Thid(cmd->sender)));
			binder_thread_ReplyDead(cmd->sender);
		}
		cmdHead = cmd->next;
		binder_transaction_Destroy(cmd);
	}

	first = TRUE;
	while ((lm = localMappings)) {
		if (first) {
			first = FALSE;
			DBSHUTDOWN((KERN_WARNING "Binder team %p: cleaning up local mappings.\n", that));
		}
		localMappings = lm->next;
		// FIXME: send death notification
		kmem_cache_free(local_mapping_cache, lm);
	}
	
	first = TRUE;
	while ((rm = reverseMappings)) {
		if (first) {
			first = FALSE;
			DBSHUTDOWN((KERN_WARNING "Binder team %p: cleaning up reverse mappings.\n", that));
		}
		reverseMappings = rm->next;
		DBSHUTDOWN((KERN_WARNING "Removed reverse mapping from node %p to descriptor %d\n",
					rm->node, rm->descriptor+1));
		// FIXME: decrement use count and possibly notify owner.  It seems like we do this below.
		kmem_cache_free(reverse_mapping_cache, rm);
	}

	first = TRUE;
	if (descriptors) {
		int i;
		for (i=0;i<descriptorCount;i++) {
			if ((n = descriptors[i].node)) {
				if (first) {
					first = FALSE;
					DBSHUTDOWN((KERN_WARNING "Binder team %p: cleaning up descriptors.\n", that));
				}
				if (descriptors[i].priRef || descriptors[i].secRef) {
					// If we host this node, mark the node's m_team NULL so
					// that other teams can handle obituaries.
				}
				if (descriptors[i].priRef) {
					#if VALIDATES_BINDER
					const s32 old = BND_RELEASE(binder_node, n, STRONG, that);
					DBSHUTDOWN((KERN_WARNING "Released primary reference on node %p (descr %ld), %ld remain.\n",
							n, i+1, old-1));
					#else
					BND_RELEASE(binder_node, n, STRONG, that);
					#endif
				}
				if (descriptors[i].secRef) {
					#if VALIDATES_BINDER
					const s32 old = BND_RELEASE(binder_node, n, WEAK, that);
					DBSHUTDOWN((KERN_WARNING "Released weak reference on node %p (descr %ld), %ld remain.\n",
							n, i+1, old-1));
					#else
					BND_RELEASE(binder_node, n, WEAK, that);
					#endif
				}
			}
		}
		kfree(descriptors);
	}
	
	if (that->m_eventTransaction) binder_transaction_Destroy(that->m_eventTransaction);
	that->m_eventTransaction = NULL;

	DBSHUTDOWN((KERN_WARNING "Binder process %p: DEAD!\n", that));

	BND_ASSERT(that->m_head == NULL, "that->m_head != NULL");
	
	that->m_state |= btCleaned;
	BND_RELEASE(binder_proc, that, WEAK, that);
	if (acquired) BND_RELEASE(binder_proc, that, STRONG, that);

	DBSHUTDOWN((KERN_WARNING "**** %s(%p, %s) done dying!\n", __func__, that, locked ? "locked" : "unlocked"));
	DBSHUTDOWN((KERN_WARNING "*****************************************\n"));
}

status_t 
binder_proc_AddToNeedFreeList(binder_proc_t *that, binder_transaction_t *t)
{
	BND_ACQUIRE(binder_proc, that, WEAK, that);
	
	binder_transaction_ReleaseTarget(t);
	
	DBLOCK((KERN_WARNING "AddToNeedFreeList() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);
	DPRINTF(2, (KERN_WARNING "AddToNeedFreeList %p for team %p\n",t,that));
	if (!binder_proc_IsAlive(that)) {
		// Don't call this with lock held -- it could cause all other
		// sorts of things to happen.
		BND_UNLOCK(that->m_lock);
		binder_transaction_ReleaseTeam(t);
		BND_LOCK(that->m_lock);
	}
	t->next = that->m_needFree;
	that->m_needFree = t;
	that->m_freeCount++;
	BND_UNLOCK(that->m_lock);
	
	BND_RELEASE(binder_proc, that, WEAK, that);
	
	return 0;
}

BND_IMPLEMENT_ACQUIRE_RELEASE(binder_proc);
BND_IMPLEMENT_ATTEMPT_ACQUIRE(binder_proc);

s32 
binder_proc_Node2Descriptor(binder_proc_t *that, binder_node_t *n, bool ref, s32 type)
{
	s32 desc=-2;
	reverse_mapping_t **head;

	DPRINTF(4, (KERN_WARNING "%s(%p, %p, %s, %s)\n", __func__, that, n, ref ? "true" : "false", type == STRONG ? "STRONG" : "WEAK"));
	BND_LOCK(that->m_lock);

	if (binder_proc_IsAlive(that)) {
		u32 bucket = hash_ptr(n, HASH_BITS);
		DPRINTF(5, (KERN_WARNING " -- node(%p) mapping to descr bucket %d\n",n,bucket));
		head = &that->m_reverseHash[bucket];
		while (*head && (n < (*head)->node)) head = &(*head)->next;
		if (*head && (n == (*head)->node)) {
			desc = (*head)->descriptor;
			DPRINTF(5, (KERN_WARNING "node(%p) found map to descriptor(%d), strong=%d\n",n,desc+1,that->m_descriptors[desc].priRef));
			if (!ref || type == WEAK || that->m_descriptors[desc].priRef > 0
					|| BND_ATTEMPT_ACQUIRE(binder_node, n, STRONG, that)) {
				if (ref) {
					DPRINTF(5, (KERN_WARNING "Incrementing descriptor %d %s: strong=%d weak=%d in team %p\n", desc+1, type == STRONG ? "STRONG" : "WEAK", that->m_descriptors[desc].priRef, that->m_descriptors[desc].secRef, that));
					if (type == STRONG) that->m_descriptors[desc].priRef++;
					else that->m_descriptors[desc].secRef++;
				}
				DPRINTF(5, (KERN_WARNING "node(%p) mapped to descriptor(%d) in team %p\n",n,desc+1,that));
			} else {
				// No longer exists!
				desc = -2;
			}
		} else if (ref && (type != STRONG || BND_ATTEMPT_ACQUIRE(binder_node, n, STRONG, that))) {
			reverse_mapping_t *map;
			int i;
			if (type != STRONG) BND_ACQUIRE(binder_node, n, WEAK, that);
			for (i=0;i<that->m_descriptorCount;i++) {
				if (that->m_descriptors[i].node == NULL) {
					that->m_descriptors[i].node = n;
					if (type == STRONG) {
						that->m_descriptors[i].priRef = 1;
						that->m_descriptors[i].secRef = 0;
					} else {
						that->m_descriptors[i].priRef = 0;
						that->m_descriptors[i].secRef = 1;
					}
					desc = i;
					// DPRINTF(5, (KERN_WARNING "Initializing descriptor %d: strong=%d weak=%d in team %p\n", i+1, that->m_descriptors[i].priRef,that->m_descriptors[i].secRef,that));
					DPRINTF(5, (KERN_WARNING "node(%p) mapped to NEW descriptor(%d) in team %p\n",n,desc+1,that));
					break;
				}
			}
	
			if (desc < 0) {
				int i;
				s32 newCount = that->m_descriptorCount*2;
				if (!newCount) newCount = 32;
				// that->m_descriptors = (descriptor_t*)kernel_realloc(that->m_descriptors,sizeof(descriptor_t)*newCount,"descriptors");
				{
					descriptor_t *d = kmalloc(sizeof(descriptor_t)*newCount, GFP_KERNEL);
					// FIXME: BeOS code did not deal with allocation failures
					memcpy(d, that->m_descriptors, that->m_descriptorCount*sizeof(descriptor_t));
					kfree(that->m_descriptors);
					that->m_descriptors = d;
				}
				for (i=newCount-1;i>=that->m_descriptorCount;i--) that->m_descriptors[i].node = NULL;
				desc = that->m_descriptorCount;
				DPRINTF(5, (KERN_WARNING "Initializing descriptor %d: strong=%d weak=%d in team %p\n", desc+1, that->m_descriptors[desc].priRef,that->m_descriptors[desc].secRef,that));
				that->m_descriptors[desc].node = n;
				if (type == STRONG) {
					that->m_descriptors[desc].priRef = 1;
					that->m_descriptors[desc].secRef = 0;
				} else {
					that->m_descriptors[desc].priRef = 0;
					that->m_descriptors[desc].secRef = 1;
				}
				that->m_descriptorCount = newCount;
				DPRINTF(5, (KERN_WARNING "node(%p) mapped to NEW descriptor(%d) in team %p\n",n,desc+1,that));
			}
	
			map = (reverse_mapping_t*)kmem_cache_alloc(reverse_mapping_cache, GFP_KERNEL);
			map->node = n;
			map->descriptor = desc;
			map->next = *head;
			*head = map;
		}
	}
	
	BND_UNLOCK(that->m_lock);
	return desc+1;
}

binder_node_t *
binder_proc_Descriptor2Node(binder_proc_t *that, s32 descriptor, const void* id, s32 type)
{
	binder_node_t *n;
	(void)id;
	
	descriptor--;

	DBLOCK((KERN_WARNING "Descriptor2Node() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);

	n = NULL;
	if (binder_proc_IsAlive(that)) {
		if ((descriptor >= 0) &&
			(descriptor < that->m_descriptorCount) &&
			(that->m_descriptors[descriptor].node != NULL)) {
			if (type == STRONG) {
				if (that->m_descriptors[descriptor].priRef > 0) {
					n = that->m_descriptors[descriptor].node;
					BND_ACQUIRE(binder_node, n, STRONG, id);
				} else {
					UPRINTF((KERN_WARNING "Descriptor2Node failed primary: desc=%d, max=%d, node=%p, strong=%d\n",
						descriptor+1, that->m_descriptorCount,
						that->m_descriptors[descriptor].node,
						that->m_descriptors[descriptor].priRef));
				}
			} else {
				if (that->m_descriptors[descriptor].secRef > 0) {
					n = that->m_descriptors[descriptor].node;
					BND_ACQUIRE(binder_node, n, WEAK, id);
				} else {
					UPRINTF((KERN_WARNING "Descriptor2Node failed secondary: desc=%d, max=%d, node=%p, weak=%d\n",
						descriptor+1, that->m_descriptorCount,
						that->m_descriptors[descriptor].node ,
						that->m_descriptors[descriptor].secRef));
				}
			}
		} else {
			UPRINTF((KERN_WARNING "Descriptor2Node failed: desc=%d, max=%d, node=%p, strong=%d\n",
				descriptor+1, that->m_descriptorCount,
				(descriptor >= 0 && descriptor < that->m_descriptorCount) ? that->m_descriptors[descriptor].node : NULL,
				(descriptor >= 0 && descriptor < that->m_descriptorCount) ? that->m_descriptors[descriptor].priRef : 0));
		}
	}

	BND_UNLOCK(that->m_lock);
	return n;
}

status_t 
binder_proc_Ptr2Node(binder_proc_t *that, void *ptr, void *cookie, binder_node_t **n, iobuffer_t *io, const void* id, s32 type)
{
	u32 bucket;
	local_mapping_t **head;
	local_mapping_t *newMapping;
	(void)id;
	
	if (ptr == NULL) {
		DPRINTF(5, (KERN_WARNING "ptr(%p) mapping to NULL node in team %p\n",ptr,that));
		*n = NULL;
		return 0;
	}
	
	DBLOCK((KERN_WARNING "Ptr2Node() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);

	if (!binder_proc_IsAlive(that)) {
		BND_UNLOCK(that->m_lock);
		return -ENOENT;
	}
	
	bucket = hash_ptr(ptr, HASH_BITS);
	DPRINTF(9, (KERN_WARNING "ptr(%p) mapping to ptr bucket %u (value %p) in team %p\n",ptr,bucket,that->m_localHash[bucket],that));
	head = &that->m_localHash[bucket];
	while (*head && (ptr < (*head)->ptr)) head = &(*head)->next;
	if (*head && (ptr == (*head)->ptr)) {
		if ((type == STRONG) && BND_ATTEMPT_ACQUIRE(binder_node, (*head)->node, STRONG, id)) {
			*n = (*head)->node;
			DPRINTF(4, (KERN_WARNING "%s(%p, %p, %s): %p (OLD)\n", __func__, that, ptr, type == STRONG ? "STRONG" : "WEAK", *n));
			BND_UNLOCK(that->m_lock);
			return 0;
		} else if (BND_ATTEMPT_ACQUIRE(binder_node, (*head)->node, WEAK, id)) {
			if((*head)->next)
				BND_ASSERT(io || (*head)->next->ptr != ptr || atomic_read(&((*head)->next->node->m_secondaryRefs)) == 0, "May remove wrong node");

			*n = (*head)->node;
			DPRINTF(4, (KERN_WARNING "%s(%p, %p, %s): %p (OLD)\n", __func__, that, ptr, type == STRONG ? "STRONG" : "WEAK", *n));
			if (type == STRONG) {
				/*	Other teams have a secondary reference on this node, but no
					primary reference.  We need to make the node alive again, and
					tell the calling team that the driver now has a primary
					reference on it.  The two calls below will force a new primary
					reference on the node, and remove the secondary reference we
					just acquired above.  All the trickery with the secondary reference
					is protection against a race condition where another team removes
					the last secondary reference on the object, while we are here
					trying to add one.
				*/
				int count;
				DPRINTF(9, (KERN_WARNING "Apply a new primary reference to node (%p) in team %p\n",*n,that));
				count = BND_FORCE_ACQUIRE(binder_node, *n, id);
				BND_RELEASE(binder_node, *n, WEAK, id);
				
				BND_ASSERT(io != NULL, "Acquiring new strong reference without io");
				if (count == 0) {
					that->m_numRemoteStrongRefs++;
					if (io) {
						BND_ACQUIRE(binder_node, *n, STRONG, that); // add a second reference to avoid the node being released before the aquire has finished
						iobuffer_write_u32(io, brACQUIRE);
						iobuffer_write_void(io, ptr);
						iobuffer_write_void(io, (*head)->cookie);
						DPRINTF(5, (KERN_WARNING " -- wrote brACQUIRE: %p\n", ptr));
					}
				}
				else {
					printk(KERN_WARNING "%s(%p, %p, %s): %p Reaquired strong reference, but someone beat us to it\n", __func__, that, ptr, type == STRONG ? "STRONG" : "WEAK", (*head)->node);
				}
			}
			BND_UNLOCK(that->m_lock);
			return 0;
		}
#if 1
		else {
			DPRINTF(4, (KERN_WARNING "%s(%p, %p, %s): %p (OLD) FAILED AttempAcquire!\n", __func__, that, ptr, type == STRONG ? "STRONG" : "WEAK", (*head)->node));
		}
#endif
	}

	{
		local_mapping_t **thead;
		thead = &that->m_localHash[hash_ptr(ptr, HASH_BITS)];
		while (*thead) {
			if((*thead)->ptr == ptr) {
				BND_ASSERT(atomic_read(&((*head)->node->m_primaryRefs)) == 0, "Creating new node when a node with strong refs already exists");
				BND_ASSERT(atomic_read(&((*head)->node->m_secondaryRefs)) == 0, "Creating new node when a node with weak refs already exists");
			}
			thead = &(*thead)->next;
		}
	}

	if (io && (iobuffer_remaining(io) < 8)) {
		BND_UNLOCK(that->m_lock);
		return -EINVAL;
	}

	newMapping = (local_mapping_t*)kmem_cache_alloc(local_mapping_cache, GFP_KERNEL);
	newMapping->ptr = ptr;
	newMapping->cookie = cookie;
	newMapping->node = binder_node_init(that,ptr,cookie);
	*n = newMapping->node;
	DPRINTF(4, (KERN_WARNING "%s(%p, %p, %s): %p (NEW)\n", __func__, that, ptr, type == STRONG ? "STRONG" : "WEAK", *n));
	BND_FIRST_ACQUIRE(binder_node, *n, type, id);
	newMapping->next = *head;
	*head = newMapping;
		
	if (io) {
		if (type == STRONG) {
			BND_ACQUIRE(binder_node, *n, STRONG, that); // add a second reference to avoid the node being released before the aquire has finished
			that->m_numRemoteStrongRefs++;
			iobuffer_write_u32(io, brACQUIRE);
			iobuffer_write_void(io, ptr);
			iobuffer_write_void(io, cookie);
			DPRINTF(5, (KERN_WARNING " -- wrote brACQUIRE: %p\n", ptr));
		}
		BND_ACQUIRE(binder_node, *n, WEAK, that); // add a second reference to avoid the node being released before the aquire has finished
		iobuffer_write_u32(io, brINCREFS);
		iobuffer_write_void(io, ptr);
		iobuffer_write_void(io, cookie);
		DPRINTF(5, (KERN_WARNING " -- wrote brINCREFS: %p\n", ptr));
	}
	else {
		if (type == STRONG)
			printk(KERN_WARNING "%s() creating new node without brACQUIRE\n", __func__);
		else
			printk(KERN_WARNING "%s() creating new node without brINCREFS\n", __func__);
	}

	BND_UNLOCK(that->m_lock);
	return 0;
}

bool 
binder_proc_RefDescriptor(binder_proc_t *that, s32 descriptor, s32 type)
{
	bool r=FALSE;

	descriptor--;

	DBLOCK((KERN_WARNING "RefDescriptor() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);

	if (binder_proc_IsAlive(that)) {
		descriptor_t *d;
		if ((descriptor >= 0) &&
			(descriptor < that->m_descriptorCount) &&
			((d=&that->m_descriptors[descriptor])->node != NULL)) {
			r = TRUE;
			DPRINTF(5, (KERN_WARNING "Incrementing descriptor %d %s: strong=%d weak=%d in team %p\n", descriptor+1, type == STRONG ? "STRONG" : "WEAK", d->priRef,d->secRef,that));
			if (type == STRONG) {
				if (d->priRef > 0) d->priRef++;
				else {
					UPRINTF((KERN_WARNING "No strong references exist for descriptor: desc=%d, max=%d, node=%p, weak=%d\n",
						descriptor+1, that->m_descriptorCount,
						(descriptor >= 0 && descriptor < that->m_descriptorCount) ? that->m_descriptors[descriptor].node : NULL,
						(descriptor >= 0 && descriptor < that->m_descriptorCount) ? that->m_descriptors[descriptor].secRef : 0));
					r = FALSE;
				}
			} else if (type == WEAK) {
				if (d->secRef > 0) d->secRef++;
				else if (d->priRef > 0) {
					// Note that we allow the acquisition of a weak reference if only holding
					// a strong because for transactions we only increment the strong ref
					// count when sending a strong reference...  so we need to be able to recover
					// weak reference here.
					d->secRef++; BND_ACQUIRE(binder_node, d->node, WEAK, that);
				} else {
					UPRINTF((KERN_WARNING "No weak references exist for descriptor: desc=%d, max=%d, node=%p, strong=%d\n",
						descriptor+1, that->m_descriptorCount,
						(descriptor >= 0 && descriptor < that->m_descriptorCount) ? that->m_descriptors[descriptor].node : NULL,
						(descriptor >= 0 && descriptor < that->m_descriptorCount) ? that->m_descriptors[descriptor].priRef : 0));
					r = FALSE;
				}
			}
		}
	}

	BND_UNLOCK(that->m_lock);
	return r;
}

bool 
binder_proc_UnrefDescriptor(binder_proc_t *that, s32 descriptor, s32 type)
{
	binder_node_t *n = NULL;
	bool r=FALSE;

	descriptor--;

	DPRINTF(4, (KERN_WARNING "%s(%p, %d, %s)\n", __func__, that, descriptor, type == STRONG ? "STRONG" : "WEAK"));

	BND_LOCK(that->m_lock);

	if (binder_proc_IsAlive(that)) {
		descriptor_t *d;
		bool remove = FALSE;
		if ((descriptor >= 0) &&
			(descriptor < that->m_descriptorCount) &&
			((d=&that->m_descriptors[descriptor])->node != NULL)) {
			r = TRUE;
			DPRINTF(5, (KERN_WARNING "Decrementing descriptor %d %s: strong=%d weak=%d in team %p\n", descriptor+1, type == STRONG ? "STRONG" : "WEAK", d->priRef,d->secRef,that));
			if (type == STRONG) {
				if (--d->priRef == 0) n = d->node;
			} else {
				if (--d->secRef == 0) n = d->node;
			}
			DPRINTF(5, (KERN_WARNING "Descriptor %d is now: strong=%d weak=%d in team %p\n", descriptor+1, d->priRef,d->secRef,that));
			if (n && d->priRef <= 0 && d->secRef <= 0) {
				d->node = NULL;
				remove = TRUE;
			}
		}
	
		if (remove) {
			reverse_mapping_t *entry,**head = &that->m_reverseHash[hash_ptr(n, HASH_BITS)];
			while (*head && (n < (*head)->node)) head = &(*head)->next;
			if (*head && (n == (*head)->node)) {
				entry = *head;
				*head = entry->next;
				kmem_cache_free(reverse_mapping_cache, entry);
			}
		}
	}

	BND_UNLOCK(that->m_lock);
	if (n) BND_RELEASE(binder_node, n, type, that);
	return r;
}

bool 
binder_proc_RemoveLocalMapping(binder_proc_t *that, void *ptr, struct binder_node *node)
{
	local_mapping_t *entry=NULL;
	
	DBLOCK((KERN_WARNING "RemoveLocalMapping() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);

	if (binder_proc_IsAlive(that)) {
		local_mapping_t **head;
		DPRINTF(5, (KERN_WARNING "RemoveLocalMapping %p in team %p\n", ptr, that));
		head = &that->m_localHash[hash_ptr(ptr, HASH_BITS)];
		while (*head) {
//			(KERN_WARNING "RemoveLocalMapping %08x %08x\n",ptr,(*head)->ptr);
			if (ptr >= (*head)->ptr && ((*head)->node == node || ptr > (*head)->ptr))
				break;
			head = &(*head)->next;
		}
	
//		while (*head && (ptr <= (*head)->ptr)) head = &(*head)->next;
		if (*head && (ptr == (*head)->ptr)) {
			entry = *head;
			*head = entry->next;
		}
		BND_ASSERT(entry != NULL, "RemoveLocalMapping failed for live process");
		if(entry == NULL) {
			head = &that->m_localHash[hash_ptr(ptr, HASH_BITS)];
			while (*head) {
				if((*head)->node == node)
					break;
				head = &(*head)->next;
			}
			if(*head != NULL)
				printk(KERN_WARNING "RemoveLocalMapping failed, but exists in the wrong place, ptr = %p node = %p node->ptr = %p\n", ptr, node, (*head)->ptr);
		}
	}

	BND_UNLOCK(that->m_lock);
	
	if (entry) {
		kmem_cache_free(local_mapping_cache, entry);
//		(KERN_WARNING "RemoveLocalMapping success\n");
		return TRUE;
	}

	DPRINTF(0, (KERN_WARNING "RemoveLocalMapping failed for %p in team %p\n", ptr, that));
	return FALSE;
}

void
binder_proc_RemoveLocalStrongRef(binder_proc_t *that, binder_node_t *node)
{
	bool goodbye;
	
	DBLOCK((KERN_WARNING "RemoveLocalStrongRef() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);
	
	// It is time for this process to go away if:
	// (a) This is the last strong reference on it, and
	// (b) The process published a root object.  (If it didn't publish
	//     a root object, then someone else is responsible for managing its lifetime.)
	goodbye = --that->m_numRemoteStrongRefs == 0 ? (that->m_rootObject != NULL) : FALSE;
	
	// Oh, and also, if the object being released -is- the root object, well that...
	if (that->m_rootObject == node) {
		that->m_rootObject = (binder_node_t*)-1;	// something we know isn't a valid address.
		if (that->m_rootStopsProcess) goodbye = TRUE;
	}
	
	BND_UNLOCK(that->m_lock);
	
	if (goodbye) binder_proc_Die(that, FALSE);
}

void
binder_proc_AddLocalStrongRef(binder_proc_t *that, binder_node_t *node)
{
	DBLOCK((KERN_WARNING "AddLocalStrongRef() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);
	that->m_numRemoteStrongRefs++;
	BND_UNLOCK(that->m_lock);
}

bool 
binder_proc_AttemptRefDescriptor(binder_proc_t *that, s32 descriptor, binder_node_t **out_target)
{
	binder_node_t *n = NULL;
	bool r=FALSE;

	descriptor--;

	DBLOCK((KERN_WARNING "AttemptRefDescriptor() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);

	if (binder_proc_IsAlive(that)) {
		descriptor_t *d;
		if ((descriptor >= 0) &&
			(descriptor < that->m_descriptorCount) &&
			((d=&that->m_descriptors[descriptor])->node != NULL)) {
			r = TRUE;
			DPRINTF(5, (KERN_WARNING "Attempt incrementing descriptor %d primary: strong=%d weak=%d in team %p\n", descriptor+1, d->priRef,d->secRef,that));
			if (d->priRef > 0 || (d->node && BND_ATTEMPT_ACQUIRE(binder_node, d->node, STRONG, that))) {
				d->priRef++;
			} else {
				// If no strong references currently exist, we can't
				// succeed.  Instead return the node this attempt was
				// made on.
				r = FALSE;
				if ((n=d->node) != NULL) BND_ACQUIRE(binder_node, n, WEAK, that);
			}
		}
	}

	BND_UNLOCK(that->m_lock);
	
	*out_target = n;
	return r;
}

void
binder_proc_ForceRefNode(binder_proc_t *that, binder_node_t *node, iobuffer_t *io)
{
	bool recovered = FALSE;
	const s32 descriptor = binder_proc_Node2Descriptor(that, node, FALSE, STRONG) - 1;
	
	DBLOCK((KERN_WARNING "ForceRefNode() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);

	if (binder_proc_IsAlive(that)) {
		descriptor_t *d;
		if ((descriptor >= 0) &&
			(descriptor < that->m_descriptorCount) &&
			((d=&that->m_descriptors[descriptor])->node != NULL)) {
			DPRINTF(5, (KERN_WARNING "Force incrementing descriptor %d: strong=%d weak=%d in team %p\n", descriptor+1, d->priRef, d->secRef,that));
			if (d->priRef == 0) {
				if (BND_FORCE_ACQUIRE(binder_node, node, that) == 0) {
					recovered = TRUE;
				}
			}
			d->priRef++;
		} else {
			BND_ASSERT(FALSE, "ForceRefNode() got invalid descriptor!");
		}
	}

	BND_UNLOCK(that->m_lock);
	
	// If this operation recovered a strong reference on the object, we
	// need to tell its owning process for proper bookkeeping;
	if (recovered) {
		if (binder_node_Home(node) != NULL) {
			binder_proc_AddLocalStrongRef(binder_node_Home(node), node);
		}
	} else {
		iobuffer_write_u32(io, brRELEASE);
		iobuffer_write_void(io, binder_node_Ptr(node));		// binder object token
		iobuffer_write_void(io, binder_node_Cookie(node));	// binder object cookie
	}
}

status_t 
binder_proc_FreeBuffer(binder_proc_t *that, void *ptr)
{
	binder_transaction_t **p,*t;
	DBLOCK((KERN_WARNING "FreeBuffer() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);
	for (p = &that->m_needFree; *p && (binder_transaction_UserData(*p) != ptr); p = &(*p)->next);
	if ((t = *p)) *p = t->next;
	if (t) that->m_freeCount--;
	BND_UNLOCK(that->m_lock);

	if (t) {
		DPRINTF(5, (KERN_WARNING "FreeBuffer %p in team %p, now have %d\n",ptr,that,that->m_freeCount));

		binder_transaction_Destroy(t);
		return 0;
	} else {
		BND_ASSERT(!binder_proc_IsAlive(that), "FreeBuffer failed");
	}
	return -EINVAL;
}

static void
binder_proc_RemoveThreadFromWaitStack(binder_proc_t *that, binder_thread_t *thread)
{
	assert_spin_locked(&that->m_spin_lock);
	BND_ASSERT(!list_empty(&thread->waitStackEntry), "thread not on waitstack");

	list_del_init(&thread->waitStackEntry);
	that->m_waitStackCount--;
	DIPRINTF(0, (KERN_WARNING "%s(%p) popped thread %p from waitStack %d threads left\n", __func__, that, who, that->m_waitStackCount));
	if(thread->idle && that->m_waitStackCount > BND_PROC_MAX_IDLE_THREADS)
		mod_timer(&that->m_idleTimer, that->m_idleTimeout + jiffies);
	else if(that->m_waitStackCount == BND_PROC_MAX_IDLE_THREADS)
		del_timer(&that->m_idleTimer);
}

static void
binder_proc_DeliverTransacton(binder_proc_t *that, binder_transaction_t *t)
{
	binder_thread_t *thread;

	assert_spin_locked(&that->m_spin_lock);
	
	if(!list_empty(&that->m_waitStack)) {
		// TODO: pop thread from wait stack here
		thread = list_entry(that->m_waitStack.next, binder_thread_t, waitStackEntry);
		binder_proc_RemoveThreadFromWaitStack(that, thread);
		BND_ASSERT(thread->nextRequest == NULL, "Thread already has a request!");
		//DBTRANSACT((KERN_WARNING "Delivering transaction %p to thread %d from thread %d!\n",
		//			t, binder_thread_Thid(thread), current->pid));
		thread->nextRequest = t;
		set_thread_priority(binder_thread_Thid(thread), binder_transaction_Priority(t));
		binder_thread_Wakeup(thread);
	}
	else {
		DBSPAWN((KERN_WARNING "%s(%p) empty waitstack\n", __func__, that));
		*that->m_tail = t;
		that->m_tail = &t->next;
	}
}

status_t 
binder_proc_Transact(binder_proc_t *that, binder_transaction_t *t)
{
	binder_thread_t *thread;
	unsigned long flags;
	
	DBLOCK((KERN_WARNING "Transact() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);

	DBTRANSACT((KERN_WARNING "Thread %d transacting %p to team %p, vthid=%d\n",
			current->pid, t, that, t->sender ? binder_thread_VirtualThid(t->sender) : -1));
	
	if (!binder_proc_IsAlive(that)) {
		BND_UNLOCK(that->m_lock);
		if (t->sender) binder_thread_ReplyDead(t->sender);
		binder_transaction_Destroy(t);
		return 0;
	}
	
	BND_ASSERT(t->next == NULL, "Transaction not correctly initialized");
	
	/*	First check if the target team is already waiting on a reply from
		this thread.  If so, we must reflect this transaction directly
		into the thread that is waiting for us.
	*/
	if (t->sender && binder_thread_VirtualThid(t->sender)) {
		for (thread = that->m_threads;
			 thread &&
			 	(binder_thread_VirtualThid(thread) != binder_thread_VirtualThid(t->sender)) &&
			 	(binder_thread_Thid(thread) != binder_thread_VirtualThid(t->sender));
			 thread = thread->next);

		if (thread) {
			/*	Make sure this thread starts out at the correct priority.
				Its user-space looper will restore the old priority when done. */
			set_thread_priority(binder_thread_Thid(thread), binder_transaction_Priority(t));
			BND_UNLOCK(that->m_lock);
			DBTRANSACT((KERN_WARNING "Thread %d reflecting %p!\n", current->pid, t));
			binder_thread_Reflect(thread, t);
			return 0;
		}
	}
	
	spin_lock_irqsave(&that->m_spin_lock, flags);
	/*	Enqueue or deliver this transaction */
	binder_proc_DeliverTransacton(that, t);
	that->m_syncCount++;
	
	BND_ASSERT(that->m_syncCount > 0, "Synchronous transaction count is bad!");
	// that->m_syncCount++;
	
	// DBTRANSACT((KERN_WARNING "Added to team %p queue -- needNewThread=%d, that->m_nonblockedThreads=%d\n", that, needNewThread, that->m_nonblockedThreads));
		
	spin_unlock_irqrestore(&that->m_spin_lock, flags);
	
	if (that->m_nonblockedThreads <= 0) {
		DBSPAWN((KERN_WARNING "*** TRANSACT NEEDS TO SPAWN NEW THREAD!\n"));
		binder_proc_spawn_looper(that);
	}
	
	BND_UNLOCK(that->m_lock);
	
	return 0;
}

status_t 
binder_proc_TakeMeOffYourList(binder_proc_t *that)
{
	DBLOCK((KERN_WARNING "binder_proc_TakeMeOffYourList() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);
	that->m_nonblockedThreads--;
	DBSPAWN((KERN_WARNING "*** TAKE-ME-OFF-YOUR-LIST %p -- now have %d nonblocked\n", that, that->m_nonblockedThreads));
	BND_ASSERT(that->m_nonblockedThreads >= 0, "Nonblocked thread count is bad!");
	if ((that->m_nonblockedThreads <= 0) && that->m_syncCount) {
		/* Spawn a thread if all blocked and synchronous transaction pending */
		DBSPAWN((KERN_WARNING "*** TAKE-ME-OFF-YOUR-LIST NEEDS TO SPAWN NEW THREAD!\n"));
		binder_proc_spawn_looper(that);
	}
	BND_UNLOCK(that->m_lock);
	return 0;
}

status_t 
binder_proc_PutMeBackInTheGameCoach(binder_proc_t *that)
{
	DBLOCK((KERN_WARNING "binder_proc_PutMeBackInTheGameCoach() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);
	BND_ASSERT(that->m_nonblockedThreads >= 0, "Nonblocked thread count is bad!");
	that->m_nonblockedThreads++;
	DBSPAWN((KERN_WARNING "*** PUT-ME-BACK-IN-THE-GAME-COACH %p -- now have %d nonblocked\n", that, that->m_nonblockedThreads));
	BND_UNLOCK(that->m_lock);
	return 0;
}

status_t 
binder_proc_WaitForRequest(binder_proc_t *that, binder_thread_t* who, binder_transaction_t **t)
{
	status_t err = 0;
	unsigned long flags;
	
	DBLOCK((KERN_WARNING "WaitForRequest() going to lock %p in %d\n", that, binder_thread_Thid(who)));
	BND_LOCK(that->m_lock);

	BND_ASSERT(atomic_read(&that->m_lock.count) <= 0, "WaitForRequest() lock still free after BND_LOCK");
	
	if (who->m_isSpawned && who->m_firstLoop) {
		/*	This is a new thread that is waiting for its first time. */
#if 0
		DPRINTF(0, (KERN_WARNING "*** ENTERING SPAWNED THREAD!  Now looping %d, spawning %d\n",
				atomic_read(&that->m_loopingThreads), that->m_spawningThreads));
		that->m_spawningThreads--;
#else
		DPRINTF(0, (KERN_WARNING "*** ENTERING SPAWNED THREAD!  Now looping %d\n", atomic_read(&that->m_loopingThreads)));
#endif
		who->m_firstLoop = FALSE;
	} else {
		/*	This is an existing thread that is going to go back to waiting. */
		that->m_waitingThreads++;
	}

	BND_ASSERT(who->nextRequest == NULL, "Thread already has a request!");
	BND_ASSERT(list_empty(&who->waitStackEntry), "Thread on wait stack!");
	
	/*	Look for a pending request to service.  Only do this if we are not
		yet on the wait stack, or are at the top of the stack -- otherwise,
		we need to wait for the thread on top of us to execute. */
	spin_lock_irqsave(&that->m_spin_lock, flags);
	if((*t = that->m_head) != NULL) {
		DIPRINTF(5, (KERN_WARNING "Processing transaction %p, next is %p\n", *t, (*t)->next));
		that->m_head = (*t)->next;
		if (that->m_tail == &(*t)->next) that->m_tail = &that->m_head;
		(*t)->next = NULL;
		set_thread_priority(binder_thread_Thid(who), binder_transaction_Priority(*t));
	}
	else {
		/*	If there are no pending transactions, unlock the team state and
			wait for next thing to do. */
		
		// Add to wait stack.
		DIPRINTF(5, (KERN_WARNING "Pushing thread %d on to wait stack.\n", binder_thread_Thid(who)));
		#if VALIDATES_BINDER
		binder_thread_t* pos;
		list_for_each_entry(pos, &that->m_waitStack, waitStackEntry) {
			DBSTACK((KERN_WARNING "Thread %ld looking through wait stack: %p (%ld)\n",
					current, pos, binder_thread_Thid(pos)));
			BND_ASSERT(pos != who, "Pushing thread already on wait stack!");
		}
		#endif
		list_add(&who->waitStackEntry, &that->m_waitStack);
		that->m_waitStackCount++;
		DIPRINTF(0, (KERN_WARNING "%s(%p) added thread %p to waitStack %d threads now waiting\n", __func__, that, who, that->m_waitStackCount));
		if(that->m_waitStackCount == BND_PROC_MAX_IDLE_THREADS + 1) {
			mod_timer(&that->m_idleTimer, that->m_idleTimeout + jiffies);
		}
		set_thread_priority(binder_thread_Thid(who), that->m_idlePriority);
		spin_unlock_irqrestore(&that->m_spin_lock, flags);
		
		BND_UNLOCK(that->m_lock);
		err = binder_thread_AcquireIOSem(who);
		DBLOCK((KERN_WARNING "WaitForRequest() #2 going to lock %p in %d\n", that, binder_thread_Thid(who)));
		BND_LOCK(that->m_lock);
		
		//DPRINTF(5, (KERN_WARNING "Thread %d: err=0x%08x, wakeupTime=%Ld\n", binder_thread_Thid(who), err, who->wakeupTime));

		spin_lock_irqsave(&that->m_spin_lock, flags);
		if(err != 0) {
			// wakeup or idle timer may have released the thread
			atomic_set(&who->m_wake_count, 0);
		}
		if ((*t=who->nextRequest) != NULL) {
			/*	A request has been delivered directly to us.  In this
				case the thread has already been removed from the wait
				stack. */
			DPRINTF(1, (KERN_WARNING "Thread %d received transaction %p, err=0x%08x\n", binder_thread_Thid(who), *t, err));
			who->nextRequest = NULL;
			err = 0;
		
		} else {
			/*	The snooze ended without a transaction being returned.
				If the thread ends up returning at this point, we will
				need to pop it off the wait stack.  Make note of that,
				find out what happened, and deal with it.
			*/
		
			DBTRANSACT((KERN_WARNING "Thread %d snooze returned with err=0x%08x\n",
						binder_thread_Thid(who), err));

			if(who->idle) {
				who->idle = FALSE; // the main thread may ignore a request to die
				err = -ETIMEDOUT;
				DBSPAWN((KERN_WARNING "*** TIME TO DIE!  waiting=%d, nonblocked=%d\n",
						that->m_waitingThreads, that->m_nonblockedThreads));
			}
			else {
				BND_ASSERT(err < 0 || !binder_proc_IsAlive(that), "thread woke up without a reason");
				/*	If this thread is still on the wait stack, remove it. */
				DBTRANSACT((KERN_WARNING "Popping thread %d from wait stack.\n",
							binder_thread_Thid(who)));
				binder_proc_RemoveThreadFromWaitStack(that, who);
			}
		}
	}
	spin_unlock_irqrestore(&that->m_spin_lock, flags);
	
	//DBTRANSACT(if ((*t) != NULL) (KERN_WARNING "*** EXECUTING TRANSACTION %p FROM %ld IN %ld\n", *t, (*t)->sender ? binder_thread_Thid((*t)->sender) : -1, binder_thread_Thid(who)));
	
	if ((*t) != NULL) {
		if (!binder_transaction_IsEvent(*t)) {
			/*	Removing a synchronous transaction from the queue */
			BND_ASSERT(that->m_syncCount >= 0, "Count of synchronous transactions is bad!");
			that->m_syncCount--;
		} else {
			BND_ASSERT(*t == that->m_eventTransaction, "Event thread is not the expected instance!");
			
			/*	Tell caller to process an event. */
			who->returnedEventPriority = binder_transaction_Priority(*t);
			err = REQUEST_EVENT_READY;
			*t = NULL;
			
			/*	Clear out current event information. */
			that->m_state &= ~btEventInQueue;
		}
	} else {
		if(err == -ERESTARTSYS) {
			DBTRANSACT((KERN_WARNING "*** NON-TRANSACTION IN %d!  Error=-ERESTARTSYS\n", binder_thread_Thid(who)));
		}
		else {
			DBTRANSACT((KERN_WARNING "*** NON-TRANSACTION IN %d!  Error=0x%08x\n", binder_thread_Thid(who), err));
		}
		// By default (such as errors) run at priority 10.
		set_thread_priority(binder_thread_Thid(who), 10);
	}
	
	#if VALIDATES_BINDER
	{
		binder_thread_t* pos;
		list_for_each_entry(pos, &that->m_waitStack, waitStackEntry) {
			DBSTACK((KERN_WARNING "Thread %d looking through wait stack: %p (%d)\n",
					current, pos, binder_thread_Thid(pos)));
			BND_ASSERT(pos != who, "Thread still on wait stack!");
		}
	}
	#endif
	
	that->m_waitingThreads--;
	
	/*	Spawn a new looper thread if there are no more waiting
		and we have not yet reached our limit. */
#if 1
	if ((that->m_waitingThreads <= 0) && (atomic_read(&that->m_loopingThreads) < that->m_maxThreads)) {
		DBSPAWN((KERN_WARNING "*** I THINK I WANT TO SPAWN A LOOPER THREAD!\n"));
		binder_proc_spawn_looper(that);
	}
#endif
	
	BND_ASSERT(who->nextRequest == NULL, "Thread leaving with a request!");
	BND_ASSERT(list_empty(&who->waitStackEntry), "Thread left on wait stack!");
	
	BND_UNLOCK(that->m_lock);
	
	return err;
}

void
binder_proc_StartLooper(binder_proc_t *that, bool driver_spawned)
{
	DBLOCK((KERN_WARNING "StartLooper() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);
	/*	When the driver spawns a thread, it incremements the non-blocked
		count right away.  Otherwise, we must do it now. */
	if (!driver_spawned) that->m_nonblockedThreads++;
	atomic_inc(&that->m_loopingThreads);
	DPRINTF(0, (KERN_WARNING "*** STARTING A LOOPER FOR %p!  Now have %d waiting, %d nonblocked.\n",
			that, that->m_waitingThreads, that->m_nonblockedThreads));
	BND_UNLOCK(that->m_lock);
}

void
binder_proc_FinishLooper(binder_proc_t *that, bool driverSpawned)
{
	DBLOCK((KERN_WARNING "FinishLooper() going to lock %p in %d\n", that, current->pid));
	BND_LOCK(that->m_lock);
	that->m_nonblockedThreads--;
	DBSPAWN((KERN_WARNING "*** FINISHING A LOOPER FOR %p!  Now have %d waiting, %d nonblocked, %d looping.\n",
			that, that->m_waitingThreads, that->m_nonblockedThreads, atomic_read(&that->m_loopingThreads)));
	if ((that->m_nonblockedThreads <= 1) && that->m_syncCount && binder_proc_IsAlive(that)) {
		/* Spawn a thread if all blocked and synchronous transaction pending */
		DBSPAWN((KERN_WARNING "*** FINISH-LOOPER NEEDS TO SPAWN NEW THREAD!\n"));
		binder_proc_spawn_looper(that);
	}
	BND_UNLOCK(that->m_lock);
	
	if (driverSpawned) {
		atomic_dec(&that->m_loopingThreads);
		BND_ASSERT(atomic_read(&that->m_loopingThreads) >= 0, "Looping thread count is bad!");
	}
}

status_t
binder_proc_SetWakeupTime(binder_proc_t *that, bigtime_t time, s32 priority)
{
	unsigned long flags;
	bool earlier;
	if (time < 0) time = 0;
	// convert to jiffies
	do_div(time, TICK_NSEC);
	time += get_jiffies_64();
	BND_LOCK(that->m_lock);
	DPRINTF(4, (KERN_WARNING "%s(%p, %Ld, %d)\n", __func__, that, time, priority));
	spin_lock_irqsave(&that->m_spin_lock, flags);
	if (time != that->m_wakeupTime && !(that->m_state & btEventInQueue)) {
		DIPRINTF(9, (KERN_WARNING "-- previously %Ld\n", that->m_wakeupTime));
		earlier = time < that->m_wakeupTime;
		that->m_wakeupTime = time;
		mod_timer(&that->m_wakeupTimer, time);
	}
	that->m_wakeupPriority = priority;
	spin_unlock_irqrestore(&that->m_spin_lock, flags);
	BND_UNLOCK(that->m_lock);
	return 0;
}

status_t
binder_proc_SetIdleTimeout(binder_proc_t *that, bigtime_t timeDelta)
{
	DPRINTF(4, (KERN_WARNING "%s(%p, %Ld)\n", __func__, that, timeDelta));
	that->m_idleTimeout = timeDelta;
	return 0;
}

status_t
binder_proc_SetReplyTimeout(binder_proc_t *that, bigtime_t timeDelta)
{
	DPRINTF(4, (KERN_WARNING "%s(%p, %Ld)\n", __func__, that, timeDelta));
	that->m_replyTimeout = timeDelta;
	return 0;
}

status_t
binder_proc_SetMaxThreads(binder_proc_t *that, s32 num)
{
	DPRINTF(4, (KERN_WARNING "%s(%p, %d)\n", __func__, that, num));
	that->m_maxThreads = num;
	return 0;
}

status_t
binder_proc_SetIdlePriority(binder_proc_t *that, s32 pri)
{
	DPRINTF(4, (KERN_WARNING "%s(%p, %d)\n", __func__, that, pri));
	that->m_idlePriority = (pri > 0 ? (pri <= B_REAL_TIME_PRIORITY ? pri : B_REAL_TIME_PRIORITY) : 1);
	return 0;
}

#define LARGE_TRANSACTION (64 * 1024)
static range_map_t * binder_proc_free_map_alloc_l(binder_proc_t *that, size_t length)
{
	bool large;
	struct rb_node *n;
	struct rb_node * (*rbstep)(struct rb_node *);
	range_map_t *rm = NULL;
	unsigned long avail;
	
	large = (length > LARGE_TRANSACTION ? TRUE : FALSE);
	DPRINTF(5, (KERN_WARNING "%s(%p, %08x) large = %d\n", __func__, that, length, large));
	n = large ? rb_last(&that->m_freeMap) : rb_first(&that->m_freeMap);
	rbstep = large ? rb_prev : rb_next;

	while (n) {
		rm = rb_entry(n, range_map_t, rm_rb);
		avail = rm->end - rm->start;
		DPRINTF(5, (KERN_WARNING "%s(%p, %08x) rm = %p [%08lx-%08lx], avail %lu\n", __func__, that, length, rm, rm->start, rm->end, avail));
		if (avail >= length) {
			avail -= length;
			if (avail) {
				range_map_t *newrm = kmem_cache_alloc(range_map_cache, GFP_KERNEL);
				// use only part of range
				if (large) {
					// consume address space from the right
					newrm->end = rm->end;
					rm->end -= length;
					newrm->start = rm->end;
					newrm->page = NULL;
				} else {
					// consume address space from the left
					newrm->start = rm->start;
					rm->start += length;
					newrm->end = rm->start;
				}
				DPRINTF(5, (KERN_WARNING "%s(%p, %08x) newrm = %p [%08lx-%08lx]\n", __func__, that, length, newrm, newrm->start, newrm->end));
				DPRINTF(5, (KERN_WARNING "%s(%p, %08x) remaining rm = %p [%08lx-%08lx], avail %lu\n", __func__, that, length, rm, rm->start, rm->end, avail));
				newrm->team = that;
				rm = newrm;
			} else {
				// use entire range
				rb_erase(n, &that->m_freeMap);
			}
			break;
		}
		n = rbstep(n);
		rm = NULL;
	}
	return rm;
}

range_map_t * binder_proc_free_map_insert(binder_proc_t *that, range_map_t *buffer)
{
	struct rb_node ** p = &that->m_freeMap.rb_node;
	struct rb_node * parent = NULL;
	range_map_t *rm = NULL;
	const unsigned long address = buffer->start;
	struct rb_node *next;
	struct rb_node *prev;

	DPRINTF(0, (KERN_WARNING "%s(%p, %p) %08lx::%08lx\n", __func__, that, buffer, buffer->start, buffer->end));

	while (*p)
	{
		parent = *p;
		rm = rb_entry(parent, range_map_t, rm_rb);

		if (address < rm->start)
			p = &(*p)->rb_left;
		else if (address >= rm->end)
			p = &(*p)->rb_right;
		else {
			DPRINTF(0, (KERN_WARNING "%s found buffer already in the free list!\n", __func__));
			return rm;
		}
	}

	if (rm) {
		if (rm->end == buffer->start) {
			DPRINTF(9, (KERN_WARNING "%s: buffer merges to the right\n", __func__));
			// merge to the right
			rm->end = buffer->end;
			kmem_cache_free(range_map_cache, buffer);
			// try merge right again (did we fill up a hole?)
			next = rb_next(parent);
			if (next) {
				range_map_t *rm_next = rb_entry(next, range_map_t, rm_rb);
				if (rm->end == rm_next->start) {
					DPRINTF(9, (KERN_WARNING "%s: buffer merges to the left, too\n", __func__));
					rm->end = rm_next->end;
					rb_erase(next, &that->m_freeMap);
					kmem_cache_free(range_map_cache, rm_next);
				}
			}
			return NULL;
		} else if (buffer->end == rm->start) {
			DPRINTF(9, (KERN_WARNING "%s: buffer merges to the left\n", __func__));
			// merge to the left
			rm->start = buffer->start;
			kmem_cache_free(range_map_cache, buffer);
			// try merge left again (did we fill up a hole?)
			prev = rb_prev(parent);
			if (prev) {
				range_map_t *rm_prev = rb_entry(prev, range_map_t, rm_rb);
				if (rm_prev->end == rm->start) {
					DPRINTF(9, (KERN_WARNING "%s: buffer merges to the right, too\n", __func__));
					rm->start = rm_prev->start;
					rb_erase(prev, &that->m_freeMap);
					kmem_cache_free(range_map_cache, rm_prev);
				}
			}
			return NULL;
		}
	}
	DPRINTF(9, (KERN_WARNING "%s: buffer stands alone\n", __func__));

	// default case: insert in the middle of nowhere
	rb_link_node(&buffer->rm_rb, parent, p);
	rb_insert_color(&buffer->rm_rb, &that->m_freeMap);

	return NULL;
}

static inline range_map_t * binder_proc_range_map_insert(binder_proc_t *that, range_map_t *buffer)
{
	struct rb_node ** p = &that->m_rangeMap.rb_node;
	struct rb_node * parent = NULL;
	range_map_t *rm;
	const unsigned long address = buffer->start;

	while (*p)
	{
		parent = *p;
		rm = rb_entry(parent, range_map_t, rm_rb);

		if (address < rm->start)
			p = &(*p)->rb_left;
		else if (address >= rm->end)
			p = &(*p)->rb_right;
		else {
			DPRINTF(1, (KERN_WARNING "%s: %p (%08lx::%08lx) overlaps with "
			        "existing entry %p (%08lx::%08lx)\n",
			        __func__, buffer, buffer->start, buffer->end,
			        rm, rm->start, rm->end));
			return rm;
		}
	}

	rb_link_node(&buffer->rm_rb, parent, p);
	rb_insert_color(&buffer->rm_rb, &that->m_rangeMap);

	return NULL;
}

static inline range_map_t * binder_proc_range_map_search(binder_proc_t *that, unsigned long address)
{
	struct rb_node * n = that->m_rangeMap.rb_node;
	range_map_t *rm;
	DPRINTF(0, (KERN_WARNING "%s(%p, %lu)\n", __func__, that, address));

	while (n)
	{
			rm = rb_entry(n, range_map_t, rm_rb);
			// range_map covers [start, end)
			DPRINTF(9, (KERN_WARNING " -- trying %08lx::%08lx\n", rm->start, rm->end));
			if (address < rm->start)
				n = n->rb_left;
			else if (address >= rm->end)
				n = n->rb_right;
			else {
				DPRINTF(9, (KERN_WARNING " -- found it!\n"));
				return rm;
			}
	}
	DPRINTF(0, (KERN_WARNING " -- failed to find containing range\n"));
	return NULL;
}

#if 0
// Remove the buffer containing address from the tree.  The caller owns the returned memory.
static inline range_map_t * binder_proc_range_map_remove(binder_proc_t *that, unsigned long address)
{
	range_map_t *rm = binder_proc_range_map_search(that, address);
	if (rm) rb_erase(&rm->rm_rb, &that->m_rangeMap);
	return rm;
}
#endif

bool
binder_proc_ValidTransactionAddress(binder_proc_t *that, unsigned long address, struct page **pageptr)
{
	// Find the struct page* containing address in the process specified by
	// that.  Return FALSE and leave *pageptr unchanged if address doesn't
	// represent a valid buffer.

	range_map_t *rm;

	BND_LOCK(that->m_map_pool_lock);
	rm = binder_proc_range_map_search(that, address);
	BND_UNLOCK(that->m_map_pool_lock);

	if (rm) {
		unsigned int index = (address - rm->start) >> PAGE_SHIFT;
		*pageptr = rm->page + index;
		BND_ASSERT(rm->next == NULL, "binder_proc_ValidTransactionAddress found page in free pool");
		return TRUE;
	}
	return FALSE;
}

// Alternatively, 2x number of active threads?
#define POOL_THRESHOLD 16
// POOL_BUFFER_LIMIT should never exceed LARGE_TRANSACTION size, or things will get ugly
#define POOL_BUFFER_LIMIT LARGE_TRANSACTION
range_map_t *
binder_proc_AllocateTransactionBuffer(binder_proc_t *that, size_t size)
{
	// ensure order-sized allocations
	unsigned long order = calc_order_from_size(size);

	range_map_t *rm;
	unsigned long avail = ~0;
	range_map_t **prev;

	BND_LOCK(that->m_map_pool_lock);

	rm = that->m_pool;
	prev = &that->m_pool;
	
	size = (1 << order) << PAGE_SHIFT;

	DPRINTF(0, (KERN_WARNING "%s(%p, %u)\n", __func__, that, size));
	DPRINTF(9, (KERN_WARNING " -- order %lu produces size %u\n", order, size));
	// don't bother checking the pool for large buffers
	//if (size < POOL_BUFFER_LIMIT) {
		DPRINTF(9, (KERN_WARNING " -- searching the pool\n"));
		while (rm && ((avail = rm->end - rm->start) < size)) {
			prev = &rm->next;
			rm = rm->next;
		}
	//}

	if (rm && (avail == size)) {
		// unlink
		*prev = rm->next;
		rm->next = NULL;
		// un-count
		that->m_pool_active--;
		DPRINTF(9, (KERN_WARNING " -- reusing transaction buffer\n"));
	} else {
		DPRINTF(9, (KERN_WARNING " -- allocating a new transaction buffer\n"));
		// make a new one
		rm = binder_proc_free_map_alloc_l(that, size);
		if (rm) {
			// allocate RAM for it
			rm->page = alloc_pages(GFP_KERNEL, order);
			if (!rm->page) {
				binder_proc_free_map_insert(that, rm);
				rm = 0;
				DPRINTF(9, (KERN_WARNING " -- allocation failed\n"));
			} else {
				// add to the valid range maps
				rm->next = NULL;
				binder_proc_range_map_insert(that, rm);
			}
		}
	}
	DPRINTF(9, (KERN_WARNING " -- returning %p\n", rm));
	if (rm) {
		DPRINTF(9, (KERN_WARNING " --- %08lx::%08lx\n", rm->start, rm->end));
	}
	BND_UNLOCK(that->m_map_pool_lock);
	return rm;
}

void
binder_proc_FreeTransactionBuffer(binder_proc_t *that, range_map_t *buffer)
{
	unsigned long size = buffer->end - buffer->start;
	range_map_t *rm;
	range_map_t **prev;

	BND_LOCK(that->m_map_pool_lock);

	DPRINTF(5, (KERN_WARNING "%s(%p) m_pool_active: %d, size: %lu\n", __func__, that, that->m_pool_active, size));
	//if ((that->m_pool_active < POOL_THRESHOLD) && (size < POOL_BUFFER_LIMIT)) {
		DPRINTF(5, (KERN_WARNING "%d putting %p (%08lx::%08lx) back in the pool\n", current->pid, buffer, buffer->start, buffer->end));
		rm = that->m_pool;
		prev = &that->m_pool;
		while (rm && ((rm->end - rm->start) < size)) {
			prev = &rm->next;
			rm = rm->next;
		}
		buffer->next = rm;
		*prev = buffer;
		that->m_pool_active++;
#if 0 // This is not safe to enable until we find some way to unmap the page from the userspace
	} else {
		DPRINTF(5, (KERN_WARNING "%d releasing %p (%08lx::%08lx) for later use\n", current->pid, buffer, buffer->start, buffer->end));
		// unmap the range
#if 0
		// FIXME: use unmap_mapping_range() to unmap pages
		// FIXME: "as" always turns up NULL, so unmapping doesn't work
		struct address_space *as = page_mapping(buffer->page);
		DPRINTF(5, (KERN_WARNING " -- address_space: %p\n", as));
		if (as) unmap_mapping_range(as, buffer->start - that->m_mmap_start, buffer->end - buffer->start, 0);
#endif
		// remove from the valid range maps
		rb_erase(&buffer->rm_rb, &that->m_rangeMap);
		// toss this range
		__free_pages(buffer->page, calc_order_from_size(size));
		buffer->page = NULL;
		// give back the address space
		binder_proc_free_map_insert(that, buffer);
	}
#endif
	BND_UNLOCK(that->m_map_pool_lock);
}

/* ALWAYS call this with that->m_lock held */
void binder_proc_spawn_looper(binder_proc_t *that)
{
	DBSPAWN((KERN_WARNING "%s(%p)\n", __func__, that));
#if 0
	if ((++that->m_spawningThreads == 1) && binder_proc_IsAlive(that)) {
		atomic_inc(&that->m_noop_spawner);
		DBSPAWN((KERN_WARNING " -- upped m_noop_spawner to %d\n", atomic_read(&that->m_noop_spawner)));
	}
#else
	if (binder_proc_IsAlive(that) && (test_and_set_bit(SPAWNING_BIT, &that->m_noop_spawner) == 0)) {
		set_bit(DO_SPAWN_BIT, &that->m_noop_spawner);
		DBSPAWN((KERN_WARNING " -- upped m_noop_spawner\n"));
		++that->m_waitingThreads;
		++that->m_nonblockedThreads;
	}
#endif
	DBSPAWN((KERN_WARNING "%s(%p) finished\n", __func__, that));
}

void binder_proc_wakeup_timer(unsigned long data)
{
	unsigned long flags;
	binder_proc_t *that = (binder_proc_t *)data;

	DIPRINTF(0, (KERN_WARNING "%s(%p) -- Enqueueing handler transaction\n", __func__, that));

	spin_lock_irqsave(&that->m_spin_lock, flags);

	BND_ASSERT(that->m_eventTransaction != NULL, "m_eventTransaction == NULL");

	if(!(that->m_state & btEventInQueue)) {
		BND_ASSERT(that->m_eventTransaction->next == NULL, "Event transaction already in queue!");
		binder_transaction_SetPriority(that->m_eventTransaction, (s16)that->m_wakeupPriority);
		that->m_wakeupTime = B_INFINITE_TIMEOUT;
		that->m_wakeupPriority = 10;
		that->m_state |= btEventInQueue;

		binder_proc_DeliverTransacton(that, that->m_eventTransaction);
	}
	else {
		BND_ASSERT(0, "event already in queue");
	}
	spin_unlock_irqrestore(&that->m_spin_lock, flags);
}

void binder_proc_idle_timer(unsigned long data)
{
	unsigned long flags;
	binder_proc_t *that = (binder_proc_t *)data;
	binder_thread_t *thread;

	DIPRINTF(0, (KERN_WARNING "%s(%p) -- Signal idle thread\n", __func__, that));

	spin_lock_irqsave(&that->m_spin_lock, flags);

	if(that->m_waitStackCount > BND_PROC_MAX_IDLE_THREADS) {
		BND_ASSERT(!list_empty(&that->m_waitStack), "bad m_waitStackCount");
		thread = list_entry(that->m_waitStack.prev, binder_thread_t, waitStackEntry);
		thread->idle = TRUE;
		binder_proc_RemoveThreadFromWaitStack(that, thread);
		binder_thread_Wakeup(thread);
	}
	else {
		DBSPAWN((KERN_WARNING "%s(%p) idle timer ignored\n", __func__, that));
	}
	spin_unlock_irqrestore(&that->m_spin_lock, flags);
}

