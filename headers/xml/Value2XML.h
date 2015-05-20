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

#ifndef _VALUE_TO_XML_H

#define _VALUE_TO_XML_H

#include <support/IByteStream.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif

status_t ValueToXML (const BNS(::palmos::support::)sptr<BNS(palmos::support::) IByteOutput>& stream,
					const BNS(palmos::support::) SValue &value);

status_t XMLToValue (const BNS(::palmos::support::)sptr<BNS(palmos::support::) IByteInput>& stream,
						BNS(palmos::support::) SValue &value);

#if _SUPPORTS_NAMESPACE
} } // palmos::xml
#endif
					
#endif
