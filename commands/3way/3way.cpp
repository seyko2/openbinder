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

#include <support/Looper.h>
#include <support/Process.h>
#include <support/Context.h>
#include <support/Catalog.h>
#include <support/StdIO.h>
#include <app/ICommand.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <assert.h>
#include <SysThread.h>

bool g_crash = false;

// ===================================================
class Object : public BBinder
{
	SString m_name;

public:
	Object(const char *name)
		:m_name(name)
	{
	}
	
	virtual	status_t Transact(uint32_t code, SParcel& data, SParcel* reply, uint32_t flags)
	{
		data.SetPosition(0);
		reply->SetPosition(0);
		bout << "process=" << getpid() << " object \"" << m_name
			<< "\" received code=" << (void*)code << " data=" << data.ReadValue() << endl;
		reply->WriteValue(SValue::String(m_name));
		if (g_crash && m_name != data.ReadValue().AsString()) {
			*(int*)23 = 42;
		}
		return B_OK;
	}
};

static void
publish(const SContext &context, const char *path)
{
	printf("publish at %s returned %d\n", path, context.Publish(SString(path), SValue::Binder(new Object(path))));
}

volatile uint32_t g_transactionID = 0;

static void
call(const SContext &context, const char *path, const char *me, uint32_t proc /* high byte only */)
{
	sptr<IBinder> b = context.Lookup(SString(path)).AsBinder();
	if (b == NULL) {
		printf("%s ERROR: process=%d couldn't lookup object at \"%s\" will wait and try again\n", me, getpid(), path);
		SysThreadDelay(B_MS2NS(200), B_RELATIVE_TIMEOUT);
		return ;
	}
	uint32_t code = (SysAtomicInc32(&g_transactionID) & 0x00ffffff) | proc;
	SParcel data, reply;
	data.WriteValue(SValue::String(me));
	b->Transact(code, data, &reply, 0);
	reply.SetPosition(0);
	bout << "code=" << (void*)code << " process=" << getpid()
		<< " reply=" << reply.ReadValue() << endl;
}

static void
thrash(const SContext &context, const char *me, uint32_t proc /* high byte only */,
			nsecs_t pause = B_MS2NS(1000))
{
	while (true) {
		SysThreadDelay(pause, B_RELATIVE_TIMEOUT);
		call(context, "/one", me, proc);
		SysThreadDelay(pause, B_RELATIVE_TIMEOUT);
		call(context, "/two", me, proc);
		SysThreadDelay(pause, B_RELATIVE_TIMEOUT);
		call(context, "/three", me, proc);
	}
}

static bool validate_context(const SString& /*name*/, const sptr<IBinder>& caller, void* /*userData*/)
{
	return true;
}

// ===================================================
static void
one()
{
	printf("one\n");

	if (!SLooper::BecomeContextManager(validate_context, NULL)) {
		fprintf(stderr, "one FAILED to become the context manager, exiting...\n");
		return ;
	}

	SContext context = SContext(new BCatalog());
	SLooper::SetContextObject(context.Root()->AsBinder(), SString("user"));
	SLooper::SetContextObject(context.Root()->AsBinder(), SString("system"));

	publish(context, "/one");
	thrash(context, "/one", 0x01000000);
}

static void
two()
{
	printf("two\n");

	SContext context= get_default_context();
	bout << "two context=" << context.Root() << endl;

	publish(context, "/two");
	thrash(context, "/two", 0x02000000);
}

static void
three()
{
	printf("three\n");

	SContext context= get_default_context();
	bout << "three context=" << context.Root() << endl;

	publish(context, "/three");
	thrash(context, "/three", 0x03000000);
}

// ===================================================
int g_argc;
char * const*g_argv;

static int
usage()
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "    3way [-c]\n");
	fprintf(stderr, "       starts all three processes\n");
	fprintf(stderr, "    3way [-c] 1\n");
	fprintf(stderr, "       be process one\n");
	fprintf(stderr, "    3way [-c] 2\n");
	fprintf(stderr, "       be process two\n");
	fprintf(stderr, "    3way [-c] 3\n");
	fprintf(stderr, "       be process three\n");
	fprintf(stderr, "    -c will make it sometimes crash while in a transaction\n");
	return 1;
}

static void
run(const char *arg1)
{
	const char * args[4];
	args[0] = g_argv[0];
	if (g_crash) {
		args[1] = "-c";
		args[2] = arg1;
	} else {
		args[1] = arg1;
		args[2] = NULL;
	}
	args[3] = NULL;
	int err = execv(args[0], const_cast<char**>(args));

	printf("error with execv\n");
	exit(1);
}

static void
spawn_two_and_three()
{
	pid_t two, three;
	
	two = fork();
	if (two == 0) {
		run("2");
	}

	three = fork();
	if (three == 0) {
		run("3");
	}
	
	one();

	int result;
	two = waitpid(two, &result, 0);
	printf("two=%d result=%d\n", two, result);

	three = waitpid(three, &result, 0);
	printf("three=%d result=%d\n", three, result);
}

int
main(int argc, char * const*argv)
{
	g_argv = argv;
	g_argc = argc;

	setbuf(stdout, NULL ) ; // unbuffered I/O on stdout so we can debug before the inivitible crash

	if (argc == 1 || (argc == 2 && strcmp(argv[1], "-c") == 0)) {
		if (argc != 1) {
			g_crash = true;
		}
		spawn_two_and_three();
		return 0;
	}
	if (argc == 2 || argc == 3) {
		int i=1;
		if (argc == 3) {
			if (strcmp(argv[2], "-c") == 0) {
				i++;
				g_crash = true;
			} else {
				usage();
				return 1;
			}
		}
		if (strcmp("1", argv[i]) == 0) {
			one();
			return 0;
		}
		else if (strcmp("2", argv[i]) == 0) {
			two();
			return 0;
		}
		else if (strcmp("3", argv[i]) == 0) {
			three();
			return 0;
		}
	}

	usage();
	return 1;
}
