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

#ifndef _BINDER_COMMANDS_H
#define _BINDER_COMMANDS_H

#include <support/Package.h>
#include <support/Catalog.h>
#include <app/BCommand.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::app;
#endif

class BinderCommand : public BCommand, public SPackageSptr
{
protected:
	BinderCommand(const SContext& context);
	virtual ~BinderCommand();
};

class BHelp : public BinderCommand
{
public:
	BHelp(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BSleep : public BinderCommand
{
public:
	BSleep(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BStrError : public BinderCommand
{
public:
	BStrError(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BDb : public BinderCommand
{
public:
	BDb(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BReboot : public BinderCommand
{
public:
	BReboot(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BEcho : public BinderCommand
{
public:
	BEcho(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BEnv : public BinderCommand
{
public:
	BEnv(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BComponents : public BinderCommand
{
public:
	BComponents(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BNewProcess : public BinderCommand
{
public:
	BNewProcess(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BStopProcess : public BinderCommand
{
public:
	BStopProcess(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BContextCommand : public BinderCommand
{
public:
	BContextCommand(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BSU : public BinderCommand
{
public:
	BSU(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BNew : public BinderCommand
{
public:
	BNew(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BInspect : public BinderCommand
{
public:
	BInspect(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;

};

class BInvoke : public BinderCommand
{
public:
	BInvoke(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;

};

class BPut : public BinderCommand
{
public:
	BPut(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;

};

class BGet : public BinderCommand
{
public:
	BGet(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;

};

class BLs : public BinderCommand
{
public:
	BLs(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;

};

class BPublish : public BinderCommand
{
public:
	BPublish(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;

};

class BUnpublish : public BinderCommand
{
public:
	BUnpublish(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;

};

class BLookup : public BinderCommand
{
public:
	BLookup(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;

};

class BMkdir : public BinderCommand
{
public:
	BMkdir(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;

};

class BLink : public BinderCommand
{
public:
	BLink(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

// run a bsh script, maybe in another process
class BSpawn : public BinderCommand
{
public:
	BSpawn(const SContext& context);

	SValue Run(const ArgList& args);

};

// not a command -- used by the spawn (BSpawn) command
class BSpawnFaerie : public BBinder, public SPackageSptr
{
public:
	BSpawnFaerie(const SContext& context, const SValue &args);

	virtual	void InitAtom();

private:
	SValue m_args;
	sptr<BSpawnFaerie> m_myself;

	static void thread_func(void *ptr);
};

class BTrue : public BinderCommand
{
public:
	BTrue(const SContext& context);

	SValue Run(const ArgList& args);
};

class BFalse : public BinderCommand
{
public:
	BFalse(const SContext& context);

	SValue Run(const ArgList& args);
};

class BTime : public BinderCommand
{
public:
	BTime(const SContext& context);

	SValue Run(const ArgList& args);
};

class BPush : public BinderCommand
{
public:
	BPush(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BClear : public BinderCommand
{
public:
	BClear(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
};

class BCat : public BinderCommand
{
public:
	BCat(const SContext& context);
	
	SValue Run(const ArgList& args);
	SString Documentation() const;
};

#if 0
class BCheckNamespace : public BinderCommand
{
public:
	BCheckNamespace(const SContext& context);

	SValue Run(const ArgList& args);
	SString Documentation() const;
private:
	class Record : public SLightAtom
	{
	public:
		Record();
		
	};
	
	status_t walk_namespace_data(const SString& name, const sptr<ICatalog>& dir);
	status_t walk_namespace_datum(const SString& name, const sptr<ICatalog>& dir);

	SKeyedVector<sptr<ICatalog>, sptr<Record> > m_records;
};
#endif

#endif // _BINDER_COMMANDS_H
