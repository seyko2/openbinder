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

#include "iobuffer.h"
#include "binder_defs.h"
#include <asm/uaccess.h>

int iobuffer_init(iobuffer_t *that, unsigned long base, int size, int consumed) {
	// require 4 byte alignment for base
	if ((base & 0x3) != 0) printk(KERN_WARNING "iobuffer_init() bad buffer alignment\n");
	if ((base & 0x3) != 0) return -EFAULT;
	if (!access_ok(VERIFY_WRITE, base, size)) printk(KERN_WARNING "access_ok(): FALSE\n");
	if (!access_ok(VERIFY_WRITE, base, size)) return -EFAULT;
	DPRINTF(9, (KERN_WARNING "iobuffer_init(%p, %08lx, %d)\n", that, base, size));
	that->m_base = base;
	that->m_size = size;
	that->m_offs = that->m_consumed = consumed;
	return 0;
}

int iobuffer_read_raw(iobuffer_t *that, void *data, int size)
{
	if ((that->m_size-that->m_offs) < size) return -EFAULT;
	copy_from_user(data, (void*)(that->m_base+that->m_offs), size);
	that->m_offs += size;
	return 0;
}

int iobuffer_read_u32(iobuffer_t *that, u32 *data)
{
	if ((that->m_size-that->m_offs) < sizeof(u32)) return -EFAULT;
	copy_from_user(data, (void*)(that->m_base+that->m_offs), sizeof(u32));
	that->m_offs += sizeof(u32);
	return 0;
}

int iobuffer_read_void(iobuffer_t *that, void **data)
{
	if ((that->m_size-that->m_offs) < sizeof(void*)) return -EFAULT;
	copy_from_user(data, (void*)(that->m_base+that->m_offs), sizeof(void*));
	that->m_offs += sizeof(void*);
	return 0;
}

int iobuffer_write_raw(iobuffer_t *that, const void *data, int size)
{
	if ((that->m_size-that->m_offs) < size) return -EFAULT;
	copy_to_user((void*)(that->m_base+that->m_offs), data, size);
	that->m_offs += size;
	return 0;
}

int iobuffer_write_u32(iobuffer_t *that, u32 data)
{
	if ((that->m_size-that->m_offs) < sizeof(u32)) return -EFAULT;
	// *((u32*)(that->m_base+that->m_offs)) = data;
	__put_user(data, ((u32*)(that->m_base+that->m_offs)));
	that->m_offs += sizeof(u32);
	return 0;
}

int iobuffer_write_void(iobuffer_t *that, const void *data)
{
	if ((that->m_size-that->m_offs) < sizeof(void *)) return -EFAULT;
	// *((void **)(that->m_base+that->m_offs)) = data;
	__put_user(data, ((void**)(that->m_base+that->m_offs)));
	that->m_offs += sizeof(void*);
	return 0;
}

int iobuffer_drain(iobuffer_t *that, int size) {
	if (size > (that->m_size-that->m_offs)) size = that->m_size-that->m_offs;
	that->m_offs += size;
	return size;
}

int iobuffer_remaining(iobuffer_t *that)
{
	return that->m_size-that->m_offs;
}

int iobuffer_consumed(iobuffer_t *that)
{
	return that->m_consumed;
}

void iobuffer_mark_consumed(iobuffer_t *that)
{
	that->m_consumed = that->m_offs;
}

void iobuffer_remainder(iobuffer_t *that, void **ptr, int *size)
{
	*ptr = ((uint8_t*)that->m_base)+that->m_offs;
	*size = that->m_size - that->m_offs;
}

