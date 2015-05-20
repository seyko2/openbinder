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

#ifndef _SUPPORT_BUFFER_CHAIN_H_
#define _SUPPORT_BUFFER_CHAIN_H_

#include <support/Buffer.h>
#include <support/StdIO.h>

#include <string.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

template<class BUFFERTYPE>
class SBufferChain
{
	public:
		inline				SBufferChain();
		inline				SBufferChain(const SBufferChain<BUFFERTYPE>& chain);
		inline				SBufferChain(const BUFFERTYPE& chain);
		inline				~SBufferChain();

		inline	SBufferChain<BUFFERTYPE>& operator=(const SBufferChain<BUFFERTYPE>& chain);
		inline	SBufferChain<BUFFERTYPE>& operator=(const BUFFERTYPE& chain);

		inline	void		MakeEmpty();
		
		inline	status_t	AppendBuffer(const BUFFERTYPE& buffer);
		inline	status_t	PrependBuffer(const BUFFERTYPE& buffer);
		
		inline	status_t	AppendChain(const SBufferChain<BUFFERTYPE>& chain);
		inline	status_t	AppendChain(const BUFFERTYPE& buffer);
		inline	status_t	PrependChain(const SBufferChain<BUFFERTYPE>& chain);
		inline	status_t	PrependChain(const BUFFERTYPE& buffer);
		
		inline	status_t	RemoveFirst();

		inline	size_t		Size() const;

		BUFFERTYPE *		RootBuffer() const;		

	private:
		enum
		{
			ALLOC_SIZE		= 4
		};
		
		BUFFERTYPE *		m_root;
		BUFFERTYPE *		m_last;
		BUFFERTYPE *		m_free;

		BUFFERTYPE *		next();
		status_t			alloc(size_t capacity);
//		static status_t		copy(const BUFFERTYPE * from, BUFFERTYPE ** root, BUFFERTYPE ** last);
		status_t			init_copy(const BUFFERTYPE* clone);
		void				free_chain(BUFFERTYPE* root);
};

template<class BUFFERTYPE> inline BUFFERTYPE *
SBufferChain<BUFFERTYPE>::next()
{
	if (!m_free)
	{
		status_t err = alloc(ALLOC_SIZE);
		if (err < B_OK) return 0;
	}

	BUFFERTYPE * result = m_free;
	m_free = static_cast<BUFFERTYPE*>(m_free->next);
	result->next = 0;
	return result;
}

template<class BUFFERTYPE> inline status_t
SBufferChain<BUFFERTYPE>::alloc(size_t capacity)
{
	if (m_free != 0) return B_NOT_ALLOWED;

	BUFFERTYPE * result = 0;
	BUFFERTYPE ** i = &result;
	for (size_t n = 0; n < capacity; n++)
	{
		BUFFERTYPE * b = new BUFFERTYPE();
		if (!b) return B_NO_MEMORY;
		*i = b;
		i = (BUFFERTYPE**)(&b->next);
	}
	*i = 0;

	m_free = result;

	return B_OK;
}

#if 0
template<class BUFFERTYPE> inline status_t
SBufferChain<BUFFERTYPE>::copy(const BUFFERTYPE * from, BUFFERTYPE ** root, BUFFERTYPE ** last)
{
	BUFFERTYPE ** i = root;
	for (const BUFFERTYPE * pos = from; pos; pos = static_cast<const BUFFERTYPE*>(pos->next))
	{
		if (m_free)
		{
			*i = m_free;
			m_free = m_free->next;
		}
		else
		{
			*i = new BUFFERTYPE(*pos);
			if (*i == 0) return B_NO_MEMORY;
		}
		*last = *i;
		i = (BUFFERTYPE**)(&(*i)->next);
	}
	*i = 0;
	return B_OK;
}
#endif

template<class BUFFERTYPE> inline void
SBufferChain<BUFFERTYPE>::free_chain(BUFFERTYPE* root)
{
	for (BUFFERTYPE * i = root; i;)
	{
		BUFFERTYPE * next = static_cast<BUFFERTYPE*>(i->next);
		delete i;
		i = next;
	}
}

template<class BUFFERTYPE> inline
SBufferChain<BUFFERTYPE>::SBufferChain()
{
	m_root = 0;
	m_last = 0;
	m_free = 0;
}

template<class BUFFERTYPE> inline status_t
SBufferChain<BUFFERTYPE>::init_copy(const BUFFERTYPE* clone)
{
	m_last = 0;
	m_free = 0;
	m_root = 0;
	
	if (clone != 0)
	{
		return AppendChain(*clone);
	}
	return B_OK;
}

template<class BUFFERTYPE> inline
SBufferChain<BUFFERTYPE>::SBufferChain(const BUFFERTYPE& chain)
{
	init_copy(&chain);
}

template<class BUFFERTYPE> inline
SBufferChain<BUFFERTYPE>::SBufferChain(const SBufferChain<BUFFERTYPE>& chain)
{
	init_copy(chain.m_root);
}

template<class BUFFERTYPE> inline SBufferChain<BUFFERTYPE>&
SBufferChain<BUFFERTYPE>::operator=(const SBufferChain<BUFFERTYPE>& chain)
{
	MakeEmpty();
	AppendChain(chain);
	return *this;
}

template<class BUFFERTYPE> inline SBufferChain<BUFFERTYPE>&
SBufferChain<BUFFERTYPE>::operator=(const BUFFERTYPE& chain)
{
	MakeEmpty();
	AppendChain(chain);
	return *this;
}

template<class BUFFERTYPE> inline
SBufferChain<BUFFERTYPE>::~SBufferChain()
{
	free_chain(m_root);
	free_chain(m_free);
}

template<class BUFFERTYPE> inline void
SBufferChain<BUFFERTYPE>::MakeEmpty()
{
	if (m_last)
	{
		// move all entries to free list
		m_last->next = m_free;
		m_free = m_root;
		
		m_root = 0;
		m_last = 0;
	}
}

template<class BUFFERTYPE> inline status_t
SBufferChain<BUFFERTYPE>::AppendBuffer(const BUFFERTYPE& buffer)
{
	BUFFERTYPE * b = next();
	if (b == 0) return B_NO_MEMORY;
	*b = buffer;
	
	if (m_last == 0)
	{
		m_root = b;
		m_last = b;
	}
	else
	{
		m_last->next = b;
		m_last = b;
	}
	
	return B_OK;
}

template<class BUFFERTYPE> inline status_t
SBufferChain<BUFFERTYPE>::PrependBuffer(const BUFFERTYPE& buffer)
{
	BUFFERTYPE * b = next();
	if (b == 0) return B_NO_MEMORY;
	*b = buffer;
	
	if (m_root == 0)
	{
		m_root = b;
		m_last = b;
		b->next = 0;
	}
	else
	{
		b->next = m_root;
		m_root = b;
	}

	return B_OK;
}

template<class BUFFERTYPE> inline status_t
SBufferChain<BUFFERTYPE>::AppendChain(const SBufferChain<BUFFERTYPE>& chain)
{
	if (chain.m_root != 0)
	{
		return AppendChain(*chain.m_root);
	}
	return B_OK;
}

template<class BUFFERTYPE> inline status_t
SBufferChain<BUFFERTYPE>::AppendChain(const BUFFERTYPE& buffer)
{
	const BUFFERTYPE * src = &buffer;
	while (src != 0)
	{
		BUFFERTYPE * b = next();
		if (b == 0) return B_NO_MEMORY;
		*b = *src;

		if (m_last == 0)
		{
			m_root = b;
			m_last = b;
		}
		else
		{
			m_last->next = b;
			m_last = b;
		}
		
		src = static_cast<const BUFFERTYPE*>(src->next);
	}
	return B_OK;
}

template<class BUFFERTYPE> inline status_t
SBufferChain<BUFFERTYPE>::PrependChain(const SBufferChain<BUFFERTYPE>& chain)
{
	if (chain.m_root != 0)
	{
		return PrependChain(*chain.m_root);
	}
	return B_OK;
}

template<class BUFFERTYPE> inline status_t
SBufferChain<BUFFERTYPE>::PrependChain(const BUFFERTYPE& buffer)
{
	const BUFFERTYPE * src = &buffer;
	while (src != 0)
	{
		BUFFERTYPE * b = next();
		if (b == 0) return B_NO_MEMORY;
		*b = *src;

		if (m_root == 0)
		{
			m_root = b;
			m_last = b;
			b->next = 0;
		}
		else
		{
			b->next = m_root;
			m_root = b;
		}
		
		src = static_cast<const BUFFERTYPE*>(src->next);
	}
	return B_OK;
}

template<class BUFFERTYPE> inline status_t
SBufferChain<BUFFERTYPE>::RemoveFirst()
{
	if (m_root == 0)
	{
		return B_NOT_ALLOWED;
	}

	BUFFERTYPE * newRoot = static_cast<BUFFERTYPE*>(m_root->next);
	m_root->next = m_free;
	m_free = m_root;
	if (m_last == m_root)
	{
		m_last = 0;
	}
	m_root = newRoot;

	return B_OK;
}

template<class BUFFERTYPE> inline BUFFERTYPE *
SBufferChain<BUFFERTYPE>::RootBuffer() const
{
	return m_root;
}

template<class BUFFERTYPE> inline size_t
SBufferChain<BUFFERTYPE>::Size() const
{
	if (m_root)
	{
		return m_root->ChainSize();
	}
	else
	{
		return 0;
	}
}

#if _SUPPORTS_NAMESPACE
} } // palmos::support
#endif

#endif // _SUPPORT_BUFFER_CHAIN_H_

