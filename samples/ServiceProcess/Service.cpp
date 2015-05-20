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

#include "Service.h"

#include <support/StdIO.h>

//! The constructor for the service does basic initialization.
/*!	Note that it passes its SContext up to the BCatalog class, so later
	we can retrieve it by calling Context() (a method implemented in BBinder).

	One thing to be careful of -- do NOT do anything here that may acquire
	a reference on this object, since at this point it has no references.
	That kind of initialization should be done in InitAtom().  In general
	all initialization that doesn't involve simple member variable
	initialization is done in InitAtom().
*/
Service::Service(const SContext& context, const SValue &attr)
	: BCatalog(context)
{
}

//!	Destructor does nothing interesting.
Service::~Service()
{
}

//!	InitAtom() is called immediately after the first reference is acquired on the object
/*!	This method is usually called when the object is first assigned to
	a sptr<>.  At this point you can assume that no other threads can be
	accessing the object, since that would require they have their own
	references.
*/
void Service::InitAtom()
{
	BCatalog::InitAtom();
}
