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

#include <xml/Value2XML.h>

#include <xml/XML2ValueParser.h>

#include <xml/Writer.h>
#include <xml/DataSource.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif

static status_t
ValueToXML (BWriter &writer, const SValue &key, const SValue &value)
{
	SValue attributes;
	uint32_t hints=0;
	
	if (key.IsSpecified())
	{
		ASSERT(key.IsSimple());
		
		attributes.JoinItem(SValue::String("id"), SValue::String(key.AsString()));

		SString type;
				
		switch (key.Type())
		{
			case B_STRING_TYPE:
				type="string";
				break;
				
			case B_INT32_TYPE:
				type="int32_t";
				break;
			
			default:
				type="unknown";
				TRESPASS();
				
				return B_BAD_TYPE;
				break;
		}				

		if (type!="string")
			attributes.JoinItem(SValue::String("id_type"),SValue::String(type));
	}
	
	bool write_data = true;
	bool type_is_raw = false;
	SString tc;
	char *tcs;
	uint32_t t;
	
	if (value.IsSimple())
	{
		hints=BWriter::NO_EXTRA_WHITESPACE;
		
		SString type;
		if (!value.IsDefined())
		{
			type="undefined";
			write_data = false;
		}
		else if (value.IsWild())
		{
			type="wild";
			write_data = false;
		}
		else switch (value.Type())
		{
			case B_STRING_TYPE:
				type="string";
				hints=0;
				break;
			
			case B_INT8_TYPE:
				type="int8_t";
				break;
			
			case B_INT16_TYPE:
				type="int16_t";
				break;

			case B_INT32_TYPE:
				type="int32_t";
				break;

			case B_INT64_TYPE:
				type="int64_t";
				break;

			case B_TIME_TYPE:
				type="time";
				break;

			case B_BOOL_TYPE:
				type="bool";
				break;

			case B_FLOAT_TYPE:
				type="float";
				break;

			case B_DOUBLE_TYPE:
				type="double";
				break;
			
			default:
				type="raw";
				type_is_raw = true;

				t = value.Type();
				// type code is 3 bytes
				tcs = tc.LockBuffer(6);
				tcs[0] = '[';
				tcs[1] = (char)((t >> 24) & 0xff);
				tcs[2] = (char)((t >> 16) & 0xff);
				tcs[3] = (char)((t >> 8) & 0xff);
				tcs[4] = ']';
				tcs[5] = 0;
				tc.UnlockBuffer();
				attributes.JoinItem(SValue::String("type_code"), SValue::String(tc));
				attributes.JoinItem(SValue::String("size"), SValue::Int32(value.Length()));
				break;
		}
				
		attributes.JoinItem(SValue::String("type"), SValue::String(type));
	}

	writer.StartTag(SString("value"),attributes,hints);

	if (value.IsSimple() && write_data)
	{
		if (!type_is_raw)
		{
			SString s = value.AsString();	
			writer.TextData(s.String(),s.Length());
		}
		else
		{
//			bout << "===========================" << endl;
//			bout << "writing SValue: " << value << endl;
//			bout << SHexDump(value.Data(), value.Length()) << endl;
//			bout << "===========================" << endl;
			writer.WriteEscaped((const char*)value.Data(), value.Length());
		}
	}
	else if (write_data)
	{
		SValue sub_key;
		SValue sub_value;
		
		void *cookie=NULL;
		while (value.GetNextItem(&cookie,&sub_key,&sub_value)>=B_OK)
		{
			status_t result=ValueToXML(writer,sub_key,sub_value);
			
			if (result<B_OK)
			{
				return result;
			}
		}
	}
	
	writer.EndTag();
	
	return B_OK;
}

status_t
ValueToXML (const sptr<IByteOutput>& stream, const SValue &value)
{	
	BWriter writer(stream,BWriter::BALANCE_WHITESPACE);

	return ValueToXML(writer,B_WILD_VALUE,value);		
}

status_t 
XMLToValue (const sptr<IByteInput>& stream, SValue &value)
{
	sptr<BCreator> creator = new BXML2ValueCreator(value, SValue::Undefined());
	BXMLIByteInputSource source(stream);
	return ParseXML(creator,&source,0);	
}

#if _SUPPORTS_NAMESPACE
} } // palmos::xml
#endif
