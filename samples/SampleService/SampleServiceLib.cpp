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

#include "SampleServiceLib.h"

#include <ISampleService.h>

#include <support/Locker.h>

#include <ErrorMgr.h>

#if _SUPPORTS_NAMESPACE
using namespace org::openbinder::samples;
#endif


// Privates
// ==========================================================
static sptr<ISampleService>	g_cachedService;
static SLocker				g_cachedServiceLock;

static sptr<ISampleService> get_sample_service()
{
	SLocker::Autolock _l(g_cachedServiceLock);

	if (g_cachedService != NULL) return g_cachedService;

	// First time -- look up the service and cache a local
	// pointer to it.  This is a performance optimization to avoid
	// doing an IPC to whatever process the service may be
	// hosted in.
	SContext context = SContext::UserContext();
	g_cachedService = ISampleService::AsInterface(context.LookupService(SString("sample_service")));
	
	return g_cachedService;
}

// Test sample service
// ==========================================================
int32_t SampleServiceTest()
{
	// Retrieve the service.
	const sptr<ISampleService> sampleService = get_sample_service();
	DbgOnlyFatalErrorIf(sampleService == NULL, "Unable to find sample_service!");

	// You may or may not care about failing gracefully in this case (of
	// the service not being found).
	if (sampleService == NULL) return 0;
	
	// Call the corresponding method on the service, and return its
	// result.  Note that if the service is hosted in another process,
	// you will actually be calling through a proxy implementation of
	// ISampleService that does an IPC to the other process for you.
	return sampleService->Test();
}
