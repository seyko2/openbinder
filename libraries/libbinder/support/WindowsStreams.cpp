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

#include <support/WindowsStreams.h>

#include <DebugMgr.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// This is what printf() in libc currently does.
// So we do it, too.  It's fun!
static long HostVPrintF(const char* fmt, va_list args)
{
	return printf(fmt, args);
}

static long HostGetChar(void)
{
	return getchar();
}

static int HostPrintF(const char *format, ...)
{
	va_list	arg;
	int		length = 0;

	va_start(arg, format);
	length = HostVPrintF(format, arg);
	va_end(arg);

	return length;	
}


/*-------------------------------------------------------------*/

BWindowsOutputStream::BWindowsOutputStream()
{
}

BWindowsOutputStream::~BWindowsOutputStream()
{
}

ssize_t 
BWindowsOutputStream::WriteV(const struct iovec *vector, ssize_t count, uint32_t /*flags*/)
{
	ssize_t total = 0;
	for (ssize_t index = 0 ; index < count ; index++) 
	{
		total += vector[index].iov_len;
	}
	
	char stack_buf[256];
	char* buf;
	if (total < 256) buf = stack_buf;
	else buf = (char*)malloc(total+1);
	if (buf)
	{
		total = 0;
		for (ssize_t index = 0 ; index < count ; index++) 
		{
			memcpy(buf+total, vector[index].iov_base, vector[index].iov_len);
			total += vector[index].iov_len;
		}
		buf[total] = 0;
		HostPrintF("%s", buf);
		if (buf != stack_buf) free(buf);
	}
	
	return total;
}

status_t 
BWindowsOutputStream::Sync()
{
	return B_OK;
}

BWindowsInputStream::BWindowsInputStream()
{
}

BWindowsInputStream::~BWindowsInputStream()
{
}

ssize_t 
BWindowsInputStream::ReadV(const struct iovec* vector, ssize_t count)
{
	// Low-level debugging facility returns only one character
	// at a time.
	ssize_t amt = 0;
	if (count > 0 && vector->iov_len > 0) {
		int result = HostGetChar();
		if (result >= 0) {
			*(char*)(vector->iov_base) = result;
			amt = 1;
		}
		if (result == '\r') {
			// PalmSim doesn't move to the next line, so force it.
			printf("\n");
		}
		if (result == '\b') {
			// Likewise for backspace.
			printf(" \b");
		}
	}

	return amt;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
