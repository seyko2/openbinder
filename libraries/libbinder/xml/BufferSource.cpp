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

#include <string.h>		// strlen()

#include <xml/DataSource.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif

// =====================================================================
BXMLBufferSource::BXMLBufferSource(const char * buffer, int32_t len)
	:_data(buffer),
	 _length(len)
{
	if (_length < 0 && buffer)
		_length = strlen(buffer);
	else
		_length = len;
	// Nothing
}


// =====================================================================
BXMLBufferSource::~BXMLBufferSource()
{
	// Nothing
}


// =====================================================================
status_t
BXMLBufferSource::GetNextBuffer(size_t * size, uint8_t ** data, int * done)
{
	if (!_data || _length < 0)
		return B_NO_INIT;
	*size = _length;
	*data = (uint8_t *) _data;
	*done = 1;
	return B_OK;
}


#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif
