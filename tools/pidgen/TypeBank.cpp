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
#include "yacc.h"
#include <stdio.h>
#include <stdlib.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

// to add types, update initTypeBank and scanner.l
// you may also want to update the functions in OutputUtil.cpp

void cleanTypeBank(SKeyedVector<SString, sptr<IDLType> >& tb)
{
	tb.MakeEmpty();	
}

status_t
initTypeBank(SKeyedVector<SString, sptr<IDLType> >& tb)
{

	size_t	numberoftypes;
	SKeyedVector<SString, uint32_t> stdtypes;
	SKeyedVector<SString, SString>	 ToSValue;
	SKeyedVector<SString, SString>	 FromSValue;

	// 0-4 : std types where X -> X() & var.AsX();		
	stdtypes.AddItem(SString("bool"), B_BOOL_TYPE);				
	stdtypes.AddItem(SString("float"), B_FLOAT_TYPE);				
	stdtypes.AddItem(SString("double"), B_DOUBLE_TYPE);			
	stdtypes.AddItem(SString("int32_t"), B_INT32_TYPE);				
	stdtypes.AddItem(SString("int64_t"), B_INT64_TYPE);				
	// 5-11 : std types where X-> Int32(var) & (x)var.AsInt32();
	stdtypes.AddItem(SString("int8_t"), B_INT8_TYPE);				
	stdtypes.AddItem(SString("int16_t"), B_INT16_TYPE);				
	stdtypes.AddItem(SString("uint8_t"), B_UINT8_TYPE);				
	stdtypes.AddItem(SString("uint16_t"), B_UINT16_TYPE);			
	stdtypes.AddItem(SString("uint32_t"), B_UINT32_TYPE);			
	stdtypes.AddItem(SString("size_t"), B_SIZE_T_TYPE);			
	stdtypes.AddItem(SString("char"), B_CHAR_TYPE);				
	// 12-15 : binder types
	stdtypes.AddItem(SString("sptr"), B_BINDER_TYPE);				
	stdtypes.AddItem(SString("wptr"), B_BINDER_WEAK_TYPE);				
	// 16-27 : std types that need customization
	stdtypes.AddItem(SString("nsecs_t"), B_NSECS_TYPE);		
	stdtypes.AddItem(SString("bigtime_t"), B_BIGTIME_TYPE);		
	stdtypes.AddItem(SString("off_t"), B_OFF_T_TYPE);				
	stdtypes.AddItem(SString("ssize_t"), B_SSIZE_T_TYPE);			
	stdtypes.AddItem(SString("status_t"), B_INT32_TYPE);			
	stdtypes.AddItem(SString("uint64_t"), B_UINT64_TYPE);			
	stdtypes.AddItem(SString("wchar32_t"), B_WCHAR_TYPE);		
	stdtypes.AddItem(SString("SValue"), B_VALUE_TYPE);
	stdtypes.AddItem(SString("SString"), B_STRING_TYPE);
	stdtypes.AddItem(SString("SMessage"), B_MESSAGE_TYPE);
	stdtypes.AddItem(SString("char*"), B_CONSTCHAR_TYPE);

	numberoftypes=stdtypes.CountItems();

	// bout << "Debug TypeBank - # of stdtypes loaded=" << numberoftypes << endl << endl;

	for (size_t index=0; index<numberoftypes ; index++)
	{	SString name=stdtypes.KeyAt(index);
		SString output=name;

		uint32_t	code=stdtypes.ValueAt(index);

		// initialize for the standard types

		switch (code) {

		case B_BOOL_TYPE :				
		case B_FLOAT_TYPE :			
		case B_DOUBLE_TYPE:				
		{
			output.Capitalize();
			ToSValue.AddItem(name, output);
			output.Prepend("As");
			FromSValue.AddItem(name, output);
		}	break;

		case B_INT8_TYPE:				
		case B_INT16_TYPE:			
		case B_UINT8_TYPE:				
		case B_UINT16_TYPE:			
		case B_UINT32_TYPE:			
		case B_SIZE_T_TYPE:			
		case B_CHAR_TYPE:	
		case B_WCHAR_TYPE:
		case B_INT32_TYPE:
		{
			ToSValue.AddItem(name, SString("Int32"));
			FromSValue.AddItem(name, SString("AsInt32"));
		}	break;

		case B_INT64_TYPE:
		{
			ToSValue.AddItem(name, SString("Int64"));
			FromSValue.AddItem(name, SString("AsInt64"));
		}	break;

		case B_BINDER_TYPE:
		case B_BINDER_WEAK_TYPE:
		{	
			ToSValue.AddItem(name, SString());
			FromSValue.AddItem(name, SString());
		}	break;
		// initialize types that need customized output
		case B_MESSAGE_TYPE:
		case B_VALUE_TYPE:
		{	output=SString();
			ToSValue.AddItem(name, output);
			output.Prepend("As");
			FromSValue.AddItem(name, output);
		}	break;

		case B_STRING_TYPE:
		case B_BIGTIME_TYPE:
		case B_NSECS_TYPE:
		case B_OFF_T_TYPE:				
		case B_SSIZE_T_TYPE:			
		case B_UINT64_TYPE:	
		case B_CONSTCHAR_TYPE:
		{	
			if (name=="char*")
				{	output=SString("String"); }
			if (name=="SString") 
				{	output=SString("String"); }
			if ((name=="nsecs_t") || (name=="bigtime_t")) 
				{	output=SString("Time"); }	
			if (name=="off_t")
				{	output=SString("Offset"); }		
			if (name=="ssize_t")
				{	output=SString("SSize"); }
			if (name=="uint64_t")
				{	output=SString("Int64"); }
			
			ToSValue.AddItem(name, output);
			output.Prepend("As");

			FromSValue.AddItem(name, output);
		}	break;
		}
	}

	//  put all the info we have into the typebank
	for (size_t	index2=0; index2 < numberoftypes; index2++)
	{
			bool present=false;
			bool present1=false;

			SString tname=stdtypes.KeyAt(index2);
			uint32_t tcode=stdtypes.ValueAt(index2);
			SString func1=ToSValue.EditValueFor(tname, &present);
			SString func2=FromSValue.EditValueFor(tname, &present1);

			if ((present) && (present1))
			{	
				sptr<IDLType>	typeobj=new IDLType(tname, tcode);
				sptr<jmember>	f1=new jmember(func1, SString("SValue"));
				sptr<jmember>	f2=new jmember(func2, tname);
				
				typeobj->AddMember(f1);
				typeobj->AddMember(f2);

				tb.AddItem(tname, typeobj);
			}
			
			else { berr << "type initialization for typename=" << tname << " failed \n"; }	
	}

	//checktb(tb);

	return B_OK;
}

void 
checktb (SKeyedVector<SString, sptr<IDLType> > tb)
{	
	int32_t  numberoftypes=tb.CountItems();

	if (numberoftypes>0) {
			bout << "we have initialized " << numberoftypes << " types" << endl;

			for (int32_t index=0; index<numberoftypes ; index++)
			{	bout << endl;
	
				SString	typekey=tb.KeyAt(index);
				sptr<IDLType>	temp=tb.ValueAt(index);
				bout << "key=" << typekey << " type=" << temp->GetCode() << endl;
					
					for (int32_t index2=0; index2<2 ; index2++)
					{		
						bout << "function=" << (temp->GetMemberAt(index2))->ID() << endl;
					}
			}
	}
}
