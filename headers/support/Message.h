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

#ifndef _SUPPORT_MESSAGE_H
#define _SUPPORT_MESSAGE_H

/*!	@file support/Message.h
	@ingroup CoreSupportUtilities
	@brief Data object that is processed by SHandler.
*/

#include <support/SupportDefs.h>
#include <support/ITextStream.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SMessageList;

/*-------------------------------------------------------------*/

//!	Data object that is processed by SHandler.
class SMessage
{
public:

						SMessage();
						SMessage(uint32_t what);
						SMessage(uint32_t what, int32_t priority);
						SMessage(const SMessage &);
						SMessage(const SValue &);
						~SMessage();

		SMessage&		operator=(const SMessage &);

inline	uint32_t		What() const;
inline	void			SetWhat(uint32_t val);
		
inline	nsecs_t			When() const;
inline	void			SetWhen(nsecs_t val);
		
inline	int32_t			Priority() const;
inline	void			SetPriority(int32_t val);
		
inline	const SValue&	Data() const;
inline	SValue&			Data();
inline	void			SetData(const SValue& data);
		
		// Convenience functions.  These are the same as calling
		// the corresponding function on Data().
inline	void			Join(const SValue& from, uint32_t flags = 0);
inline	void			JoinItem(const SValue& key, const SValue& value, uint32_t flags=0);
inline	const SValue&	ValueFor(const SValue& key) const;
inline	const SValue&	ValueFor(const char* key) const;
inline	const SValue&	operator[](const SValue& key) const		{ return ValueFor(key); }
inline	const SValue&	operator[](const char* key) const		{ return ValueFor(key); }
		
		SValue			AsValue() const;
inline					operator SValue() const			{ return AsValue(); }

		status_t		PrintToStream(const sptr<ITextOutput>& io, uint32_t flags = 0) const;

private:

friend	class			SMessageList;

		uint32_t		m_what;
		nsecs_t			m_when;
		int32_t			m_priority;
		SValue			m_data;
		
		SMessage *		m_next;
		SMessage *		m_prev;
};

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SMessage& message);

/*!	@} */

/*----------------------------------------------------------------*/
/*----- inline definitions ---------------------------------------*/

inline uint32_t SMessage::What() const
{
	return m_what;
}

inline void SMessage::SetWhat(uint32_t val)
{
	m_what = val;
}

inline nsecs_t SMessage::When() const
{
	return m_when;
}

inline void SMessage::SetWhen(nsecs_t val)
{
	m_when = val;
}

inline int32_t SMessage::Priority() const
{
	return m_priority;
}

inline void SMessage::SetPriority(int32_t val)
{
	m_priority = val;
}

inline const SValue& SMessage::Data() const
{
	return m_data;
}

inline SValue& SMessage::Data()
{
	return m_data;
}

inline void SMessage::SetData(const SValue& data)
{
	m_data = data;
}

inline void SMessage::Join(const SValue& from, uint32_t flags)
{
	m_data.Join(from, flags);
}

inline void SMessage::JoinItem(const SValue& key, const SValue& value, uint32_t flags)
{
	m_data.JoinItem(key, value, flags);
}

inline const SValue& SMessage::ValueFor(const SValue& key) const
{
	return m_data.ValueFor(key);
}

inline const SValue& SMessage::ValueFor(const char* key) const
{
	return m_data.ValueFor(key);
}

/*-------------------------------------------------------------*/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _MESSAGE_H */
