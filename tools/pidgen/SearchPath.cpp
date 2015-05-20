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

#include "SearchPath.h"

//#include <storage/Entry.h> - it's not in storage2 now

using namespace palmos::storage;

SearchPath gSearchPath;

void
SearchPath::AddPath(const SString &path)
{
	m_paths.AddItem(path);
}

/*
status_t
SearchPath::FindFullPath(const SString &file, SString &path)
{
	if (file.Length() <= 0) return B_BAD_VALUE;
	
	// If absolute
	if (file.ByteAt(0) == '/') {
		path = file;
		return B_OK;
	}
	
	BEntry inCWD(file.String());
	if (inCWD.Exists()) {
		SString yo;
		inCWD.GetPath(&yo);
		path = yo.String();
		return B_OK;
	}
	uint32_t i, count = m_paths.CountItems();
	for (i=0; i<count; i++) {
		uint32_t index = count-1-i;
		SString test;
		if (test.PathSetTo(m_paths[index].String(), file.String()) != B_OK) continue;
		BEntry entry(test.String());
		if (entry.Exists()) {
			path = test.String();
			return B_OK;
		}
	}
	
	return B_FILE_NOT_FOUND;
}
*/
