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

#ifndef APP_BCOMMAND_H
#define APP_BCOMMAND_H

#include <support/Binder.h>
#include <support/Catalog.h>
#include <support/IByteStream.h>
#include <support/IInterface.h>
#include <support/ITextStream.h>
#include <support/KeyedVector.h>
#include <support/Locker.h>
#include <support/String.h>
#include <support/Value.h>
#include <support/Vector.h>

#include <app/ICommand.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace app {
using namespace palmos::support;
#endif

class BCommand : public BnCommand
{
public:
							BCommand();
							BCommand(const SContext& context);
	
	virtual	SValue			Run(const ArgList& args) = 0;
	virtual	sptr<ICommand>	Clone() const;
	virtual	sptr<ICommand>	Spawn(const SString & command) const;

	virtual void			AddCommand(const SString& name, const sptr<ICommand>& command);
	virtual sptr<ICommand>	GetCommand(const SString& name) const;
	virtual void			RemoveCommand(const SString& name);

	virtual	SValue			GetProperty(const SValue& key) const;	
	virtual	void			SetProperty(const SValue& key, const SValue& value);
	virtual	void			RemoveProperty(const SValue& key);

	virtual	SValue			Environment() const;
	
	virtual	void			SetEnvironment(const SValue& env);
	virtual	void			SetFileDescriptors(const SValue& fds);

	virtual	void			SetByteInput(const sptr<IByteInput>& input);
	virtual	void			SetByteOutput(const sptr<IByteOutput>& output);
	virtual	void			SetByteError(const sptr<IByteOutput>& error);

	virtual	sptr<IByteInput>		ByteInput() const;
	virtual	sptr<IByteOutput>		ByteOutput() const;
	virtual	sptr<IByteOutput>		ByteError() const;

			sptr<ITextInput>		TextInput();
			sptr<ITextOutput>		TextOutput();
			sptr<ITextOutput>		TextError();

			sptr<ICommand>	SpawnCustom(const SString & command,
										const SContext& context,
										const sptr<IProcess>& team) const;

			//!	Given an argument value, convert it to an absolute path.
			/*!	If the SValue contains a string, it is appended to the
				PWD and returned.  Otherwise, "" is returned. */
			SString			ArgToPath(const SValue& arg) const;

			//!	Given an argument value, convert it to an object.
			/*!	If the SValue contains an object, the binder is returned
				directly.  Otherwise, if it is a string, this is taken
				as a path and looked up in the context. */
			sptr<IBinder>	ArgToBinder(const SValue& arg) const;
			
			// Returns a sequence number that is incremented each time
			// the command's environment properties change.
			int32_t			CurrentPropertySequence() const;

			// Conveniences for checking some standard environment variables.
			bool			GetEchoFlag() const;		// ECHO
			bool			GetTraceFlag() const;		// TRACE
			bool			GetColorFlag() const;		// BSH_COLOR

			SString			CurDir() const;				// PWD
			
			FILE *			__sF(int which) const;
			const char*		getenv(const char* key) const;
			int				putenv(const char* string);
			int				setenv(const char *name, const char *value, int rewrite);
			void			unsetenv(const char *name);
			int				system(const char *cmd);

			//! Retrieve a short description of the command.
			/*! (Here's one we stole from Python.)  Every command may
				optionally have a documentation property.  This can be
				retrieved at runtime to provide detailed help on any
				command.  Shell users everywhere rejoice! */
	virtual SString 		Documentation() const;

protected:
	virtual					~BCommand();

private:

			//!	Methods to bridge between FILE* and ITextInput/ITextOutput
	static	int				FILERead(void *cookie, char *buf, int n);
	static	int				FILEWrite(void *cookie, const char *buf, int n);
	static	int				FILEClose(void *cookie);

	mutable	SLocker				m_lock;

			enum				{ NUM_GETS = 4 };
	mutable	int32_t				m_lastGet;
	mutable	SString				m_gets[NUM_GETS];

			sptr<IByteInput>	m_in;
			sptr<IByteOutput>	m_out;
			sptr<IByteOutput>	m_err;

	mutable	sptr<ITextInput>	m_textIn;
	mutable	sptr<ITextOutput>	m_textOut;
	mutable	sptr<ITextOutput>	m_textErr;

	mutable	FILE*				m_stdStreams[3];

			SValue				m_env;
			SValue				m_fds;

			int32_t				m_propSeq;

	mutable	int32_t				m_echoSeq;
	mutable	int32_t				m_traceSeq;
	mutable	int32_t				m_colorSeq;

	mutable	bool				m_echoFlag : 1;
	mutable	bool				m_traceFlag : 1;
	mutable	bool				m_colorFlag : 1;

	SKeyedVector<SString, sptr<ICommand> > m_functions;
};

class BUnixCommand : public BCommand
{
public:
							BUnixCommand(const SContext& context);

	virtual	SValue			Run(const ArgList& args);

protected:
	virtual					~BUnixCommand();

	virtual	SValue			main(int argc, char** argv) = 0;

private:

	// copy constructor not supported
	BUnixCommand( const BUnixCommand& rhs );

	// operator= not supported
	BUnixCommand& operator=( const BUnixCommand& rhs );
};

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::app
#endif

#endif // APP_BCOMMAND_H
