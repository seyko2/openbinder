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

#include <support/IOSStream.h>

#if TARGET_HOST == TARGET_HOST_PALMOS

#include <IOS.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*-------------------------------------------------------------*/

BIOSStream::BIOSStream(int32_t descriptor, uint32_t flags)
	:	m_descriptor(descriptor), m_flags(flags)
{
}

BIOSStream::~BIOSStream()
{
	if ((m_flags&B_IOSSTREAM_NO_AUTO_CLOSE) == 0) {
		if (m_descriptor >= 0) IOSClose(m_descriptor);
	}
	m_descriptor = iosErrBadFD;
}

SValue BIOSStream::Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags)
{
	SValue result;
	if ((m_flags*B_IOSSTREAM_HIDE_OUTPUT) == 0)
		result.Join(BnByteOutput::Inspect(caller, which, flags));
	if ((m_flags*B_IOSSTREAM_HIDE_INPUT) == 0)
		result.Join(BnByteInput::Inspect(caller, which, flags));
	return result;
}

ssize_t 
BIOSStream::WriteV(const struct iovec *vector, ssize_t count, uint32_t flags)
{
	if (m_descriptor >= 0) {
		ssize_t amt;
		status_t err = errNone;

		if (count == 1) amt = IOSWrite(m_descriptor,vector->iov_base,vector->iov_len,&err);
		else if (count > 1) amt = IOSWritev(m_descriptor,vector,count,&err);
		else amt = 0;

		if (err != errNone) return err;

		//printf("*** Wrote %ld of %ld bytes from file descriptor %ld!\n", amt, vector->iov_len, m_descriptor);
		if (amt >= 0 && (flags&B_WRITE_END) != 0) {
			ssize_t tot;
			if (count > 0) {
				tot = vector->iov_len;
				while (--count > 0) {
					tot += vector[count].iov_len;
				}
			} else {
				tot = count;
			}
			if (amt == tot) {
				if ((m_flags&B_IOSSTREAM_NO_AUTO_CLOSE) == 0) {
					if (m_descriptor >= 0) IOSClose(m_descriptor);
					// XXX Need to trunctate?
				}
				m_descriptor = iosErrBadFD;
			}
		}
		return amt;
	} else
		return m_descriptor;
}

status_t 
BIOSStream::Sync()
{
	// Doesn't seem to be an IOS API for this?
	if (m_descriptor >= 0)
		return B_OK; //fsync(m_descriptor);
	else
		return m_descriptor;
}

ssize_t 
BIOSStream::ReadV(const struct iovec *vector, ssize_t count, uint32_t /*flags*/)
{
	if (m_descriptor >= 0) {
		ssize_t amt;
		status_t err = errNone;

		if (count == 1) amt = IOSRead(m_descriptor,vector->iov_base,vector->iov_len,&err);
		else if (count > 1) amt = IOSReadv(m_descriptor,vector,count,&err);
		else amt = 0;

		//printf("*** Retrieved %ld bytes from file descriptor %ld!\n", amt, m_descriptor);
		return err == errNone ? amt : (ssize_t)err;
	} else
		return m_descriptor;
}

int32_t
BIOSStream::FileDescriptor() const
{
	return m_descriptor;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif // TARGET_HOST != TARGET_HOST_LINUX
