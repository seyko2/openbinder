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
#include <support/IByteStream.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif

// =====================================================================
BXMLIByteInputSource::BXMLIByteInputSource(const sptr<IByteInput>& data)
	:_data(data)
{
	// Nothing
}


// =====================================================================
BXMLIByteInputSource::~BXMLIByteInputSource()
{
	// Nothing
}


// =====================================================================
status_t
BXMLIByteInputSource::GetNextBuffer(size_t * size, uint8_t ** data, int * done)
{
	if (_data == NULL)
		return B_NO_INIT;
	ssize_t bufSize = *size;
	bufSize = _data->Read(*data, bufSize);
	if (bufSize < 0)
		return bufSize;						// Error
	*done = ((size_t) bufSize) < *size;
	*size = bufSize;
	return B_OK;
}


#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif
