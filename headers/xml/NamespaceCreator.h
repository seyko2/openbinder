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

#ifndef _XML2_NAMESPACE_CREATOR_H
#define _XML2_NAMESPACE_CREATOR_H

#include <xml/Parser.h>
#include <support/KeyedVector.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif // _SUPPORTS_NAMESPACE

class BNamespaceMap : public SAtom
{
public:
	
	
private:
	
};

class BNamespaceCreator : public BCreator
{
public:
	
	BNamespaceCreator();
	//BNamespaceCreator(
	
	virtual status_t	OnStartTag(				SString			& name,
												SValue			& attributes,
												sptr<BCreator>	& newCreator	);
									
	virtual status_t	OnStartTag(				const SString			& localName,
												const SString			& nsID
												const SValue			& attributes,
												sptr<BNamespaceCreator>	& newCreator	);
									
	virtual status_t	OnEndTag(				SString			& name			);
	
};


#if _SUPPORTS_NAMESPACE
} } // namespace palmos::xml
#endif // _SUPPORTS_NAMESPACE

#endif // _XML2_NAMESPACE_CREATOR_H
