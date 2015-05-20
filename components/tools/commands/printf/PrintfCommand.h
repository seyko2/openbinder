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

#ifndef FORTUNE_COMMAND_H_
#define FORTUNE_COMMAND_H_

#include <support/Package.h>
#include <app/BCommand.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::app;
#endif

class BPrintfCommand : public BCommand, private SPackageSptr
{
public:
	BPrintfCommand(const SContext& context);

	SValue Run(const SValue& args);
	virtual SString Documentation() const;
};

#endif // FORTUNE_COMMAND_H_
