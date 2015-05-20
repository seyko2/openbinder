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

#include <support/CallStack.h>
#include <support/Looper.h>
#include <support/StdIO.h>
#include <support/String.h>
#include <support/StringIO.h>
#include <storage/File.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <DebugMgr.h>

#include "Parser.h"
#include "bsh.h"

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

B_STATIC_STRING_VALUE_LARGE(kBSH_HOST_PWD, "BSH_HOST_PWD", );

bool find_some_input(const SValue& val, const sptr<BCommand>& shell, sptr<ITextInput>* input, SString* outPath)
{
	//bout << "Shell is looking for input: " << val << endl;

	status_t err;
	sptr<IByteInput> stream;
	sptr<IStorage> storage;
	SValue src(val);
	
	// First, try to resolve path in the Binder namespace.
	SString path(shell->ArgToPath(val));
	//bout << "Namespace path: " << path << endl;
	if (path != "") {
		// Look it up.
		SNode node(shell->Context().Root());
		src = node.Walk(path, (uint32_t)0);
		*outPath = path;
	}
	
	// If this is a byte stream, use it directly.
	stream = IByteInput::AsInterface(src);
	if (stream != NULL) goto finish;

	// Likewise for a storage interface.
	// $^#@%$^@!!! ADS!!!!!!!!!!
	storage = IStorage::AsInterface(src);
	if (storage != NULL) goto finish;

	// Finally, try looking on the host filesystem.
	// XXX We need to unify these namespaces to remove this!
	path = shell->GetProperty(kBSH_HOST_PWD).AsString();
	path.PathAppend(val.AsString(&err));
	if (err == B_OK)
	{
		sptr<BFile> file = new BFile();
		//bout << "Host path: " << path << endl;
		err = file->SetTo(path.String(), O_RDONLY);
		if (err == B_OK)
		{
			storage = file;
			*outPath = path;
		}
	}

finish:
	if (stream == NULL && storage != NULL) {
		stream = new BByteStream(storage);
	}
	if (stream != NULL) {
		*input = new BTextInput(stream);
		return true;
	}
	
	return false;
}

static ssize_t parse_variable(const sptr<ICommand>& shell, const char* buffer, int32_t origIndex, SValue* outValue, bool* outGotVal, bool* command_expansion);

enum structure_state
{
	SIMPLE_STATE,
	MAPPING_STATE
};

enum operation_state
{
	BEGIN_OP,
	JOIN_OP,
	OVERLAY_OP,
	INHERIT_OP,
	REMOVE_OP,
	MAP_VALUES_OP,
	LOOKUB_OP
};

static void trim_whitespace(int32_t* index, const char* data)
{
	int32_t i = *index;
	while (isspace(data[i])) {
		i++;
	}
	*index = i;
}

static void consume_value(int32_t* index, const char* data, SString* outValue)
{
	int32_t i = *index;
	int32_t cnt = 0;

	// Parsing of normal string quoting scheme.
	if (data[i] == '"')
	{
		i = *index = *index + 1;
		while ( data[i] != '\0' &&
				data[i] != '"' )
		{
			if (data[i] == '\\') i++;
			i++;
			cnt++;
		}
		if (data[i] != '\0') i++;
	}
	// Parsing of special quoting schemes -- leave the quotes
	// in-place, as they are important to the meaning.
	// Note we handle SValue's character integer syntax.  This
	// isn't ambiguous with the ValueFor operator because the
	// operator must appear after a value and values must be
	// separated by operators.
	else if (data[i] == '[' || data[i] == '(')
	{
		const char endc = (data[i] == '[' ? ']' : ')');
		while ( data[i] != '\0' &&
				data[i] != endc )
		{
			if (data[i] == '\\') i++;
			i++;
			cnt++;
		}
		if (data[i] != '\0')
		{
			i++;
			cnt++;
		}
	}
	else
	{
		// XXX There is a little nastiness here -- we don't just
		// break on a '-' because the boot scripts use stuff like
		// os:window-layer->num.  Arguably the key there should be
		// enclosed in quotes, and leaving it this way means the
		// syntax for the - operator is inconsistent with other
		// operators.
		while (	data[i] != '\0' &&
				!isspace(data[i]) && 
				data[i] != ',' &&
				data[i] != '{' &&
				data[i] != '}' &&
				data[i] != '[' &&
				data[i] != ']' &&
				data[i] != '+' &&
				data[i] != '*' &&
					(data[i] != '-' ||
					data[i+1] != '>') &&
				data[i] != '@')
		{
			if (data[i] == '\\') i++;
			i++;
			cnt++;
		}
	}

	// Retrieve the final string.
	if (cnt > 0 && outValue)
	{
		char* out = outValue->LockBuffer(cnt);
		char* end = out+cnt;
		int32_t i = *index;
		while (out < end)
		{
			if (data[i] == '\\') i++;
			*out++ = data[i++];
		}
		outValue->UnlockBuffer(cnt);
	}

	*index = i;
}

SValue expand_argument(const sptr<ICommand>& shell, const SString& expand, bool* command_expansion)
{
	SValue nuvalue;
	SString nustring;

	const char* buffer = expand.String();
	ssize_t length = expand.Length();

	ssize_t start = 0;
	ssize_t index = 0;
	bool singleQuote = false;
	bool doubleQuote = false;
	bool gotvar = false;

//	bout << "Expand variable: " << expand << endl;

	while (index < length)
	{
		const char byte = buffer[index];

		// If there are no special characters, barrel on through.
		if (byte != '\\' && byte != '\"' && byte != '\'' &&
			byte != '$' && byte != '@')
		{
			index++;
			continue;
		}

		// Inside a single quote, there are only a few special chars.
		if (singleQuote && byte != '\'') {
			index++;
			continue;
		}

		// First we need to collect any characters previously found.
		if (start < index) {
			if (gotvar) {
				// If we previously generated a SValue, that is
				// the initial contents of the generated string.
				nustring = nuvalue.AsString();
				nuvalue.Undefine();
				gotvar = false;
			}
			nustring.Append(buffer+start, index-start);
			start = index;
		}

		if (byte == '\\')
		{
			index++;
			if (buffer[index] == '\n')
			{
				// if the buffer contains a newline eat it
				if (index < length) index++;
				start = index;
			}
			else
			{
				start = index;
				if (index < length) index++;
			}
			continue;

		}
		else if (byte == '\"')
		{
			index++;
			if (!singleQuote) {
				start = index;
				doubleQuote = !doubleQuote;
			}
			continue;

		}
		else if (byte == '\'')
		{
			index++;
			if (!doubleQuote) {
				start = index;
				singleQuote = !singleQuote;
			}
			continue;

		}

		// if nuvalue is defined append it to nustring before we parse
		// the next variable. this will allow $FOO$BAR/bar$FOO expansion.
		if (gotvar)
		{
			SString temp = nuvalue.AsString();
			nustring.Append(temp, temp.Length());
		}

		// Default -- handle substitution of '@' or '$'.
		bool localgotvar = false;
		ssize_t where = parse_variable(shell, buffer, index, &nuvalue, &localgotvar, command_expansion);
		if (localgotvar) gotvar = true;
	
		if (where != B_BAD_VALUE)
		{
			start = where;
			index = where;
		}
		else
		{
			index++;
		}

		if (doubleQuote && !localgotvar)  {
			gotvar = true;
			nuvalue = SValue::String("");
		}

		if (nustring.Length() > 0 || (index < length && !doubleQuote))
		{
			// String already being built, so need to concatenate this new data.
			nustring.Append(nuvalue.AsString());
			nuvalue.Undefine();
			gotvar = false;
		}
	}

	// Let's see what we've got...  first, if it's just one big
	// blob of text, return that directly.
	if (start == 0 && index >= length)
	{
		//bout << "Returning same string: " << expand << endl;
		return SValue::String(expand);
	}

	// If there is no string, return whatever SValue we found.
	if (nustring.Length() == 0 && gotvar)
	{
		//bout << "Returning raw value: " << nuvalue << endl;
		return nuvalue;
	}

	// Otherwise, concatenate any final value and return result.
	if (gotvar)
	{
		nustring.Append(nuvalue.AsString());
	}
	else
	{
		// it is possilbe to encounter a '@' in the middle of an
		// argument that isn't a value. just append it.
		nustring.Append(SString(buffer + start, index - start));
	}

	//bout << "Returning manipulated string: " << nustring << endl;

	return SValue::String(nustring);
}

void collect_arguments(const SString& expanded, SVector<SValue>* outArgs)
{
	SValue returned;

	const char* output = expanded.String();
	int32_t length = expanded.Length();

	int32_t last = 0;
	bool escape = false;
	for (int32_t i = 0 ; i < length ; i++)
	{
		if (output[i] == '\\')
		{
			i++;
		}
		else if (output[i] == '"')
		{
			escape = !escape;
		}
		else if (!escape && isspace(output[i]))
		{
			int32_t end = i;

			SString string(output+last, end - last);

			//bout << "Adding string " << string.String() << endl;
			outArgs->AddItem(SValue::String(string));
			while (isspace(output[i]))
			{
				i++;
			}
			
			last = i;
			if (output[i] == 0) i--;
		}
	}
}

enum
{
	OP_NULL				= 0,
	OP_VALUE,
	OP_MINUS			= 0x10,
	OP_PLUS,
	OP_LOGNOT			= 0x20,
	OP_BITNOT,
	OP_EXP				= 0x30,
	OP_MULT				= 0x40,
	OP_DIV,
	OP_REM,
	OP_ADD				= 0x50,
	OP_SUB,
	OP_LEFT				= 0x60,
	OP_RIGHT,
	OP_LT				= 0x70,
	OP_LE,
	OP_GE,
	OP_GT,
	OP_EQ				= 0x80,
	OP_NE,
	OP_BITAND			= 0x90,
	OP_BITXOR			= 0xa0,
	OP_BITOR			= 0xb0,
	OP_LOGAND			= 0xc0,
	OP_LOGOR			= 0xd0,
	OP_ASSIGN			= 0xe0,
	OP_ASSIGN_MULT,
	OP_ASSIGN_DIV,
	OP_ASSIGN_REM,
	OP_ASSIGN_ADD,
	OP_ASSIGN_SUB,
	OP_ASSIGN_LEFT,
	OP_ASSIGN_RIGHT,
	OP_ASSIGN_BITAND,
	OP_ASSIGN_BITXOR,
	OP_ASSIGN_BITOR
};

struct ExpressionNode
{
	int32_t Evaluate(const sptr<ICommand>& shell)
	{
		if (op == OP_VALUE) return left.Evaluate(shell);
		
		int32_t lefti = op != OP_ASSIGN ? left.Evaluate(shell) : 0;
		int32_t righti = right.Evaluate(shell);
		int32_t res;
		switch (op) {
			case OP_NULL:			res = 0; break;
			case OP_MINUS:			res = -righti; break;
			case OP_PLUS:			res = +righti; break;
			case OP_LOGNOT:			res = !righti ; break;
			case OP_BITNOT:			res = ~righti; break;
			case OP_EXP:			res = 0; break; //(int32_t)(pow(lefti, righti)); break;
			case OP_ASSIGN_MULT:
			case OP_MULT:			res = lefti * righti; break;
			case OP_ASSIGN_DIV:
			case OP_DIV:			res = righti > 0 ? lefti / righti : 0; break;
			case OP_ASSIGN_REM:
			case OP_REM:			res = righti > 0 ? lefti % righti : 0; break;
			case OP_ASSIGN_ADD:
			case OP_ADD:			res = lefti + righti; break;
			case OP_ASSIGN_SUB:
			case OP_SUB:			res = lefti - righti; break;
			case OP_ASSIGN_LEFT:
			case OP_LEFT:			res = lefti << righti; break;
			case OP_ASSIGN_RIGHT:
			case OP_RIGHT:			res = lefti >> righti; break;
			case OP_LT:				res = lefti < righti; break;
			case OP_LE:				res = lefti <= righti; break;
			case OP_GE:				res = lefti >= righti; break;
			case OP_GT:				res = lefti > righti; break;
			case OP_EQ:				res = lefti = righti; break;
			case OP_NE:				res = lefti != righti; break;
			case OP_ASSIGN_BITAND:
			case OP_BITAND:			res = lefti & righti; break;
			case OP_ASSIGN_BITXOR:
			case OP_BITXOR:			res = lefti ^ righti; break;
			case OP_ASSIGN_BITOR:
			case OP_BITOR:			res = lefti | righti; break;
			case OP_LOGAND:			res = lefti && righti; break;
			case OP_LOGOR:			res = lefti || righti; break;
			case OP_ASSIGN:			res = righti; break;
			default:				res = 0; break;
		}
		
		if (op >= OP_ASSIGN && op <= OP_ASSIGN_BITOR) {
			shell->SetProperty(SValue::String(left.value), SValue::Int32(res));
		}
		
		return res;
	}
	
	void Print()
	{
		if (op == OP_VALUE) {
			if (left.expr) left.expr->Print();
			else bout << left.value;
			return;
		}
		if (left.expr) {
			bout << "(";
			left.expr->Print();
			bout << ")";
		} else bout << left.value;
		switch (op) {
			case OP_NULL:			bout << "NULL"; break;
			case OP_MINUS:			bout << "-"; break;
			case OP_PLUS:			bout << "+"; break;
			case OP_LOGNOT:			bout << "!"; break;
			case OP_BITNOT:			bout << "~"; break;
			case OP_EXP:			bout << "**"; break;
			case OP_MULT:			bout << "*"; break;
			case OP_DIV:			bout << "/"; break;
			case OP_REM:			bout << "%"; break;
			case OP_ADD:			bout << "+"; break;
			case OP_SUB:			bout << "-"; break;
			case OP_LEFT:			bout << "<<"; break;
			case OP_RIGHT:			bout << ">>"; break;
			case OP_LT:				bout << "<"; break;
			case OP_LE:				bout << "<="; break;
			case OP_GE:				bout << ">="; break;
			case OP_GT:				bout << ">"; break;
			case OP_EQ:				bout << "=="; break;
			case OP_NE:				bout << "!+"; break;
			case OP_BITAND:			bout << "&"; break;
			case OP_BITXOR:			bout << "^"; break;
			case OP_BITOR:			bout << "|"; break;
			case OP_LOGAND:			bout << "&&"; break;
			case OP_LOGOR:			bout << "||"; break;
			case OP_ASSIGN:			bout << "="; break;
			case OP_ASSIGN_MULT:	bout << "*="; break;
			case OP_ASSIGN_DIV:		bout << "/="; break;
			case OP_ASSIGN_REM:		bout << "%="; break;
			case OP_ASSIGN_ADD:		bout << "+="; break;
			case OP_ASSIGN_SUB:		bout << "-="; break;
			case OP_ASSIGN_LEFT:	bout << "<<="; break;
			case OP_ASSIGN_RIGHT:	bout << ">>="; break;
			case OP_ASSIGN_BITAND:	bout << "&="; break;
			case OP_ASSIGN_BITXOR:	bout << "^="; break;
			case OP_ASSIGN_BITOR:	bout << "|="; break;
			default:				bout << "?" << op << "?" << endl;
		}
		if (right.expr) {
			bout << "(";
			right.expr->Print();
			bout << ")";
		} else bout << right.value;
	}
	
	void FreeAll()
	{
		if (left.expr) left.expr->FreeAll();
		if (right.expr) right.expr->FreeAll();
		delete this;
	}
	
	struct side
	{
		int32_t Evaluate(const sptr<ICommand>& shell)
		{
			if (expr) return expr->Evaluate(shell);
			if (value == "") return 0;
			
			status_t err;
			int32_t res=SValue::String(value).AsInt32(&err);
			if (err == B_OK) return res;
			
			// XXX deal with errors!
			res = shell->GetProperty(SValue::String(value)).AsInt32(&err);
			if (err != B_OK) bout << "expression: can't understand value " << value << endl;
			return res;
		}
		
		ExpressionNode* expr;
		SString value;
		
		side() : expr(NULL) { }
		~side() { }
	};
	
	side left;
	int32_t op;
	side right;
	
	ExpressionNode() : op(OP_NULL) { }
	~ExpressionNode() {  }
};

static ExpressionNode* parse_expression(const sptr<ICommand>& shell, const char** buffer)
{
	const char* p = *buffer;
	
	ExpressionNode cur;
	int32_t elem = 0;
	
	while (*p && *p !=')') {
		// strip spaces
		while (*p && isspace(*p)) p++;
		
		int32_t op = OP_NULL;
		switch (*p) {
			case '(':
				if (elem == 0) cur.left.expr = parse_expression(shell, &p);
				else if (elem == 1) cur.right.expr = parse_expression(shell, &p);
				else {
					ExpressionNode* nd = parse_expression(shell, &p);
					delete nd;
					bout << "expression: mis-placed open paren at " << p << endl;
				}
				continue;
			
			case '-':
				if (p[1] == '=') {
					p++;
					op = OP_ASSIGN_SUB;
				} else {
					if (elem == 0) {
						op = OP_MINUS,
						elem++;
					} else {
						op = OP_SUB;
					}
				}
				break;
				
			case '+':
				if (p[1] == '=') {
					p++;
					op = OP_ASSIGN_ADD;
				} else {
					if (elem == 0) {
						op = OP_PLUS,
						elem++;
					} else {
						op = OP_ADD;
					}
				}
				break;
				
			case '!':
				op = OP_LOGNOT;
				if (p[1] == '=') {
					p++;
					op = OP_NE;
				} else {
					if (elem == 0) {
						elem++;
					} else {
						bout << "expression: mis-placed '!' at " << p << endl;
					}
				}
				break;
				
			case '~':
				op = OP_BITNOT;
				if (elem == 0) {
					elem++;
				} else {
					bout << "expression: mis-placed '^' at " << p << endl;
				}
				break;
				
			case '*':
				if (p[1] == '=') {
					p++;
					op = OP_ASSIGN_MULT;
				} else {
					if (p[1] == '*') {
						op = OP_EXP;
						p++;
					} else {
						op = OP_MULT;
					}
				}
				break;
				
			case '/':
				if (p[1] == '=') {
					p++;
					op = OP_ASSIGN_DIV;
				} else {
					op = OP_DIV;
				}
				break;
				
			case '%':
				if (p[1] == '=') {
					p++;
					op = OP_ASSIGN_REM;
				} else {
					op = OP_REM;
				}
				break;
				
			case '<':
				if (p[1] == '<') {
					p++;
					if (p[1] == '=') {
						op = OP_ASSIGN_LEFT;
						p++;
					} else {
						op = OP_LEFT;
					}
				} else if (p[1] == '=') {
					p++;
					op = OP_LE;
				} else {
					op = OP_LT;
				}
				break;
				
			case '>':
				if (p[1] == '>') {
					p++;
					if (p[1] == '=') {
						op = OP_ASSIGN_RIGHT;
						p++;
					} else {
						op = OP_RIGHT;
					}
				} else if (p[1] == '=') {
					p++;
					op = OP_GE;
				} else {
					op = OP_GT;
				}
				break;
				
			case '=':
				if (p[1] == '=') {
					p++;
					op = OP_EQ;
				} else {
					op = OP_ASSIGN;
				}
				break;
				
			case '&':
				if (p[1] == '=') {
					p++;
					op = OP_ASSIGN_BITAND;
				} else if (p[1] == '&') {
					p++;
					op = OP_LOGAND;
				} else {
					op = OP_BITAND;
				}
				break;
				
			case '|':
				if (p[1] == '=') {
					p++;
					op = OP_ASSIGN_BITOR;
				} else if (p[1] == '|') {
					p++;
					op = OP_LOGOR;
				} else {
					op = OP_BITOR;
				}
				break;
				
			case '^':	op = OP_BITXOR; break;
			
			default: {
				// Not an operator, so consume the value.
				const char* e = p+1;
				while (*e && ((*e>='a'&&*e<='z')||(*e>='A'&&*e<='Z')||(*e>='0'&&*e<='9')||*e=='_')) e++;
				if (elem == 0) {
					cur.left.value = SString(p, e-p);
					cur.op = OP_VALUE;
				} else cur.right.value = SString(p, e-p);
				elem++;
				p = e;
			}
			continue;
		}
		
		// At this point, we have parsed the next operator.  Now
		// figure out what to do with it.  If 'elem' is 0 or 1,
		// we are still filling in values for the expression.
		// Otherwise, we have completed an expression and need to
		// start a new one with which it will be combined.
		if (elem == 0 || elem == 1) {
			cur.op = op;
		} else if (elem == 2) {
			ExpressionNode* last = new ExpressionNode(cur);
			if (!last) {
				cur.FreeAll();
				return NULL;
			}
			if ((op>>4) < (cur.op>>4)) {
				cur = ExpressionNode();
				cur.left.expr = last;
				cur.op = op;
			} else {
				p++;
				ExpressionNode* rhs = parse_expression(shell, &p);
				if (!rhs) {
					cur.FreeAll();
					last->FreeAll();
					return NULL;
				}
				cur.left.expr = last;
				cur.op = op;
				cur.right.expr = rhs;
				goto finished;
			}
			elem = 1;
		}
		
		p++;
	}
	
	//bout << "Expression: " << SString(*buffer, p-*buffer) << endl;
	
finished:
	*buffer = p;
	return new ExpressionNode(cur);
}

class VariableCommand : public BCommand, public SPackageSptr
{
public:
	VariableCommand(const sptr<IBinder>& binder, const SValue& method, const SValue& args)
		:	m_binder(binder),
			m_method(method),
			m_args(args)
	{	
	}

	virtual SValue Run(const ArgList& /*args*/)
	{
		SValue result;
		
		if (m_binder != NULL)
		{
			result = m_args.IsDefined() ? m_binder->Invoke(m_method, m_args) : m_binder->Invoke(m_method);
		}

		return result["res"];
	}
	
private:
	sptr<IBinder> m_binder;
	SValue m_method;
	SValue m_args;
};

static ssize_t parse_variable(const sptr<ICommand>& shell, const char* buffer, int32_t origIndex, SValue* outValue, bool* outGotValue, bool* command_expansion)
{
//	bout << SPrintf("parse_variable buffer='%s'", buffer) << endl;
	int32_t index = origIndex;
	int32_t start = index;
	int32_t end = start;

	const char one = buffer[origIndex];
	const char two = buffer[origIndex+1];

	if (one != '$' && one != '@') {
		ErrFatalError("parse_varaible must start with $ or @");
		return B_BAD_VALUE;
	}

	// First find the end of this variable if it is quoted in some way.
	if (two == '(' || two == '[' || two == '{') {
		index += 2;
		start = index;

		const char term = (two == '(' ? ')' : (two == '[' ? ']' : '}'));
		int32_t nesting = 0;
		while (buffer[index] != 0 && (buffer[index] != term || nesting != 0)) {
			if (buffer[index] == two) nesting++;
			else if (buffer[index] == term) nesting--;
			index++;
			if (buffer[index] == '\\') index++;
		}
		end = index;
		if (buffer[index] == term) index++;
	}

	if (one == '$' && (two == '(' || two == '['))
	{
		if (two == '(' && buffer[start] == '(') 
		{
			if (command_expansion)
				*command_expansion = false;
			const char* tmp = buffer+start+1;
			ExpressionNode* node = parse_expression(shell, &tmp);
			int32_t value = 0;
			if (node) {
				//bout << "Expression: ";
				//node->Print();
				//bout << endl;
				value = node->Evaluate(shell);
				//bout << "Result: " << value << endl;
				node->FreeAll();
			}
			*outValue = SValue::Int32(value);
			*outGotValue = true;
			return node ? index : B_ERROR;
		}
		
		sptr<ICommand> subshell = shell->Spawn(SString("sh"));
		if (subshell == NULL) return B_NO_MEMORY;

		ICommand::ArgList args;

		args.AddItem(SValue::String("sh"));
		args.AddItem(SValue::String("-c"));
		args.AddItem(SValue(B_STRING_TYPE, buffer+start, end-start));

		if (two == '(')
		{
			if (command_expansion)
				*command_expansion = true;
			
			//bout << "trying to run " << args << endl;
			SValue outputed;
			sptr<BStringIO> outputStream = new BStringIO();
			subshell->SetByteOutput(outputStream.ptr());
			subshell->Run(args);
	
			outputed = SValue::String(outputStream->String());
			*outGotValue = true;
			*outValue = outputed;
		}
		else
		{
			// return whatever the return value is
			*outValue = subshell->Run(args);
			*outGotValue = true;
		}
	}
	else if (one == '$')
	{
		if (two != '{') {
			index = origIndex+1;
			start = index;
			// stop at non-alphanumeric
			while (buffer[index] != 0 && (isalnum(buffer[index]) || buffer[index] == '_'))
				index++;
			end = index;
		}
	
		// Special case: If this is $PROCESS, handle it directly.  This is
		// because we don't want to publish our process interface as an
		// environment variable, which others can get access to.  (We
		// -might- decide to do so...  but then need to be careful to
		// not propagate it to child shells that are spawned and other
		// places.)
		SString propName(buffer+start, end-start);
		if (propName == "PROCESS") {
			*outValue = SValue::Binder(SLooper::Process()->AsBinder());
			*outGotValue = true;
		} else {
			*outValue = shell->GetProperty(SValue(SString(buffer+start, end-start)));
			if (outValue->IsDefined()) *outGotValue = true;
		}


		// if there was a dot try to see if the rest is a function call
		if (buffer[end] == '.')
		{
			// now find the starting '(' and determine if the $XXX is a binder
			sptr<IBinder> binder = outValue->AsBinder();

			if (binder != NULL)
			{
				index++;
				start = end = index;
				while (buffer[index] != '(')
					index++;
				end = index;
				
				SValue method(SString(buffer+start, end-start));
				
				start = index + 1;
			
				SValue args;
				size_t argIndex = 0;
				bool cmdExpansion = false;
				do
				{
					index++;
					if (buffer[index] == ',' || buffer[index] == ')' && start != index)
					{
						SString currString(buffer+start, index-start);
						SValue currArg = expand_argument(shell, currString, &cmdExpansion);
						args.JoinItem(SValue::Int32(argIndex), currArg);
						argIndex++;
						start = index + 1;
					}
				} while (buffer[index] != ')');
			
				sptr<ICommand> command = new VariableCommand(binder, method, args);
				*outValue = SValue::Binder(command->AsBinder());
				*outGotValue = true;
			}
		}
		
	}
	else if (one == '@' && two == '{')
	{
		// Return the evaluation, skipping $.
		int32_t newIndex = origIndex+1;
		*outValue = make_value(shell, &newIndex, buffer, index);
		*outGotValue = true;
		index = newIndex;
	}
	else if (one == '@' && two == '\0')
	{
		// just return the at sign
		*outValue = SValue::String(buffer);
		*outGotValue = true;
		index++;
	}
	else
	{
		*outValue = SValue::Undefined();
		return B_BAD_VALUE;
	}

	return index;
}

static bool is_float(const char* str)
{
	bool hasPeriod = false;
	bool hasExp = false;
	while (*str == ' ' || *str == '\t') str++;
	if (*str == 0) return false;
	if ((*str < '0' || *str > '9') && *str != '.') return false;
	if (*str == '-' || *str == '+') str++;
	while (*str) {
		if (*str == '.') {
			if (hasPeriod) return false;
			hasPeriod = true;
			str++;
			continue;
		}
		if (*str == 'e' || *str == 'E') {
			if (hasExp) return false;
			hasExp = true;
			hasPeriod = true;
			str++;
			if (*str == '-' || *str == '+') str++;
			// Fall through, because there must be a number next.
		}
		if (*str < '0' || *str > '9') break;
		str++;
	}
	while (*str == ' ' || *str == '\t') str++;
	return *str == 0;
}

SValue make_value(const sptr<ICommand>& shell, int32_t* outIndex, const char* data, int32_t length)
{
	structure_state state = SIMPLE_STATE;
	operation_state op = BEGIN_OP;
	int32_t index = *outIndex;
	char endc;
 
	// go passed any white space
	trim_whitespace(&index, data);
	
	// make sure that this is a value build up
	// note that we need to deal with the case of an [] operator
	if (data[index] == '{')
		endc = '}';
	else if (data[index] == '[')
		endc = ']';
	else
		return SValue::Undefined();
	
	index++;

	// we do not like you whitespace
	trim_whitespace(&index, data);
	
	SValue returned;
	SVector<SValue> keys;
	SValue value;
	
	while (index < length)
	{
		trim_whitespace(&index, data);
		
		if (data[index] == endc)
		{
			// we've reached our wits end.
			index++;
			break;
		}

		bool lookup = false;
		if (data[index] == '@')
		{
			index++;
			lookup = true;
		}
		
		const char* debbugging = data + index;

		if (data[index] == '$')
		{
			bool dummy;
			index = parse_variable(shell, data, index, &value, &dummy, NULL);
		}
		else
		{
			int32_t type = 0;
			SString string;

			int32_t start = index;
			consume_value(&index, data, &string);
		
			if (data[start] == '(')
			{
				// determine the cast.
				//bout << "cast: " << string << endl;
				if (string == "(int32_t)")
					type = B_INT32_TYPE;
				else if (string == "(int32)")
					type = B_INT32_TYPE;
				else if (string == "(int64_t)")
					type = B_INT64_TYPE;
				else if (string == "(int64)")
					type = B_INT64_TYPE;
				else if (string == "(status_t)")
					type = B_STATUS_TYPE;
				else if (string == "(status)")
					type = B_STATUS_TYPE;
				else if (string == "(nsecs_t)")
					type = B_NSECS_TYPE;
				else if (string == "(nsecs)")
					type = B_NSECS_TYPE;
				else if (string == "(time)")
					type = B_NSECS_TYPE;
				else if (string == "(string)")
					type = B_STRING_TYPE;
				else if (string == "(boolean)" || string == "(bool)")
					type = B_BOOL_TYPE;
				else if (string == "(float)")
					type = B_FLOAT_TYPE;
				else if (string == "(double)")
					type = B_DOUBLE_TYPE;
				else if (string == "(rect)")
					type = B_RECT_TYPE;
				else if (string == "(sptr)")
					type = B_BINDER_TYPE;
				else if (string == "(wptr)")
					type = B_BINDER_WEAK_TYPE;
				else
					type = B_STRING_TYPE;
				
				// Retrieve the value itself.
				trim_whitespace(&index, data);
				start = index;
				if (data[index] == '$' || data[index] == '@') {
					bool dummy;
					index = parse_variable(shell, data, index, &value, &dummy, NULL);
				} else {
					consume_value(&index, data, &string);
					value = SValue::String(string);
				}
			}
			else
			{
				// we need to determine the type of the value.
				
				if (data[start] == '"')
				{
					type = B_STRING_TYPE;
					//bout << "quote: " << string << endl;
				}
				else if (data[start] == '[')
				{
					type = B_INT32_TYPE;
					//bout << "chars: " << string << endl;
				}
				else
				{
					//bout << "regular: " << string << endl;
					if (!lookup)
					{	
						// we only want to do this if there is no lookup
						if (string == "wild")
							type = B_WILD_TYPE;
						else if (string == "undefined")
							type = B_UNDEFINED_TYPE;
						else if (string == "undef")
							type = B_UNDEFINED_TYPE;
						else if (string == "null")
							type = B_NULL_TYPE;
						else if (string.IsNumber())
							type = B_INT32_TYPE;
						else if (is_float(string.String()))
							type = B_FLOAT_TYPE;
						else if(string == "true" || string == "false")
							type = B_BOOL_TYPE;
						else
							type = B_STRING_TYPE;
					}
				}

				value = SValue::String(string);
			}
			
			// look up the bvalue at the specified place in the catalog.
			if (lookup)
			{
				value = SValue::Undefined();
			}
			else if (type != value.Type())
			{
				switch (type)
				{
					case B_WILD_TYPE:
						value = B_WILD_VALUE;
						break;

					case B_UNDEFINED_TYPE:
						value = B_UNDEFINED_VALUE;
						break;

					case B_NULL_TYPE:
						value = B_NULL_VALUE;
						break;

					case B_STRING_TYPE:
						value = SValue::String(value.AsString());
						break;

					case B_INT32_TYPE:
						value = SValue::Int32(value.AsInt32());
						break;
					
					case B_INT64_TYPE:
						value = SValue::Int64(value.AsInt64());
						// !!! %s with empty string -> workaround for EABI changes
						// wrt. 64-bit vararg parameter alignment changes.
						//printf("%sConverted to int64_t: %lld\n", "", value.AsInt64());
						break;
					
					case B_STATUS_TYPE:
						value = SValue::Status(value.AsStatus());
						break;
					
					case B_NSECS_TYPE:
						value = SValue::Time(value.AsTime());
						// !!! %s with empty string -> workaround for EABI changes
						// wrt. 64-bit vararg parameter alignment changes.
						//printf("%sConverted to nsecs_t: %lld\n", "", value.AsTime());
						break;
					
					case B_BOOL_TYPE:
						value = SValue::Bool(value.AsBool());
						//bout << "bool: " << value << " " << SValue::Bool(true) << endl;
						break;
						
					case B_FLOAT_TYPE:
						value = SValue::Float(value.AsFloat());
						//bout << "float: " << value << " " << SValue::Float(0.5) << endl;
						break;

					case B_DOUBLE_TYPE:
						value = SValue::Double(value.AsDouble());
						//bout << "double: " << value << " " << SValue::Double(0.5) << endl;
						break;

					case B_RECT_TYPE:
#if 0
						value = BRect(value).AsValue();
#endif
						break;

					case B_BINDER_TYPE:
						value = SValue::Binder(value.AsBinder());
						break;

					case B_BINDER_WEAK_TYPE:
						value = SValue::WeakBinder(value.AsWeakBinder());
						break;
				}
			}
		}

		trim_whitespace(&index, data);
		
		if (data[index] == '{')
		{
			// we can disregard the value we created.
			value = make_value(shell, &index, data, length);
			trim_whitespace(&index, data);
		}
		
		if (data[index] == '-' && data[index+1] == '>')
		{
			state = MAPPING_STATE;
			keys.Push(value);
			value.Undefine();
			index += 2;
		}
		else
		{
			if (state == MAPPING_STATE)
			{
				// Collect all keys.
				size_t i = keys.CountItems();
				while (i > 0)
				{
					i--;
					value = SValue(keys[i], value, B_NO_VALUE_FLATTEN);
				}
				keys.MakeEmpty();
				state = SIMPLE_STATE;
			}

			switch (op)
			{
			case BEGIN_OP:
			case JOIN_OP:
				returned.Join(value);
				break;
			case OVERLAY_OP:
				returned.Overlay(value);
				break;
			case INHERIT_OP:
				returned.Inherit(value);
				break;
			case REMOVE_OP:
				returned.Remove(value);
				break;
			case MAP_VALUES_OP:
				returned.MapValues(value);
				break;
			}

			if (data[index] == ',')
			{
				op = JOIN_OP;
				index++;
			}
			else if (data[index] == '+')
			{
				if (data[index+1] == '>')
				{
					op = INHERIT_OP;
					index++;
				}
				else if (data[index+1] == '<')
				{
					op = OVERLAY_OP;
					index++;
				}
				else
				{
					op = JOIN_OP;
				}
				index++;
			}
			else if (data[index] == '-')
			{
				op = REMOVE_OP;
				index++;
			}
			else if (data[index] == '*')
			{
				op = MAP_VALUES_OP;
				index++;
			}
			else if (data[index] == '[')
			{
				SValue lookup = make_value(shell, &index, data, length);
				returned = returned[lookup];
				op = JOIN_OP;
			}
		}
	}
	
	(*outIndex) = index;
	return returned;
}

SValue make_value(const sptr<ICommand>& shell, const SString& string)
{
	int32_t index = 0;
	return make_value(shell, &index, string.String(), string.Length());
}

// ---------------------------------------------------------------------------------

const size_t kAllocSize = 512;

Lexer::Lexer(const sptr<ICommand>& shell, const sptr<ITextInput>& input, const sptr<ITextOutput>& output, bool interactive)
	:	m_shell(shell),
		m_textInput(input),
		m_textOutput(output),
		m_interactive(interactive),
		m_state(Lexer::SCAN_NORMAL),
		m_depth(0),
		m_index(0),
		m_tokenPos(0),
		m_buffer(NULL),
		m_bufferSize(kAllocSize)
{
	m_buffer = (char*)malloc((size_t)m_bufferSize);

	if (m_buffer)
		memset(m_buffer, 0, (size_t)m_bufferSize);
}

Lexer::~Lexer()
{
	if (m_buffer)
		free(m_buffer);
}

Lexer::Iterator Lexer::Buffer()
{
	if ((m_state & Lexer::STREAM_EOF) == 0 &&
			(m_buffer[m_tokenPos] == '\0' || (m_state & Lexer::APPEND_BUFFER) != 0)) {
		if (m_interactive) {
			// we need to read more input - prompt the user
			m_textOutput << expand_argument(m_shell, m_nextPrompt).AsString();
			m_textOutput->Flush();
			//printf("%s", (const char *)m_nextPrompt);
		
			// save the last prompt so if someone calls GetPrompt they
			// will get the last one that was printed out not the next one.
			m_lastPrompt = m_nextPrompt;
			
			// PS2 is used for every prompt but the first
			m_nextPrompt = m_ps2;
			//printf("nextpromt: %s", (const char *)m_nextPrompt);
		}
			
		// If we are in SCAN_NORMAL read in the buffer and reset the tokenpos.
		// if we are in any other state we want to append the to the current buffer.


		if (m_state == Lexer::SCAN_NORMAL) {
			// overwrite current buffer contents
			m_tokenPos = 0;
		}

		// note that ReadLineBuffer() always null-terminates the buffer
		if (m_bufferSize - m_tokenPos <= 1) {
			// We don't have room for anything, grow buffer by standard amount
			size_t size = m_bufferSize + kAllocSize;
			char* buffer = (char*) realloc(m_buffer, size);
			if (buffer != NULL) {
				m_buffer = buffer;
				m_bufferSize = size;
			}
			else {
				DbgOnlyFatalError("[bsh]: Lexer::Buffer(): Cannot allocate buffer for input!");
			}
		}

		ssize_t amount = m_textInput->ReadLineBuffer((m_buffer + m_tokenPos), (m_bufferSize - m_tokenPos));
		if (amount == 0) 
		{
			// the stream has run dry (or we are out of buffer room) - don't bother checking it again
			m_state |= Lexer::STREAM_EOF;
		}
	}

	return Iterator(m_buffer);
}

bool Lexer::HasMoreTokens()
{
	int token = NextToken();
	if (token == Lexer::END_OF_STREAM)
		return false;

	Rewind(1);
	return true;
}

void Lexer::SetPrompt(const sptr<ICommand>& shell)
{
	// don't bother if we're never gonna display any prompts
	if (m_interactive)
	{
		// remember the current value of PS2, in case we start processing a multiline cmd
		m_ps2 = shell->GetProperty(SValue::String("PS2")).AsString();

		// we are now at the beginning of a command - next time we prompt the user, use PS1
		m_nextPrompt = shell->GetProperty(SValue::String("PS1")).AsString();
	}
}

void Lexer::Rewind()
{
	Rewind(1);
}

void Lexer::Rewind(int rewind)
{
	//bout << "lexer rewind  " << rewind << endl;
	m_index -= rewind;

	if (m_index < 0) {
		DbgOnlyFatalError("Binder Shell lexer rewinding too far");
		m_index = 0;
	}

	//printf("Lexer: rewinding to token '%d' '%s'\n", m_tokenTypes.ItemAt(m_index), m_tokens.ItemAt(m_index));
}

int Lexer::NextToken()
{
	if (m_index <= (ssize_t)m_tokens.CountItems() - 1)
		m_index++;
	else
		ConsumeToken();

	//printf("Lexer: next token(%d) '%d' '%s'\n", m_index - 1, m_tokenTypes.ItemAt(m_index - 1), m_tokens.ItemAt(m_index - 1));
	return m_tokenTypes.ItemAt(m_index - 1);
}

SString Lexer::CurrentToken()
{
	int index = m_index - 1;

	if (index < 0)
		index = 0;

	if (m_tokens.CountItems() == 0)
		return B_EMPTY_STRING;

	//printf("Lexer: current token(%d) '%d' '%s'\n", index, m_tokenTypes.ItemAt(index), m_tokens.ItemAt(index));
	return m_tokens.ItemAt(index);
}

void Lexer::FlushTokens()
{
	if (m_index >= (ssize_t)m_tokens.CountItems()) {
		//bout << "bsh: flushing all " << m_index << " tokens" << endl;
		m_tokenTypes.MakeEmpty();
		m_tokens.MakeEmpty();
	} else {
		//bout << "bsh: flushing " << m_index << " tokens" << endl;
		ssize_t num = m_index;
		if (num > (ssize_t)m_tokenTypes.CountItems()) num = m_tokenTypes.CountItems();
		m_tokenTypes.RemoveItemsAt(0, num);

		num = m_index;
		if (num > (ssize_t)m_tokens.CountItems()) num = m_tokens.CountItems();
		m_tokens.RemoveItemsAt(0, num);
	}
	m_index = 0;
}

void Lexer::PrintToken()
{
#if 0
	int index = (m_index == 0) ? 0 : m_index - 1;
	if (m_tokens.CountItems() > 0)
	{
		bout << "Lexer: token#" << index << " " << m_tokens.ItemAt(index) << endl;
	}
#endif

	for (size_t i = 0 ; i < m_tokens.CountItems() ; i++)
	{
		bout << "token#" << i << "'" << m_tokens.ItemAt(i) << "'" << endl;
	}
}

void Lexer::ConsumeComments()
{
	ConsumeWhiteSpace();

	Iterator buffer = Buffer();

	if (buffer[m_tokenPos] == '#')
	{
		buffer[m_tokenPos] = 0x00;

		// now remove all "blank" lines (lines containing only comments and whitespace)
		while (true) {
			buffer = Buffer();
			if (buffer[0] == 0x00) {
				// we've run out input data - give up
				break;
			}
			
			while (isspace(buffer[m_tokenPos]))
				m_tokenPos++;

			if (buffer[m_tokenPos] == 0x00 || buffer[m_tokenPos] == '#') {
				// there's no data on this line - move to the next one
				buffer[m_tokenPos] = 0x00;

			} else {
				// this line has non-comment characters - leave it be
				break;
			}
		}
	}
}

void Lexer::ConsumeWhiteSpace()
{
	size_t size = Size();

	Iterator buffer = Buffer();
	for (size_t index = m_tokenPos ; index < size ; index++)
	{
		char c = buffer[m_tokenPos];
		
		if (c != '\t' && c != '\v' && c != ' ' && c != '\f') {
			break;
		}
		m_tokenPos++;
	}
}

// Parses the next token adding the token to m_tokens and the type to m_tokenTypes.
// m_index is equal to m_tokens.CountItems.
void Lexer::ConsumeToken()
{
	// remove all white space execpt \n
	ConsumeComments();

//	bout << "Lexer::ConsumeToken() buffer = " << Buffer() << endl;
//	bout << "Lexer::ConsumeToken() position = " << m_tokenPos << endl;
	Iterator line = Buffer();
	line += m_tokenPos;

	if (*line == 0)
	{
		m_tokens.AddItem(B_EMPTY_STRING);
		m_tokenTypes.AddItem(Lexer::END_OF_STREAM);
		m_index++;
		return;
	}

	size_t start = m_tokenPos;
	Iterator begin = line;
	Iterator end = begin;
	
	if (!ConsumeDelimeter() && !ConsumeKeyword())
	{
		m_state = Lexer::SCAN_NORMAL;
		int size = 0;
		int delimeter = Lexer::NOT_A_DELIMETER;
		
		do
		{
			// increment the size of the string that we want to copy.
			size++;
			bool escaped = false;

			if (end[0] == '\\')
			{
				escaped = true;
				// if a \n has been found then we need to read in more
				// of the line. we first need to set the state to
				// Lexer::APPEND_BUFFER so that the buffer will be
				// appended and not deleted.
				if (end[1] == '\n' && end[2] == '\0')
				{
					m_state |= Lexer::APPEND_BUFFER;
					line = Buffer();
					line += start;
					m_state &= ~Lexer::APPEND_BUFFER;

					// Buffer() just overwrote the '\\' - process
					// the character which took its place (which
					// could be another '\\')
					size--;
					continue;
				}
			}
			else if (m_state != Lexer::SINGLE_QUOTED && end[0] == '"')
			{
				m_state ^= Lexer::DOUBLE_QUOTED;
			}
			else if (m_state != Lexer::DOUBLE_QUOTED && end[0] == '\'')
			{
				m_state ^= Lexer::SINGLE_QUOTED;
			}
			else if (m_state == Lexer::SCAN_NORMAL)
			{
				if (end[0] == '$' && end[1] == '{')
				{
					m_state |= Lexer::PARAMETER_EXPANSION;
					m_depth++;

					// increment past the '{'.
					end++;
					m_tokenPos++;
					size++;
				}
				else if (end[0] == '$' && end[1] == '(')
				{
					m_state |= Lexer::COMMAND_EXPANSION;
					m_depth++;

					// increment past the '('.
					end++;
					m_tokenPos++;
					size++;
				}
				else if (end[0] == '$' && end[1] == '[')
				{
					m_state |= Lexer::VALUE_EXPANSION;
					m_depth++;

					// increment past the '['.
					end++;
					m_tokenPos++;
					size++;
				}
				else if (end[0] == '@' && end[1] == '{')
				{
					m_state |= Lexer::PARAMETER_SUBSITUTION;
					m_depth++;
					
					// nasty hack so to get around the '{'. which actaully turns out
					// to be a lot faster. I will probally change the others as well.
					end++;
					m_tokenPos++;
					size++;
				}
				else if (end[0] == '$')
				{
					// we found a varibale. it will either be in the form of $FOO or
					// $FOO.CallFoo() or $FOO.CallFoo(@{"with this arg"}).
					m_state |= Lexer::VARIABLE_EXPANSION;
					
//					bout << "starting = '" << end << "'" << endl;
					end++;
					m_tokenPos++;
					size++;
				}
			}
			else if (m_state == Lexer::VARIABLE_EXPANSION)
			{
//				bout << "end = '" << *end << "'" << endl;
				
				if (end[0] == '(')
				{
					m_depth++;
				}
				else if (end[0] == ')')
				{
					m_depth--;
				}
				
				// XXX Shouldn't variable parsing stop on any non-alphanumeric
				// character??
				if (m_depth == 0 && (IsSpace(end[0]) || end[0] < ' ' || end[0] == ';'))
				{
//					bout << "getting out of Lexer::VARIABLE_EXPANSION" << endl;
					m_state ^= Lexer::VARIABLE_EXPANSION;

					end--;
					m_tokenPos--;
					size--;
				}
			}
			else if (m_state == Lexer::PARAMETER_EXPANSION)
			{
				if (end[0] == '{')
				{
					m_depth++;
				}
				else if (end[0] == '}')
				{
					m_depth--;

					if (m_depth == 0)
						m_state ^= Lexer::PARAMETER_EXPANSION;
				}
			}
			else if (m_state == Lexer::COMMAND_EXPANSION)
			{
				if (end[0] == '(')
				{
					m_depth++;
				}
				else if (end[0] == ')')
				{
					m_depth--;

					if (m_depth == 0)
						m_state ^= Lexer::COMMAND_EXPANSION;
				}
			}
			else if (m_state == Lexer::VALUE_EXPANSION)
			{
				if (end[0] == '[')
				{
					m_depth++;
				}
				else if (end[0] == ']')
				{
					m_depth--;

					if (m_depth == 0)
						m_state ^= Lexer::VALUE_EXPANSION;
				}
			}
			else if (m_state == Lexer::PARAMETER_SUBSITUTION)
			{
				if (end[0] == '{')
				{
					m_depth++;
				}
				else if (end[0] == '}')
				{
					m_depth--;

					if (m_depth == 0)
					{
						m_state ^= Lexer::PARAMETER_SUBSITUTION;
						
						// oh I hate doing this. increment the pointers and
						// get the hell out of dodge! we have to do this because
						// the ending '}' will be interpreted as a token.
						end++;
						m_tokenPos++;
						// getting the hell out of dodge!
						break;
					}
				}
			}

			end++;
			m_tokenPos++;
			
			// if the next character is a delimiter we do not want to include it.
			delimeter = (escaped) ? false : IsDelimeter();
		} while (*end && (m_state != Lexer::SCAN_NORMAL || (!IsSpace(*end) && !delimeter && *end != '\n')));

		// determine if the the string is a WORD, ASSIGNMENT_WORD, NEWLINE, NAME 

		SString token = begin.AsString(size);
//		bout << "token(" << token.Length() << "): " << token << endl;

		if (token.IsNumber() && (delimeter == Lexer::LESS || delimeter == Lexer::GREATER))
		{
			m_tokens.AddItem(token);
			m_tokenTypes.AddItem(Lexer::IO_NUMBER);
		}
		else if (token.IsOnlyWhitespace())
		{
			m_tokens.AddItem(token);
			m_tokenTypes.AddItem(Lexer::NEWLINE);
		}
		else
		{
			// determine if it a WORD or and ASSIGNMENT_WORD
			
						// check to see if it is an ASSIGNMENT_WORD

			int equal = token.FindFirst("=");
	
			if (equal != -1 && equal != 0 && IsValidName(SString(token.String(), equal)))
				m_tokenTypes.AddItem(Lexer::ASSIGNMENT_WORD);
			else
				m_tokenTypes.AddItem(Lexer::WORD);

			m_tokens.AddItem(token);
		}
	}

	m_index++;
}

// return true for all white space execpt \n and \r
bool Lexer::IsSpace(int c)
{
	return (c == '\t' || c == '\v' || c == ' ' || c == '\f') ? true : false;
}

bool Lexer::IsValidName(const SString& token)
{
	char byte = token.ByteAt(0);
	bool valid = (isalpha(byte) || byte == '_');
	
	for (size_t i = 1 ; valid && (ssize_t)i < token.Length() ; i++)
	{
		byte = token[i];
		if (!isalpha(byte) && !isdigit(byte) && byte != '_')
		{
			valid = false;
		}
	}

	return valid;
}

bool Lexer::ConsumeKeyword()
{
	int increment = 0;

	Iterator line = Buffer();
	line += m_tokenPos;
	
	switch (line[0])
	{
		/* 'case' */
		case 'c':
		{
			if (line[1] == 'a' && line[2] == 's' && line[3] == 'e' && isspace(line[4]))
			{
				increment = 4;
				m_tokenTypes.AddItem(Lexer::CASE);
			}
			break;
		}

		/* 'else' 'elif' 'esac' 'export' */
		case 'e':
		{
			if (line[1] == 'l')
			{
				if (line[2] == 'i' && line[3] == 'f' && isspace(line[4]))
				{
					increment = 4;
					m_tokenTypes.AddItem(Lexer::ELIF);
				}
				else if(line[2] == 's' && line[3] == 'e' && isspace(line[4]))
				{
					increment = 4;
					m_tokenTypes.AddItem(Lexer::ELSE);
				}
			}
			else if (line[1] == 's' && line[2] == 'a' && line[3] == 'c' && isspace(line[4]))
			{
				increment = 4;
				m_tokenTypes.AddItem(Lexer::ESAC);
			}
			else if (line[1] == 'x' && line[2] == 'p' && line[3] == 'o'
					&& line[4] == 'r' && line[5] == 't' && isspace(line[6]))
			{
				increment = 6;
				m_tokenTypes.AddItem(Lexer::EXPORT);
			}

			break;
		}
		
		/* 'do' 'done' */
		case 'd':
		{
			if (line[1] == 'o' && line[2] == 'n' && line[3] == 'e' && isspace(line[4]))
			{
				increment = 4;
				m_tokenTypes.AddItem(Lexer::DONE);
			}
			else if (line[1] == 'o' && isspace(line[2]))
			{
				increment = 2;
				m_tokenTypes.AddItem(Lexer::DO);
			}
			break;
		}
		
		/* 'fi' 'for' */
		case 'f':
		{
			if (line[1] == 'i' && isspace(line[2]))
			{
				increment = 2;
				m_tokenTypes.AddItem(Lexer::FI);
			}
			else if (line[1] == 'o' && line[2] == 'r' && isspace(line[3]))
			{
				increment = 3;
				m_tokenTypes.AddItem(Lexer::FOR);
			}
			else if (line[1] == 'o' &&
					 line[2] == 'r' &&
					 line[3] == 'e' &&
					 line[4] == 'a' &&
					 line[5] == 'c' &&
					 line[6] == 'h' &&
					 isspace(line[7]))
			{
				increment = 7;
				m_tokenTypes.AddItem(Lexer::FOREACH);
			}
			else if (line[1] == 'u' && 
					 line[2] == 'n' && 
					 line[3] == 'c' &&
					 line[4] == 't' &&
					 line[5] == 'i' &&
					 line[6] == 'o' &&
					 line[7] == 'n' &&
					 isspace(line[8]))
			{
				increment = 8;
				m_tokenTypes.AddItem(Lexer::FUNCTION);
			}

			break;
		}
		
		/* 'if' 'in' */
		case 'i':
		{
			if (line[1] == 'f' && isspace(line[2]))
			{
				increment = 2;
				m_tokenTypes.AddItem(Lexer::IF);
			}
			else if (line[1] == 'n' && isspace(line[2]))
			{
				increment = 2;
				m_tokenTypes.AddItem(Lexer::IN);
			}
			break;
		}
		
		/* 'over' */
		case 'o':
		{
			if (line[1] == 'v' && line[2] == 'e' && line[3] == 'r' && isspace(line[4]))
			{
				increment = 4;
				m_tokenTypes.AddItem(Lexer::OVER);
			}
			break;
		}

		/* 'then' */
		case 't':
		{
			if (line[1] == 'h' && line[2] == 'e' && line[3] == 'n' && isspace(line[4]))
			{
				increment = 4;
				m_tokenTypes.AddItem(Lexer::THEN);
			}
			break;
		}

		case 'u':
		{
			if (line[1] == 'n' && line[2] == 't' && line[3] == 'i' && line[4] == 'l' && isspace(line[5]))
			{	
				increment = 5;
				m_tokenTypes.AddItem(Lexer::UNTIL);
			}

			break;
		}
		
		case 'w':
		{
			if (line[1] == 'h' && line[2] == 'i' && line[3] == 'l' && line[4] == 'e' && isspace(line[5]))
			{	
				increment = 5;
				m_tokenTypes.AddItem(Lexer::WHILE);
			}

			break;
		}
		
		case '!':
		{
			// do reconize '!=' as a delimeter
			if (line[1] != '=')
			{
				increment = 1;
				m_tokenTypes.AddItem(Lexer::BANG);
			}

			break;
		}

		case '\0':
		{
			increment = 0;
			
			m_tokens.AddItem(B_EMPTY_STRING);
			m_tokenTypes.AddItem(Lexer::END_OF_STREAM);
			
			break;
		}

		default:
			break;
	}

	bool is = increment > 0 ? true : false;

	if (is)
	{
		m_tokens.AddItem(line.AsString(increment));
		m_tokenPos += increment;
	}

	return is;
}

bool Lexer::ConsumeDelimeter()
{
	int increment = 0;
	bool consumeComments = false;

	int type = IsDelimeter();
	switch (type)
	{
		case Lexer::AND:
		case Lexer::OR:
		case Lexer::SEMI:
		case Lexer::LESS:
		case Lexer::GREATER:
		case Lexer::LBRACE:
		case Lexer::RBRACE:
		case Lexer::LPAREN:
		case Lexer::RPAREN:
		{
			increment = 1;
			break;
		}

		case Lexer::AND_IF:
		case Lexer::OR_IF:
		case Lexer::DSEMI:
		case Lexer::LESSAND:
		case Lexer::DLESS:
		case Lexer::LESSGREATER:
		case Lexer::DGREATER:
		case Lexer::GREATERAND:
		{
			increment = 2;
			break;
		}

		case Lexer::DLESSDASH:
		{
			increment = 3;
			break;
		}
		
		case Lexer::NEWLINE:
		{
			increment = 1;
			consumeComments = true;
			break;	
		}

		default:
		{
			increment = 0;
			break;
		}
	}

	if (increment > 0)
	{
		Iterator line = Buffer();
		line += m_tokenPos;
		m_tokens.AddItem(line.AsString(increment));
		m_tokenTypes.AddItem(type);
		m_tokenPos += increment;

		// we just devoured a newline there may be a comment.
//		if (consumeComments)
//			ConsumeComments();

		return true;
	}

	return false;
}

int Lexer::IsDelimeter()
{
	Iterator line = Buffer();
	line += m_tokenPos;

	int type;

	switch (line[0])
	{
		case '&':
		{
			if (line[1] == '&')
				type = Lexer::AND_IF;
			else
				type = Lexer::AND;

			break;
		}
		case '|':
		{
			if (line[1] == '|')
				type = Lexer::OR_IF;
			else
				type = Lexer::OR;

			break;
		}

		case ';':
		{
			if (line[1] == ';')
				type = Lexer::DSEMI;
			else
				type = Lexer::SEMI;

			break;
		}

		/* '<<' '<&' '<>' '<<-' */  
		case '<':
		{
			if (line[1] == '<') 
				type = Lexer::DLESS;
			else if (line[1] == '&')
				type = Lexer::LESSAND;
			else if (line[1] == '>')
				type = Lexer::LESSGREATER;
			else if (line[1] == '<' && line[2] == '-')
				type = Lexer::DLESSDASH;
			else
				type = Lexer::LESS;

			break;
		}
		
		/* '>>' '>&' */
		case '>':
		{
			if (line[1] == '>')
				type = Lexer::DGREATER;
			else if (line[1] == '&')
				type = Lexer::GREATERAND;
			else
				type = Lexer::GREATER;

			break;
		}

		case '{':
		{
			type = Lexer::LBRACE;
			break;
		}
		
		case '}':
		{
			type = Lexer::RBRACE;
			break;
		}
		
		case '(':
		{
			type = Lexer::LPAREN;
			break;
		}

		case ')':
		{
			type = Lexer::RPAREN;
			break;
		}

#if 0
		case '!':
		{
			// do reconize '!=' as a delimeter
			if (line[1] != '=')
				type = Lexer::BANG;
			else
				type = Lexer::NOT_A_DELIMETER;

			break;
		}
#endif

		case '\n':
		case '\r':
		{
			type = Lexer::NEWLINE;
			break;
		}

		case '\0':
		{
			type = Lexer::END_OF_STREAM;
			break;
		}

		default:
			type = Lexer::NOT_A_DELIMETER;
			break;
	}

	return type;
}

Parser::Parser(const sptr<BShell>& shell)
	:	m_shell(shell),
		m_commandDepth(0)
{
	
}

SValue Parser::Parse(const sptr<Lexer>& lexer)
{

	m_lexer = lexer;
	
	SValue returned;

	const sptr<ICommand> icommand(m_shell);
	
	while (true)
	{
		m_lexer->SetPrompt(m_shell.ptr());

		if (m_lexer->HasMoreTokens() == false)
			break;

		while (remove_newline())
		{
			m_lexer->SetPrompt(m_shell.ptr());
		}
		
		sptr<Command> command = make_compelete_command();
		m_lexer->FlushTokens();

		if (m_shell->GetTraceFlag()) 
		{
			command->Decompile(m_shell->TextError());
			m_shell->TextError() << endl;
		}

		bool doExit = false;
		returned = command->Evaluate(m_shell, icommand, &doExit);
		m_shell->SetLastResult(returned);
		if (m_lexer->IsInteractive()) 
		{
			if (m_shell->GetEchoFlag()) 
			{
				status_t err;
				ssize_t status = returned.AsSSize(&err);
				if (status != B_OK || err != B_OK) 
				{
					m_shell->TextOutput() << "Result: ";
					returned.PrintToStream(m_shell->TextOutput());
					m_shell->TextOutput() << endl;
				}
			}

			// unload any package that needs to be unloaded as 
			// a result of executing a command. Note we only
			// do this for interactive mode.
			SLooper* looper = SLooper::This();
			looper->ExpungePackages();
			looper->Process()->BatchPutReferences();
		}
		
		if (doExit)
		{
			break;
		}
	}
	
	return returned;
}

void Parser::ParseError()
{
	m_lexer->Rewind();

	m_token = m_lexer->NextToken();

	m_shell->TextOutput() << "bsh: syntax error near unexpected token '"; 
	
	if (m_token == Lexer::NEWLINE)
		m_shell->TextOutput() << "newline'" << endl;
	else
		m_shell->TextOutput() << m_lexer->CurrentToken() << "'" << endl;

#if BUILD_TYPE != BUILD_TYPE_RELEASE
	if (m_token == Lexer::NEWLINE)
		bout << "[PARSER]: error token=" << m_token <<  "'newline'" << endl;
	else
		bout << "[PARSER]: error token=" << m_token <<  "'" << m_lexer->CurrentToken().String() << "'" << endl;
#endif
}

bool Parser::remove_newline()
{
	bool newline = false;

	if (m_lexer->NextToken() == Lexer::NEWLINE)
		newline = true;
	else
		m_lexer->Rewind();

	return newline;
}

bool Parser::remove_newline_list()
{
	bool newline = false;
	
	while (m_lexer->NextToken() == Lexer::NEWLINE)
	{
		newline = true;
	}
	
	// move the lexer back one
	m_lexer->Rewind();

	return newline;
}

void Parser::remove_linebreak()
{
	remove_newline_list();
}

bool Parser::remove_separator_op()
{
	bool is = true;
	
	int m_token = m_lexer->NextToken();

	if (m_token != Lexer::AND && m_token != Lexer::SEMI)
	{
		m_lexer->Rewind();
		is = false;
	}

	return is;
}

bool Parser::remove_separator()
{
	bool removed = false;

	if (remove_separator_op())
	{
		remove_linebreak();
		removed = true;
	}
	else
	{
		removed = remove_newline_list();
	}

	return removed;
}

bool Parser::remove_sequential_separator()
{
	bool is = (m_lexer->NextToken() == Lexer::SEMI); 
	
	if (is)
	{
		remove_linebreak();
	}
	else
	{
		m_lexer->Rewind();
		is = remove_newline_list();
	}

	return is;
}


sptr<Command> Parser::make_compelete_command()
{
//	remove_newline_list();
	sptr<List> list = make_list();
	sptr<Commands> commands = new Commands(list);
	remove_newline();
	return commands.ptr();
}

sptr<List> Parser::make_list()
{
	sptr<List> list = new List();

	do
	{
		list->Add(make_and_or());
	} while (remove_separator_op());

	return list;
}

sptr<AndOr> Parser::make_and_or()
{
	sptr<AndOr> andor = NULL;

	while (true)
	{
		sptr<Pipeline> pipeline = make_pipeline();

		if (pipeline == NULL)
		{
			// none shall pass!
			break;
		}

		if (andor == NULL)
			andor = new AndOr();

		andor->Add(pipeline);
		
		if (m_lexer->IsInteractive()) m_shell->ByteOutput()->Sync();
		m_token = m_lexer->NextToken();
		
		if (m_token != Lexer::AND_IF && m_token != Lexer::OR_IF)
		{
			// we shall go not further
			m_lexer->Rewind();
			break;
		}
		else
		{
			andor->Add(m_token);
		}

		// remove optional linebreak
		remove_linebreak();
	}

	return andor;
}

sptr<Pipeline> Parser::make_pipeline()
{
	sptr<Pipeline> pipeline = NULL;

	m_token = m_lexer->NextToken();

	bool bang = false;
	
	if (m_token == Lexer::BANG)
		bang = true;
	else
		m_lexer->Rewind();

	while (true)
	{
		sptr<Command> command = make_command();
		
		if (command == NULL)
		{
			// i'm out of here!
			break;
		}

		if (pipeline == NULL)
			pipeline = new Pipeline();

		pipeline->Add(command);

		if (m_lexer->NextToken() != Lexer::OR)
		{
			// i am out of here!
			m_lexer->Rewind();
			break;
		}
			
		// remove optional linebreak
		remove_linebreak();	 
	}
	
	if (bang)
	{
		if (pipeline == NULL)
			m_lexer->Rewind();
		else
			pipeline->Bang();
	}

	return pipeline;
}

sptr<Command> Parser::make_brace_group()
{
	// increment the command depth since we are 
	// starting a new command.
	m_commandDepth++;
	sptr<CompoundList> list = make_compound_list();
	m_commandDepth--;
	
	sptr<BraceGroup> brace = new BraceGroup(list);

	if (m_lexer->NextToken() != Lexer::RBRACE)
	{
		ParseError();
		return NULL;
	}
	
	return brace.ptr();
}

sptr<Command> Parser::make_command()
{
	sptr<Command> command = NULL;
	m_token = m_lexer->NextToken();

	// compound_commands
	if (m_token == Lexer::LBRACE)
	{
		// brace_group
		command = make_brace_group();
	}
	else if (m_token == Lexer::LPAREN)
	{
		// subshell
		command = make_subshell();
	}
	else if (m_token == Lexer::IF)
	{
		command = make_if();
	}
	else if (m_token == Lexer::FOR)
	{
		// for clause
		command = make_for();
	}
	else if (m_token == Lexer::FOREACH)
	{
		// foreach clause
		command = make_for_each();
	}
	else if (m_token == Lexer::EXPORT)
	{
		// export clause
		command = make_command_prefix(true).ptr();
	}
	else if (m_token == Lexer::CASE)
	{
		// case clause
		command = make_case();
	}
	else if (m_token == Lexer::WHILE)
	{
		// while clause
		command = make_while();
	}
	else if (m_token == Lexer::UNTIL)
	{
		command = make_until();
	}
	else if (m_token == Lexer::ASSIGNMENT_WORD)
	{
		m_lexer->Rewind();
		command = make_simple_command();
	}
	else if (m_token == Lexer::FUNCTION)
	{
		command = make_function();
	}
	else if (m_token == Lexer::WORD)
	{
		if (m_lexer->NextToken() == Lexer::LPAREN && 
			m_lexer->NextToken() == Lexer::RPAREN)
		{
			m_lexer->Rewind(3);
			command = make_function();
		}
		else
		{
			// simple command
			m_lexer->Rewind(2);
			command = make_simple_command();
		}
	}
	else if (m_commandDepth > 0)
	{
		// we are in a command let the command deal with if 
		// it is a syntax error or not.
		m_lexer->Rewind();
	}
	else
	{
		// it is not a parse error if we reached the end 
		// of the stream and m_commandDepth <= 0
		if (m_token != Lexer::END_OF_STREAM &&
			m_token != Lexer::AND_IF &&
			m_token != Lexer::OR_IF)
		{
			ParseError();
		}

		if (m_token == Lexer::AND_IF ||
			m_token == Lexer::OR_IF)
		{
			m_lexer->Rewind();
		}

		command = NULL;
	}

	return command;
}

sptr<Command> Parser::make_subshell()
{
	sptr<CompoundList> list = make_compound_list();
	sptr<SubShell> subshell = new SubShell(list);
		
	if (m_lexer->NextToken() != Lexer::RPAREN)
	{
		ParseError();
		return NULL;
	}

	return subshell.ptr();
}

sptr<CompoundList> Parser::make_compound_list()
{
	sptr<CompoundList> list = NULL;
	
	// optional remove the newline list before term
	bool removed = remove_newline_list();

	sptr<AndOr> addme = NULL;
	
	do
	{
		addme = make_and_or();

		if (addme == NULL)
			break;
		
		if (list == NULL)
			list = new CompoundList();

		list->Add(addme);
	} while (remove_separator());


	if (list != NULL)
	{
		// there may be a trailing separator after term
		remove_separator();
	}
	else if (removed)
	{
		// this was not for us
		m_lexer->Rewind();
	}

	return list;
}

sptr<Command> Parser::make_for()
{
	sptr<For> forwho = new For();
	
	if (m_lexer->NextToken() != Lexer::WORD)
	{
		// parse error!
		ParseError();
		return NULL;
	}

	forwho->SetCondition(m_lexer->CurrentToken());
	
	// remove optional linebreak
	remove_linebreak();
	
	m_token = m_lexer->NextToken();

	if (m_token == Lexer::IN)
	{
		while (!remove_sequential_separator())
		{
			m_token = m_lexer->NextToken();
			if (m_token != Lexer::WORD)
			{
				// parse error!
				ParseError();
				return NULL;
			}
			forwho->Add(m_lexer->CurrentToken());
		}

		m_token = m_lexer->NextToken();
	}
	
	if (m_token != Lexer::DO)
	{
		//parse error!
		ParseError();
		return NULL;
	}

	m_commandDepth++;
	forwho->SetDoList(make_compound_list());
	m_commandDepth--;

	if (m_lexer->NextToken() != Lexer::DONE)
	{
		// parse error!
		ParseError();
		return NULL;
	}

	return forwho.ptr();
}

sptr<Command> Parser::make_for_each()
{
	sptr<Foreach> foreach = new Foreach();

	if (m_lexer->NextToken() != Lexer::WORD)
	{
		// parse error!
		ParseError();
		return NULL;
	}

	foreach->SetKeyVariable(m_lexer->CurrentToken());
	
	// remove optional linebreak
	remove_linebreak();
	m_token = m_lexer->NextToken();

	if (m_token != Lexer::IN && m_token != Lexer::OVER)
	{
		foreach->SetValueVariable(m_lexer->CurrentToken());
		
		// remove optional linebreak
		remove_linebreak();
		m_token = m_lexer->NextToken();
	}

	if (m_token == Lexer::IN || m_token == Lexer::OVER)
	{
		foreach->SetOverMode(m_token == Lexer::OVER);

		while (!remove_sequential_separator())
		{
			m_token = m_lexer->NextToken();
			if (m_token != Lexer::WORD)
			{
				// parse error!
				ParseError();
				return NULL;
			}
			foreach->Add(m_lexer->CurrentToken());
		}

		m_token = m_lexer->NextToken();
	}
	
	if (m_token != Lexer::DO)
	{
		//parse error!
		ParseError();
		return NULL;
	}

	m_commandDepth++;
	foreach->SetDoList(make_compound_list());
	m_commandDepth--;

	if (m_lexer->NextToken() != Lexer::DONE)
	{
		// parse error!
		ParseError();
		return NULL;
	}

	// The old implementation.  This syntax doesn't match
	// the 'for' syntax, which I think is wrong.
#if 0
	foreach->SetVariable(m_lexer->CurrentToken());
	
	m_token = m_lexer->NextToken();

	if (m_token != Lexer::LPAREN)
	{
		//parse error!
		ParseError();
		return NULL;
	}

	m_token = m_lexer->NextToken();

	while (m_token != Lexer::RPAREN)
	{
		if (m_token != Lexer::WORD)
		{
			//parse error!
			ParseError();
			return NULL;
		}

		foreach->Add(m_lexer->CurrentToken());
		m_token = m_lexer->NextToken();

	}

	// remove optional linebreak
	remove_linebreak();

	m_commandDepth++;
	foreach->SetCommand(make_command());
	m_commandDepth--;
#endif

	return foreach.ptr();
}

sptr<Command> Parser::make_case()
{
	sptr<Case> command = new Case();

	if (m_lexer->NextToken() != Lexer::WORD)
	{
		// parse error
		ParseError();
		return NULL;
	}

	command->SetLabel(m_lexer->CurrentToken());

	// remove optional linebreak from the stream
	remove_linebreak();

	if (m_lexer->NextToken() != Lexer::IN)
	{
		// parse error
		ParseError();
		return NULL;
	}

	// remove optional linebreak from the stream
	remove_linebreak();
	
	sptr<CaseList> prev = NULL;
	sptr<CaseList> list = NULL;

	bool dsemi = true;

	while (true)
	{
		m_token = m_lexer->NextToken();
	
		// this is applying rule 4. It may need to be checked 
		// after the first Lexer::LPAREN as well.
		if (m_token == Lexer::ESAC)
		{
			break;
		}

		if (!dsemi)
		{
			// we want to keep going but someone forgot the DSEMI
			ParseError();
			return NULL;
		}

		if (m_token == Lexer::LPAREN)
			m_token = m_lexer->NextToken();

		list = new CaseList();
		do
		{
			if (m_token != Lexer::WORD)
			{
				// parser error
				ParseError();
				return NULL;
			}
			list->AddPattern(m_lexer->CurrentToken());
			m_token = m_lexer->NextToken();
		} while (m_token != Lexer::RPAREN);


		//     pattern ')'					linebreak
		//     pattern ')' compound_list	linebreak
		// '(' pattern ')'					linebreak
		// '(' pattern ')' compound_list	linebreak	
		
		//     pattern ')' linebreak		DSEMI linebreak
		//     pattern ')' compound_list	DSEMI linebreak
		// '(' pattern ')' linebreak		DSEMI linebreak
		// '(' pattern ')' compound_list	DSEMI linebreak

		
		m_commandDepth++;
		sptr<CompoundList> compound = make_compound_list();
		m_commandDepth--;

		if (compound != NULL)
		{
			list->AddList(compound);
			m_token = m_lexer->NextToken();
			
			if (m_token != Lexer::DSEMI)
				dsemi = false;
			
			remove_linebreak();
		}
		else
		{
			remove_linebreak();
		
			m_token = m_lexer->NextToken();
				
			if (m_token != Lexer::DSEMI)
				dsemi = false;
			else
				remove_linebreak();
		}

		if (prev == NULL)
		{
			prev = list;
			command->SetCaseList(prev);
		}
		else
		{
			prev->SetNext(list);
			prev = list;
		}
	}

	return command.ptr();
}


sptr<Command> Parser::make_if()
{
	sptr<If> ifwhat = new If();

	m_commandDepth++;

	ifwhat->SetCondition(make_compound_list());

	if (m_lexer->NextToken() != Lexer::THEN)
	{
		// parse error!
		ParseError();
		m_commandDepth--;
		return NULL;
	}

	ifwhat->SetThen(make_compound_list());
	
	m_token = m_lexer->NextToken();
	
	if (m_token != Lexer::FI)
	{
		sptr<If> next = ifwhat;
		do
		{
			// rewind the lexer because we are going to check 
			// NextToken() in make_else()
			m_lexer->Rewind();
			sptr<If> elif = make_else();
			next->SetNext(elif.ptr());
			next = elif;
		} while (m_lexer->NextToken() != Lexer::FI);
	}

	m_commandDepth--;

	return ifwhat.ptr();
}

sptr<If> Parser::make_else()
{
	sptr<If> next = new If();

	m_token = m_lexer->NextToken();

	if (m_token == Lexer::ELIF)
	{
		next->SetCondition(make_compound_list());
		if (m_lexer->NextToken() == Lexer::THEN)
		{
			next->SetThen(make_compound_list());
		}
		else
		{
			m_lexer->Rewind();
		}
	}
	else if (m_token == Lexer::ELSE)
	{
		next->SetCondition(NULL);
		next->SetThen(make_compound_list());
	}
	else
	{
		ParseError();
		return NULL;
	}
	
	return next;
}

sptr<Command> Parser::make_until()
{
	sptr<Until> wild = new Until();

	m_commandDepth++;
	wild->SetCondition(make_compound_list());

	if (m_lexer->NextToken() != Lexer::DO)
	{
		//parse error!
		ParseError();
		m_commandDepth--;
		return NULL;
	}

	wild->SetDoList(make_compound_list());

	m_commandDepth--;

	if (m_lexer->NextToken() != Lexer::DONE)
	{
		// parse error!
		ParseError();
		return NULL;
	}

	return wild.ptr();
}

sptr<Command> Parser::make_while()
{
	sptr<While> wild = new While();

	m_commandDepth++;

	wild->SetCondition(make_compound_list());

	if (m_lexer->NextToken() != Lexer::DO)
	{
		//parse error!
		ParseError();
		m_commandDepth--;
		return NULL;
	}

	wild->SetDoList(make_compound_list());

	m_commandDepth--;

	if (m_lexer->NextToken() != Lexer::DONE)
	{
		// parse error!
		ParseError();
		return NULL;
	}

	return wild.ptr();
}

sptr<Command> Parser::make_simple_command()
{
	sptr<SimpleCommand> simple = new SimpleCommand();
	
	sptr<CommandPrefix> prefix = make_command_prefix(false);
	simple->SetPrefix(prefix);

	m_token = m_lexer->NextToken();

	if (m_token == Lexer::WORD)
	{
		simple->SetCommand(m_lexer->CurrentToken());
	
		sptr<CommandSuffix> suffix = make_command_suffix();
		simple->SetSuffix(suffix);
	}
	else
	{
		// be kind rewind!
		m_lexer->Rewind();
	}

	return simple.ptr();
}

sptr<CommandSuffix> Parser::make_command_suffix()
{
	sptr<CommandSuffix> suffix = new CommandSuffix();
	
	sptr<IORedirect> tierOne = NULL;
	sptr<IORedirect> tierTwo = NULL;

	while (true)
	{
		tierTwo = make_io_redirect();
		
		if (tierTwo == NULL)
		{
			// check to see if it is an ASSIGNMENT_WORD
			m_token = m_lexer->NextToken();

			if (m_token != Lexer::WORD && 
				m_token != Lexer::ASSIGNMENT_WORD &&
				m_token != Lexer::BANG &&
				m_token != Lexer::LBRACE &&
				m_token != Lexer::RBRACE &&
				(m_token & Lexer::KEY_WORD) != Lexer::KEY_WORD)
			{
				// not for me
				m_lexer->Rewind();
				break;
			}
			
			//bout << "[Parser] adding '" << m_lexer->CurrentToken() << "'" << endl;
			suffix->AddWord(m_lexer->CurrentToken());
		}
		else
		{
			if (!suffix->HasRedirect())
			{
				suffix->SetRedirect(tierTwo);
			}

			if (tierOne != NULL)
				tierOne->SetNext(tierTwo);

			tierOne = tierTwo;
		}
	}

	return (suffix->HasWord() || suffix->HasRedirect()) ? suffix : (sptr<CommandSuffix>) NULL;
}

sptr<CommandPrefix> Parser::make_command_prefix(bool isExport)
{
	sptr<CommandPrefix> prefix = new CommandPrefix();
	prefix->SetExport(isExport);

	sptr<IORedirect> tierOne = NULL;
	sptr<IORedirect> tierTwo = NULL;

	while (true)
	{
		if (!isExport)
			tierTwo = make_io_redirect();
		
		if (tierTwo == NULL)
		{
			// check to see if it is an ASSIGNMENT_WORD
			m_token = m_lexer->NextToken();			
			if (m_token != Lexer::ASSIGNMENT_WORD)
			{
				// not for me
				m_lexer->Rewind();
				break;
			}
					
			prefix->AddAssignment(m_lexer->CurrentToken());
		}
		else
		{
			if (!prefix->HasRedirect())
			{
				prefix->SetRedirect(tierTwo);
			}

			if (tierOne != NULL)
				tierOne->SetNext(tierTwo);

			tierOne = tierTwo;
		}
	}

	return (prefix->HasAssignment() || prefix->HasRedirect()) ? prefix : (sptr<CommandPrefix>) NULL;
}

sptr<IORedirect> Parser::make_io_redirect()
{
	sptr<IORedirect> redirect = new IORedirect();

	m_token = m_lexer->NextToken();
	
	bool hadIONumber = false;
	bool isRedirect = true;
	
	if (m_token == Lexer::IO_NUMBER)
	{
		redirect->SetFileDescriptor(m_lexer->CurrentToken());
		m_token = m_lexer->NextToken();
		hadIONumber = true;
	}

	switch (m_token)
	{
		case Lexer::LESS:
		case Lexer::LESSAND:
		case Lexer::GREATER:
		case Lexer::GREATERAND:
		case Lexer::DGREATER:
		case Lexer::LESSGREATER:
		case Lexer::CLOBBER:
		case Lexer::DLESS:
		case Lexer::DLESSDASH:
		{
			redirect->SetDirection(m_token);
			break;
		}

		case Lexer::NEWLINE:
		{
			isRedirect = false;
			redirect = NULL;
			m_lexer->Rewind();
//			printf("Parser: make_io_redirect reached NEWLINE\n");
			break;
		}

		case Lexer::END_OF_STREAM:
		{
			isRedirect = false;
			redirect = NULL;
			m_lexer->Rewind();
//			printf("Parser: make_io_redirect reached END_OF_STREAM\n");
			break;
		}

		default:
		{
			m_token = m_lexer->NextToken();

			if (m_token != Lexer::NEWLINE || m_token != Lexer::END_OF_STREAM)
			{
				// rewind twice once for just checking token the other for checking for redirection.
				m_lexer->Rewind(2);
			}
			else
			{
//				printf("Parser: make_io_redirect reached NEWLINE or END_OF_STREAM\n");	
			}

			isRedirect = false;
			redirect = NULL;

			break;
		}
	}

	if (isRedirect)
	{
		m_token = m_lexer->NextToken();

		if (m_token != Lexer::WORD)
		{
			// parse error!
		}

		redirect->SetFileName(m_lexer->CurrentToken());
	}

	return redirect;
}

sptr<Command> Parser::make_function()
{
	sptr<Function> function = new Function();

	m_token = m_lexer->NextToken();

	if (m_token != Lexer::WORD)
	{
		ParseError();
		return NULL;
	}
		
	function->SetName(m_lexer->CurrentToken());

	// just double check to see if this really is a function definition

	if (m_lexer->NextToken() == Lexer::LPAREN && 
		m_lexer->NextToken() == Lexer::RPAREN)
	{
		// remove optional linebreak
		remove_linebreak();

		sptr<Command> command = make_command();

		if (command != NULL)
		{
			function->SetCommand(command);
			function->SetRedirectList(make_redirect_list());
		}
		else
		{
			ParseError();
			return NULL;
		}
	}
	else
	{
		// parse error
		ParseError();
		return NULL;
	}

	// TODO: store the function in the shell
	// m_shell->StoreCommand(function);

	return function.ptr();
}

sptr<RedirectList> Parser::make_redirect_list()
{
	sptr<RedirectList> list = new RedirectList();

	sptr<IORedirect> tierOne = NULL;
	sptr<IORedirect> tierTwo = NULL;

	while (true)
	{
		tierTwo = make_io_redirect();
		
		if (tierTwo == NULL)
		{
			// we are out of here
			break;
		}
		else
		{
			if (!list->HasRedirect())
			{
				// set the top level
				list->SetRedirect(tierTwo);
			}

			if (tierOne != NULL)
				tierOne->SetNext(tierTwo);

			tierOne = tierTwo;
		}
	}

	return (list->HasRedirect()) ? list : (sptr<RedirectList>) NULL;
}
