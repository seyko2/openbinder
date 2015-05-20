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

#include <storage/ValueDatum.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

/* The size_t value with all bits set */
#define MAX_SIZE_T           (~(size_t)0)

// *********************************************************************************
// ** BValueDatum ******************************************************************
// *********************************************************************************

BValueDatum::BValueDatum(uint32_t mode)
	: BStreamDatum(SContext(), mode)
{
}

BValueDatum::BValueDatum(const SContext& context, uint32_t mode)
	: BStreamDatum(context, mode)
{
}

BValueDatum::BValueDatum(const SValue& value, uint32_t mode)
	: BStreamDatum(SContext(), mode)
	, m_value(value)
{
}

BValueDatum::BValueDatum(const SContext& context, const SValue& value, uint32_t mode)
	: BStreamDatum(context, mode)
	, m_value(value)
{
}

BValueDatum::~BValueDatum()
{
}

uint32_t BValueDatum::ValueTypeLocked() const
{
	return m_value.Type();
}

status_t BValueDatum::StoreValueTypeLocked(uint32_t valueType)
{
	// XXX NULL-terminate string values?
	return m_value.SetType(valueType);
}

off_t BValueDatum::SizeLocked() const
{
	return m_value.Length();
}

status_t BValueDatum::StoreSizeLocked(off_t size)
{
	// XXX NULL-terminate string values?

	const size_t s = (size_t)size;
	if (s != size) return B_OUT_OF_RANGE;

	const size_t len = m_value.Length();
	void* d = m_value.BeginEditBytes(m_value.Type(), s, B_EDIT_VALUE_DATA);
	if (d) {
		if (s > len) memset(((char*)d)+len, 0, s-len);
		m_value.EndEditBytes(s);
		return B_OK;
	}

	return B_NO_MEMORY;
}

SValue BValueDatum::ValueLocked() const
{
	return m_value;
}

status_t BValueDatum::StoreValueLocked(const SValue& value)
{
	m_value = value;
	return B_OK;
}

const void* BValueDatum::StartReadingLocked(const sptr<Stream>& /*stream*/, off_t position,
	ssize_t* inoutSize, uint32_t /*flags*/) const
{
	const size_t len = m_value.Length();
	if (position > len) {
		*inoutSize = 0;
		return NULL;
	}

	const void* d = ((const char*)m_value.Data()) + (size_t)position;
	if (((size_t)position) + (*inoutSize) > len) {
		*inoutSize = len-((size_t)position);
	}

	return d;
}

void BValueDatum::FinishReadingLocked(const sptr<Stream>& /*stream*/, const void* /*data*/) const
{
}

void* BValueDatum::StartWritingLocked(const sptr<Stream>& /*stream*/, off_t position,
	ssize_t* inoutSize, uint32_t flags)
{
	if ((position+*inoutSize) > MAX_SIZE_T) {
		*inoutSize = B_OUT_OF_RANGE;
		return NULL;
	}

	const size_t wlen = (size_t)position + (size_t)*inoutSize;
	const size_t len = m_value.Length();
	m_writeLen = (len > wlen && (flags&B_WRITE_END) == 0) ? len : wlen;
	void* d = m_value.BeginEditBytes(m_value.Type(), m_writeLen, B_EDIT_VALUE_DATA);
	if (!d) {
		*inoutSize = m_value.IsSimple() ? B_NO_MEMORY : B_NOT_ALLOWED;
		return NULL;
	}

	return ((char*)d) + (size_t)position;
}

void BValueDatum::FinishWritingLocked(const sptr<Stream>& /*stream*/, void* /*data*/)
{
	m_value.EndEditBytes(m_writeLen);
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
