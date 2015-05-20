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

#include <support/Datum.h>

#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// =================================================================================

SDatum::SDatum()
{
}

SDatum::SDatum(const SValue& value)
	: m_value(value)
	, m_datum(interface_cast<IDatum>(value))
{
}

SDatum::SDatum(const sptr<IBinder>& binder)
	: m_datum(interface_cast<IDatum>(binder))
{
}

SDatum::SDatum(const sptr<IDatum>& datum)
	: m_datum(datum)
{
}

SDatum::SDatum(const SDatum& datum)
	: m_value(datum.m_value)
	, m_datum(datum.m_datum)
{
}

SDatum::~SDatum()
{
}

SDatum& SDatum::operator=(const SDatum& o)
{
	m_value = o.m_value;
	m_datum = o.m_datum;
	return *this;
}

bool SDatum::operator<(const SDatum& o) const
{
	return m_datum != NULL ? m_datum < o.m_datum : m_value < o.m_value;
}

bool SDatum::operator<=(const SDatum& o) const
{
	return m_datum != NULL ? m_datum <= o.m_datum : m_value <= o.m_value;
}

bool SDatum::operator==(const SDatum& o) const
{
	return m_datum != NULL ? m_datum == o.m_datum : m_value == o.m_value;
}

bool SDatum::operator!=(const SDatum& o) const
{
	return m_datum != NULL ? m_datum != o.m_datum : m_value != o.m_value;
}

bool SDatum::operator>=(const SDatum& o) const
{
	return m_datum != NULL ? m_datum >= o.m_datum : m_value >= o.m_value;
}

bool SDatum::operator>(const SDatum& o) const
{
	return m_datum != NULL ? m_datum > o.m_datum : m_value > o.m_value;
}

status_t SDatum::StatusCheck() const
{
	return m_datum != NULL ? B_OK : (m_value.IsSimple() ? B_NO_INIT : B_OK);
}

sptr<IDatum> SDatum::Datum() const
{
	return m_datum;
}

SValue SDatum::FetchValue(size_t maxSize, status_t* outError) const
{
	return do_fetch_value(maxSize, outError, false);
}

SValue SDatum::FetchTruncatedValue(size_t maxSize, status_t* outError) const
{
	return do_fetch_value(maxSize, outError, true);
}

sptr<IByteInput> SDatum::OpenInput(uint32_t mode, status_t* outError) const
{
	if (m_datum != NULL) {
		sptr<IBinder> b = m_datum->Open(mode);
		sptr<IByteInput> in(interface_cast<IByteInput>(b));
		if (outError) *outError = in != NULL ? B_OK : B_ERROR;
		return in;
	}

	if (outError) *outError = B_NO_INIT;
	return NULL;
}


sptr<IByteOutput> SDatum::OpenOutput(uint32_t mode, status_t* outError)
{
	return OpenOutput(mode, NULL, 0, outError);
}

sptr<IByteOutput> SDatum::OpenOutput(uint32_t mode, const sptr<IBinder>& editor, uint32_t newType, status_t* outError)
{
	if (m_datum != NULL) {
		sptr<IBinder> b = m_datum->Open(mode, editor, newType);
		sptr<IByteOutput> out(interface_cast<IByteOutput>(b));
		if (outError) *outError = out != NULL ? B_OK : B_ERROR;
		return out;
	}

	if (outError) *outError = B_NO_INIT;
	return NULL;
}

SValue SDatum::do_fetch_value(size_t maxSize, status_t* outError, bool truncate) const
{
	// If we aren't an IDatum object, then our value is just whatever
	// is in m_value.
	if (m_datum == NULL) {
		if (!m_value.IsObject()) {
			if (outError) *outError = m_value.IsDefined() ? B_OK : B_NO_INIT;
			return m_value;
		}
		if (outError) *outError = B_NO_INIT;
		return SValue::Undefined();
	}

	// Try to retrieve the value directly.
	SValue v = m_datum->Value();
	if (v.IsDefined()) {
		if (outError) *outError = B_OK;
		return v;
	}

	// We need to try to read the data in.
	off_t size = m_datum->Size();
	if (size > maxSize) {
		// Fail if we are not allowed to truncate the data.
		if (!truncate) {
			if (outError) *outError = B_OUT_OF_RANGE;
			return v;
		}
		size = maxSize;
	} else {
		// Not truncating, all is good!
		truncate = false;
	}

	// Open the datum for reading.
	const sptr<IByteInput> in(OpenInput(IDatum::READ_ONLY, outError));
	if (in == NULL) return v;

	// Start editing the return value.
	size_t s = (size_t)size;
	void* d = v.BeginEditBytes(m_datum->ValueType(), s);
	if (d == NULL) {
		if (outError) *outError = B_NO_MEMORY;
		return v;
	}

	// Read in the data.
	while (s > 0) {
		size_t amt = s > (3*1024) ? (3*1024) : s;
		ssize_t r = in->Read(d, amt);
		if (r < (ssize_t)amt) {
			if (r >= 0) r = B_IO_ERROR;
			v.EndEditBytes(0);
			if (outError) *outError = r;
			return SValue::Undefined();
		}
		s -= amt;
	}

	// All done!
	v.EndEditBytes();
	if (outError) *outError = truncate ? B_DATA_TRUNCATED : B_OK;
	return v;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
