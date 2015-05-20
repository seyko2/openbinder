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

#ifndef _SUPPORT_PIPE_H
#define _SUPPORT_PIPE_H

/*!	@file support/Pipe.h
	@ingroup CoreSupportDataModel
	@brief Pipe-like implementation of byte input and output streams.
*/

#include <support/ByteStream.h>
#include <support/Locker.h>
#include <support/ConditionVariable.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

//!	Pipe-like implementation of byte input and output streams.
class BPipe : public BnByteInput, public BnByteOutput
{
	BPipe (const BPipe &);
	BPipe &operator= (const BPipe &);

	size_t mSize;
	void *mBuffer;

	size_t mReadOffset,mWriteOffset;
	volatile int32_t mSpaceAvailable;

	volatile bool mPipeHalfClosed;

	SLocker mReadSerializer,mWriteSerializer;
	SConditionVariable mSpaceAvailableCV,mDataAvailableCV;

	protected:
		virtual ~BPipe();

		ssize_t PrvWrite (const void *data, size_t size, uint32_t flags);
		status_t PrvWriteEOS();

		ssize_t PrvRead (void *data, size_t size, uint32_t flags);

	public:

		BPipe (size_t size);
				
		virtual	ssize_t	ReadV (const struct iovec *vector, 
								ssize_t count,
								uint32_t flags = 0);

		virtual	ssize_t	WriteV (const struct iovec *vector, 
								ssize_t count, 
								uint32_t flags = 0);

		virtual	status_t Sync();

		virtual SValue Inspect (const sptr<IBinder>& caller,
								const SValue &which, 
								uint32_t flags = 0);
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif // _SUPPORT_PIPE_H
