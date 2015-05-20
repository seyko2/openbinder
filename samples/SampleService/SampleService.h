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

#ifndef SAMPLE_SERVICE_H
#define SAMPLE_SERVICE_H

#include <ISampleService.h>

#include <support/Locker.h>
#include <support/Package.h>

#if _SUPPORTS_NAMESPACE
using namespace vendor::sample;
#endif

class SampleService : public BnSampleService, public SPackageSptr
{
public:
									SampleService(	const SContext& context,
													const SValue &attr);

	// ISampleService
	virtual	int32_t					Test();

protected:
	virtual							~SampleService();
	virtual	void					InitAtom();

private:
	SLocker							m_lock;

	int32_t							m_count;
	bool							m_countSupplied;
};


#endif // HARD_INPUT_VIEW_H
