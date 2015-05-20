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

#include <support/Pipe.h>
#include <support/Errors.h>
#include <support/Autolock.h>
#include <support/Debug.h>

#include <support_p/SupportMisc.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

BPipe::BPipe (size_t size)
	: mSize(size),
	  mBuffer(malloc(mSize)),
	  mReadOffset(0),
	  mWriteOffset(0),
	  mSpaceAvailable(mSize),
	  mPipeHalfClosed(false),
	  mReadSerializer("BPipe ReadSerializer"),
	  mWriteSerializer("BPipe WriteSerializer"),
	  mSpaceAvailableCV("BPipe SpaceAvailable"),
	  mDataAvailableCV("BPipe DataAvailable")
{
}

BPipe::~BPipe()
{
	free(mBuffer);
}

ssize_t
BPipe::PrvWrite (const void *data, size_t size, uint32_t flags)
{
	if (mPipeHalfClosed)
		return B_BROKEN_PIPE;

	if (size == 0)
		return 0;

	while (mSpaceAvailable == 0)
	{
		if (flags&B_DO_NOT_BLOCK) return B_WOULD_BLOCK;
		mSpaceAvailableCV.Wait();
	}
	mSpaceAvailableCV.Close();

	size_t space_avail = mSpaceAvailable;

	if (size > space_avail)
		size = space_avail;

	if (size <= mSize-mWriteOffset)
	{
		memcpy((char *)mBuffer + mWriteOffset,data,size);
		
		mWriteOffset += size;

		if (mWriteOffset == mSize)
			mWriteOffset = 0;
	}
	else
	{
		const size_t chunk = mSize-mWriteOffset;

		// size > chunk guaranteed
		memcpy((char *)mBuffer + mWriteOffset,data,chunk);
		memcpy(mBuffer,(const char *)data + chunk,size-chunk);

		mWriteOffset = size-chunk;
	}

	if (g_threadDirectFuncs.atomicAdd32(&mSpaceAvailable,-(int32_t)size) == mSize)
	{
		// buffer used to be empty
		mDataAvailableCV.Open();
	}

	return size;
}

status_t
BPipe::PrvWriteEOS()
{
	if (mPipeHalfClosed)
		return B_BROKEN_PIPE;

	mPipeHalfClosed = true;

	if (mSpaceAvailable == mSize)
		mDataAvailableCV.Open();		// unblock readers

	return B_OK;
}

ssize_t
BPipe::PrvRead (void *data, size_t size, uint32_t flags)
{
	if (size == 0)
		return 0;

	if (mSpaceAvailable == mSize)
	{
		if (flags&B_DO_NOT_BLOCK) return B_WOULD_BLOCK;
		mDataAvailableCV.Wait();
	}
	mDataAvailableCV.Close();

	if (mSpaceAvailable == mSize)
	{
		ASSERT(mPipeHalfClosed);

		mDataAvailableCV.Open();

		return 0;
	}

	size_t data_avail = mSize-mSpaceAvailable;

	if (size > data_avail)
		size = data_avail;

	if (size <= mSize-mReadOffset)
	{
		memcpy(data,(const char *)mBuffer + mReadOffset,size);

		mReadOffset += size;

		if (mReadOffset == mSize)
			mReadOffset = 0;
	}
	else
	{
		const size_t chunk = mSize-mReadOffset;

		// size > chunk guaranteed
		
		memcpy(data,(const char *)mBuffer + mReadOffset,chunk);
		memcpy((char *)data + chunk,mBuffer,size-chunk);

		mReadOffset = size-chunk;
	}

	if (g_threadDirectFuncs.atomicAdd32(&mSpaceAvailable,size) == 0)
	{
		// buffer used to be full
		mSpaceAvailableCV.Open();
	}

	return size;
}

ssize_t 
BPipe::WriteV (const struct iovec *iov, ssize_t count, uint32_t flags)
{
	if (count < 0) return count;

	SAutolock autoLock(mWriteSerializer.Lock());

	size_t total = 0;

	// If blocking, write ALL requested bytes.
	// Otherwise, write until we will block.
	while (count--)
	{
		const char* addr = (const char*)iov->iov_base;
		ssize_t len = iov->iov_len;
		total += len;

		while (len > 0)
		{
			ssize_t result = PrvWrite(addr,len,flags);

			if (result < B_OK)
				return total > 0 ? total : result;

			addr += result;
			len -= result;
		}

		++iov;
	}

	if (flags & B_WRITE_END)
		PrvWriteEOS();

	return total;
}

ssize_t 
BPipe::ReadV (const struct iovec *iov, ssize_t count, uint32_t flags)
{
	if (count < 0) return count;

	SAutolock autoLock(mReadSerializer.Lock());

	size_t total = 0;

	// If blocking, read only what is available (up
	// to the requested amount).  Only block if no
	// data is currently available.
	// If non-blocking, don't block if no data is
	// available.
	while (count--)
	{
		ssize_t result = PrvRead(iov->iov_base,iov->iov_len,flags);

		if (result < B_OK)
			return total > 0 ? total : result;

		total += (size_t)result;

		if ((size_t)result < iov->iov_len)
			break;

		++iov;
	}

	return total;
}

status_t 
BPipe::Sync()
{
	return B_OK;
}

SValue	
BPipe::Inspect (const sptr<IBinder>& caller, const SValue &which, uint32_t flags)
{
	return BnByteInput::Inspect(caller,which,flags)
			+ BnByteOutput::Inspect(caller,which,flags);
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
