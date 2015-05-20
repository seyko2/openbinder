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

#include <support/BufferIO.h>
#include <support/PositionIO.h>

#include <string.h>
#include <support/Debug.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

BBufferIO::BBufferIO(BPositionIO * stream, size_t buf_size, bool owns_stream)
{
	if (buf_size < 512) buf_size = 512;
	m_buffer_start = 0;
	m_stream = stream;
	m_buffer = new char[buf_size];
	m_buffer_phys = buf_size;
	m_buffer_used = 0;
	m_seek_pos = m_stream->Position();
	m_len = m_stream->Seek(0, SEEK_END);
	m_stream->Seek(m_seek_pos, SEEK_SET);
	m_buffer_dirty = false;
	m_owns_stream = owns_stream;
}


BBufferIO::~BBufferIO()
{
	if (m_buffer_dirty) {
		(void)Flush();
	}
	delete[] m_buffer;
	if (m_owns_stream) {
		delete m_stream;
	}
}


ssize_t 
BBufferIO::ReadAt(off_t pos, void *buffer, size_t size)
{
	ssize_t err = B_OK;
	if (size >= m_buffer_phys) {		/* un-buffered read */
		/* check for overlap and flush */
		if ((pos < m_buffer_start + m_buffer_used) && (pos+size > m_buffer_start)) {
			if (m_buffer_dirty) {
				err = Flush();
				if (err < B_OK) return err;
			}
		}
		err = m_stream->ReadAt(pos, buffer, size);
		/* Suppose we have a file with a table of contents or index in one place, */
		/* and large chunks of data in the rest of the file. In that situation, it */
		/* makes sense to keep the buffer filled with data from the small-read */
		/* section, and just pass large requests through to the underlying I/O. */
		/* That way, we also preserve the file system semantics of bypassing */
		/* the buffer cache for "large" reads. */
#if DEBUG
		fprintf(stderr, "ReadAt big %d\n", err);
#endif
		return err;
	}
	if ((pos+size <= m_buffer_start) || (pos >= m_buffer_start+m_buffer_used) || 
		((pos <= m_buffer_start) && (pos+size >= m_buffer_start+m_buffer_used))) {
		/* totally outside buffer, or surrounding buffer */
		if (m_buffer_dirty) {
			err = Flush();
			if (err < B_OK) return err;
		}
		m_buffer_start = pos;
		m_buffer_used = 0;
		err = m_stream->ReadAt(m_buffer_start, m_buffer, m_buffer_phys);
		if (err < 0) {
			return err;
		}
		m_buffer_used = err;
		memcpy(buffer, m_buffer, (err < (ssize_t)size) ? err : size);
#if DEBUG
		fprintf(stderr, "ReadAt surrounds %d %d\n", err, size);
#endif
		return (err < (ssize_t)size) ? err : size;
	}
	if ((pos >= m_buffer_start) && (pos+size <= m_buffer_start+m_buffer_used)) {
		/* totally inside buffer */
		memcpy(buffer, m_buffer+(pos-m_buffer_start), size);
		return size;
	}
	if (pos < m_buffer_start) {
		/* partial overlap, end of data, start of buffer */
		ASSERT(pos+size > m_buffer_start);
		ASSERT(pos+size <= m_buffer_start+m_buffer_used);
		/* we do not move buffer, as the read ends up within buffer */
		size_t delta = m_buffer_start-pos;
		err = m_stream->ReadAt(pos, buffer, delta);
		if (err < (ssize_t)delta) {
#if DEBUG
			fprintf(stderr, "returning error: %08x\n", err);
#endif
			return err;
		}
		memcpy(((char *)buffer)+delta, m_buffer, size-delta);
#if DEBUG
		fprintf(stderr, "ReadAt partial overlap: size %d\n", size);
#endif
		return size;
	}
	/* partial overlap, start of data, end of buffer */
	ASSERT(pos+size > m_buffer_start+m_buffer_used);
	ASSERT(pos < m_buffer_start+m_buffer_used);
	size_t delta = pos-m_buffer_start;
	if (m_buffer_dirty) {
		err = Flush();
		if (err < B_OK) return err;
	}
	int rd = m_buffer_used-delta;
	if (rd > 0) {
		memcpy(buffer, m_buffer+delta, rd);
	}
	/* fill buffer */
	m_buffer_start = pos+rd;
	m_buffer_used = 0;
	err = m_stream->ReadAt(m_buffer_start, m_buffer, m_buffer_phys);
	if (err <= 0) {
		return rd;
	}
	m_buffer_used = err;
	memcpy(((char *)buffer)+rd, m_buffer, (err < (ssize_t)(size-rd)) ? err : (size-rd));
#if DEBUG
	fprintf(stderr, "ReadAt end overlap: %d %d %d = %d\n", rd, err, size, rd + ((err < size-rd) ? err : (size-rd)));
#endif
	return rd + (err < (ssize_t)(size-rd) ? err : (size-rd));
}

ssize_t 
BBufferIO::WriteAt(off_t pos, const void *buffer, size_t size)
{
	if (pos+size > m_len) {
		m_len = pos+size;
	}
	
	ssize_t err;
	if (size >= m_buffer_phys) {	/* large-block uncached write */
		if ((pos+size > m_buffer_start) && (pos < m_buffer_start+m_buffer_used)) {
			/* flush if there is some overlap (sub-optimal, but safe) */
			err = Flush();
			if (err < 0) return err;
			m_buffer_used = 0;
		}
		return m_stream->WriteAt(pos, buffer, size);
	}
	if ((pos <= m_buffer_start) && (pos+size >= m_buffer_start+m_buffer_used)) {
		/* Write surrounds buffer; all previous changes are wiped */
		memcpy(m_buffer, buffer, size);
		m_buffer_start = pos;
		m_buffer_used = size;
		m_buffer_dirty = true;
		return size;	/* this "write" worked OK */
	}
	if ((pos >= m_buffer_start) && (pos+size <= m_buffer_start+m_buffer_phys)) {
		if (pos > m_buffer_start + m_buffer_used) {
			/* Suppose we write at beginning of cache, and then at end, */
			/* with a hole in the middle. Also suppose this is past end of file. */
			memset(m_buffer+m_buffer_used, 0, (pos-m_buffer_start-m_buffer_used));
			off_t len = m_stream->Seek(0, SEEK_END);	/* find end -- could be cached */
			if (len > m_buffer_start+m_buffer_used) {
				/* Read data if the hole appears somewhere within the file size */
				ssize_t err = m_stream->ReadAt(m_buffer_start+m_buffer_used, m_buffer+m_buffer_used, 
					m_buffer_phys-m_buffer_used);
				if (err < 0) {
					return err;
				}
				m_buffer_used += err;
			}
		}
		/* Buffer surrounds Write; just copy data */
		memcpy(m_buffer+(pos-m_buffer_start), buffer, size);
		if (pos-m_buffer_start+size > m_buffer_used) {
			m_buffer_used = pos-m_buffer_start+size;
		}
		m_buffer_dirty = true;
		return size;	/* this "write" worked OK */
	}
	err = Flush();
	if (err < 0) return err;
	/* fill buffer */
	ASSERT(size <= m_buffer_phys);
	m_buffer_start = pos;
	m_buffer_used = size;
	memcpy(m_buffer, buffer, size);
	m_buffer_dirty = true;
	return size;	/* this "write" was fine */
}


off_t 
BBufferIO::Seek(off_t position, uint32_t seek_mode)
{
	switch (seek_mode) {
		case SEEK_SET:
			m_seek_pos = position;
			break;
		case SEEK_CUR:
			m_seek_pos += position;
			break;
		case SEEK_END:
			m_seek_pos = m_len + position;
			break;
	}
	return m_seek_pos;
}

off_t 
BBufferIO::Position() const
{
	return m_seek_pos;
}


status_t 
BBufferIO::SetSize(off_t size)
{
	status_t err = m_stream->SetSize(size);
	if (err >= 0) {
		m_len = size;
	}
	if (size <= m_buffer_start) {
		if (err >= 0) {		/* if OK, potential changes are lost */
			m_buffer_start = 0;
			m_buffer_used = 0;
			m_buffer_dirty = false;
		}
		return err;
	}
	if (size < m_buffer_start+m_buffer_used) {
		if (err >= 0) {		/* lose changes after new EOF */
			m_buffer_used = (size - m_buffer_start);
		}
		return err;
	}
	return err;
}

status_t
BBufferIO::Sync()
{
	status_t err = Flush();
	if (err >= B_OK) err = m_stream->Sync();
	return err;
}

status_t
BBufferIO::Flush()
{
	if (!m_buffer_dirty) {
		return B_OK;
	}
	
	/* extend size of file, if needed */
	off_t len = m_stream->Seek(0, SEEK_END);	/* find end -- could be cached */
	if (len < m_buffer_start+m_buffer_used) {
		/* reserve file space */
		status_t err = m_stream->SetSize(m_buffer_start+m_buffer_used);
		if (err < B_OK) {
			return err;
		}
	}
	
	/* write data into file and reset buffer */
	ssize_t err = m_stream->WriteAt(m_buffer_start, m_buffer, m_buffer_used);
	if (err != (ssize_t)m_buffer_used) {
		return (err < 0) ? err : B_IO_ERROR;
	}
	m_buffer_dirty = false;
	
	return B_OK;
}

BPositionIO * 
BBufferIO::Stream() const
{
	return m_stream;
}

size_t 
BBufferIO::BufferSize() const
{
	return m_buffer_phys;
}

void
BBufferIO::PrintToStream() const
{
	fprintf(stderr, "stream 0x%08x\n", (uint)m_stream);
	fprintf(stderr, "buffer 0x%08x\n", (uint)m_buffer);
	fprintf(stderr, "start  %Ld\n", m_buffer_start);
	fprintf(stderr, "used   %ld\n", m_buffer_used);
	fprintf(stderr, "phys   %ld\n", m_buffer_phys);
	fprintf(stderr, "dirty  %s\n", m_buffer_dirty ? "true" : "false");
	fprintf(stderr, "owns   %s\n", m_owns_stream ? "true" : "false");
}

status_t BBufferIO::_Reserved_BufferIO_0(void *)
{
	return B_ERROR;
}

status_t BBufferIO::_Reserved_BufferIO_1(void *)
{
	return B_ERROR;
}

status_t BBufferIO::_Reserved_BufferIO_2(void *)
{
	return B_ERROR;
}

status_t BBufferIO::_Reserved_BufferIO_3(void *)
{
	return B_ERROR;
}

status_t BBufferIO::_Reserved_BufferIO_4(void *)
{
	return B_ERROR;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
