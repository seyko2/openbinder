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

#include <support/NullStreams.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

BNullStream::BNullStream() { }
BNullStream::~BNullStream() { }

SValue BNullStream::Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags)
{
	return BnByteOutput::Inspect(caller, which, flags)
		.Join(BnByteInput::Inspect(caller, which, flags));
}

ssize_t BNullStream::WriteV(const struct iovec *vector, ssize_t count, uint32_t flags)
{
	// "Consume" all data.
	ssize_t amount = 0;
	while (count > 0) {
		amount += vector->iov_len;
		count--;
		vector++;
	}
	return amount;
}

status_t BNullStream::Sync()
{
	return B_OK;
}

ssize_t BNullStream::ReadV(const struct iovec * /*vector*/, ssize_t /*count*/, uint32_t /*flags*/)
{
	return 0;
}

BNullTextOutput::BNullTextOutput() { }
BNullTextOutput::~BNullTextOutput() { }

status_t
BNullTextOutput::Print(const char *debugText, ssize_t len)
{
	(void) debugText;
	(void) len;
	return B_OK;
}

void
BNullTextOutput::MoveIndent(int32_t delta)
{
	(void) delta;
}

status_t
BNullTextOutput::LogV(const log_info& info, const iovec *vector, ssize_t count, uint32_t flags)
{
	(void) info;
	(void) vector;
	(void) count;
	(void) flags;
	return B_OK;
}

void
BNullTextOutput::Flush()
{
}

status_t
BNullTextOutput::Sync()
{
	return B_OK;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
