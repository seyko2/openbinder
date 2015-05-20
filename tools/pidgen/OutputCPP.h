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

#ifndef OUTPUTCPP_H
#define OUTPUTCPP_H

#include "InterfaceRec.h"

#include <support/ITextStream.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

status_t WriteCPP(sptr<ITextOutput> stream, SVector<InterfaceRec *> &recs,
				const SString & filename,
				const SString & lHeader,
				bool systemHeader);



#endif // OUTPUTCPP_H
