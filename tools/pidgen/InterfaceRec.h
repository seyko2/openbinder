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

#ifndef INTERFACEREC_H
#define INTERFACEREC_H

#include <support/Atom.h>
#include "idlc.h"

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

enum dcltype {
	FWD, IMP, DCL
};

// Attributes of a named item in the interface,
// The parser is in charge of assuring that only the applicable
// attributes are applied on that item.  (For example, [in] doesn't
// apply to methods or interfaces.
enum AttributeKind {
	kNoAttributes	= 0,	// default to anything
	kIn				= 0x00000001,	// parameter
	kOut			= 0x00000002,	// parameter
	kInOut			= 0x00000004,	// parameter
	kReadOnly		= 0x00000008,	// property
	kWriteOnly		= 0x00000010,	// property
	kWeak			= 0x00000020,	// parameter or property
	kOptional		= 0x00000040,	// parameter
	kLocal			= 0x00000080,	// method or interface
	kReserved		= 0x00000100,	// method or property
	kAutoMarshal	= 0x00000200	// type: use autobinder marshalling
};
const uint32_t kDirectionMask = kIn + kOut + kInOut;

class InterfaceRec;

// jnamedtype & jmember - goes away when we can describe types in idl
class jnamedtype : public SAtom
{	
	public:
								jnamedtype();
								jnamedtype(SString id, SString type);
								jnamedtype(const sptr<const jnamedtype>& orig);

		  SString				m_id;
		  SString				m_type;
};

// used in IDLType for storing members & member functions
// if used to storing members, member type=m_returnType and member name=m_id
class jmember : public SAtom
{
	public:	
											jmember();
											jmember(SString id, SString rtype);
											jmember(const sptr<const jmember>& orig);

			status_t						AddParam(SString id, SString type);
			int32_t							CountParams();
			sptr<jnamedtype>				ParamAt(int32_t i);
			SString							ID();
			SString							ReturnType();
	
	private:
			SString							m_id;
			SString							m_returnType;
			SVector<sptr<jnamedtype> >		m_params;
};

class IDLCommentBlock : public SAtom
{
	public:
											IDLCommentBlock();
		void								AddComment(const SString& comment);
		void								AppendToComment(const SString& more);

		void								Output(const sptr<ITextOutput> &stream, bool startWithTab = true) const;
	
	private:
		SVector<SString>					m_comments;
};

// IDL classes
class IDLType : public SAtom
{
	public:
	
											IDLType();
											IDLType(SString code);
											IDLType(SString name, uint32_t code);
											IDLType(SString name, uint32_t code, uint32_t attr);
											IDLType(const sptr<const IDLType>& orig);
											//IDLType& operator = (const IDLType& orig);

			status_t						SetCode(uint32_t code);
			status_t						SetName(SString name);
			status_t						SetPrimitiveName(SString name);		// for typedefs
			status_t						SetAttribute(AttributeKind attr);
			status_t						AddAttributes(uint32_t attributes);
			status_t						SetAttributes(uint32_t attributes);
			status_t						SetIface(SString typeiface);
			status_t						AddMember(sptr<jmember> typemem);
			uint32_t						GetCode() const;
			SString							GetName() const;
			bool							HasAttribute(AttributeKind attr) const;
			uint32_t						GetAttributes() const;
			uint32_t						GetDirection() const;
			SString							GetPrimitiveName() const;
			SString							GetIface() const;
			sptr<jmember>					GetMemberAt(int32_t i) const;
			int32_t							CountMembers();

	private:
			uint32_t						m_code;
			SString							m_name;
			uint32_t						m_attributes;
			SString							m_iface;
			SString							m_primitive;
			SVector<sptr<jmember> >			typemembers;

};

class IDLNameType : public IDLType
{
	public:
	
										IDLNameType();
										IDLNameType(SString id, sptr<IDLType> typeptr, const sptr<IDLCommentBlock>& comment, bool custom =false);
										IDLNameType(const sptr<const IDLNameType>& orig);

			void						OutputComment(const sptr<ITextOutput> &stream, bool startWithTab = true);
			bool						HasComment() const;

			SString						m_id;
			sptr<IDLType>				m_type;
			sptr<IDLCommentBlock>		m_comment;
			bool						m_custom;
			ssize_t						m_index;
};


// IDLTypeScopes are temporary holders of types/names
// They are used to build IDLMethods or IDLNameTypes when
// a full production is realized.
// It can hold single items (such as a global type name)
// or it can hold multple (such as a parameter list)
class IDLTypeScope : public IDLNameType
{
	public:
			
										IDLTypeScope();
										IDLTypeScope(SString id, const sptr<IDLCommentBlock>& comment);
										IDLTypeScope(const sptr<const IDLTypeScope>& orig);
	
			status_t					AddParam(SString id, const sptr<IDLType>& typeptr, const sptr<IDLCommentBlock>& comment);
			int32_t						CountParams();
			sptr<IDLNameType>			ParamAt(int32_t i);
			SString						ID();
			status_t					SetAutoBinder(bool t);
			bool						AutoBinder();

private:
			bool						m_autobinder;
			SVector<sptr<IDLNameType> >	m_params;
};


class IDLMethod : public IDLNameType
{
	public:
			
											IDLMethod();
											IDLMethod(SString id, const sptr<IDLType>& typeptr, const sptr<IDLCommentBlock>& comment, bool isconst=false);
											IDLMethod(const sptr<const IDLMethod>& orig, bool isconst=false);
	
			status_t						AddParam(SString id, const sptr<IDLType>& typeptr, const sptr<IDLCommentBlock>& comment);
			int32_t							CountParams();
			sptr<IDLNameType>				ParamAt(int32_t i);
			SString							ID();
			sptr<IDLType>					ReturnType();
			status_t						SetAutoBinder(bool t);
			bool							AutoBinder();
			status_t						SetConst(bool t);
			bool							IsConst() const;

			void							AddTrailingComment(const sptr<IDLCommentBlock>& comment);
			void							OutputTrailingComment(const sptr<ITextOutput> &stream) const;
			bool							HasTrailingComment() const;

	private:
			bool							m_autobinder;
			bool							m_const;
			sptr<IDLType>					m_returnType;
			SVector<sptr<IDLNameType> >		m_params;
			sptr<IDLCommentBlock>			m_trailing_comment;
};

// events are just like methods ... same syntax
typedef IDLMethod IDLEvent;
// enums/structs are packaged as methods with enumerators/members as parameters
typedef IDLMethod IDLConstruct;

class InterfaceRec
{
	public:
										InterfaceRec();
										InterfaceRec(SString aId, SString nspace, SVector<SString> cppnspace, const sptr<IDLCommentBlock>& aComment, dcltype adecl);
					
			SString						ID() const;
			SString						Namespace() const;
			SVector<SString>			CppNamespace() const;

			dcltype						Declaration() const;

			SString						FullInterfaceName() const;
	
			bool						InNamespace() const;
			SString						FullClassName(const SString &classPrefix) const;

			SVector<SString>			Parents() const;
			bool						HasMultipleBases() const;
			SString						LeftMostBase() const;

			status_t					AddCppNamespace(SString cppn);
			status_t					AddProperty(SString aId, const sptr<IDLType>& type, const sptr<IDLCommentBlock>& aComment, bool aCustom);
			status_t					AddMethod(const sptr<IDLMethod>& method);
			status_t					AddEvent(const sptr<IDLEvent>& event);
			status_t					AddConstruct(const sptr<IDLConstruct>& construct);
			status_t					AddTypedef(SString aId, const sptr<IDLType>& type, const sptr<IDLCommentBlock>& aComment);
			status_t					AddParent(SString parent);
			status_t					SetNamespace(SString nspace);
			status_t					SetDeclaration(dcltype adecl);
	
			int32_t						CountTypedefs();
			sptr<IDLNameType>			TypedefAt(int32_t i);

			int32_t						CountProperties();
			sptr<IDLNameType>			PropertyAt(int32_t i);
	
			int32_t			 			CountMethods();
			sptr<IDLMethod>				MethodAt(int32_t i);
	
			int32_t						CountEvents();
			sptr<IDLEvent>				EventAt(int32_t i);

			int32_t						CountConstructs();
			sptr<IDLConstruct>			ConstructAt(int32_t i);

										// bouts to look at the whole interface - debugging only
			status_t					View();

			status_t					SetAttribute(AttributeKind attr);
			bool						HasAttribute(AttributeKind attr) const; 
	
			void						OutputComment(const sptr<ITextOutput> &stream, bool startWithTab = true);

	private:

			bool						is_id_in_use(const SString & id);
			bool						look_in_properties(const SString & id);
			bool						look_in_methods_and_events(const SString & id);

				
			SString							m_id;
			SString							m_namespace;
			SVector<SString>				m_cppNamespace;
			SVector<SString>				m_parents;
			sptr<IDLCommentBlock>			m_comment;
			SVector<sptr<IDLNameType> >		m_properties;
			SVector<sptr<IDLMethod> >		m_methods;
			SVector<sptr<IDLEvent> >		m_events;
			SVector<sptr<IDLNameType> >		m_typedefs;
			SVector<sptr<IDLConstruct> >	m_constructs;
			dcltype							m_declared;
			uint32_t						m_attributes;
};

class IncludeRec
{
public:
	IncludeRec() { }
	IncludeRec(const SString& file, const sptr<IDLCommentBlock>& comments = NULL) : m_file(file), m_comments(comments) { }

	inline SString File() const { return m_file; }
	inline sptr<IDLCommentBlock> Comments() const { return m_comments; }

private:
	SString					m_file;
	sptr<IDLCommentBlock>	m_comments;
};

#endif // INTERFACEREC_H
