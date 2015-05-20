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

// @(#)regexp.c	1.3 of 18 April 87
//
//	Copyright (c) 1986 by University of Toronto.
//	Written by Henry Spencer.  Not derived from licensed software.
//
//	Permission is granted to anyone to use this software for any
//	purpose on any computer system, and to redistribute it freely,
//	subject to the following restrictions:
//
//	1. The author is not responsible for the consequences of use of
//		this software, no matter how awful, even if they arise
//		from defects in it.
//
//	2. The origin of this software must not be misrepresented, either
//		by explicit claim or by omission.
//
//	3. Altered versions must be plainly marked as such, and must not
//		be misrepresented as being the original software.
//
// Beware that some of this code is subtly aware of the way operator
// precedence is structured in regular expressions.  Serious changes in
// regular-expression syntax might require a total rethink.
//

// ALTERED VERSION: Adapted to ANSI C and C++ for the OpenTracker
// project (www.opentracker.org), Jul 11, 2000.

// ALTERED VERSION: Modified to work as a Palm OS API, 2001-2004.

#ifndef _REG_EXB_H
#define _REG_EXB_H

/*!	@file support/RegExp.h
	@ingroup CoreSupportUtilities
	@brief Regular expression processing classes.
*/

#include <support/Debug.h>
#include <support/SupportDefs.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SString;

const int32_t kSubExpressionMax = 10;

struct regexp 
{
	const char *startp[kSubExpressionMax];
	const char *endp[kSubExpressionMax];
	char regstart;		/* Internal use only. See RegExp.cpp for details. */
	char reganch;		/* Internal use only. */
	const char *regmust;/* Internal use only. */
	int regmlen;		/* Internal use only. */
	char program[1];	/* Unwarranted chumminess with compiler. */
};

class SRegExp 
{

public:
	SRegExp();
	SRegExp(const char *);
	SRegExp(const SString &);
	~SRegExp();
	
	status_t InitCheck() const;
	
	status_t SetTo(const char*);
	status_t SetTo(const SString &);
	
	bool Matches(const char *string) const;
	bool Matches(const SString &) const;

	bool Search(const char *text, int32_t searchStart, int32_t *matchStart, int32_t *matchEnd);

	int32_t RunMatcher(regexp *, const char *) const;
	regexp *Compile(const char *);
	regexp *Expression() const;
	const char *ErrorString() const;

#if DEBUG
	void Dump();
#endif

private:

	void SetError(status_t error) const;

	// Working functions for Compile():
	char *Reg(int32_t, int32_t *);
	char *Branch(int32_t *);
	char *Piece(int32_t *);
	char *Atom(int32_t *);
	char *Node(char);
	char *Next(char *);
	const char *Next(const char *) const;
	void EmitChar(char);
	void Insert(char, char *);
	void Tail(char *, char *);
	void OpTail(char *, char *);

	// Working functions for RunMatcher():
	int32_t Try(regexp *, const char *) const;
	int32_t Match(const char *) const;
	int32_t Repeat(const char *) const;

	// Utility functions:
#if DEBUG
	char *Prop(const char *) const;
	void RegExpError(const char *) const;
#endif
	inline int32_t UCharAt(const char *p) const;
	inline char *Operand(char* p) const;
	inline const char *Operand(const char* p) const;
	inline bool	IsMult(char c) const;

// --------- Variables -------------

	mutable status_t fError;
	regexp *fRegExp;

	// Work variables for Compile().

	const char *fInputScanPointer;
	int32_t fParenthesisCount;		
	char fDummy;
	char *fCodeEmitPointer;		// &fDummy = don't.
	long fCodeSize;		

	// Work variables for RunMatcher().

	mutable const char *fStringInputPointer;
	mutable const char *fRegBol;	// Beginning of input, for ^ check.
	mutable const char **fStartPArrayPointer;
	mutable const char **fEndPArrayPointer;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} }
#endif

#endif
