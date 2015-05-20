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

#ifndef _SUPPORT_BITSTREAM_READER_H_
#define _SUPPORT_BITSTREAM_READER_H_

#include <support/Buffer.h>
#include <support/ByteOrder.h>
#include <support/StdIO.h>
#include <assert.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define SWAP_INT32(uarg)									\
{															\
	uint32_t swapped = uarg ^ ((uarg<<16)|(uarg>>16));		\
	swapped &= 0xFF00FFFF;									\
	uarg = (uarg>>8)|(uarg<<24);							\
	uarg = (swapped >> 8) ^ uarg;							\
}
#else
#define SWAP_INT32(uarg)
#endif


class SBitstreamReader
{
	public:
								SBitstreamReader();
								SBitstreamReader(const SBuffer& sourceBuffer);
								SBitstreamReader(const void * data, size_t size);
								~SBitstreamReader();

		inline	status_t		SetSource(const SBuffer& sourceBuffer);
		inline	status_t		SetSource(const void * data, size_t size);

		inline	bool			IsInRange() const;

		inline	uint32_t		PeekBits(size_t bits) const;
		inline	uint32_t		GetBits(size_t bits);

		inline	void			SkipBit();
		inline	void			SkipByte();
		inline	void			SkipBits(size_t bits);
		inline	void			SkipToByteBoundary();

		inline	void			RewindBits(size_t bits);

		inline	bool			Find(uint32_t pattern, size_t bits);
		inline	bool			FindByteAligned(uint32_t pattern, size_t bits);
		inline	bool			FindReverse(uint32_t pattern, size_t bits);
		inline	bool			FindByteAlignedReverse(uint32_t pattern, size_t bits);

		inline	size_t			BytePosition() const;
		inline	size_t			BitPosition() const;

		// note that this will only return meaningful values <= 32, as it only
		// reports the amount of data in the cache.  use BytesAvailable() to get
		// the distance from the end of the source (this is considerably slower.)
		inline	size_t			BitsAvailable() const
		{
			if (m_cacheOffset > m_cacheSize) return 0;
			return m_cacheSize - m_cacheOffset;
		}

		inline	size_t			BytesAvailable() const;

		inline	status_t		SetBytePosition(size_t pos);
		inline	status_t		SetBitPosition(size_t pos);

		inline	size_t			ByteLength() const;

	private:
		struct	SFragment
		{
			const uint8_t *		data;
			size_t				size;
			size_t				offset;
		};
		
				uint32_t		m_cache[2];
				size_t			m_cacheOffset;			// bits
				const uint8_t *	m_fragmentPointer;
				size_t			m_fragmentRemaining;	// bytes

				size_t			m_cacheSize;		// bits cached; usually 64 unless out of data

				SFragment *		m_fragments;
				size_t			m_fragmentCount;
				size_t			m_fragmentIndex;

		inline	size_t			fill_forward(size_t fillOffset)
		{
			assert (fillOffset == 0 || fillOffset == 4);
			size_t n;

			m_cache[fillOffset >> 2] = 0;
			uint8_t * cacheByte = (uint8_t*)m_cache + fillOffset;
			size_t toCache = 4;

			assert (m_fragmentPointer >= m_fragments[m_fragmentIndex].data);
			
			// handle source(s) with fewer than 4 bytes of data
			while (true)
			{
				while (m_fragmentRemaining == 0)
				{
					if (m_fragmentIndex + 1 < m_fragmentCount)
					{
						m_fragmentIndex++;
						m_fragmentPointer = m_fragments[m_fragmentIndex].data;
						m_fragmentRemaining = m_fragments[m_fragmentIndex].size;
					}
					else
					{
						toCache = 0;
						break;
					}
				}

				if (m_fragmentRemaining >= toCache) break;
			
				*cacheByte = *m_fragmentPointer;
				m_fragmentPointer++;
				cacheByte++;
				toCache--;
				m_fragmentRemaining--;
			}
			
			size_t shift = 0;
			while (toCache > 0)
			{
				assert (m_fragmentPointer >= m_fragments[m_fragmentIndex].data);
				assert (m_fragmentRemaining >= toCache); // this precondition is handled by the first loop

				// read the next cache word; we want to read on 32-bit
				// boundaries after this point, so pad accordingly if
				// there's room.
				size_t align = 4 - (uint32_t(m_fragmentPointer) & 0x3);
					
				if (align == 4 && toCache == 4)
				{
					// easy case: aligned word
					m_cache[fillOffset >> 2] = *(uint32_t*)m_fragmentPointer;
					m_fragmentPointer += 4;
					m_fragmentRemaining -= toCache;
					cacheByte += 4;
					break;
				}
				else
				{
					size_t toRead;
					if (align != 4 && align < toCache)
					{
						toRead = align;
						
						// we have room to pad.  shift current data so that the following
						// read can be a whole word.  this is rather hairy because, depending
						// on where we started filling the cache, the first word may or
						// may not be byteswapped.
						shift = toCache - align;
						if (shift > 4) shift -= 4;

						toCache -= shift;
						
						shift <<= 3;

						m_cacheOffset += shift;
					}
					else
					{
						toRead = toCache;
					}
						
					for (n = 0; n < toRead; n++)
					{
						*cacheByte = *m_fragmentPointer;
						m_fragmentPointer++;
						m_fragmentRemaining--;
						cacheByte++;
					}
					toCache -= toRead;
				}
			}

			if (fillOffset == 0)
			{
				SWAP_INT32(m_cache[0]);
			}
			SWAP_INT32(m_cache[1]);

			if (shift > 0)
			{
				uint32_t v = m_cache[0];
				m_cache[0] >>= shift;
				m_cache[1] >>= shift;
				m_cache[1] |= (v << (32-shift));
				cacheByte += (shift >> 3);
			}

			size_t writeOffset = (cacheByte - (uint8_t*)m_cache);
			return (writeOffset - fillOffset) << 3;
		}

		inline	void			fill_reverse()
		{
			// read one word before current cache position
			size_t cachedBytes = (m_cacheSize >> 3);

			m_cache[0] = 0;
			uint8_t * cacheByte = ((uint8_t*)&m_cache[0]) + 3;
			size_t toRead = 4;

			// rewind fragment pointer to first byte we want to read
			const uint8_t * fptr = m_fragmentPointer;
			size_t findex = m_fragmentIndex;
			int32_t toRewind = cachedBytes;
			int32_t dataLeft = m_fragmentPointer - m_fragments[m_fragmentIndex].data;
			while (toRewind > 0)
			{
				if (toRewind >= dataLeft)
				{
					toRewind -= dataLeft;
					assert (findex > 0);
					--findex;
					dataLeft = m_fragments[findex].size;
					fptr = m_fragments[findex].data + dataLeft;
				}
				else
				{
					fptr -= toRewind;
					dataLeft -= toRewind;
					break;
				}
			}
			
			// read
			while (toRead > 0)
			{
				fptr--;
				dataLeft--;
				if (dataLeft < 0)
				{
					if (findex == 0)
					{
						// out of data
						fptr++;
						break;
					}
					--findex;
					dataLeft = m_fragments[findex].size - 1;
					fptr = m_fragments[findex].data + dataLeft;
				}
				
				*cacheByte = *fptr;
				cacheByte--;
				toRead--;
			}

			if (m_cacheSize < 32)
			{
				m_cacheSize += 32;
			}
			else
			{
				m_cacheSize = 64;
			}

			cachedBytes = (m_cacheSize >> 3) - toRead;

			SWAP_INT32(m_cache[0]);

			// advance fragment pointer past cached data
			size_t toAdvance = cachedBytes;
			size_t foffset = fptr - m_fragments[findex].data;
			size_t fremaining = m_fragments[findex].size - foffset;
			while (toAdvance > 0)
			{
				if (toAdvance > fremaining)
				{
					toAdvance -= fremaining;
					assert (findex + 1 < m_fragmentCount);
					++findex;
					fremaining = m_fragments[findex].size;
					fptr = m_fragments[findex].data;
				}
				else
				{
					fptr += toAdvance;
					fremaining -= toAdvance;
					break;
				}
			}
			m_fragmentPointer = fptr;
			m_fragmentIndex = findex;
			m_fragmentRemaining = fremaining;
		}
		
		inline	void			next_word()
		{
//berr << ">> next_word : frag remaining " << m_fragmentRemaining << " ptr align " << ((uint32_t)m_fragmentPointer & 0x3) << " << \n";
			assert (m_cacheOffset >= 32);
			
			// cache[0] is empty
			m_cache[0] = m_cache[1];
			m_cacheOffset -= 32;

			if (m_fragmentRemaining >= 4 &&
				((uint32_t)m_fragmentPointer & 0x3) == 0)
			{
				// aligned read
				m_cache[1] = *(uint32_t*)m_fragmentPointer;
				m_fragmentPointer += 4;
				m_fragmentRemaining -= 4;
				SWAP_INT32(m_cache[1]);
			}
			else
			{
				// unaligned read
				if (m_cacheSize == 64)
				{
					// the cache is full, so there may be more data to read.
					size_t readCount = fill_forward(4);
					m_cacheSize = 32 + readCount;
				}
				else
				if (m_cacheSize > 32)
				{
					m_cacheSize -= 32;
				}
				else
				{
					m_cacheSize = 0;
				}
			}
		}
};

// ------------------------------------------------------------------------- //

inline 
SBitstreamReader::SBitstreamReader()
	: m_cacheOffset(0),
	  m_fragmentRemaining(0),
	  m_cacheSize(0),
	  m_fragments(0),
	  m_fragmentCount(0),
	  m_fragmentIndex(0)
{
}

inline 
SBitstreamReader::SBitstreamReader(const SBuffer& sourceBuffer)
	: m_fragments(0)
{
	SetSource(sourceBuffer);
}

inline 
SBitstreamReader::SBitstreamReader(const void * data, size_t size)
	: m_fragments(0)
{
	SetSource(data, size);
}

inline
SBitstreamReader::~SBitstreamReader()
{
	if (m_fragments != 0)
	{
		delete [] m_fragments;
	}
}

inline status_t
SBitstreamReader::SetSource(const SBuffer& sourceBuffer)
{
	if (m_fragments != 0)
	{
		delete [] m_fragments;
	}

	// count fragments
	m_fragmentCount = 0;
	const SBuffer * i;
	for (i = &sourceBuffer; i; i = i->next)
	{
		++m_fragmentCount;
	}
	m_fragments = new SFragment[m_fragmentCount];
	if (m_fragments == 0)
	{
		assert (!"SBitstreamReader: no memory for fragment list");
		return B_NO_MEMORY;
	}
	size_t n;
	size_t offset = 0;
	for (n = 0, i = &sourceBuffer; i; i = i->next, n++)
	{
		m_fragments[n].data = (const uint8_t*)i->Data();
		m_fragments[n].size = i->Size();
		m_fragments[n].offset = offset;
		offset += m_fragments[n].size;
	}
	
	m_cacheOffset = 0;
	m_fragmentIndex = 0;
	m_fragmentPointer = m_fragments[0].data;
	m_fragmentRemaining = m_fragments[0].size;

	m_cacheSize = fill_forward(0);
	m_cacheSize += fill_forward(4);

	return B_OK;
}	

inline status_t
SBitstreamReader::SetSource(const void * data, size_t size)
{
	if (m_fragments != 0)
	{
		delete [] m_fragments;
	}
	m_fragmentCount = 1;
	m_fragments = new SFragment[1];
	if (m_fragments == 0)
	{
		assert (!"SBitstreamReader: no memory for fragment list");
		return B_NO_MEMORY;
	}
	m_fragments[0].data = (const uint8_t*)data;
	m_fragments[0].size = size;
	m_fragments[0].offset = 0;
	
	m_cacheOffset = 0;
	m_fragmentIndex = 0;
	m_fragmentPointer = (const uint8_t*)data;
	m_fragmentRemaining = size;

	m_cacheSize = fill_forward(0);
	m_cacheSize += fill_forward(4);

	return B_OK;
}

inline bool
SBitstreamReader::IsInRange() const
{
	return (m_cacheSize > 0);
}

inline uint32_t
SBitstreamReader::PeekBits(size_t bits) const
{
	assert (m_cacheOffset + bits <= m_cacheSize);

	uint32_t result = m_cache[0] << m_cacheOffset;
	if (bits > (32-m_cacheOffset))
	{
		uint32_t v = m_cache[1] >> (32-m_cacheOffset);
		result |= v;
	}
	return result >> (32-bits);
}

inline uint32_t
SBitstreamReader::GetBits(size_t bits)
{
	assert (m_cacheOffset + bits <= m_cacheSize);
	
	uint32_t result = m_cache[0] << m_cacheOffset;
	if (bits > (32-m_cacheOffset))
	{
		uint32_t v = m_cache[1] >> (32-m_cacheOffset);
		result |= v;
	}
	m_cacheOffset += bits;
	if (m_cacheOffset >= 32)
	{
		next_word();
	}
	return result >> (32-bits);
}

inline void
SBitstreamReader::SkipBit()
{
	m_cacheOffset++;
	if (m_cacheOffset >= 32)
	{
		next_word();
	}
}

inline void
SBitstreamReader::SkipByte()
{
	m_cacheOffset += 8;
	if (m_cacheOffset >= 32)
	{
		next_word();
	}
}

inline void
SBitstreamReader::SkipBits(size_t bits)
{
	m_cacheOffset += bits;
	while (m_cacheOffset >= 32)
	{
		next_word();
	}
}

inline void
SBitstreamReader::SkipToByteBoundary()
{
	m_cacheOffset = (m_cacheOffset + 7) & ~7;
	if (m_cacheOffset >= 32)
	{
		next_word();
	}
}

inline void
SBitstreamReader::RewindBits(size_t bits)
{
	assert (bits > 0);
	assert (bits <= 32);
//berr << SPrintf("RewindBits in (%ld): cache 0x%08lx 0x%08lx offset %ld\n", bits, m_cache[0], m_cache[1], m_cacheOffset );
	
	if (m_cacheOffset >= bits)
	{
		m_cacheOffset -= bits;
//berr << SPrintf("RewindBits (simple): cache 0x%08lx 0x%08lx offset %ld\n", m_cache[0], m_cache[1], m_cacheOffset );
		return;
	}
	m_cache[1] = m_cache[0];
	fill_reverse();
	m_cacheOffset += (32 - bits);

// berr << SPrintf("RewindBits: cache 0x%08lx 0x%08lx cacheOffset %ld cacheSize %ld frag %ld offset %ld\n",
		// m_cache[0], m_cache[1], m_cacheOffset, m_cacheSize, m_fragmentIndex,
		// m_fragmentPointer - m_fragments[m_fragmentIndex].data);
}

inline bool
SBitstreamReader::Find(uint32_t pattern, size_t bits)
{
	while (BitsAvailable() >= bits)
	{
		uint32_t v = PeekBits(bits);
		if (v == pattern) return true;

		SkipBit();
	}
	return false; // +++ should rewind to previous position
}

inline bool
SBitstreamReader::FindByteAligned(uint32_t pattern, size_t bits)
{
	SkipToByteBoundary();
	while (BitsAvailable() >= bits)
	{
		uint32_t v = PeekBits(bits);
		if (v == pattern) return true;

		SkipByte();
	}

	return false; // +++ should rewind to previous position
}

inline bool
SBitstreamReader::FindReverse(uint32_t pattern, size_t bits)
{
	// start search at (current position) - bits
	if (BitPosition() < bits)
	{
		return false;
	}
	RewindBits(bits);
	while (true)
	{
		uint32_t v = PeekBits(bits);
		if (v == pattern) return true;

		if (BitPosition() == 0)
		{
			break;
		}
		RewindBits(1);
	}
	return false; // +++ should rewind to previous position
}

inline bool
SBitstreamReader::FindByteAlignedReverse(uint32_t pattern, size_t bits)
{
	int32_t pos = (int32_t)BitPosition();
	pos -= (bits + 7);
	pos = (pos + 0x7) & ~0x7;
   	if (pos < 0)
	{
		return false;
	}
	SetBytePosition(pos >> 3);
	while (true)
	{
		uint32_t v = PeekBits(bits);
		if (v == pattern) return true;

		if (BitPosition() == 0)
		{
			break;
		}
		RewindBits(8);
	}

	return false; // +++ should rewind to previous position
}

inline size_t
SBitstreamReader::BytesAvailable() const
{
	return ByteLength() - BytePosition();
}

inline size_t
SBitstreamReader::BytePosition() const
{
	const SFragment& f = m_fragments[m_fragmentIndex];
	size_t pos = f.offset + (f.size - m_fragmentRemaining);
	pos -= ((m_cacheSize - m_cacheOffset) >> 3);
	return pos;
}

inline size_t
SBitstreamReader::BitPosition() const
{
	const SFragment& f = m_fragments[m_fragmentIndex];
	size_t pos = f.offset + (f.size - m_fragmentRemaining);
	pos <<= 3;
	pos -= (m_cacheSize - m_cacheOffset);
	return pos;
}

inline status_t
SBitstreamReader::SetBytePosition(size_t pos)
{
	// find fragment corresponding to pos
	size_t findex;
	for (findex = 0; findex < m_fragmentCount; findex++)
	{
		const SFragment & f = m_fragments[findex];
		size_t offset = f.offset;
		size_t end = offset + f.size;
		if (offset <= pos && end >= pos)
		{
			// cache
			m_fragmentIndex = findex;
			m_fragmentPointer = m_fragments[findex].data + (pos - offset);
			m_fragmentRemaining = end - pos;
			m_cacheOffset = 0;
			m_cacheSize = fill_forward(0);
			m_cacheSize += fill_forward(4);
			return B_OK;
		}
	}
	return B_OUT_OF_RANGE;
}

inline status_t
SBitstreamReader::SetBitPosition(size_t pos)
{
	status_t result = SetBytePosition(pos >> 3);
	if (result < B_OK)
	{
		return result;
	}
	size_t bit = pos & 0x7;
	if (BitsAvailable() < bit)
	{
		return B_OUT_OF_RANGE;
	}
	SkipBits(bit);
	return B_OK;
}

inline size_t
SBitstreamReader::ByteLength() const
{
	if (m_fragmentCount == 0)
	{
		return 0;
	}
	const SFragment& f = m_fragments[m_fragmentCount-1];
	return f.offset + f.size;
}

#if _SUPPORTS_NAMESPACE
} } // palmos::support
#endif

#endif // _SUPPORT_BITSTREAM_READER_H_
