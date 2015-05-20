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

#include <stdlib.h>
#include <string.h>
#include <support/SupportDefs.h>
#include <support/Bitfield.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

struct SBitfield::bitfield_fill_info {
	size_t startWord;
	size_t endWord;
	uint32_t leftMask;
	uint32_t rightMask;
	size_t numWords;
};

bool SBitfield::is_inline() const
{
	return (m_numBits <= BF_NUM_INLINE_BITS);
}


SBitfield::SBitfield(size_t size)
	:	m_bits(NULL),
		m_numBits(0)
{
	if (size) {
		status_t result = Resize(size);
		if (result < B_OK) {
			m_numBits = result;
		}
	}
}

SBitfield::SBitfield(size_t size, bool initial)
	:	m_bits(NULL),
		m_numBits(0)
{
	if (size) {
		status_t result = Resize(size);
		if (result < B_OK) {
			m_numBits = result;
		}
	}
	// Clear() called for us by Resize, so
	// just call Set() if initial is true
	if (initial) {
		Set(0, size);
	}
}

SBitfield::SBitfield(const SBitfield& copy)
	:	m_bits(NULL),
		m_numBits(0)
{
	if (copy.m_numBits >= 0) {
		if (copy.is_inline()) {
			m_inlineBits = copy.m_inlineBits;
			m_numBits = copy.m_numBits;
		} else {
			status_t result = Resize(copy.m_numBits);
			if (result < B_OK) {
				m_numBits = result;
			} else {
				m_numBits = copy.m_numBits;
				memcpy(m_bits, copy.m_bits, alloc_size(m_numBits));
			}
		}
	} else {
		m_numBits = copy.m_numBits;
	}
}

SBitfield::~SBitfield()
{
	if (!is_inline()) {
		free(m_bits);
	}
}

status_t SBitfield::InitCheck() const
{
	if (m_numBits >= 0)
		return B_OK;
	return (status_t)m_numBits;
}

bool SBitfield::operator == (const SBitfield& copy)
{
	if (this == &copy) return true;
	if (m_numBits != copy.m_numBits) return false;
	if (m_numBits > 0) {
		if (is_inline()) {
			const size_t mask = 0xFFFFFFFFLU << (32 - ((size_t)m_numBits & 0x1F));
			if ((m_inlineBits & mask) != (copy.m_inlineBits & mask)) {
				return false;
			}
		} else {
			bitfield_fill_info info;
			compute_masks(0, m_numBits, info);
			size_t last = info.endWord;
			for (size_t i = info.startWord; i < last; i++) {
				if (m_bits[i] != copy.m_bits[i]) return false;
			}
			if ((m_bits[last]&info.rightMask) != (copy.m_bits[last]&info.rightMask)) {
				return false;
			}
		}
	}
	return true;
}

void SBitfield::compute_masks(size_t start, size_t len, bitfield_fill_info& info)
{
	const size_t end = start+len-1;
	info.startWord = start >> 5;
	info.endWord = end >> 5;
	info.leftMask = 0xFFFFFFFFLU >> (start & 0x1F);
	info.rightMask = 0xFFFFFFFFLU << (31 - (end & 0x1F));
	info.numWords = info.endWord - info.startWord;
	if (info.numWords) info.numWords--;
}

status_t SBitfield::Set(size_t start, size_t len)
{
	if ((ssize_t)(start+len) > m_numBits)
		return B_BAD_VALUE;
	bitfield_fill_info info;
	compute_masks(start, len, info);
	if (is_inline()) {
		m_inlineBits |= (info.leftMask & info.rightMask);
	} else {	
		if (info.startWord == info.endWord) {
			m_bits[info.startWord] |= (info.leftMask & info.rightMask);
		} else {
			m_bits[info.startWord] |= info.leftMask;
			if (info.numWords) memset(m_bits+info.startWord+1, 0xFF, info.numWords*4);
			m_bits[info.endWord] |= info.rightMask;
		}
	}
	return B_OK;
}

bool SBitfield::TestAndSet(size_t bit)
{
	uint32_t* const bits = (m_numBits > BF_NUM_INLINE_BITS) ? (m_bits+(bit>>5)) : (&m_inlineBits);
	bit &= 0x1F;
	const bool test = ((*bits) & (0x80000000LU >> bit)) != 0;
	(*bits) |= (0x80000000LU >> bit);
	return test;
}

status_t SBitfield::Clear(size_t start, size_t len)
{
	if ((ssize_t)(start+len) > m_numBits)
		return B_BAD_VALUE;	
	bitfield_fill_info info;
	compute_masks(start, len, info);
	if (is_inline()) {
		m_inlineBits &= ~(info.leftMask & info.rightMask);
	} else {	
		if (info.startWord == info.endWord) {
			m_bits[info.startWord] &= ~(info.leftMask & info.rightMask);
		} else {
			m_bits[info.startWord] &= ~info.leftMask;
			if (info.numWords) memset(m_bits+info.startWord+1, 0, info.numWords*4);
			m_bits[info.endWord] &= ~info.rightMask;
		}
	}
	return B_OK;
}

status_t SBitfield::Resize(size_t bits)
{
	if ((ssize_t)bits <= m_numBits) {
		// Shrinking...
		if (!is_inline()) {
			if (bits <= BF_NUM_INLINE_BITS) {
				// We go back in inline mode
				const uint32_t theBits = *m_bits;
				free(m_bits);
				m_inlineBits = theBits;
				m_numBits = BF_NUM_INLINE_BITS;
			} else {
				// Resize the bitfield down
				m_bits = (uint32_t*)realloc(m_bits, alloc_size(bits));
				m_numBits = (ssize_t)bits;
			}
		}
		return B_OK;
	}
	
	// ... or growing the bitfield (We need to allocate more bits)
	if (bits > BF_NUM_INLINE_BITS) {
		const ssize_t oldBits = m_numBits;
		uint32_t *newbuf;
		if (is_inline()) { // the bitfield was inline but is not anymore -> malloc
			newbuf = (uint32_t*)malloc(alloc_size(bits));
			*newbuf = m_inlineBits;
		} else { // the bitfield was not inline -> realloc
			newbuf = (uint32_t*)realloc(m_bits, bits/8);
		}
		if (newbuf) {
			m_bits = newbuf;
			m_numBits = (ssize_t)bits;
			Clear(oldBits, bits-oldBits);
		} else {
			return B_NO_MEMORY;
		}
	} else {
		// the bits are now inline (meaning they already were)
		const ssize_t oldBits = m_numBits;
		m_numBits = BF_NUM_INLINE_BITS;
		if (oldBits) {
			Clear(oldBits, bits-oldBits);
		} else {
			m_inlineBits = 0;
		}
	}
	return B_OK;
}

int32_t SBitfield::word_set(uint32_t word)
{ // returns the first bit set in a non zero 32 bits word
	int32_t bit = 31;
	if (word & 0xFFFF0000) word >>= 16, bit -= 16;
	if (word & 0x0000FF00) word >>= 8,	bit -= 8;
	if (word & 0x000000F0) word >>= 4,	bit -= 4;
	if (word & 0x0000000C) word >>= 2,	bit -= 2;
	if (word & 0x00000002) word >>= 1,	bit -= 1;
	return bit;
}

ssize_t SBitfield::FirstSet() const
{
	if (m_numBits < 0)
		return m_numBits;
	if (is_inline()) {
		if (m_inlineBits)
			return word_set(m_inlineBits);
	} else {
		const size_t nbWords = (size_t)m_numBits / 32;
		for (size_t i=0 ; i<nbWords ; i++) {
			if (m_bits[i]) {
				return static_cast<ssize_t>((i*32) + word_set(m_bits[i]));
			}
		}
	}
	return -1;
}

ssize_t SBitfield::FirstClear() const
{
	if (m_numBits < 0)
		return m_numBits;
	if (is_inline()) {
		if (~m_inlineBits)
			return word_set(~m_inlineBits);
	} else {
		const size_t nbWords = (size_t)m_numBits / 32;
		for (size_t i=0 ; i<nbWords ; i++) {
			if (~m_bits[i]) {
				return static_cast<ssize_t>((i*32) + word_set(~m_bits[i]));
			}
		}
	}
	return -1;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
