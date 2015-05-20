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

#ifndef _SUPPORT_P_INTRUSIVE_PROFILER_H
#define _SUPPORT_P_INTRUSIVE_PROFILER_H

#include <PalmTypes.h>
#include <sys/types.h>

struct DProfileTarget {
	nsecs_t		time;
	uint32_t	hits;
	char const * const target;
	DProfileTarget(char const * const t);
	~DProfileTarget();
	void Stats(void);
};

class DAutoProfile {
	nsecs_t	start;
	DProfileTarget &target;
public:
	DAutoProfile(DProfileTarget &dpt);
	~DAutoProfile();
};

#if !defined(INTRUSIVE_PROFILING)
#define INTRUSIVE_PROFILING 0
#endif

#if INTRUSIVE_PROFILING > 0
#define DPT(x) static DProfileTarget dg_ ## x (# x)
#define DAP(x) DAutoProfile _dap(dg_ ## x)
#else
#define DPT(x)
#define DAP(x)
#endif
#endif
