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

#ifndef OUTPUTUTIL_H
#define OUTPUTUTIL_H

#include <support/String.h>
#include <support/SortedVector.h>
#include "InterfaceRec.h"
#include "AST.h"

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

extern const char* gNativePrefix;
extern const char* gProxyPrefix;

status_t HeaderGuard(const SString &in, SString &out, bool system);

//returns appropriate type m_name for C++; may have TypeToJavaType, TypeToxxxType, etc later

// InterfaceRec can be NULL when we are within the class definition itself.
// (use kInsideClassScope)
// For all other uses, pass in owning class so that typedefs can be qualified appropriately
extern const InterfaceRec* kInsideClassScope;
SString TypeToCPPType(const InterfaceRec* rec, const sptr<IDLType>& obj, bool arg);

//returns correct default value for the given type.
SString TypeToDefaultValue(const InterfaceRec* rec, const sptr<IDLType>& obj);

//type->SValue

SString ToSValueConversion(const sptr<IDLType>& type, const SString &variable);
// Use FromValueExpression instead of directly calling FromSValueConversion
// SString FromSValueConversion(const sptr<IDLType>& type, const SString &variable, bool setError);

SString IndexToSValue(int32_t index);

enum ExpressionKind {
	INITIALIZE,				// BClass v = BClass(value, &err);
	ASSIGN,					// arg = BClass(value, &err);
	INDIRECT_ASSIGN,		// *arg = BClass(value, &err);
	RETURN					// return BClass(value, &err);
};

SString FromSValueExpression(const InterfaceRec* rec,
							 const sptr<IDLType>& type, 
							 const SString& varname, 
							 const SString& valuename, 
							 ExpressionKind kind,
							 bool setError);

void AddFromSValueStatements(const InterfaceRec* rec,
							 sptr<StatementList> statementList, 
							 const sptr<IDLType>& vartype,
							 const SString& varname,
							 const SString& valuename);

bool IsAutoMarshalType(const sptr<IDLType> &type);
bool IsAutobinderType(const sptr<IDLType> &type);

int32_t CountStringTabs(const SString& str, int32_t tabLen=4);
const char* PadString(const SString& str, int32_t fieldTabs, int32_t tabLen=4);

void CollectParents(InterfaceRec* rec, const SVector<InterfaceRec*>& classes, SVector<InterfaceRec*>* out);

struct NamespaceGenerator
{
public:
	NamespaceGenerator();
	~NamespaceGenerator();

	void EnterNamespace(const sptr<ITextOutput> stream, const SString& newNS, const SVector<SString>& newUsing);

	void CloseNamespace(const sptr<ITextOutput> steam);

private:
	SString prevNamespace;
	SSortedVector<SString> prevUsing;
	size_t namespaceDepth;
	bool first;
};

#endif // OUTPUTUTIL_H
