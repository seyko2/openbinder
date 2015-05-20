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

#ifndef _SUPPORT__OBSERVER_H
#define _SUPPORT__OBSERVER_H

/*!	@file support/Observer.h
	@ingroup CoreSupportBinder
	@brief Generic target of Binder links.
*/

#include <support/Binder.h>
#include <support/Handler.h>
#include <support/Locker.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportBinder
	@{
*/

//!	Generic target of Binder links.
class BObserver : public BBinder
{
public:
						BObserver(const SContext& context);

protected:
	virtual				~BObserver();

	//!	Override this to receive observed events.
	virtual void		Observed(const SValue& key, const SValue& value) = 0;

	//!	Capture effect calls and send through DispatchEffect().
	virtual	status_t	HandleEffect(	const SValue &in,
										const SValue &inBindings,
										const SValue &outBindings,
										SValue *out);
	//!	Turn an effect into into a series of Observed() calls.
	virtual	status_t	DispatchEffect(const SValue& told);
};

enum {
	B_SERIAL_OBSERVER_MSG = 'sero'
};

//!	BObserver that ensures Observed() is thread-safe.
class BSerialObserver : public BObserver, public SHandler
{
public:
						BSerialObserver(const SContext& context);

	virtual	status_t	HandleMessage(const SMessage &msg);

	//!	Get and set the (thread) priority at which we dispatch.
	/*!	Default is B_NORMAL_PRIORITY. */
			void		SetPriority(int32_t priority);
			int32_t		Priority() const;

protected:
	virtual				~BSerialObserver();
	virtual	void		InitAtom();

	virtual	status_t	DispatchEffect(const SValue& told);

	//!	Manual synchronization for derived classes.
	/*!	Derived classes can acquire SerialLock and
		serialize other actions with HandleMessage/Observed
		(both are serialized here by using SHandler(&m_lock)) */
	SLocker&			SerialLock() const	{ return m_lock; }

private:
	mutable	SLocker		m_lock;
			int32_t		m_priority;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // palmos::support
#endif

#endif // _SUPPORT__OBSERVER_H
