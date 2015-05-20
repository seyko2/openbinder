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

#include <support/EventFlag.h>
#include <support/StdIO.h>

#include <libpalmroot.h>
#include <pthread.h>
#include <assert.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

enum {
	CLEARED = 0,
	SET = 1
};

/* LINUX_DEMO_HACK: this implementation is untested.  It removed the previous
 * functionality of having multiple bits, which was unused.  It has been
 * simplified so we don't have to test more than we need, if anyone will ever
 * test it, which I doubt.  It's only intended to support the crap-layer
 * EventQueue.cpp.
 */
	
SEventFlag::SEventFlag()
	:m_bits(SET)
{
	pthread_cond_init(&m_cond, NULL);
	pthread_mutex_init(&m_mutex, NULL);
}


SEventFlag::~SEventFlag()
{
	pthread_cond_destroy(&m_cond);
	pthread_mutex_destroy(&m_mutex);
}


status_t
SEventFlag::Set()
{
	pthread_mutex_lock(&m_mutex);
	if (m_bits != SET) {
		m_bits = SET;
		pthread_cond_broadcast(&m_cond);
	}
	pthread_mutex_unlock(&m_mutex);
	return B_OK;
}


status_t
SEventFlag::Clear()
{
	pthread_mutex_lock(&m_mutex);
	m_bits = CLEARED;
	pthread_mutex_unlock(&m_mutex);
	return B_OK;
}


status_t
SEventFlag::Wait(nsecs_t timeout)
{
	status_t err;
	timespec abstime;
	
	pthread_mutex_lock(&m_mutex);
	
	if (m_bits == SET) {
		goto done;
	}
	
	if (timeout != -1) {
		clock_gettime(CLOCK_REALTIME, &abstime);
		palmos::clock_timespec_add_nano(&abstime, timeout);
	}
	
	int result;
loop:	
	if (timeout == -1) {
		result = pthread_cond_wait(&m_cond, &m_mutex);
	} else {
		result = pthread_cond_timedwait(&m_cond, &m_mutex, &abstime);
	}
	
	if (result == EINTR) {
		goto loop;
	}
	
done:
	
	m_bits = CLEARED;
	   
	pthread_mutex_unlock(&m_mutex);
	
	return B_OK;
}
	

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

