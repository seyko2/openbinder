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

#ifndef _PARSER_H
#define _PARSER_H

#include <support/ByteStream.h>
#include <support/ITextStream.h>
#include <support/String.h>
#include <support/StringIO.h>
#include <support/Vector.h>

#include <app/BCommand.h>

#include "SyntaxTree.h"

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::app;
#endif

bool find_some_input(const SValue& val, const sptr<BCommand>& shell, sptr<ITextInput>* input, SString* path);

SValue make_value(const sptr<ICommand>& shell, int32_t* outIndex, const char* data, int32_t length);
SValue make_value(const sptr<ICommand>& shell, const SString& string);
SValue expand_argument(const sptr<ICommand>& shell, const SString& expand, bool* command_expansion = NULL);
void collect_arguments(const SString& expanded, SVector<SValue>* outArgs);

SString next_token(const sptr<BStringIO>& script);

// It is intentional for the Lexer not to derive virtually from SAtom.
// Dianne says that it is not needed for private classes???
class Lexer : public SAtom
{
public:

	Lexer(const sptr<ICommand>& shell, const sptr<ITextInput>& input, const sptr<ITextOutput>& output, bool interactive);
	virtual ~Lexer();
		
	size_t Size() const;

	int NextToken();
	SString CurrentToken();
	bool HasMoreTokens();
	
	// Remove all tokens before current.  Can not
	// rewind before this point.
	void FlushTokens();

	//! rewind by the number of tokens
	void Rewind();
	void Rewind(int rewind);

	void ConsumeComments();
	void ConsumeWhiteSpace();

	void SetPrompt(const sptr<ICommand>& shell);
	SString GetPrompt() const;
	
	inline bool IsInteractive() const { return m_interactive; }

	enum
	{
		END_OF_STREAM	= 0x80000000,
		NOT_A_DELIMETER	= 0,
		AND,
		AND_IF,
		ASSIGNMENT_WORD,
		BANG,
		CLOBBER,
		DGREATER,
		DLESS,
		DLESSDASH,
		DSEMI,
		GREATER,
		GREATERAND,
		IO_NUMBER,
		LBRACE,
		LESS,
		LESSAND,
		LESSGREATER,
		LPAREN,
		NAME,
		NEWLINE,
		OR,
		OR_IF,
		RBRACE,
		RPAREN,
		SEMI,
		WORD,

		KEY_WORD				= 0x10000000,
		CASE					= KEY_WORD | 1,
		DO						= KEY_WORD | 2,
		DONE					= KEY_WORD | 3,
		ELIF					= KEY_WORD | 4,
		ELSE					= KEY_WORD | 5,
		ESAC					= KEY_WORD | 6,
		EXPORT					= KEY_WORD | 7,
		FI						= KEY_WORD | 8,
		FOR						= KEY_WORD | 9,
		FOREACH					= KEY_WORD | 10,
		FUNCTION				= KEY_WORD | 11,
		IF						= KEY_WORD | 12,
		IN						= KEY_WORD | 13,
		THEN					= KEY_WORD | 14,
		UNTIL					= KEY_WORD | 15,
		WHILE					= KEY_WORD | 16,
		OVER					= KEY_WORD | 17,
		
		SCAN_NORMAL				= 0x0000,
		DOUBLE_QUOTED			= 0x0001,
		SINGLE_QUOTED			= 0x0002,
		VALUE_EXPANSION			= 0x0010,
		COMMAND_EXPANSION		= 0x0020,
		PARAMETER_SUBSITUTION 	= 0x0040,
		PARAMETER_EXPANSION		= 0x0080,
		VARIABLE_EXPANSION 		= 0x0100,
		APPEND_BUFFER	 		= 0x0200,
		STREAM_EOF				= 0x0400,
	};

private:

	/*! m_buffer used to be a static size.  However, large tokens
		could overflow the buffer.  Normally m_buffer is line oriented,
		but an entire token is buffered at once.  Imagine a script 
			exit @{...}
		where the entire script is in the SValue.
		Lots of code cached pointers into the buffer.  Since the code
		was pretty clear already, I created this Iterator class that
		looks like a char*, but always uses m_buffer so that when
		m_buffer moves, so does the Iterator. */
	class Iterator {
	public:
		Iterator(char*& buffer) : m_ptr(buffer)	
		{
			m_offset = 0;
		}

		Iterator(const Iterator& other) : m_ptr(other.m_ptr)
		{
			m_offset = other.m_offset;
		}

		Iterator& operator=(const Iterator& other)
		{
			m_ptr = other.m_ptr;
			m_offset = other.m_offset;
			return *this;
		}

		char& operator[](size_t pos)								
		{ 
			return m_ptr[pos+m_offset];	
		}

		char& operator*()
		{
			return m_ptr[m_offset];
		}

		Iterator& operator+=(size_t offset)
		{
			m_offset += offset;
			return *this;
		}

		Iterator& operator++(int)
		{
			m_offset += 1;
			return *this;
		}

		Iterator& operator--(int)
		{
			m_offset -= 1;
			return *this;
		}

		/*! Explicitly create a string rather
			than having an operator char*()
			That would open up holes in our alias. */
		SString AsString(size_t length)
		{
			return SString(&(m_ptr[m_offset]), length);
		}
	private:
		char*&	m_ptr;
		size_t	m_offset;
	};

	off_t Position() const;

	void PrintToken();

	bool IsSpace(int c);

	bool ConsumeDelimeter();
	bool ConsumeKeyword();
	void ConsumeToken();
	
	int IsDelimeter();

	bool IsValidName(const SString& token);

	Iterator Buffer();

	const sptr<ICommand> m_shell;
	const sptr<ITextInput> m_textInput;
	const sptr<ITextOutput> m_textOutput;
	const bool m_interactive;
	
	int m_state;
	int m_depth;
	ssize_t m_index;
	size_t m_tokenPos;

	SVector<SString> m_tokens;
	SVector<int> m_tokenTypes;

	sptr<IByteInput> m_input;
	SString m_nextPrompt;
	SString m_lastPrompt;
	SString m_ps2;

	char* m_buffer;
	size_t m_bufferSize;
};

class Parser
{
public:
	Parser(const sptr<BShell>& shell);

	SValue Parse(const sptr<Lexer>& lexer);

private:
	void				ParseError();

	void				remove_linebreak();
	bool				remove_newline();
	bool				remove_newline_list();
	bool				remove_separator();
	bool				remove_separator_op();
	bool				remove_sequential_separator();

	sptr<AndOr>			make_and_or();
	sptr<Command>		make_brace_group();
	sptr<Command>		make_case();
	sptr<Command>		make_command();
	sptr<Command>		make_compelete_command();
	sptr<CompoundList>	make_compound_list();
	sptr<CommandPrefix>	make_command_prefix(bool isExport);
	sptr<CommandSuffix>	make_command_suffix();
	sptr<If>			make_else();
	sptr<Command>		make_function();
	sptr<Command>		make_for();
	sptr<Command>		make_for_each();
	sptr<Command>		make_if();
	sptr<IORedirect>	make_io_redirect();
	sptr<List>			make_list();
	sptr<Pipeline>		make_pipeline();
	sptr<RedirectList>	make_redirect_list();
	sptr<Command>		make_simple_command();
	sptr<Command>		make_subshell();
	sptr<Command>		make_until();
	sptr<Command>		make_while();

	sptr<BShell>		m_shell;
	sptr<Lexer>			m_lexer;
	int32_t				m_token;
	int32_t				m_commandDepth;
};

inline size_t Lexer::Size() const
{
	return m_bufferSize;
}

inline SString Lexer::GetPrompt() const
{
	return m_lastPrompt;
}

#endif
