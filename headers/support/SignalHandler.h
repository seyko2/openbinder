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

#ifndef _SUPPORT_SIGNAL_HANDLER_H
#define _SUPPORT_SIGNAL_HANDLER_H

#include <support/Atom.h>
#include <SysThread.h>
#include <signal.h>
#include <sys/wait.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

class SSignalHandler : public virtual SAtom
{
public:
	SSignalHandler();
	
	virtual bool OnSignal(int32_t sig, const siginfo_t* si, const void* ucontext) = 0;

	status_t RegisterFor(int32_t sig);
	status_t UnregisterFor(int32_t sig);

	void Block(int32_t sig);
	void Unblock(int32_t sig);

protected:
	virtual ~SSignalHandler();
};

class SChildSignalHandler : public SSignalHandler
{
public:
	SChildSignalHandler();
	
	virtual bool OnSignal(int32_t sig, const siginfo_t* si, const void* ucontext);
	virtual bool OnChildSignal(int32_t sig, const siginfo_t* si, const void* ucontext, int pid, int status, const struct rusage * usage) = 0;

	void BlockChildHandling();
	void UnblockChildHandling();

protected:
	virtual ~SChildSignalHandler();
};

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_SIGNAL_HANDLER_H */

