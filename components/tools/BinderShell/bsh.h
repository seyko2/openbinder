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

#ifndef _BINDER_SHELL_H
#define _BINDER_SHELL_H

#include <stdio.h>

#include <support/IByteStream.h>
#include <support/Package.h>
#include <support/IInterface.h>
#include <support/Handler.h>
#include <support/StringIO.h>
#include <support/Pipe.h>

#include "Parser.h"

class SelfBinder : public BBinder
{
public:
									SelfBinder(const SContext& c) : BBinder(c) { };
	virtual							~SelfBinder() { }

	virtual	SValue					Inspect(const sptr<IBinder>& caller,
											const SValue &which,
											uint32_t flags = 0);

protected:
	virtual	status_t				Told(const SValue& what, const SValue& in);
	virtual	status_t				Asked(const SValue& what, SValue* out);
	virtual	status_t				Called(	const SValue& func,
											const SValue& args,
											SValue* out);

	virtual	SValue					GetScriptProperty(const SValue& key, bool safe=false) = 0;
	virtual	void					SetScriptProperty(const SValue& key, const SValue& value) = 0;
	virtual	SValue					ExecScriptFunction(const SValue& func, const SValue& args, bool* outFound) = 0;
};

class BShell : public BCommand, public SelfBinder, public SPackageSptr
{
public:
	BShell(const SContext& context);

	virtual	SValue Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags = 0)
	{
		SValue result = SelfBinder::Inspect(caller, which, flags);
		if (caller == (BCommand*)this) result.Join(BCommand::Inspect(caller, which, flags));
		return result;
	}

	virtual SValue Run(const ArgList& args);
	
	// We intercept these to make sure some basic shell environment
	// variables are set up, and overwrite anything we may get from
	// the parent environment.
	virtual	void SetProperty(const SValue& key, const SValue& value);
	virtual	SValue GetProperty(const SValue& key) const;
	virtual	void SetEnvironment(const SValue& env);

	void SetVMArguments(const SValue& args);

	void GetArguments(SVector<SValue>* outArgs) const;
	void SetArguments(const SVector<SValue>& args);
	
	void SetLastResult(const SValue& val);
	
	SString GetPS1();
	SString GetPS2();

	void RequestExit();
	bool ExitRequested() const;
	
	inline SContext			Context() const { return BCommand::Context(); }

protected:
	virtual ~BShell();

	virtual	void InitAtom();

	virtual	SValue					GetScriptProperty(const SValue& key, bool safe=false);
	virtual	void					SetScriptProperty(const SValue& key, const SValue& value);
	virtual	SValue					ExecScriptFunction(const SValue& func, const SValue& args, bool* outFound);

			void					SetEnvironmentFromHost();
			
private:
	mutable SLocker		m_lock;
	SVector<SValue>		m_args;
	SValue				m_lastResult;
	bool				m_hasEnvironment;
	bool				m_exitRequested;
};

#endif // _BINDER_SHELL_H
