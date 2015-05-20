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

#ifndef _B_XML2_DATA_SOURCE_H
#define _B_XML2_DATA_SOURCE_H

#include <support/IByteStream.h>
#include <support/SupportDefs.h>
#include <stdio.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace support;
#endif

// BXMLDataSource
// =====================================================================
// Allows you to get some data to parse with minimal copying
class BXMLDataSource
{
public:
	// GetNextBuffer is the same idea as DataIO, except it doesn't require
	// a data copy.  It will be provided with a buffer in *data, so if you
	// have to copy data, then copy into that, but if you have your own
	// buffer, then just replace *data with your pointer.
	// You should return how much data there is in size, and
	// if this is the last iteration, then return a non-zero value in done.
	virtual status_t	GetNextBuffer(size_t * size, uint8_t ** data, int * done) = 0;
	virtual 			~BXMLDataSource();
};



// BXMLIByteInputSource
// =====================================================================
// Subclass of BBXMLDataSource that uses an IByteInput to get the data
class BXMLIByteInputSource : public BXMLDataSource
{
public:
						BXMLIByteInputSource(const sptr<IByteInput>& data);
	virtual				~BXMLIByteInputSource();
	virtual status_t	GetNextBuffer(size_t * size, uint8_t ** data, int * done);

private:

	sptr<IByteInput>		_data;
};



// BXMLBufferSource
// =====================================================================
// Subclass of BBXMLDataSource that uses a buffer you give it to get the data
class BXMLBufferSource : public BXMLDataSource
{
public:
						// If len < 0, it is null terminated, so do strlen
						BXMLBufferSource(const char * buffer, int32_t len = -1);
	virtual				~BXMLBufferSource();
	virtual status_t	GetNextBuffer(size_t * size, uint8_t ** data, int * done);

private:
						// Illegal
						BXMLBufferSource();
						BXMLBufferSource(const BXMLBufferSource & copy);
	const char	* _data;
	int32_t		_length;
};


// BXMLfstreamSource
// =====================================================================
// Subclass of BBXMLDataSource that uses a buffer you give it to get the data
class BXMLfstreamSource : public BXMLDataSource
{
public:
						// If len < 0, it is null terminated, so do strlen
						BXMLfstreamSource(FILE *file);
	virtual				~BXMLfstreamSource();
	virtual status_t	GetNextBuffer(size_t * size, uint8_t ** data, int * done);

private:
						// Illegal
						BXMLfstreamSource();
						BXMLfstreamSource(const BXMLBufferSource & copy);
	FILE *m_file;
};



#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif

#endif // _B_XML2_DATA_SOURCE_H
