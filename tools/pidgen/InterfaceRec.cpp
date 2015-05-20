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

#include "InterfaceRec.h"
#include <ctype.h>

jnamedtype::jnamedtype()
	: SAtom()
{
}

jnamedtype::jnamedtype(SString id, SString type)
	: SAtom(), m_id(id), m_type(type)
{
}

jnamedtype::jnamedtype(const sptr<const jnamedtype>& orig)
	: SAtom(),
	m_id(orig->m_id),
	m_type(orig->m_type)
{
}

jmember::jmember()
	: SAtom()
{
}

jmember::jmember(SString id, SString rtype)
	: SAtom(), m_id(id), m_returnType(rtype)
{
}

jmember::jmember(const sptr<const jmember>& orig)
	: SAtom(),
	m_id(orig->m_id), 
	m_returnType(orig->m_returnType),
	m_params(orig->m_params)
{
}

status_t
jmember::AddParam(SString id, SString type)
{
	int32_t i, count = CountParams();
	for (i=0; i<count; i++) {
		if (ParamAt(i)->m_id == id)
			return B_NAME_IN_USE;
	}

	sptr<jnamedtype> j=new jnamedtype(id, type);
	m_params.AddItem(j);	
	
	return B_OK;
}

int32_t
jmember::CountParams()
{
	return m_params.CountItems();
}

sptr<jnamedtype>
jmember::ParamAt(int32_t i)
{
	return m_params.ItemAt(i);
}

SString
jmember::ID()
{
	return m_id;
}

SString
jmember::ReturnType()
{
	return m_returnType;
}


IDLType::IDLType()
	: SAtom()
{	
	m_code=B_UNDEFINED_TYPE;
	m_attributes = kNoAttributes;
}

IDLType::IDLType(SString name)
	: SAtom(), m_code(B_UNDEFINED_TYPE), m_name(name), m_attributes(kNoAttributes)
{ 	
	#ifdef TYPEDEBUG	
		bout << "<----- interfacerec.cpp -----> IDLType Constructor called" << endl;
		bout << "<----- interfacerec.cpp ----->		m_name=" << m_name << endl;
	#endif
}

IDLType::IDLType(SString name, uint32_t code)
	: SAtom(), m_code(code), m_name(name), m_attributes(kNoAttributes)
{ 	
	#ifdef TYPEDEBUG	
		bout << "<----- interfacerec.cpp -----> IDLType Constructor called" << endl;
		bout << "<----- interfacerec.cpp ----->		m_name=" << name << endl;
		bout << "<----- interfacerec.cpp ----->		m_code=" << code << endl;
	#endif
}

IDLType::IDLType(SString name, uint32_t code, uint32_t attr)
	: SAtom(), m_code(code), m_name(name), m_attributes(attr)
{ 	
	#ifdef TYPEDEBUG	
		bout << "<----- interfacerec.cpp -----> IDLType Constructor called" << endl;
		bout << "<----- interfacerec.cpp ----->		m_name=" << m_name << endl;
		bout << "<----- interfacerec.cpp ----->		m_attributes=" << m_attributes << endl;
	#endif
}


IDLType::IDLType(const sptr<const IDLType>& orig)
	: SAtom(),
	m_code(orig->m_code),
	m_name(orig->m_name), 
	m_attributes(orig->m_attributes), 
	m_iface(orig->m_iface),
	m_primitive(orig->m_primitive),
	typemembers(orig->typemembers)
{
}

status_t
IDLType::SetCode(uint32_t code)
{  
	m_code=code;	
	return B_OK;
}

status_t
IDLType::SetName(SString name)
{	
	m_name=name;
	return B_OK;
}

status_t
IDLType::SetAttribute(AttributeKind attr)
{	
	// we have to do extra work if we get a direction attribute
	// because those are exclusive
	if ((attr & kDirectionMask) != 0) {
		m_attributes &= ~kDirectionMask;
	}
	m_attributes |= attr;
	return B_OK;
}

status_t
IDLType::AddAttributes(uint32_t attributes)
{	
	// AddAttributes needs to be careful with the directional
	// attributes, because those are exclusive and not

	uint32_t attributeDirection = attributes & kDirectionMask;
	if (attributeDirection != 0) {
		// we have a direction attribute, so set it
		// (which will take are of handling exclusive)
		this->SetAttribute((AttributeKind)attributeDirection);
	}
	else {
		// some other attribute - just or it in
		m_attributes |= attributes;
	}

	return B_OK;
}

status_t
IDLType::SetAttributes(uint32_t attributes)
{	
	m_attributes = attributes;
	return B_OK;
}

status_t
IDLType::SetIface(SString typeiface)
{
	m_iface=typeiface;
	return B_OK;
}

status_t
IDLType::SetPrimitiveName(SString name)
{
	m_primitive = name;
	return B_OK;
}

status_t
IDLType::AddMember(sptr<jmember> typemem)
{
	typemembers.AddItem(typemem);
	return B_OK;
}

uint32_t
IDLType::GetCode() const
{	
	return m_code;
}

SString
IDLType::GetName() const
{	
	return m_name;
}

bool
IDLType::HasAttribute(AttributeKind attr) const
{
	// I don't just do != 0 because in+out==inout
	// so I don't want to get a false positive when
	// looking for inout
	return ((m_attributes & attr) == attr);
}

uint32_t
IDLType::GetAttributes() const
{
	return m_attributes;
}

uint32_t
IDLType::GetDirection() const
{
	return (m_attributes & kDirectionMask);
}

SString
IDLType::GetIface() const
{	
	return m_iface;
}

SString
IDLType::GetPrimitiveName() const
{	
	return m_primitive;
}

sptr<jmember>
IDLType::GetMemberAt(int32_t i) const
{	
	return typemembers.ItemAt(i);
}

int32_t							
IDLType::CountMembers()
{	
	return typemembers.CountItems();
}

IDLCommentBlock::IDLCommentBlock() : m_comments()
{
}


void
IDLCommentBlock::AddComment(const SString& comment)
{
	m_comments.AddItem(comment);
}

void
IDLCommentBlock::AppendToComment(const SString& more)
{
	// Get the last comment we added and append to it
	// If the user calls AppendToComment before
	// any AddComment - then just have this start
	// off the new comment
	size_t lastItem = m_comments.CountItems();
	if (lastItem == 0) {
		m_comments.AddItem(SString::EmptyString());
		lastItem = 1;
	}
	lastItem -= 1;
	m_comments.EditItemAt(lastItem).Append(more);
}


void
IDLCommentBlock::Output(const sptr<ITextOutput> &stream, bool startWithTab) const
{
	// Output comments in order that they were
	// added to our comment block.
	// Always start the first line with a tab, and
	// always append newline after each comment

	size_t count = m_comments.CountItems();
	for (size_t i = 0; i < count; i++) {
		if (startWithTab == true) {
			stream << "\t";
		}
		stream << m_comments[i] << endl;
	}
}

IDLNameType::IDLNameType() 
	: IDLType()
{
}

IDLNameType::IDLNameType(SString id, sptr<IDLType> typeptr, const sptr<IDLCommentBlock>& comment, bool custom)
	: IDLType(), 
	m_id(id),
	m_type(typeptr), 
	m_comment(comment), 
	m_custom(custom)
{
}

IDLNameType::IDLNameType(const sptr<const IDLNameType>& orig)
	: IDLType(orig.ptr()), 
	m_id(orig->m_id), 
	m_type(orig->m_type), 
	m_comment(orig->m_comment),
	m_custom(orig->m_custom)
{
}

void
IDLNameType::OutputComment(const sptr<ITextOutput> &stream, bool startWithTab)
{
	if (m_comment != NULL) {
		m_comment->Output(stream, startWithTab);
	}
}

bool
IDLNameType::HasComment() const
{
	return m_comment != NULL;
}

IDLTypeScope::IDLTypeScope()
	: IDLNameType()
{
}

IDLTypeScope::IDLTypeScope(SString id, const sptr<IDLCommentBlock>& comment)
	: IDLNameType(id, NULL, comment), m_autobinder(false)
{
}

IDLTypeScope::IDLTypeScope(const sptr<const IDLTypeScope>& orig)
	: IDLNameType(orig.ptr()),
	 m_params(orig->m_params), 
	 m_autobinder(false)
{
}

status_t
IDLTypeScope::AddParam(SString id, const sptr<IDLType>& typeptr, const sptr<IDLCommentBlock>& comment)
{
	int32_t i, count = CountParams();
	for (i=0; i<count; i++) {
		if (ParamAt(i)->m_id == id) {
			return B_NAME_IN_USE;
		}
	}
	
	sptr<IDLNameType> newItem= new IDLNameType(id, typeptr, comment);
	m_params.AddItem(newItem);
	return B_OK;
}

int32_t
IDLTypeScope::CountParams()
{
	return m_params.CountItems();
}

sptr<IDLNameType>
IDLTypeScope::ParamAt(int32_t i)
{
	return m_params.ItemAt(i);
}

SString
IDLTypeScope::ID()
{
	return m_id;
}

status_t
IDLTypeScope::SetAutoBinder(bool t)
{
	m_autobinder=t;
	return B_OK;
}

bool
IDLTypeScope::AutoBinder()
{
	return m_autobinder;
}


IDLMethod::IDLMethod()
	: IDLNameType()
{
	m_const=false;
}

IDLMethod::IDLMethod(SString id, const sptr<IDLType>& typeptr, const sptr<IDLCommentBlock>& comment, bool isconst)
	: IDLNameType(id, NULL, comment), 
	m_returnType(typeptr), 
	m_autobinder(false),
	m_const(isconst)
{
}


IDLMethod::IDLMethod(const sptr<const IDLMethod>& orig, bool isconst)
	: IDLNameType(orig.ptr()),
	 m_returnType(orig->m_returnType),
	 m_params(orig->m_params),
	 m_autobinder(false),
	 m_const(isconst)
{
}

status_t
IDLMethod::AddParam(SString id, const sptr<IDLType>& typeptr, const sptr<IDLCommentBlock>& comment)
{
	int32_t i, count = CountParams();
	for (i=0; i<count; i++) {
		if (ParamAt(i)->m_id == id) {
			return B_NAME_IN_USE;
		}
	}

	sptr<IDLNameType> newItem= new IDLNameType(id, typeptr, comment);
	m_params.AddItem(newItem);	
	
	return B_OK;
}

int32_t
IDLMethod::CountParams()
{
	return m_params.CountItems();
}

sptr<IDLNameType> 
IDLMethod::ParamAt(int32_t i)
{
	return m_params.ItemAt(i);
}

SString
IDLMethod::ID()
{
	return m_id;
}

sptr<IDLType>
IDLMethod::ReturnType()
{
	return m_returnType;
}

status_t
IDLMethod::SetAutoBinder(bool t)
{
	m_autobinder=t;
	return B_OK;
}

bool
IDLMethod::AutoBinder()
{
	return m_autobinder;
}

status_t
IDLMethod::SetConst(bool t)
{
	m_const=t;
	return B_OK;
}

bool
IDLMethod::IsConst() const
{
	return m_const;
}

void
IDLMethod::AddTrailingComment(const sptr<IDLCommentBlock>& comment)
{
	m_trailing_comment = comment;
}

void
IDLMethod::OutputTrailingComment(const sptr<ITextOutput> &stream) const
{
	if (m_trailing_comment != NULL) {
		m_trailing_comment->Output(stream);
	}
}

bool
IDLMethod::HasTrailingComment() const
{
	return m_trailing_comment != NULL;
}

InterfaceRec::InterfaceRec()
			: m_attributes(kNoAttributes)
{
}

InterfaceRec::InterfaceRec(SString aid, SString nspace, SVector<SString> cppnspace, const sptr<IDLCommentBlock>& aComment, dcltype adecl)
	: m_id(aid), 
	m_namespace(nspace), 
	m_cppNamespace(cppnspace),
	m_comment(aComment), 
	m_declared(adecl),
	m_attributes(kNoAttributes)
{
}

SString
InterfaceRec::ID() const
{
	return m_id;
}

SString
InterfaceRec::Namespace()  const
{
	return m_namespace;
}

SVector<SString>
InterfaceRec::CppNamespace()  const
{
	return m_cppNamespace;
}

dcltype
InterfaceRec::Declaration() const
{	
	return m_declared;
}

SString
InterfaceRec::FullInterfaceName() const
{	
	SString s(m_namespace);

	s.ReplaceAll("::", ".");
	s.Append('.', 1);
	s.Append(m_id);
	return s;
}

bool
InterfaceRec::InNamespace() const
{
	return m_namespace.Length() > 0;
}

SString
InterfaceRec::FullClassName(const SString &classPrefix) const
{
/*
	if (m_cppNamespace.Length() == 0) return m_id;
	SString s(m_cppNamespace);
	s.Append(':', 2);
	s.Append(classPrefix);
	s.Append(m_id);
	return s;
*/
	if (m_namespace.Length() == 0) {
		return m_id;
	}
	else {	
		SString noid=m_id;
		noid.RemoveFirst("I");	

		SString s("BNS(::");
		s.Append(m_namespace);
		s.Append("::) ");
		s.Append(classPrefix);
		s.Append(noid);
		return s;
	}
}

SVector<SString>
InterfaceRec::Parents() const
{	
	return m_parents;
}

bool
InterfaceRec::HasMultipleBases() const
{
	return m_parents.CountItems() > 1;
}

SString
InterfaceRec::LeftMostBase() const
{
	// in the case of single (or no) inheritance
	// LeftMostBase returns the class name itself
	SString left_most_base;
	if (this->HasMultipleBases()) {
		left_most_base = m_parents[0];
	}
	else {
		left_most_base = ID();
	}
	return left_most_base;
}

status_t						
InterfaceRec::AddCppNamespace(SString cppn)
{
	m_cppNamespace.AddItem(cppn);
	return B_OK;
}

status_t					
InterfaceRec::AddProperty(SString aId, const sptr<IDLType>& aType, const sptr<IDLCommentBlock>& aComment, bool aCustom)
{		
	if (aId.Length() <= 0) return B_BAD_VALUE;
	
	if (look_in_properties(aId)) {
		berr << "*** DUPLICATE PROPERTY: " << aId << endl;
		return B_NAME_IN_USE;
	}
	
	SString n = aId;
	int32_t l = n.Length();
	SString tmp;
	
	char * p = n.LockBuffer(l);
	*p = toupper(*p);
	n.UnlockBuffer(l);
	
	tmp = n;
	if (look_in_methods_and_events(tmp)) {
		berr << "*** PROPERTY ALREADY DEFINED AS METHOD: " << aId << endl;
		return B_NAME_IN_USE;
	}
	
	if (aType->HasAttribute(kReadOnly) == false) {
		tmp = "Set";
		tmp += n;
		if (look_in_methods_and_events(tmp)) {
			berr << "*** PROPERTY ALREADY DEFINED AS METHOD: " << aId << endl;
			return B_NAME_IN_USE;
		}
	}
	
	sptr<IDLNameType> newproperty= new IDLNameType(aId, aType, aComment, aCustom);
	m_properties.AddItem(newproperty);
	
	return B_OK;
}			


status_t
InterfaceRec::AddMethod(const sptr<IDLMethod>& method)
{
	if (is_id_in_use(method->ID())) {
		berr << "*** DUPLICATE METHOD: " << method->ID() << endl;
		return B_NAME_IN_USE;
	}
	m_methods.AddItem(method);
	return B_OK;
}

status_t
InterfaceRec::AddEvent(const sptr<IDLEvent>& event)
{
	if (is_id_in_use(event->ID())) {
		berr << "*** DUPLICATE EVENT: " << event->ID() << endl;
		return B_NAME_IN_USE;
	}
	m_events.AddItem(event);
	return B_OK;
}

status_t
InterfaceRec::AddTypedef(SString id, const sptr<IDLType>& aType, const sptr<IDLCommentBlock>& aComment)
{
	int32_t count = m_typedefs.CountItems();
	for (int32_t i = 0; i < count; i++) {
		if (m_typedefs.ItemAt(i)->m_id == id) {
			berr << "*** DUPLICATE TYPEDEF: " << id << endl;
			return B_NAME_IN_USE;
		}
	}

	sptr<IDLNameType> newtypedef = new IDLNameType(id, aType, aComment);
	m_typedefs.AddItem(newtypedef);
	return B_OK;
}

status_t						
InterfaceRec::AddConstruct(const sptr<IDLConstruct>& construct)
{	
	m_constructs.AddItem(construct);
	return B_OK;
}

status_t
InterfaceRec::AddParent(SString parent)
{	
	m_parents.AddItem(parent);
	return B_OK;
}

status_t
InterfaceRec::SetNamespace(SString nspace)
{	
	m_namespace=nspace;
	return B_OK;
}

status_t
InterfaceRec::SetDeclaration(dcltype adecl)
{	
	m_declared=adecl;
	return B_OK;
}

int32_t
InterfaceRec::CountProperties()
{
	return m_properties.CountItems();
}

sptr<IDLNameType>
InterfaceRec::PropertyAt(int32_t i)
{
	return m_properties.ItemAt(i);
}

int32_t
InterfaceRec::CountMethods()
{
	return m_methods.CountItems();
}

sptr<IDLMethod>
InterfaceRec::MethodAt(int32_t i)
{
	return m_methods.ItemAt(i);
}

int32_t
InterfaceRec::CountEvents()
{
	return m_events.CountItems();
}

sptr<IDLEvent>
InterfaceRec::EventAt(int32_t i)
{
	return m_events.ItemAt(i);
}

int32_t
InterfaceRec::CountConstructs()
{
	return m_constructs.CountItems();
}

sptr<IDLConstruct>
InterfaceRec::ConstructAt(int32_t i)
{
	return m_constructs.ItemAt(i);
}

int32_t
InterfaceRec::CountTypedefs()
{
	return m_typedefs.CountItems();
}

sptr<IDLNameType>
InterfaceRec::TypedefAt(int32_t i)
{
	return m_typedefs.ItemAt(i);
}

void
InterfaceRec::OutputComment(const sptr<ITextOutput> &stream, bool startWithTab)
{
	if (m_comment != NULL) {
		m_comment->Output(stream, startWithTab);
	}
}

status_t
InterfaceRec::View()
{
	bout << "ID = " << ID() << endl;
	bout << "Namespace = " << Namespace() << endl;
	
	SVector<SString> rents=Parents();
	for (int s=0; s<rents.CountItems(); s++) { 	
		if (s=0) {
			bout << "Parents = " << endl;
		}
		bout << " - " << rents.ItemAt(s) << endl;
	}

	int32_t num=CountProperties();
	bout << "# of Properties = " << num << endl;
	for (int s=0; s< num; s++) {
		sptr<IDLNameType> nt=PropertyAt(s);
		bout << " - " << nt->m_id << endl;
	}
	
	num=CountMethods();
	bout << "# of Methods = " << num << endl;
	for (int s=0; s< num; s++) {
		sptr<IDLMethod> m=MethodAt(s);
		bout << " - " << m->ID() << endl;
	}

	num=CountEvents();
	bout << "# of Events = " << num << endl;
	for (int s=0; s< CountEvents(); s++) {
		sptr<IDLEvent> e=EventAt(s);
		bout << " - " << e->ID() << endl;
	}
	return B_OK;
}

bool
InterfaceRec::is_id_in_use(const SString & id)
{
	SString tmp = id;
	int32_t length = tmp.Length();
	char *p = tmp.LockBuffer(length);
	p[0] = tolower(p[0]);
	tmp.UnlockBuffer(length);

	if (look_in_properties(tmp)) {
		return true;
	}
	if (0 == id.Compare("Set", 3)) {
		if (look_in_properties(SString(tmp.String() + 3))) {
			return true;
		}
	}
	
	return look_in_methods_and_events(id);
}

bool
InterfaceRec::look_in_properties(const SString & id)
{
	int32_t i, count = CountProperties();
	for (i=0; i<count; i++) {
		if (PropertyAt(i)->m_id == id) {
			return true;
		}
	}
	return false;
}

bool
InterfaceRec::look_in_methods_and_events(const SString & id)
{
	int32_t i, count = CountMethods();
	for (i=0; i<count; i++) {
		if (MethodAt(i)->ID() == id) {
			return true;
		}
	}
	
	count = CountEvents();
	for (i=0; i<count; i++) {
		if (EventAt(i)->ID() == id) {
			return true;
		}
	}
	
	return false;
}

status_t
InterfaceRec::SetAttribute(AttributeKind attr)
{	
	m_attributes |= attr;
	return B_OK;
}

bool
InterfaceRec::HasAttribute(AttributeKind attr) const
{	
	return ((m_attributes & attr) == attr);
}
