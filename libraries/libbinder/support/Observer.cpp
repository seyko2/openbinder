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

#include <support/Observer.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

BObserver::BObserver(const SContext& context)
	: BBinder(context)
{
}

BObserver::~BObserver()
{
}

status_t BObserver::HandleEffect(	const SValue &in,
									const SValue &inBindings,
									const SValue & /*outBindings*/,
									SValue * /*out*/)
{
	return DispatchEffect(inBindings * in);
}

status_t BObserver::DispatchEffect(const SValue& told)
{
	SValue key, value;
	void* cookie = NULL;
	while (told.GetNextItem(&cookie, &key, &value) == B_OK)
	{
		Observed(key, value);
	}

	return B_OK;
}


BSerialObserver::BSerialObserver(const SContext& context)
	: BObserver(context)
	, SHandler(&m_lock)
	, m_lock("SerialObserver")
{
	m_priority = B_NORMAL_PRIORITY;
}

BSerialObserver::~BSerialObserver()
{
}

void BSerialObserver::InitAtom()
{
	BObserver::InitAtom();
	BHandler::InitAtom();
}

void BSerialObserver::SetPriority(int32_t priority)
{
	m_priority = priority;
}

int32_t BSerialObserver::Priority() const
{
	return m_priority;
}

status_t BSerialObserver::HandleMessage(const SMessage &msg)
{
	// notice that since we pass a lock into SHandler,
	// that m_lock is acquired over the duration of this
	// function
	if (msg.What() == B_SERIAL_OBSERVER_MSG) {
		return BObserver::DispatchEffect(msg.Data());
	}
	return B_OK;
}

status_t BSerialObserver::DispatchEffect(const SValue& told)
{
	SMessage msg(B_SERIAL_OBSERVER_MSG, m_priority);
	msg.SetData(told);
	return PostMessage(msg);
}
