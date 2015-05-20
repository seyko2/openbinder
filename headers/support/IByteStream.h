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

#ifndef	_SUPPORT_BYTESTREAM_INTERFACE_H
#define	_SUPPORT_BYTESTREAM_INTERFACE_H

/*!	@file support/IByteStream.h
	@ingroup CoreSupportDataModel
	@brief Raw byte stream (read, write, seek) interfaces.
*/

#include <support/IInterface.h>
#include <sys/uio.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

//! Raw byte input and output transaction codes.
enum {
	B_WRITE_TRANSACTION			= 'WRTE',
	B_WRITE_FLAGS_TRANSACTION	= 'WRFL',
	B_READ_TRANSACTION			= 'READ'
};


//! Flags for IByteOutput::Write() and IByteInput::Read().
enum {
	//! Perform a non-blocking IO.
	/*! If the read or write operation would block, the
		function instead returns with the number of bytes
		processed before having to block or B_WOULD_BLOCK
		if nothing was processed.
	*/
	B_DO_NOT_BLOCK				= 0x00000001
};

//! Flags for IByteOutput::Write().
enum {
	//!	Mark the final location as the end of the stream.
	/*!	Any data currently after this location will be removed;
		further writes will extend the stream from here.  This
		flag will NOT be used if the write operation does not
		include all supplied bytes.  (I.e., it returns fewer
		bytes than the caller supplied or an error code.)
	*/
	B_WRITE_END					= 0x00010000
};

/*-------------------------------------------------------------*/
/*------- IByteInput Interface --------------------------------*/

//!	Abstract byte stream input (read) interface.
class IByteInput : public IInterface
{
	public:

		B_DECLARE_META_INTERFACE(ByteInput)

				//!	Read the bytes described by "iovec" from the stream.
				/*!	Returns the number of bytes actually read, or a
					negative error code.  A NULL 'vector' is valid if 'count'
					is <= 0, in which case 'count' is returned.  
				*/
		virtual	ssize_t		ReadV(const iovec *vector, ssize_t count, uint32_t flags = 0) = 0;
		
				//!	Convenience for reading a vector of one buffer.
				ssize_t		Read(void *buffer, size_t size, uint32_t flags = 0);
};

/*-------------------------------------------------------------*/
/*------- IByteOutput Interface -------------------------------*/

//!	Abstract byte stream output (write) interface.
class IByteOutput : public IInterface
{
	public:

		B_DECLARE_META_INTERFACE(ByteOutput)

				//!	Write the bytes described by "iovec" in to the stream.
				/*!	Returns the number of bytes actually written, or a
					negative error code.  A NULL 'vector' is valid if 'count'
					is <= 0, in which case count is returned.  If 'count' is
					zero and the B_WRITE_END flag is supplied, the stream is
					truncated at its current location.  Writing to a stream
					will normally block until all bytes have been written;
					use B_DO_NOT_BLOCK to allow partial writes.
				*/
		virtual	ssize_t		WriteV(const iovec *vector, ssize_t count, uint32_t flags = 0) = 0;
		
				//!	Convenience for writing a vector of one buffer.
				ssize_t		Write(const void *buffer, size_t size, uint32_t flags = 0);
				
				//!	Make sure all data in the stream is written to its physical device.
				/*!	Returns B_OK if the data is safely stored away, else an error code.
				*/
		virtual	status_t	Sync() = 0;
};

/*-------------------------------------------------------------*/
/*------- IByteSeekable Interface -----------------------------*/

//!	Abstract byte stream seeking interface.
class IByteSeekable : public IInterface
{
	public:

		B_DECLARE_META_INTERFACE(ByteSeekable)

				//!	Return the current location in the stream, or a negative error code.
		virtual	off_t		Position() const = 0;
		
				//!	Move to a new location in the stream.
				/*!	The seek_mode can be either SEEK_SET, SEEK_END, or SEEK_CUR.
					Returns the new location, or a negative error code.
				*/
		virtual off_t		Seek(off_t position, uint32_t seek_mode) = 0;
};

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

inline ssize_t IByteInput::Read(void *buffer, size_t size, uint32_t flags)
{
	iovec v;
	v.iov_base = buffer;
	v.iov_len = size;
	return ReadV(&v,1,flags);
}

inline ssize_t IByteOutput::Write(const void *buffer, size_t size, uint32_t flags)
{
	iovec v;
	v.iov_base = const_cast<void*>(buffer);
	v.iov_len = size;
	return WriteV(&v,1,flags);
}

/*-------------------------------------------------------------*/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_BYTESTREAM_INTERFACE_H */
