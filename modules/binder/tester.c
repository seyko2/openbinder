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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <support_p/binder.h>

typedef signed long sl_t;
typedef unsigned long ul_t;

const sl_t cmd_write_limit = 1024;
const sl_t cmd_read_limit = 1024;

int main(int argc, char **argv) {
	int result;
	binder_write_read_t bwr;
	sl_t write_count = 0;
	uint8_t *write_buf = malloc(cmd_write_limit);
	uint8_t *read_buf = malloc(cmd_read_limit);
	bwr.write_buffer = (ul_t)write_buf;
	bwr.write_size = 0;
	bwr.read_size = cmd_read_limit;
	bwr.read_buffer = (ul_t)read_buf;
	uint8_t *wb = write_buf;


	int fd = open("/dev/binder", O_RDWR);
	if (fd < 0) {
		printf("Open failed: %s\n", strerror(errno));
		return -1;
	}
	*(ul_t*)wb = bcSET_CONTEXT_MANAGER;
	bwr.write_size += sizeof(ul_t);
	wb += sizeof(ul_t);
	*(ul_t*)wb = bcENTER_LOOPER;
	bwr.write_size += sizeof(ul_t);
	result = ioctl(fd, BINDER_WRITE_READ, &bwr);
	printf("ioctl(fd, BINDER_WRITE_READ, &bwr): %08x", result);
	if (result < 0) printf(" %08x : %s", errno, strerror(errno));
	printf("\n");
	return 0;
}
