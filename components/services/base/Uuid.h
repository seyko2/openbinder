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

#ifndef _UUID_H
#define _UUID_H

#include <support/IUuid.h>
#include <support/Package.h>

class BUuid : public BnUuid, private SPackageSptr
{
public:
	BUuid(const SContext& context);

	virtual void InitAtom();
	virtual SValue Create();

private:
	struct state
	{
		uint64_t timestamp;	/* saved timestamp */
		char node[6];		/* saved node ID */
		uint16_t clockseq;	/* saved clock sequence */
	};
	
	SLocker m_lock;
	state m_state;
	uint64_t m_nextSave;
};

#endif // _UUID_H
