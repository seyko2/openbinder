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

#ifndef BINDER_TRANSACTION_H
#define BINDER_TRANSACTION_H

#include "binder_defs.h"
#include "iobuffer.h"
#include <linux/mm.h> // for page_address()

enum {
	tfUserFlags		= 0x000F,

	tfIsReply		= 0x0100,
	tfIsEvent		= 0x0200,
	tfIsAcquireReply	= 0x0400,
	tfIsDeadReply		= 0x0800,
	tfIsFreePending		= 0x0040,
	
	tfAttemptAcquire	= 0x1000,
	tfRelease		= 0x2000,
	tfDecRefs		= 0x3000,
	tfRefTransaction	= 0xF000,
	
	tfReferenced		= 0x0080
};

typedef struct binder_transaction {
	struct binder_transaction *	next;	// next in the transaction queue
	struct binder_node *		target;		// the receiving binder
	struct binder_thread *		sender;	// the sending thread
	struct binder_thread *		receiver; // the receiving thread

	u32				code;
	struct binder_proc *		team;	// do we need this?  Won't sender or receiver's m_team do?
	u16				flags;
	s16				priority;
	size_t				data_size;
	size_t				offsets_size;
	const void *			data_ptr;
	const void *			offsets_ptr;
	
	// The pointer to the actual transaction data.  The binder offsets appear
	// at (mapped address + data_size).
	struct range_map *		map;
	// 12 bytes of inlined transaction data: just enough for one binder (type, ptr/descriptor, offset)
	u8				data[12];
} binder_transaction_t;

binder_transaction_t*	binder_transaction_CreateRef(u16 refFlags, void *ptr, void *cookie, struct binder_proc* team /* = NULL */);
binder_transaction_t*	binder_transaction_Create(u32 code, size_t dataSize, const void *data, size_t offsetsSize /* = 0 */, const void *offsetsData /* = NULL */);
binder_transaction_t*	binder_transaction_CreateEmpty(void);
void					binder_transaction_Destroy(binder_transaction_t *that);
/*	Call this to destroy a transaction before you have called
	ConvertToNodes() on it.  This will avoid releasing references
	on any nodes in the transaction, which you haven't yet acquired. */
void					binder_transaction_DestroyNoRefs(binder_transaction_t *that);
/* Converts from user-types to kernel-nodes */
status_t				binder_transaction_ConvertToNodes(binder_transaction_t *that, struct binder_proc *from, iobuffer_t *io);
/* Converts from kernel-nodes to user-types */
status_t				binder_transaction_ConvertFromNodes(binder_transaction_t *that, struct binder_proc *to);
void					binder_transaction_ReleaseTarget(binder_transaction_t *that);
void					binder_transaction_ReleaseTeam(binder_transaction_t *that);

/*	Return the maximum IO bytes that will be written by
	ConvertToNodes(). */
size_t					binder_transaction_MaxIOToNodes(binder_transaction_t *that);

/*	If this transaction has a primary reference on its team,
	return it and clear the pointer.  You now own the reference. */
struct binder_proc *			binder_transaction_TakeTeam(binder_transaction_t *that, struct binder_proc *me);
status_t				binder_transaction_CopyTransactionData(binder_transaction_t *that, struct binder_proc *recipient);

#define INT_ALIGN(x) (((x)+sizeof(int)-1)&~(sizeof(int)-1))
#define binder_transaction_Data(that) ((u8*)page_address((that)->map->page))
#define binder_transaction_UserData(that) ((void*)((that)->map->start))
#define binder_transaction_DataSize(that) ((that)->data_size)
#define binder_transaction_Offsets(that) ((size_t*)(binder_transaction_Data(that)+INT_ALIGN((that)->data_size)))
#define binder_transaction_UserOffsets(that) ((void*)((that)->map->start + INT_ALIGN((that)->data_size)))
#define binder_transaction_OffsetsSize(that) ((that)->offsets_size)
	
#define binder_transaction_UserFlags(that) ((that)->flags & tfUserFlags)
#define binder_transaction_RefFlags(that) ((that)->flags & tfRefTransaction)
#define binder_transaction_IsInline(that) ((that)->flags & tfInline)
#define binder_transaction_IsRootObject(that) ((that)->flags & tfRootObject)
#define binder_transaction_IsReply(that) ((that)->flags & tfIsReply)
#define binder_transaction_IsEvent(that) ((that)->flags & tfIsEvent)
#define binder_transaction_IsAcquireReply(that) ((that)->flags & tfIsAcquireReply)
#define binder_transaction_IsDeadReply(that) ((that)->flags & tfIsDeadReply)
#define binder_transaction_IsAnyReply(that) ((that)->flags & (tfIsReply|tfIsAcquireReply|tfIsDeadReply))
#define binder_transaction_IsFreePending(that) ((that)->flags & tfIsFreePending)
#define binder_transaction_IsReferenced(that) ((that)->flags & tfReferenced)

#define binder_transaction_SetUserFlags(that, f) { (that)->flags = ((that)->flags&(~tfUserFlags)) | (f&tfUserFlags); }
#define binder_transaction_SetInline(that, f) { if (f) (that)->flags |= tfInline; else (that)->flags &= ~tfInline; }
#define binder_transaction_SetRootObject(that, f) { if (f) (that)->flags |= tfRootObject; else (that)->flags &= ~tfRootObject; }
#define binder_transaction_SetReply(that, f) { if (f) (that)->flags |= tfIsReply; else (that)->flags &= ~tfIsReply; }
#define binder_transaction_SetDeadReply(that, f) { if (f) (that)->flags |= tfIsDeadReply; else (that)->flags &= ~tfIsDeadReply; }
#define binder_transaction_SetEvent(that, f) { if (f) (that)->flags |= tfIsEvent; else (that)->flags &= ~tfIsEvent; }
#define binder_transaction_SetAcquireReply(that, f) { if (f) (that)->flags |= tfIsAcquireReply; else (that)->flags &= ~tfIsAcquireReply; }
#define binder_transaction_SetFreePending(that, f) { if (f) (that)->flags |= tfIsFreePending; else (that)->flags &= ~tfIsFreePending; }
	
#define binder_transaction_Code(that) ((that)->code)
	
#define binder_transaction_Priority(that) ((that)->priority)
#define binder_transaction_SetPriority(that, pri) { (that)->priority = pri; }

			
#endif
