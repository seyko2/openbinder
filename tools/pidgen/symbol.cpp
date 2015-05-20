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

#include "symbolstack.h"

IDLSymbol::IDLSymbol()
{
}

IDLSymbol::IDLSymbol(const SString name)
	: m_id(name), m_type(NULL)
{
}

IDLSymbol::IDLSymbol(const SString name, const sptr<IDLType>& aType)
	: m_id(name), m_type(aType)  
{	
}

IDLSymbol::IDLSymbol(const IDLSymbol& aSymbol)
	: m_id(aSymbol.m_id), m_type(aSymbol.m_type)
{	
} // copy constructor

IDLSymbol::~IDLSymbol(void)
{
	#ifdef SYMDEBUG
		//bout << "<----- symbol.cpp -----> ~IDLSymbol called - let's all go to hell!\n";
	#endif SYMDEBUG
// nothing is done here since m_type is handled by sptr reference counting
} // destructor

IDLSymbol & 
IDLSymbol::operator = (const IDLSymbol &symbol)
{	m_id=symbol.m_id;
	m_type=symbol.m_type;
	return *this;
} // assignment operator

		
status_t	
IDLSymbol::SetType(const sptr<IDLType>& aType)
{	m_type=aType;
	return B_OK;
}

sptr<IDLType> 
IDLSymbol::GetType()
{	return m_type;
}

SString
IDLSymbol::GetId()
{	return m_id;
}
