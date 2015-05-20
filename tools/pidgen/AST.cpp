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

#include "AST.h"


Expression::Expression(bool statement_requires_semicolon)
	:SAtom(),
	 m_statement_requires_semicolon(statement_requires_semicolon)
{
}

bool
Expression::StatementRequiresSemicolon()
{
	return m_statement_requires_semicolon;
}

Comment::Comment(const SString &str)
	:Expression(false),
	 m_str(str)
{
}

Comment::Comment(const char *str)
	:Expression(false),
	 m_str(str)
{
}

void
Comment::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	stream << "/* " << m_str << " */";
}

Literal::Literal(bool requires_semi, const char *fmt, ...)
	:Expression(requires_semi)
{
	va_list ap;
	va_start(ap, fmt);
	char buffer[1024];
	vsprintf(buffer, fmt, ap);
	va_end(ap);
	m_value = buffer;
}

void
Literal::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	stream << m_value;
}

StringLiteral::StringLiteral(const SString &value, bool requires_semi)
	:Expression(requires_semi),
	 m_value(value)
{
}

StringLiteral::StringLiteral(const char *value, bool requires_semi)
	:Expression(requires_semi),
	 m_value(value)
{
}

void
StringLiteral::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	stream << m_value;
}


IntLiteral::IntLiteral(int value)
	:Expression(true),
	 m_value(value)
{
}

void
IntLiteral::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	stream << m_value;
}

ArrayInitializer::ArrayInitializer()
	:Expression(true)
{
}

void
ArrayInitializer::AddItem(const sptr<Expression> &value)
{
	m_list.AddItem(value);
}

void
ArrayInitializer::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	stream << "{";
	size_t count = m_list.CountItems();
	for (size_t i=0; i<count; i++) {
		m_list[i]->Output(stream);
		if (i != count-1) {
			stream << ",";
		}
	}
	stream << "}";
}

VariableDefinition::VariableDefinition(const SString &type, const SString name,
							uint32_t cv, const sptr<Expression> &initial,
							int array, int pointer_indirection)
	:Expression(true),
	 m_type(type),
	 m_name(name),
	 m_cv(cv),
	 m_initial(initial),
	 m_array(array),
	 m_pointer(pointer_indirection)
{
}

void
VariableDefinition::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	if (m_cv & VIRTUAL) {
		stream << "virtual ";
	}
	if (m_cv & CONST) {
		stream << "const ";
	}
	if (m_cv & STATIC) {
		stream << "static ";
	}
	stream << m_type;
	stream << " ";
	for (int i=0; i<m_pointer; i++) {
		stream << "*";
	}
	stream << m_name;
	if (m_array == 0) {
		stream << "[]";
	}
	else if (m_array != -1) {
		stream << "[" << m_array << "]";
	}
	if (m_initial != NULL) {
		stream << " = ";
		m_initial->Output(stream);
	}
}

StatementList::StatementList()
	:Expression(false)
{
}

void
StatementList::AddItem(const sptr<Expression> &item)
{
	m_statements.AddItem(item);
}

void
StatementList::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	size_t count = m_statements.CountItems();
	for (size_t i=0; i<count; i++) {
		sptr<Expression> e = m_statements[i];
		e->Output(stream);
		if (e->StatementRequiresSemicolon()) {
			stream << ";";
		}
		if (dynamic_cast<StatementList*>(e.ptr()) == NULL) {
			stream << endl;
		}
	}
}

FunctionPrototype::FunctionPrototype(const SString &return_type, const SString &name, uint32_t linkage,
									 uint32_t cv, const SString & clas)
	:Expression(true),
	 m_return_type(return_type),
	 m_name(name),
	 m_classname(clas),
	 m_linkage(linkage),
	 m_cv(cv)
{
}

void
FunctionPrototype::AddParameter(const SString &type, const SString &name)
{
	Param p;
	p.type = type;
	p.name = name;
	m_params.AddItem(p);
}

void
FunctionPrototype::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	if (!(flags & nolinkage)) {
		if (m_linkage & VIRTUAL) {
			stream << " virtual ";
		}
		if (m_linkage & EXTERN) {
			stream << "extern ";
		}
		if (m_linkage & STATIC) {
			stream << "static ";
		}
	}
	stream << m_return_type << " ";
	if (flags & classname && m_classname != "") {
		stream << m_classname << "::";
	}
	stream << m_name << "(";
	size_t count = m_params.CountItems();
	for (size_t i=0; i<count; i++) {
		const Param &p = m_params[i];
		stream << p.type << " " << p.name;
		if (i != count-1) {
			stream << ",";
		}
	}
	stream << ") ";
	if (m_cv & CONST) {
		stream << " const ";
	}
}

Function::Function(const sptr<FunctionPrototype> &prototype)
	:StatementList(),
	 m_prototype(prototype)
{
}

void
Function::AddStatement(const sptr<Expression> &item)
{
	m_statements.AddItem(item);
}

void
Function::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	m_prototype->Output(stream, FunctionPrototype::nolinkage|FunctionPrototype::classname);
	stream << endl << "{" << endl << indent;
	StatementList::Output(stream);
	stream << dedent << "}" << endl;
}

Return::Return(const sptr<Expression> &value)
	:Expression(true),
	 m_value(value)
{
}

void
Return::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	stream << "return ";
	m_value->Output(stream);
}

StaticCast::StaticCast(const SString &type, int indirection,
								const sptr<Expression> value)
	:Expression(true),
	 m_type(type),
	 m_indirection(indirection),
	 m_value(value)
{
}

void
StaticCast::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	stream << "static_cast<" << m_type;
	for (int i=0; i<m_indirection; i++) {
		stream << "*";
	}
	stream << ">(";
	m_value->Output(stream);
	stream << ")";
}

FunctionCall::FunctionCall(const SString &name)
	:Expression(true),
	 m_object(),
	 m_namespace(),
	 m_name(name)
{
}

FunctionCall::FunctionCall(const SString &namespac, const SString &name)
	:Expression(true),
	 m_object(),
	 m_namespace(namespac),
	 m_name(name)
{
}

FunctionCall::FunctionCall(const SString &object, const SString &namespac, const SString &name)
	:Expression(true),
	 m_object(object),
	 m_namespace(namespac),
	 m_name(name)
{
}

void
FunctionCall::AddArgument(const sptr<Expression> &item)
{
	m_arguments.AddItem(item);
}

void
FunctionCall::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	if (m_object != "") {
		stream << m_object << "->";
	}
	if (m_namespace != "") {
		stream << m_namespace << "::";
	}
	stream << m_name << "(";
	size_t count = m_arguments.CountItems();
	for (size_t i=0; i<count; i++) {
		m_arguments[i]->Output(stream);
		if (i != count-1) {
			stream << ",";
		}
	}
	stream << ")";
}

Assignment::Assignment(const sptr<Expression> &lvalue, const sptr<Expression> &rvalue)
	:Expression(true),
	 m_lvalue(lvalue),
	 m_rvalue(rvalue)
{
}


void
Assignment::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	m_lvalue->Output(stream);
	stream << " = ";
	m_rvalue->Output(stream);
}

Optional::Optional(const sptr<Expression> &expr, bool initial, bool requires_semi)
	:Expression(requires_semi),
	 m_expr(expr),
	 m_will_output(initial)
{
}

void
Optional::SetOutput(bool will_output)
{
	m_will_output = will_output;
}

void
Optional::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	if (m_will_output) {
		m_expr->Output(stream);
	}
}

bool
Optional::StatementRequiresSemicolon()
{
	return m_will_output && Expression::StatementRequiresSemicolon();
}

ClassDeclaration::ClassDeclaration(const SString &name)
	:StatementList(),
	 m_name(name),
	 m_base_classes()
{
}

void
ClassDeclaration::AddBaseClass(const SString &name, const SString &scope)
{
	base_class bc;
		bc.name = name;
		bc.scope = scope;
	m_base_classes.AddItem(bc);
}

void
ClassDeclaration::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	stream << "class " << m_name;
	size_t base_count = m_base_classes.CountItems();
	if (base_count > 0) {
		stream << ":";
	}
	for (size_t i=0; i<base_count; i++) {
		stream << " " << m_base_classes[i].scope << " " << m_base_classes[i].name;
		if (i != base_count-1) {
			stream << ",";
		}
	}
	stream << endl << "{" << endl;
	stream << indent;
	StatementList::Output(stream);
	stream << dedent;
	stream << "};" << endl;
}

ParameterUse::ParameterUse()
			:Expression(false)
{
}

void
ParameterUse::AddParameter(const SString &name)
{
	m_expr.AddItem(name);
}

void
ParameterUse::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	size_t count = m_expr.CountItems();
	for (size_t i = 0; i < count; i++) {
		stream << "(void)" << m_expr[i] << "; ";
	}
}

Null::Null()
	:Expression(true)
{
}

void
Null::Output(const sptr<ITextOutput> &stream, int32_t flags) const
{
	stream << "NULL";
}
