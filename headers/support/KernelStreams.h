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

#ifndef	_SUPPORT_KERNELSTREAMS_H
#define	_SUPPORT_KERNELSTREAMS_H

/*!	@file support/KernelStreams.h
	@ingroup CoreSupportDataModel
	@brief Byte stream to a file descriptor (not supported on Palm OS).
*/

#include <support/SupportDefs.h>
#include <support/ByteStream.h>
#include <sys/uio.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

/*---------------------------------------------------------------------*/

class BKernelOStr : public BnByteOutput
{
	public:
								BKernelOStr(int32_t descriptor);
		virtual					~BKernelOStr();
		
		virtual	ssize_t			WriteV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);
		virtual	status_t		Sync();

	private:

				int32_t			m_descriptor;
};

/*-------------------------------------------------------------*/

class BKernelIStr : public BnByteInput
{
	public:
								BKernelIStr(int32_t descriptor);
		virtual					~BKernelIStr();
		
		virtual	ssize_t			ReadV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);

	private:

				int32_t			m_descriptor;
};

/*-------------------------------------------------------------*/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_KERNELSTREAMS_H */
