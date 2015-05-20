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

#include "SampleComponent.h"

#include <support/StdIO.h>

// These macros are optimizations for creating static SValue and
// SString objects that reside in your executable's bss section
// and don't need to have their constructor/destructor called.
B_STATIC_STRING_VALUE_LARGE(k_world, "world", );
B_STATIC_STRING_VALUE_LARGE(k_hello, "hello", );
B_STATIC_STRING_VALUE_LARGE(k_documentation, "documentation", );

SampleComponent::SampleComponent(const SContext& context, const SValue & /*attr*/)
	: BCommand(context)
{
}

SampleComponent::~SampleComponent()
{
}

SValue SampleComponent::Run(const ArgList& args)
{
	const size_t N = args.CountItems();
	
	// Retrieve first argument -- command name
	SValue me(0 < N ? args[0] : B_UNDEFINED_VALUE);

	// Retrieve second argument -- someone else
	SString you(1 < N ? args[1].AsString() : SString());

	// If second argument wasn't supplied, give a default value
	if (you == "") you = LoadString(k_world);

	// Print a message!  Yes, the localization is dorky, but
	// it's just to demonstrate the APIs.
	TextOutput() << me.AsString() << " " << LoadString(k_hello) << " " << you << "!" << endl;

	// Return success
	return SValue::Status(B_OK);
}

SString SampleComponent::Documentation() const
{
	return LoadString(k_documentation);
}
