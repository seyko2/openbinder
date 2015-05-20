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

#include <support/StopWatch.h>
#include <support/StdIO.h>
#include <support/Locker.h>
#include <support/Vector.h>

#if TARGET_HOST != TARGET_HOST_LINUX

#include <driver/Profiler.h>
#include <driver/PerformanceCounter.h>
#include <IOS.h>

#include <SysThread.h>
#include <Kernel.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif // _SUPPORTS_NAMESPACE

/* --------------------------------------------------------- */

// The Quantify API that we need.
#if TARGET_PLATFORM == TARGET_PLATFORM_PALMSIM_WIN32
static int avoidGy_44;
static int avoidGy_45;
__declspec(dllexport) int __cdecl QuantifyStartRecordingData(void) { avoidGy_44++; return 0; }
__declspec(dllexport) int __cdecl QuantifyStopRecordingData(void) { avoidGy_45++; return 0; }
#endif

// Efficient access to Intel performance counters.

#if TARGET_PLATFORM == TARGET_PLATFORM_DEVICE_ARM
extern "C" uint32_t ReadPerformanceCounter(uint32_t counter);

SLocker g_cacheLock;
void (*g_flushCacheFunc)();			// this is leaked.  i don't care.
enum { ICACHE_SIZE = 32*1024 };

void SStopWatch::FlushICache()
{
	if (!g_flushCacheFunc) {
		g_cacheLock.Lock();
		if (!g_flushCacheFunc) {
			g_flushCacheFunc = (void (*)())malloc(ICACHE_SIZE+4);
			uint32_t* instr = (uint32_t*)g_flushCacheFunc;
			for (size_t i=0; i<(ICACHE_SIZE/4); i++) {
				*instr++ = 0xe1a00000;		// nop
			}
			*instr++ = 0xe12fff1e;		// bx lr
			KALSynchronizeCache((VAddr)g_flushCacheFunc, ICACHE_SIZE+4);
		}
		g_cacheLock.Unlock();
	}

	(*g_flushCacheFunc)();
}
#else
void SStopWatch::FlushICache()
{
	KALSynchronizeCache((VAddr)&FlushICache, 0x10000);
}
#endif

/* --------------------------------------------------------- */

enum {
	NUM_COUNTERS = 2,
	MAX_RATIOS = 8
};

enum {
	EXTENSION_PROFILING = 0x0001,
	EXTENSION_PERFORMANCE = 0x0002,
	EXTENSION_PERFORMANCE_HIGH = 0x0004,
	EXTENSION_QUANTIFY_RUNNING = 0x0008,
	EXTENSION_STOPPED = 0x0010
};

struct SStopWatch::Node
{
	int32_t		Count() const;

	Node*		prev;		// Previous node at the current level
	Node*		children;	// List of child nodes

	const char*	name;

	int32_t		lap;

	nsecs_t		time;

	int64_t		counters[NUM_COUNTERS+1];
};

struct SStopWatch::Root
{
	void		PrintReport(const sptr<ITextOutput>& out) const;
	void		PrintNode(const sptr<ITextOutput>& out, const Node* node, int32_t level) const;

	const char*	PerformanceCounterName(int8_t reg) const;
	const char*	PerformanceCounterAbbreviation(int8_t reg) const;

	uint32_t	flags;
	uint32_t	extensions;
	int32_t		minWeight;
	uint8_t		origPriority;
	uint8_t		pad0[3];

	// Timing
	nsecs_t		suspendTime;

	// Profiling
	status_t	profError;
	uint32_t	profFlags;

	// Performance
	status_t	perfError;
	uint32_t	perfFlags;
	int32_t		perfCounters[NUM_COUNTERS];
	int32_t		perfNumRatios;
	int64_t		tmpPerf[NUM_COUNTERS+1];
	int8_t		perfRatioNums[MAX_RATIOS];
	int8_t		perfRatioDenoms[MAX_RATIOS];

	// Calculated overhead for retrieving time and counters.
	nsecs_t		timeOverhead;
	int64_t		counterOverhead[NUM_COUNTERS+1];

	Node*		currentNode;

	size_t		numNodes;
	size_t		curNodeIndex;

	Node		top;
	// Additional nodes follow.
};

enum {
	TYPE_PLAIN, TYPE_ROOT, TYPE_CHILD
};

static SLocker g_lock("SStopWatch globals");
static SysTSDSlotID g_nestingTSD = 0;

static int32_t g_profileFD = -1;
static int32_t g_counterFD = -1;

/* --------------------------------------------------------- */

static SStopWatch::Root* CurrentRootSlow();

static inline SStopWatch::Root* CurrentRoot()
{
	if (g_nestingTSD != 0) return (SStopWatch::Root*)SysTSDGet(g_nestingTSD);
	return CurrentRootSlow();
}

static SStopWatch::Root* CurrentRootSlow()
{
	g_lock.Lock();
	if (g_nestingTSD == 0) {
		SysTSDAllocate(&g_nestingTSD, NULL, sysTSDAnonymous);
	}
	g_lock.Unlock();
	return (SStopWatch::Root*)SysTSDGet(g_nestingTSD);
}

static inline void SetCurrentRoot(SStopWatch::Root* root)
{
	SysTSDSet(g_nestingTSD, root);
}

static int32_t ProfileFDSlow();

static inline int32_t ProfileFD()
{
	if (g_profileFD >= 0) return g_profileFD;
	return ProfileFDSlow();
}

static int32_t ProfileFDSlow()
{
	g_lock.Lock();
	if (g_profileFD < 0) {
		status_t err;
		g_profileFD = IOSOpen(B_PROFILER_DRIVER_NAME, 0, &err);
	}
	g_lock.Unlock();
	return g_profileFD;
}

static int32_t CounterFDSlow();

static inline int32_t CounterFD()
{
	if (g_counterFD >= 0) return g_counterFD;
	return CounterFDSlow();
}

static int32_t CounterFDSlow()
{
	g_lock.Lock();
	if (g_counterFD < 0) {
		status_t err;
		g_counterFD = IOSOpen(B_PERFORMANCE_COUNTER_DRIVER_NAME_BASE "PXA250-PMU", 0, &err);
	}
	g_lock.Unlock();
	return g_counterFD;
}

/* --------------------------------------------------------- */

int32_t SStopWatch::Node::Count() const
{
	const Node* c = children;
	int32_t num = 1;
	while (c) {
		num = num + c->Count();
		c = c->prev;
	}

	return num;
}

/* --------------------------------------------------------- */

void SStopWatch::Root::PrintReport(const sptr<ITextOutput>& out) const
{
	if (top.prev || top.children  || (flags&B_STOP_WATCH_RAW) == 0) {
		out << "STOP WATCH RESULTS";
		if ((flags&B_STOP_WATCH_RAW) == 0) {
			Node node;
			node.prev = NULL;
			node.children = NULL;
			node.name = " -- overhead";
			node.lap = 0;
			node.time = timeOverhead*2;
			for (int i=0; i<NUM_COUNTERS+1; i++) {
				node.counters[i] = counterOverhead[i]*2;
			}
			PrintNode(out, &node, 0);
		} else {
			out << endl;
		}
		PrintNode(out, &top, 0);
	} else {
		out << "StopWatch ";
		PrintNode(out, &top, 0);
	}
}

static const char* MakeIndent(int32_t indent)
{
	static const char space[] =
		"                                         "
		"                                         "
		"                                         "
		"                                         ";

	int32_t off = sizeof(space) - (indent*2) - 1;
	if( off < 0 ) off = 0;
	return space+off;
}

void SStopWatch::Root::PrintNode(const sptr<ITextOutput>& out, const Node* node, int32_t level) const
{
	if (node->prev) PrintNode(out, node->prev, level);

	nsecs_t time = node->time;
	int64_t counters[NUM_COUNTERS+1];
	memcpy(counters, node->counters, sizeof(counters));

	if ((flags&B_STOP_WATCH_RAW) == 0) {
		// The number of times we performed measurements is twice
		// the number of nodes at this leaf in the tree, minus one
		// because the top-most node only included one such call
		// in its measurement.
		const int32_t N = (node->Count()*2) - 1;
		time -= timeOverhead*N;
		for (int i=0; i<NUM_COUNTERS+1; i++) {
			counters[i] -= counterOverhead[i]*N;
		}
	}

	out << MakeIndent(level) << node->name << ": ";
	if ((flags&B_STOP_WATCH_NO_TIME) == 0) {
		out << (time/1000) << "us";
	}

	if (extensions&(EXTENSION_PERFORMANCE|EXTENSION_PERFORMANCE_HIGH)) {
		if ((flags&B_STOP_WATCH_NO_TIME) == 0) {
			out << ", ";
		}
		out << counters[0] << " " << PerformanceCounterName(B_COUNT_REGISTER_A)
			<< ", " << counters[1] << " " << PerformanceCounterName(B_COUNT_REGISTER_B)
			<< ", " << counters[2] << " " << PerformanceCounterName(B_COUNT_REGISTER_CYCLES);
		for (int i=0; i<perfNumRatios; i++) {
			int64_t n = perfRatioNums[i] == B_COUNT_REGISTER_CYCLES
				? counters[NUM_COUNTERS] : counters[perfRatioNums[i]-1];
			int64_t d = perfRatioDenoms[i] == B_COUNT_REGISTER_CYCLES
				? counters[NUM_COUNTERS] : counters[perfRatioDenoms[i]-1];
			if (d == 0) d = -1;
			out << ", " << (double(n)/double(d))
				<< " " << PerformanceCounterAbbreviation(perfRatioNums[i])
				<< "/" << PerformanceCounterAbbreviation(perfRatioDenoms[i]);
		}
	}

	out << endl;

	if (node->children) {
		PrintNode(out, node->children, level+1);
	}
}

const char*	SStopWatch::Root::PerformanceCounterName(int8_t reg) const
{
	static const char* const names[] = {
		"I-Miss", "---", "D-Stall", "ITLB-Miss", "DTLB-Stall", "B-Instr", "B-Miss", "Instr",
		"DBuf-Stall-Dur", "DBuf-Stall", "DCache-Access", "DCache-Miss", "DCache-WB", "PC-Change"
	};

	if (reg == B_COUNT_REGISTER_CYCLES) return "Cycles";
	return names[perfCounters[reg-1]];
}

const char*	SStopWatch::Root::PerformanceCounterAbbreviation(int8_t reg) const
{
	static const char* const names[] = {
		"IMs", "---", "DStl", "ITLB", "DTLB", "Br", "BMs", "Ins",
		"DBDur", "DBStl", "DC", "DCMs", "DCWB", "PC"
	};

	if (reg == B_COUNT_REGISTER_CYCLES) return "Cyc";
	return names[perfCounters[reg-1]];
}

/* --------------------------------------------------------- */

// Normal constructor.
SStopWatch::SStopWatch(const char *name, uint32_t flags, int32_t minWeight, size_t maxItems)
{
	Root* root = m_root = CurrentRoot();

	// If this stop watch is nesting, use the existing context.
	if (root != NULL) {
		InitChild(root, name);
		return;
	}

	const size_t numNodes = maxItems ? maxItems : (flags&B_STOP_WATCH_NESTING) ? 10000 : 0;
	root = m_root = (Root*)malloc(sizeof(Root) + sizeof(Node)*numNodes);
	memset(root, 0, sizeof(Root) + sizeof(Node)*numNodes);
	Node* node = m_node = &root->top;

	root->flags = flags;
	root->extensions = 0;
	root->minWeight = minWeight;
	root->suspendTime = 0;
	root->perfNumRatios = 0;
	root->currentNode = node;
	root->numNodes = numNodes;
	root->curNodeIndex = 0;

	node->prev = NULL;

	m_type = TYPE_PLAIN;
	m_parent = NULL;

	if ((flags&B_STOP_WATCH_NESTING) != 0) {
		SetCurrentRoot(root);
		m_type = TYPE_ROOT;
	}

	if ((flags&B_STOP_WATCH_HIGH_PRIORITY) != 0) {
		KALThreadInfoType info;
		KeyID thread = (KeyID)SysCurrentThread();
		KALThreadGetInfo(thread, kKALThreadInfoTypeCurrentVersion, &info);
		root->origPriority = info.currentPriority;
		KALThreadChangePriority(thread, sysThreadPriorityHigh);
	}

	node->children = NULL;
	node->name = name;
	node->time = 0;
	node->lap = 0;

	RestartInternal(root);
	MarkInternal(root);
}

// Constructor for child-only stop watches.
SStopWatch::SStopWatch(int32_t weight, const char *name)
{
	Root* root = m_root = CurrentRoot();
	if (root == NULL) return;
	if (root->minWeight > weight) {
		m_root = NULL;
		return;
	}

	InitChild(root, name);
}

void SStopWatch::InitChild(Root* root, const char* name)
{
	m_type = TYPE_CHILD;

	ErrFatalErrorIf(++root->curNodeIndex >= root->numNodes, "Out of memory for stop watch nodes!");

	Node* node = m_node = (&root->top) + root->curNodeIndex;

	m_parent = root->currentNode;
	root->currentNode = node;
	node->prev = m_parent->children;
	m_parent->children = node;

	node->children = NULL;
	node->name = name;
	node->time = 0;
	node->lap = 0;

	RestartInternal(root);
	MarkInternal(root);
}

/* --------------------------------------------------------- */

SStopWatch::~SStopWatch()
{
	Stop();
	if (m_type != TYPE_CHILD) free(m_root);
}

/* --------------------------------------------------------- */

void SStopWatch::Stop()
{
	Root* const root = m_root;

	if (root && (root->extensions&EXTENSION_STOPPED) == 0) {
		StopInternal(root);
	}
};

void SStopWatch::StopFast()
{
	StopInternal(m_root);
}

void SStopWatch::StopInternal(Root* root)
{
#if TARGET_PLATFORM == TARGET_PLATFORM_DEVICE_ARM
	if ((root->extensions&EXTENSION_PERFORMANCE_HIGH) != 0) {
		root->tmpPerf[0] = ReadPerformanceCounter(0);
		root->tmpPerf[1] = ReadPerformanceCounter(1);
		root->tmpPerf[2] = ReadPerformanceCounter(2);
	}
#endif

	Node* const node = m_node;

	if ((root->flags&B_STOP_WATCH_NO_TIME) == 0) {
		node->time = ElapsedTime();
	}

#if TARGET_PLATFORM == TARGET_PLATFORM_PALMSIM_WIN32
	if ((root->extensions&EXTENSION_QUANTIFY_RUNNING) != 0) {
		QuantifyStopRecordingData();
	}
#endif

	if ((root->extensions&EXTENSION_PERFORMANCE) != 0) {
		status_t err;
		IOSFastIoctl(CounterFD(), PERFCNT_READ_COUNTERS(NUM_COUNTERS+1),
						0, NULL, sizeof(root->tmpPerf), root->tmpPerf, &err);
	}

	if ((root->extensions&(EXTENSION_PERFORMANCE|EXTENSION_PERFORMANCE_HIGH)) != 0) {
		for (int i=0; i<NUM_COUNTERS+1; i++) {
			if (root->tmpPerf[i] <= node->counters[i])
				node->counters[i] = (root->tmpPerf[i]+0x10000)-node->counters[i];
			else
				node->counters[i] = root->tmpPerf[i]-node->counters[i];
		}
	}

	if (m_type == TYPE_CHILD) {
		root->currentNode = m_parent;
		return;
	}

	if ((root->extensions&EXTENSION_PROFILING) != 0) {
		status_t err;
		IOSFastIoctl(ProfileFD(), profilerStopProfiling, 0, NULL, 0, NULL, &err);
	}

	if ((root->flags&B_STOP_WATCH_HIGH_PRIORITY) != 0) {
		KALThreadChangePriority((KeyID)SysCurrentThread(), root->origPriority);
	}

	if ((root->flags&B_STOP_WATCH_SILENT) == 0) {
		root->PrintReport(bout);
	}

	if ((root->extensions&EXTENSION_PROFILING) != 0
			&& (root->profFlags&B_PROFILE_UPLOAD_SAMPLES) != 0) {
		status_t err;
		IOSFastIoctl(ProfileFD(), profilerSendSamplesToPOD, 0, NULL, 0, NULL, &err);
	}

	SetCurrentRoot(NULL);
	root->extensions |= EXTENSION_STOPPED;
}

/* --------------------------------------------------------- */

void	SStopWatch::StartProfiling(uint32_t flags)
{
	Root* root = m_root;

	if (root == NULL || m_type == TYPE_CHILD) return;

	root->extensions |= EXTENSION_PROFILING;
	root->profFlags = flags;
	root->profError = B_OK;

	RestartInternal(root);
	MarkInternal(root);
}

/* --------------------------------------------------------- */

void		SStopWatch::AddPerformanceRatio(perf_counter_reg_t numerator, perf_counter_reg_t denominator)
{
	if (m_root == NULL || m_type == TYPE_CHILD) return;

	if (m_root->perfNumRatios < MAX_RATIOS) {
		m_root->perfRatioNums[m_root->perfNumRatios] = numerator;
		m_root->perfRatioDenoms[m_root->perfNumRatios] = denominator;
		m_root->perfNumRatios++;
	}
}

/* --------------------------------------------------------- */

void		SStopWatch::StartPerformance(perf_counter_t cntA, perf_counter_t cntB, uint32_t flags)
{
	Root* root = m_root;

	if (root == NULL || m_type == TYPE_CHILD) return;

	root->extensions |= (flags&B_COUNT_HIGH_PRECISION) ? EXTENSION_PERFORMANCE_HIGH : EXTENSION_PERFORMANCE;
	root->perfFlags = flags;
	root->perfCounters[0] = cntA;
	root->perfCounters[1] = cntB;

	RestartInternal(root);
	MarkInternal(root);
}

/* --------------------------------------------------------- */

void	SStopWatch::Suspend()
{
	Root* root = m_root;

	if (root->suspendTime == 0)
		root->suspendTime = KALGetTime(B_TIMEBASE_RUN_TIME);

#if TARGET_PLATFORM == TARGET_PLATFORM_PALMSIM_WIN32
	if ((root->extensions&EXTENSION_QUANTIFY_RUNNING) != 0) {
		QuantifyStopRecordingData();
		root->extensions &= ~EXTENSION_QUANTIFY_RUNNING;
	}
#endif
}

/* --------------------------------------------------------- */

void	SStopWatch::Resume()
{
	Root* root = m_root;

#if TARGET_PLATFORM == TARGET_PLATFORM_PALMSIM_WIN32
	if ((root->flags&B_STOP_WATCH_QUANTIFY) != 0 && (root->extensions&EXTENSION_QUANTIFY_RUNNING) == 0) {
		QuantifyStartRecordingData();
		root->extensions |= EXTENSION_QUANTIFY_RUNNING;
	}
#endif

	if (root->suspendTime != 0) {
		m_node->time += KALGetTime(B_TIMEBASE_RUN_TIME) - root->suspendTime;
		root->suspendTime = 0;
	}
}

/* --------------------------------------------------------- */

const char *SStopWatch::Name() const
{
	return m_node->name;
}

/* --------------------------------------------------------- */

nsecs_t	SStopWatch::ElapsedTime() const
{
	if (m_root->suspendTime == 0) {
		return KALGetTime(B_TIMEBASE_RUN_TIME) - m_node->time;
	} else {
		return m_root->suspendTime - m_node->time;
	}
}

/* --------------------------------------------------------- */

nsecs_t	SStopWatch::TotalTime() const
{
	if ((m_root->extensions&EXTENSION_STOPPED) != 0) {
		nsecs_t time = m_node->time;
		if ((m_root->flags&B_STOP_WATCH_RAW) == 0) {
			time -= m_root->timeOverhead;
		}
		return time;
	}
	return -1;
}

/* --------------------------------------------------------- */

int64_t SStopWatch::TotalPerformance(perf_counter_reg_t reg) const
{
	if ((m_root->extensions&EXTENSION_STOPPED) != 0) {
		const size_t i = reg == B_COUNT_REGISTER_CYCLES ? NUM_COUNTERS : reg-1;
		nsecs_t cnt = m_node->counters[i];
		if ((m_root->flags&B_STOP_WATCH_RAW) == 0) {
			cnt -= m_root->counterOverhead[i];
		}
		return cnt;
	}
	return -1;
}

/* --------------------------------------------------------- */

void	SStopWatch::Restart()
{
	Root* root = m_root;
	if (root) {
		RestartInternal(root);
		MarkInternal(root);
	}
}

/* --------------------------------------------------------- */

void	SStopWatch::Mark()
{
	if (m_root) MarkInternal(m_root);
}

/* --------------------------------------------------------- */

void	SStopWatch::RestartInternal(Root* root)
{
	if (m_type != TYPE_CHILD) {
		if ((root->extensions&(EXTENSION_PERFORMANCE|EXTENSION_PERFORMANCE_HIGH)) != 0) {
			IOSFastIoctl(CounterFD(), PERFCNT_CONFIGURE_COUNTERS(NUM_COUNTERS),
						sizeof(root->perfCounters), root->perfCounters, 0, NULL, &root->perfError);
			//if (root->perfError != B_OK) root->extensions &= ~EXTENSION_PERFORMANCE;
		}
		if ((root->extensions&EXTENSION_PROFILING) != 0) {
			// Stopping returns an error code if already stopped, so ignore this result.
			IOSFastIoctl(ProfileFD(), profilerStopProfiling, 0, NULL, 0, NULL, &root->profError);
			root->profError = B_OK;

			if ((root->profFlags&B_PROFILE_KEEP_SAMPLES) == 0) {
				IOSFastIoctl(ProfileFD(), profilerResetSamples, 0, NULL, 0, NULL, &root->perfError);
			}
			if (root->profError == B_OK) {
				IOSFastIoctl(ProfileFD(), profilerStartProfiling, 0, NULL, 0, NULL, &root->perfError);
			}
			if (root->profError != B_OK) root->extensions &= ~EXTENSION_PROFILING;
		}

		if ((root->flags&B_STOP_WATCH_CLEAR_CACHE) != 0) {
			// A "big" size says to just blindly flush the whole cache.
			FlushICache();
		}

		// Compute overhead -- baseline
		if ((root->flags&B_STOP_WATCH_NO_TIME) == 0) {
			root->timeOverhead = KALGetTime(B_TIMEBASE_RUN_TIME);
		}
		if ((root->extensions&EXTENSION_PERFORMANCE) != 0) {
			memset(root->counterOverhead, 0, sizeof(root->counterOverhead));
			IOSFastIoctl(CounterFD(), PERFCNT_READ_COUNTERS(NUM_COUNTERS+1),
						0, NULL, sizeof(root->counterOverhead), root->counterOverhead, &root->perfError);
		}
#if TARGET_PLATFORM == TARGET_PLATFORM_DEVICE_ARM
		if ((root->extensions&EXTENSION_PERFORMANCE_HIGH) != 0) {
			root->counterOverhead[0] = ReadPerformanceCounter(0);
			root->counterOverhead[1] = ReadPerformanceCounter(1);
			root->counterOverhead[2] = ReadPerformanceCounter(2);
		}
#endif

		// Some internal stuff we always have to do
		if ((root->flags&B_STOP_WATCH_NESTING) != 0) {
			Root* foo = CurrentRoot();
		}

		// Compute overhead -- total up measurement overhead
		if ((root->flags&B_STOP_WATCH_NO_TIME) == 0) {
			root->timeOverhead = KALGetTime(B_TIMEBASE_RUN_TIME)-root->timeOverhead;
		}
		if ((root->extensions&EXTENSION_PERFORMANCE) != 0) {
			status_t err;
			IOSFastIoctl(CounterFD(), PERFCNT_READ_COUNTERS(NUM_COUNTERS+1), 0, NULL, sizeof(root->tmpPerf), root->tmpPerf, &err);
			for (int i=0; i<NUM_COUNTERS+1; i++) {
				root->counterOverhead[i] = root->tmpPerf[i] - root->counterOverhead[i];
			}
		}
#if TARGET_PLATFORM == TARGET_PLATFORM_DEVICE_ARM
		if ((root->extensions&EXTENSION_PERFORMANCE_HIGH) != 0) {
			root->tmpPerf[0] = ReadPerformanceCounter(0);
			root->tmpPerf[1] = ReadPerformanceCounter(1);
			root->tmpPerf[2] = ReadPerformanceCounter(2);
			for (int i=0; i<NUM_COUNTERS+1; i++) {
				root->counterOverhead[i] = root->tmpPerf[i] - root->counterOverhead[i];
			}
		}
#endif
	}
}

/* --------------------------------------------------------- */

void	SStopWatch::MarkInternal(Root* root)
{
	FlushICache();

	root->suspendTime = 0;

	if ((root->extensions&EXTENSION_PERFORMANCE) != 0) {
		IOSFastIoctl(CounterFD(), PERFCNT_READ_COUNTERS(NUM_COUNTERS+1),
					0, NULL, sizeof(m_node->counters), m_node->counters, &root->perfError);
	}

#if TARGET_PLATFORM == TARGET_PLATFORM_PALMSIM_WIN32
	if ((root->extensions&EXTENSION_QUANTIFY_RUNNING) != 0) {
		QuantifyStopRecordingData();
	}
	if ((root->flags&B_STOP_WATCH_QUANTIFY) != 0) {
		QuantifyStartRecordingData();
		root->extensions |= EXTENSION_QUANTIFY_RUNNING;
	}
#endif

	if ((root->flags&B_STOP_WATCH_NO_TIME) == 0) {
		m_node->time = KALGetTime(B_TIMEBASE_RUN_TIME);
	}

#if TARGET_PLATFORM == TARGET_PLATFORM_DEVICE_ARM
	if ((root->extensions&EXTENSION_PERFORMANCE_HIGH) != 0) {
		m_node->counters[0] = ReadPerformanceCounter(0);
		m_node->counters[1] = ReadPerformanceCounter(1);
		m_node->counters[2] = ReadPerformanceCounter(2);
	}
#endif
}

/* ---------------------------------------------------------------- */

SPerformanceSample::SPerformanceSample()
{
	SStopWatch::FlushICache();
#if TARGET_PLATFORM == TARGET_PLATFORM_DEVICE_ARM
	m_counters[0] = ReadPerformanceCounter(0);
	m_counters[1] = ReadPerformanceCounter(1);
	m_counters[2] = ReadPerformanceCounter(2);
#endif
}

SPerformanceSample::~SPerformanceSample()
{
#if TARGET_PLATFORM == TARGET_PLATFORM_DEVICE_ARM
	uint32_t cur[3];
	cur[0] = ReadPerformanceCounter(0);
	cur[1] = ReadPerformanceCounter(1);
	cur[2] = ReadPerformanceCounter(2);
	printf("%lu %lu %lu\n", cur[0]-m_counters[0], cur[1]-m_counters[1], cur[2]-m_counters[2]);
#endif
}

/* ---------------------------------------------------------------- */

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

/* --------------------------------------------------------- */
/* --------------------------------------------------------- */
/* --------------------------------------------------------- */
#endif // TARGET_HOST != TARGET_HOST_LINUX
