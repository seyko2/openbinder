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
#include <support/String.h>

#include <support/StdIO.h>

#include <storage/File.h> 

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#if TARGET_HOST == TARGET_HOST_WIN32
#	define O_RWMASK       0x0003  /* Mask to get open mode */
#	define O_ACCMODE	  O_RWMASK
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

BFile::BFile()
{
	m_fd = -1;
	m_mode = 0;
}

BFile::BFile(const char *path, uint32_t open_mode)
{
	m_mode = 0;
	SetTo(path, open_mode);
}

BFile::BFile(const BFile &file)
	:	BnStorage()
{
	if (file.m_fd > -1) 
	{
		m_fd = dup(file.m_fd);
		if (m_fd >= 0) {
			fcntl(m_fd, F_SETFD, FD_CLOEXEC);
		} else {
			m_fd = -errno;
		}
		m_mode = file.m_mode;
	}
	else
	{
		m_fd = -1;
		m_mode = 0; 
	}
}

BFile::~BFile()
{
	if (m_fd > -1)
		close(m_fd);
}
		
status_t BFile::SetTo(const char *path, uint32_t open_mode)
{
	status_t status = B_OK;

	if (m_fd > -1)
		close(m_fd);

	m_fd = open(path, open_mode, 0666);
	if (m_fd >= 0) {
		fcntl(m_fd, F_SETFD, FD_CLOEXEC);
	} else {
		status = -errno;
		m_fd = status;
	}

	m_mode = open_mode;
	
	return status;
}

bool BFile::IsReadable() const
{
	if (m_fd <= -1)
		return false;
	return !((m_mode & O_ACCMODE) == O_WRONLY); 
}

bool BFile::IsWritable() const
{
	if (m_fd <= -1)
		return false;
	return !((m_mode & O_ACCMODE) == O_RDONLY);  
}

off_t BFile::Size() const
{
	if (m_fd < 0) return m_fd;
	
	struct stat64 st;
	int res = fstat64(m_fd, &st);
	return res >= 0 ? st.st_size : -errno;
}

status_t BFile::SetSize(off_t size)
{
	if (m_fd < 0) return m_fd;

	int res = ftruncate64(m_fd, size);
	return res >= 0 ? B_OK : -errno;
}

ssize_t BFile::ReadAtV(off_t position, const struct iovec *vector, ssize_t count)
{
	if (m_fd < 0) return m_fd;
	
	ssize_t total;
	if (count == 1) {
		// Common case.
		total = pread64(m_fd, vector->iov_base, vector->iov_len, position);
		return total >= 0 ? total : -errno;
	}
	
	// XXX This is not quite right, because we should be reading the iovec atomically.
	total=0;
	while (count > 0) {
		ssize_t amt = pread64(m_fd, vector->iov_base, vector->iov_len, position);
		if (amt < vector->iov_len) {
			if (amt >= 0) return total + amt;
			if (total > 0) return total;
			return -errno;
		}
		total += amt;
		position += amt;
		vector++;
		count--;
	}
	
	return total;
}

ssize_t BFile::WriteAtV(off_t position, const struct iovec *vector, ssize_t count)
{
	if (m_fd < 0) return m_fd;
	
	ssize_t total;
	if (count == 1) {
		// Common case.
		total = pwrite64(m_fd, vector->iov_base, vector->iov_len, position);
		return total >= 0 ? total : -errno;
	}
	
	// XXX This is not quite right, because we should be writing the iovec atomically.
	total=0;
	while (count > 0) {
		ssize_t amt = pwrite64(m_fd, vector->iov_base, vector->iov_len, position);
		if (amt < vector->iov_len) {
			if (amt >= 0) return total + amt;
			if (total > 0) return total;
			return -errno;
		}
		total += amt;
		position += amt;
		vector++;
		count--;
	}
	
	return total;
}

status_t BFile::Sync()
{
	if (m_fd >= 0) fsync(m_fd);
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::storage
#endif
