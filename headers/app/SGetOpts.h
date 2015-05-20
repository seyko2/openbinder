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

#ifndef APP_SGETOPTS_H
#define APP_SGETOPTS_H

#include <support/ITextStream.h>

#include <app/BCommand.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace app {
using namespace palmos::support;
#endif

enum
{
	B_MAIN_ARGUMENT = 0,	// Option is really a main command argument
	B_NO_ARGUMENT,			// This option does not have an argument
	B_REQUIRED_ARGUMENT		// This option requires an argument
};

struct SLongOption
{
	size_t						size;	// bytes in this structure
	const char*					name;
	int32_t						type;	// one of B_*_ARGUMENT
	int32_t						option;	// single-character or int identifer
	const char*					help;	// optional help text for this option

	inline SLongOption*			Next() { return (SLongOption*)(((size_t)this)+size); }
	inline const SLongOption*	Next() const { return (const SLongOption*)(((size_t)this)+size); }
};

class SGetOpts
{
public:
						SGetOpts(const SLongOption* options);
						~SGetOpts();

	void				TheyNeedHelp();
	bool				IsHelpNeeded() const;
	void				PrintShortHelp(const sptr<ITextOutput>& out, const ICommand::ArgList& args);
	void				PrintHelp(const sptr<ITextOutput>& out, const ICommand::ArgList& args, const char* longHelp);

	// Returns > 0 for each valid option, 0 for each argument, -1 when done.
	// XXX Shouldn't pass in cmd here -- it is only used to get the text output
	// stream for printing errors.
	int32_t				Next(const sptr<BCommand>& cmd, const ICommand::ArgList& args);

	// Move to the next command argument.  Returns the new index, or an error
	// if there are no more arguments.
	ssize_t				NextArgument(const ICommand::ArgList& args);

	int32_t				ArgumentIndex() const;
	SValue				ArgumentValue() const;

	int32_t				OptionIndex() const;

private:
						SGetOpts(const SGetOpts&);
	SGetOpts&			operator=(const SGetOpts&);

	const SLongOption*	m_options;
	int32_t				m_curIndex;
	int32_t				m_optIndex;
	int32_t				m_endOfOpts;
	int32_t				m_optCode;
	SValue				m_argument;
	SValue				m_shortOpts;
	int32_t				m_shortPos;
	bool				m_helpNeeded;
};


#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::app
#endif

#endif // APP_BCOMMAND_H
