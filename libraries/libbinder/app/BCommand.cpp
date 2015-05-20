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

#include <app/BCommand.h>

#include <support/Autolock.h>
#include <support/IProcess.h>
#include <support/Package.h>
#include <support/StdIO.h>
#include <support/TextStream.h>

#include <stdarg.h>
#include <errno.h>

#if TARGET_HOST != TARGET_HOST_LINUX
// libc internal headers
#include <wcio.h>
#include <fileext.h>
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace app {
using namespace palmos::support;
#endif /* _SUPPORTS_NAMESPACE */

B_STATIC_STRING_VALUE_SMALL(kPWD,		"PWD", );
B_STATIC_STRING_VALUE_LARGE(kECHO,		"ECHO", );
B_STATIC_STRING_VALUE_LARGE(kTRACE,		"TRACE", );
B_STATIC_STRING_VALUE_LARGE(kBSH_COLOR,	"BSH_COLOR", );

#define O __LINE__

// ---------------------------------------------------------------------------

// Using SPackageSptr here seems to break the ARM build.
// I don't know why, but we can live without it, since this is
// in libbinder which won't be unloaded until the whole Binder
// infrastructure goes away.
class CreditsCommand : public BCommand //, public SPackageSptr
{
public:
	CreditsCommand(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

CreditsCommand::CreditsCommand(const SContext& context)
	:	BCommand(context)
{
}

SString CreditsCommand::Documentation() const {
	return SString(
		"usage: credits\n"
		"\n"
		"They did it all for you."
	);
}

#define ANSI_NORMAL			"\033[00m"
#define ANSI_BLACK			"\033[0;30m"
#define ANSI_BLUE			"\033[0;34m"
#define ANSI_GREEN			"\033[0;32m"
#define ANSI_CYAN			"\033[0;36m"
#define ANSI_RED			"\033[0;31m"
#define ANSI_PURPLE			"\033[0;35m"
#define ANSI_BROWN			"\033[0;33m"
#define ANSI_GRAY			"\033[0;37m"
#define ANSI_DARK_GRAY		"\033[1;30m"
#define ANSI_LIGHT_BLUE		"\033[1;34m"
#define ANSI_LIGHT_GREEN	"\033[1;32m"
#define ANSI_LIGHT_CYAN		"\033[1;36m"
#define ANSI_LIGHT_RED		"\033[1;31m"
#define ANSI_LIGHT_PURPLE	"\033[1;35m"
#define ANSI_YELLOW			"\033[1;33m"
#define ANSI_WHITE			"\033[1;37m"
#define ARVE_A				"\xc3\xa5"
#define ARVE_0				"\xc3\xb8"

//#define BLOCK_CHAR			"\xE2\x96\xAC"
//#define FRENCH_FLAG			ANSI_BLUE BLOCK_CHAR ANSI_WHITE BLOCK_CHAR ANSI_RED BLOCK_CHAR
#define FRENCH_FLAG

static void do_options(const sptr<ITextOutput>& out, const SValue& opt);

SValue CreditsCommand::Run(const ArgList& args)
{
	const sptr<ITextOutput> out(TextOutput());

	SValue arg1;
	if (args.CountItems() >= 2) arg1 = args[1];
	if (!arg1.IsDefined()) {
		out <<
				ANSI_PURPLE "PalmOS Cobalt was produced by..." ANSI_NORMAL "\n"
				"\n"
				"Angela Sticher, Brian Maas, Charlie Tritschler,\n"
				"Cyril Meurillon, David Berbessou, David Fedor,\n"
				"David Schlesinger, Doug MacMillan, Edwin Sluis,\n"
				"Eileen Broch, Fabrice Frachon, Fatima Valipour,\n"
				"George Hoffman, Jacques Bourhis, Jor Bratko,\n"
				"Larry Slotnick, Lisa Rathjens, Marion Roche,\n"
				"Mariseth Ferring, Mike Kelley, Phil Shoemaker,\n"
				"Raymond Losey, Regis Nicolas, Rene Pourtier,\n"
				"Ronald Tessier, Scott Fisher, Steven Glass,\n"
				"Steven Olson.\n"
				"\n"
				ANSI_PURPLE "Each line of code carefully crafted by..." ANSI_NORMAL "\n"
				"\n"
				ANSI_LIGHT_GREEN "Engineering" ANSI_NORMAL ", Larry Slotnick and\n"
				"Pierre Raynaud-Richard.\n"
				"\n"
				"Michael Kocheisen, Richard Levenberg,\n"
				"Gavin Peacock, Regis Nicolas.\n"
				"\n"
				ANSI_CYAN "Core Technology" ANSI_NORMAL ", Cyril Meurillon:\n"
				"\n"
				"Arve Hj" ARVE_0 "nnev" ARVE_A "g, Ricardo Lagos,\n"
				"Manuel Petit de Gabriel, Mike Chen, Justin Morey,\n"
				"Jeff Hamilton, Adam Hampton, Joe Kloss,\n"
				"Raj Mojumder, Jie Su, Christopher Tate,\n"
				"David Schlesinger, Bertand Aygon, Alain Basty,\n"
				"Denis Berger, Yann Cheri, Frederic Danis,\n"
				"Olivier Guiter, Tom Keel, Eric Lapuyade,\n"
				"Renaud Malaval, David Navarro, Hatem Oueslati,\n"
				"Ronald Tessier, Rene Pourtier, Jacques Bourhis.\n"
				"\n"
				ANSI_CYAN "Applications & Services" ANSI_NORMAL ", George Hoffman:\n"
				"\n"
				ANSI_NORMAL "Mathias Agopian, Trey Boudreau,\n"
				"Joe Onorato, Jason Parks, Jenny Tsai,\n"
				"Dianne Hackborn, Najeeb Abdulrahiman,\n"
				"Chris Bark, Grant Glouser, Vivek Magotra,\n"
				"Laz Marhenke, Chris Schneider, Andy Stewart,\n"
				"Tim Wiegman, Jesse Donaldson, Thierry Escande,\n"
				"Ludovic Ferrandis, Christophe Guiraud,\n"
				"Patrick Lechat, Paul Plaquette, Patrick Porlan,\n"
				"Luc Yriarte, Gilles Fabre, Gabe Dalbec,\n"
				"Mark Lehmoine, Erik Perotti, Chris Sanders,\n"
				"Dan Sandler, Winston Wang, Jeff Parrish,\n"
				"Ken Krugler.\n"
				"\n"
				ANSI_CYAN "Advanced Services" ANSI_NORMAL ", Steven Olson:\n"
				"\n"
				"Eric Moon, Richard Williams, Marco Nelissen,\n"
				"Tom Butler, Uma Gouru,\n"
				"Atul Gupta, William Mills, Jeff Shulman,\n"
				"Paul Wong, Gerard Pallipuram, Siyuan Bei,\n"
				"Christian Robertson, Emily Trinh, Jayita Poddar,\n"
				"Robert McKenzie.\n"
				"\n"
				ANSI_CYAN "Tools" ANSI_NORMAL ", Phil Shoemaker:\n"
				"\n"
				"Eric Cloninger, Brad Jarvinen, Steve Lemke,\n"
				"Keith Rollin, Greg Clayton, Matt Fassiotto,\n"
				"John Dance, Kevin MacDonell,\n"
				"Kenneth Albanowski, Jeff Westerinen,\n"
				"Eric Omelian, Ravikumar Duggaraju,\n"
				"Cole Goeppinger, Raymond Pierce,\n"
				"Mahmood Sheikh.\n"
				;
	}
	else {
		do_options(out, arg1);
	}

	return SValue::Status(B_OK);
}


// ---------------------------------------------------------------------------

BCommand::BCommand()
	:	m_lock("BCommand"),
		m_lastGet(0),
		m_in(StandardByteInput()),
		m_out(StandardByteOutput()),
		m_err(StandardByteError()),
		m_propSeq(0),
		m_echoSeq(-1), m_traceSeq(-1), m_colorSeq(-1)
{
	m_stdStreams[0] = NULL;
	m_stdStreams[1] = NULL;
	m_stdStreams[2] = NULL;
}


BCommand::BCommand(const SContext& context)
	:	BnCommand(context),
		m_lock("BCommand"),
		m_lastGet(0),
		m_in(StandardByteInput()),
		m_out(StandardByteOutput()),
		m_err(StandardByteError()),
		m_propSeq(0),
		m_echoSeq(-1), m_traceSeq(-1), m_colorSeq(-1)
{
	m_stdStreams[0] = NULL;
	m_stdStreams[1] = NULL;
	m_stdStreams[2] = NULL;
}

BCommand::~BCommand()
{
#if TARGET_HOST != TARGET_HOST_LINUX
	// destroy standard streams
	for (int i = 0; i < 3; i++) {
		if (m_stdStreams[i] != NULL) {
			fclose(m_stdStreams[i]);
			free(m_stdStreams[i]->_ext._base);
			free(m_stdStreams[i]);
		}
	}
#endif
}

sptr<ICommand> BCommand::Clone() const
{
	return NULL;
}

void BCommand::AddCommand(const SString& name, const sptr<ICommand>& command) 
{
	SAutolock _l(m_lock.Lock());
	m_functions.AddItem(name, command);
}

sptr<ICommand> BCommand::GetCommand(const SString& name) const
{
	SAutolock _l(m_lock.Lock());
	return m_functions.ValueFor(name);
}

void BCommand::RemoveCommand(const SString& name)
{
	SAutolock _l(m_lock.Lock());
	m_functions.RemoveItemFor(name);
}

SString BCommand::Documentation() const
{
	return B_EMPTY_STRING;
}

sptr<ICommand> BCommand::Spawn(const SString& name) const
{
	return SpawnCustom(name, SContext(NULL), NULL);
}

sptr<ICommand> BCommand::SpawnCustom(const SString & name, const SContext& _context, const sptr<IProcess>& team) const
{
//	bout << "trying to spawn command " << name;
	
	m_lock.Lock();
	sptr<ICommand> command = m_functions.ValueFor(name);
	m_lock.Unlock();

	SContext context = _context;
	if (context.Root() == NULL) context = Context();

	if (command == NULL)
	{
		if (name == "credits") {
			command = new CreditsCommand(context);
		} else {
			SString path("/packages/bin/");
			path.Append(name);
//			bout << " path = " << path << endl;
			SValue component = context.Lookup(path);
		
			if (component.IsDefined())
			{
//				bout << "spawning command " << name << " interface " << component << endl;
				if (team == NULL)
					command = ICommand::AsInterface(context.New(component));
				else
					command = ICommand::AsInterface(context.RemoteNew(component, team));
			}
		}

		if (command == NULL)
			return NULL;
		m_lock.Lock();
		SValue env = m_env;
		SValue fds = m_fds;
		m_lock.Unlock();
		command->SetEnvironment(env);
		command->SetFileDescriptors(fds);
		command->SetByteInput(ByteInput());
		command->SetByteOutput(ByteOutput());
		command->SetByteError(ByteError());
	}
	
	return command;
}

SValue BCommand::GetProperty(const SValue& key) const
{
	SAutolock _l(m_lock.Lock());
//	bout << "BCommand::GetProperty " << key << endl;
	return m_env[key];
}

void BCommand::SetProperty(const SValue& key, const SValue& value)
{
	SAutolock _l(m_lock.Lock());
	m_env.Overlay(SValue(key, value), B_NO_VALUE_RECURSION);
	m_propSeq++;
//	bout << "BCommand::setProperty " << m_env << endl;
}

void BCommand::RemoveProperty(const SValue& key)
{
	SAutolock _l(m_lock.Lock());
	m_env.Remove(SValue(key, SValue::Wild(), B_NO_VALUE_FLATTEN), B_NO_VALUE_RECURSION);
	m_propSeq++;
}

void BCommand::SetEnvironment(const SValue& env)
{
	SAutolock _l(m_lock.Lock());
	m_env = env;
	m_propSeq++;
}

SValue BCommand::Environment() const
{
	SAutolock _l(m_lock.Lock());
	return m_env;
}

void BCommand::SetFileDescriptors(const SValue& fds)
{
	SAutolock _l(m_lock.Lock());
	m_fds = fds;
}

const char* BCommand::getenv(const char* key) const
{
	SAutolock _l(m_lock.Lock());

	status_t err;
	const SString result = m_env[SValue::String(key)].AsString(&err);
	if (err != B_OK) return NULL;

	m_lastGet = (m_lastGet+1)%NUM_GETS;
	m_gets[m_lastGet] = result;
	return m_gets[m_lastGet].String();
}

int BCommand::putenv(const char* string)
{
	SAutolock _l(m_lock.Lock());

	SString key(string);
	SString value = key;

	int equal = key.FindFirst("=");
	key.Remove(equal, key.Length());
	value.Remove(0, equal + 1);

	m_env.Overlay(SValue(SValue::String(key), SValue::String(value)), B_NO_VALUE_RECURSION);
	m_propSeq++;

	return B_OK;
}

int BCommand::setenv(const char *name, const char *value, int rewrite)
{
	SAutolock _l(m_lock.Lock());

	if (rewrite)
		m_env.Overlay(SValue(SValue::String(name), SValue::String(value)), B_NO_VALUE_RECURSION);
	else
		m_env.Inherit(SValue(SValue::String(name), SValue::String(value)), B_NO_VALUE_RECURSION);
	m_propSeq++;

	return B_OK;
}

void BCommand::unsetenv(const char *name)
{
	SAutolock _l(m_lock.Lock());
	m_env.RemoveItem(SValue::String(name));
	m_propSeq++;
}

FILE *BCommand::__sF(int which) const
{
#if TARGET_HOST != TARGET_HOST_LINUX
	FILE *fp;

	SAutolock _l(m_lock.Lock());

	if (m_stdStreams[which] == NULL) {
		// this is the first time this FILE* has been accessed -
		// create it
		fp = (FILE *)malloc(sizeof(FILE));
		memset(fp, 0, sizeof(FILE));
		fp->_file = which;
		fp->_close = &BCommand::FILEClose;
		fp->_read = &BCommand::FILERead;
		fp->_write = &BCommand::FILEWrite;

		// note that the FILE is unbuffered - we'll leave all
		// the buffering to the ITextInput/ITextOutput, so as
		// to ensure "cache coherency" between it and the FILE
		fp->_flags = ((which == 0 ? __SRD : __SWR) | __SNBF);

		fp->_ext._base = (unsigned char *)malloc(sizeof(struct __sfileext));
		memset(fp->_ext._base, 0, sizeof(struct __sfileext));

		// we'll store both our 'this' pointer, and 'which' in the
		// FILEs 'cookie'
		ErrFatalErrorIf(((uint32_t)this) & 0x3, "BCommand::__sF - FILE unaligned");
		fp->_cookie = (void *)(((uint32_t)this) | which);

		m_stdStreams[which] = fp;
	}
#else
#endif
	return m_stdStreams[which];
}

int BCommand::system(const char *cmd)
{
	sptr<ICommand> shell = Spawn(SString("sh"));

	ArgList args;
	args.AddItem(SValue::String("sh"));
	args.AddItem(SValue::String("-c"));
	args.AddItem(SValue::String(cmd));

	SValue status = shell->Run(args);
	status_t err = status.AsStatus();
	if (err != B_OK)
		return err;
	return status.AsInt32();
}

SString BCommand::ArgToPath(const SValue& arg) const
{
	status_t err;
	SString path(CurDir());
	path.PathAppend(arg.AsString(&err), true);
	return err == B_OK ? path : SString();
}

sptr<IBinder> BCommand::ArgToBinder(const SValue& arg) const
{
	sptr<IBinder> binder = arg.AsBinder();
	
	if (binder == NULL)
	{
		SString path(ArgToPath(arg));
		if (path != "") {
			SNode node(Context().Root());
			binder = node.Walk(path, (uint32_t)0).AsBinder();
		}
	}

	return binder;
}

void BCommand::SetByteInput(const sptr<IByteInput>& input)
{
	SAutolock _l(m_lock.Lock());
	m_in = input;
	m_textIn = NULL;
}

void BCommand::SetByteOutput(const sptr<IByteOutput>& output)
{
	SAutolock _l(m_lock.Lock());
	m_out = output;
	m_textOut = NULL;
}
	
void BCommand::SetByteError(const sptr<IByteOutput>& error)
{
	SAutolock _l(m_lock.Lock());
	m_err = error;
	m_textErr = NULL;
}

sptr<IByteInput> BCommand::ByteInput() const
{
	SAutolock _l(m_lock.Lock());
	return m_in;
}

sptr<IByteOutput> BCommand::ByteOutput() const
{
	SAutolock _l(m_lock.Lock());
	return m_out;
}

sptr<IByteOutput> BCommand::ByteError() const
{
	SAutolock _l(m_lock.Lock());
	return m_err;
}

sptr<ITextInput> BCommand::TextInput()
{
	if (m_textIn == NULL) {
		SAutolock _l(m_lock.Lock());
		if (m_textIn == NULL && m_in != NULL) {
			m_textIn = new BTextInput(m_in);
		}
		if (m_textIn == NULL) {
			// Last resort.
			//m_textIn = bin;
		}
	}

	return m_textIn;
}

sptr<ITextOutput> BCommand::TextOutput()
{
	if (m_textOut == NULL) {
		SAutolock _l(m_lock.Lock());
		if (m_textOut == NULL && m_out != NULL) {
			m_textOut = new BTextOutput(m_out);
		}
		if (m_textOut == NULL) {
			// Last resort.
			m_textOut = bout;
		}
	}

	return m_textOut;
}

sptr<ITextOutput> BCommand::TextError()
{
	if (m_textErr == NULL) {
		SAutolock _l(m_lock.Lock());
		if (m_textErr == NULL && m_err != NULL) {
			m_textErr = new BTextOutput(m_err);
		}
		if (m_textErr == NULL) {
			// Last resort.
			m_textErr = berr;
		}
	}

	return m_textErr;
}

int32_t BCommand::CurrentPropertySequence() const
{
	return m_propSeq;
}

bool BCommand::GetEchoFlag() const
{
	// XXX not thread safe
	if (m_echoSeq != m_propSeq) {
		m_echoSeq = m_propSeq;
		m_echoFlag = const_cast<BCommand*>(this)->GetProperty(kECHO).AsBool();
	}
	return m_echoFlag;
}

bool BCommand::GetTraceFlag() const
{
	// XXX not thread safe
	if (m_traceSeq != m_propSeq) {
		m_traceSeq = m_propSeq;
		m_traceFlag = const_cast<BCommand*>(this)->GetProperty(kTRACE).AsBool();
	}
	return m_traceFlag;
}

bool BCommand::GetColorFlag() const
{
	// XXX not thread safe
	if (m_colorSeq != m_propSeq) {
		m_colorSeq = m_propSeq;
		m_colorFlag = const_cast<BCommand*>(this)->GetProperty(kBSH_COLOR).AsBool();
	}
	return m_colorFlag;
}

SString BCommand::CurDir() const
{
	SString path(GetProperty(kPWD).AsString());
	return (path != "")  ? path : SString("/");
}

int BCommand::FILERead(void *cookie, char *buf, int n)
{
	// only stdin can be read
	uint32_t which = (((uint32_t)cookie) & 0x3);
	if (which != 0) {
		errno = EBADF;
		return -1;
	}

	BCommand* This = reinterpret_cast<BCommand*>(((uint32_t)cookie) & ~0x3);

	// because our 'stdin' FILE is unbuffered, 'n' will always
	// be one - just read a character
	ssize_t c = This->TextInput()->ReadChar();
	if (c == EOF)
		return 0;
	else if (c < 0) {
		errno = c;
		return -1;
	}

	buf[0] = (char)c;
	return 1;
}

int BCommand::FILEWrite(void *cookie, const char *buf, int n)
{
	// only stdout/stderr can be written
	uint32_t which = (((uint32_t)cookie) & 0x3);
	if (which == 0) {
		errno = EBADF;
		return -1;
	}

	BCommand* This = reinterpret_cast<BCommand*>(((uint32_t)cookie) & ~0x3);
	sptr<IByteOutput> out = (which == 1 ? This->ByteOutput() : This->ByteError());

	ssize_t res = out->Write(buf, n, 0);
	if (res < 0) {
		errno = res;
		return -1;
	} else
		return res;
}

int BCommand::FILEClose(void * /*cookie*/)
{
	// the standard streams can't be closed
	return -1;
}

// -----------------------------------------------------------------------

BUnixCommand::BUnixCommand(const SContext& context)
	:	BCommand(context)
{
}

SValue BUnixCommand::Run(const ArgList& args)
{
//	bout << "BUnixCommand::Run: " << args << endl; 
	size_t index;
	const size_t count = args.CountItems();
	
	char** argv = (char**)malloc(sizeof(char*) * (count+1));

	for (index=0; index<count; index++)
	{
		status_t err = B_OK;
		SValue value = args[index];
		SString string = value.AsString(&err);
	
		if (err == B_OK)
		{
			argv[index] = (char*)malloc(string.Length()+1);
			strncpy(argv[index], string.String(), string.Length()+1);
		}
		else
		{
			switch (value.Type())
			{
				case B_VALUE_TYPE:
				case B_BINDER_TYPE:
				case B_BINDER_HANDLE_TYPE:
				case B_BINDER_WEAK_TYPE:
				case B_BINDER_WEAK_HANDLE_TYPE:
				{
					sptr<IBinder> binder = value.AsBinder();
					argv[index] = (char*)malloc(32);
					snprintf(argv[index], 32, "sptr<IBinder>(0x%lx)", (unsigned long)binder.ptr());
					break;
				}

				default:
				{
					argv[index] = (char*)malloc(sizeof("UNKNOWN VALUE"));
					strncpy(argv[index], "UNKNOWN VALUE", sizeof("UNKNOWN VALUE")); 
					break;
				}
			}
		}
	}

	// null terminate the array. why? because this is what they do in C.
	argv[count] = NULL;

	// copy the argv table as the main may modify it (i.e. keep a copy of the pointers to the args)
	char** argv_copy = (char**)malloc(sizeof(char*) * (count+1));
	memcpy(argv_copy, argv, sizeof(char*) * (count+1));
	
	SValue result = main(count, argv_copy);
	
	for (index = 0 ; index < count-1 ; index++)
	{
		free(argv[index]);
	}

	free(argv);
	free(argv_copy);

//	bout << "result from test_main was " << result << endl;;
	return result;
}

BUnixCommand::~BUnixCommand()
{
}

static void do_options(const sptr<ITextOutput>& out, const SValue& opt)
{
#if O
	if (opt == "--key") {
		out <<	"David Nagel, David Limp, Lamar Potts,\n"
				"Dory Yochum, Gabriele Schindler, Eric " ANSI_BLUE "B" ANSI_RED "e" ANSI_NORMAL "nhamou,\n"
				"Larry Slotnick, Albert Wood.\n";
	}
	else if (opt == "-B") {
		out <<	"Pierre Raynaud-Richard, Cyril Meurillon,\n"
				"Arve Hj" ARVE_0 "nnev" ARVE_A "g, Manuel Petit de Gabriel,\n"
				"Jeff Hamilton, Joe Kloss, Christopher Tate,\n"
				"George Hoffman, Mathias Agopian, Trey Boudreau,\n"
				"J" ARVE_0 "e On" ARVE_0 "r" ARVE_A "t" ARVE_0 ", Jason Parks, Jenny Tsai,\n"
				"Dianne Hackborn, Laz Marhenke, Chris Sanders,\n"
				"Dan Sandler, Steven Olson, Eric Moon,\n"
				"Justin Morey, Marco Nelissen, John Dance,\n"
				"Miriam Ayala, Fabrice Frachon, Myron Walker,\n"
				"Brian Dawbin.\n";
	}
#endif
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::app
#endif /* _SUPPORTS_NAMESPACE */
