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

#include <support_p/IntrusiveProfiler.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

#define PROFILE_CALL_TIMING 0
struct prv_debug_init {
	nsecs_t dg_timer_overhead;
	prv_debug_init();
	~prv_debug_init();
	nsecs_t voidfunc();
};
prv_debug_init::prv_debug_init() {
#if PROFILE_CALL_TIMING
	nsecs_t start = KALGetTime(B_TIMEBASE_RUN_TIME);
	nsecs_t dummy;
	for (int i = 0; i < 99; i++) dummy = KALGetTime(B_TIMEBASE_RUN_TIME);
	dg_timer_overhead = (KALGetTime(B_TIMEBASE_RUN_TIME) - start) / B_MAKE_INT64(100);
	dg_timer_overhead = 0;
#if 0
	start = KALGetTime(B_TIMEBASE_RUN_TIME);
	for (int i = 0; i < 99; i++) dummy = voidfunc();
	dg_timer_overhead -= KALGetTime(B_TIMEBASE_RUN_TIME) - start;

	// dg_timer_overhead = KALGetTime(B_TIMEBASE_RUN_TIME) - start;
	// dg_timer_overhead = 0;
	// dg_timer_overhead = ((dg_timer_overhead << 2) - dg_timer_overhead) >> 2;
	//bout << "dg_timer_overhead " << dg_timer_overhead << " per call pair" << endl;
#endif
#endif
}
prv_debug_init::~prv_debug_init() {
};
nsecs_t
prv_debug_init::voidfunc() {
	return 0;
}
static prv_debug_init dbg;

DProfileTarget::DProfileTarget(char const * const t)
	: time(0), hits(0), target(t)
{
}

DProfileTarget::~DProfileTarget()
{
#if PROFILE_CALL_TIMING
	Stats();
#endif
}

#if PROFILE_CALL_TIMING
static int print_overhead = 1;
#else
static int sDepth = 1;
#endif

void
DProfileTarget::Stats(void)
{
#if PROFILE_CALL_TIMING
	if (print_overhead) {
		bout << "dg_timer_overhead " << dbg.dg_timer_overhead << " per call pair" << endl;
		print_overhead = 0;
	}
	// use the %s to ensure 8-byte alignment for the last two parameters
	bout << SPrintf("%6lu %s %9lld usecs %7lld avg ", hits, "hits", (time / B_MAKE_INT64(1000)), hits ? (time / (B_MAKE_INT64(1000) * (int64_t)hits)) : 0) << target << endl;
#endif
}

DAutoProfile::DAutoProfile(DProfileTarget &dpt)
	: target(dpt)
{
#if PROFILE_CALL_TIMING
	target.hits++;
	start = KALGetTime(B_TIMEBASE_RUN_TIME);
#else
	bout << SPrintf("%*c%s {", sDepth, ' ', target.target) << endl;
	sDepth++;
#endif
}

DAutoProfile::~DAutoProfile()
{
#if PROFILE_CALL_TIMING
	target.time += (KALGetTime(B_TIMEBASE_RUN_TIME) - start) - dbg.dg_timer_overhead;
#else
	sDepth--;
	bout << SPrintf("%*c}" /* %s" */, sDepth, ' ', target.target) << endl;
	if (sDepth == 1) bout << endl;
#endif
}
