/*
 * Copyright (c) 2005 Palmsource, Inc.
 * 
 * This software is licensed as described in the file LICENSE, which
 * you should have received as part of this distribution. The terms
 * are also available at http://www.openbinder.org/license.html.
 * 
 * This software consists of voluntary contributions made by many
 * individuals. For the exact contribution history, see the revision
 * history and logs, available at http://www.openbinder.org
 */

#ifndef _TLS_H
#define	_TLS_H	1

#include <support/SupportDefs.h>

#include <SysThread.h>

// KILL THIS FILE PLEASE!

#ifdef __cplusplus
extern "C" {
#endif

// <support/TLS.h>

static inline int32_t tls_allocate()
{
	int32_t index = -1;
	SysTSDAllocate((SysTSDSlotID*)&index, NULL, sysTSDAnonymous);
	return index;
}

static inline void* tls_get(int32_t index)
{
	return SysTSDGet((SysTSDSlotID)index);
}

static inline void tls_set(int32_t index, void *value)
{
	SysTSDSet((SysTSDSlotID)index, value);
}


#ifdef __cplusplus
}
#endif

#endif
