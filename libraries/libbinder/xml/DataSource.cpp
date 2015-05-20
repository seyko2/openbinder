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

#include <xml/DataSource.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif


// =====================================================================
BXMLDataSource::~BXMLDataSource()
{
}


// =====================================================================
BXMLfstreamSource::BXMLfstreamSource(FILE *file)
	:m_file(file)
{
}

BXMLfstreamSource::~BXMLfstreamSource()
{
}

status_t
BXMLfstreamSource::GetNextBuffer(size_t *size, uint8_t **data, int *done)
{
	if (m_file == NULL) return B_NO_INIT;
	*size = fread(*data, 1, *size, m_file);
	*done = feof(m_file);
	return ferror(m_file);
}


#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif
