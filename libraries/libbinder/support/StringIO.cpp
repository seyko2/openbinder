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

#include <support/StringIO.h>

#include <support/Debug.h>
#include <support/String.h>

#include <string.h>
#include <unistd.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// ----------------------------------------------------------------- //

BStringIO::BStringIO() : BTextOutput(this)
{
}

// ----------------------------------------------------------------- //

BStringIO::~BStringIO()
{
}

// ----------------------------------------------------------------- //

const char * BStringIO::String()
{
	if (!Buffer()) return "";
	off_t size = Size();
	if (size <= 0x7fffffff && !AssertSpace(static_cast<size_t>(size+1))) {
		((char*)Buffer())[Size()] = 0;
	}
	return (const char*)Buffer();
}

// ----------------------------------------------------------------- //

size_t BStringIO::StringLength() const
{
	return (size_t)Size();
}

// ----------------------------------------------------------------- //

void BStringIO::Clear(off_t to)
{ 
	off_t pos = Position();

	if ((to + pos >= Size()))
	{
		WriteV(NULL, 0, B_WRITE_END);
	}
	else
	{
		char* copy = pos + to + (char*)Buffer();
		off_t size = ((const char*)Buffer() + Size()) - copy;
	
		//do the copy
		Write(copy, (size_t)size, 0); 
		// null out thr rest.
		WriteV(NULL, 0, B_WRITE_END);
		// reset our position to the position before the clear.
		Seek(pos, SEEK_SET);
	}
}

// ----------------------------------------------------------------- //

void BStringIO::Reset()
{
	Seek(0,SEEK_SET);
	WriteV(NULL, 0, B_WRITE_END);
}

// ----------------------------------------------------------------- //

void BStringIO::PrintAndReset(const sptr<ITextOutput>& io)
{
	io << String();
	Reset();
}

// ----------------------------------------------------------------- //

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
