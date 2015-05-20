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

#ifndef AST_H
#define AST_H

#include <support/Atom.h>
#include <support/String.h>
#include <support/ITextStream.h>
#include <support/Vector.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

class Expression : public virtual SAtom
{
public:
					Expression(bool statement_requires_semicolon);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const = 0;
	virtual bool	StatementRequiresSemicolon();
private:
	bool m_statement_requires_semicolon;
};

class Comment : public Expression
{
public:
					Comment(const SString &str);
					Comment(const char *str);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SString m_str;
};

class Literal : public Expression
{
public:
					Literal(bool requires_semi, const char *fmt, ...);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SString m_value;
};

class StringLiteral : public Expression
{
public:
					StringLiteral(const SString &value, bool requires_semi = true);
					StringLiteral(const char *value, bool requires_semi = true);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SString m_value;
};

class IntLiteral : public Expression
{
public:
					IntLiteral(int value);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	int m_value;
};

class ArrayInitializer : public Expression
{
public:
					ArrayInitializer();
	void			AddItem(const sptr<Expression> &value);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SVector<sptr<Expression> > m_list;
};

enum {
	STATIC		= 0x00000001,
	CONST		= 0x00000002,
	EXTERN		= 0x00000004,
	VIRTUAL		= 0x00000008
};

class VariableDefinition : public Expression
{
public:
					// array is 0 for an unsized array and -1 for not an array
					VariableDefinition(const SString &type, const SString name,
							uint32_t cv = 0, const sptr<Expression> &initial = NULL,
							int array = -1, int pointer_indirection = 0);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SString m_type;
	SString m_name;
	uint32_t m_cv;
	sptr<Expression> m_initial;
	int m_array;
	int m_pointer;
};

class StatementList : public Expression
{
public:
					StatementList();
	void			AddItem(const sptr<Expression> &item);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SVector<sptr<Expression> > m_statements;
};

class FunctionPrototype : public Expression
{
public:
					FunctionPrototype(const SString &return_type, const SString &name, uint32_t linkage, uint32_t cv = 0, const SString & clas = SString(""));
	void			AddParameter(const SString &type, const SString &name);
	enum {
		nolinkage = 1,
		classname = 2
	};
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SString m_return_type;
	SString m_classname;
	SString m_name;
	uint32_t m_linkage;
	uint32_t m_cv;
	struct Param {
		SString type;
		SString name;
	};
	SVector<Param> m_params;
};

class Function : public StatementList
{
public:
					Function(const sptr<FunctionPrototype> &prototype);
	void			AddStatement(const sptr<Expression> &item);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	sptr<FunctionPrototype> m_prototype;
	SVector<sptr<Expression> > m_statements;
};

class Return : public Expression
{
public:
					Return(const sptr<Expression> &value);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	sptr<Expression> m_value;
};

class StaticCast : public Expression
{
public:
					StaticCast(const SString &type, int indirection,
								const sptr<Expression> value);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SString m_type;
	int m_indirection;
	sptr<Expression> m_value;
};

class FunctionCall : public Expression
{
public:
					FunctionCall(const SString &name);
					FunctionCall(const SString &namespac, const SString &name);
					FunctionCall(const SString &object, const SString &namespac, const SString &name);
	void			AddArgument(const sptr<Expression> &item);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SString m_object;
	SString m_namespace;
	SString m_name;
	SVector<sptr<Expression> > m_arguments;
};

class Assignment : public Expression
{
public:
					Assignment(const sptr<Expression> &lvalue, const sptr<Expression> &rvalue);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	sptr<Expression> m_lvalue;
	sptr<Expression> m_rvalue;
};

class Optional : public Expression
{
public:
					Optional(const sptr<Expression> &expr, bool initial, bool requires_semi = true);
	void			SetOutput(bool will_output);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
	virtual bool	StatementRequiresSemicolon();
private:
	sptr<Expression> m_expr;
	bool m_will_output;
};

class ClassDeclaration : public StatementList
{
public:
					ClassDeclaration(const SString &name);
	void			AddBaseClass(const SString &name, const SString &scope = SString("public"));
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SString m_name;
	struct base_class {
		SString name;
		SString scope;
	};
	SVector<base_class> m_base_classes;
};

class ParameterUse : public Expression
{
public:
					ParameterUse();
	void			AddParameter(const SString &name);
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
private:
	SVector<SString> m_expr;
};

class Null : public Expression
{
public:
					Null();
	virtual void	Output(const sptr<ITextOutput> &stream, int32_t flags=0) const;
};

#endif // AST_H
