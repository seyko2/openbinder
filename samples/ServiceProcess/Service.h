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

#ifndef SERVICE_H
#define SERVICE_H

#include <support/Locker.h>
#include <support/Package.h>
#include <support/Catalog.h>

#if _SUPPORTS_NAMESPACE
using namespace vendor::sample;
#endif

class Service : public BCatalog, public SPackageSptr
{
public:
									Service(const SContext& context,
											const SValue &attr);

protected:
	virtual							~Service();
	virtual	void					InitAtom();
};


#endif // SERVICE_H
