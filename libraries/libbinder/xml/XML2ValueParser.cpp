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

#include <xml/XML2ValueParser.h>

#include <stdlib.h>
#include <ctype.h>

#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace palmos::support;
#endif

static int32_t gID = 0;

B_STATIC_STRING_VALUE_SMALL(kIDAttribute, "id", );
B_STATIC_STRING_VALUE_LARGE(kIDTypeAttribute, "id_type", );
B_STATIC_STRING_VALUE_LARGE(kTypeAttribute, "type", );
B_STATIC_STRING_VALUE_LARGE(kTypeCodeAttribute, "type_code", );
B_STATIC_STRING_VALUE_LARGE(kSizeAttribute, "size", );
B_STATIC_STRING_VALUE_LARGE(kResIDAttribute, "resid", );
B_STATIC_STRING_VALUE_LARGE(kIndexAttribute, "index", );
B_STATIC_STRING_VALUE_SMALL(kRefAttribute, "ref", );

// ==================================================================
BXML2ValueCreator::BXML2ValueCreator(SValue &targetValue, const SValue &attributes, const SPackage& resources,
										const SValue& references)
	:m_resources(resources)
	,m_targetValue(targetValue)
	,m_status(B_OK)
	,m_references(references)
	,m_isReference(false)
{
	m_id = gID++;

	SValue ref=attributes[kRefAttribute];
	if (ref.IsDefined()) {
		if (attributes.CountItems() != 1) {
			// if you're a value ref, then that's it
			m_status = B_BAD_VALUE;
			return ;
		}
		m_value = references[ref];
		m_isReference = true;
	}
	
	SValue key=attributes[kIDAttribute];
	status_t err;
	
	if (key.IsDefined())
	{
		SString key_type;
		
		key_type = attributes[kIDTypeAttribute].AsString(&err);
		if (err != B_OK)
			key_type = "string";
		
		if (key_type == "string")
			m_key = SValue::String(key.AsString());
		else if (key_type == "int32_t")
		{
			int64_t val;
			status_t result=ParseSignedInteger(key.AsString(),0x7fffffff,&val);
			
			if (result<B_OK)
				return ;
			
			m_key = SValue::Int32(static_cast<int32_t>(val));
		}
		else
		{
			return ;
		}
	}
	else	
		m_key = B_WILD_VALUE;

	SValue dataType = attributes[kTypeAttribute];
	if (!dataType.IsDefined())
		m_dataType = "wild";
	m_dataType = dataType.AsString();
	
	SValue rawTypeCode = attributes[kTypeCodeAttribute];
	if (rawTypeCode.IsDefined())
		m_rawTypeCode = B_TYPE_CODE(rawTypeCode.AsInt32());
	else
		m_rawTypeCode = 0;
	
	SValue rawSize = attributes[kSizeAttribute];
	if (rawSize.IsDefined())
		m_rawSize = rawSize.AsInt32();
	else
		m_rawSize = rawSize.AsInt32();

	m_resID = attributes[kResIDAttribute].AsInt32(&err);
	if (err != B_OK) m_resID = -1;
	else {
		m_strIndex = attributes[kIndexAttribute].AsInt32(&err);
	}

	m_data.Truncate(0);
	
}

status_t
BXML2ValueCreator::OnStartTag(	SString			& name,
								SValue			& attributes,
								sptr<BCreator>	& newCreator	)
{
	if (m_status != B_OK) return m_status;
	if (m_isReference) return B_BAD_VALUE;
	if (name != "value") return B_BAD_VALUE;
	newCreator = new BXML2ValueCreator(m_value, attributes, m_resources, m_references);
	return B_OK;
}

status_t
BXML2ValueCreator::OnEndTag(	SString			& name			)
{
	if (m_status != B_OK) return m_status;
	(void) name;
	return B_OK;
}

status_t
BXML2ValueCreator::OnText(		SString			& data			)
{
	if (m_status != B_OK) return m_status;
	if (m_isReference && !data.IsOnlyWhitespace()) return B_BAD_VALUE;
	m_data.Append(data);
	
	return B_OK;
}

static unsigned char
fromhex(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c-'0';
	else if (c >= 'a' && c <= 'f')
		return c-'a'+0xa;
	else if (c >= 'A' && c <= 'F')
		return c-'A'+0xa;
	else
		return 0;
}

status_t
BXML2ValueCreator::Done()
{
	if (m_status != B_OK) return m_status;

	if (m_isReference) {
		// m_value is already the value
	}
	else if (m_resID >= 0)
	{
		ErrFatalError("BXML2ValueCreator: implement reading strings from the package");
#if !LINUX_DEMO_HACK
		if (m_resources == B_NO_PACKAGE) m_value = SValue::Status(B_BAD_DATA);
		else m_value = SValue::String(m_resources.LoadString(m_resID, m_strIndex));
#endif
	}
	else if (m_dataType == "undefined")
		m_value.Undefine();
	else if (m_dataType == "wild")
		m_value=B_WILD_VALUE;
	else if (m_dataType == "raw")
	{
		unsigned char *d = (unsigned char *)malloc(m_data.Length()+1); // max size is the same as the old one if nothing's escaped
		unsigned char *data = d;
		const unsigned char *p = (const unsigned char *)m_data.String();
		size_t length = 0;
		
		while (*p)
		{
			if (*p == '&')
			{
				p++;
				if (*p == '#')
				{
					p++;
					if (*p == 'x')
					{
						p++;
						// only support one byte at a time. not correct wrt utf-8
						unsigned char c = fromhex(*p);
						c <<= 4;
						p++;
						c |= fromhex(*p);
						*d = c;
						p++;
						d++;
						length++;
						ErrFatalErrorIf(*p != ';', "XML2ValueParser got XML that it can't handle");
						p++; // handle the ';'
					}
					else
						; // not supported -- the writer won't write this
				}
				else
					; // not supported -- the writer won't write this
			}
			else
			{
				*d = *p;
				d++;
				p++;
				length++;
			}
		}
#if 0		
		bout << "*****************************" << endl;
		bout << "m_key = " << m_key << endl;
		bout << "m_rawSize is: " << m_rawSize << endl;
		bout << "m_rawTypeCode = " << STypeCode(m_rawTypeCode) << endl;
		bout << SHexDump(data, m_rawSize) << endl;
#endif
		m_value.Assign(m_rawTypeCode, data, m_rawSize);
		
		free(data);
#if 0
		bout << "BXML2ValueCreator::Done with m_dataType == raw m_data: " << m_data << endl;
		bout << "SValue is: " << m_value << endl;
		bout << "*****************************" << endl;
#endif
	}
	else if (!m_data.IsOnlyWhitespace())
	{
		m_data.Trim();
		
		status_t result;
		int64_t val;
		
		if (m_dataType == "string")
		{
			m_value = SValue::String(m_data);
		}
		else if (m_dataType == "int8_t")
		{
			result = ParseSignedInteger(m_data,0x7f,&val);
			
			if (result<B_OK)
				return B_BAD_DATA;
				
			m_value = SValue::Int8(static_cast<int8_t>(val));
		}
		else if (m_dataType == "int16_t")
		{
			result = ParseSignedInteger(m_data,0x7fff,&val);
			
			if (result<B_OK)
				return B_BAD_DATA;
				
			m_value = SValue::Int16(static_cast<int16_t>(val));
		}
		else if (m_dataType == "int32_t")
		{
			result = ParseSignedInteger(m_data,0x7fffffff,&val);
			
			if (result<B_OK)
				return B_BAD_DATA;
				
			m_value = SValue::Int32(static_cast<int32_t>(val));
		}
		else if (m_dataType == "int64_t")
		{
			result = ParseSignedInteger(m_data,0x7fffffffffffffffLL,&val);
			
			if (result<B_OK)
				return B_BAD_DATA;
				
			m_value = SValue::Int64(val);
		}
		else if (m_dataType == "time")
		{
			result = ParseSignedInteger(m_data,0x7fffffffffffffffLL,&val);
			
			if (result<B_OK)
				return B_BAD_DATA;
				
			m_value = SValue::Time(val);
		}
		else if (m_dataType == "bool")
		{
			if (m_data == "true")
				m_value = SValue::Bool(true);
			else if (m_data == "false")
				m_value = SValue::Bool(false);
			else
				return B_BAD_DATA;
		}
		else if (m_dataType == "float")
		{
			const char* ex = m_data.String();
			float float_val = static_cast<float>(strtod(ex, const_cast<char**>(&ex)));
			
			m_value = SValue::Float(float_val);
		}
		else if (m_dataType == "double")
		{
			const char* ex = m_data.String();
			double double_val = strtod(ex, const_cast<char**>(&ex));
			
			m_value = SValue::Double(double_val);
		}
		else
			return B_BAD_DATA;
	}
	
	m_data.Truncate(0);
	
	m_targetValue.JoinItem(m_key, m_value);
	return B_OK;
}

status_t
BXML2ValueCreator::ParseSignedInteger(const SString &from, int64_t maximum, int64_t *val)
{
	status_t err;
	*val = SValue(from).AsInt64(&err);
	
	if (err != B_OK)
		return err;
	
	if ((*val)<(-maximum-1) || (*val)>maximum)
		return B_ERROR;
		
	return B_OK;
}
								

#if _SUPPORTS_NAMESPACE
} } // palmos::xml
#endif
