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

#include "Service.h"
#include "ServiceCommand.h"

#include <support/InstantiateComponent.h>

// -----------------------------------------------------------------

// Given a component name
// and the context it is being instantiated in, return the
// IBinder for a new instance of the component.  The "context"
// is the IContext in which this new component is to be running
// in.
sptr<IBinder> InstantiateComponent(	const SString& component,
									const SContext& context,
									const SValue &args)
{
	if (component == "")
	{
		return static_cast<BnNode*>(new Service(context, args));
	}
	else if (component == "Command")
	{
		return static_cast<BnCommand*>(new ServiceCommand(context, args));
	}

	return NULL;
}
