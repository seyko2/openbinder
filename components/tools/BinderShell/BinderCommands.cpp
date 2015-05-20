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

#include <app/SGetOpts.h>
#include <support/ICatalog.h>
#include <support/IOSStream.h>
#include <support/IProcess.h>
#include <support/Iterator.h>
#include <support/KeyedVector.h>
#include <support/Looper.h>
#include <support/Pipe.h>
#include <support/StdIO.h>
#include <support/Process.h>

#include <DebugMgr.h>
#include <ErrorMgr.h>
#include <SysThread.h>

#include <package_p/PackageManager.h>

#include <time.h>

#include "BinderCommands.h"
#include "ANSIEscapes.h"
#include "bsh.h"

// Set this to 1 to ALWAYS see the publish/unpublish messages
// (otherwise they're shown on DEBUG builds only)
#define SHOW_PUBLISHING		0


#if _SUPPORTS_NAMESPACE
using namespace palmos::package;
using namespace palmos::storage;
using namespace palmos::support;
#endif

B_STATIC_STRING_VALUE_SMALL(value_get,	"get",);
B_STATIC_STRING_VALUE_SMALL(value_set,	"set",);
B_STATIC_STRING_VALUE_SMALL(kStar, "*",);
B_STATIC_STRING_VALUE_SMALL(kSh, "sh",);
B_STATIC_STRING_VALUE_LARGE(kNEW_CONTEXT, "NEW_CONTEXT",);
B_STATIC_STRING_VALUE_LARGE(kSELF, "SELF",);
B_STATIC_STRING_VALUE_SMALL(kDasha, "-a",);
B_STATIC_STRING_VALUE_SMALL(kDashd, "-d",);
B_STATIC_STRING_VALUE_SMALL(kDashi, "-i",);
B_STATIC_STRING_VALUE_SMALL(kDashl, "-l",);
B_STATIC_STRING_VALUE_SMALL(kDasht, "-t",);
B_STATIC_STRING_VALUE_SMALL(kSlash, "/",);
B_STATIC_STRING_VALUE_LARGE(kDashInspect, "--inspect",);
B_STATIC_STRING_VALUE_LARGE(kPurposes, "purposes",);
B_STATIC_STRING_VALUE_LARGE(kPurpose, "purpose",);
B_STATIC_STRING_VALUE_SMALL(kBin, "bin",);
B_STATIC_STRING_VALUE_LARGE(kExit, "exit",);
B_STATIC_STRING_VALUE_LARGE(kSyslog, "syslog",);
B_STATIC_STRING_VALUE_LARGE(kPackagesInterfaces, "/packages/interfaces",);
B_STATIC_STRING_VALUE_LARGE(kPackagesComponents, "/packages/components",);
B_STATIC_STRING_VALUE_LARGE(kCommands, "commands",);
B_STATIC_STRING_VALUE_LARGE(kPackagesBin, "/packages/bin",);
B_STATIC_STRING_VALUE_LARGE(kExamples, "examples",);

BinderCommand::BinderCommand(const SContext& context) : BCommand(context) { }
BinderCommand::~BinderCommand() { }

BHelp::BHelp(const SContext& context)
	:	BinderCommand(context)
{
}

SString BHelp::Documentation() const {
	return SString(
		"usage:\n"
		"    help\n"
		"        Display general information about the\n"
		"        Binder Shell (bsh).\n"
		"\n"
		"   help commands\n"
		"        List all shell commands and the name\n"
		"        of the component that implements them.\n"
		"\n"
		"   help examples\n"
		"        Show examples using shell.\n"
		"\n"
		"   help <commandname>\n"
		"        Show help specific to <commandname>."
	);
}

SValue BHelp::Run(const ArgList& args)
{
	const sptr<ITextOutput> out(TextOutput());

	SValue arg1 = args.CountItems() > 1 ? args[1] : B_UNDEFINED_VALUE;

	if (!arg1.IsDefined()) {

		out <<	"BinderShell (bsh) is a POSIX-compatible shell with some extensions:\n"
				"  * Environment variables, arguments, and results\n"
				"    are typed values.\n"
				"  * $[cmd ...] syntax to capture command result\n"
				"    value (analogous to $() in POSIX to capture\n"
				"    output).\n"
				"  * @{ ... } value-construction syntax:\n"
				"      @{ V1 }  creates a (typed) value\n"
				"      @{ V1->V2 ... }  creates a mapping\n"
				"      @{ V1, V2 ... }  creates a set\n"
				"      @{ V1 +> V2 ... }  replace V2 with V1\n"
				"      @{ V1 +< V2 ... }  replace V1 with V2\n"
				"      @{ V1 - V2 ... }  remove V2 from V1\n"
				"      @{ MAP[KEY] }  looks up key in a mapping\n"
				"\n"
				"      V1, V2, MAP, KEY can be:\n"
				"      $ENVVAR, $[cmd result], @{ VALUE },\n"
				"      \"string\", (string)string, (int32)1,\n"
				"      (float)1.0, (bool)true, (rect)(l,t,r,b).\n"
				"\n"

				"POSIX shell features that aren't yet implemented:\n"
				"  Pipes, switch, job control\n"
				"\n"

				"Use Page Up/Down in Terminal View to scroll\n"
				"through output.\n"
				"\n" ;

		out << (GetColorFlag()
					? "Use " ANSI_GREEN "'help help'" ANSI_NORMAL " for more help options."
					: "Use 'help help' for more help options") << endl;

	} else if (arg1 == kCommands) {

		out << "Available commands:" << endl;

		sptr<IIterable> bin = IIterable::AsInterface(Context().Lookup(kPackagesBin));
		if (bin == NULL) {
			out << "*** /packages/bin not found! ***" << endl;
			return (const SValue&)SSimpleStatusValue(B_NAME_NOT_FOUND);
		}

		
		// First sort the commands.  We don't want them printed
		// in SValue's normal bag order.

		SKeyedVector<SString, SString> commands;
		
		SIterator it(bin.ptr());
		SValue key, value;
		while (it.Next(&key, &value, INode::REQUEST_DATA) == B_OK)
		{
			SString str = key.AsString();
			if (str != "") commands.AddItem(key.AsString(), value.AsString());
		}
		
		// This is a built-in command.
		commands.AddItem(kExit, SString::EmptyString());

		// Now print commands to stdout.

		const size_t N = commands.CountItems();
		size_t i;
		for (i=0; i<N; i++) {
			out << "  " << commands.KeyAt(i);
			SString comp = commands.ValueAt(i);
			if (comp != "") {
				if (GetColorFlag()) out << " (" ANSI_GREEN << comp << ANSI_NORMAL ")";
				else out << " (" << comp << ")";
			}
			out << endl;
		}

	} else if (arg1 == kExamples) {

		out <<
			"Turn on PACE traces:\n"
			"  $ feature set [a68k] 2 4\n"
			"Report device is ARM (emulate ARMlets):\n"
			"  $ feature set [psys] 2 0x123456\n"
			"Show screen updates:\n"
			"  $ screen showu 50\n"
			"Force redraw of screen:\n"
			"  $ screen invalidate\n"
			"Send an update to current form:\n"
			"  $ compat event update\n"
			"Disable view transaction batching:\n"
			"  $ S=$[inspect /services/surface\n"
			"      org.openbinder.view.IInputSink]\n"
			"  $ invoke $S PostEvent @{ \"b:what\"->[_STM],\n"
			"      \"b:data\"->{\"transaction\"->0} }\n"
			"Disable DIA animation:\n"
			"  $ put /services/pen animationEnabled false\n"
			"Run a script from an expansion card:\n"
			"  $ sh -s $[\n"
			"      new org.openbinder.services.MovieServer.vfs_reader\\\n"
			"      @{url->file:////script.sh} ]\n"
			;

	} else {
		sptr<ICommand> cmd = Spawn(arg1.AsString());

		if (!cmd.ptr()) {
			out << "help: command '" << arg1.AsString() << "' not found" << endl;
			return (const SValue&)SSimpleStatusValue(B_NAME_NOT_FOUND);
		} else {
			SString doco = cmd->Documentation();
			if (doco.Length() > 0) {
				out << doco << endl;
			} else {
				out << "help: no documentation available for '" << arg1.AsString() << "'" << endl;
			}
		}
	}

	return (const SValue&)SSimpleStatusValue(B_OK);
}

BSleep::BSleep(const SContext& context)
	:	BinderCommand(context)
{
}

SString BSleep::Documentation() const {
	return SString(
		"usage: sleep <seconds>\n"
		"\n"
		"snooze()s the shell thread for the specified number of seconds."
	);
}

SValue BSleep::Run(const ArgList& args)
{
	const SValue time(args.CountItems() > 1 ? args[1] : B_UNDEFINED_VALUE);
	if (!time.IsDefined()) {
		TextError() << "sleep: no time supplied" << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	float secs = time.AsFloat();
	SysThreadDelay((nsecs_t)B_SECONDS(secs), B_RELATIVE_TIMEOUT);
	return (const SValue&)SSimpleStatusValue(B_OK);
}

BStrError::BStrError(const SContext& context)
	:	BinderCommand(context)
{
}

SString BStrError::Documentation() const {
	return SString(
		"usage: strerror <code>\n"
		"\n"
		"prints descriptive text for the given error code."
	);
}

SValue BStrError::Run(const ArgList& args)
{
	const SValue err(args.CountItems() > 1 ? args[1] : B_UNDEFINED_VALUE);
	if (!err.IsDefined()) {
		TextError() << "strerror: no value supplied" << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}
	return SValue::String(SStatus(err.AsInt32()).AsString());
}

BDb::BDb(const SContext& context)
	:	BinderCommand(context)
{
}

SString BDb::Documentation() const {
	return SString(
		"usage: db [message] [filename] [lineno]\n"
		"\n"
		"Breaks into the debugger, optionally with a text message,\n"
		"a file name, and a line number, as in ErrFatalErrorInContext.\n"
	);
}

SValue BDb::Run(const ArgList& args)
{
	SString a1(args.CountItems() > 1 ? args[1].AsString() : SString());
	SString a2(args.CountItems() > 2 ? args[2].AsString() : SString());
	int32_t a3(args.CountItems() > 3 ? args[3].AsInt32() : 0);
	ErrFatalErrorInContext(a2, a3, a1);
	return (const SValue&)SSimpleStatusValue(B_OK);
}

BReboot::BReboot(const SContext& context)
	:	BinderCommand(context)
{
}

SString BReboot::Documentation() const {
	return SString(
		"usage: reboot\n"
		"\n"
		"Calls SysReset(), rebooting the system."
	);
}

SValue BReboot::Run(const ArgList& /*args*/)
{
#ifdef LINUX_DEMO_HACK
	*(long*)0 = 0;
#else
	SysReset();
#endif
	return (const SValue&)SSimpleStatusValue(B_OK);
}


BEcho::BEcho(const SContext& context)
	:	BinderCommand(context)
{
}

SString BEcho::Documentation() const {
	return SString(
		"usage: echo <string> ...\n"
		"\n"
		"Emits the string representation of one or more SValues on the standard output stream."
	);
}

SValue BEcho::Run(const ArgList& args)
{
	const size_t N = args.CountItems();
	size_t i=1;

	const sptr<ITextOutput> out(TextOutput());

	while (i < N) {
		status_t err;
		SString str = args[i].AsString(&err);
		if (err == B_OK) out << str;
		else out << args[i];
		out << " ";
		i++;
	}

	out << endl;

	return (const SValue&)SSimpleStatusValue(B_OK);
}

BEnv::BEnv(const SContext& context)
	:	BinderCommand(context)
{
}

SString BEnv::Documentation() const {
	return SString(
		"usage: env\n"
		"\n"
		"Emits a mapping representing the entire environment of the current shell."
	);
}

SValue BEnv::Run(const ArgList& /*args*/)
{
	return Environment();
}

BComponents::BComponents(const SContext& context)
	:	BinderCommand(context)
{
}

SString BComponents::Documentation() const {
	return SString(
		"usage: components\n"
		"       components {-i|--interface} <fully.qualified.InterfaceName>\n"
		"\n"
		"Lists all component packages in the system.\n"
		"If -i supplied, restricts results to components implementing the specified interface (the interface must appear in /packages/interfaces)."
	);
}

SValue BComponents::Run(const ArgList& args)
{
	sptr<INode> filterDir;
	SValue filterVal;
	const size_t N = args.CountItems();
	size_t i = 1;

	const sptr<ITextOutput> out(TextOutput());

	// Collect arguments.
	while (i < N) {
		const SValue opt = args[i];

		// First check for something that definitely isn't a valid
		// option
		if (opt.Type() != B_STRING_TYPE || opt.Length() <= 1) {
			break;
		}

		// This is a string.  If the first character isn't a dash,
		// then it isn't an option.
		if (((const char*)opt.Data())[0] != '-') {
			break;
		}

		// Parse the option.
		if (opt == "-i" || opt == "--interface") {
			i++;
			filterDir = interface_cast<INode>(Context().Lookup(kPackagesInterfaces));
			if (i >= N) {
				TextError() << "components: no interface supplied" << endl;
			}
			filterVal = args[i];

		} else {
			TextError() << "components: unknown option " << opt << endl;
		}

		i++;
	}

	SKeyedVector<SString, SValue> components;

	if (filterDir != NULL && filterVal != kStar)
	{
		SString path = filterVal.AsString();
		SValue componentsValue;
		status_t err = filterDir->Walk(&path, INode::REQUEST_DATA, &componentsValue);
		
		SValue key, value;
		void* cookie = NULL;
		while (componentsValue.GetNextItem(&cookie, &key, &value) == B_OK)
		{
			components.AddItem(value.AsString(), SValue::Undefined());
		}
	}
	else
	{
		if (filterDir == NULL)
			filterDir = interface_cast<INode>(Context().Lookup(kPackagesComponents));

		if (filterDir == NULL) return (const SValue&)SSimpleStatusValue(B_ERROR);

		// Retrieve components, placing into sorted list.
		SIterator it(filterDir);
		SValue key, value;
		while (it.Next(&key, &value, INode::REQUEST_DATA) == B_OK)
		{
			components.AddItem(key.AsString(), value);
		}
		
		if (components.CountItems() == 0) {
			TextError() << "components: failure returning full component list" << endl
						<< "components: (due to IPC size limitations)" << endl;
		}
	}
	
	// Now print components to stdout.

	const size_t NC = components.CountItems();
	for (i=0; i<(int32_t)NC; i++) {
		out << components.KeyAt(i);
		out << endl;
	}
	out << endl;

	return (const SValue&)SSimpleStatusValue(B_OK);
}

BNewProcess::BNewProcess(const SContext& context)
	:	BinderCommand(context)
{
}

SString BNewProcess::Documentation() const {
	return SString(
		"usage: new_process [{-p|--publish}] <name>\n"
		"\n"
		"Creates a new empty process and returns it.\n"
		"The <name> is an arbtrary human-readable name\n"
		"to associate with the process.\n"
		"\n"
		"OPTIONS:\n"
		"    --publish Publish (relative to /processes)."
	);
}

SValue BNewProcess::Run(const ArgList& args)
{
	bool publish = false;
	const size_t N = args.CountItems();
	size_t i = 1;

	// Collect arguments.
	while (i < N) {
		const SValue opt = args[i];

		// First check for something that definitely isn't a valid
		// option
		if (opt.Type() != B_STRING_TYPE || opt.Length() <= 1) {
			break;
		}

		// This is a string.  If the first character isn't a dash,
		// then it isn't an option.
		if (((const char*)opt.Data())[0] != '-') {
			break;
		}

		// Parse the option.
		if (opt == "-p" || opt == "--publish") {
			publish = true;

		} else {
			TextError() << "new_process: unknown option " << opt << endl;
		}

		i++;
	}

	// Next argument must be the name of the process to
	// create.
	const SString processName = i < N ? args[i++].AsString() : SString();

	if (processName == "") {
		TextError() << "new_process: no process name specified" << endl;
		return SValue::Status(B_BAD_VALUE);
	}

	sptr<IProcess> proc = Context().NewProcess(processName);
	const SValue returned = SValue::Binder(proc->AsBinder());
	if (publish) {
		SString path("/processes/");
		path.PathAppend(processName, true);
		Context().Publish(path, returned);
	}
	return returned;
}

BStopProcess::BStopProcess(const SContext& context)
	:	BinderCommand(context)
{
}

SString BStopProcess::Documentation() const {
	return SString(
		"usage: stop_process [{-f|--force}] <process>\n"
		"\n"
		"Makes the given process exit.  Normally, a process\n"
		"exits when there are no more remote references on\n"
		"its objects.  This command lets you make it exit\n"
		"earlier.  After using this command, the process\n"
		"will exit as soon as all references on its process\n"
		"object <process> go away.\n"
		"\n"
		"OPTIONS:\n"
		"    --force Make the process exit immediately, even\n"
		"            while there are references on <process>.\n"
	);
}

SValue BStopProcess::Run(const ArgList& args)
{
	bool now = false;
	const size_t N = args.CountItems();
	size_t i = 1;

	// Collect arguments.
	while (i < N) {
		const SValue opt = args[i];

		// First check for something that definitely isn't a valid
		// option
		if (opt.Type() != B_STRING_TYPE || opt.Length() <= 1) {
			break;
		}

		// This is a string.  If the first character isn't a dash,
		// then it isn't an option.
		if (((const char*)opt.Data())[0] != '-') {
			break;
		}

		// Parse the option.
		if (opt == "-f" || opt == "--force") {
			now = true;

		} else {
			TextError() << "stop_process: unknown option " << opt << endl;
		}

		i++;
	}

	if (i >= N) {
		TextError() << "stop_process: no process specified" << endl;
		return SValue::Status(B_BAD_VALUE);
	}
	
	const sptr<IBinder> object(ArgToBinder(args[i]));
	if (object == NULL) {
		TextError() << "new_process: target process must be an object" << endl;
		return SValue::Status(B_BAD_VALUE);
	}

	SLooper::StopProcess(object, now);
	return SSimpleStatusValue(B_OK);
}

BContextCommand::BContextCommand(const SContext& context)
	:	BinderCommand(context)
{
}

SString BContextCommand::Documentation() const {
	return SString(
		"usage: context [{-s|--set} <object>] <context-id>\n"
		"\n"
		"Sets or gets a standard Binder context.\n"
		"\n"
		"OPTIONS:\n"
		"    --set Change the context ID (only usable from system process)."
	);
}

SValue BContextCommand::Run(const ArgList& args)
{
	sptr<IBinder> set;
	SString name;
	const size_t N = args.CountItems();
	size_t i = 1;

	// Collect arguments.
	while (i < N) {
		const SValue opt = args[i];

		// First check for something that definitely isn't a valid
		// option
		if (opt.Type() != B_STRING_TYPE || opt.Length() <= 1) {
			break;
		}

		// This is a string.  If the first character isn't a dash,
		// then it isn't an option.
		if (((const char*)opt.Data())[0] != '-') {
			break;
		}

		// Parse the option.
		if (opt == "-s" || opt == "--set") {
			// first see if we have an actual context, and if not
			// try create from a directory
			i++;
			SValue val(i < N ? args[i] : B_UNDEFINED_VALUE);
			status_t err = B_OK;
			set = val.AsBinder(&err);
			if (err != B_OK) {
				TextError() << "context: set argument " << val << " is not an object" << endl;
				return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
			}
			sptr<ICatalog> dirInterface = interface_cast<ICatalog>(set);
			if (dirInterface == NULL)
			{
				// do set the root object if it is not an ICatalog
				set = NULL;
			}
		} 
		else {
			TextError() << "context: unknown option " << opt << endl;
		}

		i++;
	}

	// Next argument must be the context name.
	const SValue nameValue(i < N ? args[i] : B_UNDEFINED_VALUE);

	if (!nameValue.IsDefined()) {
		TextError() << "context: no context name specified" << endl;
		return SValue::Binder(NULL);
	}

	status_t err;
	name = nameValue.AsString(&err);
	if (err != B_OK) {
		TextError() << "context: context name must be a string" << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	// Setting?
	if (set != NULL) {
		SLooper::SetContextObject(set, name);
	}

	return SLooper::GetContextObject(name, SLooper::Process()->AsBinder());
}

BSU::BSU(const SContext& context)
	:	BinderCommand(context)
{
}

SString BSU::Documentation() const {
	return SString(
		"usage: su [{-r||-P|--process} <process>] [<context>]\n"
		"\n"
		"Start a sub-shell in a new context.  If no context is supplied,\n"
		"the shell is started in the system context."
		"\n"
		"OPTIONS:\n"
		"    --process         Process (IProcess) in which to run the shell."
	);
}

SValue BSU::Run(const ArgList& args)
{
	sptr<IProcess> team;
	const size_t N = args.CountItems();
	size_t i = 1;

	// Collect arguments.
	while (i < N) {
		const SValue opt = args[i];

		// First check for something that definitely isn't a valid
		// option
		if (opt.Type() != B_STRING_TYPE || opt.Length() <= 1) {
			break;
		}

		// This is a string.  If the first character isn't a dash,
		// then it isn't an option.
		if (((const char*)opt.Data())[0] != '-') {
			break;
		}

		// Parse the option.
		if (opt == "-r" || opt == "-P" || opt == "--process") {
			i++;
			// XXX It would be REALLY nice to do this in the -new-
			// context (so we can for example see /processes/system).
			SValue v(i < N ? args[i] : B_UNDEFINED_VALUE);
			team = interface_cast<IProcess>(ArgToBinder(v));
			if (team == NULL) {
				TextError() << "su: process argument '" << v << "' invalid" << endl;
			}
		} 
		else {
			TextError() << "su: unknown option " << opt << endl;
		}

		i++;
	}

	// Get context to use, if specified.
	SContext context;
	const SValue contextValue = i < N ? args[i++] : B_UNDEFINED_VALUE;
	if (contextValue.IsDefined()) {
		context = SContext(interface_cast<INode>(ArgToBinder(contextValue)));
		if (context.Root() == NULL) {
			status_t err;
			const SString name = contextValue.AsString(&err);
			if (err == B_OK) {
				context = SContext::GetContext(name);
			}
		}
		if (context.Root() == NULL) {
			TextError() << "su: invalid context " << contextValue << endl;
			return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
		}
	} else {
		context = SContext::SystemContext();
	}

	if (context.Root() == NULL) {
		TextError() << "su: not allowed to access system context" << endl;
		return (const SValue&)SSimpleStatusValue(B_PERMISSION_DENIED);
	}

	sptr<ICommand> cmd = SpawnCustom(kSh, context, team);
	if (cmd == NULL) {
		TextError() << "su: unable to instantiate shell component" << endl;
		return (const SValue&)SSimpleStatusValue(B_ERROR);
	}

	ArgList cmdArgs;
	cmdArgs.AddItem(kSh);
	TextOutput() << "Starting sub-shell in "
		<< (contextValue.IsDefined() ? "new" : "system") << " context..." << endl;
	return cmd->Run(cmdArgs);
}

BNew::BNew(const SContext& context)
	:	BinderCommand(context)
{
}

SString BNew::Documentation() const {
	return SString(
		"usage: new [{-p|--publish} <catalog-path>] [{-c|--context} <context>] [{-r|-P|--process} <process>] [{-i|--inspect} <interface-name>] <component-name>\n"
		"\n"
		"Instantiates a component by name (fully-qualified, as in org.openbinder.tools.BinderShell.New).\n"
		"\n"
		"OPTIONS:\n"
		"    --publish      Also publish the binder in the system catalog at the specified path (see 'publish' command).\n"
		"    --context      Create the component in specified context (also looks at $NEW_CONTEXT).\n"
		"    --process      Remote process (IProcess) in which to instantiate the component.\n"
		"    --inspect      Before returning or publishing, do an Inspect to select the given interface."
	);
}

SValue BNew::Run(const ArgList& args)
{
	SString publish;
	SValue iface;
	sptr<IProcess> team = NULL;
	const size_t N = args.CountItems();
	size_t i = 1;

	// default to using NEW_CONTEXT
	SContext context(interface_cast<INode>(GetProperty(kNEW_CONTEXT).AsBinder()));

	// Collect arguments.
	while (i < N) {
		const SValue opt = args[i];

		// First check for something that definitely isn't a valid
		// option
		if (opt.Type() != B_STRING_TYPE || opt.Length() <= 1) {
			break;
		}

		// This is a string.  If the first character isn't a dash,
		// then it isn't an option.
		if (((const char*)opt.Data())[0] != '-') {
			break;
		}

		// Parse the option.
		if (opt == "-p" || opt == "--publish") {
			i++;
			const SValue publishVal(i < N ? args[i] : B_UNDEFINED_VALUE);
			publish = ArgToPath(publishVal);
			if (publish == "") {
				TextError() << "new: publish argument " << publishVal << " is not a string" << endl;
			}

		}
		else if (opt == "-i" || opt == "--inspect") {
			i++;
			iface = i < N ? args[i] : B_UNDEFINED_VALUE;
			if (!iface.IsDefined()) {
				TextError() << "new: interface argument " << iface << " is not defined" << endl;
			}
		}
		else if (opt == "-r" || opt == "-P" || opt == "--process" || opt == "-t") {
			i++;
			SValue val = i < N ? args[i] : B_UNDEFINED_VALUE;
			team = interface_cast<IProcess>(ArgToBinder(val));
			if (team == NULL) {
				TextError() << "new: process argument '" << val << "' invalid" << endl;
			}
		} 
		else if (opt == "-c" || opt == "--context") {
			i++;
			SValue val = i < N ? args[i] : B_UNDEFINED_VALUE;
			context = SContext(interface_cast<INode>(ArgToBinder(val)));
			if (context.Root() == NULL) {
				TextError() << "new: context argument '" << val << "' invalid" << endl;
			}
		}
		else {
			TextError() << "new: unknown option " << opt << endl;
		}

		i++;
	}

	// Next argument must be the name of the component to
	// instantiate.
	const SValue service = i < N ? args[i++] : B_UNDEFINED_VALUE;

	if (!service.IsDefined()) {
		TextError() << "new: no component specified" << endl;
		return SValue::Binder(NULL);
	}

	//bout << "new: command args were " << args << endl;
	//bout << "new: passing to instantiate \"" << service << "\" args " << args[i] << endl;

	// if not context specified (or environment variable set)
	// then just use our context
	if (context.Root() == NULL) {
		context = Context();
	}

	const SValue serviceArgs = i < N ? args[i++] : B_UNDEFINED_VALUE;
	
	sptr<IBinder> binder = ( team == NULL
		? context.New(service, serviceArgs)
		: context.RemoteNew(service, team, serviceArgs) );
	if (binder == NULL) {
		TextError() << "new: unable to instantiate component \"" << service << "\"" << endl;
		return SValue::Binder(NULL);
	}

	SValue returned;

	if (iface.IsDefined()) {
		returned = binder->Inspect(binder, iface);
	} else {
		returned = SValue::Binder(binder);
	}

	if (publish != "") {
#if ((SHOW_PUBLISHING) || (BUILD_TYPE == BUILD_TYPE_DEBUG))
		bout << "Publishing: " << publish << endl;
#endif
		status_t result = Context().Publish(publish, returned);
		if (result != B_OK) {
			TextError() << "new: error " << (void*)result << " publishing instance" << endl;
		}
	}

	//errOut << "bnew: returning " << returned << endl;
	return returned;
}

BInspect::BInspect(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BInspect::Run(const ArgList& args)
{
	const SValue object = args.CountItems() > 1 ? args[1] : B_UNDEFINED_VALUE;
	const sptr<IBinder> binder = ArgToBinder(object);
	if (binder == NULL)
	{
		TextError() << "inspect: invalid target object '" << object.AsString() << "'." << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	SValue descriptor = args.CountItems() > 2 ? args[2] : B_WILD_VALUE;

	return binder->Inspect(binder, descriptor);
}

SString BInspect::Documentation() const {
	return SString(
		"usage: inspect <object> [<interface>]\n"
		"\n"
		"Retrieve the <interface> of <object>.\n"
		"If the interface doesn't exist, undefined\n"
		"is returned.  If <interface> isn't supplied,\n"
		"all interfaces are returned.\n\n"
		"See also get, put, and invoke."
	);
}

BInvoke::BInvoke(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BInvoke::Run(const ArgList& args)
{
	//bout << "BInvoke::Run given args: " << args << endl;

	SValue object;
	SValue descriptor;
	bool all = false;

	const size_t N = args.CountItems();
	size_t idx = 1;
	
	object = idx < N ? args[idx] : B_UNDEFINED_VALUE;
	while (1) {
		if (object == kDashi) {
			idx++;
			descriptor = idx < N ? args[idx] : B_UNDEFINED_VALUE;
		} else if (object == kDasha) {
			all = true;
		} else {
			break;
		}
		idx++;
		object = idx < N ? args[idx] : B_UNDEFINED_VALUE;
	}

	sptr<IBinder> binder = ArgToBinder(object);
	if (binder == NULL)
	{
		TextError() << "invoke: invalid target object '" << object.AsString() << "'.  Not going to do an invoke." << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	if (descriptor.IsDefined()) {
		binder = binder->Inspect(binder, descriptor).AsBinder();
		if (binder == NULL) {
			TextError() << "invoke: interface " << descriptor.AsString() << " not found on object " << object.AsString() << "." << endl;
			return (const SValue&)SSimpleStatusValue(B_BINDER_BAD_INTERFACE);
		}
	}

	idx++;
	SValue method = idx < N ? args[idx] : B_UNDEFINED_VALUE;

	SValue nuargs;

	++idx;
	for (int32_t i = idx ; i < N ; i++)
	{
		nuargs.JoinItem(SSimpleValue<int32_t>(i - idx), args[i]);
	}

	// bout << "BInvoke: " << binder << " " << method << " " << nuargs << endl;

	// allow calling with zero args
	SLooper::LastError();
	SValue result = nuargs.IsDefined() ? binder->Invoke(method, nuargs) : binder->Invoke(method);
	status_t err = SLooper::LastError();
	if (err == B_OK)
		return all ? result : result["res"];
	return (const SValue&)SSimpleStatusValue(err);
}

SString BInvoke::Documentation() const {
	return SString(
		"usage: invoke [-a] [-i <interface] <object> <method> [<arg> ...]\n"
		"\n"
		"Invoke <method> on <object>.  Any desired\n"
		"arguments can be supplied.\n\n"
		"-a requests that all results (including any output\n"
		"parameters) be returned as a set of mappings.\n\n"
		"-i performs the invoke on a specific interface.\n\n"
		"See also inspect, get, and put."
	);
}

BPut::BPut(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BPut::Run(const ArgList& args)
{
	//bout << "BPut::Run given args: " << args << endl;

	SValue object;
	SValue descriptor;

	const size_t N = args.CountItems();
	size_t idx = 1;
	
	object = idx < N ? args[idx] : B_UNDEFINED_VALUE;
	while (1) {
		if (object == kDashi) {
			idx++;
			descriptor = idx < N ? args[idx] : B_UNDEFINED_VALUE;
		} else {
			break;
		}
		idx++;
		object = idx < N ? args[idx] : B_UNDEFINED_VALUE;
	}

	sptr<IBinder> binder = ArgToBinder(object);
	if (binder == NULL)
	{
		TextError() << "put: invalid target object '" << object.AsString() << "'.  Not going to do a put." << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	if (descriptor.IsDefined()) {
		binder = binder->Inspect(binder, descriptor).AsBinder();
		if (binder == NULL) {
			TextError() << "put: interface " << descriptor.AsString() << " not found on object " << object.AsString() << "." << endl;
			return (const SValue&)SSimpleStatusValue(B_BINDER_BAD_INTERFACE);
		}
	}

	idx++;
	SValue key = idx < N ? args[idx] : B_UNDEFINED_VALUE;

	idx++;
	const SValue value = idx < N ? args[idx] : B_UNDEFINED_VALUE;
	if (!value.IsDefined()) {
		TextError() << "put: no value supplied" << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	status_t result = binder->Put(SValue(key, value));

	return (const SValue&)SSimpleStatusValue(result);
}

SString BPut::Documentation() const {
	return SString(
		"usage: put [-i <interface] <object> <property> <value>\n"
		"\n"
		"Set <property> of <object> to <value>.\n\n"
		"-i performs the put on a specific interface.\n\n"
		"See also inspect, get, and invoke."
	);
}

BGet::BGet(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BGet::Run(const ArgList& args)
{
	//bout << "BGet::Run given args: " << args << endl;

	SValue object;
	SValue descriptor;

	const size_t N = args.CountItems();
	size_t idx = 1;
	
	object = idx < N ? args[idx] : B_UNDEFINED_VALUE;
	while (1) {
		if (object == kDashi) {
			idx++;
			descriptor = idx < N ? args[idx] : B_UNDEFINED_VALUE;
		} else {
			break;
		}
		idx++;
		object = idx < N ? args[idx] : B_UNDEFINED_VALUE;
	}

	sptr<IBinder> binder = ArgToBinder(object);
	if (binder == NULL)
	{
		TextError() << "get: invalid target object '" << object.AsString() << "'.  Not going to do a get." << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	if (descriptor.IsDefined()) {
		binder = binder->Inspect(binder, descriptor).AsBinder();
		if (binder == NULL) {
			TextError() << "get: interface " << descriptor.AsString() << " not found on object " << object.AsString() << "." << endl;
			return (const SValue&)SSimpleStatusValue(B_BINDER_BAD_INTERFACE);
		}
	}

	idx++;
	SValue key = idx < N ? args[idx] : B_UNDEFINED_VALUE;
	if (!key.IsDefined()) key = SValue::Wild();

	SValue out;
	status_t result = binder->Effect(B_UNDEFINED_VALUE,B_UNDEFINED_VALUE,key,&out);

	if (result != B_OK) {
		return (const SValue&)SSimpleStatusValue(result);
	}

	return out;
}

SString BGet::Documentation() const {
	return SString(
		"usage: get [-i <interface] <object> [<property>]\n"
		"\n"
		"Retrieve the value of <property> from <object>.\n"
		"If <property> is not supplied, all properties\n"
		"are returned.\n\n"
		"-i performs the get on a specific interface.\n\n"
		"See also inspect, put, and invoke."
	);
}

BLs::BLs(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BLs::Run(const ArgList& args)
{
	const sptr<ITextOutput> out(TextOutput());

	const size_t N=args.CountItems();
	size_t optind = 1;
	bool longFormat = false;

	SValue opt = optind < N ? args[optind] : B_UNDEFINED_VALUE;
	if (opt == kDashl) {
		longFormat = true;
		optind++;
	}

	sptr<IBinder> rootObj = optind < N ? args[optind].AsBinder() : NULL;
	if (rootObj != NULL) {
		optind++;
	} else {
		rootObj = Context().Root()->AsBinder();
	}
	
	sptr<IIterable> dir;
	SValue pathVal = optind < N ? args[optind] : B_UNDEFINED_VALUE;
	if (!pathVal.IsDefined() || pathVal.Type() == B_STRING_TYPE) {
		SString catPath(ArgToPath(pathVal.IsDefined() ? pathVal : SValue::String("")));
		SValue catValue;
		if (catPath == "" || catPath == kSlash) {
			catValue = SValue::Binder(rootObj);
		} else {
			sptr<INode> rootDir(interface_cast<INode>(rootObj));
			if (rootDir == NULL) {
				TextError() << "ls: not an INode: "
					<< rootObj << endl;
				return SValue::Status(B_BAD_VALUE);
			}
			catValue = SNode(rootDir).Walk(catPath);
		}
		
		if (catValue.ErrorCheck() != B_OK) {
			TextError() << "ls: error " << (void*)catValue.ErrorCheck() << " retrieving path" << endl;
			return (const SValue&)SSimpleStatusValue(catValue.ErrorCheck());
		}
		if (!catValue.IsDefined()) {
			TextError() << "ls: path not found: " << pathVal.AsString() << endl;
			return (const SValue&)SSimpleStatusValue(B_ENTRY_NOT_FOUND);
		}
		
		dir = interface_cast<IIterable>(catValue);
		
		if (dir == NULL) {
			TextError() << "ls: not iterable: " << catValue << endl;
			return (const SValue&)SSimpleStatusValue(B_BINDER_BAD_INTERFACE);
		}
	} else {
		TextError() << "ls: invalid path: " << pathVal << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	// Retrieve items in this directory.
	SIterator it(dir.ptr());
	if (it.ErrorCheck() != B_OK) return (const SValue&)SSimpleStatusValue(it.ErrorCheck());
	
	//bout << "i got here!" << endl;
	// Sort catalog items.
	SValue key, value;
	status_t err;
	while ((err=it.Next(&key, &value, INode::REQUEST_DATA)) == B_OK)
	{
		//bout << "*** key = "  << key << " value = " << value << endl;
		if (value == SValue::Undefined())
		{
			out << key.AsString() << endl;
		}
		else
		{
			// Fancy color-ls for different types of catalog objects.
			type_code type = value.Type();
			switch(type)
			{
				case B_BINDER_TYPE:
				case B_BINDER_HANDLE_TYPE:
				{
					sptr<INode> ami(interface_cast<INode>(value));

					if (ami != NULL) {
						if (GetColorFlag()) out << ANSI_BLUE;
						out << key.AsString();
						if (GetColorFlag()) out << ANSI_NORMAL;
						out << "/";
					} else
						out << key.AsString();
				} break;

				case B_BINDER_WEAK_TYPE:
				case B_BINDER_WEAK_HANDLE_TYPE:
				{
					if (GetColorFlag()) out << ANSI_PURPLE;
					out << key.AsString();
					if (GetColorFlag()) out << ANSI_NORMAL;
				} break;

				case B_VALUE_TYPE:
				{
					// Yellow: complex SValues
					if (GetColorFlag()) out << ANSI_YELLOW;
					out << key.AsString();
					if (GetColorFlag()) out << ANSI_NORMAL;
				} break;

				default:
				{
					// Green: scalar SValue types
					if (GetColorFlag()) out << ANSI_GREEN;
					out << key.AsString();
					if (GetColorFlag()) out << ANSI_NORMAL;
				} break;
			}
			if (longFormat) {
				out << " -> ";
				value.PrintToStream(out);
				out << endl;
			} else {
				out << endl;
			}
		}
	}

	return (const SValue&)SSimpleStatusValue(err == B_END_OF_DATA ? B_OK : err);
}

SString BLs::Documentation() const {
	SString help(
		"usage: ls [-l] [<rootObject>] [<path>]\n"
		"\n"
		"Lists the items at the directory specified\n"
		"by " COLOR(CYAN, "<path>") ", as traversed starting from\n"
		COLOR(CYAN, "<rootObject>") ".\n"
		"\n"
		"If not supplied, " COLOR(CYAN, "<rootObject>") " defaults to the\n"
		"shell context root directory.\n"
		"\n"
		"If not supplied, " COLOR(CYAN, "<path>") " defaults to '/' \n"
		"(no traversal, just list the root).\n"
		"\n"
		"If " COLOR(CYAN, "-l") " is supplied, the value of each item\n"
		"is also displayed.\n"
		);
	if (GetColorFlag()) {
		help +=
			"\n"
			"Colors:\n"
			ANSI_BLUE   "    Blue:" ANSI_NORMAL " directory\n"
			            "  Normal: strong pointer to an object\n"
			ANSI_PURPLE "  Purple:" ANSI_NORMAL " weak pointer to an object\n"
			ANSI_GREEN  "   Green:" ANSI_NORMAL " simple value\n"
			ANSI_YELLOW "  Yellow:" ANSI_NORMAL " complex value\n\n";
	}

	help += "See also " ANSI_GREEN "lookup" ANSI_NORMAL " to retrieve data from directories.";
	return help;
}

BPublish::BPublish(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BPublish::Run(const ArgList& args)
{

	const SString path = args.CountItems() > 1 ? ArgToPath(args[1]) : SString();
	if (path == "") {
		TextError() << "publish: no path supplied" << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	const SValue value = args.CountItems() > 2 ? args[2] : B_UNDEFINED_VALUE;
	if (!value.IsDefined()) {
		TextError() << "publish: no value supplied" << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

#if ((SHOW_PUBLISHING) || (BUILD_TYPE == BUILD_TYPE_DEBUG))
	bout << "Publishing: " << path << endl;
#endif
	status_t result = Context().Publish(path, value);

	return (const SValue&)SSimpleStatusValue(result);
}

SString BPublish::Documentation() const {
	return SString(
		"usage: publish <path> <value>\n"
		"\n"
		"Publishes the item <value> at the given directory\n"
		"<path>.\n\n"
		"See also unpublish and lookup."
	);
}

BUnpublish::BUnpublish(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BUnpublish::Run(const ArgList& args)
{
	const SString path = args.CountItems() > 1 ? ArgToPath(args[1]) : SString();
	if (path == "") {
		TextError() << "unpublish: no path supplied" << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

#if ((SHOW_PUBLISHING) || (BUILD_TYPE == BUILD_TYPE_DEBUG))
	bout << "Unpublishing: " << path << endl;
#endif
	status_t result = Context().Unpublish(path);

	return (const SValue&)SSimpleStatusValue(result);
}

SString BUnpublish::Documentation() const {
	return SString(
		"usage: unpublish <path>\n"
		"\n"
		"Removes the item at the given directory <path>.\n\n"
		"See also publish and lookup."
	);
}

BLookup::BLookup(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BLookup::Run(const ArgList& args)
{
	SValue interface;
	const size_t N=args.CountItems();
	size_t i=1;
	while (true) {
		SValue arg = i < N ? args[i] : B_UNDEFINED_VALUE;
		if (arg == kDashi || arg == kDashInspect) {
			i++;
			interface = i < N ? args[i] : B_UNDEFINED_VALUE;
			i++;
		}
		else {
			break;
		}
	}

	sptr<INode> rootDir;
	sptr<IBinder> rootObj = i < N ? args[i].AsBinder() : NULL;
	if (rootObj != NULL) {
		i++;
		rootDir = INode::AsInterface(rootObj);
		if (rootDir == NULL) {
			TextError() << "lookup: not an INode: "
				<< rootObj << endl;
			return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
		}
	} else {
		rootDir = Context().Root();
	}
	
	const SString path = i < N ? ArgToPath(args[i]) : SString();
	if (path == "") {
		TextError() << "lookup: no path supplied" << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	SNode dir(rootDir);
	SValue v(dir.Walk(path));

	if (interface.IsDefined()) {
		sptr<IBinder> binder = v.AsBinder();
		if (binder == NULL) {
			return SValue::Undefined();
		}
		v = binder->Inspect(binder, interface);
	}
	return v;
}

SString BLookup::Documentation() const {
	return SString(
		"usage: lookup [-i <interface>]\n"
		"              [<rootDirectoryObject>]\n"
		"              <path>\n"
		"\n"
		"Returns the item at the given directory " COLOR(CYAN, "<path>") ".\n"
		"If " COLOR(CYAN, "<rootDirectoryObject>") " is not specified, the\n"
		"shell context root directory will be walked.\n"
		"\n"
		"If " COLOR(CYAN, "-i") " used, result will be inspected as " COLOR(CYAN, "<interface>") ".\n"
		"\n"
		"See also " COLOR(GREEN, "publish") " and " COLOR(GREEN, "unpublish") "."
	);
}

BMkdir::BMkdir(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BMkdir::Run(const ArgList& args)
{
	const size_t N=args.CountItems();
	size_t i = 1;
	sptr<INode> rootDir;
	sptr<IBinder> rootObj = i < N ? args[i].AsBinder() : NULL;
	if (rootObj != NULL) {
		i++;
		rootDir = INode::AsInterface(rootObj);
		if (rootDir == NULL) {
			TextError() << "mkdir: not an INode: "
				<< rootObj << endl;
			return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
		}
	} else {
		rootDir = Context().Root();
	}
	
	SString fullpath = i < N ? ArgToPath(args[i]) : SString();

	while(fullpath.PathLeaf()[0] == '\0') {
		SString shorter;
		fullpath.PathGetParent(&shorter);
		fullpath = shorter;
		if (fullpath == "") break;
	}
	
	if (fullpath == "") {
		TextError() << "mkdir: no path supplied" << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}
	
	SString leaf = SString(fullpath.PathLeaf());
	SString path;
	if (fullpath.PathGetParent(&path) == B_ENTRY_NOT_FOUND) {
		// TODO: when we get $CWD, use $CWD here
		// ... although, how does it interact with rootDir?
		path = "/";
	}

	SNode dir(rootDir);
	SValue v(dir.Walk(path));

	sptr<ICatalog> parent = ICatalog::AsInterface(v.AsBinder());
	if (parent == NULL) {
		TextError() << "mkdir: not a catalog: " << path << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	status_t result;

	sptr<INode> newDir = parent->CreateNode(&leaf, &result);

	if (result != B_OK) {
		TextError() << "mkdir: error " << (void*)result << " creating directory" << endl;
		return SValue::Undefined();
	}

	return SValue::Binder(newDir->AsBinder());
}

SString BMkdir::Documentation() const {
	return SString(
		"usage: mkdir [<rootDirectoryObject>] \n"
		"       [<path>/]<newdir>\n"
		"\n"
		"Creates " COLOR(CYAN,"<newdir>") " in the directory specified\n"
		"by walking " COLOR(CYAN,"<path>") ", which may be empty.\n"
		"\n"
		"Returns a reference to the directory, or undefined\n"
		"if there was an error (which is printed).\n"
		"\n"
		"If " COLOR(CYAN, "<rootDirectoryObject>") " is not specified,\n"
		"then " COLOR(CYAN, "<path>") " will be followed starting from\n"
		"the root directory of the shell's context."
	);
}

class LinkPrinter : public BBinder
{
public:
						LinkPrinter(const sptr<ITextOutput>& out);
	virtual	status_t	HandleEffect(	const SValue &in,
										const SValue &inBindings,
										const SValue &outBindings,
										SValue *out);
private:
	sptr<ITextOutput> m_out;
};

LinkPrinter::LinkPrinter(const sptr<ITextOutput>& out)
	:BBinder(),
	 m_out(out)
{
}

status_t
LinkPrinter::HandleEffect(	const SValue& in,
							const SValue& /*inBindings*/,
							const SValue& /*outBindings*/,
							SValue* /*out*/)
{
	m_out << "link pushed: " << in << endl;
	return B_OK;
}

BLink::BLink(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BLink::Run(const ArgList& args)
{
	sptr<IBinder> link, target;
	SValue bindings;
	uint32_t flags = 0;

	const size_t N=args.CountItems();
	SString arg0 = N > 0 ? args[0].AsString() : SString();

	size_t i=1;
	while (true) {
		SValue a = i < N ? args[i] : B_UNDEFINED_VALUE;
		//bout << "arg [" << i << "] " << a << endl;
		if (!a.IsDefined()) break;
		SString s = a.AsString();

		if (s == "--weak") {
			flags |= B_WEAK_BINDER_LINK;
		}
		else if (s == "--sync") {
			flags |= B_SYNC_BINDER_LINK;
		}
		else if (s == "--notranslate") {
			flags |= B_NO_TRANSLATE_LINK;
		}
		else if (s == "--printer") {
			target = new LinkPrinter(bout);
		}
		else {
			// first
			if (link == NULL) {
				sptr<IBinder> b = ArgToBinder(a);
				if (b == NULL) {
					TextError() << arg0 << " (link) given bad parameter: " << a << endl;
					return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
				} else {
					link = b;
				}
			}

			// second
			else if (target == NULL) {
				sptr<IBinder> b = ArgToBinder(a);
				if (b == NULL) {
					TextError() << arg0 << " (link) given bad parameter: " << a << endl;
					return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
				} else {
					target = b;
				}
			}

			// third
			else if (!bindings.IsDefined()) {
				bindings = a;
			}

			// huh?
			else {
				TextError() << "link given bad parameter: " << a << endl;
				return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
			}
		}

		i++;
	}

	/*
	bout << "link:     " << link << endl;
	bout << "target:   " << target << endl;
	bout << "flags:    " << flags << endl;
	bout << "bindings: " << bindings << endl;
	*/

	if (link == NULL || target == NULL || !bindings.IsDefined()) {
		TextError() << "link: not enough parameters provided... missing" << endl;
		if (link == NULL) TextError() << " link";
		if (target == NULL) TextError() << " target";
		if (!bindings.IsDefined()) TextError() << " bindings";
		TextError() << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}

	status_t result = B_BAD_VALUE;

	if (arg0 == "link") {
		result = link->Link(target, bindings, flags);
	}
	else if (arg0 == "unlink") {
		result = link->Unlink(target, bindings, flags);
	}
	else {
		TextError() << arg0 << ": unknown argv[0]: " << args[(int32_t)0] << endl;
		TextError() << Documentation() << endl;
	}

	return (const SValue&)SSimpleStatusValue(result);
}

SString BLink::Documentation() const {
	return SString(
		"usage: link|unlink [--weak] [--sync] [--notranslate] [--printer] <source> <target> <bindings>\n"
		"\n"
		"Add or remove a data link from one object to\n"
		"another.  The <bindings> argument maps from a\n"
		"property in <source> to a property in <target>.\n"
		"Any time the source property changes, the new\n"
		"value is pushed to the target property.\n\n"
		"--weak has the source hold a weak pointer target.\n"
		"--sync is currently unnused.\n"
		"--notranslate does not translate <bindings> when\n"
		"the link is followed, sending them directly.\n"
		"--printer uses a built-in target that prints all\n"
		"data pushed to it."
	);
}

// short to fit into SValue inline storage
B_STATIC_STRING_VALUE_4 (key_environment,	"env",)
B_STATIC_STRING_VALUE_4 (key_script_args,	"scr",)

B_STATIC_STRING_VALUE_8 (key_creator,	"creator",)
B_STATIC_STRING_VALUE_8 (key_type,		"type",)
B_STATIC_STRING_VALUE_8 (key_restype,	"restype",)
B_STATIC_STRING_VALUE_8 (key_resid,		"resid",)


BSpawn::BSpawn(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BSpawn::Run(const ArgList& args)
{
	TextError() << "LINUX_DEMO_HACK spawn command must be reimplemented using files instead of databases" << endl;
	return (const SValue&)SSimpleStatusValue(B_BAD_DESIGN_ENCOUNTERED);
#if !LINUX_DEMO_HACK
	size_t optind = 1;
	SValue script_args;
	SValue env_args;
	sptr<IProcess> team;
	sptr<BDatabaseStore> script;

	while (1) {
		const SValue opt = args[SSimpleValue<int32_t>(optind)];
		if (opt == kDashd) {
			++optind;

			int32_t type = args[SSimpleValue<int32_t>(optind++)].AsInt32();
			int32_t creator = args[SSimpleValue<int32_t>(optind++)].AsInt32();
			int32_t restype = args[SSimpleValue<int32_t>(optind++)].AsInt32();
			DmResourceID resid = (DmResourceID)args[SSimpleValue<int32_t>(optind++)].AsInt32();

			script_args.JoinItem(B_0_INT32, SValue::String("spawn_faerie"));
			script_args.JoinItem(B_1_INT32, SValue::String("-s"));
			SValue database_args;
			database_args.JoinItem(key_type, SSimpleValue<int32_t>(type));
			database_args.JoinItem(key_creator, SSimpleValue<int32_t>(creator));
			database_args.JoinItem(key_restype, SSimpleValue<int32_t>(restype));
			database_args.JoinItem(key_resid, SSimpleValue<int32_t>(resid));
			script_args.JoinItem(B_2_INT32, database_args);

			script = new BDatabaseStore(type, creator, restype, resid);
		}
		else if (opt == kDasht) {
			++optind;
			SValue tm = args[SSimpleValue<int32_t>(optind++)];
			team = IProcess::AsInterface(tm);
			if (team == NULL) {
				SString path = tm.AsString();
				if (path.Length() > 0) {
					team = IProcess::AsInterface(Context().Lookup(path));
				}
				if (team == NULL) {
					TextError() << "spawn invalid team passed with the -t option" << endl;
					return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
				}
			}
		}
		else {
			break;
		}
	}

	env_args = args[SSimpleValue<int32_t>(optind++)];

	// check that they passed a valid database
	if (script == NULL || script->ErrorCheck() != B_OK) {
		TextError() << "spawn couldn't initialize database" << endl;
		return (const SValue&)SSimpleStatusValue(B_BAD_VALUE);
	}
	script = NULL; // close it

	if (team == NULL) {
		// get the local team
		team = SLooper::Process().ptr();
	}

	SValue faerie_component("org.openbinder.tools.BinderShell.Spawn.Faerie");
	SValue faerie_args(key_environment, env_args);
		faerie_args.JoinItem(key_script_args, script_args);

	sptr<IBinder> faerie = Context().RemoteNew(faerie_component, team, faerie_args);

	return (const SValue&)SSimpleStatusValue(B_OK);
#endif // LINUX_DEMO_HACK
}


BSpawnFaerie::BSpawnFaerie(const SContext& context, const SValue &args)
	:	BBinder(context),
		m_args(args)
{
}

void BSpawnFaerie::InitAtom()
{
	status_t err;

	IncStrong(NULL);

	SysHandle thread;
	err = SysThreadCreateEZ("shel", BSpawnFaerie::thread_func, this, &thread);
	if (err != errNone) {
		return ;
	}

	err = SysThreadStart(thread);
	if (err != errNone) {
		return ;
	}
}

void BSpawnFaerie::thread_func(void *ptr)
{
	BSpawnFaerie *This = static_cast<BSpawnFaerie*>(ptr);

#if !LINUX_DEMO_HACK // rewrite the spawn command to use files instead of databases

	sptr<BDatabaseStore> script;

	// bout << "BSpawnFaerie created in new thread in team " << SLooper::ProcessID()
	// 					<< " with args " << This->m_args << endl;

	sptr<BShell> shell = new BShell(This->Context());

	// the shell's environment may contain strong references - we
	// must be careful not to leave extra sptrs (stored in the
	// below SValues) lying around after they are transferred
	// to the shell
	{
		SValue env_args = This->m_args[key_environment];
		void *cookie = NULL;
		SValue k, v;
		while (B_OK == env_args.GetNextItem(&cookie, &k, &v)) {
			shell->SetProperty(k, v);
		}

		This->m_args.RemoveItem(key_environment);
	}

	SValue script_args = This->m_args[key_script_args];

	shell->SetByteInput(NULL);
	shell->SetByteOutput(StandardByteOutput());
	shell->SetByteError(StandardByteOutput());

	shell->Run(script_args);

	This->DecStrong(NULL);
#endif // LINUX_DEMO_HACK
}

BTrue::BTrue(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BTrue::Run(const ArgList& /*args*/)
{
	return (const SValue&)SSimpleValue<int32_t>(0);
}

BFalse::BFalse(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BFalse::Run(const ArgList& /*args*/)
{
	return (const SValue&)SSimpleValue<int32_t>(1);
}

BTime::BTime(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BTime::Run(const ArgList& /*args*/)
{
	time_t theTime = time(NULL);
	struct tm *timeRec;
	timeRec = localtime(&theTime);
	TextOutput() << ctime(&theTime);
	return (const SValue&)SSimpleStatusValue(B_OK);
}

BPush::BPush(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BPush::Run(const ArgList& args)
{
	sptr<IBinder> b = GetProperty(kSELF).AsBinder();
	sptr<BBinder> self = b->LocalBinder();
	if (self != NULL) {
		self->Push(args.CountItems() > 1 ? args[1] : B_UNDEFINED_VALUE);
	} else {
		TextError() << "push can't get a local SELF binder :-(" << endl;
	}
	return (const SValue&)SSimpleStatusValue(B_OK);
}

SString BPush::Documentation() const
{
	return SString(
		"usage:\n"
		"    push VALUE\n"
		"\n"
		"    VALUE is pushed onto the shell's SELF binder\n");
}

BClear::BClear(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BClear::Run(const ArgList& /*args*/)
{
	TextOutput() << "\f";

	return (const SValue&)SSimpleStatusValue(B_OK);
}

SString BClear::Documentation() const {
	return SString(
		"usage: clear\n"
		"\n"
		"clears the screen\n");
}

BCat::BCat(const SContext& context)
	:	BinderCommand(context)
{
}

SValue BCat::Run(const ArgList& args)
{
	SString path(args.CountItems() > 1 ? ArgToPath(args[1]) : SString());

	SNode dir(Context().Root());
	status_t err;
	SValue file = dir.Walk(path, &err, 0);
	if (err != B_OK) return SValue::Status(err);

	sptr<IDatum> datum = IDatum::AsInterface(file);
	if (datum == NULL)
	{
		if (path != "") {
			TextOutput() << "cat: not a datum: " << file << endl;
			return SValue::Status(B_BAD_VALUE);
		}
	}
	else
	{
		file = datum->Open(IDatum::READ_ONLY);
	}
	
	sptr<IByteInput> byteInput = IByteInput::AsInterface(file);
	if (byteInput == NULL)
	{
		if (path != "") {
			TextOutput() << "cat: unable to open datum: " << file << endl;
			return SValue::Status(B_BAD_VALUE);
		}
		byteInput = ByteInput();
	}
	
	SVector<uint8_t> buffer;
	buffer.SetSize(16*1024);
	
	ssize_t amount;
	do
	{
		amount = byteInput->Read(buffer.EditArray(), buffer.CountItems());
		if (amount > 0) ByteOutput()->Write(buffer.Array(), amount);
	} while (amount > 0);
	
	return (const SValue&)SSimpleStatusValue(amount >= 0 ? B_OK : amount);
}


SString BCat::Documentation() const
{
	return SString(
			"usage:\n"
			"    cat PATH\n");
}


#if 0
BCheckNamespace::BCheckNamespace(const SContext& context)
	:	BinderCommand(context)
{
}

class Record
{
public:
	Record(const sptr<ICatalog>& dir);
	
	void AddCreateDatum(const SString& name);
	void AddCreateEntry(const SString& name);
	void AddRemoveEntry(const SString& name);
	
	void ConfirmCreateEvent(const SString& name);
	void ConfirmRemoveEvent(const SString& name);

private:
	enum
	{
		CREATE_DATUM_EVENT		= 0x0001,
		CREATE_ENTRY_EVENT		= 0x0002,
		REMOVE_ENTRY_EVENT		= 0x0004,

		CONFIRM_CREATE_EVENT	= 0x1000,
		CONFIRM_REMOVE_EVENT	= 0x2000,
	};

	SLocker m_lock;
	sptr<ICatalog> m_dir;
	SKeyedVector<SString, uint32_t> m_events;
};

Record::Record(const sptr<ICatalog>& dir)
	:	m_dir(dir)
{
}

void Record::AddCreateDatum(const SString& name)
{
	SLocker::Autolock lock(m_lock);
	m_events.AddItem(name, CREATE_DATUM_EVENT);
}

void Record::AddCreateEntry(const SString& name)
{
	SLocker::Autolock lock(m_lock);
	m_events.AddItem(name, CREATE_ENTRY_EVENT);
}

void Record::AddRemoveEntry(const SString& name)
{
	SLocker::Autolock lock(m_lock);
	m_events.AddItem(name, REMOVE_ENTRY_EVENT);
}

void Record::ConfirmCreateEvent(const SString& name)
{
	SLocker::Autolock lock(m_lock);
	bool found = false;
	uint32_t value = m_events.ValueFor(name, &found);

	if (found)
	{
		value |= CONFIRM_CREATE_EVENT;
		m_events.AddItem(value);
	}
	else
	{
		// report an error!
	}
}

void Record::ConfirmRemoveEvent(const SString& name)
{
	SLocker::Autolock lock(m_lock);
	bool found = false;
	uint32_t value = m_events.ValueFor(name, &found);

	if (found)
	{
		value |= CONFIRM_REMOVE_EVENT;
		m_events.AddItem(value);
	}
	else
	{
		// report an error!
	}
}

status_t BCheckNamespace::check_create_datum(const SString& path, const sptr<ICatalog>& dir)
{
	SString name = SValue::Int64(system_real_time()).AsString();
	m_createRecords.AddItem(dir, name); 
	
	// first check to see if the directory supports creating datums
	status_t err;
	sptr<IDatum> datum = dir->CreateDatum(name, 0, &err);

	// if the directory doesn't support creating of datum's then remove
	// this record from the create event.
	if (err == B_UNSUPPORTED)
		m_createRecords.RemoveValueFor(dir);
	
}

status_t BCheckNamespace::walk_namespace_data(const SString& name, const sptr<ICatalog>& dir)
{
	return B_ERROR;
}

status_t BCheckNamespace::walk_namespace_datum(const SString& name, const sptr<ICatalog>& dir)
{
	const sptr<ITextOutput> out(TextOutput());

	//XXX: check to see if the datum returned by Iterate is the same as the one returned by Walk().

	SIterator it(dir.ptr());
	if (it.ErrorCheck() != B_OK)
	{
		bout << "could not create an iterator for directory: " << name << endl;
		bout << "could not create an iterator for directory: " << name << endl;
		out << "could not create an iterator for directory: " << name << endl;
		return it.ErrorCheck();
	}

	status_t err = B_OK;
	SValue key, value;
	while (it.Next(&key, &value) == B_OK)
	{
		sptr<IBinder> binder = value.AsBinder();

		if (key.AsString() == "")
		{
			bout << "key == NULL: '" << name << "'" << endl;
			bout << "key == NULL: '" << name << "'" << endl;
			out << "key == NULL: '" << name << "'" << endl;
			continue;
		}
		
		if (binder == NULL)
		{
			bout << "datum == NULL: '" << name << "'" << indent << endl << "for key '" << key.AsString() << "'" << dedent << endl;
			bout << "datum == NULL: '" << name << "'" << indent << endl << "for key '" << key.AsString() << "'" << dedent << endl;
			out << "datum == NULL: '" << name << "'" << indent << endl << "for key '" << key.AsString() << "'" << dedent << endl;
			continue;
		}

		
		sptr<ICatalog> child = ICatalog::AsInterface(binder);

		if (child != NULL)
		{
			SString path(name);
			path.PathAppend(key.AsString());
			err = walk_namespace_datum(path, child);
		}
		else
		{
			// check to make sure that the datum returned by itereate is the same
			// one that is returned by walk. this will ensure that a directory is
			// returning the same datum for the requested path/key.

			SValue datum;
			SString leaf(key.AsString());
			err = dir->Walk(&leaf, 0, &datum);
			if (err != B_OK || (datum.AsBinder().ptr() != binder.ptr()))
			{
				bout << "datum != binder: '" << name << "'" << indent << endl << "for key '" << key.AsString() << "'" << dedent << endl;
				bout << "datum != binder: '" << name << "'" << indent << endl << "for key '" << key.AsString() << "'" << dedent << endl;
				bout << indent;
				bout << "datum = " << datum << endl;
				bout << "binder = " << SValue::Binder(binder) << endl;
				bout << dedent;
				out << "datum != binder: '" << name << "'" << indent << endl << "for key '" << key.AsString() << "'" << dedent << endl;
			}
		}
	}

	return err;
}

SValue BCheckNamespace::Run(const ArgList& /*args*/)
{
	sptr<ICatalog> root = Context().Root();
	status_t err = walk_namespace_datum(kSlash, root);
	return (const SValue&)SSimpleStatusValue(err);
}

SString BCheckNamespace::Documentation() const
{
	return SString("no docs");
}
#endif

