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

#ifndef SAMPLE_COMPONENT_H
#define SAMPLE_COMPONENT_H

#include <app/BCommand.h>
#include <support/Package.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::app;
#endif

// Demonstrates basic a basic component.
//
// This component implements the ICommand interface so you
// can use it from the shell.

class SampleComponent : public BCommand, public SPackageSptr
{
public:
									SampleComponent(	const SContext& context,
														const SValue &attr);

	virtual	SValue					Run(const ArgList& args);
	virtual	SString					Documentation() const;

protected:
	virtual							~SampleComponent();
};


#endif // SAMPLE_COMPONENT_H
