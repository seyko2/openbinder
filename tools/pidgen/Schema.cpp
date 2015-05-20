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

#if _SUPPORTS_NAMESPACE
using namespace palmos::soap;
#endif

B_CONST_STRING_VALUE_8	(key_id, 				"id",);
B_CONST_STRING_VALUE_8	(key_ref, 				"ref",);
B_CONST_STRING_VALUE_8	(key_form, 				"form",);
B_CONST_STRING_VALUE_8	(key_name, 				"name",);
B_CONST_STRING_VALUE_8	(key_type, 				"type",);
B_CONST_STRING_VALUE_8	(key_block, 			"block",);
B_CONST_STRING_VALUE_8	(key_final, 			"final",);
B_CONST_STRING_VALUE_8	(key_fixed, 			"fixed",);
B_CONST_STRING_VALUE_8	(key_default, 			"default",);
B_CONST_STRING_VALUE_12	(key_abstract, 			"abstract",);
B_CONST_STRING_VALUE_12	(key_nillable, 			"nillable",);
B_CONST_STRING_VALUE_12	(key_max_occurs, 		"maxOccurs",);
B_CONST_STRING_VALUE_12	(key_min_occurs, 		"minOccurs",);
B_CONST_STRING_VALUE_20	(key_sub_group, 		"substitutionGroup",);

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
	else
	{
		SString msg("BXSDComplexTypeCreator: Unsupported tag: ");
		msg.Append(name);
		bout << msg << endl;
		debugger(msg.String());
	}
	return B_OK;
}

status_t BXSDComplexTypeCreator::OnEndTag(SString& name)
{
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
