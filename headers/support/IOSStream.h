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

#ifndef	_SUPPORT_IOSSTREAM_H
#define	_SUPPORT_IOSSTREAM_H

/*!	@file support/IOSStream.h
	@ingroup CoreSupportDataModel
	@brief Byte stream to an IOS device.
*/

#include <sys/uio.h>
#include <support/SupportDefs.h>
#include <support/ByteStream.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

/*---------------------------------------------------------------------*/

enum {
	B_IOSSTREAM_NO_AUTO_CLOSE		= 0x0001,
	B_IOSSTREAM_HIDE_INPUT			= 0x0002,
	B_IOSSTREAM_HIDE_OUTPUT			= 0x0004
};

//!	Byte input and output streams on an IOS device.
class BIOSStream : public BnByteOutput, public BnByteInput
{
public:
							BIOSStream(	int32_t descriptor, uint32_t flags = 0);
	virtual					~BIOSStream();
	
	virtual	SValue			Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags = 0);

	virtual	ssize_t			WriteV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);
	virtual	status_t		Sync();
	virtual	ssize_t			ReadV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);

	virtual	int32_t			FileDescriptor() const;

private:
			int32_t			m_descriptor;
			uint32_t		m_flags;
};

/*-------------------------------------------------------------*/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_IOSSTREAM_H */
