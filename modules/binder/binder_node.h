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

#ifndef BINDER2_NODE_H
#define BINDER2_NODE_H

#include "binder_defs.h"
#include "binder_proc.h"

typedef struct binder_node {
	atomic_t		m_primaryRefs;
	atomic_t		m_secondaryRefs;
	void *			m_ptr;
	void *			m_cookie;
	binder_proc_t *	m_team;
} binder_node_t;

int				binder_node_GlobalCount(void);

binder_node_t *	binder_node_init(binder_proc_t *team, void *ptr, void *cookie);
void			binder_node_destroy(binder_node_t *that);
				
void			binder_node_Released(binder_node_t *that);

BND_DECLARE_ACQUIRE_RELEASE(binder_node);
// BND_DECLARE_FORCE_ACQUIRE(binder_node);
				
/*	Super-special AttemptAcquire() that also lets you attempt
	to acquire a secondary ref.  But note that binder_proc_t is
	the ONLY one who can attempt a secondary, ONLY while holding
	its lock, for the simple reason that binder_node's destructor
	unregisters itself from the team.  In other words, it's a
	dihrty hawck.
*/
BND_DECLARE_ATTEMPT_ACQUIRE(binder_node);

/*	Send a transaction to this node. */
// void				binder_node_Send(struct binder_transaction *t);
// void *				binder_node_Ptr(binder_node_t *that);
// binder_proc_t *		binder_node_Home(binder_node_t *that);

#define binder_node_Ptr(that)	((that)->m_ptr)
#define binder_node_Cookie(that)	((that)->m_cookie)
#define binder_node_Home(that)	((that)->m_team)
#define binder_node_IsAlive(that)	(binder_proc_IsAlive((that)->m_team))
#define binder_node_IsRoot(that)	((that)->m_isRoot)
#define binder_node_Send(that, t)	binder_proc_Transact((that)->m_team, t)

#endif // BINDER2_NODE_H
