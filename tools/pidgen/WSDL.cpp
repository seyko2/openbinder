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

#include <support/Autolock.h>
#include <support/StdIO.h>

#include "WSDL.h"

B_CONST_STRING_VALUE_SMALL	(key_id, 				"id",);
B_CONST_STRING_VALUE_SMALL	(key_end, 				"end",);
B_CONST_STRING_VALUE_SMALL	(key_ref, 				"ref",);
B_CONST_STRING_VALUE_LARGE	(key_form, 				"form",);
B_CONST_STRING_VALUE_LARGE	(key_name, 				"name",);
B_CONST_STRING_VALUE_LARGE	(key_type, 				"type",);
B_CONST_STRING_VALUE_LARGE	(key_block, 			"block",);
B_CONST_STRING_VALUE_LARGE	(key_final, 			"final",);
B_CONST_STRING_VALUE_LARGE	(key_fixed, 			"fixed",);
B_CONST_STRING_VALUE_LARGE	(key_begin, 			"begin",);
B_CONST_STRING_VALUE_LARGE	(key_default, 			"default",);
B_CONST_STRING_VALUE_LARGE	(key_abstract, 			"abstract",);
B_CONST_STRING_VALUE_LARGE	(key_nillable, 			"nillable",);
B_CONST_STRING_VALUE_LARGE	(key_max_occurs, 		"maxOccurs",);
B_CONST_STRING_VALUE_LARGE	(key_min_occurs, 		"minOccurs",);
B_CONST_STRING_VALUE_LARGE	(key_sub_group, 		"substitutionGroup",);

// #################################################################################
// #################################################################################
// #################################################################################

class BXSDComplexTypeCreator : public BCreator
{
public:
	BXSDComplexTypeCreator(const sptr<BXSDComplexType>& type);
	
	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);

private:
	sptr<BXSDComplexType> m_complexType;
};

class BXSDComplexContentCreator : public BCreator
{
public:
	BXSDComplexContentCreator(const sptr<BXSDComplexContent>& content);
	
	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);

private:
	sptr<BXSDComplexContent> m_complexContent;
	SValue m_meta;
};

class BXSDSimpleContentCreator : public BCreator
{
public:
	BXSDSimpleContentCreator(const sptr<BXSDSimpleContent>& schema);
	
	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);

private:
	sptr<BXSDSimpleContent> m_simpleContent;
};

class BWsdlBindingCreator : public BCreator
{
public:
	BWsdlBindingCreator(const sptr<BWsdl::Binding>& wsdl);

	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);

private:
	sptr<BWsdl::Binding> m_binding;
	BWsdl::Binding::Operation m_op;
};

class BWsdlMessageCreator : public BCreator
{
public:
	BWsdlMessageCreator(const sptr<BWsdl::Message>& wsdl);

	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);

private:
	sptr<BWsdl::Message> m_message;
};

class BWsdlPortTypeCreator : public BCreator
{
public:
	BWsdlPortTypeCreator(const sptr<BWsdl::PortType>& wsdl);

	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);

private:
	sptr<BWsdl::PortType> m_portType;
	BWsdl::PortType::Operation m_op;
};

class BWsdlServiceCreator : public BCreator
{
public:
	BWsdlServiceCreator(const sptr<BWsdl::Service>& wsdl);

	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);

private:
	sptr<BWsdl::Service> m_service;
	SValue m_port;
	size_t m_index;
};

// #################################################################################
// ## SXMLTag ######################################################################
// #################################################################################

SXMLTag::SXMLTag()
{
}

SXMLTag::SXMLTag(const SString& tag)
{
	split(tag);
}

SXMLTag::SXMLTag(const SXMLTag& tag)
	:	m_namespace(tag.m_namespace),
		m_accessor(tag.m_accessor)
{
}

void SXMLTag::split(const SString& tag)
{
	int32_t colon = tag.FindFirst(":");
	if (colon > 0)
	{
		tag.CopyInto(m_namespace, 0, colon);
		tag.CopyInto(m_accessor, colon + 1, tag.Length() - colon - 1);
	}
	else 
	{
		m_namespace = "";
		m_accessor = tag;
	}
}

SString SXMLTag::Namespace()
{
	return m_namespace;
}

SString SXMLTag::Accessor()
{
	return m_accessor;
}
	

// #################################################################################
// ## BXMLSchema ###################################################################
// #################################################################################

BXMLSchema::BXMLSchema()
{
}

ssize_t BXMLSchema::AddElement(const sptr<BXSDElement>& type)
{
	SAutolock lock(m_lock.Lock());
	return m_elements.AddItem(type);
}

ssize_t BXMLSchema::AddComplexType(const sptr<BXSDComplexType>& type)
{
	SAutolock lock(m_lock.Lock());
	return m_complexTypes.AddItem(type);
}

ssize_t BXMLSchema::AddSimpleType(const sptr<BXSDSimpleType>& type)
{
	SAutolock lock(m_lock.Lock());
	return m_simpleTypes.AddItem(type);
}

size_t BXMLSchema::CountElements() const
{
	SAutolock lock(m_lock.Lock());
	return m_elements.CountItems();
}

size_t BXMLSchema::CountComplexTypes() const
{
	SAutolock lock(m_lock.Lock());
	return m_complexTypes.CountItems();
}

size_t BXMLSchema::CountSimpleTypes() const
{
	SAutolock lock(m_lock.Lock());
	return m_simpleTypes.CountItems();
}

sptr<BXSDElement> BXMLSchema::ElementAt(size_t index) const
{
	SAutolock lock(m_lock.Lock());
	return m_elements.ItemAt(index);
}

sptr<BXSDComplexType> BXMLSchema::ComplexTypeAt(size_t index) const
{
	SAutolock lock(m_lock.Lock());
	return m_complexTypes.ItemAt(index);
}

sptr<BXSDSimpleType> BXMLSchema::SimpleTypeAt(size_t index) const
{
	SAutolock lock(m_lock.Lock());
	return m_simpleTypes.ItemAt(index);
}

const SValue& BXMLSchema::Attributes() const
{
	SAutolock lock(m_lock.Lock());
	return m_attributes;
}

void BXMLSchema::SetAttributes(const SValue& attrs)
{
	SAutolock lock(m_lock.Lock());
	m_attributes = attrs;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const sptr<BXMLSchema>& schema)
{
#if 0
	size_t elementCount = schema->CountElements();
	size_t complexCount = schema->CountComplexTypes();
	size_t simpleCount = schema->CountSimpleTypes();
	
	io << "BXMLSchema:   elements = " << elementCount << endl;
	io << "              complex types = " << complexCount << endl;
	io << "              simple types = " << simpleCount << endl;
	io << "---------------------------------------------------------" << endl;
	
	for (size_t i = 0 ; i < elementCount ; i++)
	{
		bout << schema->ElementAt(i) << endl << endl;
	}
	
	for (size_t i = 0 ; i < complexCount ; i++)
	{
		bout << schema->ComplexTypeAt(i) << endl << endl;
	}

	for (size_t i = 0 ; i < simpleCount ; i++)
	{
	}
	
	io << "---------------------------------------------------------" << endl;
#endif
	return io;
}

// #################################################################################
// ## BXSDElement ##################################################################
// #################################################################################

BXSDElement::BXSDElement()
{
}

BXSDElement::BXSDElement(const SValue& attributes)
{
	m_attributes = attributes;
}

SString BXSDElement::Name() const
{
	return m_attributes[key_name].AsString();
}

SString BXSDElement::Id() const
{
	return m_attributes[key_id].AsString();
}

SString BXSDElement::Ref() const
{
	return m_attributes[key_ref].AsString();
}

SString BXSDElement::Type() const
{
	return m_attributes[key_type].AsString();
}

size_t BXSDElement::MinOccurs() const
{
	return (size_t)m_attributes[key_min_occurs].AsInt32();
}

size_t BXSDElement::MaxOccurs() const
{
	return (size_t)m_attributes[key_min_occurs].AsInt32();
}

bool BXSDElement::Nillable() const
{
	return m_attributes[key_nillable].AsBool();
}

SString BXSDElement::SubstitutionGroup() const
{
	return m_attributes[key_sub_group].AsString();
}

bool BXSDElement::Abstract() const
{
	return m_attributes[key_abstract].AsBool();
}

SString BXSDElement::Final() const
{
	return m_attributes[key_final].AsString();
}

SString BXSDElement::Block() const
{
	return m_attributes[key_block].AsString();
}

SString BXSDElement::Default() const
{
	return m_attributes[key_default].AsString();
}

SString BXSDElement::Fixed() const
{
	return m_attributes[key_fixed].AsString();
}

SString BXSDElement::Form() const
{
	return m_attributes[key_form].AsString();
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const sptr<BXSDElement>& element)
{
	io << "<xsd:element ";
	io << "name=\"" << element->Name() << "\" ";
	io << "type=\"" << element->Type() << "\"/>" << endl;
	return io;
}

// #################################################################################
// ## BXSDComplexType ##############################################################
// #################################################################################

BXSDComplexType::BXSDComplexType()
{
}

BXSDComplexType::BXSDComplexType(const SValue& attributes)
	:	m_attributes(attributes)
{
	m_name = m_attributes[key_name].AsString();
}

SString BXSDComplexType::Name() const
{
	return m_name;
}

size_t BXSDComplexType::CountElements() const
{
	SAutolock lock(m_lock.Lock());
	return m_elements.CountItems();
}

size_t BXSDComplexType::CountComplexContents() const
{
	SAutolock lock(m_lock.Lock());
	return m_complexContents.CountItems();
}

size_t BXSDComplexType::CountSimpleContents() const
{
	SAutolock lock(m_lock.Lock());
	return m_simpleContents.CountItems();
}

ssize_t BXSDComplexType::AddElement(const sptr<BXSDElement>& type)
{
	SAutolock lock(m_lock.Lock());
	return m_elements.AddItem(type);
}

ssize_t BXSDComplexType::AddComplexContent(const sptr<BXSDComplexContent>& content)
{
	SAutolock lock(m_lock.Lock());
	return m_complexContents.AddItem(content);
}

ssize_t BXSDComplexType::AddSimpleContent(const sptr<BXSDSimpleContent>& content)
{
	SAutolock lock(m_lock.Lock());
	return m_simpleContents.AddItem(content);
}

sptr<BXSDElement> BXSDComplexType::ElementAt(size_t index) const
{
	SAutolock lock(m_lock.Lock());
	return m_elements.ItemAt(index);
}

sptr<BXSDComplexContent> BXSDComplexType::ComplexContentAt(size_t index) const
{
	SAutolock lock(m_lock.Lock());
	return m_complexContents.ItemAt(index);
}

sptr<BXSDSimpleContent> BXSDComplexType::SimpleContentAt(size_t index) const
{
	SAutolock lock(m_lock.Lock());
	return m_simpleContents.ItemAt(index);
}

const SValue& BXSDComplexType::Attributes() const
{
	SAutolock lock(m_lock.Lock());
	return m_attributes;
}

void BXSDComplexType::BeginSequence()
{
	SAutolock lock(m_lock.Lock());
	m_sequences.AddItem(SValue(key_begin, SValue::Int32(m_elements.CountItems())));
}

void BXSDComplexType::EndSequence()
{
	SAutolock lock(m_lock.Lock());
	SValue& value = m_sequences.EditTop();
	value.JoinItem(key_end, SValue::Int32(m_elements.CountItems()));
}
 
const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const sptr<BXSDComplexType>& type)
{
	size_t elementCount = type->CountElements();

	io << "<xsd:complexType name=\"" << type->Attributes()[key_name].AsString() << "\">" << endl;

	for (size_t i = 0 ; i < elementCount ; i++)
	{
		//io << type->ElementAt(i) << endl;
	}
	return io;
}

// #################################################################################
// ## BXSDSimpleType ###############################################################
// #################################################################################

BXSDSimpleType::BXSDSimpleType()
{
}

BXSDSimpleType::BXSDSimpleType(const SValue& attributes)
	:	m_attributes(attributes)
{
}

// #################################################################################
// ## BXSDComplexContent ###########################################################
// #################################################################################

BXSDComplexContent::BXSDComplexContent()
{
}

ssize_t BXSDComplexContent::AddAttribute(const SValue& value)
{
	return m_attributes.AddItem(value);
}

size_t BXSDComplexContent::CountAttributes() const
{
	return m_attributes.CountItems();
}

const SValue& BXSDComplexContent::AttributeAt(size_t index) const
{
	return m_attributes.ItemAt(index);
}
// #################################################################################
// ## BXSDSimpleContent ############################################################
// #################################################################################

BXSDSimpleContent::BXSDSimpleContent()
{
}

BXSDSimpleContent::BXSDSimpleContent(const SValue& attributes)
	:	m_attributes(attributes)
{
}

// #################################################################################
// ## BXMLSchemaCreator ############################################################
// #################################################################################

BXMLSchemaCreator::BXMLSchemaCreator(const sptr<BXMLSchema>& schema)
	:	m_schema(schema)
{
}

status_t BXMLSchemaCreator::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	SXMLTag tag(name);

	SString accessor = tag.Accessor();
	if (accessor == "schema")
	{
		m_schema->SetAttributes(attributes);
	}
	else if (accessor == "element")
	{
		m_schema->AddElement(new BXSDElement(attributes));
	}
	else if (accessor == "complexType")
	{
		sptr<BXSDComplexType> type = new BXSDComplexType(attributes);
		m_schema->AddComplexType(type);
		newCreator = new BXSDComplexTypeCreator(type);
	}
	else if (accessor == "simpleType")
	{
#if 0	
		sptr<BXSDSimpleType> type = new BXSDSimpleType(attributes);
		m_schema->AddSimpleType(type);
		newCreator = new BXSDSimpleTypeCreator(type);
#endif	
	}

	return B_OK;
}

status_t BXMLSchemaCreator::OnEndTag(SString& name)
{
	return B_OK;
}

status_t BXMLSchemaCreator::OnText(SString& data)
{
	return B_OK;
}

// #################################################################################
// ## BXSDComplexTypeCreator #######################################################
// #################################################################################

BXSDComplexTypeCreator::BXSDComplexTypeCreator(const sptr<BXSDComplexType>& type)
	:	m_complexType(type)
{
}

status_t BXSDComplexTypeCreator::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	SXMLTag tag(name);

	if (tag.Accessor() == "all")
	{
	}
	else if (tag.Accessor() == "complexContent")
	{
		sptr<BXSDComplexContent> content = new BXSDComplexContent();
		m_complexType->AddComplexContent(content);
		newCreator = new BXSDComplexContentCreator(content);
	}
	else if (tag.Accessor() == "simpleContent")
	{
		SString msg("BXSDComplexTypeCreator: simpleContent is not implemented!");
		msg.Append(name);
		bout << msg << endl;
		ErrFatalError(msg.String());

#if 0
		sptr<BXSDSimpleContent> content = new BXSDSimpleContent(attributes);
		m_complexType->AddSimpleContent(content);
		newCreator = new BXSDSimpleContentCreator(content);
#endif
	}
	else if (tag.Accessor() == "element")
	{
		m_complexType->AddElement(new BXSDElement(attributes));	
	}
	else if (tag.Accessor() == "sequence")
	{
		m_complexType->BeginSequence();
	}
	else
	{
		SString msg("BXSDComplexTypeCreator: Unsupported tag: \"");
		msg.Append(name);
		msg.Append("\"");
		bout << msg << endl;
		ErrFatalError(msg.String());
	}
	return B_OK;
}

status_t BXSDComplexTypeCreator::OnEndTag(SString& name)
{
	SXMLTag tag(name);

	if (tag.Accessor() == "sequence")
	{
		m_complexType->EndSequence();
	}
	return B_OK;
}

status_t BXSDComplexTypeCreator::OnText(SString& data)
{
	return B_OK;
}


// #################################################################################
// ## BXSDComplexContentCreator ####################################################
// #################################################################################

BXSDComplexContentCreator::BXSDComplexContentCreator(const sptr<BXSDComplexContent>& content)
	:	m_complexContent(content)
{
}

status_t BXSDComplexContentCreator::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	SXMLTag tag(name);

	if (tag.Accessor() == "restriction" ||
		tag.Accessor() == "extension")
	{
		m_meta.Undefine();
		m_meta.JoinItem(SValue::String("type"), SValue::String(tag.Accessor()));
		m_meta.Overlay(attributes);
	}
	else
	{
		SValue content;
		content.JoinItem(SValue::String("tag"), SValue::String(tag.Accessor()));
		content.Overlay(m_meta);
		content.Overlay(attributes);
		m_complexContent->AddAttribute(content);
	}
	
	return B_OK;
}

status_t BXSDComplexContentCreator::OnEndTag(SString& name)
{
	return B_OK;
}

status_t BXSDComplexContentCreator::OnText(SString& data)
{
	return B_OK;
}

// #################################################################################
// ## BXSDSimpleContentCreator #####################################################
// #################################################################################

BXSDSimpleContentCreator::BXSDSimpleContentCreator(const sptr<BXSDSimpleContent>& content)
	:	m_simpleContent(content)
{
}

status_t BXSDSimpleContentCreator::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	return B_OK;
}

status_t BXSDSimpleContentCreator::OnEndTag(SString& name)
{
	return B_OK;
}

status_t BXSDSimpleContentCreator::OnText(SString& data)
{
	return B_OK;
}

// #################################################################################
// ## BWsdl ########################################################################
// #################################################################################

BWsdl::BWsdl()
{
	m_schema = new BXMLSchema();
}

SString BWsdl::Name() const
{
	return m_definitions["name"].AsString();
}

const SValue& BWsdl::Definitions() const
{
	return m_definitions;
}

ssize_t BWsdl::AddBinding(const sptr<BWsdl::Binding>& binding)
{
	SAutolock lock(m_lock.Lock());
	return m_bindings.AddItem(binding->Name(), binding);
}

ssize_t BWsdl::AddMessage(const sptr<BWsdl::Message>& message)
{
	SAutolock lock(m_lock.Lock());
	return m_messages.AddItem(message->Name(), message);
}

ssize_t BWsdl::AddPortType(const sptr<BWsdl::PortType>& port)
{
	SAutolock lock(m_lock.Lock());
	return m_portTypes.AddItem(port->Name(), port);
}

ssize_t BWsdl::AddService(const sptr<BWsdl::Service>& service)
{
	SAutolock lock(m_lock.Lock());
	return m_services.AddItem(service);
}

sptr<BXMLSchema> BWsdl::Schema() const
{
	SAutolock lock(m_lock.Lock());
	return m_schema;
}

size_t BWsdl::CountBindings() const
{
	SAutolock lock(m_lock.Lock());
	return m_bindings.CountItems();
}

size_t BWsdl::CountMessages() const
{
	SAutolock lock(m_lock.Lock());
	return m_messages.CountItems();
}

size_t BWsdl::CountPortTypes() const
{
	SAutolock lock(m_lock.Lock());
	return m_portTypes.CountItems();
}

size_t BWsdl::CountServices() const
{
	SAutolock lock(m_lock.Lock());
	return m_services.CountItems();
}

sptr<BWsdl::Message> BWsdl::MessageAt(size_t index) const
{
	SAutolock lock(m_lock.Lock());
	return m_messages.ValueAt(index);
}

sptr<BWsdl::Message> BWsdl::MessageFor(const SString& message)
{
	SAutolock lock(m_lock.Lock());
	return m_messages.ValueFor(message);
}

sptr<BWsdl::PortType> BWsdl::PortTypeAt(size_t index) const
{
	SAutolock lock(m_lock.Lock());
	return m_portTypes.ValueAt(index);	
}

sptr<BWsdl::PortType> BWsdl::PortTypeFor(const SString& port) const
{
	SAutolock lock(m_lock.Lock());
	bout << "PortTypeFor() port = " << port << endl;
	return m_portTypes.ValueFor(port);	
}


sptr<BWsdl::Service> BWsdl::ServiceAt(size_t index) const
{
	SAutolock lock(m_lock.Lock());
	return m_services.ItemAt(index);	
}

sptr<BWsdl::Binding> BWsdl::BindingAt(size_t index) const
{
	SAutolock lock(m_lock.Lock());
	return m_bindings.ValueAt(index);
}

sptr<BWsdl::Binding> BWsdl::BindingFor(const SString& binding) const
{
	SAutolock lock(m_lock.Lock());
	bout << "BindingFor() binding = " << binding << endl;
	bout << "m_bindings = " << m_bindings.CountItems() << endl;
	return m_bindings.ValueFor(binding);
}

void BWsdl::DumpMessages()
{
	size_t count = CountMessages();

	for (size_t i = 0 ; i < count ; i++)
	{
		sptr<BWsdl::Message> message = MessageAt(i);
		
		bout << "<SOAP-ENV:Envelope ";
		bout << "xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" ";
		bout << "xmlns:xsi=\"http://www.w3.org/1999/XMLSchema-instance\" ";
		bout << "xmlns:xsd=\"http://www.w3.org/1999/XMLSchema\">" << endl;
		bout << indent;
		bout << "<SOAP-ENV:Body>" << endl;
		bout << indent;
		bout << "<" << message->Name() << " ";
		// add the urn:namespace
		bout << "SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">" << endl;
		bout << dedent;
		bout << dedent;
		bout << endl;

	}
}

void BWsdl::SetDefinitions(const SValue& value)
{
	m_definitions = value;
}

BWsdl::Binding::Binding(const SValue& attrs)
	:	m_name(attrs["name"].AsString()),
		m_type(attrs["type"].AsString())
{
	SXMLTag name(attrs["name"].AsString());
	SXMLTag type(attrs["type"].AsString());

	m_name = name.Accessor();
	m_type = type.Accessor();
}

SString BWsdl::Binding::Name() const
{
	return m_name;
}

SString BWsdl::Binding::Type() const
{
	return m_type;
}

SString BWsdl::Binding::Style() const
{
	return m_style;
}

SString BWsdl::Binding::Transport() const
{
	return m_transport;
}

void BWsdl::Binding::SetStyle(const SString& style)
{
	m_style = style;
}

void BWsdl::Binding::SetTransport(const SString& trans)
{
	m_transport = trans;
}

ssize_t BWsdl::Binding::AddOperation(const Operation& operation)
{
	return m_operations.AddItem(operation);
}

size_t BWsdl::Binding::CountOperations() const
{
	return m_operations.CountItems();
}

BWsdl::Binding::Operation BWsdl::Binding::OperationAt(size_t index) const
{
	return m_operations.ItemAt(index);
}
		
BWsdl::Message::Message(const SValue& attrs)
	:	m_attributes(attrs)
{
}

SString BWsdl::Message::Name() const
{
	return m_attributes["name"].AsString();
}

ssize_t BWsdl::Message::AddPart(const SValue& part)
{
	return m_parts.AddItem(part);
}

size_t BWsdl::Message::CountParts() const
{
	return m_parts.CountItems();
}

const SValue& BWsdl::Message::PartAt(size_t index) const
{
	return m_parts.ItemAt(index);
}

BWsdl::PortType::PortType(const SString& name)
	:	m_name(name)
{
}

SString BWsdl::PortType::Name() const
{
	return m_name;
}

ssize_t BWsdl::PortType::AddOperation(const Operation& operation)
{
	return m_operations.AddItem(operation.name, operation);
}

size_t BWsdl::PortType::CountOperations() const
{
	return m_operations.CountItems();
}

BWsdl::PortType::Operation BWsdl::PortType::OperationAt(size_t index) const
{
	return m_operations.ValueAt(index);
}

BWsdl::PortType::Operation BWsdl::PortType::OperationFor(const SString& op) const
{
	return m_operations.ValueFor(op);
}

		
BWsdl::Service::Service(const SValue& attrs)
	:	m_attributes(attrs)
{
}

SString BWsdl::Service::Name() const
{
	return m_attributes["name"].AsString();
}

ssize_t BWsdl::Service::AddPort(const SValue& port)
{
	return m_ports.AddItem(port);
}

size_t BWsdl::Service::CountPorts() const
{
	return m_ports.CountItems();
}

const SValue& BWsdl::Service::PortAt(size_t index) const
{
	return m_ports.ItemAt(index);
}

// #################################################################################
// ## BWsdlCreator #################################################################
// #################################################################################

BWsdlCreator::BWsdlCreator()
{
}

status_t BWsdlCreator::Parse(const SString& string, const sptr<BWsdl>& wsdl)
{
	m_wsdl = wsdl;
//	bout << "Parse:" << indent << endl << string << dedent << endl;
	status_t err = ParseXML(this, new BXMLBufferSource(string.String(), string.Length()), B_XML_DONT_EXPAND_CHARREFS);
//	bout << "Parse: err = " << (void*)err << endl;
//	bout << "Parse: B_XML_ATTR_NAME_IN_USE = " << (void*)B_XML_ATTR_NAME_IN_USE << endl;
	return B_OK;
}

status_t BWsdlCreator::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	SXMLTag tag(name);

//	bout << "BWsdlCretor name = " << name << " attrs = " << attributes << endl;

	if (tag.Accessor() == "types")
	{
		newCreator = new BXMLSchemaCreator(m_wsdl->Schema());
	}
	else if (tag.Accessor() == "message")
	{
		sptr<BWsdl::Message> message = new BWsdl::Message(attributes);
		m_wsdl->AddMessage(message);
		newCreator = new BWsdlMessageCreator(message);
	}
	else if (tag.Accessor() == "portType")
	{
		sptr<BWsdl::PortType> port = new BWsdl::PortType(attributes["name"].AsString());
		m_wsdl->AddPortType(port);
		newCreator = new BWsdlPortTypeCreator(port);
	}
	else if (tag.Accessor() == "binding")
	{
		sptr<BWsdl::Binding> binding = new BWsdl::Binding(attributes);
		m_wsdl->AddBinding(binding);
		newCreator = new BWsdlBindingCreator(binding);
	}
	else if (tag.Accessor() == "service")
	{
		sptr<BWsdl::Service> service = new BWsdl::Service(attributes);
		m_wsdl->AddService(service);
		newCreator = new BWsdlServiceCreator(service);
	}
	else if (tag.Accessor() == "definitions")
	{
		m_wsdl->SetDefinitions(attributes);
	}
	
	return B_OK;
}

status_t BWsdlCreator::OnEndTag(SString& tag)
{
	return B_OK;
}

status_t BWsdlCreator::OnText(SString& value)
{
	return B_OK;
}

// #################################################################################
// ## BWsdlBindingCreator ##########################################################
// #################################################################################

BWsdlBindingCreator::BWsdlBindingCreator(const sptr<BWsdl::Binding>& binding)
	:	m_binding(binding)
{
}

status_t BWsdlBindingCreator::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
//	bout << "BWsdlBindingCreator name = " << name << " attr = " << attributes << endl;
	if (name == "soap:binding")
	{
		m_binding->SetStyle(attributes["style"].AsString());
		m_binding->SetTransport(attributes["transport"].AsString());
	}
	else if (name == "operation")
	{
		m_op.name = attributes["name"].AsString();
	}
	else if (name == "soap:operation")
	{
		m_op.urn = attributes["soapAction"].AsString();
	}
	
	return B_OK;
}

status_t BWsdlBindingCreator::OnEndTag(SString& tag)
{
	if (tag == "operation")
	{
		m_binding->AddOperation(m_op);
		m_op.name = "";
		m_op.urn = "";
	}
	
	return B_OK;
}

status_t BWsdlBindingCreator::OnText(SString& value)
{
	return B_OK;
}

// #################################################################################
// ## BWsdlMessageCreator ##########################################################
// #################################################################################

BWsdlMessageCreator::BWsdlMessageCreator(const sptr<BWsdl::Message>& message)
	:	m_message(message)
{
}

status_t BWsdlMessageCreator::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	if (name == "part")
	{
		m_message->AddPart(attributes);
	}
	
	return B_OK;
}

status_t BWsdlMessageCreator::OnEndTag(SString& tag)
{
	return B_OK;
}

status_t BWsdlMessageCreator::OnText(SString& value)
{
	return B_OK;
}

// #################################################################################
// ## BWsdlPortTypeCreator #########################################################
// #################################################################################

BWsdlPortTypeCreator::BWsdlPortTypeCreator(const sptr<BWsdl::PortType>& portType)
	:	m_portType(portType)
{
}

status_t BWsdlPortTypeCreator::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
//	bout << "BWsdlPortTypeCreator name = " << name << " attrs = " << attributes << endl;
	if (name == "operation")
	{
		m_op.name = attributes["name"].AsString();
		m_op.input = "";
		m_op.output = "";
	}
	else if (name == "input")
	{
		m_op.input = attributes["message"].AsString();
	}
	else if (name == "output")
	{
		m_op.output = attributes["message"].AsString();
	}
	else
	{
		bout << "BWsdlPortTypeCreator::OnStartTag: could not process tag=" << name << endl;
	}

	return B_OK;
}

status_t BWsdlPortTypeCreator::OnEndTag(SString& tag)
{
	if (tag == "operation")
	{
		m_portType->AddOperation(m_op);
	}
	return B_OK;
}

status_t BWsdlPortTypeCreator::OnText(SString& value)
{
	return B_OK;
}

// #################################################################################
// ## BWsdlServiceCreator ##########################################################
// #################################################################################

BWsdlServiceCreator::BWsdlServiceCreator(const sptr<BWsdl::Service>& service)
	:	m_service(service),
		m_index(0)
{
}

status_t BWsdlServiceCreator::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	SXMLTag tag(name);
	
	if (tag.Accessor() == "port")
	{
		m_port.Overlay(attributes);
	}
	else
	{
		m_port.JoinItem(SValue::Int32(m_index), SValue(SValue::String(tag.Accessor()), attributes[SValue::String("location")]));
		m_index++;
	}
	
	return B_OK;
}

status_t BWsdlServiceCreator::OnEndTag(SString& tag)
{
	if (tag == "port")
	{
		m_port.JoinItem(SValue::String("count"), SValue::Int32(m_index));
		m_service->AddPort(m_port);
		m_port.Undefine();
		m_index = 0;
	}
	return B_OK;
}

status_t BWsdlServiceCreator::OnText(SString& value)
{
	return B_OK;
}
