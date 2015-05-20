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

#ifndef _SUPPORT_VECTORIO_H
#define _SUPPORT_VECTORIO_H

/*!	@file support/VectorIO.h
	@ingroup CoreSupportUtilities
	@brief A specialized vector for storing an iovec array.
*/

#include <support/SupportDefs.h>

#include <sys/uio.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*--------------------------------------------------------*/
/*----- SVectorIO class ----------------------------------*/

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SVectorIO
{
public:
						SVectorIO();
virtual					~SVectorIO();

		const iovec*	Vectors() const;
inline					operator const iovec*() const			{ return Vectors(); }

		ssize_t			CountVectors() const;
		size_t			DataLength() const;
		
		ssize_t			AddVector(void* base, size_t length);
		ssize_t			AddVector(const iovec* vector, size_t count = 1);
		void*			Allocate(size_t length);
		
		void			MakeEmpty();
		void			SetError(status_t err);
		
		ssize_t			Write(void* buffer, size_t avail) const;
		ssize_t			Read(const void* buffer, size_t avail) const;
		
private:

virtual	status_t		_ReservedVectorIO1();
virtual	status_t		_ReservedVectorIO2();
virtual	status_t		_ReservedVectorIO3();
virtual	status_t		_ReservedVectorIO4();
virtual	status_t		_ReservedVectorIO5();
virtual	status_t		_ReservedVectorIO6();
virtual	status_t		_ReservedVectorIO7();
virtual	status_t		_ReservedVectorIO8();
virtual	status_t		_ReservedVectorIO9();
virtual	status_t		_ReservedVectorIO10();

						SVectorIO(const SVectorIO& o);
		SVectorIO&		operator=(const SVectorIO& o);

		enum {
			MAX_LOCAL_VECTOR	= 16,
			MAX_LOCAL_DATA		= 256
		};
		
		struct alloc_data {
			size_t	size;
			size_t	next_pos;
		};
		
		iovec					m_localVector[MAX_LOCAL_VECTOR];
		SVector<iovec>*			m_heapVector;
		ssize_t					m_vectorCount;
		size_t					m_totalLength;
		
		alloc_data				m_localData;
		uint8_t					m_localDataBuffer[MAX_LOCAL_DATA];
		SVector<alloc_data*>*	m_heapData;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif
