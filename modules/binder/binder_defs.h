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

#ifndef BINDER_DEFS_H
#define BINDER_DEFS_H

#include <asm/atomic.h>
#include <asm/semaphore.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>

#include <linux/version.h>
#include <linux/kobject.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <support_p/binder_module.h>

#if defined(CONFIG_ARM)
/* Define this if you want to use the linux threads hack on ARM */
#define USE_LINUXTHREADS
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12)
#define assert_spin_locked(x)
#endif

extern kmem_cache_t *transaction_cache;
extern kmem_cache_t *thread_cache;
extern kmem_cache_t *node_cache;
extern kmem_cache_t *local_mapping_cache;
extern kmem_cache_t *reverse_mapping_cache;
extern kmem_cache_t *range_map_cache;

#define HASH_BITS 8
#define HASH_SIZE (1 << HASH_BITS)

enum ref_count_type {
	STRONG = 1,
	WEAK = 2
};

/* ------------------------------------------------------------------ */
/* --------------------- COMPILING AS A DRIVER ---------------------- */
/* ------------------------------------------------------------------ */

void soft_yield(void);

#define STOP_ON_ASSERT //msleep_interruptible(1000*60*60*24*7)

#define BND_MEM_DEBUG 1

#if 0

#define VALIDATES_BINDER 0
#define DIPRINTF(level,a) do { if (level <= 9) printk a; } while(0)
#define DPRINTF(level,a) do { if (level <= 9) { printk a; soft_yield();} } while(0)
#define BND_FAIL(msg)
#define BND_ASSERT(cond, msg) do { if (!(cond)) { printk(KERN_WARNING "BND_ASSERT file %s line %d: %s\n", __FILE__, __LINE__, msg); dump_stack(); STOP_ON_ASSERT;} } while (FALSE)
#define DBTRANSACT(x) printk x
#define DBSHUTDOWN(x) printk x
#define DBSPAWN(x) printk x
#define DBSTACK(x) printk x
#define DBLOCK(x) printk x
#define DBREFS(x) printk x
#define DBREAD(x) printk x

#else
#define DIPRINTF(level,a)
#define DPRINTF(level,a)
#define BND_FAIL(msg)
//#define BND_ASSERT(cond, msg) if (!(cond)) printk(KERN_WARNING "BND_ASSERT file %s line %d: %s\n", __FILE__, __LINE__, msg)
#define BND_ASSERT(cond, msg) do { if (!(cond)) { printk(KERN_WARNING "BND_ASSERT file %s line %d: %s\n", __FILE__, __LINE__, msg); dump_stack(); } } while (FALSE)
#define DBTRANSACT(x)
#define DBSHUTDOWN(x) //printk x
#define DBSPAWN(x)
#define DBSTACK(x)
#define DBLOCK(x)
#define DBREFS(x)
#define DBREAD(x)
#endif

// errors triggered by userspace bugs
#define UPRINTF(a) do { printk a; } while(0)
#define BND_UASSERT(cond, msg) if (!(cond)) printk(KERN_WARNING "BND_UASSERT file %s line %d: %s\n", __FILE__, __LINE__, msg)

#if BND_MEM_DEBUG
void *dbg_kmem_cache_alloc(kmem_cache_t *a, unsigned int b);
void dbg_kmem_cache_free(kmem_cache_t *a, void *b);

#define kmem_cache_alloc dbg_kmem_cache_alloc
#define kmem_cache_free dbg_kmem_cache_free
#endif

struct binder_thread;

typedef ssize_t status_t;

typedef unsigned int bool;
#define FALSE (0)
#define TRUE (~FALSE)

/*	Special function, implemented in binder.c, to try to find
	a binder_thread structure for a pid.  If 'create' is TRUE,
	a new structure will be created for you (unattached to
	a process) if it doesn't already exist; otherwise it will
	return NULL.  Returns with a strong reference held on the
	thread.
	
	*** NOTE: Must not call this while holding a thread or
	process lock! */
struct binder_thread *	check_for_thread(pid_t thread_pid, bool create);

/*	Special function, implemented in binder.c, for a parent to
	lookup (or pre-create) the state for main thread of a child
	process it is spawning.  This function calls 
	binder_thread_SetParentThread() for you on the child thread,
	and returns with a strong reference held on the thread.
	
	*** NOTE: Must not call this while holding a thread or
	process lock! */
struct binder_thread *	attach_child_thread(pid_t child_pid, struct binder_thread *parent);

/*	Special function, implemented in binder.c, to remove a
	thread structure from the global list.  This needs to be
	called when using the above two functions to create such
	a structure, to remove it from the list when it is no
	longer used.  A strong reference is removed from the thread
	and, if the strong count goes to zero AND the structure has
	not yet been accessed by its user space thread, then the
	thread structure will be removed from the list.
	
	*** NOTE: Must not call this while holding a thread or
	process lock! */
void					forget_thread(struct binder_thread *thread);

// Perform an accuire/release on an object.
#define BND_ACQUIRE(cname, that, type, id) cname##_Acquire(that, type)
#define BND_ATTEMPT_ACQUIRE(cname, that, type, id) cname##_AttemptAcquire(that, type)
#define BND_FIRST_ACQUIRE(cname, that, type, id) cname##_ForceAcquire(that, type)
#define BND_FORCE_ACQUIRE(cname, that, id) cname##_ForceAcquire(that, STRONG)
#define BND_RELEASE(cname, that, type, id) cname##_Release(that, type)

// Declare acquire/release methods for a class.
#define BND_DECLARE_ACQUIRE_RELEASE(cname)						\
	void cname##_Acquire(cname##_t *that, s32 type);			\
	int cname##_ForceAcquire(cname##_t *that, s32 type);		\
	int cname##_Release(cname##_t *that, s32 type);				\
/**/

// Declare attempt acquire method for a class.
#define BND_DECLARE_ATTEMPT_ACQUIRE(cname)						\
	int cname##_AttemptAcquire(cname##_t *that, s32 type);		\
/**/

extern void dump_stack(void);
// Implement acquire/release methods for a class.
#define BND_IMPLEMENT_ACQUIRE_RELEASE(cname)					\
void															\
cname##_Acquire(cname##_t *that, s32 type)						\
{																\
	int res;													\
	if (type == STRONG) {										\
		res = atomic_inc_return(&that->m_primaryRefs);			\
		BND_ASSERT(res > 1, "STRONG Acquire without strong ref");	\
	}															\
	res = atomic_inc_return(&that->m_secondaryRefs);			\
	if (type == STRONG) {										\
		BND_ASSERT(res > 1, "STRONG Acquire without weak ref");	\
	}															\
	else {														\
		BND_ASSERT(res > 1, "WEAK Acquire without weak ref");	\
	}															\
	DPRINTF(5, (KERN_WARNING "%s(%p, %s) s:%d w:%d\n", __func__, that, type == STRONG ? "STRONG" : "WEAK", atomic_read(&that->m_primaryRefs), atomic_read(&that->m_secondaryRefs)));\
	/*dump_stack()*/;\
}																\
int																\
cname##_ForceAcquire(cname##_t *that, s32 type)					\
{																\
	int res;													\
	res = atomic_inc_return(&that->m_secondaryRefs);			\
	if (type == STRONG) {										\
		res = atomic_inc_return(&that->m_primaryRefs);			\
	}															\
	DPRINTF(5, (KERN_WARNING "%s(%p, %s) s:%d w:%d\n", __func__, that, type == STRONG ? "STRONG" : "WEAK", atomic_read(&that->m_primaryRefs), atomic_read(&that->m_secondaryRefs)));\
	return res-1;												\
}																\
int															\
cname##_Release(cname##_t *that, s32 type)						\
{																\
	int rv1=-2, rv2=-2;                                         \
	DPRINTF(5, (KERN_WARNING "%s(%p, %s) s:%d w:%d\n", __func__, that, type == STRONG ? "STRONG" : "WEAK", atomic_read(&that->m_primaryRefs), atomic_read(&that->m_secondaryRefs)));\
	if(type == STRONG) {                                        \
		BND_ASSERT(atomic_read(&that->m_primaryRefs) > 0, "Strong reference underflow");\
	}                                                           \
	BND_ASSERT(atomic_read(&that->m_secondaryRefs) > 0, "Weak reference underflow");\
	/*dump_stack()*/;\
	switch (type) {												\
		case STRONG:											\
			if ((rv1 = atomic_dec_return(&that->m_primaryRefs)) == 0) {	\
				cname##_Released(that);							\
			}													\
		case WEAK:											\
			if ((rv2 = atomic_dec_return(&that->m_secondaryRefs)) == 0) {\
				cname##_destroy(that);							\
			}													\
	}															\
	return ((type == STRONG) ? rv1 : rv2) + 1;                  \
}																\
/**/

// Implement attempt acquire method for a class.
#define BND_IMPLEMENT_ATTEMPT_ACQUIRE(cname)					\
int																\
cname##_AttemptAcquire(cname##_t *that, s32 type)				\
{																\
	int cur;												\
	switch (type) {												\
		case STRONG:											\
			cur = atomic_read(&that->m_primaryRefs);		\
			while (cur > 0 &&								\
					!cmpxchg32(	&that->m_primaryRefs.counter,			\
								&cur, cur+1));			\
			if (cur <= 0) {\
				DPRINTF(5, (KERN_WARNING "%s(%p, %s) FAILED!\n", __func__, that, type == STRONG ? "STRONG" : "WEAK"));\
				/*dump_stack()*/;\
				return FALSE;						\
			}\
			cur = atomic_inc_return(&that->m_secondaryRefs);	\
			BND_ASSERT(cur > 1, "ATTEMPT ACQUIRE STONG without WEAK ref");	\
			DPRINTF(5, (KERN_WARNING "%s(%p, %s) s:%d w:%d\n", __func__, that, type == STRONG ? "STRONG" : "WEAK", atomic_read(&that->m_primaryRefs), atomic_read(&that->m_secondaryRefs)));\
			/*dump_stack()*/;\
			return TRUE;										\
		case WEAK:											\
			cur = atomic_read(&that->m_secondaryRefs);		\
			while (cur > 0 &&								\
					!cmpxchg32(	&that->m_secondaryRefs.counter,			\
								&cur, cur+1));			\
			if (cur <= 0) {\
				DPRINTF(5, (KERN_WARNING "%s(%p, %s) FAILED!\n", __func__, that, type == STRONG ? "STRONG" : "WEAK"));\
				/*dump_stack()*/;\
				return FALSE;						\
			}\
			DPRINTF(5, (KERN_WARNING "%s(%p, %s) s:%d w:%d\n", __func__, that, type == STRONG ? "STRONG" : "WEAK", atomic_read(&that->m_primaryRefs), atomic_read(&that->m_secondaryRefs)));\
			/*dump_stack()*/;\
			return TRUE;										\
	}															\
	return FALSE;												\
}																\
/**/

extern spinlock_t cmpxchg32_spinner;

// Quick hack -- should be checking for x86, not ARM.

#if defined(CONFIG_ARM)

static __inline__ int cmpxchg32(volatile int *atom, int *val, int newVal) {
	unsigned long flags;
	spin_lock_irqsave(&cmpxchg32_spinner, flags);
	if (*atom == *val) {
		*atom = newVal;
		spin_unlock_irqrestore(&cmpxchg32_spinner, flags);
		return 1;
	}
	*val = *atom;
	spin_unlock_irqrestore(&cmpxchg32_spinner, flags);
	return 0;
};

#else

static __inline__  int compare_and_swap32(volatile int *location, int oldValue, int newValue)
{
	int success;
	asm volatile("lock; cmpxchg %%ecx, (%%edx); sete %%al; andl $1, %%eax"
		: "=a" (success) : "a" (oldValue), "c" (newValue), "d" (location));
	return success;
}

static __inline__  bool cmpxchg32(volatile int *atom, int *value, int newValue)
{
	int success = compare_and_swap32(atom, *value, newValue);
	if (!success)
		*value = *atom;

	return success;
};

#endif

#define BND_LOCK(x) do { down(&(x)); \
	BND_ASSERT(atomic_read(&((x).count)) <= 0, "BND_LOCK() lock still free"); } while (0)
#define BND_UNLOCK(x) do { \
	BND_ASSERT(atomic_read(&((x).count)) <= 0, "BND_UNLOCK() lock already free"); \
	up(&(x)); } while (0)

#if defined(CONFIG_ARM)
// __cpuc_flush_user_range is arm specific, but the generic function need a
// vm_area_struct and will flush the entire page.
#define BND_FLUSH_CACHE(start, end) do { \
		__cpuc_flush_user_range((size_t)start & ~(L1_CACHE_BYTES-1), L1_CACHE_ALIGN((size_t)end), 0); \
	} while(0)
#else
#define BND_FLUSH_CACHE(start, end)
#endif

#define B_CAN_INTERRUPT (1)

#define B_INFINITE_TIMEOUT ((~(0ULL))>>1)
#define B_ABSOLUTE_TIMEOUT (1)

#define B_BAD_THREAD_ID ((pid_t)0)
#define B_REAL_TIME_PRIORITY (10)

#endif // BINDER_DEFS_H
