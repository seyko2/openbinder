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

#include <support_p/Sequence.h>
#include <support/Errors.h>
#include <string.h>
#include <malloc.h>

SSequence::~SSequence()
{
}

void
SGapBuffer::SeekTo(uint32_t position)
{
	uint32_t pos = position;
	gap_buffer *buf = m_data;

	// short circut the linear walk, if we can
	if (pos >= m_current_base) {
		buf = m_current;
		pos -= m_current_base;
	}

	// walk the list of buffers until we find the one that contains position
	do {
		m_current = buf;
		m_current_base = position - pos;
		pos -= BufferSize(buf);
		buf = buf->next;
	} while (buf && (pos <= position));
}

status_t
SGapBuffer::Insert(uint32_t position, uint8_t const *src, uint32_t count)
{
	status_t result;
	status_t copied = 0;
	position *= m_item_size;
	count *= m_item_size;
	while (count) {
		SeekTo(position);
		MoveGap(m_current, position - m_current_base);
		uint32_t avail = (uint32_t)(m_current->end - m_current->start);
		if (avail < count) {
			result = SplitBuffer(&m_current, position - m_current_base);
			if (result != B_OK) return (copied ? copied : result);
			// splitting at the front of the first buffer needs special treatment
			if (m_current->next == m_data) m_data = m_current;
			avail = (uint32_t)(m_current->end - m_current->start);
		}
		uint32_t bytes_to_copy = (avail < count ? avail : count);
		memcpy(m_current->data + m_current->start, src, bytes_to_copy);
		m_current->start += bytes_to_copy;
		count -= bytes_to_copy;
		src += bytes_to_copy;
		position += bytes_to_copy;
		copied += bytes_to_copy;
		m_length += bytes_to_copy;
	}
	return copied;
}

status_t
SGapBuffer::Delete(uint32_t position, uint32_t count)
{
	position *= m_item_size;
	count *= m_item_size;
	uint32_t avail;

	SeekTo(position);
	MoveGap(m_current, position - m_current_base);
	avail = (uint32_t)(m_buf_size - m_current->end);

	if (avail < count) {
		// this could leave an entire page blank
		m_current->end += avail;
		count -= avail;
		m_length -= avail;
	} else {
		m_current->end += count;
		m_length -= count;
		return B_OK;
	}

	avail = BufferSize(m_current->next);
	gap_buffer *buf = 0;
	while (count > avail)
	{
		buf = m_current->next;
		m_current->next = buf->next;
		count -= avail;
		m_length -= avail;
		free(buf);
		avail = BufferSize(m_current->next);
	}

	if (count) {
		MoveGap(buf, 0);
		buf->end += count;
		m_length -= count;
	}
	return B_OK;
}

SSpan
SGapBuffer::ItemsAt(uint32_t position, uint32_t count)
{
	position *= m_item_size;
	count *= m_item_size;

	SeekTo(position);
	position -= m_current_base;
	gap_buffer *buf = m_current;
	uint32_t size = BufferSize(buf);
	if (position > size) return SSpan(0,0);
	uint32_t avail;
	if (position > buf->start) {
		position += (uint32_t)(buf->end - buf->start);
		avail = m_buf_size - position;
	} else {
		avail = buf->start - position;
	}
	return SSpan(buf->data + position, (avail < count ? avail : count) / m_item_size);
}

SGapBuffer::SGapBuffer(uint32_t items_per_page, uint32_t item_size)
	: SSequence(item_size), m_buf_size(items_per_page * item_size)
{
	size_t const struct_size = (sizeof(*m_data) - sizeof(m_data->data));
	m_page_size = m_buf_size + struct_size;
	m_data = NewBuffer();
	m_current = m_data;
	m_current_base = 0;
}

SGapBuffer::~SGapBuffer()
{
	gap_buffer *curr, *next;
	curr = m_data;
	while (curr)
	{
		next = curr->next;
		free(curr);
		curr = next;
	}
}

SGapBuffer::gap_buffer *
SGapBuffer::NewBuffer(void)
{
	gap_buffer *buf = (gap_buffer *)malloc(m_page_size);
	if (buf)
	{
		buf->next = 0;
		buf->start = 0;
		buf->end = m_buf_size;
	}
	return buf;
}

status_t
SGapBuffer::SplitBuffer(gap_buffer **srcp, uint32_t pos)
{
	gap_buffer *src = *srcp;
	gap_buffer *dst = NewBuffer();
	if (!dst) return B_NO_MEMORY;
	// normalize
	MoveGap(src, pos);
	
	if (src->start == 0) {
		// nothing on the left side
		m_current = dst;
		dst->next = src;
		*srcp = dst;
		return B_OK;
	}

	// link in the new buffer
	dst->next = src->next;
	src->next = dst;

	if (src->end == m_buf_size) {
		// nothing to copy on the right side
		m_current = dst;
		m_current_base += m_buf_size;
		return B_OK;
	}
	// copy the right side of the src buffer to dst
	memcpy(dst->data + pos, src->data + pos, m_buf_size - pos);
	// put the gap at the front of dst...
	dst->start = 0;
	dst->end = pos;
	// and leave it at the end of src
	src->start = pos;
	src->end = m_buf_size;
	return B_OK;
}

void
SGapBuffer::MoveGap(gap_buffer *buf, uint32_t pos)
{
	// make the gap start at pos and use all of the available space
	// pos marks the *logical* position in the buffer, not physical
	uint32_t start = buf->start;
	// quick out no moving
	if (start == pos) return;
	// quick out for full buffer
	if (start == buf->end) {
		buf->start = pos;
		buf->end = pos;
		return;
	}
	if (pos < start)
	{
		// logical == physical
		// 0 ... pos ... start ... end ... m_buf_size
		uint32_t bytes_to_move = start - pos;
		buf->end -= bytes_to_move;
		buf->start -= bytes_to_move;
		memmove(buf->data + buf->end, buf->data + pos, bytes_to_move);
	} else {
		// adjust pos to physical address
		pos += buf->end - start;
		// 0 ... start ... end ... pos ... m_buf_size
		uint32_t bytes_to_move = pos - buf->end;
		memmove(buf->data + start , buf->data + pos, bytes_to_move);
		buf->start += bytes_to_move;
		buf->end += bytes_to_move;
	}
}

/***********************************************************************
 *
 * Piece Tables : http://www.cs.unm.edu/~crowley/papers/sds/sds.html 
 *
 ***********************************************************************/
#define PIECE_TABLES 0
#if PIECE_TABLES
class PPieceTable : public SSequence {
public:
			PPieceTable(uint32_t item_size);
	virtual	~PPieceTable();

	virtual status_t Insert(uint32_t position, uint8_t const *src, uint32_t count = 1);
	virtual status_t Delete(uint32_t position, uint32_t count = 1);
	virtual SSpan ItemsAt(uint32_t position, uint32_t count = 1);

private:
			PPieceTable(SSequence *pieces, uint32_t item_size);
			PPieceTable(PPieceTable const &rhs);

};

typedef struct pt_buffer {
	pt_buffer	*next;
	uint16_t	length;		// switch to uint16_t?
	uint16_t	used;
	uint8_t		data[0];
} pt_buffer;

typedef struct pt_piece {
	pt_buffer	*buffer;
	uint16_t	offset;		// switch to uint16_t?
	uint16_t	length;
} pt_piece;

typedef struct pt_table {
	pt_buffer	*head;		// backing store for managed data
	pt_buffer	*tail;		// 
	pt_table	*spans;
	struct {
		pt_piece	*piece;
		uint16_t	offset;
	}			cursor;
} pt_table;

pt_table *
pt_table_create()
{
}

pt_table_insert()
{
}

#endif


PPieceTable::PPieceTable(uint32_t item_size)
	: SSequence(item_size)
{
	m_pieces = new PPieceTable(new SGapBuffer(128, sizeof(pt_piece)), sizeof(pt_piece));
}

PPieceTable::PPieceTable(SSequence *pieces, uint32_t item_size)
	: SSequence(item_size), m_pieces(pieces)
{
}

PPieceTable::~PPieceTable()
{
}

status_t
PPieceTable::Insert(uint32_t position, uint8_t const *src, uint32_t count)
{
	// copy src data to the append buffer as will fit
	// insert the pt_piece to track it
	// if more src remains
	//   append a new append buffer
	//   adjust src ptr and try again
	return B_OK;
}

status_t
PPieceTable::Delete(uint32_t position, uint32_t count)
{
	return B_OK;
}

SSpan
PPieceTable::ItemsAt(uint32_t position, uint32_t count)
{
	return SSpan(0,0);
}
