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

#include <support/Debug.h>
#include <support/ByteStream.h>
#include <support/Pipe.h>
#include <support/String.h>
#include <support/KernelStreams.h>
#include <support/StdIO.h>
#include <support/Looper.h>

#if SUPPORTS_FEATURES
#include <services/IFeatures.h>
#endif

#if TARGET_HOST == TARGET_HOST_PALMOS
#include <storage/BDatabaseStore.h>
#endif

#include <app/BCommand.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::storage;
using namespace palmos::app;
#endif

#if TARGET_HOST == TARGET_HOST_PALMOS
#include <DataMgr.h>
#include <SystemResources.h> // for sysFileTLibrary
#include <UIResources.h>
#endif

#include "BinderCommands.h"
#include "ANSIEscapes.h"
#include "bsh.h"
#include "CV.h"
#include "vm.h"

#include "Test.h"

B_STATIC_STRING_VALUE_SMALL(kPS1, "PS1", );
B_STATIC_STRING_VALUE_SMALL(kPS2, "PS2", );
B_STATIC_STRING_VALUE_SMALL(kPWD, "PWD", );
B_STATIC_STRING_VALUE_LARGE(kBSH_HOST_PWD, "BSH_HOST_PWD", );
B_STATIC_STRING_VALUE_LARGE(kBSH_SCRIPT_FILE, "BSH_SCRIPT_FILE", );
B_STATIC_STRING_VALUE_LARGE(kBSH_SCRIPT_DIR, "BSH_SCRIPT_DIR", );
B_STATIC_STRING_VALUE_LARGE(kECHO, "ECHO", );
B_STATIC_STRING_VALUE_LARGE(kUSER, "USER", );
B_STATIC_STRING_VALUE_LARGE(kBARON, "baron", );
B_STATIC_STRING_VALUE_LARGE(kCONTEXT, "CONTEXT", );
B_STATIC_STRING_VALUE_LARGE(kINTERFACE, "INTERFACE", );
B_STATIC_STRING_VALUE_LARGE(kWILD, "WILD", );
B_STATIC_STRING_VALUE_LARGE(kSELF, "SELF", );
B_STATIC_STRING_VALUE_LARGE(kPALMSOURCE_PLATFORM, "PALMSOURCE_PLATFORM", );
B_STATIC_STRING_VALUE_LARGE(kPLATFORM_PALMSIM_WIN32, "PALMSIM_WIN32", );
B_STATIC_STRING_VALUE_LARGE(kPLATFORM_DEVICE_ARM, "DEVICE_ARM", );
B_STATIC_STRING_VALUE_LARGE(kPALMSOURCE_BUILDTYPE, "PALMSOURCE_BUILDTYPE", );
B_STATIC_STRING_VALUE_LARGE(kBUILD_TYPE_DEBUG, "DEBUG", );
B_STATIC_STRING_VALUE_LARGE(kBUILD_TYPE_TEST, "TEST", );
B_STATIC_STRING_VALUE_LARGE(kBUILD_TYPE_RELEASE, "RELEASE", );
B_STATIC_STRING_VALUE_LARGE(kPALMSOURCE_OS_VERSION, "PALMSOURCE_OS_VERSION", );
B_STATIC_STRING_VALUE_LARGE(kPALMSOURCE_OS_VERSION_LONG, "PALMSOURCE_OS_VERSION_LONG", );
B_STATIC_STRING_VALUE_LARGE(kSYSTEM_DIRECTORY, "SYSTEM_DIRECTORY", );

B_STATIC_STRING_VALUE_SMALL(kres, "res", );
B_STATIC_STRING_VALUE_LARGE(k__get__, "__get__", );
B_STATIC_STRING_VALUE_LARGE(k__set__, "__set__", );

B_STATIC_STRING_VALUE_SMALL(kPound, "#", );
B_STATIC_STRING_VALUE_SMALL(kUnderscore, "_", );

B_STATIC_STRING_VALUE_SMALL(kC_OPT, "-c", );
B_STATIC_STRING_VALUE_SMALL(kS_OPT, "-s", );
B_STATIC_STRING_VALUE_LARGE(kLOGIN_OPT, "--login", );

#define ALWAYS_SET_PROMPTS	1
#define USER_PS1_PLAIN		"$PWD\\$ "
#define USER_PS1_COLOR		COLOR(YELLOW,       USER_PS1_PLAIN)
#define ROOT_PS1_PLAIN		"$PWD\\# "
#define ROOT_PS1_COLOR		COLOR(RED,          ROOT_PS1_PLAIN)
#define USER_PS2_PLAIN		"\\> "
#define USER_PS2_COLOR		COLOR(LIGHT_PURPLE, USER_PS2_PLAIN)

SValue SelfBinder::Inspect(const sptr<IBinder>&, const SValue &which, uint32_t )
{
	SValue myint = GetScriptProperty(kINTERFACE, true);
	SValue result;
	
	void* cookie = NULL;
	SValue key, value;
	while (myint.GetNextItem(&cookie, &key, &value) == B_OK) {
		if (key.IsWild()) result.Join(which * SValue(value, SValue::Binder(this)));
		else result.Join(which * SValue(key, value));
	}
	
	return result;
}

status_t SelfBinder::Told(const SValue& what, const SValue& in)
{
	if (what != kCONTEXT)
		SetScriptProperty(what, in);
	return B_OK;
}

status_t SelfBinder::Asked(const SValue& what, SValue* out)
{
	*out = GetScriptProperty(what);
	if (out->HasItem(kCONTEXT)) out->Remove(SValue(kCONTEXT, B_WILD_VALUE));
	return B_OK;
}

status_t SelfBinder::Called(const SValue& func, const SValue& args, SValue* out)
{
	bool found;
	*out = ExecScriptFunction(func, args, &found);
	if (found && !out->HasItem(kres)) {
		*out = SValue(kres, *out);
	}
	return B_OK;
}

BShell::BShell(const SContext& context)
	: BCommand(context), SelfBinder(context)
	, m_lock("BShell::m_lock")
	, m_hasEnvironment(false)
	, m_exitRequested(false)
{
}

BShell::~BShell()
{
#if 0
	SPackageSptr image;
	bout << "BShell " << this << " exiting.  " << (image.Image()->StrongCount()-1)
		<< " refs remain on image." << endl;
#endif
}

void BShell::InitAtom()
{
	BCommand::InitAtom();
	SelfBinder::InitAtom();
}

void BShell::SetProperty(const SValue& key, const SValue& value)
{
	if (!m_hasEnvironment) SetEnvironmentFromHost();
	
	status_t err;
	int32_t idx=key.AsInt32(&err);
	if (err == B_OK) {
		// Don't allow setting of position parameters.
		return;
	}
	
	BCommand::SetProperty(key, value);
}

SValue BShell::GetProperty(const SValue& key) const
{
	// Don't set up the initial environment if kINTERFACE is
	// being requested, since the caller will pretty much always
	// do an Inspect() on us.
	if (!m_hasEnvironment && key != kINTERFACE) const_cast<BShell*>(this)->SetEnvironmentFromHost();

	status_t err;
	int32_t idx=key.AsInt32(&err);
	if (err == B_OK) {
		SLocker::Autolock _l(m_lock);
		if (idx >= 0 && idx <m_args.CountItems())
			return m_args[idx];
		return SValue::Undefined();
	} else if (key == kPound) {
		SLocker::Autolock _l(m_lock);
		return SValue::Int32(m_args.CountItems());
	} else if (key == kUnderscore) {
		SLocker::Autolock _l(m_lock);
		return m_lastResult;
	}
	
	return BCommand::GetProperty(key);
}

void BShell::SetEnvironment(const SValue& env)
{
	BCommand::SetEnvironment(env);
	BCommand::SetProperty(kCONTEXT, SValue::Binder(Context().Root()->AsBinder()));
	BCommand::SetProperty(kWILD, SValue::Wild());
	BCommand::SetProperty(kSELF, SValue::WeakBinder((SelfBinder*)this));
	BCommand::SetProperty(kSYSTEM_DIRECTORY, SValue::String(get_system_directory()));
	m_hasEnvironment = true;
}

void BShell::SetVMArguments(const SValue& args)
{
	if (!m_hasEnvironment) SetEnvironmentFromHost();
	
	void* cookie=NULL;
	SValue key, value;
	while (args.GetNextItem(&cookie, &key, &value) == B_OK) {
		if (key != kCONTEXT && key != kWILD && key != kSELF
				&& key != kSYSTEM_DIRECTORY) {
			SetProperty(key, value);
		}
	}
}

void BShell::GetArguments(SVector<SValue>* outArgs) const
{
	SLocker::Autolock _l(m_lock);
	*outArgs = m_args;
}

void BShell::SetArguments(const SVector<SValue>& args)
{
	SLocker::Autolock _l(m_lock);
	m_args = args;
}

void BShell::SetLastResult(const SValue& val)
{
	SLocker::Autolock _l(m_lock);
	m_lastResult = val;
}

extern char **environ;

void BShell::SetEnvironmentFromHost()
{
	SValue env;
	
	// Collect all environment variables.
	char** e = environ;
	while (e && *e) {
		char* p = strchr(*e, '=');
		if (p) {
			env.JoinItem(SValue::String(SString(*e, p-*e)), SValue::String(SString(p+1)));
		}
		e++;
	}
	
	// XXX Until we unify the host and binder namespaces,
	// force the current directory to start at the root.
	env.Overlay(SValue(kBSH_HOST_PWD, env[kPWD]));
	env.Overlay(SValue(kPWD, SValue::String("/")));
	
	SetEnvironment(env);
}

SString BShell::GetPS1()
{
	return GetProperty(kPS1).AsString();
}

SString BShell::GetPS2()
{
	return GetProperty(kPS2).AsString();
}

void BShell::RequestExit()
{
	SLocker::Autolock _l(m_lock);
	m_exitRequested = true;
}

bool BShell::ExitRequested() const
{
	SLocker::Autolock _l(m_lock);
	return m_exitRequested;
}

SValue BShell::GetScriptProperty(const SValue& key, bool safe)
{
	SString get_func = key.AsString();
	get_func.ToUpper(0, 1);
	sptr<ICommand> cmd = GetCommand(get_func);
	SString set_func = get_func;
	set_func.Prepend("Set");
	if (cmd != NULL) {
		ArgList cmdArgs;
		cmdArgs.AddItem(SValue::String(get_func));
		return cmd->Run(cmdArgs);
	}
	else if (GetCommand(set_func) != NULL) {
		// Do nothing, but don't do GetProperty, because the property
		// isn't automatic unless both the set and get func are not defined
		return SValue::Undefined();
	}
	else {
		//bout << "Shell: get prop " << key << endl;
		// If there is a generic property get function, use that.
		// This will hide all other properties.
		sptr<ICommand> cmd = GetCommand(k__get__);
		if (cmd != NULL) {
			ArgList cmdArgs;
			cmdArgs.AddItem(k__get__);
			cmdArgs.AddItem(key);
			SValue result = cmd->Run(cmdArgs);
			if (!safe || result.IsDefined()) return result;
		}
		return GetProperty(key);
	}
}

void BShell::SetScriptProperty(const SValue& key, const SValue& value)
{
	//bout << "BShell::SetScriptProperty key=" << key << " value=" << value << endl;

	// this sucks!  it's a function instead, so call that
	bool found;
	ExecScriptFunction(key, value, &found);
	if (found) return;

	// Given a function Prop, if there is a function called Prop
	// or a function called SetProp, then don't automatically set
	// the variable.
	SString set_func = key.AsString();
	set_func.ToUpper(0, 1);
	SString get_func = set_func;
	set_func.Prepend("Set");
	sptr<ICommand> cmd = GetCommand(set_func);
	if (cmd != NULL) {
		ArgList cmdArgs;
		cmdArgs.AddItem(SValue::String(set_func));
		cmdArgs.AddItem(value);
		cmd->Run(cmdArgs);
	}
	else if (GetCommand(get_func) != NULL) {
		// Do nothing, but don't do SetProperty, because the property
		// isn't automatic unless both the set and get func are not defined
		;
	}
	else {
		// If there is a generic property set function, use that.
		// This will hide all other properties.
		sptr<ICommand> cmd = GetCommand(k__set__);
		if (cmd != NULL) {
			ArgList cmdArgs;
			cmdArgs.AddItem(k__set__);
			cmdArgs.AddItem(key);
			cmdArgs.AddItem(value);
			cmd->Run(cmdArgs);
		}
		//bout << "Shell: set prop " << key << " = " << value << endl;
		SetProperty(key, value);
	}
}

SValue BShell::ExecScriptFunction(const SValue& func, const SValue& args, bool* outFound)
{
	//bout << "Shell: exec func " << func << " ( " << args << " )" << endl;
	sptr<ICommand> cmd = GetCommand(func.AsString());
	if (cmd != NULL) {
		*outFound = true;
		ArgList cmdArgs;
		cmdArgs.AddItem(func);
		SValue key, value;
		void* cookie = NULL;
		while (args.GetNextItem(&cookie, &key, &value) == B_OK) {
			status_t err;
			int32_t i = key.AsInt32(&err);
			if (err == B_OK) cmdArgs.AddItem(value);
		}
		return cmd->Run(cmdArgs);
	}
	*outFound = false;
	return SValue::Undefined();
}

#if TARGET_HOST == TARGET_HOST_PALMOS

B_STATIC_STRING_VALUE_8	(key_creator,		"creator", );
B_STATIC_STRING_VALUE_8	(key_type,			"type", );
B_STATIC_STRING_VALUE_8	(key_resid,			"resid", );
B_STATIC_STRING_VALUE_8	(key_restype,		"restype", );

#endif

SValue BShell::Run(const ArgList& args)
{
	sptr<ITextInput> textInput;

	SValue opt;
	size_t opti = 1;
	bool login = false;
	bool interactive = false;

	const size_t N = args.CountItems();
	
	SString path;
	
	if (!m_hasEnvironment) SetEnvironmentFromHost();

	while (opti < N) {
		opt = args[opti++];

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

		if (opt == kC_OPT)
		{
			sptr<BMallocStore> mem = new BMallocStore();
			if (mem != NULL) {
				if (opti >= N) {
					TextError() << "bsh: No argument for -c" << endl;
					return SValue::Status(B_BAD_VALUE);
				}
				SString string = args[opti].AsString();
				opti++;
				mem->WriteAt(0, string.String(), string.Length());

				// without a trailing newline, the buffer won't
				// be parsed correctly (due to the way Lexer::Buffer()
				// works, the last token on the last line of the input
				// gets destroyed)
				mem->WriteAt(string.Length(), "\n", 1);
				textInput = new BTextInput(mem);
			}
		}
		else if (opt == kS_OPT)
		{
			if (opti >= N) {
				TextError() << "bsh: No argument for -s" << endl;
				return SValue::Status(B_BAD_VALUE);
			}
			if (find_some_input(args[opti], this, &textInput, &path) == false) {
				TextError() << "bsh: unable to use " << args[opti] << " for input." << endl;
				return SValue::Status(B_BAD_VALUE);
			}
			opti++;
		}
		else if (opt == kLOGIN_OPT)
		{
			login = true;

			// Um....  run the .profile.  Yeah, that's it.

			SetProperty(kECHO, B_1_INT32);
			SetProperty(kUSER, kBARON);

#if SUPPORTS_FEATURES
			sptr<IFeatures> features = IFeatures::AsInterface(Context().LookupService(SString("compat/feature")));
			if (features != NULL) {
				uint32_t fval;
				if (features->GetFeature(sysFtrCreator, sysFtrNumROMVersion, &fval) == errNone) {
					SString vers("");
					vers << sysGetROMVerMajor(fval)
						<< "." << sysGetROMVerMinor(fval);
					SetProperty(kPALMSOURCE_OS_VERSION, SValue::String(vers));
					switch (sysGetROMVerStage(fval)) {
						case sysROMStageRelease:
							vers << "-RELEASE"; break;
						case sysROMStageAlpha:
							vers << "-ALPHA"; break;
						case sysROMStageBeta:
							vers << "-BETA"; break;
						case sysROMStageDevelopment:
						default:
							vers << "-DEVEL"; break;
					}
					SetProperty(kPALMSOURCE_OS_VERSION_LONG, SValue::String(vers));
				}
			}
#endif

#if TARGET_PLATFORM == TARGET_PLATFORM_PALMSIM_WIN32
			SetProperty(kPALMSOURCE_PLATFORM, kPLATFORM_PALMSIM_WIN32);
#else
			SetProperty(kPALMSOURCE_PLATFORM, kPLATFORM_DEVICE_ARM);
#endif

#if BUILD_TYPE == BUILD_TYPE_DEBUG
			SetProperty(kPALMSOURCE_BUILDTYPE, kBUILD_TYPE_DEBUG);
#elif BUILD_TYPE == BUILD_TYPE_RELEASE
			SetProperty(kPALMSOURCE_BUILDTYPE, kBUILD_TYPE_RELEASE);
#else
			SetProperty(kPALMSOURCE_BUILDTYPE, kBUILD_TYPE_TEST);
#endif

		}
		else
			bout << "sh: unknown option " << opt << endl;
	}

	SVector<SValue> newArgs;
	
	if (textInput == NULL) {
		if (opti < N) {
			if (find_some_input(args[opti], this, &textInput, &path) == false) {
				TextError() << "bsh: unable to use " << args[opti] << " for input." << endl;
				return SValue::Status(B_BAD_VALUE);
			}
			newArgs.AddItem(args[opti]);
			opti++;
		} else if (N > 0) {
			newArgs.AddItem(args[0]);
			SetLastResult(args[0]);
		}
	}
	
	if (path != "") {
		SetLastResult(SValue::String(path));
	}
	SetProperty(kBSH_SCRIPT_FILE, SValue::String(path));
	SString parent;
	path.PathGetParent(&parent);
	SetProperty(kBSH_SCRIPT_DIR, SValue::String(parent));
	
	// Collect shell arguments.
	while (opti < N) {
		newArgs.AddItem(args[opti++]);
	}
	
	SetArguments(newArgs);
	
	if (textInput == NULL) {
		// If no input stream was given, run in interactive mode.
	
		SContext context(Context());

		// we want to show a different "root" prompt to remind you when
		// you're in a privileged context
		bool isSystem = ((context.Root() != NULL) && 
				context.Lookup(SString("/processes/system")).IsDefined());

		// Make sure prompts are set.
		if (ALWAYS_SET_PROMPTS || !GetProperty(kPS1).IsDefined()) {
			SetProperty(kPS1, 
				SValue::String(GetColorFlag() 
					? (isSystem ? ROOT_PS1_COLOR : USER_PS1_COLOR) 
					: (isSystem ? ROOT_PS1_PLAIN : USER_PS1_PLAIN)));
		}
		if (ALWAYS_SET_PROMPTS || !GetProperty(kPS2).IsDefined()) {
			SetProperty(kPS2, 
				SValue::String(GetColorFlag() 
					? USER_PS2_COLOR : USER_PS2_PLAIN));
		}

		interactive = true;
		textInput = new BTextInput(ByteInput());

		if (login) {
			TextOutput() << (GetColorFlag() ? "Welcome to " ANSI_BLUE "Palm OS " ANSI_NORMAL : "Welcome to Palm OS ")
				<< GetProperty(kPALMSOURCE_OS_VERSION_LONG).AsString()
				<< " on "
				<< GetProperty(kPALMSOURCE_PLATFORM).AsString()
				<< "." << endl
				<< (GetColorFlag() ? "Type " ANSI_GREEN "'help'" ANSI_NORMAL " to get started." : "Type 'help' to get started.") << endl << endl;

			// XXX A little bit of fun...
			ArgList args;
			args.AddItem(SValue::String("fortune"));
			if (args.CountItems() > 0) {
				sptr<ICommand> fortune = Spawn(args[0].AsString());
				if (fortune != NULL) {
					fortune->Run(args);
					fortune = NULL;
					TextOutput() << endl;
				}
			}
		}
	}

	if (textInput != NULL) {
		sptr<Lexer> lexer = new Lexer(this, textInput, TextOutput(), interactive);
		if (lexer != NULL) {
			Parser parser(this);
			return parser.Parse(lexer);
		}
	}

	TextError() << "bsh: no input stream!" << endl;

	return SValue::Int32(B_ERROR);
}

#if TARGET_HOST == TARGET_HOST_WIN32
int main(int argc, char** argv)
{
	SValue args;
	for (int i = 0 ; i < argc ; i++)
	{
		args.JoinItem(SValue::Int32(i), SValue::String(argv[i]));
	}

//	sptr<BShell> shell = new BShell();
//	shell->Run(args);
//
	return 0;
}
#endif

extern "C" sptr<IBinder> InstantiateComponent(const SString& component, const SContext& context, const SValue& args)
{
	sptr<IBinder> obj;

	if (component == "" || component == "Shell")
	{
		obj = (BCommand*)new BShell(context);
	}
	else if (component == "VM")
	{
		obj = new VirtualMachine(context);
	}
	else if (component == "ConditionVariable")
	{
		obj = (new ConditionVariable(args));
	}
	else if (component == "Help")
	{
		obj = new BHelp(context);
	}
	else if (component == "Sleep")
	{
		obj = new BSleep(context);
	}
	else if (component == "StrError")
	{
		obj = new BStrError(context);
	}
	else if (component == "Db")
	{
		obj = new BDb(context);
	}
	else if (component == "Reboot")
	{
		obj = new BReboot(context);
	}
	else if (component == "Echo")
	{
		obj = new BEcho(context);
	}
	else if (component == "Env")
	{
		obj = new BEnv(context);
	}
	else if (component == "Test")
	{
		obj = new Test(context);
	}
	else if (component == "Components")
	{
		obj = new BComponents(context);
	}
	else if (component == "Context")
	{
		obj = new BContextCommand(context);
	}
	else if (component == "SU")
	{
		obj = new BSU(context);
	}
	else if (component == "New")
	{
		obj = new BNew(context);
	}
	else if (component == "NewProcess")
	{
		obj = new BNewProcess(context);
	}
	else if (component == "StopProcess")
	{
		obj = new BStopProcess(context);
	}
	else if (component == "Inspect")
	{
		obj = new BInspect(context);
	}
	else if (component == "Invoke")
	{
		obj = new BInvoke(context);
	}
	else if (component == "Put")
	{
		obj = new BPut(context);
	}
	else if (component == "Get")
	{
		obj = new BGet(context);
	}
	else if (component == "Ls")
	{
		obj = new BLs(context);
	}
	else if (component == "Publish")
	{
		obj = new BPublish(context);
	}
	else if (component == "Unpublish")
	{
		obj = new BUnpublish(context);
	}
	else if (component == "Lookup")
	{
		obj = new BLookup(context);
	}
	else if (component == "Mkdir")
	{
		obj = new BMkdir(context);
	}
	else if (component == "Link")
	{
		obj = new BLink(context);
	}
	else if (component == "Spawn")
	{
		obj = new BSpawn(context);
	}
	else if (component == "True")
	{
		obj = (new BTrue(context));
	}
	else if (component == "False")
	{
		obj = (new BFalse(context));
	}
	else if (component == "Time")
	{
		obj = (new BTime(context));
	}
	else if (component == "Push")
	{
		obj = (new BPush(context));
	}
	else if (component == "Clear")
	{
		obj = (new BClear(context));
	}
	else if (component == "Cat")
	{
		obj = (new BCat(context));
	}
#if 0
	else if (component == "CheckNamespace")
	{
		obj = (new BCheckNamespace(context));
	}
#endif

	return obj;
}
