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

#include <support/KernelStreams.h>

#include <unistd.h>
#include <stdio.h>

#if TARGET_HOST == TARGET_HOST_WIN32
#include <support_p/WindowsCompatibility.h>
extern "C" {
_CRTIMP int __cdecl setmode(int, int);
}
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*-------------------------------------------------------------*/

BKernelOStr::BKernelOStr(int32_t descriptor) : m_descriptor(descriptor)
{
#if TARGET_HOST == TARGET_HOST_WIN32
	setmode(descriptor, O_BINARY);
#endif
}

BKernelOStr::~BKernelOStr()
{
	if (m_descriptor >= 0) close(m_descriptor);
	m_descriptor = B_NO_INIT;
}

ssize_t 
BKernelOStr::WriteV(const struct iovec *vector, ssize_t count, uint32_t flags)
{
	if (m_descriptor >= 0) {
		ssize_t amt;
		if (count == 1) amt = write(m_descriptor,vector->iov_base,vector->iov_len);
		else if (count > 1) amt = writev(m_descriptor,vector,count);
		else if (count < 0) return count;
		else amt = 0;
		if (amt < 0) return -errno;
		if (!(flags&B_WRITE_END)) return amt;
		
		// Need to figure out if we wrote everything, so the stream
		// can be ended.  XXX This flag is actually supposed to
		// just truncate the file to this size, so I think we shouldn't
		// be closing it but for a raw stream like this just ignoring it.
		ssize_t tot = 0;
		if (count > 0) {
			tot = vector->iov_len;
			while (--count > 0) {
				tot += vector[count].iov_len;
			}
		}
		if (amt == tot) {
			if (m_descriptor >= 0) close(m_descriptor);
			m_descriptor = B_NO_INIT;
		}
		return amt;
	} else
		return m_descriptor;
}

status_t 
BKernelOStr::Sync()
{
	if (m_descriptor >= 0)
		return fsync(m_descriptor);
	else
		return m_descriptor;
}

BKernelIStr::BKernelIStr(int32_t descriptor) : m_descriptor(descriptor)
{
#if TARGET_HOST == TARGET_HOST_WIN32
	setmode(descriptor, O_BINARY);
#endif
}

BKernelIStr::~BKernelIStr()
{
	if (m_descriptor >= 0) close(m_descriptor);
	m_descriptor = B_NO_INIT;
}

ssize_t 
BKernelIStr::ReadV(const struct iovec *vector, ssize_t count, uint32_t /*flags*/)
{
	if (m_descriptor >= 0) {
		ssize_t amt;
		do {
			if (count == 1) amt = read(m_descriptor,vector->iov_base,vector->iov_len);
			else if (count > 1) amt = readv(m_descriptor,vector,count);
			else if (count < 0) return count;
			else return 0;
			if (amt < 0) amt = -errno;
		} while (amt == -EINTR);
		//printf("*** Retrieved %ld bytes from file descriptor %ld!\n", amt, m_descriptor);
		return amt;
	} else
		return m_descriptor;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
