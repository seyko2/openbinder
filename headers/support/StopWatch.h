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

#ifndef _SUPPORT_STOP_WATCH_H_
#define _SUPPORT_STOP_WATCH_H_

/*!	@file support/StopWatch.h
	@ingroup CoreSupportUtilities
	@brief Debugging tool for measuring time.
*/

#include <support/SupportDefs.h>

#ifndef _INCLUDES_PERFORMANCE_INSTRUMENTATION
#define _INCLUDES_PERFORMANCE_INSTRUMENTATION 0
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif // _SUPPORTS_NAMESPACE

/*!	@addtogroup CoreSupportUtilities
	@{
*/

// Flags that can be passed in to constructor.
enum {
	B_STOP_WATCH_SILENT			= (1<<0),		// Don't print anything.
	B_STOP_WATCH_NESTING		= (1<<1),		// Peform nesting, print all results when top is done.
	B_STOP_WATCH_CLEAR_CACHE	= (1<<2),		// Clear caches before starting.
	B_STOP_WATCH_RAW			= (1<<3),		// Don't try to adjust values for overhead.
	B_STOP_WATCH_HIGH_PRIORITY	= (1<<4),		// Run thread at a high priority.
	B_STOP_WATCH_QUANTIFY		= (1<<5),		// Enable Quantify instrumentation during the stop watch.
	B_STOP_WATCH_NO_TIME		= (1<<6)		// Don't measure elapsed time.
};

// Flags for StartProfiling()
enum {
	B_PROFILE_KEEP_SAMPLES		= (1<<0),		// If set, any existing samples are kept -- not cleared.
	B_PROFILE_UPLOAD_SAMPLES	= (1<<1)		// Dump results to POD when profiling has finished.
};

// Available performance counters.  These are just the DPXA255 performance
// counter values, there is no abstraction here (yet).
enum perf_counter_t {
	B_COUNT_ICACHE_MISS				= 0,
	B_COUNT_DATA_STALL				= 2,
	B_COUNT_ITLB_MISS				= 3,
	B_COUNT_DTLB_MISS				= 4,
	B_COUNT_BRANCH_INSTRUCTION		= 5,
	B_COUNT_BRANCH_MISPREDICTED		= 6,
	B_COUNT_INSTRUCTION				= 7,
	B_COUNT_DBUFFER_STALL_DURATION	= 8,
	B_COUNT_DBUFFER_STALL			= 9,
	B_COUNT_DCACHE_ACCESS			= 10,
	B_COUNT_DCACHE_MISS				= 11,
	B_COUNT_DCACHE_WRITEBACK		= 12,
	B_COUNT_PC_CHANGE				= 13
};

// Flags for StartPerformance();
enum {
	B_COUNT_HIGH_PRECISION		= (1<<0)		// Use high precision (low range) access to counters.
};

// Counter registers, used for building ratios.
enum perf_counter_reg_t {
	B_COUNT_REGISTER_CYCLES			= 0,		// Special register, always available, of cycle count
	B_COUNT_REGISTER_A				= 1,
	B_COUNT_REGISTER_B				= 2
};

// Standard performance node weights.
enum {
	B_PERFORMANCE_WEIGHT_EMPTY	= 100,			// Function does nothing.
	B_PERFORMANCE_WEIGHT_LIGHT	= 200,			// Function does very little -- get or set a few basic variables.
	B_PERFORMANCE_WEIGHT_MEDIUM	= 300,			// Function does about the same work as the instrumentation.
	B_PERFORMANCE_WEIGHT_HEAVY	= 400			// Function does significantly more work than instrumentation.
};

// This macro inserts a performance instrumentation node at the current location.  It is usually
// used at the top of a function to instrument that function.  The weight is the
#if _INCLUDES_PERFORMANCE_INSTRUMENTATION
#define B_PERFORMANCE_NODE(weight, name) SStopWatch __performance_node__(weight, name)
#else
#define B_PERFORMANCE_NODE(weight, name)
#endif

/*-------------------------------------------------------------*/
/*----- SStopWatch class --------------------------------------*/

class SStopWatch
{
public:
			// Normal constructor.  'name' is the label for this stop watch;
			// the memory it points to must be valid for the lifetime of
			// of the object (or root if nested).  Flags are as above.
			// minWeight is the minimum weight stopwatch to instrument when
			// nesting.  maxItems is the maximum number of nested stop watches
			// that can be stored.
						SStopWatch(const char *name, uint32_t flags = 0, int32_t minWeight = 0, size_t maxItems = 0);

			// Constructor for B_PERFORMANCE_NODE.
						SStopWatch(int32_t weight, const char *name);

						~SStopWatch();

	static	void		FlushICache();

			const char*	Name() const;

			// The following functions enable additional features in the
			// stop watch.  Calling one of these functions implicitly does a
			// Restart() of the stop watch.

			// Call this to run the profiler.
			void		StartProfiling(uint32_t flags = 0);

			// Call this to run the performance counters.  You can optionally
			// call AddPerformanceRatio() (multiple times) to generate additional
			// statistics to display.
			void		AddPerformanceRatio(perf_counter_reg_t numerator, perf_counter_reg_t denominator);
			void		StartPerformance(perf_counter_t cntA, perf_counter_t cntB, uint32_t flags = 0);

			// Completely restart the stop-watching, as if it had been re-created.
			// This means flushing caches, re-computing overhead, and finally
			// calling Mark().
			void		Restart();

			// Mark the current location as the start time of the stop watch.  All
			// metrics will be reported with this as the baseline.
			void		Mark();

			// Start and stop the stop watch timer.  Currently only start and stops
			// time and quantify -- does NOT control profiling or performance counters.
			void		Suspend();
			void		Resume();

			// Return current elapsed  time.  Can be called while stop watch is
			// running.
			nsecs_t		ElapsedTime() const;

			// Stop measurement.  Can not be restarted.
			void		Stop();
			void		StopFast();

			// Retrieve final measurements.  Only valid after Stop() has been
			// called.
			nsecs_t		TotalTime() const;
			int64_t		TotalPerformance(perf_counter_reg_t reg) const;

			struct		Node;
			struct		Root;

/*----- Private or reserved ---------------*/
private:

						SStopWatch(const SStopWatch &);
			SStopWatch	&operator=(const SStopWatch &);
			
			void		InitChild(Root* root, const char* name);
			void		MarkInternal(Root* root);
			void		RestartInternal(Root* root);
			void		StopInternal(Root* root);

			Root*		m_root;
			Node*		m_node;
			Node*		m_parent;
			int32_t		m_type;
};

/*-------------------------------------------------------------*/
/*----- SPerformanceSample class ------------------------------*/

class SPerformanceSample
{
public:
						SPerformanceSample();
						~SPerformanceSample();

private:
			uint32_t	m_counters[3];
};

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

/*!	@} */

#endif /* _SUPPORT_STOP_WATCH_H_ */
