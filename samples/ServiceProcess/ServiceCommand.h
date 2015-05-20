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

#ifndef SERVICE_COMMAND_H
#define SERVICE_COMMAND_H

#include <app/BCommand.h>
#include <support/Package.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::app;
#endif

class ServiceCommand : public BCommand, public SPackageSptr
{
public:
									ServiceCommand(	const SContext& context,
														const SValue &attr);

	virtual	SValue					Run(const ArgList& args);
	virtual	SString					Documentation() const;

protected:
	virtual							~ServiceCommand();
};


#endif // SERVICE_COMMAND_H
