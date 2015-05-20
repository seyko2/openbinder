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

#ifndef _SUPPORT_MEMORY_H_
#define _SUPPORT_MEMORY_H_

/*!	@file support/Memory.h
	@ingroup CoreSupportBinder
	@brief Simple implementation of IMemoryHeap.
*/
#include <support/Binder.h>
#include <support/IInterface.h>
#include <support/Value.h>
#include <support/IMemory.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportBinder
	@{
*/

#define MEMORYDEALER_NAME_TEMPLATE "/tmp/memoryarea.%s.%d"

//////////////////////////////////////////////////////////////////
// BMemoryHeap
//////////////////////////////////////////////////////////////////

//!	A simple IMemoryHeap implementation.
class BMemoryHeap : public BnMemoryHeap
{
public:
							BMemoryHeap(int32_t area, void* basePtr);
		virtual				~BMemoryHeap();
		virtual	int32_t		HeapID() const;
		virtual void *		HeapBase() const;
private:
				int32_t		m_area;
				void *		m_basePtr;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif // _SUPPORT_MEMORY_H_
