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

#include "vm.h"
#include "bsh.h"

#include <support/StdIO.h>
#include <package/PackageKit.h>
#include <storage/File.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::package;
using namespace palmos::storage;
#endif

B_STATIC_STRING_VALUE_SMALL(key_sh,"sh",)
B_STATIC_STRING_VALUE_SMALL(key_dash_s,"-s",)
B_STATIC_INT32_VALUE(key_0,0,)
B_STATIC_INT32_VALUE(key_1,1,)
B_STATIC_INT32_VALUE(key_2,2,)

VirtualMachine::VirtualMachine(const SContext &context)
	:BnVirtualMachine(context),
	 SPackageSptr()
{
}

void
VirtualMachine::Init()
{
}

sptr<IBinder>
VirtualMachine::InstantiateComponent(const SValue& info, const SString& component,
	const SValue& args, status_t* outError)
{
	status_t err;
	SContext context = Context();
	
	// Load the script from the file
	sptr<IByteInput> data = get_script_data_from_value(info, &err);
	if (data == NULL || err != B_OK) {
		if (outError) *outError = err == B_OK ? B_ERROR : err;
		return NULL;
	}
	
	// Setup the shell and run the script
	sptr<BShell> shell = new BShell(context);
	if (shell == NULL) {
		if (outError) *outError = B_NO_MEMORY;
		return NULL;
	}
	
	shell->SetByteInput(NullByteInput());
	shell->SetByteOutput(StandardByteOutput());
	shell->SetByteError(StandardByteError());
	shell->SetVMArguments(args);
	
	ICommand::ArgList sh_args;
	sh_args.AddItem(key_sh);
	sh_args.AddItem(key_dash_s);
	sh_args.AddItem(SValue::Binder(data->AsBinder()));
	
	SValue res = shell->Run(sh_args);
	if (res.AsStatus() != B_OK) {
		if (outError) *outError = res.AsStatus();
		return NULL;
	}

	if (outError) *outError = B_OK;
	
	// Return the self binder
	sptr<IBinder> self = static_cast<SelfBinder *>(shell.ptr());
	return self;
}
