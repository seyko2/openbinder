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

#include <SysThread.h>
#include <CmnErrors.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>

namespace palmos {

// precondition: well-formed timespec, with 0 <= .tv_nsec < 10^9
void clock_timespec_add_nano(struct timespec * t, nsecs_t adj)
{
	assert(t->tv_nsec >= 0 && t->tv_nsec < B_ONE_SECOND);
	
	// residue of nanoseconds: nanoseconds fragment from timespec, with
	// total timeout (in nanoseconds): we have to carry seconds
	nsecs_t part_nano = t->tv_nsec + adj;

	t->tv_sec += part_nano / B_ONE_SECOND;
	t->tv_nsec = part_nano % B_ONE_SECOND;
	if (t->tv_nsec < 0) { // requires tv_nsec to be signed field
		// the residue of a negative adjustment can result
		// in a negative nanoseconds field. Since it will
		// be abolutely less than one second, we can compensate
		// simply by borrowing back one second
		t->tv_sec -= 1;
		t->tv_nsec += B_ONE_SECOND;
	}
}

void clock_timespec_from_nano(struct timespec * t, nsecs_t desired)
{
	t->tv_sec = desired / B_ONE_SECOND;
	t->tv_nsec = desired % B_ONE_SECOND;

	// This would probably mean a desired time before the clock epoch.
	// For CLOCK_REALTIME that is unlikely, but if another clock started
	// at system start, then this might make sense.
	
	if (t->tv_nsec < 0) { // requires tv_nsec to be signed field
		// the residue of a negative adjustment can result
		// in a negative nanoseconds field. Since it will
		// be abolutely less than one second, we can compensate
		// simply by borrowing back one second
		t->tv_sec -= 1;
		t->tv_nsec += B_ONE_SECOND;
	}
}

} // end namespace palmos

// FIXME_LINUX: Not run time, but actual GMT time. Consider changing this to CLOCK_MONOTONIC

nsecs_t SysGetRunTime(void)
{
	struct timespec t;	
	clock_gettime(CLOCK_REALTIME, &t);
	
	return ((nsecs_t)t.tv_sec * B_ONE_SECOND) + t.tv_nsec;
}

status_t SysThreadDelay(nsecs_t timeout, timeoutFlags_t flags)
{
	if (timeout == 0 || flags == B_WAIT_FOREVER)
	{
	  /* Sleep forever */
          select(0, NULL,NULL,NULL, NULL /*timeout */);
	  return -1; /* By definition, an interrupt occured */
	}
	
	struct timespec rqtp;

        status_t err;
	
	if (flags == B_RELATIVE_TIMEOUT) {
	  palmos::clock_timespec_from_nano(&rqtp, timeout);
	  
	  do
	  {
	    err = clock_nanosleep(CLOCK_REALTIME, 0, &rqtp, &rqtp); // relative internal
	  } while (err == -1 && errno == EINTR);
	  
        } else if (flags == B_ABSOLUTE_TIMEOUT) {
	  palmos::clock_timespec_from_nano(&rqtp, timeout);
	  
          err = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &rqtp, &rqtp); // absolute (i.e., relative to UNIX epoch)
        } else {
          err = -1; // Bad parameter, I suppose
        }

	return err;
}
