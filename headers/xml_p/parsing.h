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

#ifndef _PARSING_P_H
#define _PARSING_P_H

#include <xml/Parser.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif

// extern palmos::xml::BXMLObjectFactory be_default_xml_factory;

// This function does the parsing
// All the public xml parsing functinons, as well as some of the stuff in the entity handling
// uses this directly.
// Defined ParseXML.cpp
status_t _do_the_parsing_yo_(BXMLDataSource * data, BXMLParseContext * context, bool dtdOnly, uint32_t flags);

#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif

#endif // _PARSING_P_H
