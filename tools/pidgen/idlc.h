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

#ifndef J_IDLC_H
#define J_IDLC_H

#include <stdio.h>

#include <storage/File.h>
#include <support/KeyedVector.h>
#include <support/SortedVector.h>
#include <support/StdIO.h>
#include <support/String.h>
#include <support/SupportDefs.h>
#include <support/ByteStream.h>
#include <support/TextStream.h>
#include <support/TypeConstants.h>
#include <support/Vector.h>
#include <unistd.h>

// Temporary -- SMessage should not be a builtin type.
enum {
	B_MESSAGE_TYPE = 100
};

//#define SYMDEBUG 1		//debugs symbol & symbolstack
//#define TYPEDEBUG 1	//debugs type engine - outpututil.cpp, typeparser.cpp, interfacerec.h&cpp
//#define IDLDEBUG 1 // debug parser (yacc.y)
//#define CPPDEBUG 1 // debug cpp output
//#define STRUCTDEBUG 1

// categories for IDLTypes - default is undefined, 
// S=standard types, B=B Prefixed types, U=user defined types

int yyerror(char *errmsg);
extern "C" int  yyparse(void *voidref);



#endif //J_IDLC_H
