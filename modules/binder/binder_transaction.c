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
#include "binder_transaction.h"
#include "binder_proc.h"
#include "binder_thread.h"
#include "binder_node.h"
#include <asm/uaccess.h>
#include <asm/cacheflush.h>

#define PURGATORY 0
#if PURGATORY
static DECLARE_MUTEX(sem);
static binder_transaction_t* head = NULL;
static binder_transaction_t** tail = &head;
static int count = 0;

static void my_free_trans(binder_transaction_t *t)
{
	down(&sem);
	*tail = t;
	tail = &t->next;
	count++;
	if (count > 20) {
		t = head;
		head = head->next;
		kmem_cache_free(transaction_cache, t);
		count--;
	}
	up(&sem);
}
#define ALLOC_TRANS kmem_cache_alloc(transaction_cache, GFP_KERNEL)
#define FREE_TRANS(x) my_free_trans(x)
#else
#define ALLOC_TRANS kmem_cache_alloc(transaction_cache, GFP_KERNEL)
#define FREE_TRANS(x) kmem_cache_free(transaction_cache, x)
#endif

void					binder_transaction_dtor(binder_transaction_t *that);

void					binder_transaction_Init(binder_transaction_t *that);
void					binder_transaction_debug_dump(binder_transaction_t *that);

status_t 
binder_transaction_ConvertToNodes(binder_transaction_t *that, binder_proc_t *from, iobuffer_t *io)
{
	DPRINTF(4, (KERN_WARNING "%s(%p, %p, %p)\n", __func__, that, from, io));
	if (binder_transaction_RefFlags(that)) return 0;

	if (that->team != from) {
		BND_ACQUIRE(binder_proc, from, WEAK, that);
		if (that->team) BND_RELEASE(binder_proc, that->team, WEAK, that);
		that->team = from;
	}

	if (that->offsets_size > 0) {
		u8 *ptr = binder_transaction_Data(that);
		const size_t *off = binder_transaction_Offsets(that); //(const size_t*)(ptr + INT_ALIGN(that->data_size));
		const size_t *offEnd = off + (that->offsets_size/sizeof(size_t));
		struct flat_binder_object* flat;

		// This function is called before any references have been acquired.
		BND_ASSERT((that->flags&tfReferenced) == 0, "ConvertToNodes() already called!");
		that->flags |= tfReferenced;
	
		BND_FLUSH_CACHE(  binder_transaction_UserData(that),
		                  binder_transaction_UserOffsets(that) +
		                  binder_transaction_OffsetsSize(that) );

		while (off < offEnd) {
			bool strong = TRUE;
			BND_ASSERT(*off <= (that->data_size-sizeof(struct flat_binder_object)), "!!! ConvertToNodes: type code pointer out of range.");
			flat = (struct flat_binder_object*)(ptr + *off++);
			switch (flat->type) {
			case kPackedLargeBinderHandleType:
				DPRINTF(5,(KERN_WARNING "ConvertToNodes B_BINDER_HANDLE_TYPE %ld\n",flat->handle));
				// Retrieve node and acquire reference.
				flat->node = binder_proc_Descriptor2Node(from, flat->handle,that, STRONG);
				break;
			case kPackedLargeBinderType:
				DPRINTF(5,(KERN_WARNING "ConvertToNodes B_BINDER_TYPE %p\n",flat->binder));
				// Lookup node and acquire reference.
				if (binder_proc_Ptr2Node(from, flat->binder,flat->cookie,&flat->node,io,that, STRONG) != 0) return -EINVAL;
				if (binder_transaction_IsRootObject(that)) {
					DPRINTF(5,(KERN_WARNING "Making node %p a root node\n", flat->node));
					binder_proc_SetRootObject(from, flat->node);
				}
				break;
			case kPackedLargeBinderWeakHandleType:
				DPRINTF(5,(KERN_WARNING "ConvertToNodes B_BINDER_WEAK_HANDLE_TYPE %ld\n",flat->handle));
				// Retrieve node and acquire reference.
				flat->node = binder_proc_Descriptor2Node(from, flat->handle,that,WEAK);
				strong = FALSE;
				break;
			case kPackedLargeBinderWeakType:
				DPRINTF(5,(KERN_WARNING "ConvertToNodes B_BINDER_WEAK_TYPE %p\n",flat->binder));
				// Lookup node and acquire reference.
				if (binder_proc_Ptr2Node(from, flat->binder,flat->cookie,&flat->node,io,that,WEAK) != 0) return -EINVAL;
				strong = FALSE;
				break;
			default:
				BND_ASSERT(FALSE, "Bad binder offset given to transaction!");
				DPRINTF(0, (KERN_WARNING "ConvertToNodes: unknown typecode %08lx, off: %p, offEnd: %p\n", flat->type, off, offEnd));
				BND_FLUSH_CACHE(ptr, offEnd);
				return -EINVAL;
			}
			flat->type = strong ? kPackedLargeBinderNodeType : kPackedLargeBinderWeakNodeType;
		}
		BND_FLUSH_CACHE(ptr, offEnd);
	}
	
	return 0;
}

status_t 
binder_transaction_ConvertFromNodes(binder_transaction_t *that, binder_proc_t *to)
{
	u8 *ptr;
	size_t *off;
	size_t *offEnd;
	DPRINTF(4, (KERN_WARNING "%s(%p, %p)\n", __func__, that, to));
	if (binder_transaction_RefFlags(that)) return 0;

	if (that->team != to) {
		BND_ACQUIRE(binder_proc, to, WEAK, that);
		if (that->team) BND_RELEASE(binder_proc, that->team, WEAK, that);
		that->team = to;
	}

	if (that->offsets_size > 0) {
		// This function is called after references have been acquired.
		BND_ASSERT((that->flags&tfReferenced) != 0, "ConvertToNodes() not called!");
	
		ptr = binder_transaction_Data(that);
		off = binder_transaction_Offsets(that); //(const size_t*)(ptr + INT_ALIGN(that->data_size));
		offEnd = off + (that->offsets_size/sizeof(size_t));
		struct flat_binder_object* flat;
		
		BND_FLUSH_CACHE(  binder_transaction_UserData(that),
		                  binder_transaction_UserOffsets(that) +
		                  binder_transaction_OffsetsSize(that) );
		while (off < offEnd) {
			flat = (struct flat_binder_object*)(ptr + *off++);
			binder_node_t *n = flat->node;
			if (flat->type == kPackedLargeBinderNodeType) {
				if (!n) {
					flat->type = kPackedLargeBinderType;
					flat->binder = NULL;
					flat->cookie = NULL;
				} else if (binder_node_Home(n) == to) {
					flat->type = kPackedLargeBinderType;
					flat->binder = binder_node_Ptr(n);
					flat->cookie = binder_node_Cookie(n);
					// Keep a reference on the node so that it doesn't
					// go away until this transaction completes.
				} else {
					flat->type = kPackedLargeBinderHandleType;
					flat->handle = binder_proc_Node2Descriptor(to, n, TRUE, STRONG);
					flat->cookie = NULL;
					// We now have a reference on the node through the
					// target team's descriptor, so remove our own ref.
					BND_RELEASE(binder_node, n, STRONG, that);
				}
			} else if (flat->type == kPackedLargeBinderWeakNodeType) {
				if (!n) {
					flat->type = kPackedLargeBinderWeakType;
					flat->binder = NULL;
					flat->cookie = NULL;
				} else if (binder_node_Home(n) == to) {
					flat->type = kPackedLargeBinderWeakType;
					flat->binder = binder_node_Ptr(n);
					flat->cookie = binder_node_Cookie(n);
					// Keep a reference on the node so that it doesn't
					// go away until this transaction completes.
				} else {
					flat->type = kPackedLargeBinderWeakHandleType;
					flat->handle = binder_proc_Node2Descriptor(to, n, TRUE, WEAK);
					flat->cookie = NULL;
					// We now have a reference on the node through the
					// target team's descriptor, so remove our own ref.
					BND_RELEASE(binder_node, n, WEAK, that);
				}
			} else {
				BND_ASSERT(FALSE, "Bad binder offset given to transaction!");
				DPRINTF(0, (KERN_WARNING "ConvertToNodes: unknown typecode %08lx, off: %p, offEnd: %p\n", flat->type, off, offEnd));
				BND_FLUSH_CACHE(ptr, offEnd);
				return -EINVAL;
			}
		}
		BND_FLUSH_CACHE(ptr, offEnd);
	}

	return 0;
}

void
binder_transaction_ReleaseTarget(binder_transaction_t *that)
{
	DPRINTF(4, (KERN_WARNING "%s(%p)\n", __func__, that));
	if (that->sender) {
		DPRINTF(5, (KERN_WARNING "%s(%p) release sender %p\n", __func__, that, that->sender));
		BND_RELEASE(binder_thread, that->sender, WEAK, that);
		that->sender = NULL;
	}
	if (that->receiver) {
		DPRINTF(5, (KERN_WARNING "%s(%p) release receiver %p\n", __func__, that, that->receiver));
		BND_RELEASE(binder_thread, that->receiver, WEAK, that);
		that->receiver = NULL;
	}

	if (that->target) {
		DPRINTF(5, (KERN_WARNING "%s(%p) release target %p\n", __func__, that, that->target));
		BND_RELEASE(binder_node, that->target, binder_transaction_RefFlags(that) == tfAttemptAcquire ? WEAK : STRONG,that);
		that->target = NULL;
	}
	DPRINTF(4, (KERN_WARNING "%s(%p) fini\n", __func__, that));
}

void
binder_transaction_ReleaseTeam(binder_transaction_t *that)
{
	DPRINTF(4, (KERN_WARNING "%s(%p), team: %p\n", __func__, that, that->team));

	if (that->team) {
		BND_RELEASE(binder_proc, that->team, binder_transaction_RefFlags(that) ? STRONG : WEAK, that);
		that->team = NULL;
	}
}

size_t
binder_transaction_MaxIOToNodes(binder_transaction_t *that)
{
	DPRINTF(4, (KERN_WARNING "%s(%p): %d\n", __func__, that, (that->offsets_size/8)*16));
	// Each offsets entry is 4 bytes, and could result in 24 bytes
	// being written.  (To be more accurate, we could actually look
	// at the offsets and only include the ones that are a
	// B_BINDER_TYPE or B_BINDER_WEAK_TYPE.)
	return (that->offsets_size/4)*24;
}

binder_proc_t *
binder_transaction_TakeTeam(binder_transaction_t *that, binder_proc_t * me)
{
	binder_proc_t *ret;
	DPRINTF(4, (KERN_WARNING "%s(%p, %p)\n", __func__, that, me));
	if (that->team != me || binder_transaction_RefFlags(that)) return NULL;
	
	ret = that->team;
	that->team = NULL;
	return ret;
}

binder_transaction_t*
binder_transaction_CreateRef(u16 refFlags, void *ptr, void *cookie, binder_proc_t *team)
{
	binder_transaction_t* that = ALLOC_TRANS;
	DPRINTF(4, (KERN_WARNING "%s(%04x, %p, %p): %p\n", __func__, refFlags, ptr, team, that));
	if (that) {
		binder_transaction_Init(that);
		BND_ASSERT((refFlags&(~tfRefTransaction)) == 0 && (refFlags&tfRefTransaction) != 0,
					"Bad flags to binder_transaction::create_ref()");
		that->flags |= refFlags;
		that->data_ptr = ptr;
		that->offsets_ptr = cookie;
		if (team) {
			that->team = team;
			BND_ACQUIRE(binder_proc, that->team, STRONG, that);
		}
	}
	return that;
}

binder_transaction_t*
binder_transaction_Create(u32 _code, size_t _dataSize, const void *_data, size_t _offsetsSize, const void *_offsetsData)
{
	binder_transaction_t* that = ALLOC_TRANS;
	DPRINTF(4, (KERN_WARNING "%s(%08x, %u:%p, %u:%p): %p\n", __func__, _code, _dataSize, _data, _offsetsSize, _offsetsData, that));
	if (that) {
		binder_transaction_Init(that);
		that->code = _code;
		BND_ASSERT(_dataSize == 0 || _data != NULL, "Transaction with dataSize > 0, but NULL data!");
		if (_dataSize && _data) {
			that->data_size = _dataSize;
			that->data_ptr = _data;
			BND_ASSERT(_offsetsSize == 0 || _offsetsData != NULL, "Transaction with offsetsSize > 0, but NULL offsets!");
			if (_offsetsSize && _offsetsData) {
				that->offsets_size = _offsetsSize;
				that->offsets_ptr = _offsetsData;
			}
		}
	}
	return that;
}

binder_transaction_t* binder_transaction_CreateEmpty(void)
{
	binder_transaction_t* that = ALLOC_TRANS;
	DPRINTF(4, (KERN_WARNING "%s(void): %p\n", __func__, that));
	if (that) binder_transaction_Init(that);
	return that;
}

void binder_transaction_Destroy(binder_transaction_t *that)
{
	DPRINTF(4, (KERN_WARNING "%s(%p)\n", __func__, that));
	if (that) {
		binder_transaction_dtor(that);
	}
}

void binder_transaction_DestroyNoRefs(binder_transaction_t *that)
{
	DPRINTF(4, (KERN_WARNING "%s(%p)\n", __func__, that));
	if (that) {
		that->offsets_size = 0;
		binder_transaction_dtor(that);
	}
}

void binder_transaction_Init(binder_transaction_t *that)
{
	that->next = NULL;
	that->target = NULL;
	that->sender = NULL;
	that->receiver = NULL;

	that->code = 0;
	that->team = NULL;
	that->flags = 0;
	that->priority = 10; // FIXME?
	that->data_size = 0;
	that->offsets_size = 0;
	that->data_ptr = NULL;
	that->offsets_ptr = NULL;

	that->map = NULL;
}

void
binder_transaction_dtor(binder_transaction_t *that)
{
	binder_proc_t *owner = NULL;
	DPRINTF(4, (KERN_WARNING "%s(%p)\n", __func__, that));
	if (that->offsets_size > 0) {
		DPRINTF(5, (KERN_WARNING "  -- have binders to clean up\n"));
		BND_ASSERT((that->flags&tfReferenced) != 0, "ConvertToNodes() not called!");
		BND_ASSERT((that->map) != NULL, "binder_transaction_dtor that->map == NULL");
		if (that->team && BND_ATTEMPT_ACQUIRE(binder_proc, that->team, STRONG, that)) owner = that->team;

		DPRINTF(5, (KERN_WARNING "  -- that->map == %p\n", that->map));
		if(that->map != NULL) { // avoid crash due to corrupt transaction
			u8 *ptr = 0;
			const size_t *off;
			const size_t *offEnd;
			struct flat_binder_object* flat;

			ptr = binder_transaction_Data(that);
			off = (const size_t*)binder_transaction_Offsets(that);
			offEnd = off + (that->offsets_size/sizeof(size_t));

			BND_FLUSH_CACHE(  binder_transaction_UserData(that),
			                  binder_transaction_UserOffsets(that) +
			                  binder_transaction_OffsetsSize(that) );
			while (off < offEnd) {
				DPRINTF(9, (KERN_WARNING "type ptr: %p\n", ptr+*off));
				flat = (struct flat_binder_object*)(ptr + *off++);
				DPRINTF(9, (KERN_WARNING " type: %08lx\n", flat->type));
				switch (flat->type) {
				case kPackedLargeBinderHandleType:
					DPRINTF(9, (KERN_WARNING "Delete binder_transaction B_BINDER_HANDLE_TYPE %ld\n",flat->handle));
					// Only call if there are primary references on the team.
					// Otherwise, it has already removed all of its handles.
					if (owner) binder_proc_UnrefDescriptor(owner, flat->handle, STRONG);
					break;
				case kPackedLargeBinderType:
					// Only do this if there are primary references on the team.
					// The team doesn't go away until all published binders are
					// removed; after that, there are no references to remove.
					if (owner) {
						binder_node_t *n;
						if (binder_proc_Ptr2Node(owner, flat->binder,flat->cookie,&n,NULL,that, STRONG) == 0) {
							if (n) {
								BND_RELEASE(binder_node, n, STRONG,that); // once for the grab we just did
								BND_RELEASE(binder_node, n, STRONG,that); // and once for the reference this transaction holds
							}
						} else {
							BND_ASSERT(FALSE, "Can't find node!");
						}
					}
					break;
				case kPackedLargeBinderNodeType:
					if (flat->node) BND_RELEASE(binder_node, flat->node, STRONG,that);
					break;
				case kPackedLargeBinderWeakHandleType:
					DPRINTF(9, (KERN_WARNING "Delete binder_transaction B_BINDER_HANDLE_TYPE %ld\n",flat->handle));
					// Only call if there are primary references on the team.
					// Otherwise, it has already removed all of its handles.
					if (owner) binder_proc_UnrefDescriptor(owner, flat->handle, WEAK);
					break;
				case kPackedLargeBinderWeakType:
					// Only do this if there are primary references on the team.
					// The team doesn't go away until all published binders are
					// removed; after that, there are no references to remove.
					if (owner) {
						binder_node_t *n;
						if (binder_proc_Ptr2Node(owner, flat->binder,flat->cookie,&n,NULL,that,WEAK) == 0) {
							if (n) {
								BND_RELEASE(binder_node, n, WEAK,that); // once for the grab we just did
								BND_RELEASE(binder_node, n, WEAK,that); // and once for the reference this transaction holds
							}
						} else {
							BND_ASSERT(FALSE, "Can't find node!");
						}
					}
					break;
				case kPackedLargeBinderWeakNodeType:
					if (flat->node) BND_RELEASE(binder_node, flat->node, WEAK,that);
					break;
				}
			}
			BND_FLUSH_CACHE(ptr, offEnd);
		}
	}
	
	// release the RAM and address space in the receiver.
	if (that->map) {
		if (that->map->team) binder_proc_FreeTransactionBuffer(that->map->team, that->map);
		else printk(KERN_WARNING "%s(%p) -- no team trying to release map %p\n", __func__, that, that->map);
	}

	if (owner) BND_RELEASE(binder_proc, owner, STRONG,that);

	binder_transaction_ReleaseTeam(that);	
	binder_transaction_ReleaseTarget(that);

	// release the RAM
	FREE_TRANS(that);
}

/* We need the recipient team passed in because we can't always know the
 * receiver at this point. */
status_t
binder_transaction_CopyTransactionData(binder_transaction_t *that, binder_proc_t *recipient)
{
	status_t result = -EINVAL;
	size_t tSize = INT_ALIGN(that->data_size) + INT_ALIGN(that->offsets_size);
	DPRINTF(0, (KERN_WARNING "%s(%p, %p)\n", __func__, that, recipient));
	// Do we need to ensure that->map contains NULL?  What do we do if it doesn't?
	if (binder_transaction_IsAcquireReply(that)) {
		// No data to copy
		result = 0;
	} else {
	// if (tSize >= sizeof(that->data)) {
		that->map = binder_proc_AllocateTransactionBuffer(recipient, tSize);
		if (that->map) {
			// locate our kernel-space address
			u8 *to = page_address(that->map->page);
			size_t not_copied;
			// copy the data from user-land
			BND_FLUSH_CACHE(  binder_transaction_UserData(that),
			                  binder_transaction_UserData(that) + tSize );
			not_copied = copy_from_user(to, that->data_ptr, that->data_size);
			// and the offsets, too
			if ((not_copied == 0) && (that->offsets_size != 0)) {
				to += INT_ALIGN(that->data_size);
				not_copied = copy_from_user(to, that->offsets_ptr, that->offsets_size);
				if (not_copied) {
					DPRINTF(0, (KERN_WARNING " -- failed to copy %u of %u bytes of offsets from %p to %p\n", not_copied, that->offsets_size, that->offsets_ptr, to));
				}
			} else if (not_copied) {
				// BUSTED!
				DPRINTF(0, (KERN_WARNING " -- Couldn't copy %u of %u bytes from user-land %p to %p\n", not_copied, that->data_size, that->data_ptr, to));
			}
			BND_FLUSH_CACHE(  binder_transaction_Data(that),
			                  binder_transaction_Data(that) + tSize );
			result = not_copied ? -EFAULT : 0;
		}
		else {
			DPRINTF(0, (KERN_WARNING "binder_transaction_CopyTransactionData() failed to allocate transaction buffer in %p\n", recipient));
		}
	// } else {
	// // ignore inlined data for now
	//	printk(KERN_WARNING "Small transaction in binder_transaction_CopyTransactionData\n");
	//	binder_transaction_SetInline(that, TRUE);
	// }
	}
	return result;
}

