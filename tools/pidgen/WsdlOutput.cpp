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

#include <support/ByteStream.h>
#include <support/SortedVector.h>
#include <support/StdIO.h>
#include <support/URL.h>

#include <ctype.h>

#include "WSDL.h"
#include "WsdlOutput.h"
#include "OutputUtil.h"
#include "InterfaceRec.h"

#if _SUPPORTS_NAMESPACE
using namespace palmos::storage;
using namespace palmos::support;
using namespace palmos::xml;
#endif

const char* g_http_transport = "http://schemas.xmlsoap.org/soap/http";


NamedType::NamedType()
{
}

NamedType::NamedType(const SString& id, const SString& type)
	:	m_id(id)
{
	set_type(type);
}

SString NamedType::Id() const
{
	return m_id;
}

SString NamedType::Type() const
{
	return m_type;
}

void NamedType::set_type(const SString& type)
{
	SXMLTag tag(type);
	
	if (tag.Accessor() == "boolean")
	{
		m_type = "bool";
	}
	else if (tag.Accessor() == "string")
	{
		m_type = "SString";
	}
	else if (tag.Accessor() == "int")
	{
		m_type = "int32_t";
	}
	else if (tag.Accessor() == "base64Binary")
	{
		m_type = "SValue";
	}
	else
	{
		m_type = tag.Accessor();
	}
}

void NamedType::Print() const
{
}

void NamedType::Print(sptr<ITextOutput> stream) const
{
	stream << m_type << " " << m_id;
}

void NamedType::PrintArg(sptr<ITextOutput> stream) const
{
	if (m_type == "int32_t" || m_type == "bool" || m_type == "double" || m_type == "int64_t" || m_type == "float")
	{
		stream << m_type << " " << m_id;
	}
	else
	{
		stream << "const " << m_type << "& " << m_id;
	}
}

Method::Method()
{
}

Method::Method(const SString& id, const SString& returnType, const SString& url, const SString& urn)
	:	m_messageID(id),
		m_id(id),
		m_url(url),
		m_urn(urn)
{
	char* temp = m_id.LockBuffer(m_id.Length());
	temp[0] = toupper(temp[0]);
	m_id.UnlockBuffer(m_id.Length());
	
	set_return_type(returnType);
}

ssize_t Method::AddParam(const SString& id, const SString& type)
{
	sptr<NamedType> named = new NamedType(id, type);
	return m_params.AddItem(named);
}

size_t Method::CountParams() const
{
	return m_params.CountItems();
}

sptr<NamedType> Method::ParamAt(size_t index) const
{
	return m_params.ItemAt(index);
}

SString Method::Id() const
{
	return m_id;
}

SString Method::Url() const
{
	return m_url;
}

SString Method::Urn() const
{
	return m_urn;
}

SString Method::ReturnType() const
{
	return m_returnType;
}

SString Method::MessageId() const
{
	return m_messageID;
}

void Method::Print() const
{
}

void Method::PrintHeader(sptr<ITextOutput> stream) const
{
	stream << ReturnType() << " " << m_id << "(";
	
	size_t params = m_params.CountItems();
	for (size_t i = 0 ; i < params ; i++)
	{
		if (i > 0) stream << ", ";
		m_params.ItemAt(i)->PrintArg(stream);
	}
	
	stream << ")";
}

void Method::PrintInterface(sptr<ITextOutput> stream) const
{
	stream << ReturnType() << " " << m_id << "(";
	
	size_t params = m_params.CountItems();
	for (size_t i = 0 ; i < params ; i++)
	{
		if (i > 0) stream << ", ";
		m_params.ItemAt(i)->Print(stream);
	}
	
	stream << ")";
}

void Method::set_return_type(const SString& returnType)
{
	SXMLTag tag(returnType);

	if (tag.Accessor() == "boolean")
	{
		m_returnType = "bool";
	}
	else if (tag.Accessor() == "string")
	{
		m_returnType = "SString";
	}
	else if (tag.Accessor() == "int")
	{
		m_returnType = "int32_t";
	}
	else if (tag.Accessor() == "base64Binary")
	{
		m_returnType = "SValue";
	}
	else
	{
		m_returnType = tag.Accessor();
	}
}

WsdlClass::WsdlClass(const SString& id, const sptr<BWsdl>& wsdl)
	:	m_id(id),
		m_wsdl(wsdl)
{
	if (m_wsdl != NULL)
	{
		populate_parents();

		m_interfaceName = "I";
		m_interfaceName.Append(m_id);
	}
}

void WsdlClass::InitAtom()
{
}

void WsdlClass::populate_parents()
{
	// add the Bn-class of the interface
	SString lclass("Bn");
	lclass.Append(m_id);
	AddParent(lclass);

	for (size_t i = 0 ; i < m_wsdl->CountBindings() ; i++)
	{
		// determine what soap client to use
		sptr<BWsdl::Binding> binding = m_wsdl->BindingAt(i);

		if (binding != NULL)
		{
			SString style = binding->Style();
			SString trans = binding->Transport();

			bout << "style = " << style << " trans = " << trans << endl;
			if (trans == g_http_transport)
			{
				AddParent(SString("BSoapHttpClient"));
			}
		}
	}
}

SString WsdlClass::InterfaceName() const
{
	return m_interfaceName;
}

SString WsdlClass::Id() const
{
	return m_id;
}

SString WsdlClass::Namespace() const
{
	return m_namespace;
}

ssize_t WsdlClass::AddMethod(const sptr<Method>& method)
{
	return m_methods.AddItem(method);
}

ssize_t WsdlClass::AddMemeber(const sptr<NamedType>& type)
{
	return m_members.AddItem(type);
}

size_t WsdlClass::CountMethods() const
{
	return m_methods.CountItems();
}

size_t WsdlClass::CountTypes() const
{
	return m_types.CountItems();
}

ssize_t WsdlClass::AddType(const sptr<WsdlType>& type)
{
	return m_types.AddItem(type->Id(), type);
}

sptr<WsdlType> WsdlClass::TypeAt(size_t index) const
{
	return m_types.ValueAt(index);	
}

sptr<WsdlType> WsdlClass::TypeFor(const SString& id, bool* found) const
{
	return m_types.ValueFor(id, found);
}

sptr<Method> WsdlClass::MethodAt(size_t index) const
{
	return m_methods.ItemAt(index);
}

size_t WsdlClass::CountMembers() const
{
	return m_members.CountItems();
}

sptr<NamedType> WsdlClass::MemberAt(size_t index) const
{
	return m_members.ItemAt(index);
}

size_t WsdlClass::CountParents() const
{
	return m_parents.CountItems();
}

SString WsdlClass::ParentAt(size_t index) const
{
	return m_parents.ItemAt(index);
}

ssize_t WsdlClass::AddParent(const SString& parent)
{
	return m_parents.AddItem(parent);
}

void WsdlClass::Print() const
{
	bout << "WsdlClass: id=" << m_id << endl;
	bout << indent;
	size_t size = m_methods.CountItems();
	for (size_t i = 0 ; i < size ; i++)
	{
		m_methods.ItemAt(i)->Print();
	}
	
	size = m_members.CountItems();
	for (size_t i = 0 ; i < size ; i++)
	{
		m_members.ItemAt(i)->Print();
	}
	bout << dedent << endl;
}

void WsdlClass::PrintHeader(sptr<ITextOutput> stream) const
{
	stream << "class " << Id();
	size_t parents = m_parents.CountItems();
	for (size_t i = 0 ; i < parents ; i++)
	{
		if (i == 0) stream << " : ";
		stream << "public " << m_parents.ItemAt(i);
		if (i < (parents - 1)) stream << ", ";
	}
	stream << endl;
	stream << "{" << endl;
	stream << "public:" << endl;
	stream << indent;
	stream << Id() << "();" << endl;
	stream << Id() << "(const SContext& context);" << endl;
	stream << endl;
	
	size_t methods = CountMethods();
	for (size_t j = 0 ; j < methods ; j++)
	{
		stream << "virtual ";
		MethodAt(j)->PrintHeader(stream);
		stream << ";" << endl;
	}

	bool inPublic = true;
	size_t members = CountMembers();
	for (size_t j = 0 ; j < members ; j++)
	{
		sptr<NamedType> type = MemberAt(j);
	
		int32_t index = type->Id().FindFirst("m_");
		if (index == 0 && inPublic)
		{
			stream << dedent;
			stream << "private:" << endl;
			stream << indent;
		}
		else if (index < 0 && !inPublic)
		{
			stream << dedent;
			stream << "public:" << endl;
			stream << indent;
		}

		type->Print(stream);
		stream << ";" << endl;
	}

	stream << endl;
	stream << dedent;
	stream << "protected:" << endl;
	stream << indent;
	stream << "virtual ~" << Id() << "();" << endl;
	
	stream << dedent << "};" << endl << endl;
}

SString type_to_value_string(const sptr<NamedType>& type)
{
	SString result;
	bool builtin = true;

	if (type->Type() == "bool")
	{
		result = "SValue::Bool("; 
	}
	else if (type->Type() == "SString")
	{
		result = "SValue::String("; 
	}
	else if (type->Type() == "int32_t")
	{
		result = "SValue::Int32("; 
	}
	else if (type->Type() == "double")
	{
		result = "SValue::Double("; 
	}
	else
	{
		builtin = false;
		result << type->Id() << ".AsValue()";
	}
	
	if (builtin) result << type->Id() << ")";

	return result;
}

void WsdlClass::PrintInterface(sptr<ITextOutput> stream) const
{
	stream << "interface " << InterfaceName() << endl;
	stream << "{" << endl;
	stream << "methods:" << endl;
	stream << indent;

	size_t methods = CountMethods();
	for (size_t j = 0 ; j < methods ; j++)
	{
		MethodAt(j)->PrintInterface(stream);
		stream << ";" << endl;
	}

	stream << dedent << "}" << endl << endl;
}

void WsdlClass::PrintCPP(sptr<ITextOutput> stream) const
{
	SValue port = m_wsdl->ServiceAt(0)->PortAt(0);
	SValue nspace = m_wsdl->Definitions();
	
	stream << Id() << "::" << Id() << "()" << endl;
	stream << "{" << endl;
	// set the url
	stream << indent;
	stream << "SetUrl(\"" << port[SValue::Int32(0)]["address"].AsString() << "\");" << endl;
	stream << "SetUrn(\"" << nspace["targetNamespace"].AsString() << "\");" << endl;
	stream << dedent;
	stream << "}" << endl;
	stream << endl;


	stream << Id() << "::" << Id() << "(const SContext& context)" << endl;
	size_t parents = CountParents();
	size_t i = 0;
	while (i < parents)
	{
		if (i == 0) stream << indent << ":" ;
		
		stream << "\t" << ParentAt(i) << "()";
		i++;
		if (i < parents) stream << ",";
		stream << endl;
	}
	stream << dedent << endl;
	stream << "{" << endl;
	// set the url
	stream << indent;
	stream << "SetUrl(\"" << port[SValue::Int32(0)]["address"].AsString() << "\");" << endl;
	stream << "SetUrn(\"" << nspace["targetNamespace"].AsString() << "\");" << endl;
	stream << dedent;
	stream << "}" << endl;
	stream << endl;

	stream << Id() << "::~" << Id() << "()" << endl;
	stream << "{" << endl;
	stream << "}" << endl;
	stream << endl;
	
	size_t methods = CountMethods();
	for (size_t j = 0 ; j < methods ; j++)
	{
		sptr<Method> method = MethodAt(j);

		stream << method->ReturnType() << " " << Id() << "::" << method->Id() << "(";
	
		size_t params = method->CountParams();
		for (size_t i = 0 ; i < params ; i++)
		{
			sptr<NamedType> type = method->ParamAt(i);
			if (i > 0) stream << ", ";
			type->PrintArg(stream);
		}
		
		stream << ")" << endl;
		stream << "{" << endl;
		stream << indent;
		stream << "SValue args;" << endl;
		for (size_t i = 0 ; i < params ; i++)
		{
			sptr<NamedType> type = method->ParamAt(i);
			stream << "args.JoinItem(SValue::Int32(" << i << "), ";
			stream << "SValue(\"" << type->Id() << "\", ";
			stream << type_to_value_string(type) << "));" << endl;
		}
		stream << endl;
//		stream << "SetUrl(\"" << method->Url() << "\");" << endl;
		stream << "SString urn(\"" << method->Urn() << "\");" << endl;
		stream << "SValue result = Process(SString(\"" << method->MessageId() << "\"), urn, args);" << endl;
		stream << "return ";
	
		SString rtype = method->ReturnType();

		if (rtype == "SValue")
		{
			stream << "result[\"return\"];" << endl;
		}
		else if (rtype == "SString")
		{
			stream << "result[\"return\"].AsString();" << endl;
		}
		else
		{
			stream << rtype << "(result[\"return\"]);" << endl;
		}
		
		stream << dedent;
		stream << "}" << endl;
		stream << endl;
	}
}

SString method_name(const SString& name)
{
	SString method = name;

	char* temp = method.LockBuffer(method.Length());
	temp[0] = toupper(temp[0]);
	method.UnlockBuffer(method.Length());

	return method;
}

sptr<Method> create_wsdl_method(const sptr<BWsdl>& wsdl, BWsdl::PortType::Operation operation, const SString& url, const SString& urn)
{
	bout << "create_wsdl_method()" << endl;
	SXMLTag itag(operation.input);
	SXMLTag otag(operation.output);

	for (size_t i = 0 ; i < wsdl->CountMessages() ; i++)
	{
		bout << "message name = " << wsdl->MessageAt(i)->Name() << endl;
	}
	
	bout << "input = " << itag.Accessor() << endl;
	bout << "output = " << otag.Accessor() << endl;

	sptr<BWsdl::Message> input = wsdl->MessageFor(itag.Accessor());
	sptr<BWsdl::Message> output = wsdl->MessageFor(otag.Accessor());

	if (input == NULL || output == NULL)
	{
		bout << "create_wsdl_method: failed to find input or output message: input=";
		bout << itag.Accessor() << " output=" << otag.Accessor() << endl;
		return NULL;
	}

	SValue value = output->PartAt(0);
	
	SString returnType;
	SString name = value["name"].AsString();
	
	// always check the type first, if it is null the check for element
	returnType = value["type"].AsString();
	if (returnType == "")
	{
		returnType = value["element"].AsString();
		if (returnType == "")
		{
			bout << "create_wsdl_method: failed to retrieve return type" << endl;
			return NULL;
		}
	}
	
	sptr<Method> method = new Method(operation.name, returnType, url, urn);
	
	size_t parts = input->CountParts();
	for (size_t i = 0 ; i < parts ; i++)
	{
		SValue part = input->PartAt(i);
		SString type = part["type"].AsString();
		if (type == "") type = part["element"].AsString();
		if (type == "")
		{
			bout << "part does not have type" << endl;
			ErrFatalError("part does not have type");
		}
		method->AddParam(part["name"].AsString(), type);
	}

	return method;
}


void sort_classes(SVector< sptr<WsdlClass> >& classes)
{
	size_t size = classes.CountItems();

	for (size_t i = 0 ; i < size ; i++)
	{
		sptr<WsdlClass> wsdl = classes.Top();
		classes.Pop();
		classes.AddItemAt(wsdl, i);
	}
}

status_t create_wsdl_class(const sptr<BWsdl>& wsdl, sptr<WsdlClass>& obj)
{
	obj = new WsdlClass(wsdl->Name(), wsdl);

	SKeyedVector<SString, SValue> metadata;
	
	size_t services = wsdl->CountServices();
	for (size_t i = 0 ; i < services ; i++)
	{
		sptr<BWsdl::Service> service = wsdl->ServiceAt(i);
		bout << "service: name = " << service->Name() << indent << endl;
		size_t ports = service->CountPorts();
		for (size_t j = 0 ; j < ports ; j++)
		{
			SValue port = service->PortAt(j);
			bout << "port: " << port << indent << endl;
			SXMLTag tag(port["binding"].AsString());
			sptr<BWsdl::Binding> binding = wsdl->BindingFor(tag.Accessor());
			if (binding != NULL)
			{
				bout << "binding:name = " << binding->Name() << endl;
				bout << "binding:type = " << binding->Type() << endl;
				sptr<BWsdl::PortType> type = wsdl->PortTypeFor(binding->Type());
				bout << "porttype = "<< type->Name() << endl;
				bout << dedent << dedent << endl;
				size_t operations = binding->CountOperations();
				for (size_t k = 0 ; k < operations ; k++)
				{
					BWsdl::Binding::Operation attrs = binding->OperationAt(k);
					BWsdl::PortType::Operation op = type->OperationFor(attrs.name);
				
					sptr<Method> method = create_wsdl_method(wsdl, op, port[SValue::Int32(0)]["address"].AsString(), attrs.urn);
					if (method != NULL) obj->AddMethod(method);
				}
			}
			else
			{
				bout << "binding(" << port["binding"].AsString() << ")  == NULL" << endl;
			}
		}
	}
	
#if 0
	size_t size = wsdl->CountPortTypes();
	for (size_t i = 0 ; i < size ; i++)
	{
		sptr<BWsdl::PortType> type = wsdl->PortTypeAt(i);
		size_t types = type->CountOperations();
		for (size_t j = 0 ; j < types ; j++)
		{
			sptr<Method> method = create_wsdl_method(wsdl, type->OperationAt(j));
			if (method != NULL) obj->AddMethod(method);
		}
	}
#endif

	// now go through the complex types
	
	sptr<BXMLSchema> schema = wsdl->Schema();
	size_t size = schema->CountComplexTypes();
	for (size_t i = 0 ; i < size ; i++)
	{
		sptr<BXSDComplexType> complexType = schema->ComplexTypeAt(i);
		size_t elements = complexType->CountElements();
		size_t contents = complexType->CountComplexContents();
	
		sptr<WsdlType> type = new WsdlType(complexType->Name());
		for (size_t j = 0 ; j < elements ; j++)
		{
			sptr<BXSDElement> element = complexType->ElementAt(j);
			type->AddMemeber(new NamedType(element->Name(), element->Type()));
		}
	
		for (size_t j = 0 ; j < contents ; j++)
		{
			sptr<BXSDComplexContent> content = complexType->ComplexContentAt(j);

			size_t attrs = content->CountAttributes();
			for (size_t k = 0 ; k < attrs ; k++)
			{
				SValue value = content->AttributeAt(k);
				SString base = value["base"].AsString();

				if (ICompare(base, SString("soapenc:array")) == 0)
				{
					SXMLTag arrayTag(value["wsdl:arrayType"].AsString());
					SString arrayType(arrayTag.Accessor(), arrayTag.Accessor().Length() - 2);
				
					SString parent;
					parent << "SVector<" << arrayType << ">";
					type->AddParent(parent);
				}
			}
		}

		type->Print();
		obj->AddType(type);
	}

	//sort_classes(classes);
	
	return B_OK;
}

status_t print_type(const sptr<WsdlClass>& obj, const sptr<WsdlType>& type, sptr<ITextOutput> stream, SSortedVector<SString>& printed)
{
	ssize_t index = printed.IndexOf(type->Id());
	if (index >= 0)
	{
		return B_OK;
	}

	size_t parents = type->CountParents();
	for (size_t i = 0 ; i < parents ; i++)
	{
		SString vector("SVector<");
		SString parent = type->ParentAt(i);
		if (parent.FindFirst(vector) == 0)
		{
			int32_t index = parent.FindLast(">");
			SString name;
			parent.CopyInto(name, vector.Length(), index - vector.Length());

			bool found = false;
			sptr<WsdlType> nutype = obj->TypeFor(name, &found);
			index = printed.IndexOf(name);
			if (found && index < 0)
			{
				// we found a type that hasn't been printed out yet. do so.
				print_type(obj, nutype, stream, printed);
			}
		}
		
	}
	
	size_t members = type->CountMembers();
	for (size_t i = 0 ; i < members ; i++)
	{
		sptr<NamedType> member = type->MemberAt(i);
	
		// now check to see if this is a type on the WsdlClass
		bool found = false;
		sptr<WsdlType> nutype = obj->TypeFor(member->Type(), &found);
		ssize_t index = printed.IndexOf(member->Type());
		
		// see if we already printed out this type
		
		if (found && index < 0)
		{
			// we found a type that hasn't been printed out yet. do so.
			print_type(obj, nutype, stream, printed);
		}
	}

	// we should be at the leaf node
	type->PrintHeader(stream);
	printed.AddItem(type->Id());

	return B_OK;
}

status_t wsdl_create_types_header(const sptr<WsdlClass>& obj, sptr<ITextOutput> stream, const SString& filename)
{
	stream << "/*=============================================================================== */" << endl;
	stream << "// " << filename << " is automatically generated by PIDGEN - DO NOT MODIFY" << endl;
	stream << "/*=============================================================================== */" << endl << endl;
	
	SString headerGuard;
	status_t err = HeaderGuard(filename, headerGuard, false);
	if (err != B_OK) return err;
	
	stream << "#ifndef " << headerGuard << endl;
	stream << "#define " << headerGuard << endl;
	stream << endl;

	stream << "#include <support/String.h>" << endl;
	stream << "#include <support/Value.h>" << endl;
	stream << "#include <support/Vector.h>" << endl;
	stream << endl;
	stream << "#if _SUPPORTS_NAMESPACE" << endl;
	stream << "using namespace palmos::soap;" << endl;
	stream << "using namespace palmos::support;" << endl;
	stream << "#endif" << endl;
	stream << endl;

	SSortedVector<SString> printed;
	
	size_t count = obj->CountTypes();
	for (size_t i = 0 ; i < count ; i++)
	{
		sptr<WsdlType> type = obj->TypeAt(i);
		print_type(obj, type, stream, printed);
	}
	
	stream << "#endif // " << headerGuard << endl;
	
	stream << endl;
	return B_OK;
}

status_t wsdl_create_header(const sptr<WsdlClass>& obj, sptr<ITextOutput> stream, const SString& filename)
{
	stream << "/*=============================================================================== */" << endl;
	stream << "// " << filename << " is automatically generated by PIDGEN - DO NOT MODIFY" << endl;
	stream << "/*=============================================================================== */" << endl << endl;

	SString headerGuard;
	status_t err = HeaderGuard(filename, headerGuard, false);
	if (err != B_OK) return err;
	
	stream << "#ifndef " << headerGuard << endl;
	stream << "#define " << headerGuard << endl;
	stream << endl;

	stream << "#include <soap/Client.h>" << endl;
	stream << "#include <support/String.h>" << endl;
	stream << "#include <support/Vector.h>" << endl;
	stream << endl;
	stream << "#include \"" << obj->InterfaceName() << ".h\"" << endl;
	stream << endl;
	stream << "#if _SUPPORTS_NAMESPACE" << endl;
	stream << "using namespace palmos::soap;" << endl;
	stream << "using namespace palmos::support;" << endl;
	stream << "#endif" << endl;
	stream << endl;

	obj->PrintHeader(stream);
	
	stream << "#endif // " << headerGuard << endl;
	
	stream << endl;
	return B_OK;
}

status_t wsdl_create_cpp(const sptr<WsdlClass>& obj, sptr<ITextOutput> stream, const SString& filename, const SString& header)
{
	stream << "/*=============================================================================== */" << endl;
	stream << "// " << filename << " is automatically generated by PIDGEN - DO NOT MODIFY" << endl;
	stream << "/*=============================================================================== */" << endl << endl;

	stream << "#include \"" << header << "\"" << endl << endl;

	obj->PrintCPP(stream);
	
	return B_OK;
}

status_t wsdl_create_types_cpp(const sptr<WsdlClass>& obj, sptr<ITextOutput> stream, const SString& filename, const SString& header)
{
	stream << "/*=============================================================================== */" << endl;
	stream << "// " << filename << " is automatically generated by PIDGEN - DO NOT MODIFY" << endl;
	stream << "/*=============================================================================== */" << endl << endl;

	stream << "#include \"" << header << "\"" << endl << endl;

	size_t types = obj->CountTypes();
	for (size_t i = 0 ; i < types ; i++)
	{
		obj->TypeAt(i)->PrintCPP(stream);
	}
	
	return B_OK;
}


WsdlType::WsdlType(const SString& id)
	:	WsdlClass(id, NULL)
{
}

void WsdlType::Print() const
{
}

void WsdlType::PrintHeader(sptr<ITextOutput> stream) const
{
	stream << "class " << Id();
	size_t parents = CountParents();
	for (size_t i = 0 ; i < parents ; i++)
	{
		if (i == 0) stream << " :";
		stream << " public " << ParentAt(i);
	}
	stream << endl;
	stream << "{" << endl;
	stream << "public:" << endl;
	stream << indent;
	stream << Id() << "();" << endl;
	stream << Id() << "(const SValue& value, status_t* result = NULL);" << endl;
	stream << endl;

	stream << "SValue AsValue() const;" << endl;
	stream << "inline operator SValue() const { return AsValue(); }" << endl;

	size_t members = CountMembers();
	if (members > 0) stream << endl;
	for (size_t j = 0 ; j < members ; j++)
	{
		sptr<NamedType> type = MemberAt(j);
		type->Print(stream);
		stream << ";" << endl;
	}

	stream << dedent;
	
	stream << "};" << endl << endl;
}

void WsdlType::PrintCPP(sptr<ITextOutput> stream) const
{
	stream << Id() << "::" << Id() << "()" << endl;
	stream << "{" << endl;
	stream << "}" << endl;
	stream << endl;

	stream << Id() << "::" << Id() << "(const SValue& value, status_t* result)" << endl;
	stream << "{" << endl;
	stream << indent;

	size_t parents = CountParents();
	for (size_t i = 0 ; i < parents ; i++)
	{
		SString parent = ParentAt(i);
		SString vector("SVector<");
		size_t index = parent.FindFirst(vector);

		if (index >= 0)
		{
			size_t last = parent.FindFirst(">");
			SString type;
			parent.CopyInto(type, vector.Length(), last - vector.Length());

			stream << "SValue key;" << endl;
			stream << "SValue val;" << endl;
			stream << "void* cookie = NULL;" << endl;
			stream << endl;

			stream << "while (value.GetNextItem(&cookie, &key, &val) == B_OK)" << endl;
			stream << "{" << endl;
			stream << indent;
			stream << type << " obj(val[\"item\"]);" << endl;
			stream << "AddItem(obj);" << endl;
			stream << dedent;
			stream << "}" << endl;
		}
	}
	
	size_t members = CountMembers();
	for (size_t i = 0 ; i < members ; i++)
	{
		sptr<NamedType> named = MemberAt(i);
		
		SString name = named->Id();
		SString type = named->Type();
	
		SString as;
		
		if (type == "bool")			as = ".AsBool()";
		else if (type == "float")	as = ".AsFloat()";
		else if (type == "double")	as = ".AsDouble()";
		else if (type == "int32_t")	as = ".AsInt32()";
		else if (type == "int64_t")	as = ".AsInt64()";
		else if (type == "SString")	as = ".AsString()";
	
		if (as != "" || type == "SValue")
		{
			stream << name << " = value[\"" << name << "\"]" << as << ";" << endl;
		}
		else
		{
			stream << name << " = " << type << "(value[\"" << name << "\"]);" << endl;
		}
	}

	stream << endl;
	stream << "if (result) *result = B_OK;" << endl;
	
	stream << dedent;
	stream << "}" << endl;
	stream << endl;

	stream << "SValue " << Id() << "::AsValue() const" << endl;
	stream << "{" << endl;
	stream << indent;

	parents = CountParents();
	stream << "SValue result;" << endl;
	for (size_t i = 0 ; i < parents ; i++)
	{
		SString parent = ParentAt(i);
		SString vector("SVector<");
		size_t index = parent.FindFirst(vector);

		if (index >= 0)
		{
			stream << "size_t size = CountItems();" << endl;
			stream << "for (size_t i = 0 ; i < size ; i++)" << endl;
			stream << "{" << endl;
			stream << indent;
			stream << "result.JoinItem(SValue::Int32(i), SValue(\"item\", ItemAt(i).AsValue()));" << endl;
			stream << dedent;
			stream << "}" << endl;
			stream << endl;
		}
	}

	members = CountMembers();
	for (size_t i = 0 ; i < members ; i++)
	{
		sptr<NamedType> named = MemberAt(i);
		
		SString name = named->Id();
		SString type = named->Type();
	
		SString as;
		
		if (type == "bool")			as = "SValue::Bool(";
		else if (type == "float")	as = "SValue::Float(";
		else if (type == "double")	as = "SValue::Double(";
		else if (type == "int32_t")	as = "SValue::Int32(";
		else if (type == "int64_t")	as = "SValue::Int64(";
		else if (type == "SString")	as = "SValue::String(";
	
		if (as != "")
		{
			stream << "result.JoinItem(SValue::Int32(" << i << "), SValue(\"" << name << "\", " << as << name << ")));" << endl;   
		}
		else
		{
			stream << "result.JoinItem(SValue::Int32(" << i << "), SValue(\"" << name << "\", " << name << ".AsValue()));" << endl;   
		}
	}
	
	stream << "return result;" << endl;
	stream << dedent;
	stream << "}" << endl;
	stream << endl;
}

void WsdlType::PrintInterface(sptr<ITextOutput> stream) const
{
	stream << "type " << Id() << endl;
	stream << "{" << endl;
	stream << "methods:" << endl;
	stream << indent;
	stream << "SValue AsValue();" << endl;
	stream << Id() << " " << Id() << "(SValue o);" << endl;
	stream << dedent << "};" << endl << endl;	
}

status_t wsdl_create_types_interface(const sptr<WsdlClass>& obj, sptr<ITextOutput> stream)
{
	stream << "/*========================================================== */ " << endl;
	stream << "// idl file automatically generated by PIDGEN - DO NOT MODIFY" << endl;
	stream << "/*========================================================== */ " << endl << endl;
	
	size_t types = obj->CountTypes();
	for (size_t i = 0 ; i < types ; i++)
	{
		sptr<WsdlType> type = obj->TypeAt(i);
		type->PrintInterface(stream);
	}
	
	return B_OK;
}

status_t wsdl_create_interface(const sptr<WsdlClass>& obj, sptr<ITextOutput> stream, const SString& typesIdl)
{
	stream << "/*========================================================== */ " << endl;
	stream << "// idl file automatically generated by PIDGEN - DO NOT MODIFY" << endl;
	stream << "/*========================================================== */ " << endl << endl;

	stream << "import <" << typesIdl << ">" << endl << endl;

	obj->PrintInterface(stream);

	return B_OK;
}
