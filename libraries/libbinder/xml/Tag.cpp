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

#include <support/Value.h>
#include <xml/Tag.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::xml;
#endif

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
