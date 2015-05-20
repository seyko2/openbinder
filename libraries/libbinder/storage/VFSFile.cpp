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

#include <support/Debug.h>
#include <support/Errors.h>
#include <support/String.h>
#include <support/SupportDefs.h>
#include <support/TypeConstants.h>
#include <support/StdIO.h>

#include <storage/VFSFile.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

// A couple macros for dealing with the larger (64 bit) BeOS parameters:
#define ClampToLongMax(n) ( (n > LONG_MAX) ? LONG_MAX : (int32_t)n)
#define ClampToULongMax(n) ( (n > ULONG_MAX) ? ULONG_MAX : (uint32_t)n)

#if BUILD_TYPE == BUILD_TYPE_DEBUG

#define checkpoint \
berr << "(" << this << ") -- " << __FILE__ << ":" << __LINE__ << endl;

#define errorpoint \
berr << "*** error *** " << __FILE__ << ":" << __LINE__ << endl;

#else
#define checkpoint
#define errorpoint
#endif


BVFSFile::BVFSFile()
	: m_initStatus(B_NO_INIT),
	  m_fd(-1),
	  m_flags(0),
	  m_pos(0)
{
}

BVFSFile::BVFSFile(const char* path, int flags, int mode, status_t * outResult)
	: m_initStatus(B_NO_INIT),
	  m_fd(-1),
	  m_flags(0),
	  m_pos(0)
{
	status_t result = SetTo(path, flags, mode);
	if (outResult != 0) *outResult = result;
}

BVFSFile::BVFSFile(const SUrl & url, int flags, int mode, status_t * outResult)
	: m_initStatus(B_NO_INIT),
	  m_fd(-1),
	  m_flags(0),
	  m_pos(0)
{
	status_t result = SetTo(url, flags, mode);
	if (outResult != 0) *outResult = result;
}

BVFSFile::~BVFSFile()
{
	if (m_fd >= 0)
	{
		close(m_fd);
	}
}

status_t
BVFSFile::InitCheck() const
{
	return m_initStatus;
}

status_t BVFSFile::SetTo(const char* path, int flags, int mode)
{
	status_t err;

	if (m_fd >= 0)
	{
		close(m_fd);
	}
	
	m_fd = open(path, flags, mode);
	if (m_fd < 0)
	{
errorpoint
		m_initStatus = (status_t)m_fd;
		return m_initStatus;
	}

	m_flags = flags;
	m_initStatus = B_OK;

	return B_OK;
}

status_t BVFSFile::SetTo(const SUrl & url, int flags, int mode)
{
	status_t err;

	// +++ we currently don't support wildcard "volume" matching -- this needs
	// to be revisited +++
	
	if (strcmp(url.GetScheme(), "file") != 0)
	{
		// not a file URL
		return B_UNSUPPORTED;
	}

	size_t pathsize = strlen(url.GetPath())+1;
	char * path = new char[pathsize];
	url.GetUnescapedPath(path, pathsize);
	err = SetTo(path, flags, mode);
	delete [] path;
	return err;
}

bool BVFSFile::IsReadable() const
{
	if (m_fd < 0)
		return false;
		
	return ((m_flags & O_ACCMODE) == O_RDONLY) || ((m_flags & O_ACCMODE) == O_RDWR);
}

bool BVFSFile::IsWritable() const
{
	if (m_fd < 0)
		return false;
		
	return ((m_flags & O_ACCMODE) == O_WRONLY) || ((m_flags & O_ACCMODE) == O_RDWR);
}

off_t BVFSFile::Size() const
{
	off_t cur = lseek(m_fd, 0, SEEK_CUR);
	if (cur < 0) return cur;

	off_t end = lseek(m_fd, 0, SEEK_END);
	if (end < 0) return end;

	(void) lseek(m_fd, cur, SEEK_SET);
	return end;
}

status_t BVFSFile::SetSize(off_t size)
{
	(void) size;
	assert (!"BVFSFile::SetSize unsupported");
	return B_UNSUPPORTED;
}

ssize_t BVFSFile::ReadAtV(off_t position, const struct iovec *vector, ssize_t count)
{
	status_t err;
	
	if (position != m_pos)
	{
		off_t seekResult = lseek(m_fd, position, SEEK_SET);
		if (seekResult < 0)
		{
			return (ssize_t)seekResult;
		}
		m_pos = position;
	}

	ssize_t readResult = readv(m_fd, vector, count);
	if (readResult < 0)
	{
		return readResult;
	}
	
	m_pos += readResult;
	return readResult;
}

ssize_t BVFSFile::WriteAtV(off_t position, const struct iovec *vector, ssize_t count)
{
	status_t err;

	if (position != m_pos)
	{
		off_t seekResult = lseek(m_fd, position, SEEK_SET);
		if (seekResult < 0)
		{
			return (ssize_t)seekResult;
		}
		m_pos = position;
	}

	ssize_t writeResult = writev(m_fd, vector, count);
	if (writeResult < 0)
	{
		return writeResult;
	}
	
	m_pos += writeResult;
	return writeResult;	
}


status_t BVFSFile::Sync()
{
	return B_UNSUPPORTED;
}

#if _SUPPORTS_NAMESPACE
} }
#endif
