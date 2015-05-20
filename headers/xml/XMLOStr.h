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

#ifndef _XML2_XMLOSTR_H
#define _XML2_XMLOSTR_H

#include <xml/IXMLOStr.h>
#include <support/Binder.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace support;
#endif

/**************************************************************************************/

class CXMLOStr : public BnInterface<IXMLOStr>
{
public:
						CXMLOStr();
	virtual				~CXMLOStr();

	virtual	status_t	Told(SValue &in);

private:
						CXMLOStr(const CXMLOStr& o);
};

/**************************************************************************************/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::xml
#endif

#endif // _B_XML2_PARSER_H
