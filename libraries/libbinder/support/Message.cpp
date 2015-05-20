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

#include <support/Message.h>

#include <support/ITextStream.h>

#include <ErrorMgr.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// Standard message fields.
static const SValue g_keyWhat("b:what");
static const SValue g_keyWhen("b:when");
static const SValue g_keyPriority("b:pri");
static const SValue g_keyData("b:data");

SMessage::SMessage()
	:	m_what(0), m_when(0), m_priority(B_NORMAL_PRIORITY),
		m_next(NULL), m_prev(NULL)
{
}

SMessage::SMessage(uint32_t what)
	:	m_what(what), m_when(0), m_priority(B_NORMAL_PRIORITY),
		m_next(NULL), m_prev(NULL)
{
}

SMessage::SMessage(uint32_t what, int32_t priority)
	:	m_what(what), m_when(0), m_priority(priority),
		m_next(NULL), m_prev(NULL)
{
}

SMessage::SMessage(const SMessage &o)
	:	m_what(o.m_what), m_when(o.m_when), m_priority(o.m_priority),
		m_data(o.m_data),
		m_next(NULL), m_prev(NULL)
{
	ErrFatalErrorIf(m_when < 0, "Watch your times, honky!");
}

SMessage::SMessage(const SValue &o)
	:	m_data(o[g_keyData]),
		m_next(NULL), 
		m_prev(NULL)
{
	status_t err;
	m_what = o[g_keyWhat].AsInteger();
	m_when = o[g_keyWhen].AsTime();
	ErrFatalErrorIf(m_when < 0, "Watch your times, honky!");
	m_priority = o[g_keyPriority].AsInt32(&err);
	if (err != B_OK) m_priority = B_NORMAL_PRIORITY;
}

SMessage::~SMessage()
{
}
	
SMessage& SMessage::operator=(const SMessage &o)
{
	m_what = o.m_what;
	m_when = o.m_when;
	m_priority = o.m_priority;
	m_data = o.m_data;
	return *this;
}

SValue SMessage::AsValue() const
{
	SValue result;
	if (m_what) result.JoinItem(g_keyWhat, SValue::Int32(m_what));
	if (m_when) result.JoinItem(g_keyWhen, SValue::Time(m_when));
	if (m_priority != B_NORMAL_PRIORITY) result.JoinItem(g_keyPriority, SValue::Int32(m_priority));
	result.JoinItem(g_keyData, m_data);
	return result;
}

status_t SMessage::PrintToStream(const sptr<ITextOutput>& io, uint32_t flags) const
{
#if SUPPORTS_TEXT_STREAM
	if (flags&B_PRINT_STREAM_HEADER) io << "SMessage ";
	
	io << "{" << endl << indent
		<< "what = " << STypeCode(m_what) << " (" << (void*)m_what << ")" << endl
		<< "when = " << m_when << endl
		<< "priority = " << m_priority << endl
		<< "data = ";
	m_data.PrintToStream(io);
	io << endl << dedent << "}";
	return B_OK;
#else
	(void)io;
	(void)flags;
	return B_UNSUPPORTED;
#endif
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SMessage& message)
{
#if SUPPORTS_TEXT_STREAM
	message.PrintToStream(io, B_PRINT_STREAM_HEADER);
#else
	(void)message;
#endif
	return io;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
