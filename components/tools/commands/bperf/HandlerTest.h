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

#ifndef _HANDLER_TEST_H
#define _HANDLER_TEST_H

#include <support/atomic.h>
#include <support/Handler.h>
#include <support/Package.h>
#include <support/ConditionVariable.h>
#include <support/SharedBuffer.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

enum {
	kTestHandler = 'TEST'
};

struct handler_test_state
{
	int32_t				iterations;
	int32_t				priority;
	size_t				dataSize;

	volatile int32_t	current;
	volatile int32_t	numThreads;
	SConditionVariable	finished;
};

class HandlerTest : public BHandler, public SPackageSptr
{
public:

	HandlerTest()
	{
		SHOW_OBJS(bout << "HandlerTest " << this << " constructed..." << endl);
	}

	void Init(handler_test_state* state, sptr<HandlerTest> other = sptr<HandlerTest>(NULL))
	{
		m_state = state;
		if (other != NULL) {
			m_other = other;
		} else {
			m_other = this;
		}
	}

	void RunTest()
	{
		m_state->current = 0;
		m_state->finished.Close();

		SMessage msg(kTestHandler);
		msg.SetPriority(m_state->priority);
		if (m_state->dataSize > 0) {
			SSharedBuffer* buf = SSharedBuffer::Alloc(m_state->dataSize);
			msg.JoinItem(SValue::String("dat"), SValue(B_RAW_TYPE, buf));
			buf->DecUsers();
		}
		PostMessage(msg);
		m_state->finished.Wait();
	}

	virtual	status_t HandleMessage(const SMessage &msg)
	{
		if (msg.What() == kTestHandler) 
		{
			if (SysAtomicAdd32(&m_state->current, 1) < m_state->iterations) 
			{
				sptr<HandlerTest> next = m_other.promote();
				if (next != NULL) 
				{
					next->PostMessage(msg);
				}
			} 
			else 
			{
				m_state->finished.Open();
			}
			return B_OK;
		}

		return BHandler::HandleMessage(msg);
	}

protected:
	virtual ~HandlerTest()
	{
		SHOW_OBJS(bout << "HandlerTest " << this << " destroyed..." << endl);
	}

private:
	wptr<HandlerTest>	m_other;
	handler_test_state*	m_state;
};

#endif /* _TRANSACTION_TEST_H */
