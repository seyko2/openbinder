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

#include "ServiceCommand.h"

#include <support/StdIO.h>

ServiceCommand::ServiceCommand(const SContext& context, const SValue & /*attr*/)
	: BCommand(context)
{
}

ServiceCommand::~ServiceCommand()
{
}

SValue ServiceCommand::Run(const ArgList& args)
{
	const size_t N = args.CountItems();
	if (N < 2) {
		TextOutput() << "service_command: no command supplied" << endl;
		return SValue::Status(B_OK);
	}
	
	SString cmd(args[1].AsString());

	sptr<IBinder> service(Context().LookupService(SString("service_process")));
	
	if (cmd == "start") {
		if (service != NULL) {
			TextOutput() << "service_command start: service already running." << endl;
		} else {
			service = Context().New(SValue::String("org.openbinder.samples.ServiceProcess"));
			if (service == NULL) {
				TextOutput() << "service_command: unable to create service." << endl;
				return SValue::Status(B_ERROR);
			}
		}
		Context().PublishService(SString("service_process"), service);
		return SValue::Binder(service);
	} else if (cmd == "stop") {
		if (service == NULL) {
			TextOutput() << "service_command: service not running" << endl;
		}
		Context().Unpublish(SString("/services/service_process"));
		return SValue::Status(B_OK);
	}

	TextOutput() << "service_command: unknown command " << cmd << endl;
	return SValue::Status(B_BAD_VALUE);
}

SString ServiceCommand::Documentation() const
{
	return SString("service_command start|stop");
}
