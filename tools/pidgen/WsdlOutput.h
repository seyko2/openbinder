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

#ifndef WSDL_OUTPUT_H
#define WSDL_OUTPUT_H

#include "WSDL.h"

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

class WsdlType;

class NamedType : public SAtom
{
public:
	NamedType();
	NamedType(const SString& id, const SString& type);

	SString Id() const;
	SString Type() const;

	void Print() const;
	void Print(sptr<ITextOutput> stream) const;
	void PrintArg(sptr<ITextOutput> stream) const;

private:
	void set_type(const SString& type);

	SString m_id;
	SString m_type;
};


class Method : public SAtom
{
public:
	Method();
	Method(const SString& id, const SString& returnType, const SString& url, const SString& urn);

	ssize_t AddParam(const SString& id, const SString& type);
	size_t CountParams() const;
	sptr<NamedType> ParamAt(size_t index) const;

	SString MessageId() const;
	SString Id() const;
	SString ReturnType() const;
	SString Url() const;
	SString Urn() const;

	void Print() const;
	void PrintHeader(sptr<ITextOutput> stream) const;
	void PrintInterface(sptr<ITextOutput> stream) const;

private:
	void set_return_type(const SString& returnType);
	SString m_messageID;
	SString m_id;
	SString m_returnType;
	SString m_url;
	SString m_urn;
	SVector< sptr<NamedType> > m_params;
};

class WsdlClass : public SAtom
{
public:
	WsdlClass(const SString& id, const sptr<BWsdl>& wsdl);

	virtual void InitAtom();

	virtual SString Id() const;
	virtual SString Namespace() const;
	virtual SString InterfaceName() const;

	virtual ssize_t AddMethod(const sptr<Method>& method);
	virtual ssize_t AddMemeber(const sptr<NamedType>& type);
	virtual ssize_t	AddParent(const SString& parent);
	virtual ssize_t AddType(const sptr<WsdlType>& type);

	virtual size_t CountMethods() const;
	virtual sptr<Method> MethodAt(size_t index) const;

	virtual size_t CountMembers() const;
	virtual sptr<NamedType> MemberAt(size_t index) const;

	virtual size_t CountParents() const;
	virtual SString ParentAt(size_t index) const;

	virtual size_t CountTypes() const;
	virtual sptr<WsdlType> TypeAt(size_t index) const;
	virtual sptr<WsdlType> TypeFor(const SString& id, bool* found = NULL) const;

	virtual void Print() const;
	virtual void PrintHeader(sptr<ITextOutput> stream) const;
	virtual void PrintCPP(sptr<ITextOutput> stream) const;
	virtual void PrintInterface(sptr<ITextOutput> stream) const;
	
private:
	void populate_parents();

	SString m_id;
	SString m_namespace;
	SString m_interfaceName;
	SVector< sptr<Method> > m_methods;
	SVector< sptr<NamedType> > m_members;
	SKeyedVector<SString, sptr<WsdlType> > m_types;
	SVector<SString> m_parents;
	sptr<BWsdl> m_wsdl;
};

class WsdlType : public WsdlClass
{
public:
	WsdlType();
	WsdlType(const SString& id);

	virtual void Print() const;
	virtual void PrintHeader(sptr<ITextOutput> stream) const;
	virtual void PrintCPP(sptr<ITextOutput> stream) const;
	virtual void PrintInterface(sptr<ITextOutput> stream) const;

};

status_t wsdl_create_header(const sptr<WsdlClass>& classes, sptr<ITextOutput> stream, const SString& filename);
status_t wsdl_create_types_header(const sptr<WsdlClass>& classes, sptr<ITextOutput> stream, const SString& filename);
status_t wsdl_create_cpp(const sptr<WsdlClass>& classes, sptr<ITextOutput> stream, const SString& filename, const SString& header);
status_t wsdl_create_types_cpp(const sptr<WsdlClass>& classes, sptr<ITextOutput> stream, const SString& filename, const SString& header);
status_t wsdl_create_types_interface(const sptr<WsdlClass>& classes, sptr<ITextOutput> stream);
status_t wsdl_create_interface(const sptr<WsdlClass>& classes, sptr<ITextOutput> stream, const SString& typesIdl);
status_t create_wsdl_class(const sptr<BWsdl>& wsdl, sptr<WsdlClass>& obj);

#endif // WSDL_OUTPUT_H
