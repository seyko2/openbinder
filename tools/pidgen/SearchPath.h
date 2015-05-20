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

#ifndef SEARCHPATH_H
#define SEARCHPATH_H

#include <support/String.h>
#include <support/Vector.h>

using namespace palmos::support;

class SearchPath
{
public:
	/*!
		Add a path to the search path.  If it's an absolute
		path, great.  If it's relative, it's relative to the
		current working directory.
	*/
	void		AddPath(const SString &path);
	
	/*!
		Find file, inside the search path, return
		the value in path.  Absolute paths just return the
		path and B_OK;
	*/
	/*status_t	FindFullPath(const SString &file, SString &path);*/
	
private:
	SVector<SString> m_paths;
};

extern SearchPath gSearchPath;

#endif // SEARCHPATH_H
