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

#include "ProfileIPC.h"

#if BUILD_TYPE == BUILD_TYPE_DEBUG

#include <support/StdIO.h>
#include <support/StringIO.h>
#include <support/Looper.h>
#include <support/ConditionVariable.h>
#include <sys/time.h>

#include <SysThread.h>
#include <SysThreadConcealed.h>
#include <DebugMgr.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#if TARGET_HOST == TARGET_HOST_PALMOS

static SysCriticalSectionType g_readyLock;
static SysConditionVariableType g_readyCondition;
static SysThreadGroupHandle g_readyGroup = NULL;
static ipc_stats* g_readyStats = NULL;
static bool g_readyDone = false;

static void stat_print_thread(void*)
{
	//bout << "*** IPC statistics printing thread has started!" << endl;

	while (!g_readyDone) {
		SysConditionVariableWait(&g_readyCondition, NULL);
		
		SysCriticalSectionEnter(&g_readyLock);
		//printf("IPC statistics being printed...\n");
		SysConditionVariableClose(&g_readyCondition);
		while (g_readyStats) {
			g_readyStats->print();
			ipc_stats* next = g_readyStats->nextReady;
			g_readyStats->nextReady = NULL;
			g_readyStats = next;
		}
		SysCriticalSectionExit(&g_readyLock);
	}
}

static void stats_ready(ipc_stats* stats)
{
	SysCriticalSectionEnter(&g_readyLock);
	if (stats->nextReady != NULL) {
		// Already in list -- leave it.
		SysCriticalSectionExit(&g_readyLock);
		return;
	}
	if (g_readyStats == NULL) SysConditionVariableOpen(&g_readyCondition);
	stats->nextReady = g_readyStats;
	g_readyStats = stats;
	//DbgMessage("Ready to print IPC statistics!\n");
	if (g_readyGroup == NULL) {
		g_readyGroup = SysThreadGroupCreate();
		SysHandle thread;
		SysThreadCreate(NULL, "ipcp IPC profile printer", sysThreadPriorityHigh, sysThreadStackUI, stat_print_thread, NULL, &thread);
		SysThreadStart(thread);
	}
	SysCriticalSectionExit(&g_readyLock);
}

void __terminate_profile_ipc()
{
	//DbgMessage("Terminating IPC statistics!\n");
	g_readyDone = true;
	SysConditionVariableOpen(&g_readyCondition);
	if(g_readyGroup) {
		SysThreadGroupDestroy(g_readyGroup);
	}
}

#else

static void stats_ready(ipc_stats* stats)
{
	stats->print();
}

void __terminate_profile_ipc()
{
   int bkpnt = 0;
}

#endif

struct calls_by_count
{
	size_t count;
	size_t index;
	
	calls_by_count()
		:	count(0), index(0)
	{
	}
	
	calls_by_count(const calls_by_count& o)
		:	count(o.count), index(o.index)
	{
	}
	
	bool operator<(const calls_by_count& o) const
	{
		if (count != o.count) return count < o.count ? true : false;
		if (index != o.index) return index < o.index ? true : false;
		return false;
	}
};

ipc_stats::ipc_stats(int32_t _dumpPeriod, int32_t _maxItems, int32_t _printSymbols, const char* _printLabel)
	:	
		dumpPeriod(_dumpPeriod), maxItems(_maxItems),
		printSymbols(_printSymbols), printLabel(_printLabel),
		hits(0), startTime(SysGetRunTime()), totalCalls(0),
		nextReady(NULL), cs(sysCriticalSectionInitializer)
{
}

ipc_stats::~ipc_stats()
{
}

void ipc_stats::lock() const
{
#if 1 // FIXME: Linux
	SysCriticalSectionEnter(&cs);
#else
	KALCriticalSectionEnter(&cs);
#endif
}

void ipc_stats::unlock() const
{
#if 1
	SysCriticalSectionExit(&cs);
#else
	KALCriticalSectionExit(&cs);
#endif
}

void ipc_stats::beginCall(ipc_call_state& state)
{
	lock();
	totalCalls++;
	bool found = false;
	ipc_item& item = calls.EditValueFor(state.stack, &found);
	if (found) {
		item.count++;
	} else {
		ipc_item item;
		item.count = 1;
		item.time = 0;
		calls.AddItem(state.stack, item);
	}
	unlock();
	
	state.startTime = SysGetRunTime();
}

void ipc_stats::finishCall(ipc_call_state& state)
{
	lock();
	bool found = false;
	bool print_it = false;
	ipc_item& item = calls.EditValueFor(state.stack, &found);
	if (found) {
		item.time += (SysGetRunTime() - state.startTime);
		hits++;
		print_it = (dumpPeriod > 0) && ((hits%dumpPeriod) == 0);
	}
	unlock();
	
	if (print_it) stats_ready(this);
	//if (print_it) print();
}

void ipc_stats::print()
{
	lock();
	SSortedVector<calls_by_count> byCount;
	
	const nsecs_t totalTime = SysGetRunTime()-startTime;
	const size_t N = calls.CountItems();
	size_t i;
	int32_t count;
	
	for (i=0; i<N; i++) {
		const ipc_item& item = calls.ValueAt(i);
		calls_by_count countItem;
		countItem.count = item.count;
		countItem.index = i;
		byCount.AddItem(countItem);
	}
	
	// Can't do this because we may get to this point during the
	// construction of a SAtom object, which fails due to the
	// trickery in SAtom for allocating memory. :(
	//sptr<BStringIO> sio(new BStringIO);
	//sptr<ITextOutput> blah(sio.ptr());

	bout << "Profile summary of " << printLabel << " in process " << SysProcessName() << " (#" << SysProcessID() << "):" << endl << indent;
	bout << "Showing " << maxItems << " of " << N
		<< " call stacks (" << totalCalls << " total calls using " << totalTime << "us) by count" << endl;
	count = 0;
	i = byCount.CountItems();
	while (count < maxItems && i > 0) {
		i--;
		count++;
		const size_t itIndex = byCount[i].index;
		const ipc_item& item = calls.ValueAt(itIndex);
		if (printSymbols) {
			bout << "#" << count << " " << item.count << " calls ("
				<< (item.count/double(totalTime)*1000000) << " per sec) for "
				<< item.time << "us:" << endl << indent;
			calls.KeyAt(itIndex).LongPrint(bout);
			bout << dedent;
		} else {
			bout << calls.KeyAt(itIndex) << " / count=" << item.count
				<< " / time=" << item.time << endl;
		}
	}
	
	bout << dedent;

	unlock();

	//bout << sio->String();
}

void
ipc_stats::reset()
{
	lock();
	hits = 0;
	totalCalls = 0;
	calls.MakeEmpty();
	unlock();
	bout << "resetting ipc_stats" << endl;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif
