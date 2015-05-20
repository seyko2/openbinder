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

/*
 * This binder condition variable implements the following
 * methods: Wait, Open, Close and Broadcast, just like
 * SConditionVariable, except that there is no Wait that
 * takes a SLocker, and that it is implemented with a
 * scripting interface.  So it will work across processes
 * but will do an IPC to lock and unlock, so don't do it,
 * because you will suck up lots of resources and waste a
 * lot of time.
 */

#ifndef CV_H
#define CV_H

#include <support/Package.h>
#include <support/ConditionVariable.h>
#include <support/Binder.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

class ConditionVariable : public BBinder, public SPackageSptr
{
public:
						ConditionVariable(const SValue &args);
	virtual				~ConditionVariable();
	
	virtual	status_t	Called(	const SValue& func,
								const SValue& args,
								SValue* out);
private:
	// no copy
	explicit ConditionVariable(const ConditionVariable &);
	
	SConditionVariable m_cv;
	const SString m_name;
};

#endif // CV_H
