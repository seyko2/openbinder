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

#ifndef XML_TAG_H
#define XML_TAG_H

#include <support/String.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace support;
#endif

class SXMLTag
{
public:
	SXMLTag();
	SXMLTag(const SString& tag);
	SXMLTag(const SXMLTag& tag);

	SString Namespace();
	SString Accessor();
private:
	void split(const SString& tag);

	SString m_namespace;
	SString m_accessor;
};

#if _SUPPORTS_NAMESPACE
} }
#endif

#endif // XML_TAG_H
