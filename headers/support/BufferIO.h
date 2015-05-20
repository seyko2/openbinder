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

#ifndef	_SUPPORT2_BUFFERIO_H
#define	_SUPPORT2_BUFFERIO_H

/*!	@file support/BufferIO.h
	@ingroup CoreSupportDataModel
	@brief Perform buffering on a byte stream.
*/

#include <support/ByteStream.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*	@addtogroup CoreSupportDataModel
	@{
*/

//!	Buffering on a generic byte stream.
/*!	Instantiante this class, pointing to an existing
	byte stream (IByteInput, IByteOutput, and IByteSeekable
	interfaces).  This gives you a new byte stream that
	performs buffering of reads/writes before calling
	to the real stream. */
class BBufferIO : public BnByteInput, public BnByteOutput, public BnByteSeekable
{
	enum {
		DEFAULT_BUF_SIZE = 65536L
	};

	public:
								BBufferIO(
									const sptr<IByteInput>& inStream,
									const sptr<IByteOutput>& outStream,
									const sptr<IByteSeekable>& seeker,
									size_t buf_size = DEFAULT_BUF_SIZE
								);
		virtual					~BBufferIO();
		
		virtual	ssize_t			ReadV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);
		virtual	ssize_t			WriteV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);
		virtual	status_t		Sync();
		
		virtual off_t			Seek(off_t position, uint32_t seek_mode);
		virtual	off_t			Position() const;

				// XXX remove in favor of Sync()??
				status_t		Flush();

	protected:

								BBufferIO(size_t buf_size = DEFAULT_BUF_SIZE);

	private:

				IByteInput *	m_in;
				IByteOutput *	m_out;
				IByteSeekable *	m_seeker;
				off_t			m_buffer_start;
				char * 			m_buffer;
				size_t 			m_buffer_phys;
				size_t 			m_buffer_used;
				off_t 			m_seek_pos;			// _reserved_ints[0-1]
				off_t 			m_len;				// _reserved_ints[2-3]
				bool 			m_buffer_dirty : 1;
				bool 			m_owns_stream : 1;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT2_BUFFERIO_H */
