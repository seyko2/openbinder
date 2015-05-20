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

#ifndef	_SUPPORT_AUTOLOCK_H
#define	_SUPPORT_AUTOLOCK_H

/*!	@file support/Autolock.h
	@ingroup CoreSupportUtilities
	@brief Stack-based automatic locking.
*/

#include <support/Locker.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*-----------------------------------------------------------------*/
/*----- SAutolock class -------------------------------------------*/

/*!	@addtogroup CoreSupportUtilities
	@{
*/

//!	Smart locking class.
/*!	This is a convenience class that automatically releases a lock
	when the class is destroyed.  Some synchronization classes supply
	their own autolockers (for example SLocker::Autolock) that are
	slightly more efficient.  This class works with any synchronization
	object as lock as it returns lock_status_t from its locking
	operation.

	Example usage:
@code
void MyClass::MySynchronizedMethod()
{
	SAutolock _l(m_lock.Lock());

	...
}
@endcode
*/
class SAutolock 
{
public:
	inline				SAutolock(const lock_status_t& status);
	inline				SAutolock();
	inline				~SAutolock();		
	inline	bool		IsLocked();

	inline	void		SetTo(const lock_status_t& status);
	inline	void		Unlock();

private:
						SAutolock(const SAutolock&);
			SAutolock&	operator = (const SAutolock&);

	lock_status_t	m_status;
};

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

inline SAutolock::SAutolock(const lock_status_t& status)
	:	m_status(status)
{
}

inline SAutolock::SAutolock()
	:	m_status(-1)
{
}

inline SAutolock::~SAutolock()
{
	m_status.unlock();
}

inline bool SAutolock::IsLocked()
{
	return m_status.is_locked();
}

inline void SAutolock::SetTo(const lock_status_t& status)
{
	m_status.unlock();
	m_status = status;
}

inline void SAutolock::Unlock()
{
	m_status.unlock();
	m_status = lock_status_t(-1);
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif /* _SUPPORT_AUTOLOCK_H */
