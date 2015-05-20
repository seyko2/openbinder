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

#ifndef OUTPUT_I_H
#define OUTPUT_I_H

#include "InterfaceRec.h"

#include <support/ITextStream.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

/*!
	any non alpha filename chars are turned to _
*/
status_t WriteIHeader(sptr<ITextOutput> stream, SVector<InterfaceRec *> &recs,
				const SString &filename, SVector<IncludeRec> & headers,
				const sptr<IDLCommentBlock>& beginComments,
				const sptr<IDLCommentBlock>& endComments,
				bool system);

#endif // OUTPUT_I_H
