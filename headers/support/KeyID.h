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

#ifndef _SUPPORT_KEYID_H
#define _SUPPORT_KEYID_H

/*!	@file support/KeyID.h
	@ingroup CoreSupportBinder
	@brief Class representing a KeyID, used to marshall and unmarshall KeyIDs
	across Binder interfaces and to send through IPC.
*/

// XXX This is old code from PalmOS, it should be removed.
// (We would like the same kind of functionality with file descriptors, though...)

#include <support/Atom.h>

typedef uint32_t KeyID;

BNS(namespace palmos {)
BNS(namespace support {)

/*!	@addtogroup CoreSupportBinder
	@{
*/

//!	Class representing a kernel KeyID.
class SKeyID : virtual public SAtom {
public:

	class FreeKey {
		public:
					FreeKey();
			virtual	~FreeKey();
			virtual	status_t	operator()(KeyID key) = 0;
	};
					SKeyID(KeyID id, FreeKey* cleanup);
	KeyID			AsKeyID();
protected:
	virtual			~SKeyID();

private:
					SKeyID(const SKeyID&);
	KeyID			m_key;
	FreeKey			*m_cleanup;
};

/*!	@} */

BNS(} })

#endif
