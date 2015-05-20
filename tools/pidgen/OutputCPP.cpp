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

#include "TypeBank.h"
#include "OutputUtil.h"
#include "OutputCPP.h"
#include "AST.h"

#include <support/StdIO.h>
#include <ctype.h>

// No longer printing keys in public headers, as we can't export data.
// Maybe some day this will change.
#define KEYS_IN_HEADER 0
extern sptr<IDLType> FindType(const sptr<IDLType>& typeptr);

bool g_writeAutobinder = true;

enum {
	OUT_PARAM = 1,
	IN_PARAM = 2
};

static SString
KeyVariableName(const SString& obj, const char* prefix, const SString& key)
{
	SString fullName(obj);
	fullName.Append("_");
	fullName.Append(prefix);
	fullName.Append("_");
	fullName.Append(key);
	return fullName;
}

static SString
DirectionString(uint32_t direction)
{
	switch (direction) {
		case kOut:
			return SString("out");
			break;
		case kInOut:
			return SString("inout");
			break;
		case kIn:
		default:
			return SString("in");
			break;
	}
}

const SString kTargetPtr("target.ptr()");
const SString kThis("this");

// Notice that in the case of multiple inheritance, that the cast in CastExpression
// differs than the cast used in InterfaceFor.  (See description at emit of InterfaceFor.)
// When marshalling/unmarshalling pointers, we always cast through the left most base
// of the concrete class.  For example, in the case of:
// class ICatalog : public INode, IIterable
// CastExpression always results in static_cast<INode*>(<expression>)
static const SString
CastExpression(const SString& normalExpr, const InterfaceRec* concreteClass)
{
	// for the simple case, return the normal uncast expression
	// However, if we have multiple bases, that is ambiguous, and we need to provide
	// a path for the pointer, so we use the left-most-base
	// (remember that LeftMostBase might return the class itself, which is just fine)

	if (concreteClass->HasMultipleBases() == false) {
		return normalExpr;
	}
	else {
		SString result("static_cast<");
		result += concreteClass->LeftMostBase();
		result += "*>(";
		result += normalExpr;
		result += ")";
		return result;
	}
}

static SString TypeToMarshaller(const InterfaceRec* rec, const sptr<IDLType>& param)
{
	SString str("marshaller_");
	str += TypeToCPPType(rec, param, false);
	return str;
}

static void
AddAutoMarshalStructure(const InterfaceRec* rec, const sptr<ArrayInitializer>& ar, const sptr<IDLType>& param)
{
	//printf("*** AddAutoMarshalStructure enter %s: atts=%08lx\n", param->GetName().String(), param->GetAttributes());
	if (IsAutoMarshalType(param)) {
		//printf("*** Goody!\n");
#if 0
		SString str("(const PTypeMarshaller*)&SMarshallerForType<");
		str += TypeToCPPType(rec, param, false);
		str += ">::marshaller";
#endif
		SString str("&");
		str += TypeToMarshaller(rec, param);
		ar->AddItem(new StringLiteral(str.String()));
	} else {
		ar->AddItem(new StringLiteral("NULL"));
	}
	//printf("*** AddAutoMarshalStructure exit\n");
}

static void
WriteKey(sptr<ITextOutput> stream, const SString& obj, const char* prefix, const SString& key)
{
#if !KEYS_IN_HEADER
	SString fullName(obj);
	fullName.Append("_");
	fullName.Append(prefix);
	fullName.Append("_");
	fullName.Append(key);
	fullName.Append(",");
#else
	SString fullName(prefix);
	fullName.Append("_");
	fullName.Append(key);
	fullName.Append(",");
#endif

	// Real length is Length() + null terminator.
	if (key.Length() >= B_TYPE_LENGTH_MAX)
		stream << "B_STATIC_STRING_VALUE_LARGE("
			<< fullName << PadString(fullName, 8)
			<< "\"" << key << "\", "
#if KEYS_IN_HEADER
			<< "I" << obj << "::"
#endif
			<< ");" << endl;
	else
		stream << "B_STATIC_STRING_VALUE_SMALL("
			<< fullName << PadString(fullName, 8)
			<< "\"" << key << "\", "
#if KEYS_IN_HEADER
			<< "I" << obj << "::"
#endif
			<< ");" << endl;
}

SString
PropertyGetFunction(const SString &name)
{
	SString s;
		s += (char) toupper(name.ByteAt(0));
		s += name.String() + 1;
	return s;
}


SString
PropertyPutFunction(const SString &name)
{
	SString s;
		s += "Set";
		s += (char) toupper(name.ByteAt(0));
		s += name.String() + 1;
	return s;
}

static sptr<Expression> 
AsBinderExpression(SString name, bool isOutput, bool isOptional, bool isWeak)
{
	const char* varName = name.String();
	if (isOutput == true) {
		if (isOptional == true) {
			if (isWeak == true) {
				return new Literal(false, "(%s != NULL && %s->promote() != NULL) ? (*%s).promote()->AsBinder() : sptr<IBinder>()",
								varName, varName, varName);
			}
			else {
				return new Literal(false, "(%s != NULL) ? (*%s)->AsBinder() : sptr<IBinder>()",
								varName, varName);
			}
		}
		else {
			if (isWeak == true) {
				return new Literal(false, "((*%s).promote() != NULL) ? (*%s).promote()->AsBinder() : sptr<IBinder>()",
								varName, varName);
			}
			else {
				return new Literal(false, "(*%s)->AsBinder()", varName);
			}
		}
	}
	else {
		if (isWeak == true) {
			return new Literal(false, "(%s.promote() != NULL) ? %s.promote()->AsBinder() : sptr<IBinder>()",
							varName, varName);
		}
		else {
			return new Literal(false, "%s->AsBinder()", varName);
		}
	}
}

static void
WriteMethodInvoke(InterfaceRec* base, InterfaceRec* rec, SString noid, const sptr<ITextOutput> stream, const sptr<IDLMethod>& method)
{
	// Yes, this currently uses a mix of direct output and AST classes, it should
	// be converted over to AST in the future.

	// If we have a local or reserved function, then just skip to next one
	if (method->HasAttribute(kLocal) || method->HasAttribute(kReserved)) {
		return;	
	}

	// Peek at the parameters so we know how to write the method
	bool hasParameters = false;
	bool needsValueParameter = false;
	bool needsPidgenError = false;

	int32_t paramCount = method->CountParams();
	if (paramCount > 0) {
		hasParameters = true;
		for (int32_t i_param = 0; i_param < paramCount; i_param++) {
			sptr<IDLNameType> nt = method->ParamAt(i_param);
			uint32_t direction = nt->m_type->GetDirection();
			sptr<IDLType> storedtype=FindType(nt->m_type);
			if (direction != kOut || (direction == kOut && nt->m_type->HasAttribute(kOptional))) {
				// we need the value parameter for any input parameters
				// or any optional output parameters
				needsValueParameter = true;
			}
			if (direction != kOut && storedtype->GetCode() != B_VALUE_TYPE) {
				needsPidgenError = true;
			}
		}
	} 

	// Invoke
	stream << "static SValue" << endl;
	stream << noid << "_invoke_" << method->ID() << "(const sptr<IInterface>& target, const SValue& value)" << endl;
	stream << "{" << endl;
	stream << indent;

	SString className(gNativePrefix);
	className += noid;

	stream << className << "* This = static_cast<" << className << " *>(" 
		   << CastExpression(kTargetPtr, base) << ");" << endl;

	// set up locals acc/to need
	if (needsValueParameter == false) {
		stream << "(void) value;" << endl;
	}
	if (hasParameters) {
		stream << "SValue v;" << endl;
	}
	if (needsPidgenError) {
		stream << "status_t _pidgen_err = B_OK;" << endl;
	}
	stream << endl;

	// our function consists of taking each parameter from
	// the value, calling the method, and then packing
	// the parameters back into a value

	bool hasOut = false;
	for (int32_t i_param = 0; i_param < paramCount; i_param++) {
		sptr<IDLNameType> nt = method->ParamAt(i_param);
		uint32_t direction = nt->m_type->GetDirection();
		bool optional = nt->m_type->HasAttribute(kOptional);

		// if direction is in, inout, or not specified
		if (direction == kOut) {
			hasOut = true;
			stream << "// " << nt->m_id << endl;
			sptr<IDLType> storedtype=FindType(nt->m_type);
			SString type = TypeToCPPType(rec, nt->m_type, false);
			SString argname("arg_");
			argname += nt->m_id;
			stream << type << " " << argname << ";" << endl;
			if (optional == true) {
				// we need to see if the other side passed in NULL
				// and if so, use NULL for our function call
					SString argPtr(argname);
					argPtr += "_p";
					stream << type << "* " << argPtr << " = &" << argname << ";" << endl;
					stream << "v = value[" << IndexToSValue(i_param) << "];" << endl;
					stream << "if (v.IsDefined()==false||v==B_NULL_VALUE) " << argPtr << " = NULL;" << endl;
			}
			stream << endl;				
		}
		else {
			if (direction == kInOut) {
				hasOut = true;
			}

			stream << "// " << nt->m_id << endl;
			stream << "v = value[" << IndexToSValue(i_param) << "];" << endl;
		
			SString var("v");
			SString type = TypeToCPPType(rec, nt->m_type, false);
			
			sptr<IDLType> storedtype=FindType(nt->m_type);
			SString argname("arg_");
			argname.Append(nt->m_id);
			if (nt->m_type->GetName()=="char*") {
				sptr<IDLType> typeptr=nt->m_type;
				typeptr=new IDLType(SString("SString"), B_STRING_TYPE);
				argname.Append("_String");
				stream << FromSValueExpression(rec, typeptr, argname, var, INITIALIZE, true) << endl;
				stream << "const char* arg_" << nt->m_id << " = " << "arg_" << nt->m_id << "_String.String();" << endl;
				if (optional == true) {
					// if the conversion failed, we know that if SValue->SString it just leaves it with "", so just ignore the error
					stream << "_pidgen_err=B_OK;" << endl;
				}
			}
			else {		
				if (optional == true) {
					stream << type << " " << argname << " = " << TypeToDefaultValue(rec, nt->m_type) << ";" << endl;
					SString argPtr(argname);
					argPtr += "_p";
					if (direction == kOut || direction == kInOut) {
						stream << type << "* " << argPtr << " = &" << argname << ";" << endl;
					}
					stream << "if (v.IsDefined()&&v!=B_NULL_VALUE) ";
					stream << FromSValueExpression(rec, nt->m_type, argname, var, ASSIGN, true) << endl;
					if (direction == kOut || direction == kInOut) {
						stream << "else " << argPtr << " = NULL;" << endl;
					}
				} 
				else {
					stream << FromSValueExpression(rec, nt->m_type, argname, var, INITIALIZE, true) << endl;
				}
			}
			

			if (storedtype->GetCode()!=B_VALUE_TYPE) {
				stream << "if (_pidgen_err != B_OK) {" << endl << indent;
				stream << "DbgOnlyFatalError(\"bad binder effect argument: " << base->ID() << "." << method->ID() << "() / " << nt->m_id << "\");" << endl;
				stream << "return SValue::Status(_pidgen_err);" << endl;
				stream << dedent << "}" << endl;
			}

			stream << endl;
		}
	}

	sptr<IDLType> returnType = method->ReturnType();
	if (returnType->GetName() == "void") {
		if (hasOut) {
			stream << "SValue rv;" << endl;
		}
		stream << "This->" << method->ID() << "(";
	} 
	else {
		sptr<IDLType> st=FindType(returnType);
		if (st->GetCode()==B_VALUE_TYPE) {
			stream << "SValue rv(" << noid << "_key_res, This->" << method->ID() << "(";
		}
		else if ((returnType->GetIface()!="IBinder") && (st->GetCode()==B_BINDER_TYPE)) {
			// We are doing it this way only to work around an ADS compiler bug.
			stream << "const " << TypeToCPPType(rec, returnType, false) << " rtmp(This->" << method->ID() << "(";
		}
		else if (st->GetCode() == B_VARIABLE_ARRAY_TYPE) {
			// (once again...) We are doing it this way only to work around an ADS compiler bug.
			// ADS has an internal failure when we call MyMethod().AsValue() when MyMethod 
			// returns an SVector<IFoo>. So we split into two statements.
			stream << "const " << TypeToCPPType(rec, returnType, false) << " rtmp(This->" << method->ID() << "(";
		}
		else if (st->GetCode()==B_WILD_TYPE || st->GetCode() == B_MESSAGE_TYPE) {
			stream << "SValue rv(" << noid << "_key_res, " << "This->" << method->ID() << "(";
		}
		else {
			SString r=ToSValueConversion(returnType, returnType->GetName());
			int pos=r.FindFirst("(");
			r.Truncate(pos+1);
			stream << "SValue rv(" << noid << "_key_res, "<< r << "This->" << method->ID() << "("; 
		}
	}

	for (int32_t i_param = 0; i_param < paramCount; i_param++) {
		if (i_param > 0) {
			stream << ", ";
		}
		sptr<IDLNameType> nt = method->ParamAt(i_param);
		uint32_t direction=nt->m_type->GetDirection();
		bool optional = nt->m_type->HasAttribute(kOptional);
		SString argName("arg_");
		argName += nt->m_id;
		// If we have an optional pointer, then use our pointer
		// (that we might have set to null during unpacking)
		// other wise, any pointer will get address of,
		// and any in parameter will go in as the variable itself
		if ((direction == kOut || direction == kInOut) && optional == true) {
			argName += "_p";
		}
		else if (direction == kOut || direction == kInOut) {
			stream << "&";
		}
		stream << argName;
	}

	if (returnType->GetName()=="void") {		
		stream << ");" << endl; 
	}
	else {
		sptr<IDLType> st=FindType(returnType);

		if (st->GetCode()==B_VALUE_TYPE) {		
			stream << "));" << endl; 
		}
		else if ((returnType->GetIface()!="IBinder") && (st->GetCode()==B_BINDER_TYPE)) {
			stream << "));" << endl;
			stream << "SValue rv(" << noid << "_key_res, ";
			stream << ToSValueConversion(returnType, SString("rtmp"));
			stream << ");" << endl;
		}
		else if (st->GetCode() == B_VARIABLE_ARRAY_TYPE) {
			stream << "));" << endl
					<< "SValue rv(" << noid << "_key_res, rtmp.AsValue());" << endl;
		}
		else if (st->GetCode() == B_WILD_TYPE || st->GetCode() == B_MESSAGE_TYPE) {
			stream << ").AsValue());" << endl;
		}
		else {		
			stream << ")));" << endl; 
		}
		
	}

	for (int32_t i_param = 0; i_param < paramCount; i_param++) {
		sptr<IDLNameType> nt = method->ParamAt(i_param);
		uint32_t direction=nt->m_type->GetDirection();
		bool optional = nt->m_type->HasAttribute(kOptional);

		if (direction == kOut || direction == kInOut) {
			SString var("arg_");
			var << nt->m_id;
			if (optional == true) {
				// for optional parameters, if the user passed in NULL, we pass NULL back
				SString argPtr(var);
				argPtr += "_p";
				stream << "rv.JoinItem(" << IndexToSValue(i_param) << ", " 
					<< argPtr << " != NULL ? " 
							  << ToSValueConversion(nt->m_type, var) 
							  << " : B_NULL_VALUE"
					<< ");" << endl;
			}
			else {
				stream << "rv.JoinItem(" << IndexToSValue(i_param) << ", " << ToSValueConversion(nt->m_type, var) << ");" << endl;
			}
		}
	}


	if (returnType->GetName()=="void" && !hasOut) {
		stream << "return SValue();" << endl;
	} 
	else {
		stream << "return rv;" << endl;
	}


	stream << dedent << "}" << endl << endl;
}

// hooks for functions decled in the interface
status_t 
WriteInterfaceHooks(sptr<ITextOutput> stream, InterfaceRec* base, const SVector<InterfaceRec*>& recs, SString noid)
{
	stream << "/* ------ Interface Hooks ------------------------- */" << endl;

	const size_t recordCount = recs.CountItems();
	for (size_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];

		// Put and Get a la properties
		int32_t propertyCount = rec->CountProperties();
		for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {

			sptr<IDLNameType> nt = rec->PropertyAt(i_prop);

			// If we have a reserved property, we don't need the hook functions
			if (nt->m_type->HasAttribute(kReserved)) {
				continue;	
			}
			
			if (nt->m_type->HasAttribute(kReadOnly) == false) {
				// Put
				
				stream << "static status_t" << endl;
				stream << noid << "_put_" << nt->m_id << "(const sptr<IInterface>& target, const SValue& value)" << endl;
				stream << "{" << endl;
				stream << indent;
				
				stream << gNativePrefix << noid << "* This = static_cast<" << gNativePrefix << noid << " *>(" 
					<< CastExpression(kTargetPtr, base) << ");" << endl;
				stream << "status_t _pidgen_err = B_OK;" << endl;

				sptr<IDLType> storedtype=FindType(nt->m_type);
				stream << FromSValueExpression(rec, nt->m_type, SString("v"), SString("value"), INITIALIZE, true) << endl;
				stream << "DbgOnlyFatalErrorIf(_pidgen_err != B_OK, \"bad binder effect property: " << base->ID() << "." << nt->m_id << "\");" << endl;
				stream << "if (_pidgen_err == B_OK) {" << endl << indent;
				stream << "This->Set" << (char) toupper(nt->m_id.ByteAt(0)) << nt->m_id.String()+1 << "(v);" << endl;
				//stream << "This->Push(SValue(" << base->ID() << "::prop_" << nt->m_id << ", value));" << endl;
				stream << dedent << "}" << endl;
				
				stream << "return _pidgen_err;" << endl;
				
				stream << dedent;
				stream << "}" << endl;
				
				stream << endl;
			}
			
			// Get
			stream << "static SValue" << endl;
			stream << noid << "_get_" << nt->m_id << "(const sptr<IInterface>& target)" << endl;
			stream << "{" << endl;
			stream << indent;
			
			// This has been reworked to deal with an ADS bug.  I think this new form
			// is actually a bit better.

			SString var = nt->m_id;
			int32_t length = var.Length();
			char *p = var.LockBuffer(length);
			p[0] = toupper(p[0]);
			var.UnlockBuffer(length);

			stream << "const " << TypeToCPPType(rec, nt->m_type, false) << " v = static_cast<" << gNativePrefix << noid
				<< "*>(" << CastExpression(kTargetPtr, base) << ")->" << var << "();" << endl;
			stream << "return " << ToSValueConversion(nt->m_type, SString("v")) << ";" << endl;			
			stream << dedent << "}" << endl << endl;
			
		}

		// hooks for invoking methods
		int32_t methodCount = rec->CountMethods();
		for (int32_t i_method = 0; i_method < methodCount; i_method++) {
			sptr<IDLMethod> method = rec->MethodAt(i_method);
			WriteMethodInvoke(base, rec, noid, stream, method);
		}
	}

	return B_OK;
}

struct array_item
{
	InterfaceRec* rec;
	size_t index;
	bool is_prop;
};

typedef status_t (*sorted_array_writer)(const sptr<ITextOutput> &stream, const array_item &item, const SString &noid);

status_t WriteActionArrayEntry(const sptr<ITextOutput> &stream, const array_item &item, const SString &noid)
{
	sptr<IDLType> t;
	sptr<IDLNameType> nt;
	sptr<IDLMethod> method;

	if (item.is_prop) {
		nt = item.rec->PropertyAt(item.index);
		stream << "{ sizeof(effect_action_def), ";
		stream << "&" << noid << "_prop_" << nt->m_id << ", " << "NULL, ";
		if (nt->m_type->HasAttribute(kReadOnly) == false) {
			stream << noid << "_put_" << nt->m_id << ", ";
		} 
		else {
			stream << "NULL, ";
		}
		stream << noid << "_get_" << nt->m_id << ", "  << "NULL }";
	} 
	else {
		method = item.rec->MethodAt(item.index);
		
		stream << "{ sizeof(effect_action_def), ";
		stream << "&" << noid << "_method_" << method->ID() << ", ";
		stream << "NULL, NULL, NULL, ";
		stream << noid << "_invoke_" << method->ID() << " }";
	}
	return B_OK;
}

status_t WriteAutobinderArrayEntry(const sptr<ITextOutput> &stream, const array_item &item, const SString &noid)
{
	SString autobinderdef_name;

	if (item.is_prop) {
		sptr<IDLNameType> property = item.rec->PropertyAt(item.index);

		autobinderdef_name += noid;
		autobinderdef_name += "_";
		autobinderdef_name += property->m_id;
		autobinderdef_name += "_autobinderdef";
	} 
	else {
		sptr<IDLMethod> method = item.rec->MethodAt(item.index);

		autobinderdef_name += noid;
		autobinderdef_name += "_";
		autobinderdef_name += method->ID();
		autobinderdef_name += "_autobinderdef";
	}
	stream << "&" << autobinderdef_name;
	return B_OK;
}

status_t WriteArray(sptr<ITextOutput> stream, const SVector<array_item>& items,
						  SString noid, sorted_array_writer func, const SString &vartype, const SString &varname)
{
	stream << "static const " << vartype << " " << noid << "_" << varname << "[] = " << endl;
	stream << "{" << endl;
	stream << indent;
	
	size_t itemCount = items.CountItems();
	for (size_t i = 0; i < itemCount; i++) {
		const array_item& item = items[i];
		func(stream, item, noid);
		if (i != itemCount-1) stream << ",";
		stream << endl;
	}

	stream << dedent << "};" << endl;
	return B_OK;
}

status_t WriteValueSortedArray(sptr<ITextOutput> stream, InterfaceRec* base, const SVector<InterfaceRec*>& recs,
						  SString noid, sorted_array_writer func, const SString &vartype, const SString &varname)
{
	size_t recordCount = recs.CountItems();

	// First determine their SValue sort order.
	SKeyedVector<SValue, array_item> keys;
	
	for (size_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];

		int32_t propertyCount = rec->CountProperties();
		int32_t methodCount = rec->CountMethods();

		for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
			sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
			// skip reserved properties
			if (nt->m_type->HasAttribute(kReserved) == false) {
				array_item item;
				item.rec = rec;
				item.index = i_prop;
				item.is_prop = true;
				keys.AddItem(SValue::String(nt->m_id), item);
			}
		}
		for (int32_t i_method = 0; i_method < methodCount; i_method++) {
			sptr<IDLMethod> method = rec->MethodAt(i_method);
			// skip local|reserved methods
			if (method->HasAttribute(kLocal) == false && method->HasAttribute(kReserved) == false) {
				array_item item;
				item.rec = rec;
				item.index = i_method;
				item.is_prop = false;
				keys.AddItem(SValue::String(method->ID()), item);
			}
		}
	}
	
	return WriteArray(stream, keys.ValueVector(), noid, func, vartype, varname);
}

status_t WriteIndexSortedArray(sptr<ITextOutput> stream, InterfaceRec* base, const SVector<InterfaceRec*>& recs,
						  SString noid, sorted_array_writer func, const SString &vartype, const SString &varname)
{
	size_t recordCount = recs.CountItems();

	// First determine their index sort order.
	SKeyedVector<size_t, array_item> keys;
	
	for (size_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];

		int32_t propertyCount = rec->CountProperties();
		int32_t methodCount = rec->CountMethods();

		for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
			sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
			if (nt->m_index >= 0) {
				array_item item;
				item.rec = rec;
				item.index = i_prop;
				item.is_prop = true;
				keys.AddItem(nt->m_index, item);
			}
		}
		for (int32_t i_method = 0; i_method < methodCount; i_method++) {
			sptr<IDLMethod> method = rec->MethodAt(i_method);
			if (method->m_index >= 0) {
				array_item item;
				item.rec = rec;
				item.index = i_method;
				item.is_prop = false;
				keys.AddItem(method->m_index, item);
			}
		}
	}
	
	return WriteArray(stream, keys.ValueVector(), noid, func, vartype, varname);
}

sptr<Expression>
LabelForType(const sptr<IDLType> &param)
{
	if (param->GetName()=="void") {
		return new StringLiteral("B_UNDEFINED_TYPE");
	} else {
		if (IsAutobinderType(param)) {
			sptr<IDLType> banktype = FindType(param);
			if (banktype->GetCode() == B_CONSTCHAR_TYPE) {
				return new IntLiteral(B_STRING_TYPE);	
			}
			else if ((banktype->GetCode() == B_BINDER_TYPE) && param->HasAttribute(kWeak)) {
				// with the IClass* syntax, the difference between wptr and sptr is
				// just the attribute, so we need special checks anytime we are
				// outputing the actual type ("sptr"|"wptr") or code (B_BINDER_TYPE|B_BINDER_WEAK_TYPE)
				return new IntLiteral(B_BINDER_WEAK_TYPE);
			}
			return new IntLiteral(banktype->GetCode());
		} 
		else {
			return new IntLiteral(B_VALUE_TYPE);
		}
	}

}

// if you don't pass in a direction, it uses the one in the type
sptr<ArrayInitializer>
InitializerForType(const InterfaceRec* rec, const sptr<IDLType> &param, uint32_t direction = 0)
{
	sptr<ArrayInitializer> result = new ArrayInitializer;

	// type
	result->AddItem(LabelForType(param));
	if (direction == 0) {
		direction = param->GetDirection();
	}

	if (direction == kOut) {
		result->AddItem(new StringLiteral("B_OUT_PARAM"));
	}
	else if (direction == kInOut) {
		result->AddItem(new StringLiteral("B_IN_PARAM|B_OUT_PARAM"));
	}
	else {
		result->AddItem(new StringLiteral("B_IN_PARAM"));
	}

	AddAutoMarshalStructure(rec, result, param);

	return result;
}

static sptr<FunctionPrototype>
AutobinderHookPrototype(const SString &funcname)
{
	sptr<FunctionPrototype> proto = new FunctionPrototype(SString("status_t"), funcname, STATIC);
	
	proto->AddParameter(SString("const sptr<IInterface>&"), SString("target"));
	proto->AddParameter(SString("SParcel*"), SString("in"));
	proto->AddParameter(SString("SParcel*"), SString("out"));

	return proto;
}

static sptr<Function>
AutobinderLocalHookFunction(const InterfaceRec* ownerClass,
							const InterfaceRec* inClass,
							const sptr<FunctionPrototype> &proto, SString noid,
							const SVector<sptr<IDLType> > &params, SString effect_method_def,
							const sptr<IDLType> &return_type, SString func)
{
	sptr<Function> hook = new Function(proto);

	// status_t err;
	sptr<Expression> err_decl_expr = new StringLiteral("status_t err");
	sptr<Optional> err_decl = new Optional(err_decl_expr, false);
	hook->AddItem(err_decl.ptr());
	
	// BnTest *This = static_cast<BnTest*>(target.ptr());
	SString l_class;
		l_class += gNativePrefix;
		l_class += noid;
	sptr<StringLiteral> a = new StringLiteral(CastExpression(kTargetPtr, ownerClass));
	sptr<StaticCast> b = new StaticCast(l_class, 1, a.ptr());
	sptr<VariableDefinition> declaration_This = new VariableDefinition(l_class, SString("This"), 0, b.ptr(), -1, 1);
	hook->AddItem(declaration_This.ptr());
	
	// Stuff we'll need for each of the arguments
	sptr<StatementList> in_as_interfaces = new StatementList;
	sptr<StatementList> out_as_interfaces = new StatementList;
	sptr<ArrayInitializer> args_initializer = new ArrayInitializer;
	sptr<FunctionCall> call = new FunctionCall(SString("This"), SString::EmptyString(), func);
	bool has_out_parameters = false;

	// int32_t a0;
	// SString a1;
	// sptr<IBinder> a2_binder;
	// sptr<IAutobinderTest> a2;
	// etc.
	size_t paramCount = params.CountItems();
	for (size_t i_param = 0; i_param < paramCount; i_param++) {
		sptr<IDLType> t = params[i_param];
		
		SString cpptype = TypeToCPPType(inClass, t, false);
		SString varname("a");
			varname << i_param;
		
		uint32_t direction;
		if (t->GetDirection() == kInOut) {
			direction = IN_PARAM | OUT_PARAM;
			varname += "_inout";
		}
		else if (t->GetDirection() == kOut) {
			direction = OUT_PARAM;
			varname += "_out";
		}
		else {
			varname += "_in";
			direction = IN_PARAM;
		}
		bool optional = t->HasAttribute(kOptional);

		SString bindername = varname;
			bindername += "_binder";
		
		SString valuename = varname;
			valuename += "_value";

		sptr<Expression> variable_definition = new VariableDefinition(cpptype, varname);
		
		// if it's an interface, we need the intermediate IBinder 
		// and to do AsInterfaceNoInspect on it
		SString iface = t->GetIface();
		bool is_interface = iface != "" && iface != "IBinder";
		bool is_autobinder_type = IsAutobinderType(t);

		if (is_interface) {
			hook->AddItem(new VariableDefinition(SString("sptr<IBinder>"), bindername));

			if (direction & IN_PARAM) {
				sptr<FunctionCall> as_interface = new FunctionCall(iface, SString("AsInterfaceNoInspect"));
					as_interface->AddArgument(new StringLiteral(bindername));
				sptr<Expression> e = new Assignment(variable_definition, as_interface.ptr());
				in_as_interfaces->AddItem(e);
			} else {
				hook->AddItem(variable_definition);
			}
			if (direction & OUT_PARAM) {
				sptr<Expression> lval = new StringLiteral(bindername);
				sptr<Expression> f = AsBinderExpression(varname, false, false, t->HasAttribute(kWeak));
				sptr<Expression> h = new Assignment(lval, f);
				out_as_interfaces->AddItem(h.ptr());
			}
		}
		else if (!is_autobinder_type) {
			hook->AddItem(new VariableDefinition(SString("SValue"), valuename));
			
			if (direction & IN_PARAM) {
				// user defined types have a constructor taking a value
				// vector types need SetFromValue
				AddFromSValueStatements(inClass, in_as_interfaces, t, varname, valuename);
			} 
			else {
				in_as_interfaces->AddItem(variable_definition);
			}
			if (direction & OUT_PARAM) {
				sptr<Expression> e = new Literal(true, "%s = %s.AsValue()", valuename.String(), varname.String());
				out_as_interfaces->AddItem(e);
			}
		}
		else {
			hook->AddItem(variable_definition);
		}
		
		// the args in void *args[2] = { &a0_binder, &a1_binder };
		SString array_argname("&");
		if (is_interface) {
			array_argname += bindername;
		}
		else if (!is_autobinder_type) {
			array_argname += valuename;
		}
		else {
			array_argname += varname;
		}
		args_initializer->AddItem(new StringLiteral(array_argname));

		// the args in the actual function call
		if (direction & OUT_PARAM) {
			// Warning ...complications ahead...
			// Any out parameters can be optional, in which case
			// autobinder_from_parcel can null out the entry in
			// the args array (if the remote client passed a NULL).
			// Rather than using our local variable (which would mean
			// that NULL isn't passed, and change the calling semantics)
			// we need to do the following for optional parameters:
			// 1. For autobinder types
			// - pass in entry in args array (which can be null)
			// 2. For interfaces/user defined types
			// - pass in args[n] != NULL ? &variable : NULL
			// The reason we need to do this for interfaces/user defined
			// types is because the extra conversion steps that are
			// needed in these cases
			//		SValue a0_inout_value;
			//		sptr<IBinder> a1_inout_binder;
			//		void *args[2] = {&a0_inout_value,&a1_inout_binder};
			//		BFont a0_inout(a0_inout_value);
			//		sptr<INode> a1_inout = INode::AsInterfaceNoInspect(a1_inout_binder);
			if (optional) {
				if (is_interface == true || is_autobinder_type == false) {
					call->AddArgument(new Literal(true, "args[%d]!=NULL?&%s:NULL", i_param, varname.String()));
				}
				else {
					call->AddArgument(new Literal(true, "static_cast<%s*>(args[%d])", cpptype.String(), i_param));
				}
			}
			else {
				call->AddArgument(new Literal(true, "&%s", varname.String()));
			}
		} 
		else {
			if (t->GetName()=="char*") {
				varname+=".String()";
				call->AddArgument(new StringLiteral(varname));
			}
			else
				call->AddArgument(new StringLiteral(varname));
		}

		if (direction & OUT_PARAM) {
			has_out_parameters = true;
		}
	}

	// return value
	bool has_return = return_type != NULL && return_type->GetName() != "void";

	// create dummy uses of in/out if we don't need them
	if (paramCount == 0) {
		hook->AddItem(new StringLiteral("(void) in", true));
	}
	if (has_out_parameters == false && has_return == false) {
		hook->AddItem(new StringLiteral("(void) out", true));
	}

	if (paramCount > 0) {
		err_decl->SetOutput(true);

		// void *args[2] = { &a0_binder, &a1_binder };
		sptr<Expression> args_array = new VariableDefinition(SString("void"),
																SString("args"), 0,
																args_initializer.ptr(),
																paramCount, 1);
		hook->AddItem(args_array);
		
		// uint32_t dirs;
		// status_t err = autobinder_from_parcel(method_Binder, *in, args);
		hook->AddItem(new Literal(true, "uint32_t dirs"));
		hook->AddItem(new Literal(true, "err = autobinder_from_parcel(%s, *in, args, &dirs)", effect_method_def.String()));

		// if (err != B_OK) return err;
		hook->AddItem(new StringLiteral("if (err != B_OK) return err"));
	}
	
	// IN a0 = IAutobinderTest::AsInterfaceNoInspect(a0_binder);
	hook->AddItem(in_as_interfaces.ptr());

	// int32_t rv = This->Int32(a0, &a1);
	SString rv_to_autobinder_varname("NULL");
	if (has_return) {
		err_decl->SetOutput(true);

		SString varname("rv");
		SString bindername("rv_binder");

		// if it's an interface, we need the intermediate IBinder 
		// and to do AsInterfaceNoInspect on it
		SString iface = return_type->GetIface();
		bool is_interface = iface != "" && iface != "IBinder";
		bool is_autobinder_type = IsAutobinderType(return_type);

		if (is_interface) {
			sptr<Expression> as_binder = AsBinderExpression(varname, false, false, return_type->HasAttribute(kWeak));
			out_as_interfaces->AddItem(new VariableDefinition(SString("sptr<IBinder>"), bindername, 0, as_binder.ptr()));

			rv_to_autobinder_varname = "&rv_binder";
		}
		else if (!is_autobinder_type) {
			out_as_interfaces->AddItem(new Literal(true, "SValue rv_value(rv.AsValue())"));
			rv_to_autobinder_varname = "&rv_value";
		}
		else {
			rv_to_autobinder_varname = "&rv";
		}

		SString cpptype = TypeToCPPType(inClass, return_type, false);
		hook->AddItem(new VariableDefinition(cpptype, varname, 0, call.ptr()));
	} else {
		hook->AddItem(call.ptr());
	}

	// OUT a0 = IAutobinderTest::AsInterfaceNoInspect(a0_binder);
	hook->AddItem(out_as_interfaces.ptr());
	
	if (has_out_parameters || has_return) {
		// err = autobinder_to_parcel(method_Int64, args, &rv, *out);
		sptr<FunctionCall> autobinder_to_parcel = new FunctionCall(SString("autobinder_to_parcel"));
			autobinder_to_parcel->AddArgument(new StringLiteral(effect_method_def));
			if (has_out_parameters) {
				autobinder_to_parcel->AddArgument(new StringLiteral("args"));
			} else {
				autobinder_to_parcel->AddArgument(new StringLiteral("NULL"));
			}
			autobinder_to_parcel->AddArgument(new StringLiteral(rv_to_autobinder_varname));
			autobinder_to_parcel->AddArgument(new StringLiteral("*out"));
			if (paramCount > 0) {
				autobinder_to_parcel->AddArgument(new StringLiteral("dirs"));
			} else {
				autobinder_to_parcel->AddArgument(new StringLiteral("0"));
			}
		sptr<Expression> x = new StringLiteral("err");
		sptr<Expression> y = new Assignment(x, autobinder_to_parcel.ptr());
		hook->AddItem(y);
		
		// return err;
		sptr<Expression> z = new StringLiteral("err");
		sptr<Expression> ret = new Return(z);
		hook->AddItem(ret);
	} else {
		// return B_OK;
		sptr<Expression> z = new StringLiteral("B_OK");
		sptr<Expression> ret = new Return(z);
		hook->AddItem(ret);
	}
	return hook;
}

SVector<sptr<IDLType> >
MethodParamList(const sptr<IDLMethod> &method)
{
	SVector<sptr<IDLType> > params;
	
	size_t paramCount = method->CountParams();
	for (size_t i = 0; i < paramCount; i++) {
		sptr<IDLNameType> nt = method->ParamAt(i);
		params.AddItem(nt->m_type);
	}

	return params;
}

// defs for the autobinder
status_t
WriteAutobinderDefs(sptr<ITextOutput> stream, InterfaceRec *base, SVector<InterfaceRec*> &recs, SString noid)
{
	size_t index = 0;

	for (size_t rec_index=0; rec_index<recs.CountItems(); rec_index++) {
		InterfaceRec *rec = recs.ItemAt(rec_index);

		stream << "/* ------ Autobinder defs (" << rec->ID() << ") --- */" << endl;

		sptr<StatementList> statements = new StatementList;
		sptr<StatementList> prototypes = new StatementList;
		sptr<StatementList> autobinder_defs = new StatementList;
		sptr<StatementList> local_funcs = new StatementList;

		// put prototypes here in the list, and then we'll fill it in as we go through
		// the methods and properties
		statements->AddItem(prototypes.ptr());
		statements->AddItem(autobinder_defs.ptr());
		statements->AddItem(local_funcs.ptr());

		prototypes->AddItem(new Comment("Properties"));
		autobinder_defs->AddItem(new Comment("Properties"));
		local_funcs->AddItem(new Comment("Properties"));
		// Properties
	
		int32_t propertyCount = rec->CountProperties();

		for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
			sptr<IDLNameType> nt = rec->PropertyAt(i_prop);

			// skip writing if property is reserved...
			if (nt->m_type->HasAttribute(kReserved) == true) {
				nt->m_index = -1;
				continue;
			}

			nt->m_index = index++;

			SString get_name("NULL"), put_name("NULL");

			// if it's writable (TBD: not implemented yet)
			if (true /* writeonly */) {
				// status status_t CLASS_get_PROPERTY_auto_hook(...)
				SString get_hook_name;
					get_hook_name += noid;
					get_hook_name += "_";
					get_hook_name += "get_";
					get_hook_name += nt->m_id;
					get_hook_name += "_auto_hook";

				sptr<FunctionPrototype> get_hook_proto = AutobinderHookPrototype(get_hook_name);
				prototypes->AddItem(get_hook_proto.ptr());

				// BEffectMethodDef CLASS_get_PROPERTY_def = {...}
				get_name = "";
					get_name += noid;
					get_name += "_get_";
					get_name += nt->m_id;
					get_name += "_def";
				
				sptr<ArrayInitializer> get_def_initializer = new ArrayInitializer;
					get_def_initializer->AddItem(LabelForType(nt->m_type));
					AddAutoMarshalStructure(rec, get_def_initializer, nt->m_type);
					get_def_initializer->AddItem(new IntLiteral(0));
					get_def_initializer->AddItem(new StringLiteral("NULL"));
					get_def_initializer->AddItem(new StringLiteral(get_hook_name));
					get_def_initializer->AddItem(new StringLiteral("NULL"));

				sptr<VariableDefinition> get_def = new VariableDefinition(SString("BEffectMethodDef"),
																			get_name, CONST,
																			get_def_initializer.ptr(), -1);
				autobinder_defs->AddItem(get_def.ptr());

				// implementation of local get hook function
				SString get_func_name;
					get_func_name += toupper(nt->m_id.ByteAt(0));
					get_func_name += nt->m_id.String() + 1;
				SVector<sptr<IDLType> > get_params;
				sptr<Function> get_hook = AutobinderLocalHookFunction(base, rec, get_hook_proto, noid, get_params, get_name,
												nt->m_type, get_func_name);
				local_funcs->AddItem(get_hook.ptr());
			}

			if (nt->m_type->HasAttribute(kReadOnly) == false) {

				// status status_t CLASS_put_PROPERTY_auto_hook(...)
				SString put_hook_name;
					put_hook_name += noid;
					put_hook_name += "_";
					put_hook_name += "put_";
					put_hook_name += nt->m_id;
					put_hook_name += "_auto_hook";

				sptr<FunctionPrototype> put_hook_proto = AutobinderHookPrototype(put_hook_name);
				prototypes->AddItem(put_hook_proto.ptr());

				// BEffectMethodDef CLASS_put_PROPERTY_def = {...}
				put_name = "";
					put_name += noid;
					put_name += "_put_";
					put_name += nt->m_id;
					put_name += "_def";
				
				SString put_def_args_varname;
					put_def_args_varname += noid;
					put_def_args_varname += "__put_";
					put_def_args_varname += nt->m_id;
					put_def_args_varname += "_args_def";

				sptr<ArrayInitializer> put_args_initializer = new ArrayInitializer;
					put_args_initializer->AddItem(InitializerForType(rec, nt->m_type).ptr());

				// this isn't the best way to do this, we should use parameter_to_parcel,
				// but this is expedient
				sptr<VariableDefinition> put_args = new VariableDefinition(SString("BParameterInfo"),
																			put_def_args_varname, CONST,
																			put_args_initializer.ptr(), 1);
				autobinder_defs->AddItem(put_args.ptr());
				
				sptr<ArrayInitializer> put_def_initializer = new ArrayInitializer;
					put_def_initializer->AddItem(LabelForType(nt->m_type));
					AddAutoMarshalStructure(rec, put_def_initializer, nt->m_type);
					put_def_initializer->AddItem(new IntLiteral(1));
					put_def_initializer->AddItem(new Literal("%s", put_def_args_varname.String()));
					put_def_initializer->AddItem(new StringLiteral(put_hook_name));
					put_def_initializer->AddItem(new StringLiteral("NULL"));

				sptr<VariableDefinition> put_def = new VariableDefinition(SString("BEffectMethodDef"),
																			put_name, CONST,
																			put_def_initializer.ptr(), -1);
				autobinder_defs->AddItem(put_def.ptr());

				// implementation of local put hook function
				SString put_func_name;
					put_func_name += "Set";
					put_func_name += toupper(nt->m_id.ByteAt(0));
					put_func_name += nt->m_id.String() + 1;
				SVector<sptr<IDLType> > put_params;
					put_params.AddItem(nt->m_type);
				sptr<Function> put_hook = AutobinderLocalHookFunction(base, rec, put_hook_proto, noid, put_params, put_name,
												NULL, put_func_name);
				local_funcs->AddItem(put_hook.ptr());
			}


			// BAutobinderDef CLASS_PROPERTY_autobinderdef = {...}
			SString autobinderdef_name;
				autobinderdef_name += noid;
				autobinderdef_name += "_";
				autobinderdef_name += nt->m_id;
				autobinderdef_name += "_autobinderdef";

			SString key_variable("&");
				key_variable += KeyVariableName(noid, "prop", nt->m_id);
			
			if (get_name != "NULL") {
				get_name.Prepend("&");
			}
			if (put_name != "NULL") {
				put_name.Prepend("&");
			}

			sptr<ArrayInitializer> autobinderdef_initializer = new ArrayInitializer;
				autobinderdef_initializer->AddItem(new IntLiteral(nt->m_index));
				autobinderdef_initializer->AddItem(new StringLiteral(key_variable));
				autobinderdef_initializer->AddItem(new StringLiteral(put_name));
				autobinderdef_initializer->AddItem(new StringLiteral(get_name));
				autobinderdef_initializer->AddItem(new StringLiteral("NULL"));
				autobinderdef_initializer->AddItem(new StringLiteral("0"));

			sptr<VariableDefinition> autobinder_def = new VariableDefinition(SString("BAutobinderDef"),
																			autobinderdef_name, CONST,
																			autobinderdef_initializer.ptr(), -1);
			autobinder_defs->AddItem(autobinder_def.ptr());
		}

		prototypes->AddItem(new Comment("Methods"));
		autobinder_defs->AddItem(new Comment("Methods"));
		local_funcs->AddItem(new Comment("Methods"));
		// Methods

		int32_t methodCount = rec->CountMethods();
		for (int32_t i_method = 0; i_method < methodCount; i_method++) {
			sptr<IDLMethod> method = rec->MethodAt(i_method);

			// skip writing this method if it is local|reserved
			if ((method->HasAttribute(kLocal) == true) || (method->HasAttribute(kReserved) == true)) {
				method->m_index = -1;
				continue;
			}

			method->m_index = index++;

			// status status_t CLASS_invoke_METHOD_auto_hook(...)
			SString invoke_hook_name;
				invoke_hook_name += noid;
				invoke_hook_name += "_";
				invoke_hook_name += "invoke_";
				invoke_hook_name += method->ID();
				invoke_hook_name += "_auto_hook";

			sptr<FunctionPrototype> invoke_hook_proto = AutobinderHookPrototype(invoke_hook_name);
			prototypes->AddItem(invoke_hook_proto.ptr());
			
			// BParameterInfo CLASS_invoke_METHOD_args_def[...] = {...}
			SString invoke_def_args_varname;
			int arg_count = method->CountParams();
			if (arg_count > 0) {
				invoke_def_args_varname += noid;
				invoke_def_args_varname += "_invoke_";
				invoke_def_args_varname += method->ID();
				invoke_def_args_varname += "_args_def";
				
				sptr<ArrayInitializer> args_def_initializer = new ArrayInitializer;
				for (int arg_index=0; arg_index<arg_count; arg_index++) {
					sptr<ArrayInitializer> arg = InitializerForType(rec, method->ParamAt(arg_index)->m_type);
					args_def_initializer->AddItem(arg.ptr());
				}
				
				sptr<VariableDefinition> invoke_def_args = new VariableDefinition(SString("BParameterInfo"),
																			invoke_def_args_varname, CONST,
																			args_def_initializer.ptr(), arg_count);
				autobinder_defs->AddItem(invoke_def_args.ptr());
			} 
			else {
				invoke_def_args_varname = "NULL";
			}

			// BEffectMethodDef CLASS_invoke_METHOD_def = {...}
			SString invoke_def_name;
				invoke_def_name += noid;
				invoke_def_name += "_invoke_";
				invoke_def_name += method->ID();
				invoke_def_name += "_def";

			sptr<ArrayInitializer> invoke_def_initializer = new ArrayInitializer;
				invoke_def_initializer->AddItem(LabelForType(method->ReturnType()));
				AddAutoMarshalStructure(rec, invoke_def_initializer, method->ReturnType());
				invoke_def_initializer->AddItem(new IntLiteral(arg_count));
				invoke_def_initializer->AddItem(new StringLiteral(invoke_def_args_varname));
				invoke_def_initializer->AddItem(new StringLiteral(invoke_hook_name));
				invoke_def_initializer->AddItem(new StringLiteral("NULL"));
			
			sptr<VariableDefinition> invoke_def = new VariableDefinition(SString("BEffectMethodDef"),
																			invoke_def_name, CONST,
																			invoke_def_initializer.ptr(), -1);
			autobinder_defs->AddItem(invoke_def.ptr());
			
			// BAutobinderDef CLASS_METHOD_autobinderdef = {...};
			SString autobinderdef_name;
				autobinderdef_name += noid;
				autobinderdef_name += "_";
				autobinderdef_name += method->ID();
				autobinderdef_name += "_autobinderdef";

			SString key_variable("&");
				key_variable += KeyVariableName(noid, "method", method->ID());

			SString address_of_invoke_def_name;
				address_of_invoke_def_name += "&";
				address_of_invoke_def_name += invoke_def_name;

			sptr<ArrayInitializer> autobinderdef_initializer = new ArrayInitializer;
				autobinderdef_initializer->AddItem(new IntLiteral(method->m_index));
				autobinderdef_initializer->AddItem(new StringLiteral(key_variable));
				autobinderdef_initializer->AddItem(new StringLiteral("NULL"));
				autobinderdef_initializer->AddItem(new StringLiteral("NULL"));
				autobinderdef_initializer->AddItem(new StringLiteral(address_of_invoke_def_name));
				autobinderdef_initializer->AddItem(new StringLiteral("0"));

			sptr<VariableDefinition> autobinder_def = new VariableDefinition(SString("BAutobinderDef"),
																			autobinderdef_name, CONST,
																			autobinderdef_initializer.ptr(), -1);
			autobinder_defs->AddItem(autobinder_def.ptr());

			// implementation of local hook function
			SVector<sptr<IDLType> > params = MethodParamList(method);
			sptr<Function> invoke_hook = AutobinderLocalHookFunction(base, rec, invoke_hook_proto, noid, params, invoke_def_name,
											method->ReturnType(), method->ID());
			local_funcs->AddItem(invoke_hook.ptr());
		}
		statements->Output(stream);
	}

	WriteIndexSortedArray(stream, base, recs, noid, WriteAutobinderArrayEntry, SString("BAutobinderDef *"), SString("autobinder_defs"));
	return B_OK;
}

sptr<FunctionPrototype>
GetPropertyPrototype(const InterfaceRec* rec, const sptr<IDLNameType> &nt, const SString &classname)
{
	SString type, var;

	type = TypeToCPPType(rec, nt->m_type, false);
	return new FunctionPrototype(type, PropertyGetFunction(nt->m_id), VIRTUAL, CONST, classname);
}

sptr<FunctionPrototype>
PutPropertyPrototype(const sptr<IDLNameType> &nt, const SString &classname)
{
	SString type, var;

	type = TypeToCPPType(kInsideClassScope, nt->m_type, true);
	sptr<FunctionPrototype> put_prototype = new FunctionPrototype(SString("void"),
									PropertyPutFunction(nt->m_id), VIRTUAL, 0, classname);
	put_prototype->AddParameter(type, SString("value"));
	return put_prototype;
}

enum {
	VARNAME,
	VARINDEX
};
sptr<FunctionPrototype>
MethodPrototype(const InterfaceRec* rec, const sptr<IDLMethod> &method, const SString &classname, uint32_t nameform = VARNAME)
{
	SString type, var;

	if (method->ReturnType()->GetName() != "void") {
		type = TypeToCPPType(rec, method->ReturnType(), false);
	} else {
		type = "void";
	}
	
	uint32_t	cv=0;
	if (method->IsConst())
		cv=CONST;

	sptr<FunctionPrototype> method_prototype = new FunctionPrototype(type,
												method->ID(), VIRTUAL, cv, classname);

	// parameters for the method
	size_t paramCount = method->CountParams();
	for (size_t i_param = 0; i_param < paramCount; i_param++) {
		sptr<IDLNameType> nt = method->ParamAt(i_param);

		uint32_t direction=nt->m_type->GetDirection();
		if ((direction == kOut) || (direction == kInOut)) {
			type = TypeToCPPType(kInsideClassScope, nt->m_type, false);
			type += "*";
		}
		else {
			type = TypeToCPPType(kInsideClassScope, nt->m_type, true); 
		}
		
		SString name;
		if (nameform == VARINDEX) {
			name += "a";
			name << i_param;
		} else {
			name += nt->m_id;
		}

		method_prototype->AddParameter(type, name);
	}

	return method_prototype;
}


// remote proxy class
sptr<ClassDeclaration>
RemoteClassDeclaration(InterfaceRec* base, const SVector<InterfaceRec*>& recs, SString noid)
{
	SString r_class_name;
		r_class_name += gProxyPrefix;
		r_class_name += noid;
	
	sptr<ClassDeclaration> class_decl = new ClassDeclaration(r_class_name);
	class_decl->AddBaseClass(base->ID());
	class_decl->AddBaseClass(SString("BpAtom"));
	class_decl->AddItem(new StringLiteral("public:", false));
	class_decl->AddItem(new Literal(false, "%s%s(const sptr<IBinder>& o) : BpAtom(o) { }", gProxyPrefix, noid.String()));
	class_decl->AddItem(new Literal(false, "virtual sptr<IBinder> AsBinderImpl() { return sptr<IBinder>(Remote()); }"));
	class_decl->AddItem(new Literal(false, "virtual sptr<const IBinder> AsBinderImpl() const { return sptr<const IBinder>(Remote()); }"));
	class_decl->AddItem(new Literal(false, "virtual void InitAtom() { BpAtom::InitAtom(); }"));
	class_decl->AddItem(new Literal(false, "virtual status_t FinishAtom(const void* id) { return BpAtom::FinishAtom(id); }"));
	class_decl->AddItem(new Literal(false, "virtual status_t IncStrongAttempted(uint32_t flags, const void* id) { return BpAtom::IncStrongAttempted(flags, id); }"));
	class_decl->AddItem(new Literal(false, "virtual status_t DeleteAtom(const void* id) { return BpAtom::DeleteAtom(id); }"));

	const size_t recordCount = recs.CountItems();
	for (size_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];

		// each of the properties
		class_decl->AddItem(new Comment("Properties"));
		int32_t propertyCount = rec->CountProperties();
		for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
			sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
			
			// get function prototype
			if (true /* is not write only */) {
				sptr<FunctionPrototype> get_prototype = GetPropertyPrototype(kInsideClassScope, nt, r_class_name);
				class_decl->AddItem(get_prototype.ptr());
			}

			// put function prototype
			if (nt->m_type->HasAttribute(kReadOnly) == false) {
				sptr<FunctionPrototype> put_prototype = PutPropertyPrototype(nt, r_class_name);
				class_decl->AddItem(put_prototype.ptr());
			}

		}
		
		// each of the methods
		class_decl->AddItem(new Comment("Methods"));
		int32_t methodCount = rec->CountMethods();
		for (int32_t i_method = 0; i_method < methodCount; i_method++) {
			sptr<IDLMethod> method = rec->MethodAt(i_method);
			sptr<FunctionPrototype> method_prototype = MethodPrototype(kInsideClassScope, method, r_class_name);
			class_decl->AddItem(method_prototype.ptr());
		}
	}

	// tack on a private copy constructor and assignment operator with no implementations
	class_decl->AddItem(new StringLiteral("private:", false));
	class_decl->AddItem(new Comment("No implementations for copy constructor & assignment operator"));
	class_decl->AddItem(new Literal(true, "%s(const %s& o)", r_class_name.String(), r_class_name.String()));
	class_decl->AddItem(new Literal(true, "%s& operator=(%s& o)", r_class_name.String(), r_class_name.String()));

	return class_decl;
}

sptr<FunctionCall>
RemoteCall(const SString &func, const char *args, const char *rv)
{
	// Remote()->AutobinderInvoke(key_ReturnInt, &def_ReturnInt, NULL, &rv);
	sptr<FunctionCall> fc = new FunctionCall(SString("Remote()"), SString(""), SString("AutobinderInvoke"));
		fc->AddArgument(new Literal(false, "key_%s", func.String()));
		fc->AddArgument(new Literal(false, "def_%s", func.String()));
		fc->AddArgument(new StringLiteral(args));
		fc->AddArgument(new StringLiteral(rv));
	return fc;
}

void
OutputTransact(const sptr<ITextOutput> &stream, InterfaceRec* base, const SString &noid)
{
	SString autobinder_defs;
		autobinder_defs += noid;
		autobinder_defs += "_autobinder_defs";

	stream << "" << endl;
	stream << "" << endl;
	stream << "status_t" << endl;
	stream << gNativePrefix << noid << "::Transact(uint32_t code, SParcel& data, SParcel* reply, uint32_t flags)" << endl;
	stream << "{" << endl;

	if (base->HasAttribute(kLocal) == false) {
		stream << "#if USE_AUTOBINDER" << endl;
		stream << "	status_t err;" << endl;
		stream << "	if (code == B_INVOKE_TRANSACTION || code == B_GET_TRANSACTION || code == B_PUT_TRANSACTION) {" << endl;
		stream << "" << endl;
		stream << "		// in case execute_autobinder fails, save the position, so that the regular" << endl;
		stream << "		// transact can have it be the same." << endl;
		stream << "		off_t data_pos = 0, reply_pos = 0;" << endl;
		stream << "		data_pos = data.Position();" << endl;
		stream << "		if (reply) reply_pos = reply->Position();" << endl;
		stream << "		" << endl;
		stream << "		err = execute_autobinder(code, " << CastExpression(kThis, base) << ", data, reply," << endl;
		stream << "									" << autobinder_defs << ", sizeof(" << autobinder_defs << ")/sizeof(" << autobinder_defs << "[0])," << endl;
		stream << "									B_ACTIONS_SORTED_BY_KEY);" << endl;
		stream << "		" << endl;
		stream << "		if (err == B_OK) return B_OK;  // if there was an error, fall through to SValue code" << endl;
		stream << "" << endl;
		stream << "		data.SetPosition(data_pos);" << endl;
		stream << "		if (reply) reply->SetPosition(reply_pos);" << endl;
		stream << "	}" << endl;
		stream << "	" << endl;
		stream << "#endif // USE_AUTOBINDER" << endl;
	}
	
	stream << "	return BBinder::Transact(code, data, reply, flags);" << endl;
	stream << "}" << endl;
	stream << "" << endl;
	stream << "" << endl;
}

static SString
autobinder_array_entry(uint32_t direction, bool optional, const SString& originalName, const SString& localName)
{
	// Given a direction, and optional flag, the original variable, and the local variable
	// form an entry array for the argument list for AutobinderInvoke.
	// If the direction is OUT_PARAM and optional, then we have a potential NULL pointer, 
	// so make sure we pass a NULL along rather than our local address.

	SString entry;
	if ((direction & OUT_PARAM) && (optional == true)) {
		entry += "(";
		entry += originalName;
		entry += "!=NULL?&";
		entry += localName;
		entry += ":NULL)";
	}
	else {
		entry += "&";
		entry += localName;
	}
	return entry;
}


static sptr<Function>
AutobinderRemoteFunction(const sptr<FunctionPrototype> &proto, SString noid,
							const SVector<sptr<IDLType> > &params, SString effect_method_def,
							const sptr<IDLType> &return_type, SString func)
{
	sptr<Function> hook = new Function(proto);

	// Stuff we'll need for each of the arguments
	sptr<StatementList> in_as_interfaces = new StatementList;
	sptr<StatementList> out_as_interfaces = new StatementList;
	sptr<ArrayInitializer> args_initializer = new ArrayInitializer;
	bool has_out_parameters = false;

	// IN a0 = IAutobinderTest::AsInterfaceNoInspect(a0_binder);
	hook->AddItem(in_as_interfaces.ptr());

	// sptr<IBinder> a2_binder;
	size_t paramCount = params.CountItems();
	for (size_t i = 0; i < paramCount; i++) {
		sptr<IDLType> t = params[i];

		SString varname("a");
		varname << i;
		SString cpptype = TypeToCPPType(kInsideClassScope, t, false);

		uint32_t direction;
		if (t->GetDirection() == kInOut) {
			direction = IN_PARAM | OUT_PARAM;
		}
		else if (t->GetDirection() == kOut) {
			direction = OUT_PARAM;
		}
		else {
			direction = IN_PARAM;
		}
		bool optional = t->HasAttribute(kOptional);

		// in the case of c-strings
		if (t->GetName()=="char*")
		{
			SString	vs=varname;
			vs+="_String";
			SString ts("SString ");
			ts+=vs;
			ts+="(";
			ts+=varname;
			ts+=")";

			sptr <Expression> tempString = new StringLiteral(ts, true);
			hook->AddItem(tempString);
			varname=vs;
		}	

		SString bindername = varname;
			bindername += "_binder";

		SString valuename = varname;
			valuename += "_value";

		// the args in void *args[2] = { &a0_binder, &a1_binder };
		SString array_argname("(void*)");

		// if it's an interface, we need the intermediate IBinder 
		// and to do AsInterfaceNoInspect on it
		SString iface = t->GetIface();
		bool is_interface = iface != "" && iface != "IBinder";
		bool is_autobinder_type = IsAutobinderType(t);

		if (is_interface) {
			sptr<Expression> binder_decl = new VariableDefinition(SString("sptr<IBinder>"), bindername);
			bool isWeak = t->HasAttribute(kWeak);
			if (direction & IN_PARAM) {
				sptr<Expression> f = AsBinderExpression(varname, direction & OUT_PARAM, optional, isWeak);
				sptr<Expression> h = new Assignment(binder_decl, f);
				hook->AddItem(h.ptr());
			} 
			else {
				hook->AddItem(binder_decl);
			}
			if (direction & OUT_PARAM) {
				sptr<FunctionCall> as_interface = new FunctionCall(iface, SString("AsInterfaceNoInspect"));
					as_interface->AddArgument(new StringLiteral(bindername));
				sptr<Expression> parameter_variable;
				if (optional) {	
					// notice that when assigning to an out optional parameter 
					// we make sure we aren't assigning through a null pointer
					parameter_variable = new Literal(false, "if (%s) *%s", varname.String(), varname.String());
				}
				else {
					parameter_variable = new Literal(false, "*%s", varname.String());
				}
				sptr<Expression> e = new Assignment(parameter_variable, as_interface.ptr());
				out_as_interfaces->AddItem(e);
			}

			array_argname += autobinder_array_entry(direction, optional, varname, bindername);
		}
		else if (!is_autobinder_type) {
			if (direction & IN_PARAM) {
				sptr<Expression> conversion;
				if (direction & OUT_PARAM) {
					// inout/out parameters are pointers, so we need to use ->
					// in addition, don't try to do as AsValue on an optional null parameter
					if (optional) {
						conversion = new Literal(true, "SValue %s = (%s != NULL) ? %s->AsValue() : B_UNDEFINED_VALUE", 
											 valuename.String(), varname.String(), varname.String());
					}
					else {
						conversion = new Literal(true, "SValue %s = %s->AsValue()", valuename.String(), varname.String());
					}
				}
				else {
					conversion = new Literal(true, "SValue %s = %s.AsValue()", valuename.String(), varname.String());
				}
				in_as_interfaces->AddItem(conversion);
			} 
			else {
				hook->AddItem(new VariableDefinition(SString("SValue"), valuename));
			}
			if (direction & OUT_PARAM) {
				SString convert = FromSValueExpression(kInsideClassScope, t, varname, valuename, INDIRECT_ASSIGN, false);
				sptr<Expression> conversion;
				if (optional) {
					conversion = new Literal(false, "if (%s) %s", varname.String(), convert.String());
				}
				else {
					conversion = new StringLiteral(convert.String(), false);
				}
				out_as_interfaces->AddItem(conversion);
			}

			array_argname += autobinder_array_entry(direction, optional, varname, valuename);
		}
		else {
			if (!(direction & OUT_PARAM)) {
					array_argname += "&";
			}
			array_argname += varname;
		}
		
		args_initializer->AddItem(new StringLiteral(array_argname));
	}

	SString args_variable_name("NULL");
	if (paramCount > 0) {

		// void *args[2] = { &a0_binder, &a1_binder };
		sptr<Expression> args_array = new VariableDefinition(SString("void"),
																SString("args"), 0,
																args_initializer.ptr(),
																paramCount, 1);
		hook->AddItem(args_array);
		args_variable_name = "args";
	}
	
	// return value
	bool has_return = return_type != NULL && return_type->GetName() != "void";
	SString rv_variable_name("NULL");
	if (has_return) {
		SString varname("rv");
		SString bindername("rv_binder");
		SString cpptype = TypeToCPPType(kInsideClassScope, return_type, false);


		// if it's an interface, we need the intermediate IBinder 
		// and to do AsInterfaceNoInspect on it
		SString iface = return_type->GetIface();
		bool is_interface = iface != "" && iface != "IBinder";
		bool is_autobinder_type = IsAutobinderType(return_type);
		if (is_interface) {
			rv_variable_name = "&rv_binder";
			hook->AddItem(new VariableDefinition(SString("sptr<IBinder>"), SString("rv_binder")));
		}
		else if (!is_autobinder_type) {
			hook->AddItem(new StringLiteral("SValue rv_value"));
			rv_variable_name = "&rv_value";
		}
		else {
			rv_variable_name = "&rv";
			hook->AddItem(new VariableDefinition(cpptype, SString("rv")));
		}
	}

	// Remote()->AutobinderInvoke(key_, &def_, args, &rv);
	hook->AddItem(new Literal(true, "Remote()->AutobinderInvoke(&%s, %s, %s)", effect_method_def.String(), args_variable_name.String(), rv_variable_name.String()));

	hook->AddItem(out_as_interfaces.ptr());
	
	if (has_return) {
		// if it's an interface, we need the intermediate IBinder 
		// and to do AsInterfaceNoInspect on it
		SString iface = return_type->GetIface();
		bool is_interface = iface != "" && iface != "IBinder";
		bool is_autobinder_type = IsAutobinderType(return_type);
		if (is_interface) {
			SString varname("rv");
			SString bindername("rv_binder");
			SString cpptype = TypeToCPPType(kInsideClassScope, return_type, false);

			sptr<FunctionCall> as_interface = new FunctionCall(iface, SString("AsInterfaceNoInspect"));
				as_interface->AddArgument(new StringLiteral(bindername));
			sptr<Expression> e = new Return(as_interface.ptr());
			out_as_interfaces->AddItem(e);
		}
		else if (!is_autobinder_type) {
			AddFromSValueStatements(kInsideClassScope, hook, return_type, SString("rv"), SString("rv_value"));
			hook->AddItem(new StringLiteral("return rv"));
		}
		else {
			hook->AddItem(new StringLiteral("return rv"));
		}
	}
	return hook;
}

static sptr<Function>
AutobinderRemoteFunctionStub(const sptr<FunctionPrototype> &proto, SString noid,
							 const SVector<sptr<IDLType> > &params, SString effect_method_def,
							 const sptr<IDLType> &return_type, SString func)
{
	// Create a stub function that asserts when the method is declared as local
	// We don't have a remote counterpart to local methods
	// We generate potentially three expressions in this stub function:
	// 1. We create a dummy usage of all the parameters (if we have parameters)
	// 2. Call ErrFatalError("Not implemented!");
	// 3. Generate a return expression (if we have a return type)

	sptr<Function> hook = new Function(proto);

	// Create a dummy usage of all the parameters to quiet the compiler
	sptr<ParameterUse> dummyUsage = new ParameterUse();
	size_t paramCount = params.CountItems();
	if (paramCount > 0) {
		hook->AddItem(dummyUsage.ptr());
	}
	for (size_t i = 0; i < paramCount; i++) {
		SString varname("a");
		varname << i;
		dummyUsage->AddParameter(varname);
	}

	// Display an error message ... this shouldn't be called
	SString proxy_function;
	proxy_function += gProxyPrefix;
	proxy_function += noid;
	proxy_function += "::";
	proxy_function += func;
	hook->AddItem(new Literal(true, "ErrFatalError(\"%s not implemented!\")", proxy_function.String()));

	// Return the appropriate type to quiet the compiler	
	bool has_return = return_type != NULL && return_type->GetName() != "void";
	if (has_return) {
		sptr<Expression> expr = new StringLiteral(TypeToDefaultValue(kInsideClassScope, return_type));
		hook->AddItem(new Return(expr));
	}

	return hook;
}

void
WriteAutobinderPropertyGet(InterfaceRec* base, SString noid, sptr<Function> get_func, sptr<IDLNameType> property)
{
	SString cpptype = TypeToCPPType(base, property->m_type, false);

	SString iface = property->m_type->GetIface();
	bool is_interface = iface != "" && iface != "IBinder";
	bool is_autobinder_type = IsAutobinderType(property->m_type);

	if (is_interface) {
		get_func->AddItem(new VariableDefinition(SString("sptr<IBinder>"), SString("value")));
	}
	else if (!is_autobinder_type) {
		get_func->AddItem(new VariableDefinition(SString("SValue"), SString("value")));
	}
	else {
		get_func->AddItem(new VariableDefinition(cpptype, SString("value")));
	}
	
	sptr<FunctionCall> fc = new FunctionCall(SString("Remote()"), SString(""), SString("AutobinderGet"));
	fc->AddArgument(new Literal(false, "&%s_%s_autobinderdef", noid.String(), property->m_id.String()));
	fc->AddArgument(new StringLiteral("&value"));
	get_func->AddItem(fc.ptr());
	if (is_interface) {
		sptr<FunctionCall> as_interface_call = new FunctionCall(iface, SString("AsInterfaceNoInspect"));
			as_interface_call->AddArgument(new StringLiteral("value"));
		get_func->AddItem(new VariableDefinition(SString(cpptype), SString("value_interface"),
												0, as_interface_call.ptr()));
		get_func->AddItem(new StringLiteral("return value_interface"));
	}
	else if (!is_autobinder_type) {
		AddFromSValueStatements(kInsideClassScope, get_func, property->m_type, SString("rv"), SString("value"));
		get_func->AddItem(new StringLiteral("return rv"));
	}
	else {
		get_func->AddItem(new StringLiteral("return value"));
	}
}

void
WriteAutobinderPropertyPut(InterfaceRec* base, SString noid, sptr<Function> put_func, sptr<IDLNameType> property)
{
	SString iface = property->m_type->GetIface();
	bool is_interface = iface != "" && iface != "IBinder";
	bool is_autobinder_type = IsAutobinderType(property->m_type);

	SString valuename;
	if (is_interface) {
		sptr<Expression> f = AsBinderExpression(SString("value"), false, false, property->m_type->HasAttribute(kWeak));
		put_func->AddItem(new VariableDefinition(SString("sptr<IBinder>"), SString("value_binder"), 0, f));
		valuename = "&value_binder";
	}
	else if (!is_autobinder_type) {
		put_func->AddItem(new Literal(true, "SValue value_value(value.AsValue())"));
		valuename = "&value_value";
	}
	else {
		valuename = "&value";
	}
	sptr<FunctionCall> fc = new FunctionCall(SString("Remote()"), SString(""), SString("AutobinderPut"));
		fc->AddArgument(new Literal(false, "&%s_%s_autobinderdef", noid.String(), property->m_id.String()));
		fc->AddArgument(new Literal("&%s", valuename.String()));
	put_func->AddItem(fc.ptr());
}

void
WritePropertyGetStub(InterfaceRec* base, SString noid, sptr<Function> get_func, sptr<IDLNameType> property)
{
	// The get property stub just returns a default value
	// This stub can be used for all uses (Bp, Bp+Autobinder, Bn)
	get_func->AddItem(new StringLiteral("// reserved for future interface expansion", false));
	sptr<Expression> expr = new StringLiteral(TypeToDefaultValue(base, property->m_type));
	get_func->AddItem(new Return(expr));
}

void
WritePropertyPutStub(InterfaceRec* base, SString noid, sptr<Function> put_func, sptr<IDLNameType> property)
{
	// The put property stub just silences the compiler
	// the parameter to a put property is always "value"
	// This stub can be used for all uses (Bp, Bp+Autobinder, Bn)
	
	put_func->AddItem(new StringLiteral("// reserved for future interface expansion", false));
	sptr<ParameterUse> dummyUsage = new ParameterUse();
	dummyUsage->AddParameter(SString("value"));
	put_func->AddItem(dummyUsage.ptr());
}

// remote proxy class
void
WriteAutobinderRemote(InterfaceRec* base, const SVector<InterfaceRec*>& recs, SString noid, const sptr<ITextOutput> &stream)
{
	SString r_class_name;
		r_class_name += gProxyPrefix;
		r_class_name += noid;
	
	SString i_class_name;
		i_class_name += "I";
		i_class_name += noid;

	const size_t recordCount = recs.CountItems();
	for (size_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];

		// each of the properties
		stream << "/* Properties */" << endl;

		int32_t propertyCount = rec->CountProperties();
		for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
			sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
			
			// get function
			sptr<FunctionPrototype> get_prototype = GetPropertyPrototype(rec, nt, r_class_name);
			sptr<Function> get_func = new Function(get_prototype);
			if (nt->m_type->HasAttribute(kReserved) == false) {
				WriteAutobinderPropertyGet(base, noid, get_func, nt);
			}
			else {
				WritePropertyGetStub(base, noid, get_func, nt);
			}
			get_func->Output(stream);

			// put function
			if (nt->m_type->HasAttribute(kReadOnly) == false) {
				sptr<FunctionPrototype> put_prototype = PutPropertyPrototype(nt, r_class_name);
				sptr<Function> put_func = new Function(put_prototype);
				if (nt->m_type->HasAttribute(kReserved) == false) {
					WriteAutobinderPropertyPut(base, noid, put_func, nt);
				}
				else {
					WritePropertyPutStub(base, noid, put_func, nt);
				}
				put_func->Output(stream);
			}
		}
		
		// each of the methods
		stream << "/* Methods */" << endl;
		int32_t methodCount = rec->CountMethods();
		for (int32_t i_method = 0; i_method < methodCount; i_method++) {
			sptr<IDLMethod> method = rec->MethodAt(i_method);
			// even if the method is local, we need the AutobinderRemoteFunction defined
			// (with a stub implementation)
			sptr<FunctionPrototype> method_prototype = MethodPrototype(rec, method, r_class_name, VARINDEX);
			SString autobinderdef_name;
				autobinderdef_name += noid;
				autobinderdef_name += "_";
				autobinderdef_name += method->ID();
				autobinderdef_name += "_autobinderdef";
			SVector<sptr<IDLType> > params = MethodParamList(method);

			sptr<Function> method_func = NULL;
			if (method->HasAttribute(kLocal) == true || method->HasAttribute(kReserved) == true) {
				method_func = AutobinderRemoteFunctionStub(method_prototype, noid,
													   params, autobinderdef_name, 
													   method->ReturnType(), method->ID());
			}
			else {
				method_func = AutobinderRemoteFunction(method_prototype, noid,
													   params, autobinderdef_name, 
													   method->ReturnType(), method->ID());
			}
			method_func->Output(stream);
		}
	}
}

void WriteFunctionBodyStub(sptr<ITextOutput> stream, InterfaceRec* base, sptr<IDLMethod> method, SString className) 
{
	// The function body for a local stub
	// access the parameters to make the compiler happy
	// do a fatal error to tell not implemented
	// do a bogus return to make the compiler happy

	// Why these functions are written differently than the AutoBinder functions... I don't know
	// However, this is the first small step to combine them into one style (jrd - 3/2004)

	stream << "{" << endl;
	stream << indent;

	sptr<StatementList> functionBody = new StatementList();

	sptr<ParameterUse> dummyUsage = new ParameterUse();
	size_t paramCount = method->CountParams();
	if (paramCount > 0) {
		functionBody->AddItem(dummyUsage.ptr());
	}
	for (size_t i = 0; i < paramCount; i++) {
		sptr<IDLNameType> aParam = method->ParamAt(i);
		SString varname("arg_");
		varname += aParam->m_id;
		dummyUsage->AddParameter(varname);
	}

	// Display an error message ... this shouldn't be called
	SString proxy_function;
	proxy_function += className;
	proxy_function += "::";
	proxy_function += method->ID();
	functionBody->AddItem(new Literal(true, "ErrFatalError(\"%s not implemented!\")", proxy_function.String()));

	sptr<IDLType> return_type = method->ReturnType();
	bool has_return = return_type != NULL && return_type->GetName() != "void";
	if (has_return) {
		sptr<Expression> expr = new StringLiteral(TypeToDefaultValue(base, return_type));
		functionBody->AddItem(new Return(expr));
	}

	functionBody->Output(stream);
	stream << dedent;
	stream << "}" << endl;
}

void WriteRemoteFunctionBody(sptr<ITextOutput> stream, InterfaceRec* base, sptr<IDLMethod> method, SString noid) 
{
	stream << "{" << endl;
	stream << indent;

	sptr<IDLType> returnType = method->ReturnType();
	int paramCount = method->CountParams();
	int numIn = 0;
	if (paramCount > 0) {
		stream << "SValue args;" << endl;
		// don't emit the _pidgen_err definition here if
		// we don't have any out parameters
		// (otherwise it causes tons of warnings in the build)
		for (int32_t i_param = 0; i_param < paramCount; i_param++) {
			sptr<IDLNameType> nt = method->ParamAt(i_param);
			uint32_t direction = nt->m_type->GetDirection();
			if ((direction == kOut) || (direction == kInOut)) {
				stream << "status_t _pidgen_err = B_OK;" << endl;
				break;
			}
		}
		stream << endl;
	}

	bool hasOut = false;
	for (int32_t i_param = 0; i_param < paramCount; i_param++) {
		// check param direction			
		sptr<IDLNameType> nt = method->ParamAt(i_param);
		uint32_t direction = nt->m_type->GetDirection();

		if (direction == kOut) { 
			// peace of mind
		 	stream << "// [" << DirectionString(direction) << "] " << i_param << "->" << "arg_" << nt->m_id << endl;
			hasOut = true;
		}	
		else {
			SString var("arg_");
			var+=nt->m_id;

			// if this is a const char*
			if (nt->m_type->GetName()=="char*") {
				var.Append("_String");
				stream << "SString args_" << nt->m_id << "_String(args_" <<nt->m_id << ");" << endl;
			}
			numIn++;

			if (direction == kInOut) {
				hasOut = true;
			}
			if ((direction == kIn) || (direction == kInOut)) {
				stream << "// [" << DirectionString(direction) << "] " << i_param << "->" << "arg_" << nt->m_id << endl; }

			// if property is optional, we marshall a B_NULL_VALUE
			if (nt->m_type->HasAttribute(kOptional)) {
				stream << "if (arg_" << nt->m_id << " == NULL)" << endl;
				stream << "{" << indent << endl;
				stream << "args.JoinItem(" << IndexToSValue(i_param) << ", B_NULL_VALUE);" << endl;
				stream << dedent << "}" << endl;
				stream << "else" << endl << "{" << endl << indent;
			}

			stream << "args.JoinItem(" << IndexToSValue(i_param) << ", ";
			if (direction == kInOut) {
				var.Prepend("(*");
				var.Append(")");
			}
			
			stream << ToSValueConversion(nt->m_type, var) << ");" << endl;
			if (nt->m_type->HasAttribute(kOptional)) {	
				stream << dedent << "}" << endl;
			}		

			#ifdef CPPDEBUG
				if (direction==NULL)
					berr << "<----- outputcpp.cpp -----> no direction(in/out/inout) specified for param " << nt->m_id << endl; 
			#endif	
		}	
	}
	
	if (returnType->GetName() != "void" || hasOut) {
		stream << "SValue rv=";
	}
	
	stream << "Remote()->Invoke(";
	stream << noid << "_method_" << method->ID();
	if (numIn > 0) {
		stream << ", args";
	} 

	stream << ");" << endl << endl;

	for (int32_t i_param = 0; i_param < paramCount; i_param++) {
		sptr<IDLNameType> nt = method->ParamAt(i_param);
		uint32_t direction = nt->m_type->GetDirection();

		if ((direction == kOut) || (direction == kInOut)) {
			stream << "// arg_" << nt->m_id << endl;
			stream << "args = rv[" << IndexToSValue(i_param) << "];" << endl;

			// Actually, let's not do any error checking on return values, for now.
		
			SString var("args");
			SString type = TypeToCPPType(kInsideClassScope, nt->m_type, false);

			sptr<IDLType> storedtype=FindType(nt->m_type);

			if (nt->m_type->HasAttribute(kOptional)) {
				stream << "if (args != B_NULL_TYPE)" << endl << "{" << endl;
				stream << indent;
			}

			SString argname("arg_");
			argname.Append(nt->m_id);
			stream << FromSValueExpression(kInsideClassScope, nt->m_type, argname, var, INDIRECT_ASSIGN, true) << endl;
			stream << "DbgOnlyFatalErrorIf(_pidgen_err != B_OK && _pidgen_err != B_BINDER_DEAD, \"bad binder effect return type: " << base->ID() << "." << method->ID() << " / " << nt->m_id << "\");" << endl;
			if (nt->m_type->HasAttribute(kOptional)) {	
				stream << dedent << "}" << endl;
			}
			stream << endl;
		}
	}

	if (returnType->GetName()!="void") {
		sptr<IDLType> st=FindType(returnType);
		SString var("rv[");
		var.Append(noid);
		var.Append("_");
		var.Append("key_res]");
		stream << FromSValueExpression(kInsideClassScope, returnType, SString("rv_obj"), var, RETURN, false) << endl;
	}

	stream << dedent;
	stream << "}" << endl;
}

void WriteFunctionPrototype(sptr<ITextOutput> stream, InterfaceRec* base, sptr<IDLMethod> method, SString className)
{
	stream << endl;
	sptr<IDLType> returnType = method->ReturnType();

	SString type = TypeToCPPType(base, returnType, false);
	stream << "" << type << endl;
	stream << className << "::" << method->ID() << "(";
	
	int paramCount = method->CountParams();
	for (int32_t i_param = 0; i_param < paramCount; i_param++) {
		sptr<IDLNameType> nt = method->ParamAt(i_param);
		uint32_t direction = nt->m_type->GetDirection();

		if ((direction == kOut) || (direction == kInOut)) {	
			type = TypeToCPPType(kInsideClassScope, nt->m_type, false); 
			stream << type << "* arg_" << nt->m_id;	
		}
		else { 
			type = TypeToCPPType(kInsideClassScope, nt->m_type, true); 
			stream << type << " arg_" << nt->m_id;
		}
		
		if (i_param != paramCount-1) {
			stream << ", ";
		}
	}
	
	if (method->IsConst()) {
		stream << ") const " << endl;
	}
	else {
		stream << ")" << endl;
	}
}

void WriteRemoteFunction(sptr<ITextOutput> stream, InterfaceRec* base, sptr<IDLMethod> method, SString noid, SString className)
{
	WriteFunctionPrototype(stream, base, method, className);

	if ((method->HasAttribute(kLocal) == true) || (method->HasAttribute(kReserved) == true)) {
		WriteFunctionBodyStub(stream, base, method, className);
	}
	else {
		WriteRemoteFunctionBody(stream, base, method, noid);
	}
}

void 
WritePropertyGet(sptr<ITextOutput> stream, InterfaceRec* base, SString noid, SString className, sptr<IDLNameType> property)
{
	// Yes, this should be converted to use AST, but I wanted to keep matching current
	// generated output, and I couldn't get an exact match with AST

	SString type = TypeToCPPType(base, property->m_type, false);
			
	stream << type << endl;
	stream << className << "::" << PropertyGetFunction(property->m_id) << "() const" << endl;
	stream << "{" << endl;
	stream << indent;
	stream << "SValue rv(Remote()->Get(" << noid << "_prop_" << property->m_id << "));" << endl;
	SString var("rv");
	stream << FromSValueExpression(kInsideClassScope, property->m_type, SString("rv_obj"), var, RETURN, false) << endl;
	stream << dedent;
	stream << "}" << endl;
}

void 
WritePropertyPut(sptr<ITextOutput> stream, InterfaceRec* /*base*/, SString noid, SString className, sptr<IDLNameType> property)
{
	SString type = TypeToCPPType(kInsideClassScope, property->m_type, true);
	stream << "void" << endl;
	stream << className << "::" << PropertyPutFunction(property->m_id) << "(" << type << " value)" << endl;
	stream << "{" << endl;
	stream << indent;
	stream << "SValue val(" << noid << "_prop_" << property->m_id << ", ";
	SString var("value");
	stream << ToSValueConversion(property->m_type, var) << ");" << endl;
	stream << "Remote()->Put(val);" << endl;
	stream << dedent;
	stream << "}" << endl;
}

// remote proxy class
status_t
WriteRemoteClass (sptr<ITextOutput> stream, InterfaceRec* base, const SVector<InterfaceRec*>& recs, SString noid)
{
	SString r_class_name;
		r_class_name += gProxyPrefix;
		r_class_name += noid;

	const size_t recordCount = recs.CountItems();
	for (size_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];

		stream << endl << endl;

		/* ------- Properties --------------------- */
		int32_t propertyCount = rec->CountProperties();
		stream << "/* Properties */" << endl;
		for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
			sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
			stream << endl;

			if (nt->m_type->HasAttribute(kReserved) == false) {
				WritePropertyGet(stream, rec, noid, r_class_name, nt);
			}
			else {
				sptr<FunctionPrototype> get_prototype = GetPropertyPrototype(rec, nt, r_class_name);
				sptr<Function> get_func = new Function(get_prototype);
				WritePropertyGetStub(rec, noid, get_func, nt);
				get_func->Output(stream);
			}
			
			if (nt->m_type->HasAttribute(kReadOnly) == false) {
				if (nt->m_type->HasAttribute(kReserved) == false) {
					WritePropertyPut(stream, rec, noid, r_class_name, nt);
				}
				else {
					sptr<FunctionPrototype> put_prototype = PutPropertyPrototype(nt, r_class_name);
					sptr<Function> put_func = new Function(put_prototype);
					WritePropertyPutStub(rec, noid, put_func, nt);
					put_func->Output(stream);
				}
			}
		}
		
		/* ------- Methods ------------------------ */
		int32_t methodCount = rec->CountMethods();
		stream << "/* Methods */" << endl;
		for (int32_t i_method = 0; i_method < methodCount; i_method++) {
			sptr<IDLMethod> method = rec->MethodAt(i_method);
			WriteRemoteFunction(stream, rec, method, noid, r_class_name);
		}
	}
		
	return B_OK;
}

status_t WriteLocalClass(sptr<ITextOutput> stream, InterfaceRec* base, const SVector<InterfaceRec*>& recs, SString noid)
{
	const size_t recordCount = recs.CountItems();
	SString class_name;
		class_name += gNativePrefix;
		class_name += noid;

	stream << "/* ------ Local Class -------------------------- */" << endl << endl;
	
	stream << "SValue" << endl
		<< class_name << "::Inspect(const sptr<IBinder>& /*caller*/, const SValue &which, uint32_t)" << endl
		<< "{" << endl << indent;
	if (recordCount <= 1) {
		stream << "return which * SValue(" << recs[0]->ID() << "::Descriptor(), SValue::Binder(this));"
			<< dedent << endl << "}" << endl;
	} 
	else {
		stream << "SValue result(which * SValue(" << recs[0]->ID() << "::Descriptor(), SValue::Binder(this)));" << endl;
		for (int32_t i_record = 1; i_record < recordCount; i_record++) {
			stream << "result.Join(which * SValue(" << recs[i_record]->ID() << "::Descriptor(), SValue::Binder(this)));" << endl;
		}
		stream << "return result;" << dedent << endl << "}" << endl;
	}

	stream << endl << "sptr<IInterface>" << endl
		<< class_name << "::InterfaceFor(const SValue &desc, uint32_t flags)" << endl
		<< "{" << endl << indent;
	// Notice that in the case of multiple inheritance, that the cast here in InterfaceFor we want the
	// actual class that derives from IInterface.
	// For example, in the case of:
	//	class ICatalog : public INode, IIterable
	// The casts are as follows:
	//	desc == INode::Descriptor()) -> static_cast<INode*>(this)
	//	desc == IIterator::Descriptor() -> static_cast<IIterator*>(this)
	//	desc == ICatalog::Descriptor() -> static_cast<INode*>(this)
	// This is different than the marshalling/unmarshalling where we would always cast to INode.
	// The reason for this is that IIterable::AsInterface(this) is called, we need the IInterface
	// that corresponds to the IIterable.
	// (So we call rec->LeftMostBase() rather than base->LeftMostBase(). For IIterable this
	// would return IIterable.  In contrast, CastExpression() would always result in INode.)
	for (int32_t i_record = 0; i_record < recordCount; i_record++) {
		stream << "if (desc == " << recs[i_record]->ID() << "::Descriptor()) return sptr<IInterface>(static_cast<" << recs[i_record]->LeftMostBase() << "*>(this));" << endl;
	}
	stream << "return BBinder::InterfaceFor(desc, flags);" << dedent << endl << "}" << endl;
	stream << endl;

	stream << class_name << "::" << class_name << "()" << endl;
	stream << "{" << endl;
	stream << "}" << endl << endl;
	
	stream << class_name << "::" << class_name << "(const SContext& context)" << endl;
	stream << indent << ":\tBBinder(context)" << dedent << endl;
	stream << "{" << endl;
	stream << "}" << endl << endl;

	stream << class_name << "::~" << class_name << "()" << endl;
	stream << "{" << endl;
	stream << "}" << endl << endl;
	
	stream << "sptr<IBinder>" << endl;
	stream << class_name << "::AsBinderImpl()" << endl;
	stream << "{" << endl;
	stream << indent << "return this;" << dedent << endl;
	stream << "}" << endl << endl;

	stream << "sptr<const IBinder>" << endl;
	stream << class_name << "::AsBinderImpl() const" << endl;
	stream << "{" << endl;
	stream << indent << "return this;" << dedent << endl;
	stream << "}" << endl << endl;

	stream << "status_t" << endl;
	stream << class_name << "::HandleEffect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)" << endl;
	stream << "{" << endl;
	stream << indent;
	
	if (base->HasAttribute(kLocal) == false) {
		stream << "return execute_effect(" 
			<< CastExpression(kThis, base) << "," << endl;
		stream << "\t\t\t\tin, inBindings, outBindings," << endl;
		stream << "\t\t\t\tout, " << noid << "_actions, " << endl;
		stream << "\t\t\t\tsizeof(" << noid << "_actions)/sizeof(" << noid << "_actions[0])," << endl;
		stream << "\t\t\t\tB_ACTIONS_SORTED_BY_KEY);" << endl;
	}
	else {
		stream << "return BBinder::HandleEffect(in, inBindings, outBindings, out);" << endl;
	}

	stream << dedent;
	stream << "}" << endl;
	stream << endl;

	// Write reserved property/method implementations
	bool didHeader = false;
	for (int32_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];

		int32_t propertyCount = rec->CountProperties();
		for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
			sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
			if (nt->m_type->HasAttribute(kReserved)) {
				if (didHeader == false) {
					stream << endl;
					stream << "/* ------- Reserved Properties/Methods ---- */" << endl;
					stream << endl;
					didHeader = true;
				}
				sptr<FunctionPrototype> get_prototype = GetPropertyPrototype(rec, nt, class_name);
				sptr<Function> get_func = new Function(get_prototype);
				WritePropertyGetStub(rec, noid, get_func, nt);
				get_func->Output(stream);
			
				if (nt->m_type->HasAttribute(kReadOnly) == false) {
					sptr<FunctionPrototype> put_prototype = PutPropertyPrototype(nt, class_name);
					sptr<Function> put_func = new Function(put_prototype);
					WritePropertyPutStub(rec, noid, put_func, nt);
					put_func->Output(stream);
				}
			}
		}

		int32_t methodCount = rec->CountMethods();
		for (int32_t i_method = 0; i_method < methodCount; i_method++) {
			sptr<IDLMethod> method = rec->MethodAt(i_method);
			if (method->HasAttribute(kReserved)) {
				if (didHeader == false) {
					stream << endl;
					stream << "/* ------- Reserved Properties/Methods ---- */" << endl;
					stream << endl;
					didHeader = true;
				}
					
				WriteFunctionPrototype(stream, rec, method, class_name);
				WriteFunctionBodyStub(stream, rec, method, class_name);
			}
		}
	}

	// Write the local Push Property methods
	didHeader = false;
	for (int32_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];

		int32_t propertyCount = rec->CountProperties();
		for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
			sptr<IDLNameType> nt = rec->PropertyAt(i_prop);

			// If this property is reserved, just skip to next...
			if (nt->m_type->HasAttribute(kReserved) == true) {
				continue;
			}

			if (didHeader == false) {
				stream << "/* ------- Push Properties ------------------------- */" << endl;
				didHeader = true;
			}

			SString type = TypeToCPPType(kInsideClassScope, nt->m_type, true);
			stream << "void" << endl;
			stream << class_name << "::Push" << (char) toupper(nt->m_id.ByteAt(0)) << nt->m_id.String()+1 << "(" << type << " newValue)" << endl;
			stream << "{" << endl;
			stream << indent;
			stream << "if (IsLinked()) {" << endl << indent;
			stream << "const SValue pushValue(" << noid << "_prop_" << nt->m_id << ", " << ToSValueConversion(nt->m_type, SString("newValue")) << ");" << endl;
			stream << "Push(pushValue);" << endl;
			stream << dedent;
			stream << "}" << endl;
			stream << dedent;
			stream << "}" << endl;
			stream << endl;
		}
	}
		
	/* ------- Events ------------------------- */
	didHeader = false;
	for (int32_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];

		int32_t eventCount = rec->CountEvents();
		for (int32_t i_event = 0; i_event < eventCount; i_event++) {
			sptr<IDLEvent> event = rec->EventAt(i_event);

			if (didHeader == false) {
				stream << "/* ------- Events ------------------------- */" << endl;
				didHeader = true;
			}

			stream << endl;			
			stream << "void" << endl;
			stream << class_name << "::" << "Push" << event->ID() << "(";
			
			int32_t paramCount = event->CountParams();
			for (int32_t i_param =0; i_param < paramCount; i_param++) {
				sptr<IDLNameType> nt = event->ParamAt(i_param);
				SString type = TypeToCPPType(kInsideClassScope, nt->m_type, true);
				stream << type << " " << nt->m_id;
				
				if (i_param != paramCount-1) stream << ", ";
			}
			
			stream << ")" << endl;
			stream << "{" << endl;
			stream << indent;
			stream << "if (IsLinked()) {" << endl << indent;

			if (paramCount > 0) {
				stream << "SValue args;" << endl;
			}
			for (int32_t i_param = 0; i_param < paramCount; i_param++) {
				sptr<IDLNameType> nt = event->ParamAt(i_param);
				stream << "args.JoinItem(" << IndexToSValue(i_param);
				stream << ", " << ToSValueConversion(nt->m_type, nt->m_id) << ");" << endl;
			}
			if (paramCount > 0) {
				stream << "Push(SValue(" << noid << "_event_" << event->ID() << ", args));" << endl;
			} 
			else {
				stream << "Push(SValue(" << noid << "_event_" << event->ID() << ", B_NULL_VALUE));" << endl;
			}
			
			stream << dedent;
			stream << "}" << endl;
			stream << dedent;
			stream << "}" << endl;
		}
	}
	
	return B_OK;
}

void WriteAllKeys(sptr<ITextOutput> stream, InterfaceRec* base, const SVector<InterfaceRec*>& recs, const SString& noid)
{
	const size_t recordCount = recs.CountItems();
	for (size_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];
		size_t propertyCount = rec->CountProperties();
		for (size_t i_prop = 0; i_prop < propertyCount; i_prop++) {
			sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
			if (nt->m_type->HasAttribute(kReserved) == false) {
				WriteKey(stream, noid, "prop", nt->m_id);
			}
		}
		
		// We don't need any method keys for local interfaces
		if (base->HasAttribute(kLocal) == false) {
			size_t methodCount = rec->CountMethods();
			for (size_t i_method = 0; i_method < methodCount; i_method++) {
				sptr<IDLMethod> method = rec->MethodAt(i_method);
				if ((method->HasAttribute(kLocal) == false) && (method->HasAttribute(kReserved) == false)) {
					WriteKey(stream, noid, "method", method->ID());
				}
			}
		}

		size_t eventCount = rec->CountEvents();
		for (size_t i_event = 0; i_event < eventCount; i_event++) {
			sptr<IDLEvent> event = rec->EventAt(i_event);
			WriteKey(stream, noid, "event", event->ID());
		}
	}

	stream << endl;
}

status_t WriteCPP(sptr<ITextOutput> stream, 
				SVector<InterfaceRec *> &recs,
				const SString & filename,
				const SString & lHeader,
				bool systemHeader)
{
	SString			type, var;

	NamespaceGenerator nsGen;

	SString printablefn=filename;
	printablefn.Append(".cpp");
	printablefn.RemoveFirst(".\\");

	stream << "/*========================================================== */ " << endl;
	stream << "// "<< printablefn << " is automatically generated by PIDGEN - DO NOT MODIFY" << endl;
	stream << "/*========================================================== */ " << endl << endl;

	stream << endl;
	stream << "#ifndef USE_AUTOBINDER" << endl;
	stream << "#define USE_AUTOBINDER 1" << endl;
	stream << "#endif // ifndef USE_AUTOBINDER" << endl;
	stream << endl;

	#if TARGET_HOST == TARGET_HOST_WIN32
		SString winHeader=lHeader;
		//bout << "winHeader="<< winHeader << endl;
		winHeader.ReplaceAll("\\", "/");
		//bout << "winHeader="<< winHeader << endl;
		stream << "#include <" << winHeader << ">" << endl;
	#else
		stream << "#include " << (systemHeader ? '<' : '\"') << lHeader << (systemHeader ? '>' : '\"') << endl;
	#endif

	stream << endl;
	stream << "#include <support/Binder.h>" << endl;
	if (g_writeAutobinder) {
		stream << "#include <support/Autobinder.h>" << endl;
	}
	stream << "#include <support/Debug.h>" << endl;
	stream << "#include <ErrorMgr.h>" << endl;
	stream << endl;
	
	// Write any typemarshallers we might need.  We'll let the
	// linker strip any that aren't actually used, instead of
	// trying to figure out here exactly which ones we actually need.
	// XXX Remove when we don't have to deal with $&#%*&%@*&@%!!! ADS
	// and our lovely template class in Autobinder.h!
	const SKeyedVector<SString, sptr<IDLType> >& tb = getTypeBank();
	size_t tbCount = tb.CountItems();
	for (size_t tb_rec = 0; tb_rec < tbCount; tb_rec++) {
		const sptr<IDLType>& t = tb.ValueAt(tb_rec);
		if (IsAutoMarshalType(t)) {
			SString name(TypeToCPPType(NULL, t, false));
			stream << "static const PTypeMarshaller "
				<< TypeToMarshaller(NULL, t) << " = {" << endl << indent
				<< "sizeof(PTypeMarshaller)," << endl
				<< "&" << name << "::MarshalParcel," << endl
				<< "&" << name << "::UnmarshalParcel," << endl
				<< "&" << name << "::MarshalValue," << endl
				<< "&" << name << "::UnmarshalValue" << endl << dedent
				<< "};" << endl << endl;
		}
	}

	size_t recordCount = recs.CountItems();
	for (size_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];
	
		SString noid=rec->ID();	
		noid.RemoveFirst("I");

		if (rec->Declaration()==IMP) {
		#ifdef CPPDEBUG
			bout << "OutputCPP.cpp - " << rec->ID() << " is an imported interface " << endl;
		#endif
		}	

		if (rec->Declaration()==FWD) {
			nsGen.EnterNamespace(stream, rec->Namespace(), rec->CppNamespace());

			stream << "/* ------ Forward Declaration ---------------------- */" << endl;
			stream << endl;

			SVector<SString> parents = rec->Parents();
			if (parents.CountItems() == 0) {
				stream << "class " << rec->ID() << " : public IInterface" << endl;
			} else {
				stream << "class " << rec->ID() << " : ";
				size_t parentCount = parents.CountItems();
				for (size_t i_parent = 0; i_parent < parentCount; i_parent++) {
					if (i_parent > 0) {
						stream << ", ";
					}
					stream << "public " << parents[i_parent];
				}
				stream << endl;
			}

			stream << "{" << endl;
			stream << "public:" << endl;
			
			stream << indent;
			
			stream << "B_DECLARE_META_INTERFACE(" << noid << ")" << endl << endl;
			stream << dedent << "};" << endl << endl;
		}	

		if (rec->Declaration()==DCL) {	
			nsGen.EnterNamespace(stream, rec->Namespace(), rec->CppNamespace());

			/* ------ Interface Keys ---------------------- */
			stream << "/* ------ Interface Keys ---------------------- */" << endl;
			stream << endl;
		
			stream << "B_STATIC_STRING_VALUE_SMALL(" << noid << "_key_res," << PadString(SString("res"), 4)
			<< "\"" << "res" << "\", " << ");" << endl;

			SVector<InterfaceRec*> allInterfaces;
			CollectParents(rec, recs, &allInterfaces);
			WriteAllKeys(stream, rec, allInterfaces, noid);

			/* ------ Class Members ---------------------- */
			stream << "/* ------ Class Members ---------------------- */" << endl;
			stream << endl;
			// Currently our only class member is an empty destructor.
			// But this is needed for the vtable copy needed for RVDS, and C++ library support
			stream << rec->ID() << "::~" << rec->ID() << "()" << endl;
			stream << "{" << endl;
			stream << "}" << endl << endl;

			// If the interface is local, then we don't need to write any of the ...
			// interface hooks, autobinder definitions, or remote class

			if (rec->HasAttribute(kLocal) == false) {
				WriteInterfaceHooks(stream, rec, allInterfaces, noid);

				stream << "/* ------ Interface Action Array ------------------------- */" << endl;
				WriteValueSortedArray(stream, rec, allInterfaces, noid, WriteActionArrayEntry, SString("effect_action_def"), SString("actions"));


				if (g_writeAutobinder) {
					stream << "#if USE_AUTOBINDER" << endl;
					WriteAutobinderDefs(stream, rec, allInterfaces, noid);
					stream << "#endif // USE_AUTOBINDER" << endl;
				}
				stream << "/* ------ Remote Class ------------------------- */" << endl << endl;
				sptr<ClassDeclaration> remote_class_decl = RemoteClassDeclaration(rec, allInterfaces, noid);
				remote_class_decl->Output(stream);
				


				if (g_writeAutobinder) {
					stream << "#if USE_AUTOBINDER" << endl;
					WriteAutobinderRemote(rec, allInterfaces, noid, stream);
					stream << "#else // USE_AUTOBINDER" << endl;
				}

				// non autobinder code hasn't been updated in a long time, 
				// if/when we want this, then use WriteRemoteClass, but then 
				// go through and update/fix written code
				stream << "// non USE_AUTOBINDER implementation not currently supported" << endl;
				// WriteRemoteClass(stream, rec, allInterfaces, noid);
				if (g_writeAutobinder) {
					stream << "#endif // USE_AUTOBINDER" << endl;
				}

				stream << "B_IMPLEMENT_META_INTERFACE(" << noid << ", \"" 
					<< rec->FullInterfaceName() << "\", "
					<< rec->LeftMostBase() << ");" << endl;

			}
			else {
				stream << "B_IMPLEMENT_META_INTERFACE_LOCAL(" << noid << ", \"" 
					<< rec->FullInterfaceName() << "\", " 
					<< rec->LeftMostBase() << ");" << endl;

				stream << "/* ------ Interface is local  ------------------------- */" << endl;
				stream << "/* ------ No generation of remote class or hooks  ------------------------- */" << endl;
			}
			stream << endl;
			stream << endl;

			WriteLocalClass(stream, rec, allInterfaces, noid);

			if (g_writeAutobinder) {
				OutputTransact(stream, rec, noid);
			}

		}

	}

	nsGen.CloseNamespace(stream);

	return B_OK;
}
