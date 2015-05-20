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

#include "OutputUtil.h"

#include <ctype.h>
#include <string.h>
#include <support/StdIO.h>

extern InterfaceRec* FindInterface(const SString &name);
extern sptr<IDLType> FindType(const sptr<IDLType>& typeptr);


// The native/proxy prefix to the class name (used to be "L" and "R")
const char* gNativePrefix = "Bn";
const char* gProxyPrefix = "Bp";

status_t
HeaderGuard(const SString &in, SString &out, bool system)
{
	// Do some validity checking
	if (!isalpha(in.ByteAt(0)))
	{
		berr << "filename must start with a letter " << in << endl;
		return B_ERROR;
	}
	
	char * p;
	int l;
	
	// Header Guard define name

	out = in;
	out.ToUpper();
	l = out.Length();
	p = out.LockBuffer(l);
	while (*p) {
		if (!isalnum(*p)) *p = '_';
		p++;
	}
	out.UnlockBuffer(l);
	
	if (system) out.Prepend('_', 1);
	
	return B_OK;
}

bool
IsAutoMarshalType(const sptr<IDLType> &type)
{
	if (type->GetName() == "void") return false;

	//printf("Finding type for: %s\n", type->GetName().String());
	sptr<IDLType> st=FindType(type);
	//printf("The type found is: %s\n", st->GetName().String());
	return (st->GetAttributes()&kAutoMarshal) != 0;
}

bool
IsAutobinderType(const sptr<IDLType> &type)
{
	sptr<IDLType> st=FindType(type);
	uint32_t code = st->GetCode();
	//printf("Type %s: attrs=%08lx\n", st->GetName().String(), st->GetAttributes());
	return (code != B_WILD_TYPE && code != B_VARIABLE_ARRAY_TYPE && code != B_MESSAGE_TYPE)
		 || ((st->GetAttributes()&kAutoMarshal) != 0);
}

// clients can use kInsideClassScope so they don't need to pass the interface
// to TypeToCPPType, but they'd better be prepared to use the naked typedef
// at that point for the resulting file to compile correctly
const InterfaceRec* kInsideClassScope = NULL;

SString
TypeToCPPType(const InterfaceRec* rec, const sptr<IDLType>& obj, bool asConst)
{
	// Convert IDLType into a C++ type name
	// rec can be NULL if we are writing the type inside the class, otherwise
	// it must point to the owning class (for typedefs)

	SString	cpptype = obj->GetName();

	if (cpptype==NULL) {
		bout << "<----- outpututil.cpp -----> invalid type when converting to CPP;"  << endl;
		_exit(1);
	}
	else {		
		// Binder Types
		// IClass* -> sptr<IClass>
		// [weak] IClass* -> wptr<IClass>
		// (both of these types have the name of "sptr")
		if (cpptype=="sptr") {
			if (obj->HasAttribute(kWeak) == true) {
				cpptype = "wptr";
			}
			InterfaceRec* iface=FindInterface(obj->GetIface());				
			cpptype.Append("<");
			if (iface->ID()== "Binder") { cpptype.Append("I"); }
			cpptype.Append(iface->ID());
			cpptype.Append(">");
			if (asConst) {
				cpptype.Prepend("const ");
				cpptype.Append("&");
			}

		} 
		else if ((cpptype=="SString") || (cpptype=="SValue") || (cpptype=="SMessage")) {
			if (asConst) {
				cpptype.Prepend("const ");
				cpptype.Append("&");
			}
		} 
		else if (cpptype=="char*") {
			if (asConst) {			
				// in the rare case this is a const char*
				cpptype.Prepend("const ");
			}
			else {
				cpptype="SString";
			}
		} 
		else if (cpptype=="void") {
			;	// cpptype is fine as is
		} 
		else {

			// deal with typedefs...
			// If the stored type is different than the object type, then
			// we have a user defined typedef... 
			// add in the class name as a qualifier, because they are defined
			// inside the IClass definition.
			sptr<IDLType> storedtype=FindType(obj);
			if (storedtype->GetName() != obj->GetName() && rec != kInsideClassScope) {
				SString classQualifier = rec->ID();
				classQualifier.Append("::");
				cpptype.Prepend(classQualifier);
			}
			if (asConst) {
				uint32_t code = storedtype->GetCode();
				if (code == B_WILD_TYPE || code == B_VARIABLE_ARRAY_TYPE) {
					// If B_WILD_TYPE then we have an exported type
					// that we really don't know about, or
					// if B_VARIABLE_ARRAY_TYPE, then we have a sequence
					// typedef.  Both of which want const references
					// for input parameters.
					cpptype.Prepend("const ");
					cpptype.Append("&");
				}
			}	
		}
	}

	return cpptype;
}

SString
TypeToDefaultValue(const InterfaceRec* rec, const sptr<IDLType>& obj)
{
	SString	cpptype=obj->GetName();

	if (cpptype==NULL) {
		bout << "<----- outpututil.cpp -----> invalid type when converting to CPP;"  << endl;
		_exit(1);
	}
	else {		
		// Binder Types
		if (cpptype=="sptr") {
			cpptype = "NULL";
		} 
		else if (cpptype=="char*") {
			cpptype = "NULL";
		} 
		else if (cpptype=="SValue") {
			cpptype = "B_UNDEFINED_VALUE";
		} 
		else if (cpptype=="SString") {
			cpptype = "SString::EmptyString()";
		} 
		else if (cpptype=="SPoint") {
			// The default constructor for SPoint does not initialize the object,
			// for performance reasons.
			cpptype = "SPoint::Origin()";
		}
		else if (cpptype == "status_t") {
			cpptype = "B_OK";
		}
		else if ((cpptype=="size_t") ||
				(cpptype=="char") ||
				(cpptype=="wchar32_t") ||
				(cpptype=="int8_t") ||
				(cpptype=="int16_t") ||
				(cpptype=="int32_t") ||
				(cpptype=="int64_t") ||
				(cpptype=="uint8_t") ||
				(cpptype=="uint16_t") ||
				(cpptype=="uint32_t") ||
				(cpptype=="uint64_t") ||
				(cpptype=="nsecs_t") ||
				(cpptype=="float") ||
				(cpptype=="double")) {
			cpptype = "0";
		} 
		else {
			cpptype = TypeToCPPType(rec, obj, false);
			cpptype.Append("()");
		}
	}

	return cpptype;
}


SString
ToSValueConversion(const sptr<IDLType>& type, const SString &variable)
{
	SString s("SValue::");
	SString tn=type->GetName();

	sptr<IDLType> storedtype=FindType(type);
	if (storedtype->CountMembers()>0) {	
		sptr<jmember> toBV=storedtype->GetMemberAt(0); 

		if (tn=="SValue") {		
			s=variable; 
		}
		else if ((tn=="SMessage")) {		
			s =variable;
			s.Append(".AsValue()");
		}		
		else if (tn=="sptr") {		
			s.Append("Binder");
			s.Append("(");
			s.Append(variable);

			if (type->GetIface()!="IBinder") {
				if (type->HasAttribute(kWeak)) {
					s.Append(".promote() != NULL ? ");
					s.Append(variable);
					s.Append(".promote()->AsBinder() : sptr<IBinder>()");
				}
				else {
					s.Append("->AsBinder()");
				}
			}
			s.Append(")");
		}
		else if ((tn!=NULL) && (storedtype->GetCode()==B_WILD_TYPE)) {		
			//bout << "<----- outpututil.cpp -----> ToSValueConversion - " << tn << " is a custom type " << endl; 
			s=variable;
			s.Append(".");
			s.Append(toBV->ID());
			s.Append("()");
		}
		else if (storedtype->GetCode() == B_VARIABLE_ARRAY_TYPE) {
			// if we are dealing with a sequence, then
			// SVector provides an AsValue
			// the storedtype we are dealing with is the element
			// types, so we ignore the toBV since we want AsValue
			// for the entire array.
			s = variable;
			s.Append(".AsValue()");
		}
		else if (tn=="char*") {
			s.Append("String(");
			s.Append(variable);
			s.Append(")");
		}
		else {
			s.Append(toBV->ID());
			s.Append("(");
			s.Append(variable);
			s.Append(")");
		}

		return s;	
	}
	else {	
		bout << "<----- outpututil.cpp -----> there is no function to marshall type=" << tn << endl; 
		_exit(1);
	}
	return NULL;
}


SString
FromSValueConversion(const sptr<IDLType>& type, const SString &variable, bool setError)
{
	SString s(variable);
	s.Append(".");

	SString tn=type->GetName();

	sptr<IDLType> storedtype=FindType(type);
	if (storedtype->CountMembers()>1) {	
		sptr<jmember> fromBV=storedtype->GetMemberAt(1);

		if ((tn=="SValue") || (tn=="SMessage")) {
			// simple assignment
			s = variable; 
			return s;
		}
		if (tn=="sptr") {		
			//bout << "interface id=" << type->GetIface() << endl;
			if (type->GetIface() == "IBinder") {
				s=variable;
				if (setError) {
					s.Append(".AsBinder(&_pidgen_err)");
				}
				else {
					s.Append(".AsBinder()");
				}
			}
			else {
				InterfaceRec*	validiface=FindInterface(type->GetIface());
				s.Truncate(0);
				/* if (validiface->InNamespace()) {
					s.Append(validiface->Namespace());
					s.Append(':', 2);
				} */
				s.Append(validiface->ID());
				s.Append("::AsInterface(");
				s.Append(variable);
				if (setError) {
					s.Append(", &_pidgen_err)");
				}
				else {
					s.Append(")");
				}
			}
			return s;	
		}
		else if ((tn!=NULL) && (storedtype->GetCode() == B_WILD_TYPE)) {		
			// custom types which have Code==B_WILD_TYPE
			// all custom types use explicit constructor
			//bout << "<----- outpututil.cpp -----> FromSValueConversion - " << tn << " is a custom type " << endl; 

			s=fromBV->ID();
			s.Append("(");
			s.Append(variable);
			if (setError) {
				s.Append(", &_pidgen_err)");
			}
			else {
				s.Append(")");
			}
			//s.Prepend(".");	
			return s;
		}
		else if ((tn!=NULL) && storedtype->GetCode() == B_VARIABLE_ARRAY_TYPE) {
			// custom types which are sequences
			// if we are dealing with an array type, then we need to
			// use the SVector SetFromValue function
			s = "SetFromValue(";
			s.Append(variable);
			if (setError) {
				s.Append(", &_pidgen_err)");
			}
			else {
				s.Append(")");
			}
			return s;
		}
		else {
			s.Append(fromBV->ID());
			if (setError) {
				s.Append("(&_pidgen_err)");
			}
			else {
				s.Append("()");
			}
	
			if ((tn=="size_t") ||
					(tn=="char") ||
					(tn=="wchar32_t") ||
					(tn=="int8_t") ||
					(tn=="int16_t") ||
					(tn=="uint8_t") ||
					(tn=="uint16_t") ||
					(tn=="uint32_t") ||
					(tn=="uint64_t")) { 
				s.Prepend(")");
				s.Prepend(tn);
				s.Prepend("("); 
			}
			return s;
		}
	}
	else {	
		bout << "<----- outpututil.cpp -----> there is no function to marshall type=" << tn << endl; 
		_exit(1);
	}
	return NULL;

}

SString IndexToSValue(int32_t index)
{
	SString s;
	if (index>=0 && index<=10) {
		s << "B_" << index << "_INT32";
	} else {
		s << "SValue::Int32(" << index << ")";
	}
	return s;
}

static const char* kAssignStr = " = ";

SString 
FromSValueExpression(const InterfaceRec* rec,
					 const sptr<IDLType>& vartype,
					 const SString& varname,
					 const SString& valuename,
					 ExpressionKind kind,
					 bool setError)
{
	// Create the appropriate expression for converting from an SValue
	// We do this here, so we don't have to special case the code all over OutputCPP.cpp
	// There are 8 different places where FromSValueConversion is called,
	// all just slightly different than the other.
	// However, they can be grouped into the following categories:
	//	INITIALIZE,				// BClass v = BClass(value, &err);
	//	ASSIGN,					// arg = BClass(value, &err);
	//	INDIRECT_ASSIGN,		// *arg = BClass(value, &err);
	//	RETURN					// return BClass(value, &err);

	//	sequences, which are handled in C++ as SVector must be treated differently.
	//	INITIALIZE,				// VectorClass v; v.SetFromValue(value, &err);
	//	ASSIGN,					// arg.SetFromValue(value, &err);
	//	INDIRECT_ASSIGN,		// arg->SetFromValue(value, &err);
	//	RETURN					// return VectorClass(value, &err);

	SString expr;
	sptr<IDLType> storedtype=FindType(vartype);
	if (storedtype->GetCode() != B_VARIABLE_ARRAY_TYPE) {
		switch (kind) {
			case INITIALIZE:
				{
				expr.Append(TypeToCPPType(rec, vartype, false));
				expr.Append(" ");
				expr.Append(varname);
				expr.Append(kAssignStr);
				break;
				}
			case ASSIGN:
				expr.Append(varname);
				expr.Append(kAssignStr);
				break;
			case INDIRECT_ASSIGN:
				expr.Append("*");
				expr.Append(varname);
				expr.Append(kAssignStr);
				break;
			case RETURN:
				expr.Append("return ");
				break;
		}
	}
	else {
		// The SFlattenable signature for SetFromValue doesn't set any errors, just returns them
		setError = false;
		switch (kind) {
			case INITIALIZE:
				// VectorClass v; v.SetFromValue(value, &err);
				expr.Append(TypeToCPPType(rec, vartype, false));
				expr.Append(" ");
				expr.Append(varname);
				expr.Append(";\n");
				expr.Append(varname);
				expr.Append(".");
				break;
			case ASSIGN:
				// arg.SetFromValue(value, &err);
				expr.Append(varname);
				expr.Append(".");
				break;
			case INDIRECT_ASSIGN:
				// arg->SetFromValue(value, &err);
				expr.Append(varname);
				expr.Append("->");
				break;
			case RETURN:
				// VectorClass rv; rv.SetFromValue(value, &err); return rv;
				expr.Append(TypeToCPPType(rec, vartype, false));
				expr.Append(" ");
				expr.Append(varname);
				expr.Append(";\n");
				expr.Append(varname);
				expr.Append(".");
				// ... to be continued after FromSValueConversion
				break;
		}
	}
	expr.Append(FromSValueConversion(vartype, valuename, setError));
	expr.Append(";");
	if (storedtype->GetCode() == B_VARIABLE_ARRAY_TYPE && kind == RETURN) {
		expr.Append("\nreturn ");
		expr.Append(varname);
		expr.Append(";");
	}
	return expr;
}


void
AddFromSValueStatements(const InterfaceRec* rec,
						sptr<StatementList> statementList, 
						const sptr<IDLType>& vartype,
						const SString& varname,
						const SString& valuename)
{
	sptr<IDLType> storedtype = FindType(vartype);
	SString cpptype = TypeToCPPType(rec, vartype, false);
	if (storedtype->GetCode() != B_VARIABLE_ARRAY_TYPE) {
		// statements = MyType rv(value);
		statementList->AddItem(new Literal(true, "%s %s(%s)", cpptype.String(), varname.String(), valuename.String()));
	}
	else {
		// statements = MyType rv;
		//				rv.SetFromValue(value);
		statementList->AddItem(new Literal(true, "%s %s", cpptype.String(), varname.String()));
		statementList->AddItem(new Literal(true, "%s.SetFromValue(%s)", varname.String(), valuename.String()));
	}
}


int32_t 
CountStringTabs(const SString& str, int32_t tabLen)
{
	return (str.Length()+(tabLen-1))/tabLen;
}

const char* 
PadString(const SString& str, int32_t fieldTabs, int32_t tabLen)
{
	static const char tabs[] =
		"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"	// 16
		"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"	// 32
		"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"	// 48
		"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";	// 64
	const int32_t len = str.Length()/tabLen;
	int32_t pad = fieldTabs - len;
	if (pad <= 0) pad = 1;
	if (pad > (sizeof(tabs)-1)) pad = sizeof(tabs)-1;
	return tabs + (sizeof(tabs) - pad - 1);
}

void CollectParents(InterfaceRec* rec, const SVector<InterfaceRec*>& classes, SVector<InterfaceRec*>* out)
{
	SVector<SString> parents = rec->Parents();
	const size_t N = parents.CountItems();
	for (size_t i=0; i<N; i++) {
		const size_t Nc = classes.CountItems();
		for (size_t c=0; c<Nc; c++) {
			if (parents[i] == classes[c]->ID() || parents[i] == classes[c]->FullInterfaceName()) {
				out->AddItem(classes[c]);
			}
		}
	}
	out->AddItem(rec);
}

NamespaceGenerator::NamespaceGenerator()
	: namespaceDepth(0), first(true)
{
}

NamespaceGenerator::~NamespaceGenerator()
{
}

void NamespaceGenerator::EnterNamespace(const sptr<ITextOutput> stream, const SString& newNS, const SVector<SString>& newUsing)
{
	SString ns, nspart;
	int32_t pos, oldpos;
	size_t i;
	bool diff = prevNamespace != newNS;

	for (i=0; !diff && i<newUsing.CountItems(); i++) {
		diff = prevUsing.IndexOf(newUsing[i]) < 0;
	}

	if (!diff) return;

	bool haveNamespaces = false;
	
	if (prevNamespace != newNS) {
		if (prevNamespace.Length() > 0) {
			stream << endl;
			if (!haveNamespaces) {
				haveNamespaces = true;
				stream << "#if _SUPPORTS_NAMESPACE" << endl;
			}

			for (i=0; i<namespaceDepth; i++) stream << "} ";
			namespaceDepth = 0;
			stream << "// namespace " << prevNamespace << endl;
			stream << endl;
		}
		prevNamespace = newNS;
		prevUsing.MakeEmpty();
		if (prevNamespace.Length() > 0) {
			ns = newNS;
			oldpos = 0;
			while ((pos = ns.FindFirst("::", oldpos)) > 0) {
				nspart.SetTo(ns.String()+oldpos, pos-oldpos);
				if (!haveNamespaces) {
					haveNamespaces = true;
					stream << "#if _SUPPORTS_NAMESPACE" << endl;
				}
				stream << "namespace " << nspart << " {" << endl;
				oldpos = pos + 2;
				namespaceDepth++;
			}
			namespaceDepth++;
			nspart.SetTo(ns.String()+oldpos);
			if (!haveNamespaces) {
				haveNamespaces = true;
				stream << "#if _SUPPORTS_NAMESPACE" << endl;
			}
			stream << "namespace " << nspart << " {" << endl;
		} else {
			namespaceDepth = 0;
		}
	}

	if (newNS != "palmos::support" && prevUsing.IndexOf(SString("palmos::support")) < 0) {
		if (!haveNamespaces) {
			stream << "#if _SUPPORTS_NAMESPACE" << endl;
		}
		haveNamespaces = true;
		stream << "using namespace palmos::support;" << endl;
		prevUsing.AddItem(SString("palmos::support"));
	}

	for (i=0; i<newUsing.CountItems(); i++) {
		const SString& n = newUsing[i];
		if (newNS != n && prevUsing.IndexOf(n) < 0) {
			if (!haveNamespaces) {
				stream << "#if _SUPPORTS_NAMESPACE" << endl;
			}
			haveNamespaces = true;
			stream << "using namespace " << n << ";" << endl;
			prevUsing.AddItem(n);
		}
	}

	if (haveNamespaces) {
		stream << "#endif /* _SUPPORTS_NAMESPACE */" << endl;
		stream << endl;
	}
}

void NamespaceGenerator::CloseNamespace(const sptr<ITextOutput> stream)
{
	if (namespaceDepth > 0) {
		size_t i;
		stream << "#if _SUPPORTS_NAMESPACE" << endl;
		for (i=0; i<namespaceDepth; i++) stream << "} ";
		stream << "// namespace " << prevNamespace << endl;
		stream << "#endif /* _SUPPORTS_NAMESPACE */" << endl;
		stream << endl;
		namespaceDepth = 0;
		prevNamespace = "";
		prevUsing.MakeEmpty();
	}
}
