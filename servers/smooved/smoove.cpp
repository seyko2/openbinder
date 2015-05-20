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

#include <string.h>

#include <storage/File.h>
#include <support/Catalog.h>
#include <support/IProcessManager.h>
#include <support/Iterator.h>
#include <support/Looper.h>
#include <support/MemoryStore.h>
#include <support/SignalHandler.h>
#include <support/StdIO.h>
#include <support/String.h>
#include <support/TextStream.h>

#include <package_p/PackageManager.h>
#include <support_p/SupportMisc.h>

#include <app/ICommand.h>
#include <package/IInstallHandler.h>

#include <SysThread.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::package;
using namespace palmos::support;
using namespace palmos::storage;
using namespace palmos::app;
#endif

static SContext g_rootContext;
bool verbose = false;

#include <DebugMgr.h>

#define DUMP_PACKAGE_CATALOG 0

B_STATIC_STRING_VALUE_LARGE(BV_PURPOSES, "purposes",)
B_STATIC_STRING_VALUE_LARGE(BV_PURPOSE, "purpose",)
B_STATIC_STRING_VALUE_LARGE(BV_MIMETYPES, "mimetypes",)
B_STATIC_STRING_VALUE_LARGE(BV_INTENT_TYPE, "intents/*/type",)
B_STATIC_STRING_VALUE_LARGE(BV_INTENT_VERB, "intents/*/verb",)
B_STATIC_STRING_VALUE_LARGE(BV_VERBS, "verbs",)
B_STATIC_STRING_VALUE_SMALL(BV_BIN, "bin",)
B_STATIC_STRING_VALUE_LARGE(BV_PACKAGES_PATH, "/packages",)
B_STATIC_STRING_VALUE_LARGE(BV_INSTALL_HANDLERS_PATH, "/packages/interfaces/org.openbinder.package.IInstallHandler",)
B_STATIC_STRING_VALUE_LARGE(BV_INSTALL_HANDLERS, "install_handlers",)

// For process manager.
B_STATIC_STRING_VALUE_LARGE(BV_PROCESSES_PATH, "/processes",)
B_STATIC_STRING_VALUE_LARGE(BV_PROCESS, "process",)
B_STATIC_STRING_VALUE_LARGE(BV_PREFER, "prefer",)
B_STATIC_STRING_VALUE_LARGE(BV_REQUIRE, "require",)
B_STATIC_STRING_VALUE_LARGE(BV_PACKAGE, "package",)
B_STATIC_STRING_VALUE_LARGE(BV_PACKAGEDIR, "packagedir",)

// =================================================================

// This function starts the Package Manager, sets up some standard
// queries on its component information, and publishes in the namespace.
void
start_package_manager(const SContext& context)
{
	// Set up the package manager
	// ------------------------------------------------------------------
	// and load into /packages
	sptr<BPackageManager> pkgMgr = new BPackageManager(context);
	pkgMgr->AddQuery(BV_PURPOSES, BV_PURPOSE);
	pkgMgr->AddQuery(BV_BIN, BV_BIN);
	pkgMgr->AddQuery(BV_MIMETYPES, BV_INTENT_TYPE);
	pkgMgr->AddQuery(BV_VERBS, BV_INTENT_VERB);
	pkgMgr->Start(verbose);
	context.Publish(BV_PACKAGES_PATH, SValue::Binder((BnCatalog*)pkgMgr.ptr()));
	
	// Print it out
#if DUMP_PACKAGE_CATALOG
	if (verbose) {
		bout << "Components: " << endl << "-------------------------" << endl;
		
		SValue key, value;
		SIterator it(context, SString("/packages/components"));
		while (it.Next(&key, &value, INode::REQUEST_DATA) == B_OK) {
			bout << SValue(key, value) << endl;
		}
	}
#endif
}

// =================================================================

// This is a simple process manager, implementing some basic policies
// for how components can request the kind of process to run in.
// XXX This should be turned into a component (probably as part of the
// package kit) that the boot script instantiates.

class ProcessManager : public BCatalog, public BnProcessManager
{
public:
	ProcessManager(const SContext& context)
		: BCatalog(context)
		, BnProcessManager(context)
		, m_lock("ProcessManager:m_lock")
	{
	}
	
	virtual SValue Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags)
	{
		SValue result(BCatalog::Inspect(caller, which, flags));
		result.Join(BnProcessManager::Inspect(caller, which, flags));
		return result;
	}
	
	virtual sptr<IBinder> NewIfRemote(const sptr<INode>& context, const SString& component,
		const SValue& args, uint32_t flags, const sptr<IProcess>& caller,
		SValue* componentInfo, status_t* status = NULL)
	{
		// First retrieve information about this component from the
		// context that was passed in.
		SContext ctx(context);
		sptr<IBinder> object;
		
		SValue procReq;
		SString procName;
		bool requireProc = false;
		
		status_t err = ctx.LookupComponent(component, componentInfo);
		if (err < B_OK) goto finished;
		
		//bout << "NewIfRemote " << component << ": info=" << *componentInfo << endl;
		
		procReq = (*componentInfo)[BV_PROCESS];
		if (procReq.IsDefined()) {
			// This component has some specific request about its process.
			procName = procReq[BV_REQUIRE].AsString();
			if (procName != "") {
				requireProc = true;
			} else {
				procName = procReq[BV_PREFER].AsString();
			}
			if (procName == BV_PACKAGE) procName = (*componentInfo)[BV_PACKAGEDIR].AsString();
		}
		
		//bout << "procName=" << procName << endl;
		
		if (procName == "") {
			// If no process was requested, don't do anything remote.
			goto finished;
		}
		if ((flags&SContext::PROCESS_MASK) == SContext::REQUIRE_LOCAL) {
			// If the caller requires for the component to be local, then
			// we have two choices: if the component requires a specific
			// process, then we fail, otherwise we can honor the local request.
			err = requireProc ? B_PERMISSION_DENIED : B_OK;
			goto finished;
		}
		
		// Okay, we need to instantiate the component in the process it
		// requested.
		// XXX We shouldn't be doing the instantiate here if the process the
		// component requested is the same as the caller!
		object = remote_instantiate(context, component, args, *componentInfo, procName, &err);
		
	finished:
		if (status) *status = err;
		return object;
	}

private:
	sptr<IBinder> remote_instantiate(const SContext& context, const SString& component,
		const SValue& args, const SValue& componentInfo, const SString& procName, status_t* err)
	{
		SValue procValue;
		sptr<IProcess> proc;
		
		{
			SLocker::Autolock _l(m_lock);
			
			LookupEntry(procName, 0, &procValue);
			proc = interface_cast<IProcess>(procValue);
			//bout << "Found proc " << procName << ": " << proc << endl;
		
			if (proc == NULL) {
				// Process doesn't exist...  create and publish it.
				proc = BCatalog::Context().NewProcess(procName, SContext::PREFER_REMOTE, B_UNDEFINED_VALUE, err);
				if (proc != NULL) {
					SValue procValue(SValue::WeakBinder(proc->AsBinder()));
					//bout << "Proc value: " << procValue << endl;
					AddEntry(procName, procValue);
				}
			}
		}
		
		if (proc != NULL) {
			return proc->InstantiateComponent(context.Root(), componentInfo, component, args, err);
		}
		
		if (*err == B_OK) *err = B_ERROR;
		return NULL;
	}
	
	SLocker	m_lock;
};

// Function to start and publish our custom process manager.

void
start_process_manager(const SContext& context)
{
	sptr<ProcessManager> mgr = new ProcessManager(context);
	context.Publish(BV_PROCESSES_PATH, SValue::Binder((BnCatalog*)mgr.ptr()));
}

// =================================================================

// This is the entry point for a function that will run an interactive
// shell session on the current process's standard input and output streams.

static void
start_shell(void* /*unused*/)
{
	sptr<ICommand> shell = ICommand::AsInterface(g_rootContext.New(SValue::String("org.openbinder.tools.BinderShell")));
	
	if (shell != NULL) {
		shell->SetByteInput(StandardByteInput());
		shell->SetByteOutput(StandardByteOutput());
		shell->SetByteError(StandardByteOutput());
		
		ICommand::ArgList args;
		args.AddItem(SValue::String("sh"));
		args.AddItem(SValue::String("--login"));
		SValue result = shell->Run(args);
		exit(result.AsInt32());
	}
}

// =================================================================

#if DUMP_PACKAGE_CATALOG
static void
print_entries_recursively(const sptr<IBinder>& b)
{
	sptr<IDatum> datum = interface_cast<IDatum>(b);
	if (datum != NULL) {
		bout << datum->Value();
	}
	
	sptr<IIterable> iterable = interface_cast<IIterable>(b);
	if (iterable != NULL) {
		bout << "{" << indent << endl;
		SIterator iterator(iterable);
		SValue key, value;
		while (B_OK == iterator.Next(&key, &value)) {
			bout << key.AsString() << "-->";
			print_entries_recursively(value.AsBinder());
			bout << endl;
		}
		bout << dedent << "}" << endl;
	}
}
#endif // DUMP_PACKAGE_CATALOG

// =================================================================

// SmooveD is the context manager, and this is the function that gets
// called when others want to request a context.  Right now the implementation
// is very simple: only allow access to the user context, unless we
// were started with BINDER_NO_CONTEXT_SECURITY set in which case access
// is granted to all contexts.
static bool validate_context(const SString& name, const sptr<IBinder>& caller, void* /*userData*/)
{
	//bout << "Validate context: name=" << name << ", caller=" << caller << endl;
	
	char* nosecurity = getenv("BINDER_NO_CONTEXT_SECURITY");
	if (name == "user") return true;
	
	return (nosecurity && atoi(nosecurity) != 0) ? true : false;
}

// =================================================================
static int
usage()
{
	fprintf(stderr, "usage: smooved [-s] [-n] [-e] [-h] [SCRIPT]\n");
	fprintf(stderr, "    -s: Drop into the Binder Shell after booting.\n");
	fprintf(stderr, "    -n: Don't run the boot script.\n");
	fprintf(stderr, "    -e: Exit when finished running the boot script.\n");
	fprintf(stderr, "    -h: Show this help.\n");
	fprintf(stderr, "    SCRIPT: (optional) An alternative boot script.\n");
	return 1;
}

int main(int argc, char* argv[])
{
	bout << "SmooveD is entering the picture..." << endl;

	setbuf(stdout, NULL ) ; // unbuffered I/O on stdout so we can debug before the inivitible crash
	openlog("smooved", LOG_PERROR | LOG_NDELAY, LOG_USER);
	
	if (!SLooper::BecomeContextManager(validate_context, NULL)) {
		fprintf(stderr, "smooved FAILED to become the context manager, exiting...\n");
		return B_ERROR;
	}

	// Create the root context All the contexts derive from this one
	g_rootContext = SContext(new BCatalog());

	start_package_manager(g_rootContext);
	start_process_manager(g_rootContext);

#if DUMP_PACKAGE_CATALOG
	{
		SValue test = g_rootContext.Lookup(SString("/packages"));
		print_entries_recursively(test.AsBinder());
	}
#endif
	
	sptr<IBinder> context = g_rootContext.Root()->AsBinder();
	SLooper::SetContextObject(context, SString("user"));
	SLooper::SetContextObject(context, SString("system"));
	
	SString script = get_system_directory();
	script.PathAppend("scripts");

	bool interactive = false;
	bool scriptSet = false;
	bool no_bootscript = false;
	bool exitWhenDone = false;

	int optind = 1;
	while (optind < argc) {
		SString arg(argv[optind]);

		if (arg == "-s" || arg == "--shell") {
			interactive = true;
			exitWhenDone = false;
		} else if (arg == "-n") {
			interactive = true;
			no_bootscript = true;
			exitWhenDone = false;
		} else if (arg == "-e") {
			exitWhenDone = true;
		} else if (arg == "-h" || arg == "--help" || arg == "?") {
			usage();
			return 0;
		} else if (arg[(size_t)0] == '-') {
			fprintf(stderr, "Unknown option: %s\n", arg.String());
			return usage();
		} else {
			script.PathAppend(arg);
			scriptSet = true;
		}
		optind++;
	}
	
	if (!no_bootscript) {
		if (!scriptSet) {
			script.PathAppend("boot_script.bsh");
		}

		status_t err = g_rootContext.RunScript(script);
		if (err != B_OK) {
			berr << "[smooved]: failed to load script '" << script << "'" << endl;	
		}

		// now that we have booted run the OnInstall's
		SValue entry = g_rootContext.Lookup(BV_INSTALL_HANDLERS_PATH);
	
		if (entry != B_UNDEFINED_VALUE)
		{	
			sptr<IProcess> process = g_rootContext.NewProcess(BV_INSTALL_HANDLERS);
		
			void* cookie = NULL;
			SValue key, value;
			while (entry.GetNextItem(&cookie, &key, &value) == B_OK)
			{
				sptr<IBinder> binder = g_rootContext.RemoteNew(value, process);
				sptr<IInstallHandler> handler = interface_cast<IInstallHandler>(binder);

				if (handler != NULL)
				{
					handler->OnInstall(IInstallHandler::BOOT);
				}
			}
		}
	}

	if (!exitWhenDone) {
		if (interactive) {
			SysHandle thread;
			status_t err = SysThreadCreateEZ("shell", start_shell, NULL, &thread);
			if (err == B_OK)
			{
				SysThreadStart(thread);
			}
		}

		// We don't want this thread to exit, so add it to the pool.  It will
		// exit when it's time for the process to exit.
		SLooper::Loop(true);
	}
	
	return B_OK;
}

