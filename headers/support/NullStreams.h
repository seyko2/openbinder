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

#ifndef	_SUPPORT_NULLSTREAMS_H
#define	_SUPPORT_NULLSTREAMS_H

/*!	@file support/NullStreams.h
	@ingroup CoreSupportDataModel
	@brief Byte streams that do nothing.
*/

#include <support/ByteStream.h>
#include <support/SupportDefs.h>
#include <support/ITextStream.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

class BNullStream : public BnByteOutput, public BnByteInput
{
public:
	BNullStream();
	
	virtual SValue Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags = 0);
	virtual	ssize_t WriteV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);
	virtual	status_t Sync();
	virtual	ssize_t ReadV(const struct iovec * /*vector*/, ssize_t /*count*/, uint32_t flags = 0);

protected:
	virtual ~BNullStream();

private:
	BNullStream(const BNullStream&);
	BNullStream& operator=(const BNullStream&);
};

class BNullTextOutput : public ITextOutput
{
public:
	BNullTextOutput();
	
	virtual	status_t				Print(	const char *debugText,
											ssize_t len = -1);
	
	//!	Adjust the current indentation level by \a delta amount.
	virtual void					MoveIndent(	int32_t delta);
	
	//!	Write a log line.
	/*!	Metadata about the text is supplied in \a info.  The text itself
		is supplied in the iovec, which is handled as one atomic unit.
		You can supply B_WRITE_END for \a flags to indicate the item
		being logged has ended. */
	virtual	status_t				LogV(	const log_info& info,
											const iovec *vector,
											ssize_t count,
											uint32_t flags = 0);

	virtual	void					Flush();
	virtual	status_t				Sync();

protected:
	virtual ~BNullTextOutput();

private:
	BNullTextOutput(const BNullTextOutput&);
	BNullTextOutput& operator=(const BNullTextOutput&);
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif // _SUPPORT_NULLSTREAMS_H
