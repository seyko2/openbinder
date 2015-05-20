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
#include <support/Context.h>

#if !LIBBE_BOOTSTRAP

#include <support/INode.h>
#include <support/IProcessManager.h>

#include <support/Catalog.h>
#include <support/Looper.h>
#include <support/StdIO.h>
#include <support/Process.h>
#include <support/SortedVector.h>
#include <app/ICommand.h> // I think ICommand belongs in the support kit --joe
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

B_STATIC_STRING_VALUE_LARGE(kProcesses, "/processes", );

// This helper function takes care of fixing up a component name
// that is actually a component ID mapped to its arguments.
static void parse_component(const SValue& component, const SValue& xtraArgs, SString* outID, SValue* outArgs)
{
	if (component.IsSimple()) {
		*outID = component.AsString();
		*outArgs = xtraArgs;
	} else {
		// Mapping is the form: component -> args.
		void* cookie = NULL;
		SValue idVal;
		component.GetNextItem(&cookie, &idVal, outArgs);
		*outID = idVal.AsString();
		outArgs->Overlay(xtraArgs);
	}
}

SContext::SContext()
	:	 m_root(NULL)
{
}

SContext::SContext(const sptr<INode>& root)
	:	m_root(root)
{
}

SContext::SContext(const SContext& context)
	:	m_root(context.m_root)
{
}

SContext::~SContext()
{
}

SContext& SContext::operator=(const SContext& o)
{
	m_root = o.m_root;
	return *this;
}

bool SContext::operator==(const SContext& o) const { return m_root == o.m_root; }
bool SContext::operator!=(const SContext& o) const { return m_root != o.m_root; }
bool SContext::operator<(const SContext& o) const { return m_root < o.m_root; }
bool SContext::operator<=(const SContext& o) const { return m_root <= o.m_root; }
bool SContext::operator>=(const SContext& o) const { return m_root >= o.m_root; }
bool SContext::operator>(const SContext& o) const { return m_root > o.m_root; }

status_t SContext::InitCheck() const
{
	return m_root != NULL ? B_OK : B_NO_INIT;
}

sptr<IBinder> SContext::LookupService(const SString& name) const
{
	SString path("/services");
	path.PathAppend(name);

	SValue value = SNode(m_root).Walk(&path, (uint32_t)0);
	
//	bout << "*** Lookup() name = " << name << endl;
//	bout << "             value = " << value << endl;
	return value.AsBinder();
}


status_t SContext::PublishService(const SString& name, const sptr<IBinder>& object) const
{
	SString path("/services");
	path.PathAppend(name);
	SString leaf(path.PathLeaf());
	path.PathGetParent(&path);
	
//	bout << "--- PublishService() path = " << path << " leaf = " << leaf << endl;

	SValue value = SNode(m_root).Walk(&path, INode::CREATE_CATALOG);
	
	status_t err;
	sptr<ICatalog> catalog = ICatalog::AsInterface(value, &err);
	if (catalog != NULL) return catalog->AddEntry(leaf, SValue::Binder(object));

#if BUILD_TYPE == BUILD_TYPE_DEBUG
	bout << "*** NAME NOT FOUND when publishing service " << object << " as " << name << endl;
#endif
	return err == B_OK ? B_NAME_NOT_FOUND : err;
}


sptr<IBinder> SContext::New(const SValue &component, const SValue &args,
	uint32_t flags, status_t* outError) const
{
	return RemoteNew(component, SLooper::Process(), args, flags, outError);
}

sptr<IBinder> SContext::RemoteNew(const SValue &component, const sptr<IProcess>& process,
	const SValue &args, uint32_t flags, status_t* outError) const
{
	// XXX Note that we should be caching a lot of the information we retrieve
	// here, so we don't have to do all this work for every component instantiation.
	SString id;
	SValue allArgs, componentInfo;
	
	parse_component(component, args, &id, &allArgs);

	sptr<IProcessManager> pmgr = interface_cast<IProcessManager>(
		SNode(m_root).Walk(kProcesses, (uint32_t)0));
	if (pmgr != NULL) {
		status_t err;
		sptr<IBinder> obj = pmgr->NewIfRemote(m_root, id, allArgs, flags, process, &componentInfo, &err);
		if (obj != NULL || err != B_OK) {
			if (outError) *outError = err;
			return obj;
		}
	} else {
		status_t err = LookupComponent(id, &componentInfo);
		if (err != B_OK) {
			if (outError != NULL) *outError = err;
			return NULL;
		}
	}
	
	sptr<IProcess> realProcess(process);
	
	// XXX I don't know if this is exactly what we want to do...  it means
	// that the new component for the process is a child of ours, which is
	// both good and bad...  Maybe there should be some other flag that you
	// want it to be in its own process that is independent of this one.
	if ((flags&PROCESS_MASK) == PREFER_REMOTE || ((flags&PROCESS_MASK) == REQUIRE_REMOTE)) {
		// The caller has asked that the component go in some other process, but the
		// package manager didn't do that.  Instead, what we will do is create a new
		// process in which to instantiate it.
		realProcess = NewProcess(id, flags, B_UNDEFINED_VALUE, outError);
		if (realProcess == NULL) return NULL;
	}
	
	return realProcess->InstantiateComponent(m_root, componentInfo, id, allArgs, outError);
}

static SString find_executable(const SString& executable)
{
	// First try to find the given executable in the current path...
	char* path = getenv("PATH");
	if (!path) path = "/bin/usr/bin";
	SString fullPath;
	while (path && *path) {
		char* sep = strchr(path, ':');
		if (sep && sep != path) {
			fullPath.SetTo(path, sep-path);
			fullPath.PathAppend(executable);
			int fd = open(fullPath.String(), O_RDONLY);
			if (fd >= 0) {
				close(fd);
				return fullPath;
			}
		}
		path = sep ? sep+1 : NULL;
	}
	
	// If we didn't find it in the path, try some other
	// standard places.
	fullPath = get_system_directory();
	fullPath.PathAppend("bin");
	fullPath.PathAppend(executable);
	int fd = open(fullPath.String(), O_RDONLY);
	if (fd >= 0) {
		close(fd);
		return fullPath;
	}
	
	return SString();
}

sptr<IProcess> SContext::NewProcess(const SString& name, uint32_t flags, const SValue& env, status_t* outError) const
{
	if (outError) *outError = B_OK;
	
	if ((flags&PROCESS_MASK) == PREFER_LOCAL || (flags&PROCESS_MASK) == REQUIRE_LOCAL) {
		return SLooper::Process();
	}
	
	if (!SLooper::PrefersProcesses() && (flags&PROCESS_MASK) != REQUIRE_REMOTE) {
		return SLooper::Process();
	}
	// If we ignored the preferres flag because REQUIRE_REMOTE is
	// set, then fail if we actually don't even support processes.
	if (!SLooper::SupportsProcesses()) {
		if (outError) *outError = B_UNSUPPORTED;
		return NULL;
	}
	
	// XXX BINDER_PROCESS_WRAPPER doesn't currently work, because we need to get
	// the pid of the real "binderproc" being run, not the wrapper command.
	SString wrapper(getenv("BINDER_PROCESS_WRAPPER"));
	SString executable("binderproc");
	
	executable = find_executable(executable);
	if (executable == "") {
		if (outError) *outError = B_ENTRY_NOT_FOUND;
		return NULL;
	}
	
	SVector<SString> args;
	if (wrapper == "") {
		wrapper = executable;
	} else {
		args.AddItem(executable);
	}
	args.AddItem(name);
	
	return interface_cast<IProcess>(NewCustomProcess(wrapper, args, flags, env, outError));
}

struct env_entry
{
	const char*	var;
	SString		buf;
};

B_IMPLEMENT_SIMPLE_TYPE_FUNCS(env_entry);

inline int32_t BCompare(const env_entry& v1, const env_entry& v2)
{
	const char* s1 = v1.var ? v1.var : v1.buf.String();
	const char* s2 = v2.var ? v2.var : v2.buf.String();
	while (*s1 != 0 && *s1 != '=' && *s2 != 0 && *s2 != '=' && *s1 == *s2) {
		s1++;
		s2++;
	}
	//printf("String %s @ %ld vs %s @ %ld\n", v1.var, s1-v1.var, v2.var, s2-v2.var);
	return int32_t(*s1) - int32_t(*s2);
}

inline bool BLessThan(const env_entry& v1, const env_entry& v2)
{
	return BCompare(v1, v2) < 0;
}

sptr<IBinder> SContext::NewCustomProcess(const SString& executable, const SVector<SString>& inArgs, uint32_t flags, const SValue& env, status_t* outError) const
{
	if (outError) *outError = B_OK;
	
	// XXX How can we handle BINDER_SINGLE_PROCESS here?
	size_t i;
	
	SString fullPath(find_executable(executable));
	if (fullPath == "") {
		if (outError) *outError = B_ENTRY_NOT_FOUND;
		return NULL;
	}
	
	SVector<const char*> argv;
	argv.AddItem(executable.String());
	for (i=0; i<inArgs.CountItems(); i++) argv.AddItem(inArgs[i].String());
	argv.AddItem(NULL);
	
	const char** newEnv = const_cast<const char**>(environ);
	SSortedVector<env_entry> entries;
	const char** allocEnv = NULL;
#if 0
	if (env.IsDefined() || (flags&B_FORGET_CURRENT_ENVIRONMENT)) {
		env_entry ent;
		if (!(flags&B_FORGET_CURRENT_ENVIRONMENT)) {
			const char** e = newEnv;
			while (e && *e) {
				ent.var = *e;
				entries.AddItem(ent);
				e++;
			}
			//for (size_t i=0; i<entries.CountItems(); i++) {
			//	bout << "Old Environment: " << entries[i].var << endl;
			//}
		}
		void* i = NULL;
		BValue k, v;
		ent.var = NULL;
		while (env.GetNextItem(&i, &k, &v) >= B_OK) {
			if (!k.IsWild()) {
				ent.buf = k.AsString();
				if (ent.buf != "") {
					ent.buf += "=";
					entries.RemoveItemFor(ent);
					if (!v.IsWild()) {
						// If v is wild, the entry is just removed.
						ent.buf += v.AsString();
						entries.AddItem(ent);
					}
				}
			}
		}
		const size_t N = entries.CountItems();
		allocEnv = static_cast<const char**>(malloc(sizeof(char*)*(N+1)));
		if (allocEnv) {
			for (size_t i=0; i<N; i++) {
				const env_entry& e = entries[i];
				allocEnv[i] = e.var ? e.var : e.buf.String();
				//bout << "New Environment: " << allocEnv[i] << endl;
			}
			allocEnv[N] = NULL;
			newEnv = allocEnv;
		}
	}
#endif

	pid_t pid = fork();

	if (pid == 0)
	{
		int err = execve(fullPath.String(), const_cast<char**>(argv.Array()), const_cast<char**>(newEnv));
		if (err < 0) {
			berr << "**** EXECV of " << fullPath << " returned err = " << strerror(errno)
				<< " (" << (void*)errno << ")" << endl;
			// XXX What to do on error?  This probably won't work,
			// because we are using the same Binder file descriptor as our parent.
			SLooper::This()->SendRootObject(NULL);
			DbgOnlyFatalError("execve failed!");
			_exit(errno);
		}
	}
	else if (pid < 0)
	{
		if (outError) *outError = errno;
		return NULL;
	}
	
	return SLooper::This()->ReceiveRootObject(pid);
}

SValue SContext::Lookup(const SString& location) const
{
	const SValue value = SNode(m_root).Walk(location);

//	bout << "*** Lookup() location = " << location << endl;
//	bout << "             value = " << value << endl;
	return value;
}

status_t SContext::Publish(const SString& location, const SValue& item) const
{
//	bout << "+++ Publish() location = " << location << endl;
//	bout << "              item = " << item << endl;

	SString path;
	location.PathGetParent(&path);

	SValue value = SNode(m_root).Walk(&path, INode::CREATE_CATALOG);
	status_t err;
	sptr<ICatalog> catalog = ICatalog::AsInterface(value, &err);
//		bout << "              catalog = " << catalog << " value = " << value << endl;
	if (catalog == NULL) return err;

	SString leaf(location.PathLeaf());
//	bout << "              adding = " << leaf << endl;
	return catalog->AddEntry(leaf, item);
}

status_t SContext::Unpublish(const SString& location) const
{
//	bout << "*** Unpublishing: '" << location << "'" << endl; 
	
	SString path;
	location.PathGetParent(&path);

	SValue value = SNode(m_root).Walk(&path, (uint32_t)0);
	status_t err;
	sptr<ICatalog> catalog = ICatalog::AsInterface(value, &err);
	if (catalog == NULL) return err;

	SString leaf(location.PathLeaf());
	return catalog->RemoveEntry(leaf);
}

sptr<INode> SContext::Root() const
{
	return m_root;
}

status_t SContext::LookupComponent(const SString& id, SValue* out_info) const
{
	SString path("/packages/components");
	path.PathAppend(id);
	
	if (id.Length() == 0) return B_BAD_VALUE;

	status_t err;
	*out_info = SNode(m_root).Walk(&path, &err);
//	bout << "+++ BConext::LookupComponent() err = " << strerror(err) << endl;
	if (err < B_OK) return err;

//	bout << "+++ BContext::LookupComponent() id = " << id << " out_info = " << *out_info << endl;
	return out_info->IsDefined() ? B_OK : B_NAME_NOT_FOUND;
}

SValue SContext::LookupComponentProperty(const SString &component, const SString &property) const
{
	status_t err;
	SValue value;
	
	err = LookupComponent(component, &value);
	if (err != B_OK) return SValue::Int32(err);
	
	return value[SValue::String(property)];
}

B_CONST_STRING_VALUE_LARGE(BSH_COMPONENT, "org.openbinder.tools.BinderShell", );
B_CONST_STRING_VALUE_SMALL(key_sh, "sh", );
B_CONST_STRING_VALUE_LARGE(key___login, "--login", );
B_CONST_STRING_VALUE_SMALL(key__s, "-s", );

status_t SContext::RunScript(const SString &filename) const
{
	sptr<ICommand> shell = ICommand::AsInterface(New(BSH_COMPONENT));
	
	shell->SetByteInput(NullByteInput());
	shell->SetByteOutput(StandardByteOutput());
	shell->SetByteError(StandardByteError());
	
	ICommand::ArgList args;
	args.AddItem(key_sh);
	args.AddItem(key___login);
	args.AddItem(key__s);
	args.AddItem(SValue::String(filename));
	
	status_t err = shell->Run(args).AsInt32(); // this returns 10 if there was an error
	return err == B_OK ? B_OK : B_ERROR;
}

B_CONST_STRING_VALUE_LARGE(key_user, "user", );
B_CONST_STRING_VALUE_LARGE(key_system, "system", );

SContext SContext::UserContext()
{
	return GetContext(key_user);
}
	
SContext SContext::SystemContext()
{
	return GetContext(key_system);
}

SContext SContext::GetContext(const SString& name)
{
	return GetContext(name, SLooper::This()->Process());
}
	
SContext SContext::GetContext(const SString& name, const sptr<IProcess>& caller)
{
	return SLooper::GetContext(name, caller->AsBinder());
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif	// LIBBE_BOOTSTRAP
