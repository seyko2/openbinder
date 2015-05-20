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

#ifndef _SUPPORT_P_SEQUENCE_H
#define _SUPPORT_P_SEQUENCE_H

#include <stdint.h>
#include <sys/types.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

struct SSpan {
	inline SSpan(uint8_t const *d, uint32_t c) : data(d), count(c) { };
	uint8_t const	*data;
	uint32_t		count;
};

class SSequence {
public:
	inline	SSequence(uint32_t item_size) : m_item_size(item_size), m_length(0) { };
	virtual	~SSequence();

	virtual status_t Insert(uint32_t position, uint8_t const *src, uint32_t count = 1) = 0;
	virtual status_t Delete(uint32_t position, uint32_t count = 1) = 0;
	virtual SSpan ItemsAt(uint32_t position, uint32_t count = 1) = 0;

	inline uint32_t Granularity(void) const { return m_item_size; };
	inline uint32_t Length(void) const { return m_length / m_item_size; };

protected:
	uint32_t	m_item_size;
	uint32_t	m_length;
private:
			SSequence(SSequence const &rhs);
};

class SGapBuffer : public SSequence {
public:
			SGapBuffer(uint32_t items_per_page, uint32_t item_size);
	virtual	~SGapBuffer();

	virtual status_t Insert(uint32_t position, uint8_t const *src, uint32_t count = 1);
	virtual status_t Delete(uint32_t position, uint32_t count = 1);
	virtual SSpan ItemsAt(uint32_t position, uint32_t count = 1);

private:
			SGapBuffer(SGapBuffer const &rhs);

	struct gap_buffer {
		gap_buffer	*next;
		uint32_t	start;
		uint32_t	end;
		uint8_t		data[4];
	};
	gap_buffer		*m_data;
	gap_buffer		*m_current;
	uint32_t		m_current_base;
	uint32_t		m_page_size;
	uint32_t		m_buf_size;
	
	gap_buffer *	NewBuffer(void);
	status_t		SplitBuffer(gap_buffer **buf, uint32_t pos);	// pos-th byte goes into the new buffer
	void			MoveGap(gap_buffer *buf, uint32_t pos);
	inline uint32_t	BufferSize(gap_buffer *buf) const { return (uint32_t)(m_buf_size - (buf->end - buf->start)); };
	void			SeekTo(uint32_t);
};

class SPieceTable : public SSequence {
public:
			SPieceTable(uint32_t item_size);
	virtual	~SPieceTable();

	virtual status_t Insert(uint32_t position, uint8_t const *src, uint32_t count = 1);
	virtual status_t Delete(uint32_t position, uint32_t count = 1);
	virtual SSpan ItemsAt(uint32_t position, uint32_t count = 1);

private:
			SPieceTable(SSequence *pieces, uint32_t item_size);
			SPieceTable(SPieceTable const &rhs);

#define ADS_SCOPE_BREAKAGE public:
#define ADS_SCOPE_UNBREAKAGE private:
ADS_SCOPE_BREAKAGE

	struct pt_buffer {
		pt_buffer	*next;
		uint32_t	length;
		uint32_t	used;
		uint8_t		data[4];
	};

ADS_SCOPE_UNBREAKAGE
#undef ADS_SCOPE_BREAKAGE
#undef ADS_SCOPE_UNBREAKAGE

	struct pt_piece {
		pt_buffer	*buffer;
		uint32_t	offset;
		uint32_t	length;
	};

	SSequence	*m_pieces;

};

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
#endif
