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

#include <app/SGetOpts.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace app {
using namespace palmos::support;
#endif /* _SUPPORTS_NAMESPACE */

SGetOpts::SGetOpts(const SLongOption* options)
	:	m_options(options), m_curIndex(0), m_optIndex(0), m_endOfOpts(0),
		m_optCode(0), m_shortPos(0), m_helpNeeded(false)
{
}

SGetOpts::~SGetOpts()
{
}

void SGetOpts::TheyNeedHelp()
{
	m_helpNeeded = true;
}

bool SGetOpts::IsHelpNeeded() const
{
	return m_helpNeeded;
}

void SGetOpts::PrintShortHelp(const sptr<ITextOutput>& out, const ICommand::ArgList& args)
{
	const SLongOption* opt;

	out << (args.CountItems() > 0 ? args[0].AsString() : SString());

	// First print options...
	for (opt = m_options; opt->name != NULL; opt = opt->Next()) {
		if (opt->type != B_MAIN_ARGUMENT) {
			out << " [";
			if (opt->name[0]) {
				out << "--" << opt->name;
				if (opt->option > ' ' && opt->option < 127) out << "|";
			}
			if (opt->option > ' ' && opt->option < 127) out << "-" << (char)opt->option;
			if (opt->type == B_REQUIRED_ARGUMENT) {
				out << " <arg>";
			}
			out << "]";
		}
	}

	// Now print arguments...
	for (opt = m_options; opt->name != NULL; opt = opt->Next()) {
		if (opt->type == B_MAIN_ARGUMENT) {
			out << " " << opt->name;
		}
	}

	out << endl;
}

void SGetOpts::PrintHelp(const sptr<ITextOutput>& out, const ICommand::ArgList& args, const char* longHelp)
{
	const SLongOption* opt;
	bool firstOpt = true;
	bool useLong = false;

	PrintShortHelp(out, args);
	if (longHelp) {
		out << endl << longHelp << endl;
	}
	out << endl;

	// First, see if we should use the long or short form.
	for (opt = m_options; opt->name != NULL && !useLong; opt = opt->Next()) {
		if (opt->help != NULL && strchr(opt->help, '\n') != NULL) useLong = true;
	}

	for (opt = m_options; opt->name != NULL; opt = opt->Next()) {
		if (firstOpt) {
			out << "Options:" << indent << endl;
			firstOpt = false;
		}
		if (opt->name[0]) out << "--" << opt->name;
		if (opt->option > ' ' && opt->option < 127) {
			if (opt->name[0]) out << " (";
			out << "-" << (char)opt->option;
			if (opt->name[0]) out << ")";
		}
		if (opt->help) {
			if (!useLong) {
				out << ": " << opt->help;
			} else {
				out << indent << endl << opt->help << dedent;
			}
		}
		out << endl;
	}

	if (firstOpt) out << dedent;
}

int32_t SGetOpts::Next(const sptr<BCommand>& cmd, const ICommand::ArgList& args)
{
	const char* str;
	int32_t which;
	const SLongOption* opt;
	bool isShort;

restart:
	// If we are currently parsing a short option
	// list, take the next option out of that.
	if (m_shortOpts.IsDefined()) {
		str = (const char*)m_shortOpts.Data();
		// Get next option.
		if ((m_optCode=str[++m_shortPos]) == 0) {
			// End of list.  Restart.
			m_shortOpts.Undefine();
			goto restart;
		}

		// Look for the option.
		for (which=0, opt=m_options; opt->name; which++, opt=opt->Next()) {
			if (opt->option == m_optCode)
				break;
		}

		// If option not found, print a message and continue.
		if (opt->name == NULL) {
			if (cmd != NULL && args.CountItems() > 0) {
				cmd->TextError() << args[0].AsString()
					<< ": bad option '" << (char)m_optCode << "'" << endl;
			}
			TheyNeedHelp();
			goto restart;
		}

		isShort = true;

	// Retrieve option from the next program argument.
	} else {
		++m_curIndex;
		if (m_curIndex >= args.CountItems()) {
			return -1;
		}
		m_argument = args[m_curIndex];
		if (m_endOfOpts != 0) {
			return 0;
		}

		// First check for something that definitely isn't a valid
		// option
		if (m_argument.Type() != B_STRING_TYPE || m_argument.Length() <= 1) {
			m_endOfOpts = m_curIndex;
			return 0;
		}

		// This is a string.  If the first character isn't a dash,
		// then it isn't an option.
		str = (const char*)m_argument.Data();
		if (str[0] != '-') {
			m_endOfOpts = m_curIndex;
			return 0;
		}

		// Explicit termination of options.  Need to skip it and
		// retrieve the first argument.
		if (str[1] == '-' && str[2] == 0) {
			m_endOfOpts = m_curIndex+1;
			goto restart;
		}

		// Short option list.  Set up in that mode and restart.
		if (str[1] != '-') {
			m_shortOpts = m_argument;
			m_shortPos = 0;
			goto restart;
		}

		// Look for the option.
		for (which=0, opt=m_options; opt->name; which++, opt=opt->Next()) {
			if (strcmp(opt->name, str+2) == 0)
				break;
		}

		// If option not found, print a message and continue.
		if (opt->name == NULL) {
			if (cmd != NULL && args.CountItems() > 0) {
				cmd->TextError() << args[0].AsString()
					<< ": bad option " << str << endl;
			}
			TheyNeedHelp();
			goto restart;
		}

		m_optCode = opt->option;

		isShort = false;

	}

	m_optIndex = which;

	// OKAY...  at this point 'which' is the current option
	// being processed.  Let's figure out what to do.
	if (opt->type == B_REQUIRED_ARGUMENT) {
		++m_curIndex;
		if (m_curIndex >= args.CountItems()) {
			if (cmd != NULL && args.CountItems() > 0) {
				sptr<ITextOutput> out = cmd->TextError();
				out << args[0].AsString()
					<< ": missing required argument for ";
				if (isShort) out << "-" << (char)(m_optCode) << endl;
				else out << "--" << opt->name << endl;
			}
			TheyNeedHelp();
			goto restart;
		}
		m_argument = args[m_curIndex];
	} else {
		m_argument.Undefine();
	}

	return m_optCode;
}

ssize_t SGetOpts::NextArgument(const ICommand::ArgList& args)
{
	++m_curIndex;
	if (m_curIndex >= args.CountItems()) return -1;
	m_argument = args[m_curIndex];
	return m_curIndex;
}

int32_t SGetOpts::ArgumentIndex() const
{
	return m_curIndex;
}

SValue SGetOpts::ArgumentValue() const
{
	return m_argument;
}

int32_t SGetOpts::OptionIndex() const
{
	return m_optIndex;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::app
#endif /* _SUPPORTS_NAMESPACE */
