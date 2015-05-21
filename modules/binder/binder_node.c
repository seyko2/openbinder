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

#include "binder_node.h"
#include "binder_proc.h"
#include "binder_transaction.h"

#define PURGATORY 0
#if PURGATORY
static DECLARE_MUTEX(sem);
static binder_node_t* head = NULL;
static binder_node_t** tail = &head;
static int count = 0;

static void my_free_node(binder_node_t *t)
{
	down(&sem);
	*tail = t;
	tail = (binder_node_t**)&t->m_ptr;
	count++;
	if (count > 20) {
		t = head;
		head = (binder_node_t*)head->m_ptr;
		kmem_cache_free(node_cache, t);
		count--;
	}
	up(&sem);
}
#define ALLOC_NODE kmem_cache_alloc(node_cache, GFP_KERNEL)
#define FREE_NODE(x) my_free_node(x)
#else
#define ALLOC_NODE kmem_cache_alloc(node_cache, GFP_KERNEL)
#define FREE_NODE(x) kmem_cache_free(node_cache, x)
#endif

static atomic_t g_count = ATOMIC_INIT(0);

int
binder_node_GlobalCount()
{
	return atomic_read(&g_count);
}

BND_IMPLEMENT_ACQUIRE_RELEASE(binder_node);
BND_IMPLEMENT_ATTEMPT_ACQUIRE(binder_node);
// BND_IMPLEMENT_FORCE_ACQUIRE(binder_node);

/*
 * For the process which manages the contexts, we treat ptr == NULL specially.
 * In particular, all transactions with a target descriptor of 0 get routed to
 * the manager process and the target pointer the process receives gets set to
 * NULL.  We don't permit any team to send a binder with a NULL ptr, so we can
 * never confuse the mappings.
 */
binder_node_t *binder_node_init(binder_proc_t *team, void *ptr, void *cookie)
{
	binder_node_t *that = ALLOC_NODE;
	atomic_inc(&g_count);
	DPRINTF(5, (KERN_WARNING "%s(team=%p, ptr=%p, cookie=%p): %p\n", __func__,
                team, ptr, cookie, that));
	atomic_set(&that->m_primaryRefs, 0);
	atomic_set(&that->m_secondaryRefs, 0);
	that->m_ptr = ptr;
	that->m_cookie = cookie;
	that->m_home = team;
	if (that->m_home) BND_ACQUIRE(binder_proc, that->m_home, WEAK, that);
	return that;
}

void binder_node_destroy(binder_node_t *that)
{
	atomic_dec(&g_count);
	DPRINTF(4, (KERN_WARNING "%s(%p): ptr=%p, cookie=%p\n", __func__, that,
                that->m_ptr, that->m_cookie));
	if (that->m_home) {
		if (that->m_ptr) {
			binder_proc_t* proc = binder_node_AcquireHome(that, that);
			if (proc) {
				binder_proc_Transact(proc, binder_transaction_CreateRef(tfDecRefs, that->m_ptr, that->m_cookie, proc));
				binder_proc_RemoveLocalMapping(proc, that->m_ptr, that);
				BND_RELEASE(binder_proc, proc, STRONG, that);
			}
		}
		BND_RELEASE(binder_proc, that->m_home, WEAK, that);
	}
	FREE_NODE(that);
}

void 
binder_node_Released(binder_node_t *that)
{
	binder_proc_t* proc = binder_node_AcquireHome(that, that);
	DPRINTF(4, (KERN_WARNING "%s(%p): ptr=%p\n", __func__, that, that->m_ptr));
	if (proc) {
		DPRINTF(5, (KERN_WARNING " -- m_secondaryRefs=%d\n",atomic_read(&that->m_secondaryRefs)));
		binder_proc_Transact(proc, binder_transaction_CreateRef(tfRelease,that->m_ptr,that->m_cookie,proc));
		binder_proc_RemoveLocalStrongRef(proc, that);
		BND_RELEASE(binder_proc, proc, STRONG, that);
	}
}

binder_proc_t*
binder_node_AcquireHome(binder_node_t *that, const void *id)
{
	if (that->m_home && BND_ATTEMPT_ACQUIRE(binder_proc, that->m_home, STRONG, id)) {
		return that->m_home;
	}
	return NULL;
}

status_t
binder_node_Send(binder_node_t *that, struct binder_transaction *t)
{
	binder_proc_t* proc = binder_node_AcquireHome(that, that);
	if (proc) {
		status_t res = binder_proc_Transact(proc, t);
		BND_RELEASE(binder_proc, proc, STRONG, that);
		return res;
	}

	if (t->sender) binder_thread_ReplyDead(t->sender);
	binder_transaction_Destroy(t);
	return 0;
}
