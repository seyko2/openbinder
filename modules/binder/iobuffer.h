/* binder driver
 * Copyright (C) 2005 Palmsource, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef _IOBUFFER_H_
#define _IOBUFFER_H_

#include <linux/types.h>

typedef struct iobuffer {
	unsigned long m_base;
	int m_offs;
	int m_size;
	int m_consumed;
} iobuffer_t;

extern int	iobuffer_init(iobuffer_t *that, unsigned long base, int size, int consumed);
extern int	iobuffer_read_raw(iobuffer_t *that, void *data, int size);
extern int	iobuffer_read_u32(iobuffer_t *that, u32 *data);
extern int	iobuffer_read_void(iobuffer_t *that, void **data);
extern int	iobuffer_write_raw(iobuffer_t *that, const void *data, int size);
extern int	iobuffer_write_u32(iobuffer_t *that, u32 data);
extern int	iobuffer_write_void(iobuffer_t *that, const void *data);
extern int	iobuffer_drain(iobuffer_t *that, int size);
extern int	iobuffer_remaining(iobuffer_t *that);
extern int	iobuffer_consumed(iobuffer_t *that);
extern void	iobuffer_mark_consumed(iobuffer_t *that);
extern void	iobuffer_remainder(iobuffer_t *that, void **ptr, int *size);

#endif
