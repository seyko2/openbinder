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

#include "OutputI.h"
#include "OutputUtil.h"
#include "TypeBank.h"

#include <support/StdIO.h>
#include <ctype.h>

extern bool g_writeAutobinder; // XXX in OutputCPP.cpp, should be in a header and set by a command line flag
extern sptr<IDLType> FindType(const sptr<IDLType>& typeptr);	// in main.cpp

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

// No longer printing keys in public headers, as we can't export data.
// Maybe some day this will change.
#define KEYS_IN_HEADER 0

enum {
	MODIFIER_TABS = 2,
	TYPE_TABS = 6,
	TOTAL_TABS = 8,
	PARM_TABS = 9
};

#if KEYS_IN_HEADER
static void
WriteKey(sptr<ITextOutput> stream, const char* prefix, const SString& key)
{
	// Real length is Length() + null terminator.
	if (key.Length() > B_TYPE_MAX_LENGTH)
		stream << "static\tvalue_clrg\t\t\t" << prefix << "_" << key << ";" << endl;
	else
		stream << "static\tvalue_csml\t\t\t" << prefix << "_" << key << ";" << endl;
}
#endif

static void 
WritePropertyDeclarations(sptr<ITextOutput> stream, sptr<IDLNameType> propertyType, bool isAbstract)
{
	// Write both get/set property method declarations into a class defintion
	// append "= 0" if method should be abstract
	
	// The associated comment goes with both the get and set method
	stream << dedent;
	propertyType->OutputComment(stream);
	stream << indent;
	SString type = TypeToCPPType(kInsideClassScope, propertyType->m_type, false);
	stream << "virtual\t" << type << PadString(type, TYPE_TABS) << (char) toupper(propertyType->m_id.ByteAt(0)) << propertyType->m_id.String()+1 << "() const";
	if (isAbstract) {
		stream << " = 0";
	}
	stream << ";" << endl;

	if (propertyType->m_type->HasAttribute(kReadOnly) == false) {	
		type = TypeToCPPType(kInsideClassScope, propertyType->m_type, true);
		stream << dedent;
		propertyType->OutputComment(stream);
		stream << indent;
		stream << "virtual\tvoid\t\t\t\t\tSet" << (char) toupper(propertyType->m_id.ByteAt(0)) << propertyType->m_id.String()+1 << "(" << type << " value)";
		if (isAbstract) {
			stream << " = 0";
		}
		stream << ";" << endl;
	}
}

static void
WriteMethodDeclaration(sptr<ITextOutput> stream, sptr<IDLMethod> method, bool inInterface)
{
	sptr<IDLType> returnType = method->ReturnType();
	SString type = TypeToCPPType(kInsideClassScope, returnType, false);
	stream << dedent;
	method->OutputComment(stream);	
	stream << indent;
	stream << "virtual\t" << type << PadString(type, TYPE_TABS) << method->ID() << "(";
	
	// if the parameters have comments, then we will have newlines and this indenting will
	// have an effect, normally it will be ignored...
	for (int32_t i = 0; i < PARM_TABS; i++) {
		stream << indent;
	}

	int32_t paramCount = method->CountParams();
	for (int32_t i_param = 0; i_param < paramCount; i_param++) {
		sptr<IDLNameType> nt = method->ParamAt(i_param);
		// start off with the comment (if it is a line comment, it will end with a newline)
		if (inInterface && nt->HasComment()) {
			stream << endl;
			nt->OutputComment(stream);
		}
		uint32_t direction = nt->m_type->GetDirection();
		bool isOptional = nt->m_type->HasAttribute(kOptional);
		if ((direction == kInOut) || (direction == kOut)) {	
			type = TypeToCPPType(kInsideClassScope, nt->m_type, false);
			// do we want to distinguish array types here?
			// if so, we would need to lookup type and check for B_VARIABLE_ARRAY_TYPE
			stream << type << "* " << nt->m_id;	
			if (isOptional) {
				stream << " = NULL";
			}
		}
		else { 
			SString type = TypeToCPPType(kInsideClassScope, nt->m_type, true); 
			stream << type << " " << nt->m_id;
			if (isOptional) {
				stream << " = " << TypeToDefaultValue(kInsideClassScope, nt->m_type);
			}
		}
		
		// end the parameter with comma (if not the last)
		if (i_param < paramCount-1) {
			stream << ", ";
		}
	}

	if (inInterface && method->HasTrailingComment()) {
		stream << endl;
		method->OutputTrailingComment(stream);
	}
	stream << ")";
	if (method->IsConst()) {
		stream << " const";
	}
	// if the method declaration is inside the interface, then
	// it is abstract also
	if (inInterface) {
		stream << " = 0";
	}

	// clean up our comment indenting
	for (int32_t i = 0; i < PARM_TABS; i++) {
		stream << dedent;
	}

	stream << ";" << endl;
}

status_t
WriteIHeader(sptr<ITextOutput> stream, SVector<InterfaceRec *> &recs,
				const SString &filename, SVector<IncludeRec> & headers,
				const sptr<IDLCommentBlock>& beginComments,
				const sptr<IDLCommentBlock>& endComments,
				bool system)
{
	NamespaceGenerator nsGen;
	SString headerGuard;
	status_t err = HeaderGuard(filename, headerGuard, system);
	if (err != B_OK) return err;
	
	stream << "/*========================================================== */ " << endl;
	stream << "// header file automatically generated by PIDGEN - DO NOT MODIFY" << endl;
	stream << "/*========================================================== */ " << endl << endl;

	stream << "#ifndef " << headerGuard << endl;
	stream << "#define " << headerGuard << endl;
	stream << endl;

	stream << "#include <support/IInterface.h>" << endl;
	stream << "#include <support/Binder.h>" << endl;
	stream << "#include <support/Context.h>" << endl;
	stream << "#include <support/String.h>" << endl;
	stream << endl;

	size_t count = headers.CountItems();
	for (size_t i=0; i<count; i++) {
		//bout << "// header comments: " << headers[i].Comments() << endl;
		if (headers[i].Comments() != NULL)
			headers[i].Comments()->Output(stream, false);
		stream << "#include " << "<" << headers[i].File() << ">" << endl;
	}
	if (count > 0) stream << endl;
	

	//bout << "// begin comments: " << beginComments << endl;
	if (beginComments != NULL) {
		beginComments->Output(stream, false);
		stream << endl;
	}

	// Write all forward declarations
	bool didHeader = false;
	size_t recordCount = recs.CountItems();
	for (size_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];
		SString noid=rec->ID();
		if (rec->Declaration()==FWD) {
			if (!didHeader) {
				stream << "/* ============= Forward Declarations =========================== */" << endl;
				stream << endl;
				didHeader = true;
			}
			nsGen.EnterNamespace(stream, rec->Namespace(), rec->CppNamespace());
			stream << "class " << rec->ID() << ";" << endl;
	  }
	}
	if (didHeader) {
		stream << endl;
	}

	// Write class definitions
	stream << "/* ============= Interface Class Declarations =========================== */" << endl;
	stream << endl;
	for (size_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];
		SString noid=rec->ID();
		noid.RemoveFirst("I");
		if (rec->Declaration()==DCL) {
			nsGen.EnterNamespace(stream, rec->Namespace(), rec->CppNamespace());
			rec->OutputComment(stream, false);
			SVector<SString> parents = rec->Parents();
			if (parents.CountItems() == 0) {
				stream << "class " << rec->ID() << " : public IInterface" << endl;
			} 
			else {
				stream << "class " << rec->ID() << " : ";
				for (size_t i_parent = 0; i_parent < parents.CountItems(); i_parent++) {
					if (i_parent > 0) {
						stream << ", ";
					}
					stream << "public " << parents[i_parent];
				}
				stream << endl;
			}
			stream << "{" << endl;

			// Write Class body...
			int32_t typedefCount = rec->CountTypedefs();
			int32_t propertyCount = rec->CountProperties();
			int32_t methodCount = rec->CountMethods();
			int32_t eventCount = rec->CountEvents();
			int32_t constructCount = rec->CountConstructs();

			stream << "public:" << endl;
			stream << indent;
			stream << "B_DECLARE_META_INTERFACE(" << noid << ")" << endl << endl;

			// print all typedefs
			if (typedefCount != 0) {
				stream << endl;
				stream << "/* ------- Typedefs --------------------- */" << endl;
			}
			for (int32_t i_typedef = 0; i_typedef < typedefCount; i_typedef++) {
				sptr<IDLNameType> def = rec->TypedefAt(i_typedef);
				stream << dedent;
				def->OutputComment(stream);
				stream << indent;
				stream << "typedef ";
				// get the base type for this typedef and convert that
				sptr<IDLType> baseType = FindType(def->m_type);
				SString cppType = TypeToCPPType(kInsideClassScope, baseType, false);
				if (baseType->GetCode() == B_VARIABLE_ARRAY_TYPE) {
					stream << "SVector<" << cppType;
					if (cppType.FindFirst('>') > 0) {
						// leave extra space when we are dealing with 
						// a template type
						stream << " ";
					}
					stream << ">";
				}
				else {
					stream << cppType;
				}
				stream << " " << def->m_id << ";" << endl;
			}

			// take care of any enums/structs we have
			// They are packaged as IDLMethods, with each member being a parameter
			if (constructCount > 0) {
				stream << endl;
				stream << "/* ------- enums/structs --------------------- */" << endl;
			}
			for (size_t i_enum = 0; i_enum < constructCount; i_enum++) {
				sptr<IDLConstruct> construct = rec->ConstructAt(i_enum);
				stream << dedent;
				construct->OutputComment(stream);	
				stream << indent;
				SString constructName = construct->ID();
				bool inStruct = (constructName.FindLast("struct") >= 0);
				stream << constructName << " {" << endl << indent;

				int32_t memberCount = construct->CountParams();
				for (int32_t i_member = 0; i_member < memberCount; i_member++) {
					sptr<IDLNameType> member = construct->ParamAt(i_member);
					// first write the comment (if it exists, it will end with newline)
					member->OutputComment(stream);
					// then write the actual member
					stream << member->m_id;
					// terminate the member/enumerator 
					if (inStruct == true) {
						stream << ";";
					}
					else if (i_member < memberCount-1) {
						stream << ",";
					}
					stream << endl;
				}
				construct->OutputTrailingComment(stream);
				stream << dedent << "};" << endl << endl;
			}

			if (propertyCount != 0) {
				stream << endl;
				stream << "/* ------- Properties --------------------- */" << endl;
			}
			for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
				sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
				stream << endl;
				WritePropertyDeclarations(stream, nt, true);
			}
			
			if (methodCount != 0) {
				stream << endl;
				stream << "/* ------- Methods ------------------------ */" << endl;
			}
			for (int32_t i_method = 0; i_method < methodCount; i_method++) {
				sptr<IDLMethod> method = rec->MethodAt(i_method);
				stream << endl;
				WriteMethodDeclaration(stream, method, true);
			}

			if (eventCount != 0) {
				stream << endl;
				stream << "/* ------- Events ------------------------ */" << endl;
			}
			SString localClass = noid;
			localClass.RemoveFirst("I");

			for (int32_t i_event = 0; i_event < eventCount; i_event++) {
				sptr<IDLEvent> event = rec->EventAt(i_event);
				stream << endl;
				// For each event, just right a doxygen comment refering them
				// to the event methods in the BnClass
				stream << "/*! \\fn void " << event->ID() << "()" << endl;
				stream << "\t\\brief An event implemented by Push" << event->ID() << endl;
				stream << "\t\\see " << gNativePrefix << localClass << "::Push" << event->ID() << " */" << endl;
			}

			/* ------- inline helpers ------------------------- */
			stream << endl << "/* ------- inline helpers ------------------------- */" << endl;

			// If we have multiple bases, then add a new AsBinder to remove ambiguous calls
			// to IIteraface::AsBinder
			if (rec->HasMultipleBases()) {
				SString left_most_base = rec->LeftMostBase();
				stream << "sptr<IBinder> AsBinder()" << endl;
				stream << "{" << endl;
				stream << indent;
				stream << "return " << left_most_base << "::AsBinder();" << endl;
				stream << dedent;
				stream << "}" << endl;
				stream << "sptr<const IBinder> AsBinder() const" << endl;
				stream << "{" << endl;
				stream << indent;
				stream << "return " << left_most_base << "::AsBinder();" << endl;
				stream << dedent;
				stream << "}" << endl;
			}

			stream << "status_t Link" << noid << "(const sptr<IBinder>& to, const SValue& mappings, uint32_t flags = 0)" << endl;
			stream << "{" << endl;
			stream << indent;
			stream << "return AsBinder()->Link(to, mappings, flags);" << endl;
			stream << dedent;
			stream << "}" << endl;
			stream << "status_t Unlink" << noid << "(const sptr<IBinder>& to, const SValue& mappings, uint32_t flags = 0)" << endl;
			stream << "{" << endl;
			stream << indent;
			stream << "return AsBinder()->Unlink(to, mappings, flags);" << endl;
			stream << dedent;
			stream << "}" << endl;

	#if KEYS_IN_HEADER
			stream << endl;

			if (propertyCount != 0) {
				stream << endl;
				stream << "/* ------- Property Keys ------------------ */" << endl;
			}	
			for (int32_t i_prop = 0; i_prop < m; i_prop++) {
				WriteKey(stream, "prop", rec->PropertyAt(i_prop)->m_id);
			}
			
			if (methodCount != 0) {
				stream << endl;
				stream << "/* ------- Method Keys -------------------- */" << endl;
			}
			for (int32_t i_method = 0; i_method < methodCount; i_method++) {
				sptr<IDLMethod> method = rec->MethodAt(i_method);
				WriteKey(stream, "method", method->ID());
				
				int32_t paramCount = method->CountParams();
				SString prefix("param_");
				prefix.Append(method->ID());
				for (int32_t i_param = 0; i_param < paramCount; i_param++) {
					WriteKey(stream, prefix.String(), method->ParamAt(i_param)->m_id);
				}
				stream << endl;
			}

			if (eventCount != 0) {
				stream << endl;
				stream << "/* ------- Event Keys --------------------- */" << endl;
			}
			for (int32_t i_event = 0; i_event < eventCount; i_event++) {
				sptr<IDLEvent> event = rec->EventAt(i_event);
				WriteKey(stream, "event", event->ID());
				
				int32_t paramCount = event->CountParams();
				SString prefix("param_");
				prefix.Append(event->ID());
				for (int32_t i_param = 0; i_param < paramCount; i_param++) {
					WriteKey(stream, prefix.String(), event->ParamAt(i_param)->m_id);
				}
				stream << endl;
			}
	#endif

			/* ------- protected default ctor/dtor ------------------------- */
			/* ------- private copy constructor and assignment operator ------------------------- */
			stream << dedent;
			stream << endl << "protected:" << endl;
			stream << indent;
			SString className = rec->ID();
			stream << "\t\t" << PadString(SString(""), TYPE_TABS) << className << "() { }" << endl;
			stream << "virtual\t" << PadString(SString(""), TYPE_TABS) << "~" << className << "();" << endl;

			stream << dedent;
			stream << endl << "private:" << endl;
			stream << indent;
			stream << "/* ------- copy/assignment protection ------------------ */" << endl;
			// SString className = rec->ID();
			stream << "\t\t" << PadString(SString(""), TYPE_TABS)
					<< className << "(const " << className << "& o);" << endl;
			SString classReference(className);
			classReference.Append("&");
			stream << "\t\t" << classReference << PadString(classReference, TYPE_TABS)
					<< "operator=(const " << className << "& o);" << endl;

			stream << dedent;
			stream << "};" << endl;
			stream << endl;
		}
	}

	stream << endl;
	stream << "/* ============= Local Class Declarations =============================== */" << endl;
	stream << endl;

	for (int32_t i_record = 0; i_record < recordCount; i_record++) {
		InterfaceRec* rec = recs[i_record];
		if (rec->Declaration()==DCL) {
			SString noid=rec->ID();
			noid.RemoveFirst("I");
					
			nsGen.EnterNamespace(stream, rec->Namespace(), rec->CppNamespace());

			SVector<SString> parents = rec->Parents();
			SVector<InterfaceRec*> allInterfaces;
			CollectParents(rec, recs, &allInterfaces);
			const size_t interfaceCount = allInterfaces.CountItems();
			size_t i;

			stream << "class " << gNativePrefix << noid << " : public " << rec->ID() << ", public BBinder" << endl;
			stream << "{" << endl;
			stream << "public:" << endl << indent;
			
			stream << "virtual\tSValue" << PadString(SString("SValue"), TYPE_TABS)
					<< "Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags = 0);" << endl;
			stream << "virtual\tsptr<IInterface>" << PadString(SString("sptr<IInterface>"), TYPE_TABS)
					<< "InterfaceFor(const SValue& descriptor, uint32_t flags = 0);" << endl;

			// Write out the padding properties/methods if any
			didHeader = false;
			for (int32_t i_interface = 0; i_interface < interfaceCount; i_interface++) {
				InterfaceRec* rec = allInterfaces[i_interface];
				
				// first properties...
				int32_t propertyCount = rec->CountProperties();
				for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
					sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
					if (nt->m_type->HasAttribute(kReserved)) {
						if (!didHeader) {
							stream << endl;
							stream << "/* ------- Reserved Properties/Methods ---- */" << endl;
							stream << endl;
							didHeader = true;
						}
						stream << endl;
						WritePropertyDeclarations(stream, nt, false);
					}
				}

				// then methods...
				int32_t methodCount = rec->CountMethods();
				for (int32_t i_method = 0; i_method < methodCount; i_method++) {
					sptr<IDLMethod> method = rec->MethodAt(i_method);
					if (method->HasAttribute(kReserved)) {
						if (!didHeader) {
							stream << endl;
							stream << "/* ------- Reserved Properties/Methods ---- */" << endl;
							stream << endl;
							didHeader = true;
						}
						stream << endl;
						WriteMethodDeclaration(stream, method, false);
					}
				}
			}

			didHeader = false;
			for (int32_t i_interface = 0; i_interface < interfaceCount; i_interface++) {
				InterfaceRec* rec = allInterfaces[i_interface];

				int32_t eventCount = rec->CountEvents();
				for (int32_t i_event = 0; i_event < eventCount; i_event++) {
					sptr<IDLEvent> event = rec->EventAt(i_event);
					if (!didHeader) {
						stream << endl;
						stream << "/* ------- Events ------------------------- */";
						stream << dedent << endl << "public:" << indent << endl;
						didHeader = true;
					}
					stream << indent;
					event->OutputComment(stream);
					stream << dedent;
					stream << "\t\tvoid" << PadString(SString("void"), TYPE_TABS)
							<< "Push" << event->ID() << "(";
					
					int32_t paramCount = event->CountParams();
					// if the parameters have comments, then we will have newlines and this indenting will
					// have an effect, normally it will be ignored...
					for (int32_t i = 0; i < PARM_TABS; i++) {
						stream << indent;
					}
					for (int32_t i_param = 0; i_param < paramCount; i_param++) {
						sptr<IDLNameType> nt = event->ParamAt(i_param);
						if (nt->HasComment()) {
							stream << endl;
							nt->OutputComment(stream);
						}
						SString type = TypeToCPPType(kInsideClassScope, nt->m_type, true);
						stream << type << " " << nt->m_id;
						if (i_param < paramCount-1) {
							stream << ", ";
						}
					}
					if (event->HasTrailingComment()) {
						stream << endl;
						event->OutputTrailingComment(stream);
					}
					stream << ");" << endl;
					// clean up our comment indenting
					for (int32_t i = 0; i < PARM_TABS; i++) {
						stream << dedent;
					}
				}
			}

			didHeader = false;
			for (int32_t i_interface = 0; i_interface < interfaceCount; i_interface++) {
				InterfaceRec* rec = allInterfaces[i_interface];

				int32_t propertyCount = rec->CountProperties();
				for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
					sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
					if ((nt->m_type->HasAttribute(kWriteOnly) == false) && (nt->m_type->HasAttribute(kReserved) == false)) {
						if (!didHeader) {
							stream << endl;
							stream << "/* ------- Push Properties ---------------- */";
							stream << dedent << endl << "public:" << indent << endl;
							didHeader = true;
						}
						SString type = TypeToCPPType(kInsideClassScope, nt->m_type, true);
						stream << "\t\tvoid" << PadString(SString("void"), TYPE_TABS)
								<< "Push" << (char) toupper(nt->m_id.ByteAt(0)) << nt->m_id.String()+1 << "(" << type << " value);" << endl;
					}
				}
			}
			
			stream << dedent << endl << "protected:" << indent << endl;

			stream << PadString(SString(""), TOTAL_TABS) << gNativePrefix << noid << "();" << endl
					<< PadString(SString(""), TOTAL_TABS) << gNativePrefix << noid
						<< "(const SContext& context);" << endl
					<< "virtual\t" << PadString(SString(""), TYPE_TABS) << "~" << gNativePrefix << noid << "();" << endl;
			
			stream << endl;
			stream << "virtual\tsptr<IBinder>" << PadString(SString("sptr<IBinder>"), TYPE_TABS)
				<< "AsBinderImpl();" << endl;
			stream << "virtual\tsptr<const IBinder>" << PadString(SString("sptr<const IBinder>"), TYPE_TABS)
				<< "AsBinderImpl() const;" << endl;
			stream << endl;

			stream << "/* ------- Marshalling Details ------------ */" << endl;
			stream << "virtual\tstatus_t" << PadString(SString("status_t"), TYPE_TABS)
					<< "HandleEffect(const SValue &, const SValue &, const SValue &, SValue *);" << endl;
			if (g_writeAutobinder) {
				stream << "virtual\tstatus_t" << PadString(SString("status_t"), TYPE_TABS)
					<< "Transact(uint32_t code, SParcel& data, SParcel* reply, uint32_t flags);" << endl;
			}

			// private copy constructor and assignment operator (no implementations)
			stream << dedent << endl;
			stream << "private:" << endl;
			stream << indent << "\t\t" << PadString(SString(""), TYPE_TABS)
					<< gNativePrefix << noid << "(const " << gNativePrefix << noid << "& o);" << endl;
			SString classReferenceType(gNativePrefix);
			classReferenceType.Append(noid);
			classReferenceType.Append("&");
			stream << "\t\t" << classReferenceType << PadString(classReferenceType, TYPE_TABS)
					<< "operator=(const " << gNativePrefix << noid << "& o);" << endl;
			
	#if KEYS_IN_HEADER
			stream << endl;
			stream << "static\tconst effect_action_def"
					<< PadString(SString("const effect_action_def"), TYPE_TABS)
					<< "kActionDefArray[];" << endl;
			
			int32_t propetyCount = rec->CountProperties();
			for (int32_t i_prop = 0; i_prop < propertyCount; i_prop++) {
				sptr<IDLNameType> nt = rec->PropertyAt(i_prop);
				stream << "static\tstatus_t" << PadString(SString("status_t"), TYPE_TABS)
						<< "put_" << nt->m_id << "(const sptr<IInterface> &, const SValue &);" << endl;
				stream << "static\tSValue" << PadString(SString("SValue"), TYPE_TABS)
						<< "get_" << nt->m_id << "(const sptr<IInterface> &);" << endl;
			}
			
			int32_t methodCount = rec->CountMethods();
			for (int32_t i_method = 0; i_method < methodCount; i_method++) {
				method = rec->MethodAt(i_method);
				stream << "static\tSValue" << PadString(SString("SValue"), TYPE_TABS)
						<< "invoke_" << method->ID() << "(const sptr<IInterface> &, const SValue &);" << endl;
			}
	#endif
			
			stream << dedent;
			
			stream << "};" << endl;
			stream << endl;
			stream << endl;
		}
	}
		
	nsGen.CloseNamespace(stream);

	//bout << "// end comments: " << endComments << endl;
	if (endComments != NULL) {
		stream << endl;
		endComments->Output(stream, false);
	}

	stream << endl;
	stream << "#endif // " << headerGuard << endl;
	stream << endl;
	
	return B_OK;
}
