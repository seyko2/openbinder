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

#ifndef __SUPPORT_PROFILEIPC_H
#define __SUPPORT_PROFILEIPC_H

#include <support/SupportDefs.h>

#if BUILD_TYPE == BUILD_TYPE_DEBUG

#include <support/Debug.h>
#include <support/ITextStream.h>
#include <support/Locker.h>
#include <support/Autolock.h>
#include <support/CallStack.h>
#include <support/KeyedVector.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

struct ipc_call_state
{
	SCallStack	stack;
	nsecs_t	startTime;

protected:
	// Must provide subclass that does stack.Update().
	inline ipc_call_state() { }
	inline ~ipc_call_state() { }
};

struct ipc_stats
{
#if 1 // FIXME: Linux
	mutable	SysCriticalSectionType	cs;
#else
	mutable	CriticalSectionType	cs;
#endif	
			int32_t				dumpPeriod;
			int32_t				maxItems;
			int32_t				printSymbols;
			const char*			printLabel;
			size_t				hits;
			nsecs_t				startTime;
			size_t				totalCalls;

			ipc_stats*			nextReady;

			struct ipc_item
			{
				size_t		count;
				nsecs_t		time;
				
				inline ipc_item()
					:	count(0), time(0)
				{
				}
				inline ipc_item(const ipc_item& o)
					:	count(o.count), time(o.time)
				{
				}
			};

			SKeyedVector<SCallStack, ipc_item>
								calls;
	
								ipc_stats(	int32_t _dumpPeriod, int32_t _maxItems,
											int32_t _printSymbols, const char* _printLabel);
	virtual						~ipc_stats();
	
			void				lock() const;
			void				unlock() const;
			
			void				beginCall(ipc_call_state& state);
			void				finishCall(ipc_call_state& state);

			void				print();

			void				reset();

private:
								ipc_stats(const ipc_stats& o);
};

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif	// BUILD_TYPE == BUILD_TYPE_DEBUG

#endif	// __SUPPORT_PROFILEIPC_H
