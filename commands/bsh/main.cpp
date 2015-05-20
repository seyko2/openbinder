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

#include <support/SupportDefs.h>
#include <support/StdIO.h>
#include <support/Looper.h>

#include <app/ICommand.h>

int main(int argc, char* argv[])
{
	// This function retrieves the default (user) Binder context,
	// providing a back-door to accessing the Binder system when
	// not started as a component.
	SContext context(get_default_context());
	if (context.InitCheck() != B_OK) {
		fprintf(stderr, "bsh: Unable to retrieve Binder context.\n");
		fprintf(stderr, "bsh: The smooved server is probably not running.\n");
		return context.InitCheck();
	}
	
	// Instantiate the Binder Shell component.
	sptr<ICommand> shell = ICommand::AsInterface(context.New(SValue::String("org.openbinder.tools.BinderShell")));
	if (shell == NULL) {
		fprintf(stderr, "bsh: Unable to instantiate Binder Shell component.\n");
		return -ENOMEM;
	}
	
	// Connect the shell to this process's default io streams.
	shell->SetByteInput(StandardByteInput());
	shell->SetByteOutput(StandardByteOutput());
	shell->SetByteError(StandardByteOutput());
	
	// Run the shell.	
	ICommand::ArgList args;
	for (int i=0; i<argc; i++) args.AddItem(SValue::String(argv[i]));
	SValue result = shell->Run(args);
	return result.AsSSize();
}
