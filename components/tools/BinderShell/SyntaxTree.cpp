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
#include <support/StdIO.h>
#include <support/TextStream.h>
#include <support/Catalog.h>
#include <support/Iterator.h>

#include <app/BCommand.h>
#include <support/Package.h>

#include <ctype.h>

#include "Parser.h"
#include "SyntaxTree.h"
#include "bsh.h"

B_STATIC_STRING_VALUE_LARGE(kBSH_SCRIPT_FILE, "BSH_SCRIPT_FILE", );
B_STATIC_STRING_VALUE_LARGE(kBSH_SCRIPT_DIR, "BSH_SCRIPT_DIR", );

class FunctionCommand : public BCommand, public SPackageSptr
{
public:
	FunctionCommand(const sptr<Function>& function, const sptr<BShell>& shell)
		:	m_function(function),
			m_shell(shell)
	{	
	}

	class ArgumentHandler
	{
	public:
		void ApplyArgs(const sptr<BShell>& shell, const ICommand::ArgList& args, bool useArg0)
		{
			shell->GetArguments(&m_oldArgs);
			if (useArg0) shell->SetArguments(args);
			else {
				SVector<SValue> newArgs(args);
				if (m_oldArgs.CountItems() > 0)
					newArgs.ReplaceItemAt(m_oldArgs[0], 0);
				shell->SetArguments(newArgs);
			}
		}
		
		void RestoreArgs(const sptr<BShell>& shell)
		{
			shell->SetArguments(m_oldArgs);
		}
		
	private:
		SVector<SValue> m_oldArgs;
	};
	
	virtual SValue Run(const ArgList& args)
	{
		sptr<BShell> shell = m_shell.promote();
	
		SValue result = SValue::Status(B_BINDER_DEAD);
		if (shell != NULL)
		{
			ArgumentHandler argHandler;
			argHandler.ApplyArgs(shell, args, false);
			result = m_function->Execute(shell);
			argHandler.RestoreArgs(shell);
		}

		return result;
	}
	
private:
	sptr<Function> m_function;
	wptr<BShell> m_shell;
};


SyntaxTree::SyntaxTree()
{
}

SyntaxTree::~SyntaxTree()
{
}

status_t SyntaxTree::AsReturnValue(const SValue& value)
{
	switch (value.Type())
	{	
		case B_UNDEFINED_TYPE:
			return B_ERROR;
		
		case B_WILD_TYPE:
			return B_OK;
		
		case B_BINDER_TYPE:		
			return value.AsBinder() == NULL;

		case B_BOOL_TYPE:
			return !value.AsInt32();

		case B_INT8_TYPE:
		case B_INT16_TYPE:
		case B_INT32_TYPE:
		case B_INT64_TYPE:
		case B_FLOAT_TYPE:
		case B_DOUBLE_TYPE:
			return value.AsInt32();
	
		default:
			return B_ERROR;
	}

//	return B_ERROR;
}

SValue SyntaxTree::Expand(const sptr<ICommand>& shell, const SString& expand, bool* command_expansion)
{
	return expand_argument(shell, expand, command_expansion);
}

Command::Command()
{
}

Pipeline::Pipeline()
	:	m_bang(false)
{
}

SValue Pipeline::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{	
	size_t index = 0;
	for (index = 0 ; index < m_commands.CountItems() - 1 && !*outExit; index++)
	{
		m_commands.ItemAt(index)->Evaluate(parent, shell, outExit);
	}

	return m_commands.ItemAt(index)->Evaluate(parent, shell, outExit);
}

void Pipeline::Decompile(const sptr<ITextOutput>& out)
{
	if (m_bang)
		out << "!";
	
	for (size_t i = 0 ; i < m_commands.CountItems() ; i++)
	{
		m_commands.ItemAt(i)->Decompile(out);
	}
}

void Pipeline::Add(const sptr<Command>& command)
{
	m_commands.AddItem(command);
}

void Pipeline::Bang()
{
	this->m_bang = true;
}

AndOr::AndOr()
{
}

SValue AndOr::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	SValue result = m_pipelines.ItemAt(0)->Evaluate(parent, shell, outExit);

	if (*outExit) return result;

	// the execution shall continue if the result is true (0) for
	// AND_IF and false(1) for OR_IF. the execution shall stop for
	// false (1) for and AND_IF and true(0) for OR_IF.
	for (size_t i = 1 ; i < m_pipelines.CountItems() ; i++)
	{
		int32_t sep = m_separators.ItemAt(i);

		sptr<Pipeline> pipeline = m_pipelines.ItemAt(i);
		if (sep == Lexer::AND_IF)
		{
			// keep going
			if (result.AsInt32() == 0)
				result = pipeline->Evaluate(parent, shell, outExit);
			else
				break;
		}
		else if (sep == Lexer::OR_IF)
		{
			if (result.AsInt32() == 1)
				result = pipeline->Evaluate(parent, shell, outExit);
			else
				break;
		}

		if (*outExit) return result;
	}

	return result;
}

void AndOr::Decompile(const sptr<ITextOutput>& out)
{
	m_pipelines.ItemAt(0)->Decompile(out);

	for (size_t i = 1 ; i < m_pipelines.CountItems() ; i++)
	{
		int separator = m_separators.ItemAt(i);

		if (separator == Lexer::AND_IF)
			out << " && ";
		else
			out << " || ";

		m_pipelines.ItemAt(i)->Decompile(out);
	}
}

void AndOr::Add(const sptr<Pipeline>& pipeline)
{
	m_pipelines.AddItem(pipeline);
}

void AndOr::Add(int separator)
{
	m_separators.AddItem(separator);
}

CompoundList::CompoundList()
{
}

SValue CompoundList::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	size_t index = 0;
	for (index = 0 ; index < m_andor.CountItems() - 1 ; index++)
	{
		const SValue result = m_andor.ItemAt(index)->Evaluate(parent, shell, outExit);
		if (*outExit) return result;
	}

	return m_andor.ItemAt(index)->Evaluate(parent, shell, outExit);
}

void CompoundList::Decompile(const sptr<ITextOutput>& out)
{
	for (size_t i = 0 ; i < m_andor.CountItems() ; i++)
	{
		m_andor.ItemAt(i)->Decompile(out);
		out << " ; ";
	}
}

void CompoundList::Add(const sptr<AndOr>& andor)
{
	m_andor.AddItem(andor);
}


List::List()
{
}

SValue List::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	SValue result = SValue::Status(B_OK);
	*outExit = false;
	
	size_t size = m_andors.CountItems();
	for (size_t i = 0 ; i < size && !*outExit; i++)
	{
		result = m_andors.ItemAt(i)->Evaluate(parent, shell, outExit);
	}

	return result;
}

void List::Decompile(const sptr<ITextOutput>& out)
{
	size_t size = m_andors.CountItems();
	for (size_t i = 0 ; i < size ; i++)
	{
		m_andors.ItemAt(i)->Decompile(out);
	}
}

void List::Add(const sptr<AndOr>& andor)
{
	if (andor != NULL)
		m_andors.AddItem(andor);
}

IORedirect::IORedirect()
	:	m_next(NULL)
{
}

SValue IORedirect::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	*outExit = false;
	
	SValue target = Expand(shell, m_fileName);

	// If the target is a string, look it up in the context.
	if (target.Type() == B_STRING_TYPE)
	{
		SNode node(parent->Context().Root());
		if (node.ErrorCheck() != B_OK)
		{
			parent->TextError() << "redirect: no CONTEXT found!" << endl;
			return SValue::Status(node.ErrorCheck());
		}
		target = node.Walk(m_fileName, INode::CREATE_DATUM).AsBinder();
	}

	sptr<IDatum> datum = interface_cast<IDatum>(target);
	if (datum != NULL) {
		uint32_t mode = IDatum::READ_ONLY;
		switch (m_direction)
		{
			case Lexer::GREATER:		mode = IDatum::READ_WRITE|IDatum::ERASE_DATUM;	break;
			case Lexer::DGREATER:		mode = IDatum::READ_WRITE|IDatum::OPEN_AT_END;	break;
			case Lexer::LESSGREATER:	mode = IDatum::READ_WRITE|IDatum::ERASE_DATUM;	break;
		}
		target = datum->Open(mode);
	}

	switch (m_direction)
	{
		case Lexer::GREATER:
		case Lexer::DGREATER:
		{
			sptr<IByteOutput> output = IByteOutput::AsInterface(target);
			if (output != NULL)
			{
				if (m_fd == "" || m_fd == "0") shell->SetByteOutput(output);
				else if (m_fd == "2") shell->SetByteError(output);
				else parent->TextError() << "redirect: don't support output fd " << m_fd << endl;
			}
			else
				parent->TextError() << "redirect: " << m_fileName << " is not an output stream." << endl;
			
			break;
		}
		
		case Lexer::LESS:
		{
			sptr<IByteInput> input = IByteInput::AsInterface(target);
			if (input != NULL)
			{
				if (m_fd == "" || m_fd == "1") shell->SetByteInput(input);
				else parent->TextError() << "redirect: don't support input fd " << m_fd << endl;
			}
			else
				parent->TextError() << "redirect: " << m_fileName << " is not an input stream." << endl;
			
			break;
		}
		
		case Lexer::LESSGREATER:
		{
			sptr<IByteOutput> output = IByteOutput::AsInterface(target);
			if (output != NULL)
			{
				shell->SetByteOutput(output);
				shell->SetByteError(output);
			}
			else
				parent->TextError() << "redirect: " << m_fileName << " is not an output stream." << endl;
			
			sptr<IByteInput> input = IByteInput::AsInterface(target);
			if (input != NULL) shell->SetByteInput(input);
			else parent->TextError() << "redirect: " << m_fileName << " is not an input stream." << endl;
			
			break;
		}

		case Lexer::DLESS:			parent->TextError() << "redirect: << not implemented" << endl; break;
		case Lexer::LESSAND:		parent->TextError() << "redirect: <& not implemented" << endl; break;
		case Lexer::GREATERAND:		parent->TextError() << "redirect: >& not implemented" << endl; break;
		case Lexer::CLOBBER:		parent->TextError() << "redirect: >| not implemented" << endl; break;
		case Lexer::DLESSDASH:		parent->TextError() << "redirect: <<- not implemented" << endl; break;
		default:
			parent->TextError() << "redirect: type " << m_direction << " not supported" << endl;
	}

	if (m_next != NULL)
		return m_next->Evaluate(parent, shell, outExit);

	return SValue();
}

void IORedirect::Decompile(const sptr<ITextOutput>& out)
{
	if (m_fd != "")
		out << m_fd << " ";

	switch (m_direction)
	{
		case Lexer::GREATER:		out << "> "; break;
		case Lexer::LESS:			out << "< "; break;
		case Lexer::DLESS:			out << "<< "; break;
		case Lexer::DGREATER:		out << ">> "; break;
		case Lexer::LESSAND:		out << "<& "; break;
		case Lexer::GREATERAND:		out << ">& "; break;
		case Lexer::LESSGREATER:	out << "<> "; break;
		case Lexer::CLOBBER:		out << ">| "; break;
		case Lexer::DLESSDASH:		out << "<<- "; break;
		default:
			printf("IORedirect::Decompile - '%d' redirection not supported\n", m_direction);
	}

	out << m_fileName << " ";

	if (m_next != NULL)
		m_next->Decompile(out);
}

void IORedirect::SetNext(const sptr<IORedirect>& next)
{
	m_next = next;
}

void IORedirect::SetDirection(int direction)
{
	m_direction = direction;
}

void IORedirect::SetFileName(const SString& fname)
{
	m_fileName = fname;
}

void IORedirect::SetFileDescriptor(const SString& fd)
{
	m_fd = fd;
}

RedirectList::RedirectList()
	:	m_direction(NULL)
{
}

SValue RedirectList::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	parent->TextError() << "RedirectList::Evaluate not yet implemented!" << endl;
	*outExit = false;
	return SValue::Status(B_UNSUPPORTED);
}

void RedirectList::Decompile(const sptr<ITextOutput>& out)
{
	out << "RedirectList::Decompile not yet implemented!" << endl;
}

bool RedirectList::HasRedirect()
{
	return (m_direction != NULL);
}

void RedirectList::SetRedirect(const sptr<IORedirect>& direction)
{
	m_direction = direction;
}


BraceGroup::BraceGroup(const sptr<CompoundList>& list)
	:	m_list(list)
{
}

SValue BraceGroup::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	return m_list->Evaluate(parent, shell, outExit);
}

void BraceGroup::Decompile(const sptr<ITextOutput>& out)
{
	out << "{" << indent << endl;
	m_list->Decompile(out);
	out << endl << dedent << "}" << endl;
}

Commands::Commands(const sptr<List>& list)
	:	m_list(list)
{
}

SValue Commands::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	return m_list->Evaluate(parent, shell, outExit);
}

void Commands::Decompile(const sptr<ITextOutput>& out)
{
	m_list->Decompile(out);
}

Function::Function()
	:	m_command(NULL),
		m_list(NULL)
{
}

SValue Function::Execute(const sptr<BShell>& shell)
{
	bool doExit = false;
	
	if (m_list != NULL)
		m_list->Evaluate(shell, shell, &doExit);

	return m_command->Evaluate(shell, shell, &doExit);
}

SValue Function::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	// create an ICommand and add it to the shell.
	sptr<ICommand> command = new FunctionCommand(this, parent);
	shell->AddCommand(m_name, command);
	
	*outExit = false;
	return SValue::Status(B_OK);
}

void Function::Decompile(const sptr<ITextOutput>& out)
{
	out << m_name << "()" << endl;
	m_command->Decompile(out);
	
	if (m_list != NULL)
		m_list->Decompile(out);
}

void Function::SetCommand(const sptr<Command>& command)
{
	m_command = command;
}

void Function::SetName(const SString& name)
{
	m_name = name;
}

void Function::SetRedirectList(const sptr<RedirectList>& list)
{
	m_list = list;
}

CaseList::CaseList()
	:	m_list(NULL),
		m_next(NULL)
{
}

SValue CaseList::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	parent->TextError() << "CaseList::Evaluate not implemented!" << endl;
	*outExit = false;
	return SValue::Status(B_UNSUPPORTED);
}

void CaseList::Decompile(const sptr<ITextOutput>& out)
{
	out << m_pattern << ")" << indent << endl;

	if (m_list != NULL)
	{
		m_list->Decompile(out);
	}

	out << endl;

	if (m_semi)
	{
		out << ";;" << endl;	
	}

	out << dedent;
}

void CaseList::AddPattern(const SString &pattern)
{
	this->m_pattern = pattern;
}

SString CaseList::GetPattern()
{
	return m_pattern;
}

void CaseList::AddList(const sptr<CompoundList>& list)
{
	this->m_list = list;
}

sptr<CompoundList> CaseList::GetList()
{
	return m_list;
}

sptr<CaseList> CaseList::GetNext()
{
	return m_next;
}

void CaseList::SetNext(const sptr<CaseList>& next)
{
	this->m_next = next;
}

void CaseList::SetSemi()
{
	m_semi = true;
}

bool CaseList::HasSemi()
{
	return m_semi;
}

Case::Case()
	:	m_list(NULL)
{
}

SValue Case::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	parent->TextError() << "Case::Evaluate not implemented!" << endl;
	*outExit = false;
	return SValue::Status(B_UNSUPPORTED);
}

void Case::Decompile(const sptr<ITextOutput>& out)
{
	out << "case " << m_label << " in" << indent << endl;
	
	sptr<CaseList> list = m_list;

	while (list != NULL)
	{
		list->Decompile(out);
		list = list->GetNext();
	}

	out << dedent << "esac" << endl;
}

void Case::SetCaseList(const sptr<CaseList>& list)
{
	m_list = list;
}

void Case::SetLabel(const SString& label)
{
	m_label = label;
}

For::For()
	:	m_dolist(NULL)
{
}

SValue For::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	SVector<SValue> args;

	for (size_t i = 0 ; i < m_words.CountItems(); i++)
	{
		bool insert_args = false;
		//bout << "WORDS [" << i << "] " << m_words[i] << endl;
		SValue expanded = Expand(shell, m_words.ItemAt(i), &insert_args);
		
		if (insert_args)
		{
			collect_arguments(expanded.AsString(), &args);
		}
		else
		{
			//bout << "adding expanded " << expanded << endl;
			args.AddItem(expanded);
		}
	}
	
	*outExit = false;
	SValue returned = SValue::Status(B_OK);
	
	for (size_t i = 0 ; i < args.CountItems() && !*outExit; i++)
	{
		//bout << "setting m_condition to be " << args.ItemAt(i) << endl;
		shell->SetProperty(SValue::String(m_condition), args.ItemAt(i));
		returned = m_dolist->Evaluate(parent, shell, outExit);
	}
	
	return returned;
}

void For::Decompile(const sptr<ITextOutput>& out)
{
	out << "For::Decompile not yet implemented!" << endl;
}

void For::Add(const SString &words)
{
	m_words.AddItem(words);
}

void For::SetCondition(const SString &condition)
{
	m_condition = condition;
}

void For::SetDoList(const sptr<CompoundList>& dolist)
{
	m_dolist = dolist;
}

Foreach::Foreach()
	:	m_overMode(false), m_dolist(NULL)
{
}

SValue Foreach::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	*outExit = false;
	SValue returned = SValue::Status(B_OK);

	size_t size = m_list.CountItems();
	for (size_t i = 0 ; i < size && !*outExit; i++)
	{
		bool insert_args = false;
		SString string = m_list.ItemAt(i);
		SValue expanded = Expand(shell, string, &insert_args);

		SValue key;
		SValue value;
		void* cookie = NULL;

		while (expanded.GetNextItem(&cookie, &key, &value) == B_OK && !*outExit)
		{
			if (m_overMode)
			{
				if (m_valueVariable != "")
				{
					shell->SetProperty(SValue::String(m_keyVariable), key);
					shell->SetProperty(SValue::String(m_valueVariable), value);
				}
				else
				{
					shell->SetProperty(SValue::String(m_keyVariable), SValue(key, value));
				}
				returned = m_dolist->Evaluate(parent, shell, outExit);
			}
			else
			{
				sptr<IBinder> binder(value.AsBinder());
				if (binder == NULL)
				{
					// Not an object; maybe a path?
					SNode node(parent->Context().Root());
					if (node.ErrorCheck() != B_OK)
					{
						parent->TextError() << "foreach: no CONTEXT found!" << endl;
						return SValue::Status(node.ErrorCheck());
					}
					binder = node.Walk(value.AsString(), (uint32_t)0).AsBinder();
				}
				SIterator it(interface_cast<IIterable>(binder));
				if (it.ErrorCheck() != B_OK) it.SetTo(interface_cast<IIterator>(binder));
				if (it.ErrorCheck() != B_OK)
				{
					parent->TextError() << "foreach: " << value << " is not iterable!" << endl;
				}
				while (it.Next(&key, &value) == B_OK) {
					if (m_valueVariable != "")
					{
						shell->SetProperty(SValue::String(m_keyVariable), key);
						shell->SetProperty(SValue::String(m_valueVariable), value);
					}
					else
					{
						shell->SetProperty(SValue::String(m_keyVariable), SValue(key, value));
					}
					returned = m_dolist->Evaluate(parent, shell, outExit);
				}
			}
		}
		shell->RemoveProperty(SValue::String(m_keyVariable));
		if (m_valueVariable != "") shell->RemoveProperty(SValue::String(m_keyVariable));
	}

	return returned;
}

void Foreach::Decompile(const sptr<ITextOutput>& out)
{
	out << "Foreach::Decompile not yet implemented!" << endl;

	out << "foreach " << m_keyVariable;
	if (m_valueVariable != "") out << " " << m_valueVariable;
	out << (m_overMode ? " over " : " in ");
	
	size_t size = m_list.CountItems();
	for (size_t i = 0 ; i < size ; i++)
	{
		out << m_list.ItemAt(i) << " ";
	}
	out << "; do" << indent << endl;
	m_dolist->Decompile(out);
	out << dedent << "done" << endl;
}

void Foreach::SetKeyVariable(const SString& var)
{
	m_keyVariable = var;
}

void Foreach::SetValueVariable(const SString& var)
{
	m_valueVariable = var;
}

void Foreach::SetOverMode(bool overMode)
{
	m_overMode = overMode;
}

void Foreach::Add(const SString& list)
{
	m_list.AddItem(list);
}

void Foreach::SetDoList(const sptr<CompoundList>& dolist)
{
	m_dolist = dolist;
}

If::If()
	:	m_condition(NULL),
		m_then(NULL),
		m_next(NULL)
{
}

SValue If::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	*outExit = false;
	SValue result(SValue::Status(B_OK));

	if (m_condition != NULL)
	{
		if (AsReturnValue(m_condition->Evaluate(parent, shell, outExit)) == B_OK)
			result = m_then->Evaluate(parent, shell, outExit);
		else if (m_next != NULL)
			result = m_next->Evaluate(parent, shell, outExit);
	}
	else
	{
		// this is an else block
		result = m_then->Evaluate(parent, shell, outExit);
	}

	return result;
}

void If::Decompile(const sptr<ITextOutput>& out)
{
	if (m_condition != NULL)
	{
		out << "if ";
		m_condition->Decompile(out);
		out << " then" << indent << endl;
	}
	else
	{
		out << "se " << indent << endl;
	}

	m_then->Decompile(out);
	out << dedent << endl;

	if (m_next != NULL)
	{
		out << "el";
		m_next->Decompile(out);
	}

	if (m_next == NULL)
		out << "fi" << endl;
}

void If::SetCondition(const sptr<CompoundList>& list)
{
	this->m_condition = list;
}

void If::SetNext(const sptr<Command>& command)
{
	m_next = command;
}

void If::SetThen(const sptr<CompoundList>& list)
{
	this->m_then = list;
}

SubShell::SubShell(const sptr<CompoundList>& list)
	:	m_list(list)
{
}

SValue SubShell::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	parent->TextError() << "SubShell::Evaluate not yet implemented!" << endl;
	*outExit = false;
	return SValue::Status(B_UNSUPPORTED);
}

void SubShell::Decompile(const sptr<ITextOutput>& out)
{
	out << "SubShell::Decompile not yet implemented!" << endl;
}

Until::Until()
	:	m_condition(NULL),
		m_dolist(NULL)
{
}

SValue Until::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	SValue result(SValue::Status(B_OK));

	while (AsReturnValue(m_condition->Evaluate(parent, shell, outExit)) != B_OK && !*outExit)
	{
		result = m_dolist->Evaluate(parent, shell, outExit);
		if (*outExit) break;
	}

	return result;
}

void Until::Decompile(const sptr<ITextOutput>& out)
{
	out << "Until::Decompile not yet implemented!" << endl;
}

void Until::SetCondition(const sptr<CompoundList>& list)
{
	m_condition = list;
}

void Until::SetDoList(const sptr<CompoundList>& list)
{
	m_dolist = list;
}

While::While()
	:	m_condition(NULL),
		m_dolist(NULL)
{
}

SValue While::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	SValue result(SValue::Status(B_OK));

	while (AsReturnValue(m_condition->Evaluate(parent, shell, outExit)) == B_OK && !*outExit)
	{
		result = m_dolist->Evaluate(parent, shell, outExit);
		if (*outExit) break;
	}

	return result;
}

void While::Decompile(const sptr<ITextOutput>& out)
{
	out << "while ";
	m_condition->Decompile(out);
	out << "do" << indent << endl;
	m_dolist->Decompile(out);
	out << endl << dedent << "done" << endl;
}

void While::SetCondition(const sptr<CompoundList>& list)
{
	m_condition = list;
}

void While::SetDoList(const sptr<CompoundList>& list)
{
	m_dolist = list;
}

CommandPrefix::CommandPrefix()
	: m_export(false)
	, m_direction(NULL)
{
}

SValue CommandPrefix::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	SValue result;

	if (m_direction != NULL)
		result = m_direction->Evaluate(parent, shell, outExit);
	if (*outExit) return result;
	
	size_t size = m_assignments.CountItems();
	for (size_t i = 0 ; i < size ; i++)
	{
		SString str(m_assignments.ItemAt(i));
		ssize_t equal = str.FindFirst("=");

		SString key(str, equal);
		SString value(str.String()+equal+1);

		result = Expand(shell, value);

		//bout << "SETTING property " << key << " to " << result << endl;
		shell->SetProperty(SValue::String(key), result);
	}

	return result;
}

void CommandPrefix::Decompile(const sptr<ITextOutput>& out)
{
	if (m_export)
		out << "export ";
	
	for (size_t i = 0 ; i < m_assignments.CountItems() ; i++)
	{
		out << m_assignments.ItemAt(i) << " ";
	}

	if (m_direction != NULL)
		m_direction->Decompile(out);
}

bool CommandPrefix::IsExport() const
{
	return m_export;
}

bool CommandPrefix::HasAssignment() const
{
	return (m_assignments.CountItems() > 0);
}

bool CommandPrefix::HasRedirect() const
{
	return (m_direction != NULL);
}

sptr<IORedirect> CommandPrefix::GetIORedirect() const
{
	return m_direction;
}

SVector<SString> CommandPrefix::GetAssignments() const
{
	return m_assignments;
}

void CommandPrefix::SetExport(bool isExport)
{
	m_export = isExport;
}

void CommandPrefix::AddAssignment(const SString& assignment)
{
	m_assignments.AddItem(assignment);
}

void CommandPrefix::SetRedirect(const sptr<IORedirect>& redirect)
{
	m_direction = redirect;
}

CommandSuffix::CommandSuffix()
	:	m_direction(NULL)
{
}

SValue CommandSuffix::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
	if (m_direction != NULL)
		return m_direction->Evaluate(parent, shell, outExit);
	*outExit = false;
	return SValue();
}

void CommandSuffix::Decompile(const sptr<ITextOutput>& out)
{
	for (size_t i = 0 ; i < m_words.CountItems() ; i++)
	{
		out << m_words.ItemAt(i) << " ";
	}

	if (m_direction != NULL)
		m_direction->Decompile(out);
}

void CommandSuffix::AddWord(const SString& word)
{
	m_words.AddItem(word);
}

bool CommandSuffix::HasWord()
{
	return (m_words.CountItems() > 0);
}

bool CommandSuffix::HasRedirect()
{
	return (m_direction != NULL);
}

void CommandSuffix::SetRedirect(const sptr<IORedirect>& redirect)
{
	m_direction = redirect;
}

SVector<SString> CommandSuffix::GetWords()
{
	return m_words;
}

sptr<IORedirect> CommandSuffix::GetIORedirect()
{
	return m_direction;
}

SimpleCommand::SimpleCommand()
	:	m_prefix(NULL),
		m_suffix(NULL)
{
}

SValue SimpleCommand::Evaluate(const sptr<BShell>& parent, const sptr<ICommand>& shell, bool* outExit)
{
//	bout << "SimpleCommand::Evaluate: " << m_command << endl;

	sptr<ICommand> command = NULL;
	bool doExit = false;
	*outExit = false;
	
	if (m_command != "" && m_command != "exit" && m_command != "return"
		&& m_command != "cd" && m_command != "." && m_command != "source")
	{
		SValue cmdName = Expand(shell, m_command);

		// If the command is actually an ICommand object,
		// run it in-place.
		// XXX This isn't quite right -- we really want
		// to spawn a copy, in which we can set our own
		// environment.
		command = ICommand::AsInterface(cmdName);

		// Not an object, try to execute by name.
		if (command == NULL) {
			command = shell->Spawn(cmdName.AsString());
		}
	
		if (command == NULL)
		{
			parent->TextError() << "bsh: " << m_command << ": command not found" << endl;
			return SValue::Status(B_NAME_NOT_FOUND);
		}
	}
	else
		command = shell;

	SValue result;

	if (m_prefix != NULL)
		result = m_prefix->Evaluate(parent, command, &doExit);
	
	if (m_suffix != NULL)
		m_suffix->Evaluate(parent, command, &doExit);
	
	SVector<SString> words;
	if (m_suffix != NULL)
		words = m_suffix->GetWords();

	// set redirects

	// build argument list
	
	ICommand::ArgList args;
	args.AddItem(SValue::String(m_command));
	
	for (size_t index = 0 ; index < words.CountItems(); index++)
	{
		bool expand = false;
		//bout << "WORDS [" << index << "] " << words[index] << endl;
		SValue expanded = Expand(shell, words.ItemAt(index), &expand);
		//bout << "WORDS [" << index << "] expaned to " << expanded << endl;
		if (expand)
		{
			collect_arguments(expanded.AsString(), &args);
		}
		else
		{
			args.AddItem(expanded);
		}
	}
		
	// only run this if we are a newly spawned command!
	if (command != shell)
	{
		result = command->Run(args);
		// This command may be a function, that could call "exit".
		*outExit = parent->ExitRequested();
	}
	else if (m_command == "exit" || m_command == "return")
	{
		result = args.CountItems() > 1 ? args[1] : SValue::Status(B_OK);
		*outExit = true;
		if (m_command == "exit") {
			parent->RequestExit();
		}
	}
	else if (m_command == "cd")
	{
		SString path;
		if (args.CountItems() > 1) {
			path = args[1].AsString();
		}
		if (path != "") {
			SString cd=shell->GetProperty(SValue::String("PWD")).AsString();
			cd.PathAppend(path, true);
			SNode node(parent->Context().Root());
			SValue dir(node.Walk(cd, (uint32_t)0));
			if (interface_cast<INode>(dir) != NULL) {
				shell->SetProperty(SValue::String("PWD"), SValue::String(cd));
				result = SValue::Status(B_OK);
			} else {
				parent->TextError() << "cd: '" << path << "' is not a directory." << endl;
				result = SValue::Status(B_BAD_VALUE);
			}
		} else {
			parent->TextError() << "cd: no directory specified." << endl;
			result = SValue::Status(B_BAD_VALUE);
		}
	}
	else if (m_command == "." || m_command == "source")
	{
		if (args.CountItems() > 1) {
			sptr<ITextInput> input;
			SString path;
			find_some_input(args[1], parent, &input, &path);
			if (input != NULL) {
				SValue oldFile = parent->GetProperty(kBSH_SCRIPT_FILE);
				SValue oldDir = parent->GetProperty(kBSH_SCRIPT_DIR);
				
				args.RemoveItemsAt(0, 2);
				args.AddItemAt(SValue::String(path), 0);
				parent->SetLastResult(SValue::String(path));
				parent->SetProperty(kBSH_SCRIPT_FILE, SValue::String(path));
				SString parentDir;
				path.PathGetParent(&parentDir);
				parent->SetProperty(kBSH_SCRIPT_DIR, SValue::String(parentDir));
				
				FunctionCommand::ArgumentHandler argHandler;
				argHandler.ApplyArgs(parent, args, true);
				sptr<Lexer> lexer = new Lexer(parent, input, parent->TextOutput(), false);
				SValue result;
				if (lexer != NULL) {
					Parser parser(parent);
					SValue result = parser.Parse(lexer);
				} else {
					parent->TextError() << m_command << ": out of memory." << endl;
					result = SValue::Status(B_NO_MEMORY);
				}
				
				argHandler.RestoreArgs(parent);
				parent->SetProperty(kBSH_SCRIPT_FILE, oldFile);
				parent->SetProperty(kBSH_SCRIPT_DIR, oldDir);
				return result;
			} else {
				parent->TextError() << m_command << ": '" << args[1] << "' is not a file." << endl;
				result = SValue::Status(B_BAD_VALUE);
			}
		} else {
			parent->TextError() << m_command << ": no file specified." << endl;
			result = SValue::Status(B_BAD_VALUE);
		}
		*outExit = parent->ExitRequested();
	}
	
	return result;
}

void SimpleCommand::Decompile(const sptr<ITextOutput>& out)
{
	if (m_prefix != NULL)
		m_prefix->Decompile(out);

	if (m_command != "")
		out << m_command << " ";

	if (m_suffix != NULL)
		m_suffix->Decompile(out);
}

void SimpleCommand::SetSuffix(const sptr<CommandSuffix>& suffix)
{
	m_suffix = suffix;
}

void SimpleCommand::SetPrefix(const sptr<CommandPrefix>& prefix)
{
	m_prefix = prefix;
}

void SimpleCommand::SetCommand(const SString& command)
{
	m_command = command;
}
