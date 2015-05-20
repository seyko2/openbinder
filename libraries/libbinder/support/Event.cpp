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

#include <support/Event.h>
#include <support/atomic.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

SEvent::SEvent()
	: fCountWaiting(0)
{
    SysSemaphoreCreateEZ(0, &fSem);
}

SEvent::~SEvent()
{
	if (fSem >= B_OK) Shutdown();
}

status_t SEvent::InitCheck() const
{
	return (fSem >= B_OK) ? B_OK : fSem;
}

void SEvent::Shutdown()
{
	if (fSem >= B_OK)
		SysSemaphoreDestroy(fSem);
	
	fSem = B_NO_INIT;
}

status_t SEvent::Wait(uint32_t flags, nsecs_t timeout)
{
	atomic_add(&fCountWaiting,1);
	
	status_t err;
	nsecs_t starttime = SysGetRunTime(), remainingtime = timeout;

	/*NOTE: the original acquire_sem_etc never returned a value = B_INFINITE_TIMEOUT.
	   1st condition of the while loop always returned false.
	   For this reason the original while loop was replaced with the following code*/
	timeoutFlags_t  timeoutflags = 
	   ((flags & B_TIMEOUT) && remainingtime != B_INFINITE_TIMEOUT) ?
	   B_RELATIVE_TIMEOUT : B_WAIT_FOREVER;
	err= SysSemaphoreWait(fSem, timeoutflags, remainingtime);
	
	
	if (err != B_OK && !(err != B_TIMED_OUT && timeout > 0)) {
		int32_t newVal,oldVal = fCountWaiting;
		do {
			newVal = oldVal;
			if (newVal > 0) newVal--;
		} while (!cmpxchg32(&fCountWaiting,&oldVal,newVal));
		if (newVal == oldVal)
		   SysSemaphoreWait(fSem, B_WAIT_FOREVER, 0);
	};
	
	return err;
}

int32_t SEvent::FireAll()
{
	int32_t oldVal = fCountWaiting;
	while (!cmpxchg32(&fCountWaiting,&oldVal,0)) ;
	//bout << "SEvent::FireAll releasing semaphore " << oldVal << " times." << endl;
	if (oldVal > 0) SysSemaphoreSignalCount(fSem, oldVal);
	return oldVal;
}

int32_t SEvent::Fire(int32_t count)
{
	int32_t newVal,oldVal = fCountWaiting;
	do {
		newVal = oldVal - count;
		if (newVal < 0) newVal = 0;
	} while (!cmpxchg32(&fCountWaiting,&oldVal,newVal));
	if (newVal < oldVal) SysSemaphoreSignalCount(fSem, oldVal-newVal);
	return (oldVal-newVal);
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
