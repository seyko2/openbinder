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

#ifndef VM_H
#define VM_H

#include <support/IVirtualMachine.h>
#include <support/Package.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

class VirtualMachine : public BnVirtualMachine, public SPackageSptr
{
public:
	VirtualMachine(const SContext &context);

	virtual void Init();
	virtual	sptr<IBinder> InstantiateComponent(const SValue& componentInfo,
				const SString& component,
				const SValue& args,
				status_t* outError = NULL);
};


#endif // VM_H
