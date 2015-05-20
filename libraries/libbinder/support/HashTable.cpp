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

#include <support/HashTable.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

typedef uint32_t hint_t;

typedef struct hash_t {
    hint_t	h1;
    hint_t	h2;
} hash_t;

#define HASH_BITS 8
#define HASH_SIZE (1<<HASH_BITS)
#define HASH_MASK (HASH_SIZE-1)

#define HINIT1	0xFAC432B1UL
#define HINIT2	0x0CD5E44AUL

#define POLY1	0x00600340UL
#define POLY2	0x00F0D50BUL

SHasher::SHasher(int32_t bits)
{
	m_bits = bits;

	hash_t		Poly[64+1];
	int i;

	/*	Polynomials to use for various crc sizes.  Start with the 64 bit
		polynomial and shift it right to generate the polynomials for fewer
		bits.  Note that the polynomial for N bits has no bit set above N-8.
		This allows us to do a simple table-driven CRC. */
	
	Poly[64].h1 = POLY1;
	Poly[64].h2 = POLY2;
	for (i = 63; i >= 16; --i) {
		Poly[i].h1 = Poly[i+1].h1 >> 1;
		Poly[i].h2 = (Poly[i+1].h2 >> 1) | ((Poly[i+1].h1 & 1) << 31) | 1;
	}

	for (i = 0; i < 256; ++i) {
		int j;
		int v = i;
		hash_t hv = { 0, 0 };
		
		for (j = 0; j < 8; ++j, (v <<= 1)) {
			hv.h1 <<= 1;
			if (hv.h2 & 0x80000000UL) hv.h1 |= 1;
			hv.h2 = (hv.h2 << 1);
			if (v & 0x80) {
				hv.h1 ^= Poly[m_bits].h1;
				hv.h2 ^= Poly[m_bits].h2;
			}
		}
		m_crcxor[i] = hv.h1;
	}
}

uint32_t 
SHasher::BaseHash(const void *data, int32_t len) const
{
	hash_t hv = { HINIT1, HINIT2 };
	
	uint8_t *p = (uint8_t*)data;
	int s = m_bits - 8;
	hint_t m = (hint_t)-1 >> (32 - m_bits);
	
	hv.h1 = 0;
	hv.h2 &= m;

	while (len--) {
		int i = (hv.h2 >> s) & 255;
		hv.h2 = ((hv.h2 << 8) & m) ^ *p ^ m_crcxor[i];
		++p;
	}
	
	return hv.h2;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
