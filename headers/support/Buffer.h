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

#ifndef _SUPPORT_BUFFER_H_
#define _SUPPORT_BUFFER_H_

#include <support/SupportDefs.h>
#include <string.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

class SBuffer
{
	public:	// members
		SBuffer *				next;	// next buffer in the chain, or NULL

	public: // methods
		inline					SBuffer();
		inline					SBuffer(void * data, size_t size);
		inline					SBuffer(const SBuffer& clone);
		inline	SBuffer&		operator=(const SBuffer& clone);

		inline	void			MakeEmpty();
		inline	status_t		SetTo(void * data, size_t size);

		inline	bool			IsValid() const;

		// access just this buffer in the chain
		inline	void const *	Data() const;
		inline	void *			WriteData() const;
		inline	size_t			Size() const;

		// access entire chain starting with this buffer
		size_t			ChainSize() const
		{
			size_t s = 0;
			for (SBuffer const * i = this; i != 0; i = i->next)
			{
				s += i->Size();
			}
			return s;
		}
		
		// copy data out of the chain starting with this buffer
		ssize_t			CopyTo(void * dest, size_t size, size_t fromOffset) const
		{
			if (dest == 0) return B_BAD_VALUE;
			ssize_t total = 0;
			SBuffer const * i = this;
			while (size && i)
			{
				if (fromOffset >= i->Size())
				{
					fromOffset -= i->Size();
					i = i->next;
					continue;
				}
				size_t toCopy = i->Size() - fromOffset;
				if (toCopy > size)
				{
					toCopy = size;
				}
				memcpy(dest, (int8_t*)i->Data() + fromOffset, toCopy);
				size -= toCopy;
				total += toCopy;
				if (size)
				{
					fromOffset = 0;
					dest = (int8_t*)dest + toCopy;
					i = i->next;
				}
			}
			return total;
		}
		
	protected:
				size_t			m_size;
				void *			m_data;
};

inline
SBuffer::SBuffer()
	: next(0), m_size(0), m_data(0)
{
}

inline
SBuffer::SBuffer(void * data, size_t size)
	: next(0), m_size(size), m_data(data)
{
}

inline
SBuffer::SBuffer(const SBuffer& clone)
{
	next = clone.next;
	m_size = clone.m_size;
	m_data = clone.m_data;
}

inline SBuffer&
SBuffer::operator=(const SBuffer& clone)
{
	next = clone.next;
	m_size = clone.m_size;
	m_data = clone.m_data;
	return *this;
}

inline void
SBuffer::MakeEmpty()
{
	next = 0;
	m_size = 0;
	m_data = 0;
}

inline status_t
SBuffer::SetTo(void * data, size_t size)
{
	m_size = size;
	m_data = data;
	return B_OK;
}

inline bool
SBuffer::IsValid() const
{
	return (m_data != 0);
}

inline void const *
SBuffer::Data() const
{
	return m_data;
}

inline void *
SBuffer::WriteData() const
{
	return m_data;
}

inline size_t
SBuffer::Size() const
{
	return m_size;
}

#if _SUPPORTS_NAMESPACE
} } // palmos::support
#endif

#endif // _SUPPORT_BUFFER_H_

