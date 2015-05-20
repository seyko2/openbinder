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
#include <support/Package.h>
#include <support/InstantiateComponent.h>
#include <support/IProcess.h>
#include <support/SortedVector.h>

#include <support/StdIO.h>

class AtomCommand : public BCommand, public SPackageSptr
{
public:
	AtomCommand(const SContext& context);
	
	virtual SValue Run(const ArgList& args);
	virtual SString Documentation() const;
};


AtomCommand::AtomCommand(const SContext& context)
	: BCommand(context)
{
}

SValue AtomCommand::Run(const ArgList& args)
{
	sptr<ITextOutput> out = TextOutput();
	
	SValue value;
	SString one;
	wptr<SAtom> atom;
	sptr<IProcess> team;
	int32_t mark;
	int32_t last;
	int32_t flags;
	status_t error;

	const SValue arg(args.CountItems() > 1 ? args[1] : B_UNDEFINED_VALUE);
	const SValue arg2(args.CountItems() > 2 ? args[2] : B_UNDEFINED_VALUE);
	const SValue arg3(args.CountItems() > 3 ? args[3] : B_UNDEFINED_VALUE);
	const SValue arg4(args.CountItems() > 4 ? args[4] : B_UNDEFINED_VALUE);
	const SValue arg5(args.CountItems() > 5 ? args[5] : B_UNDEFINED_VALUE);
	one = arg.AsString();

	if (one == "report")
	{
		atom = arg2.AsWeakBinder().unsafe_ptr_access();
		if (atom == NULL) atom = arg2.AsWeakAtom();
		if (atom == NULL) atom = ArgToBinder(arg2).ptr();
		if (atom == NULL) {
			SAtom* raw = (SAtom*)(arg2.AsInt32());
			if (SAtom::ExistsAndIncWeak(raw)) {
				atom = raw;
				raw->DecWeak(raw);
			}
		}
		if (atom != NULL) {
			atom.unsafe_ptr_access()->Report(out);
		} else {
			SLightAtom* raw = (SLightAtom*)(arg2.AsInt32());
			if (SLightAtom::ExistsAndIncStrong(raw)) {
				raw->Report(out);
				raw->DecStrong(raw);
			} else {
				if (arg2.IsDefined())
					TextError() << "atom: " << arg2 << " is not an atom" << endl;
				else
					TextError() << "atom: no object supplied" << endl;
				goto ERROR;
			}
		}
		
	}
	else if (one == "mark")
	{
		team = IProcess::AsInterface(ArgToBinder(arg2));
		if (team == NULL) {
			TextError() << "atom: no process supplied" << endl;
			goto ERROR;
		}
		
		mark = team->AtomMarkLeakReport();
		if (mark < 0) {
			TextError() << "atom: reference tracking not enabled!" << endl;
		}

		return SValue::Int32(mark);
	}
	else if (one == "leaks")
	{
		team = IProcess::AsInterface(ArgToBinder(arg2));
		if (team == NULL) {
			TextError() << "atom: no process supplied" << endl;
			goto ERROR;
		}
		
		mark = arg3.AsInt32(&error);
		if (error != B_OK) mark = 0;
		last = arg4.AsInt32(&error);
		if (error != B_OK) last = -1;
		flags = arg5.AsInt32(&error);
		if (error != B_OK) flags = 0;

		team->AtomLeakReport(mark, last, flags);
	}
	else if (one == "find")
	{
		one = arg2.AsString(&error);
		if (error != B_OK) {
			TextError() << "atom: no type name supplied for find" << endl;
			goto ERROR;
		}

		SVector<wptr<SAtom> > atoms;
		SVector<sptr<SLightAtom> > lightAtoms;
		SAtom::GetAllWithTypeName(one.String(), &atoms, &lightAtoms);

		SValue result;

		size_t N = atoms.CountItems();
		size_t i;
		for (i=0; i<N; i++) {
			result.JoinItem(SValue::Int32(i), SValue::WeakAtom(atoms[i]));
		}

		N = lightAtoms.CountItems();
		for (i=0; i<N; i++) {
			result.JoinItem(SValue::Int32(i), SValue::Int32((int32_t)lightAtoms[i].ptr()));
		}

		return result;
	}
	else if (one == "typenames")
	{
		SSortedVector<SString> typenames;
		SAtom::GetActiveTypeNames(&typenames);

		const size_t N = typenames.CountItems();
		SValue result;
		for (size_t i=0; i<N; i++) {
			result.Join(SValue::String(typenames[i]));
		}

		return result;
	}
	else
	{
ERROR:
		TextError() << Documentation() << endl;
		return SValue::Status(B_BAD_VALUE);
	}
	
	return SValue::Int32(B_OK);
}

SString AtomCommand::Documentation() const
{
	return SString(
		"usage:\n"
		"   atom report OBJECT\n"
		"       Show references on atom OBJECT\n"
		"\n"
		"   atom mark PROCESS\n"
		"       Create new bin for future atoms\n"
		"       that are created in PROCESS\n"
		"\n"
		"   atom typenames\n"
		"       List all existing atoms by type\n"
		"\n"
		"   atom find TYPE_NAME\n"
		"       Return set of atoms that are the\n"
		"       C++ class TYPE_NAME\n"
		"\n"
		"   atom leaks PROCESS [MARK] [LAST] [FLAGS]\n"
		"       List all atoms that were created in\n"
		"       bins MARK to LAST and still exist\n"
		"\n"
		"Unless otherwise specified, the atom command\n"
		"operates on the current process.  You must enable\n"
		"the atom leak tracker for this command to\n"
		"work."
	);
}

sptr<IBinder> InstantiateComponent(const SString& component, const SContext& context, const SValue &/*args*/)
{
	if (component == "")
	{
		return new AtomCommand(context);
	}
	
	return NULL;
}
