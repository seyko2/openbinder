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

#include <support/StdIO.h>
#include <support/InstantiateComponent.h>

#include "SettingsCatalog.h"

sptr<IBinder> InstantiateComponent(	const SString& component,
									const SContext& context,
									const SValue& args)
{
	if (component == "")
	{
		return BSettingsCatalog::CreateRoot()->AsBinder();
	}

	return NULL;
}
