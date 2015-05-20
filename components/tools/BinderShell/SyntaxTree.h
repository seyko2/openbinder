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

#ifndef _SYNTAX_TREE_H
#define _SYNTAX_TREE_H

#include <support/IByteStream.h>
#include <support/Package.h>
#include <support/String.h>
#include <support/StringIO.h>
#include <support/Vector.h>

#include <app/BCommand.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::app;
#endif

class AndOr;
class BraceGroup;
class Case;
class CaseList;
class Command;
class Commands;
class CompoundList;
class For;
class Foreach;
class Function;
class If;
class IORedirect;
class List;
class Pipeline;
class Redirect;
class RedirectList;
class SimpleCommand;
class Statement;
class Subshell;
class Until;
class While;

class BShell;

// NOTE that there is no need to derive virtual from SAtom
// here, since all of this is private.  We know nobody will
// use multiple inheritance.
class SyntaxTree : public SLightAtom, public SPackageSptr
{
public:

	virtual SValue		Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit) = 0;

	virtual void		Decompile(const sptr<ITextOutput>& out) = 0;
protected:
						SyntaxTree();
	virtual				~SyntaxTree();

	//! convert the return value of evaluate to an status.
	static	status_t	AsReturnValue(const SValue& returned);
	static	SValue		Expand(const sptr<ICommand>& shell, const SString& expand, bool* expand_expansion = NULL);
};

class Command : public SyntaxTree
{
public:
	Command();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit) = 0;
	virtual void Decompile(const sptr<ITextOutput>& out) = 0;
private:
};

class Pipeline : public SyntaxTree
{
public:

	Pipeline();
	
	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void Add(const sptr<Command>& command);
	void Bang();

private:
	SVector<sptr<Command> > m_commands;
	bool m_bang;
};

class AndOr : public SyntaxTree 
{
public:

	AndOr();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void Add(const sptr<Pipeline>& pipeline);
	void Add(int separator);

private:
	SVector<sptr<Pipeline> > m_pipelines;
	SVector<int> m_separators;
};

class CompoundList : public SyntaxTree
{
public:

	CompoundList();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void Add(const sptr<AndOr>& andor);
private:
	SVector<sptr<AndOr> > m_andor;
};

class List : public SyntaxTree
{
public:

	List();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void Add(const sptr<AndOr>& andor);
private:
	SVector<sptr<AndOr> > m_andors;
};

class IORedirect : public SyntaxTree
{
public:

	IORedirect();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void SetFileDescriptor(const SString& fd);
	void SetFileName(const SString& fname);
	void SetDirection(int direction);
	void SetNext(const sptr<IORedirect>& next);
private:
	int m_direction;
	sptr<IORedirect> m_next;
	SString m_fd;
	SString m_fileName;
};

class RedirectList : public SyntaxTree
{
public:

	RedirectList();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	bool HasRedirect();
	void SetRedirect(const sptr<IORedirect>& direction);
private:
	sptr<IORedirect> m_direction;
};

class BraceGroup : public Command
{
public:

	BraceGroup(const sptr<CompoundList>& list);

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);
	
private:
	sptr<CompoundList> m_list;
};

class Commands : public Command
{
public:
	
	Commands(const sptr<List>& list);

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);
private:
	sptr<List> m_list;
};

class Function : public Command
{
public:

	Function();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void SetCommand(const sptr<Command>& command);
	void SetName(const SString& name);
	void SetRedirectList(const sptr<RedirectList>& list);

	// this function is called when the function is 'executed'.
	SValue Execute(const sptr<BShell>& shell);
	
private:
	SString m_name;
	sptr<Command> m_command;
	sptr<RedirectList> m_list;
};

class CaseList : public SyntaxTree
{
public:

	CaseList();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void AddPattern(const SString& pattern);
	SString GetPattern();
	void AddList(const sptr<CompoundList>& list);
	sptr<CompoundList> GetList();
	sptr<CaseList> GetNext();
	void SetNext(const sptr<CaseList>& next);

	void SetSemi();
	bool HasSemi();
private:
	SString m_pattern;
	sptr<CompoundList> m_list;
	sptr<CaseList> m_next;
	bool m_semi;
};

class Case : public Command
{
public:

	Case();
	
	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void SetCaseList(const sptr<CaseList>& list);
	void SetLabel(const SString& label);
private:
	sptr<CaseList> m_list;
	SString m_label;
};

class CommandPrefix : public Command
{
public:

	CommandPrefix();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	bool IsExport() const;
	bool HasAssignment() const;
	bool HasRedirect() const;

	SVector<SString> GetAssignments() const;
	sptr<IORedirect> GetIORedirect() const;

	void SetExport(bool isExport);
	void AddAssignment(const SString& string);
	void SetRedirect(const sptr<IORedirect>& direction);

private:
	bool m_export;
	sptr<IORedirect> m_direction;
	SVector<SString> m_assignments;
};

class CommandSuffix : public SyntaxTree
{
public:

	CommandSuffix();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	bool HasRedirect();
	bool HasWord();

	SVector<SString> GetWords();
	sptr<IORedirect> GetIORedirect();

	void AddWord(const SString& string);
	void SetRedirect(const sptr<IORedirect>& direction);

private:
	sptr<IORedirect> m_direction;
	SVector<SString> m_words;
};

class For : public Command
{
public:

	For();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void Add(const SString& words);

	void SetCondition(const SString& word);
	void SetDoList(const sptr<CompoundList>& list);
	
private:
	SString m_condition;
	sptr<CompoundList> m_dolist;
	SVector<SString> m_words;	
};

class Foreach : public Command
{
public:

	Foreach();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void Add(const SString& list);
	void SetKeyVariable(const SString& var);
	void SetValueVariable(const SString& var);
	void SetOverMode(bool overMode);
	void SetDoList(const sptr<CompoundList>& list);
	
private:
	SString m_keyVariable;
	SString m_valueVariable;
	bool m_overMode;
	SVector<SString> m_list;
	sptr<CompoundList> m_dolist;
};


class If : public Command
{
public:

	If();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void SetNext(const sptr<Command>& next);
	void SetCondition(const sptr<CompoundList>& list);
	void SetThen(const sptr<CompoundList>& list);

private:
	sptr<CompoundList> m_condition;
	sptr<CompoundList> m_then;
	sptr<Command> m_next;
};


class SubShell : public Command
{
public:

	SubShell(const sptr<CompoundList>& list);

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

private:
	sptr<CompoundList> m_list;
};


class Until : public Command
{
public:

	Until();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void SetCondition(const sptr<CompoundList>& list);
	void SetDoList(const sptr<CompoundList>& list);
private:

	sptr<CompoundList> m_condition;
	sptr<CompoundList> m_dolist;
};

class While : public Command
{
public:

	While();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void SetCondition(const sptr<CompoundList>& list);
	void SetDoList(const sptr<CompoundList>& list);
private:

	sptr<CompoundList> m_condition;
	sptr<CompoundList> m_dolist;
};

class SimpleCommand : public Command
{
public:

	SimpleCommand();

	virtual SValue Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit);
	virtual void Decompile(const sptr<ITextOutput>& out);

	void SetCommand(const SString& command);
	void SetPrefix(const sptr<CommandPrefix>& prefix);
	void SetSuffix(const sptr<CommandSuffix>& suffix);
private:
	SString m_command;
	sptr<CommandPrefix> m_prefix;
	sptr<CommandSuffix> m_suffix;	
};

#endif
