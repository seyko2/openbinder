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

#include <storage/GenericDatum.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

// *********************************************************************************
// ** BGenericDatum ****************************************************************
// *********************************************************************************

BGenericDatum::BGenericDatum()
{
}

BGenericDatum::BGenericDatum(const SContext& context)
	: BnDatum(context)
{
}

BGenericDatum::~BGenericDatum()
{
}

enum {
	kCopySize = 3*1024
};

static status_t copy_datum(const sptr<IDatum>& src, const sptr<IDatum>& dst)
{
	uint8_t* buffer = (uint8_t*)malloc(kCopySize);
	if (buffer == NULL) return B_NO_MEMORY;

	const sptr<IByteInput> in(interface_cast<IByteInput>(src->Open(IDatum::READ_ONLY)));
	uint32_t type(src->ValueType());
	if (in == NULL) {
		free(buffer);
		return B_IO_ERROR;
	}

	const sptr<IByteOutput> out(interface_cast<IByteOutput>(dst->Open(IDatum::WRITE_ONLY|IDatum::ERASE_DATUM, NULL, type)));
	if (out == NULL) {
		free(buffer);
		return B_IO_ERROR;
	}

	ssize_t amt;
	size_t pos;
	while ((amt=in->Read(buffer, kCopySize)) > 0) {
		pos=0;
		do {
			const ssize_t written(out->Write(buffer+pos, amt));
			if (written < 0) {
				amt = written;
				goto finished;
			}
			amt -= written;
			pos += written;
		} while (amt > 0);
	}

	out->Sync();

finished:
	free(buffer);
	return amt;
}

status_t BGenericDatum::CopyTo(const sptr<IDatum>& dest, uint32_t flags)
{
	if (dest == NULL)
		return B_BAD_VALUE;

	if ((flags&NO_COPY_REDIRECTION) == 0)
		return dest->CopyFrom(this, flags|NO_COPY_REDIRECTION);

	// First try the more efficient approach of copying the value.
	SValue value(Value());
	ssize_t size(value.ArchivedSize());
	if (value.IsDefined() && size >= 0 && size < (3*1024)) {
		dest->SetValue(value);
		return B_OK;
	}

	// Do the slower approach of copying through byte streams.
	return copy_datum(this, dest);
}

status_t BGenericDatum::CopyFrom(const sptr<IDatum>& src, uint32_t flags)
{
	if (src == NULL)
		return B_BAD_VALUE;

	if ((flags&NO_COPY_REDIRECTION) == 0)
		return src->CopyTo(this, flags|NO_COPY_REDIRECTION);

	// First try the more efficient approach of copying the value.
	SValue value(src->Value());
	ssize_t size(value.ArchivedSize());
	if (value.IsDefined() && size >= 0 && size < (3*1024)) {
		SetValue(value);
		return B_OK;
	}

	// Do the slower approach of copying through byte streams.
	return copy_datum(src, this);
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
