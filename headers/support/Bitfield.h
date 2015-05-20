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

#ifndef _SUPPORT_BITFIELD_H_
#define _SUPPORT_BITFIELD_H_

/*!	@file support/Bitfield.h
	@ingroup CoreSupportUtilities
	@brief A nice Bitfield class that provides some inline storage.
*/

#include <support/SupportDefs.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/**************************************************************************************/

//!	Vector of bits.
/*! The bitfield is stored as an array of 32 bits words on the heap.
	However, if the bitfield is smaller than 33 bits, it is stored inline.
	Test(), Set() and Clear() are "unsafe" in the sense that they won't check
	the input parameter (wether or not the bit requested is in or out the bitfield),
	so be careful when using those methods.
*/
class SBitfield
{
	struct bitfield_bit;
	friend struct bitfield_bit;

public:
						SBitfield(size_t size = 0);				//!< Contruct a bitfield of \p size bits
						SBitfield(size_t size, bool intial);	//!< Contruct a bitfield of \p size bits, initialize all values to \p initial
						SBitfield(const SBitfield& copy);
						~SBitfield();

			bool		operator == (const SBitfield& other);	//!< Compare 2 bitfields
			status_t	Resize(size_t bits);					//!< Change the size of the bitfield
	inline	size_t		CountBits() const;						//!< Return current # bits in bitfield
			status_t	InitCheck() const;						//!< B_NO_MEMORY if the bitfield couldn't be allocated
	
	//! Test a bit in the bitfield
	inline	bool		Test(size_t bit) const;					//!< This call doesn't make sure that \p bit is valid

	//! Set several continuous bits in the bitfield
			status_t	Set(size_t start, size_t len);
	inline	void		Set(size_t bit);						//!< This call doesn't make sure that \p bit is valid

	//!	Set a bit in the bitfield, returning its old value
			bool		TestAndSet(size_t bit);					//!< This call doesn't make sure that \p bit is valid

	//! Clear several continuous bits in the bitfield
			status_t	Clear(size_t start, size_t len);
	inline	void		Clear(size_t bit);						//!< This call doesn't make sure that \p bit is valid

	//! Search for the first bit set in the bitfield
			ssize_t		FirstSet() const;

	//! Search for the first bit cleared in the bitfield
			ssize_t		FirstClear() const;
	
	//! Read or write the bitfield as an array
	inline	bitfield_bit	operator[] (size_t bit)			{ return bitfield_bit(*this, bit); }
	inline	bool			operator[] (size_t bit) const	{  return Test(bit); }

private:
	struct bitfield_bit {
		inline bitfield_bit(SBitfield& bitfield, size_t bit) : m_bitfield(bitfield), m_bit(bit) { }
		inline bitfield_bit& operator = (bool value);
		inline operator bool();
	private:
		SBitfield& m_bitfield;
		size_t m_bit;
	};

	inline size_t alloc_size(size_t size);

	struct bitfield_fill_info;
	static void	compute_masks(size_t start, size_t len, bitfield_fill_info&);
	static int32_t SBitfield::word_set(uint32_t word);
	bool is_inline() const;
	union {
		uint32_t *	m_bits;
		uint32_t	m_inlineBits;
	};
	ssize_t	m_numBits;
};

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

#define BF_NUM_INLINE_BITS	(32)

inline size_t SBitfield::CountBits() const
{
	return m_numBits;
}

inline bool SBitfield::Test(size_t bit) const
{
	return ( ((m_numBits > BF_NUM_INLINE_BITS) ? (m_bits[bit>>5]) : (m_inlineBits)) & (0x80000000LU >> (bit & 0x1F)) ) != 0;
	//This was in the BeOS tree:
	//return (bool)( ( ((m_numBits > BF_NUM_INLINE_BITS) ? (m_bits[bit>>5]) : m_inlineBits) >> ((~bit) & 0x1F) ) & 0x1LU );
}

inline void SBitfield::Set(size_t bit)
{
	((m_numBits > BF_NUM_INLINE_BITS) ? (m_bits[bit>>5]) : (m_inlineBits)) |= (0x80000000LU >> (bit & 0x1F));
}

inline size_t SBitfield::alloc_size(size_t size)
{
	// allocate always in multiple of 32
	// so we round up to multiple of 32 and then divide by 8
	// to return number of bytes

	return ((size+31) & ~31) >> 3; 
}

inline void SBitfield::Clear(size_t bit)
{
	((m_numBits > BF_NUM_INLINE_BITS) ? (m_bits[bit>>5]) : (m_inlineBits)) &= ~(0x80000000LU >> (bit & 0x1F));
}

inline SBitfield::bitfield_bit& SBitfield::bitfield_bit::operator = (bool value)
{
	if (value)	m_bitfield.Set(m_bit);
	else m_bitfield.Clear(m_bit);
	return *this;
}

inline SBitfield::bitfield_bit::operator bool()
{
	return m_bitfield.Test(m_bit);
}

/**************************************************************************************/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif	/* _SUPPORT_BITFIELD_H_ */
