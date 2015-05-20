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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>		// includes <linux/ioctl.h>
#include <linux/sched.h>	// for 'current'
#include <linux/mm.h>		// for vma, etc.
#include <linux/hash.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include "binder_defs.h"
#include "binder_proc.h"
#include "binder_thread.h"
#include "binder_node.h"
#include "binder_transaction.h"
#include "iobuffer.h"

MODULE_LICENSE("GPL"); // class_* symbols get exported GPL
MODULE_AUTHOR("PalmSource, Inc.");
MODULE_DESCRIPTION("Capability-based IPC");

#define BINDER_MINOR 0
#define BINDER_NUM_DEVS 1
#define BINDER_NAME "binder"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
#define CLASS_SIMPLE class_simple
#define CLASS_SIMPLE_CREATE class_simple_create
#define CLASS_SIMPLE_DEVICE_ADD class_simple_device_add
#define CLASS_SIMPLE_DESTROY class_simple_destroy
#define CLASS_SIMPLE_DEVICE_REMOVE class_simple_device_remove
#else
#define CLASS_SIMPLE class
#define CLASS_SIMPLE_CREATE class_create
#define CLASS_SIMPLE_DEVICE_ADD class_device_create
#define CLASS_SIMPLE_DESTROY class_destroy
#define CLASS_SIMPLE_DEVICE_REMOVE(a) class_device_destroy(binder_class, a)
#endif

/*
 * Prototypes
 */

struct binder_thread* find_thread(pid_t pid, binder_proc_t *proc, bool remove);

#if HAVE_UNLOCKED_IOCTL
#define USE_UNLOCKED_IOCTL 1
#else
#define USE_UNLOCKED_IOCTL 0
#endif
#if USE_UNLOCKED_IOCTL
static long binder_unlocked_ioctl(struct file *, unsigned int, unsigned long);
#else
static int binder_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
#endif
static int binder_open(struct inode *, struct file *);
static int binder_release(struct inode *, struct file *);
static int binder_mmap(struct file *, struct vm_area_struct *);

/*
 * Globals
 */

struct binder_dev {
	struct	cdev	cdev;
};

static int binder_major = 0;
static char const * const binder_name = BINDER_NAME;
static struct binder_dev binder_device;
static struct CLASS_SIMPLE *binder_class;

static struct file_operations binder_fops = {
	.owner = THIS_MODULE,
#if USE_UNLOCKED_IOCTL
	.unlocked_ioctl = binder_unlocked_ioctl,
#else
	.ioctl = binder_ioctl,
#endif
	.mmap = binder_mmap,
	.open = binder_open,
	.release = binder_release
};

static void binder_vma_open(struct vm_area_struct * area);
static void binder_vma_close(struct vm_area_struct * area);
static struct page * binder_vma_nopage(struct vm_area_struct * area, unsigned long address, int *type);

static struct vm_operations_struct binder_vm_ops = {
	.open = binder_vma_open,
	.close = binder_vma_close,
	.nopage = binder_vma_nopage
};

kmem_cache_t *transaction_cache = NULL;
kmem_cache_t *thread_cache = NULL;
kmem_cache_t *node_cache = NULL;
kmem_cache_t *local_mapping_cache = NULL;
kmem_cache_t *reverse_mapping_cache = NULL;
kmem_cache_t *range_map_cache = NULL;

spinlock_t cmpxchg32_spinner = SPIN_LOCK_UNLOCKED;
static DECLARE_MUTEX(maps_lock);

/*
 * The kernel sizes its process hash table based up on the amount of RAM, with
 * a lower limit of 4 bits and an upper limit of 12 bits.  We probably don't
 * need 8 bits worth of entries on PDAs, but it make it very likely we will
 * have chain lengths of one.
 */

#define PID_HASH_BITS (8)
static int pid_hash_bits = PID_HASH_BITS;
#define hash_proc_id(pid) hash_long(pid, pid_hash_bits)

static struct hlist_head *pid_table = NULL;

static inline binder_thread_t *
binder_thread_alloc(pid_t pid, binder_proc_t *proc, int index)
{
	binder_thread_t *thread = binder_thread_init(pid, proc);
	if (thread) {
		if (proc) {
			if(!binder_proc_AddThread(proc, thread))
				return NULL; // binder_proc_AddThread will cause the thread to be deleted if the process is dying
		}
		hlist_add_head(&(thread->node), pid_table + index);
	}
	DPRINTF(5, (KERN_WARNING "%s(%u, %p, %d): %p\n", __func__, pid, proc, index, thread));
	return thread;
}

struct binder_thread *
core_find_thread(pid_t pid, binder_proc_t *proc, bool remove)
{
	binder_thread_t *thread;
	struct hlist_node *_p;
	const int index = hash_proc_id(pid);

	DPRINTF(5, (KERN_WARNING "%s(%u, %p, %s): index %d\n", __func__, pid, proc, remove ? "TRUE" : "FALSE", index));
	hlist_for_each_entry(thread, _p, pid_table + index, node) {
		DPRINTF(5, (KERN_WARNING "thread: %p, thread->m_thid: %u\n", thread, thread->m_thid));
		if (thread->m_thid == pid) {
			DPRINTF(5, (KERN_WARNING "found thread %p, proc=%p\n", thread, thread->m_team));
			if (remove) {
				thread->attachedToThread = FALSE;
				hlist_del(&thread->node);
			} else if (proc) {
				if (thread->m_team == NULL) {
					binder_thread_AttachProcess(thread, proc);
				} else {
					BND_ASSERT(thread->m_team == proc, "proc changed");
				}
			}
			return thread;
		}
	}
	
	return NULL;
}

binder_thread_t *
find_thread(pid_t pid, binder_proc_t *proc, bool remove)
{
	binder_thread_t *thread;

	DPRINTF(5, (KERN_WARNING "%s(%u, %p, %s)\n", __func__, pid, proc, remove ? "TRUE" : "FALSE"));
	thread = core_find_thread(pid, proc, remove);

	/* binder_thread_alloc() fails for -ENOMEM only */
	if (thread == NULL && remove == FALSE) thread = binder_thread_alloc(pid, proc, hash_proc_id(pid));
	return thread;
}

struct binder_thread *
check_for_thread(pid_t pid, bool create)
{
	binder_thread_t *thread;
	int rv;
	
	rv = down_interruptible(&maps_lock);
	if(rv != 0)
		return NULL;
	if (create)
		thread = find_thread(pid, NULL, FALSE);
	else
		thread = core_find_thread(pid, NULL, FALSE);
	if(thread != NULL)
		BND_FIRST_ACQUIRE(binder_thread, thread, STRONG, thread);
	up(&maps_lock);
	
	return thread;
}

binder_thread_t *
attach_child_thread(pid_t child_pid, binder_thread_t *parent)
{
	binder_thread_t *thread;
	int rv;
	bool failed = FALSE;
	
	rv = down_interruptible(&maps_lock);
	if(rv != 0)
		return NULL;
	thread = find_thread(child_pid, NULL, FALSE);
	if(thread != NULL) {
		BND_FIRST_ACQUIRE(binder_thread, thread, STRONG, parent);
		// Note: it is important this be done with the lock
		// held.  See binder_thread_WaitForParent().
		failed = !binder_thread_SetParentThread(thread, parent);
	}
	up(&maps_lock);
	
	if (failed) {
		forget_thread(thread);
		thread = NULL;
	}
	
	return thread;
}

void
forget_thread(struct binder_thread *thread)
{
	pid_t pid;
	bool attached;
	int rv;
	
	rv = down_interruptible(&maps_lock);
	if(rv != 0)
		return;
	pid = thread->m_thid;
	attached = thread->attachedToThread;
	if(BND_RELEASE(binder_thread, thread, STRONG, thread) == 1) {
		// Remove it if not yet accessed by user space...
		if (!attached) {
			find_thread(pid, NULL, TRUE);
		}
	}
	up(&maps_lock);
}

#if BND_MEM_DEBUG
typedef struct dbg_mem_header_s {
	unsigned long state;
	kmem_cache_t *slab;
	struct dbg_mem_header_s *next;
	struct dbg_mem_header_s *prev;
} dbg_mem_header_t ;
static dbg_mem_header_t *dbg_active_memory;
#endif

void generic_slab_xtor(void *p, kmem_cache_t *slab, unsigned long flags)
{
#if BND_MEM_DEBUG
	dbg_mem_header_t *h = p;
	if(flags & SLAB_CTOR_CONSTRUCTOR) {
		h->state = 0;
		h->slab = slab;
		h->next = dbg_active_memory;
		if(h->next)
			h->next->prev = h;
		h->prev = NULL;
		dbg_active_memory = h;
	}
	else {
		BND_ASSERT(h->state == 0 || h->state == 0x22222222, "memory still in use");
		if(h->next)
			h->next->prev = h->prev;
		if(h->prev)
			h->prev->next = h->next;
		else
			dbg_active_memory = h->next;
	}
#endif
}

void transaction_slab_xtor(void *p, kmem_cache_t *slab, unsigned long flags)
{
	DIPRINTF(10, (KERN_WARNING "%s(%p, %p, %08lx)\n", __func__, p, slab, flags));
	generic_slab_xtor(p, slab, flags);
}

void thread_slab_xtor(void *p, kmem_cache_t *slab, unsigned long flags)
{
	DIPRINTF(10, (KERN_WARNING "%s(%p, %p, %08lx)\n", __func__, p, slab, flags));
	generic_slab_xtor(p, slab, flags);
}

void node_slab_xtor(void *p, kmem_cache_t *slab, unsigned long flags)
{
	DIPRINTF(10, (KERN_WARNING "%s(%p, %p, %08lx)\n", __func__, p, slab, flags));
	generic_slab_xtor(p, slab, flags);
}

void local_mapping_slab_xtor(void *p, kmem_cache_t *slab, unsigned long flags)
{
	DIPRINTF(10, (KERN_WARNING "%s(%p, %p, %08lx)\n", __func__, p, slab, flags));
	generic_slab_xtor(p, slab, flags);
}

void reverse_mapping_slab_xtor(void *p, kmem_cache_t *slab, unsigned long flags)
{
	DIPRINTF(10, (KERN_WARNING "%s(%p, %p, %08lx)\n", __func__, p, slab, flags));
	generic_slab_xtor(p, slab, flags);
}

void range_map_slab_xtor(void *p, kmem_cache_t *slab, unsigned long flags)
{
	DIPRINTF(10, (KERN_WARNING "%s(%p, %p, %08lx)\n", __func__, p, slab, flags));
	generic_slab_xtor(p, slab, flags);
}

static int /*__init*/ create_pools(void)
{
	//long cache_flags = /*SLAB_DEBUG_FREE | SLAB_DEBUG_INITIAL | SLAB_RED_ZONE |*/ SLAB_POISON;
	//long cache_flags = SLAB_RECLAIM_ACCOUNT | SLAB_NO_REAP;
	long cache_flags = 0;
#if BND_MEM_DEBUG
	size_t pad = sizeof(dbg_mem_header_t);
#else
	size_t pad = 0;
#endif
	DPRINTF(4, (KERN_WARNING "%s()\n", __func__));

	// small object pools
	transaction_cache = kmem_cache_create("binder_transaction_t", sizeof(binder_transaction_t)+pad, 0, cache_flags, transaction_slab_xtor, transaction_slab_xtor);
	if (!transaction_cache) return -ENOMEM;
	thread_cache = kmem_cache_create("binder_thread_t", sizeof(binder_thread_t)+pad, 0, cache_flags, thread_slab_xtor, thread_slab_xtor);
	if (!thread_cache) return -ENOMEM;
	node_cache = kmem_cache_create("binder_node_t", sizeof(binder_node_t)+pad, 0, cache_flags, node_slab_xtor, node_slab_xtor);
	if (!node_cache) return -ENOMEM;
	local_mapping_cache = kmem_cache_create("local_mapping_t", sizeof(local_mapping_t)+pad, 0, cache_flags, local_mapping_slab_xtor, local_mapping_slab_xtor);
	if (!local_mapping_cache) return -ENOMEM;
	reverse_mapping_cache = kmem_cache_create("reverse_mapping_t", sizeof(reverse_mapping_t)+pad, 0, cache_flags, reverse_mapping_slab_xtor, reverse_mapping_slab_xtor);
	if (!reverse_mapping_cache) return -ENOMEM;
	range_map_cache = kmem_cache_create("range_map_t", sizeof(range_map_t)+pad, 0, cache_flags, range_map_slab_xtor, range_map_slab_xtor);
	if (!range_map_cache) return -ENOMEM;

	// hash tables
	pid_table = kmalloc(sizeof(void *) << PID_HASH_BITS, GFP_KERNEL);
	if (!pid_table) return -ENOMEM;
	memset(pid_table, 0, sizeof(void *) << PID_HASH_BITS);
	return 0;
}

static int destroy_pools(void)
{
	int res = 0;
#if BND_MEM_DEBUG
	dbg_mem_header_t *m, *mn;
#endif
	DPRINTF(4, (KERN_WARNING "%s()\n", __func__));

	/*
	 * These can fail if we haven't free'd all of the objects we've allocated.
	 */

#if BND_MEM_DEBUG

	
	DPRINTF(4, (KERN_WARNING "%s() dbg_active_memory = %p\n", __func__, dbg_active_memory));
	m = dbg_active_memory;
	while(m) {
		mn = m->next;
		if(m->state == 0x11111111) {
			printk(KERN_WARNING "%s() memory still in use: %p slab %p\n", __func__, m + 1, m->slab);
			dbg_kmem_cache_free(m->slab, m + 1);
		}
		m = mn;
	}
#endif
	
	res |= kmem_cache_destroy(transaction_cache);
	res |= kmem_cache_destroy(thread_cache);
	res |= kmem_cache_destroy(node_cache);
	res |= kmem_cache_destroy(local_mapping_cache);
	res |= kmem_cache_destroy(reverse_mapping_cache);
	res |= kmem_cache_destroy(range_map_cache);
	if (pid_table) kfree(pid_table);
	return res;
}

static int __init init_binder(void)
{
	struct class_device *simple;
	int result;
	dev_t dev = 0;

	result = create_pools();
	if (result) {
		goto free_pools;
	}

	result = alloc_chrdev_region(&dev, BINDER_MINOR, BINDER_NUM_DEVS, binder_name);
	if (result < 0) {
		printk(KERN_WARNING "init_binder: alloc_chrdev_region() failed: %d\n", result);
		return result;
	}

	binder_major = MAJOR(dev);
	binder_class = CLASS_SIMPLE_CREATE(THIS_MODULE, "binderipc");
	if (IS_ERR(binder_class)) {
		result = PTR_ERR(binder_class);
		printk(KERN_WARNING "init_binder: CLASS_SIMPLE_CREATE() failed: %d\n", result);
		goto unalloc;
	}

	memset(&binder_device, 0, sizeof(binder_device)); // overkill, but we don't care
	cdev_init(&binder_device.cdev, &binder_fops);
	binder_device.cdev.owner = THIS_MODULE;
	result = cdev_add(&binder_device.cdev, dev, BINDER_NUM_DEVS);
	if (result < 0) {
		printk(KERN_WARNING "init_binder: cdev_add() failed: %d\n", result);
		goto unregister_class;
	}

	simple = CLASS_SIMPLE_DEVICE_ADD(binder_class, dev, NULL, "%s", BINDER_NAME);
	if (IS_ERR(simple)) {
		result = PTR_ERR(simple);
		goto unadd_cdev;
	}

	goto exit0;

unadd_cdev:
	cdev_del(&binder_device.cdev);
unregister_class:
	CLASS_SIMPLE_DESTROY(binder_class);
unalloc:
	unregister_chrdev_region(binder_major, BINDER_NUM_DEVS);
free_pools:
	destroy_pools();
exit0:
	return result;
}

static void __exit cleanup_binder(void)
{
	CLASS_SIMPLE_DEVICE_REMOVE(MKDEV(binder_major, 0));
	cdev_del(&binder_device.cdev);
	CLASS_SIMPLE_DESTROY(binder_class);
	unregister_chrdev_region(binder_major, BINDER_NUM_DEVS);
	destroy_pools();
}


module_init(init_binder);
module_exit(cleanup_binder);

static int binder_open(struct inode *nodp, struct file *filp)
{
	binder_proc_t *proc;

	//printk(KERN_WARNING "%s(%p %p) (pid %d)\n", __func__, nodp, filp, current->pid);
	// We only have one device, so we don't have to dig into the inode for it.

	down(&maps_lock);
	proc = new_binder_proc();
	filp->private_data = proc;
	up(&maps_lock);
	printk(KERN_WARNING "%s(%p %p) (pid %d) got %p\n", __func__, nodp, filp, current->pid, proc);
	if(proc == NULL)
		return -ENOMEM;
	return 0;
}

static int binder_release(struct inode *nodp, struct file *filp)
{
	binder_proc_t *that;
	binder_thread_t *thread;
	struct hlist_node *_p, *_pp;
	int index;
	printk(KERN_WARNING "%s(%p %p) (pid %d) pd %p\n", __func__, nodp, filp, current->pid, filp->private_data);
	that = filp->private_data;
	if (that) {
		filp->private_data = NULL;
		
		// ensure the process stays around until we can verify termination
		index = 1 << pid_hash_bits;

		DPRINTF(5, (KERN_WARNING "%s(%p) freeing threads\n", __func__, that));

		down(&maps_lock);
		while (index--) {
			hlist_for_each_entry_safe(thread, _p, _pp, pid_table + index, node) {
				if (thread->m_team == that) {
					DPRINTF(5, (KERN_WARNING "%s(%p) freeing thread %d\n", __func__, that, thread->m_thid));
					hlist_del(&thread->node);
					BND_RELEASE(binder_thread, thread, STRONG, that);
					//BND_RELEASE(binder_thread, thread, WEAK, that);
				}
			}
		}
		DPRINTF(5, (KERN_WARNING "%s(%p) done freeing threads\n", __func__, that));
		up(&maps_lock);
		
		binder_proc_Die(that, FALSE);
		BND_RELEASE(binder_proc, that, STRONG, that);
	}
	else printk(KERN_WARNING "%s(pid %d): couldn't find binder_proc to Die()\n", __func__, current->pid);
	return 0;
}

#if USE_UNLOCKED_IOCTL
static long binder_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int binder_ioctl(struct inode *nodp, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	binder_thread_t *thread;
	struct binder_proc *proc;
	int rv;

	if (_IOC_TYPE(cmd) != BINDER_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > BINDER_IOC_MAXNR) return -ENOTTY;

	DPRINTF(5, (KERN_WARNING "%s: %08x\n", __func__, cmd));

	// find the thread tracking structure
	rv = down_interruptible(&maps_lock);
	if(rv != 0)
		return rv;
	proc = filp->private_data;
	BND_ASSERT(proc != NULL, "ioctl called after release");
	if(proc == NULL || !binder_proc_IsAlive(proc))
		thread = NULL;
	else
		thread = find_thread(current->pid, proc, cmd == BINDER_THREAD_EXIT);
	if(thread != NULL) {
		BND_ACQUIRE(binder_thread, thread, WEAK, thread);
		thread->attachedToThread = TRUE;
	}

	up(&maps_lock);
	if(proc == NULL || !binder_proc_IsAlive(proc))
		return -ECONNREFUSED;
	if (thread == NULL)
		return -ENOMEM;

	//BND_ASSERT(thread->m_team == proc, "bad thread process ptr");
	if(thread->m_team != proc) {
		printk(  KERN_WARNING "%s: cmd %08x process ptr mismatch, "
		         "thread has %p, expected %p\n",
		         __func__, cmd, thread->m_team, proc );
		return -EIO;
	}

	rv = binder_thread_Control(thread, cmd, (void*)arg);
	BND_RELEASE(binder_thread, thread, WEAK, thread);
	return rv;
}

static int binder_mmap(struct file * filp, struct vm_area_struct * vma)
{
	// FIXME: Unil we see a device with ZONE_HIGH memory (currently, greater
	// than 896MB RAM) we don't need to worry about alloc_page.
	vma->vm_ops = &binder_vm_ops;
	vma->vm_flags |= VM_RESERVED | VM_WRITE | VM_READ | VM_RAND_READ | VM_IO | VM_DONTCOPY | VM_DONTEXPAND;
	vma->vm_flags &= ~(VM_SHARED);
	vma->vm_private_data = filp->private_data;
	binder_vma_open(vma);
	return 0;
}

static void binder_vma_open(struct vm_area_struct * area)
{
	binder_proc_t *that;
	DPRINTF(5, (KERN_WARNING "binder_vma_open()\n"));
	// Do we have to watch for clone()'d processes and hunt down the
	// appropriate binder_proc_t?

	that = area->vm_private_data;
	// initialize our free space map
	if (that->m_freeMap.rb_node == NULL) {
		range_map_t *rm = kmem_cache_alloc(range_map_cache, GFP_KERNEL);
		that->m_mmap_start = rm->start = area->vm_start;	
		rm->end = area->vm_end;
		rm->page = NULL;
		BND_LOCK(that->m_map_pool_lock);
		binder_proc_free_map_insert(that, rm);
		BND_UNLOCK(that->m_map_pool_lock);
		DPRINTF(5, (KERN_WARNING "vma(%08lx, %08lx) for %08x\n", rm->start, rm->end, (unsigned int)that));
	}
#if 0
	else printk(KERN_WARNING " --- didn't reconstruct the initial free-map\n");
#endif
}

static void binder_vma_close(struct vm_area_struct * area)
{
	// Uh, what?
	DPRINTF(5, (KERN_WARNING "binder_vma_close() for %08x\n", (unsigned int)area->vm_private_data));
}

static struct page * binder_vma_nopage(struct vm_area_struct * area, unsigned long address, int *type)
{
	struct page *pageptr = NULL;
	// the private data holds a pointer to owning binder_proc
	binder_proc_t *bp = (binder_proc_t *)area->vm_private_data;
	DPRINTF(5, ("binder_vma_nopage(%p, %08lx)\n", bp, address));
	// make sure this address corresponds to a valid transaction
	if (!binder_proc_ValidTransactionAddress(bp, address, &pageptr))
		return NOPAGE_SIGBUS;
	// bump the kernel reference counts
	get_page(pageptr);
	// record the fault type
	if (type) *type = VM_FAULT_MINOR;
	// return the page
	return pageptr;
}

void my_dump_stack(void) { printk(KERN_WARNING ""); dump_stack(); }

void soft_yield()
{
	static int i = 0;
	i++;
	if(i < 10)
		return;
	i = 0;
	yield();
}

#if BND_MEM_DEBUG

#undef kmem_cache_alloc
#undef kmem_cache_free

void *dbg_kmem_cache_alloc(kmem_cache_t *a, unsigned int b)
{
	dbg_mem_header_t *p;
	p = kmem_cache_alloc(a, b);
	BND_ASSERT(p != NULL, "memory allocation failed");
	if(p == NULL)
		return NULL;
	if(p->state != 0x00000000) {
		if(p->state != 0x22222222)
			DPRINTF(5, (KERN_WARNING "%s: kmem_cache_alloc(%p, %d) BAD PTR %p = 0x%08lx\n", __func__, a, b, p, p->state));
		else
			DPRINTF(6, (KERN_WARNING "%s: kmem_cache_alloc(%p, %d) NEW PTR %p = 0x%08lx\n", __func__, a, b, p, p->state));
	}
	p->state = 0x11111111;
	p++;
	DPRINTF(6, (KERN_WARNING "%s: kmem_cache_alloc(%p, %d) returned %p\n", __func__, a, b, p));
	return p;
} 

void dbg_kmem_cache_free(kmem_cache_t *a, void *b)
{
	dbg_mem_header_t *p = b;
	DPRINTF(6, (KERN_WARNING "%s: kmem_cache_free(%p, %p)\n", __func__, a, p));
	p--;
	if(p->state != 0x11111111) {
		printk(KERN_WARNING "%s: kmem_cache_free(%p, %p) BAD ARG 0x%08lx\n", __func__, a, p, p->state);
		dump_stack();
		return;
	}
		
	p->state = 0x22222222;
	kmem_cache_free(a, p);
}

#endif
