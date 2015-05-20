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

//#define DEBUG 1
#include <support/KeyID.h>
#include <support/Debug.h>
#if DEBUG
#include <support/StdIO.h>
#endif

BNS(namespace palmos {)
BNS(namespace support {)

SKeyID::SKeyID(KeyID id, FreeKey* cleanup)
	: SAtom(), m_key(id), m_cleanup(cleanup)
{
#if DEBUG
	bout << SPrintf("SKeyID(%8lx)", m_key) << endl;
#endif
}

SKeyID::~SKeyID()
{
#if DEBUG
	bout << SPrintf("~SKeyID(%8lx)", m_key) << endl;
#endif
	if (m_cleanup)
	{
		(*m_cleanup)(m_key);
		delete m_cleanup;
	}
}

KeyID
SKeyID::AsKeyID()
{
	return m_key;
}

SKeyID::FreeKey::FreeKey()
{
}

SKeyID::FreeKey::~FreeKey()
{
}

BNS(} }) // palmos::support
