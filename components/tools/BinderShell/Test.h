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

#ifndef _SHELL_TEST_H
#define _SHELL_TEST_H

#include <support/Package.h>
#include <app/BCommand.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::app;
#endif

class Test : public BUnixCommand, public SPackageSptr
{
public:
	Test(const SContext&);

	virtual SValue main(int argc, char** argv);

private:
};

#endif // _SHELL_TEST_H
