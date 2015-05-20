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

#ifndef _SUPPORT_EVENT_FLAG_H
#define _SUPPORT_EVENT_FLAG_H

/*!	@file support/EventFlag.h
	@ingroup CoreSupportUtilities
	@brief Event flag synchronization primitive.
*/

#include <support/SupportDefs.h>
#include <support/Value.h>

#include <pthread.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SEventFlag
{
public:
				SEventFlag();
				~SEventFlag();					// dtor not virtal
	
	status_t	Set();
	status_t	Clear();
	status_t	Wait(nsecs_t timeout = -1);
	
private:
	explicit	SEventFlag(const SEventFlag &o);

	uint32_t		m_bits;
	pthread_cond_t	m_cond;
	pthread_mutex_t	m_mutex;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif // _SUPPORT_EVENT_FLAG_H
