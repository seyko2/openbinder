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

#include "SampleService.h"

#include <support/StdIO.h>

/*	Path in context at which count is stored to persist across resets (and sync).

	Change the "vendor/sample" and remaining part to whatever you want -- for example,
	"/settings/apps/com/palmone/sample/test".

	Note that you don't have to use this approach for storing persistent
	settings -- you can also use the traditional PrefMgr APIs -- but this
	can be more convenient in some cases, and it is easy to view and modify
	these settings through the shell (for example "ls -l /settings/apps/vendor").
*/
B_STATIC_STRING_VALUE_LARGE(kCountPath, "/settings/apps/org.openbinder/sample", );

//	This is an argument that can be supplied to specify the starting value.
B_STATIC_STRING_VALUE_LARGE(kStart, "start", );

// ----------------------------------------------------------------------------

/*	The constructor for the service does basic initialization.

	Note that it passes its SContext up to the BnSampleService class, so later
	we can retrieve it by calling Context() (a method implemented in BBinder).

	One thing to be careful of -- do NOT do anything here that may acquire
	a reference on this object, since at this point it has no references.
	That kind of initialization should be done in InitAtom().  In general
	all initialization that doesn't involve simple member variable
	initialization is done in InitAtom().
*/
SampleService::SampleService(const SContext& context, const SValue &args)
	: BnSampleService(context)
	, m_count(0)
	, m_countSupplied(false)
{
	// Retrieve the starting value, if specified.
	status_t err;
	int32_t start = args[kStart].AsInt32(&err);
	if (err == B_OK) {
		m_count = start;
		m_countSupplied = true;
	}
}

//	Destructor does nothing interesting.
SampleService::~SampleService()
{
}

/*	InitAtom() is called immediately after the first reference is acquired on the object

	This method is usually called when the object is first assigned to
	a sptr<>.  At this point you can assume that no other threads can be
	accessing the object, since that would require they have their own
	references.
*/
void SampleService::InitAtom()
{
	BnSampleService::InitAtom();

	// If starting count was not supplied, retrieve saved value out of
	// settings; start at 10 if there is no setting.
	if (!m_countSupplied) {
		status_t err;
		m_count = Context().Lookup(kCountPath).AsInt32(&err);
		if (err != B_OK) m_count = 10;
	}
}

/*	Atomically increment the current value, save and return it.
	The new value is saved at kCountPath
*/
int32_t SampleService::Test()
{
	// Make sure to protect from multiple threads.
	SLocker::Autolock _l(m_lock);

	// Increment current count and save the new value.
	m_count++;
	Context().Publish(kCountPath, SValue::Int32(m_count));

	// Return new value.
	return m_count;
}
