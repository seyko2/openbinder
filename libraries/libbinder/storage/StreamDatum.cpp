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

#include <storage/StreamDatum.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

// =================================================================================

static ssize_t sum_iovec(const struct iovec *vector, ssize_t count)
{
	ssize_t total = 0;

	while (--count >= 0) {
		if (vector->iov_len > SSIZE_MAX) return B_OUT_OF_RANGE;
		total += vector->iov_len;
		if (total < 0) return B_OUT_OF_RANGE;
		vector++;
	}

	return total;
}

// *********************************************************************************
// ** BStreamDatum *****************************************************************
// *********************************************************************************

BStreamDatum::BStreamDatum(uint32_t mode)
	: m_lock("BStreamDatum")
	, m_mode(mode)
{
}

BStreamDatum::BStreamDatum(const SContext& context, uint32_t mode)
	: BGenericDatum(context)
	, m_lock("BStreamDatum")
	, m_mode(mode)
{
}

BStreamDatum::~BStreamDatum()
{
}

lock_status_t BStreamDatum::Lock() const
{
	return m_lock.Lock();
}

void BStreamDatum::Unlock() const
{
	m_lock.Unlock();
}

uint32_t BStreamDatum::RestrictMode(uint32_t requested, uint32_t limit)
{
	limit &= READ_WRITE_MASK;
	uint32_t newMode = requested&READ_WRITE_MASK;
	if (limit == READ_ONLY) {
		switch (newMode) {
			case WRITE_ONLY:	return 0xfffffff;
			case READ_WRITE:	newMode = READ_ONLY; break;
		}
	} else if (limit == WRITE_ONLY) {
		switch (newMode) {
			case READ_ONLY:		return 0xfffffff;
			case READ_WRITE:	newMode = WRITE_ONLY; break;
		}
	}

	return ( requested & (~READ_WRITE_MASK) ) | newMode;
}

uint32_t BStreamDatum::Mode() const
{
	return m_mode;
}

uint32_t BStreamDatum::ValueType() const
{
	SAutolock _l(Lock());
	return ValueTypeLocked();
}

void BStreamDatum::SetValueType(uint32_t valueType)
{
	SAutolock _l(Lock());

	const uint32_t oldType = ValueTypeLocked();
	if (oldType != valueType) {
		const status_t err = StoreValueTypeLocked(valueType);
		if (err == B_OK) {
			ReportChangeLocked(NULL, TYPE_CHANGED);
		}
	}
}

off_t BStreamDatum::Size() const
{
	SAutolock _l(Lock());
	return SizeLocked();
}

void BStreamDatum::SetSize(off_t size)
{
	SAutolock _l(Lock());
	const off_t oldSize = SizeLocked();
	if (oldSize != size) {
		const status_t err = StoreSizeLocked(size);
		if (err == B_OK) {
			size = SizeLocked();
			if (oldSize != size) {
				if (size > oldSize) {
					ReportChangeLocked(NULL, SIZE_CHANGED|DATA_CHANGED, oldSize, size-oldSize);
				} else {
					ReportChangeLocked(NULL, SIZE_CHANGED|DATA_CHANGED, size, oldSize-size);
				}
			}
		}
	}
}

SValue BStreamDatum::Value() const
{
	SAutolock _l(Lock());
	return ValueLocked();
}

void BStreamDatum::SetValue(const SValue& value)
{
	SAutolock _l(Lock());
	SValue oldValue = ValueLocked();
	if (oldValue != value) {
		oldValue.Undefine(); // no longer needed.
		const uint32_t oldType = ValueTypeLocked();
		const off_t oldSize = SizeLocked();
		const status_t err = StoreValueLocked(value);
		if (err == B_OK) {
			uint32_t changes = DATA_CHANGED;	// assume all data has changed; don't do memcmp().
			if (oldType != value.Type()) changes |= TYPE_CHANGED;
			if (oldSize != value.Length()) changes |= SIZE_CHANGED;
			ReportChangeLocked(NULL, changes, 0, oldSize >= value.Length() ? oldSize : value.Length());
		}
	}
}

sptr<IBinder> BStreamDatum::Open(uint32_t mode, const sptr<IBinder>& editor, uint32_t newType)
{
	mode = RestrictMode(mode, m_mode);
	if (mode == 0xfffffff) return NULL;

	sptr<Stream> stream = new Stream(Context(), this, mode, newType, editor);
	if (stream != NULL) return stream->DefaultInterface();
	return NULL;
}

status_t BStreamDatum::SetValueTypeLocked(uint32_t valueType)
{
	const uint32_t oldType = ValueTypeLocked();
	if (oldType != valueType) {
		const status_t err = StoreValueTypeLocked(valueType);
		if (err == B_OK) {
			ReportChangeLocked(NULL, TYPE_CHANGED);
		}
		return err;
	}
	return B_OK;
}

status_t BStreamDatum::SetSizeLocked(off_t size)
{
	const off_t oldSize = SizeLocked();
	if (oldSize != size) {
		const status_t err = StoreSizeLocked(size);
		if (err == B_OK) {
			size = SizeLocked();
			if (oldSize != size) {
				if (size > oldSize) {
					ReportChangeLocked(NULL, SIZE_CHANGED|DATA_CHANGED, oldSize, size-oldSize);
				} else {
					ReportChangeLocked(NULL, SIZE_CHANGED|DATA_CHANGED, size, oldSize-size);
				}
			}
		}
		return err;
	}
	return B_OK;
}

status_t BStreamDatum::SetValueLocked(const SValue& value)
{
	SValue oldValue = ValueLocked();
	if (oldValue != value) {
		oldValue.Undefine(); // no longer needed.
		const uint32_t oldType = ValueTypeLocked();
		const off_t oldSize = SizeLocked();
		const status_t err = StoreValueLocked(value);
		if (err == B_OK) {
			uint32_t changes = DATA_CHANGED;	// assume all data has changed; don't do memcmp().
			if (oldType != value.Type()) changes |= TYPE_CHANGED;
			if (oldSize != value.Length()) changes |= SIZE_CHANGED;
			ReportChangeLocked(NULL, changes, 0, oldSize >= value.Length() ? oldSize : value.Length());
		}
		return err;
	}
	return B_OK;
}

uint32_t BStreamDatum::ValueTypeLocked() const
{
	return B_RAW_TYPE;
}

status_t BStreamDatum::StoreValueTypeLocked(uint32_t /*type*/)
{
	return B_NOT_ALLOWED;
}

SValue BStreamDatum::ValueLocked() const
{
	const uint32_t type(ValueTypeLocked());
	const off_t size(SizeLocked());

	SValue val;

	// Don't be crazy!  This value happens to have
	// a relationship to the current Cobalt IPC limit.
	if (size <= 3*1024) {
		void* d = val.BeginEditBytes(type, (size_t)size);
		if (d) {
			iovec vec;
			vec.iov_len = (size_t)size;
			vec.iov_base = d;
			ssize_t s = ReadLocked(NULL, 0, &vec, 1, 0);
			val.EndEditBytes((size_t)size);
			if (s >= B_OK) goto done;

			val.SetError(s);
			goto done;
		}
		val.SetError(B_NO_MEMORY);
		goto done;
	}

	// Should find a better error code.
	val.SetError(B_OUT_OF_RANGE);

done:
	return val;
}

status_t BStreamDatum::StoreValueLocked(const SValue& value)
{
	status_t err = B_BAD_TYPE;

	const size_t size = value.Length();
	const void* d = value.Data();
	if (d) {
		err = StoreSizeLocked(size);
		if (err == B_OK) {
			iovec vec;
			vec.iov_len = size;
			vec.iov_base = (void*)d;
			ssize_t s = write_wrapper_l(NULL, 0, &vec, 1, 0);
			if (s >= B_OK) {
				// XXX It would be nice to do this before writing
				// the data (since WriteLocked() can unlock), but
				// we shouldn't change the type until we know
				// the write has succeeded.  Oh well.
				StoreValueTypeLocked(value.Type());
				goto done;
			}
			err = s;
		}
	}
	
done:
	return err;
}

void BStreamDatum::ReportChangeLocked(const sptr<IBinder>& editor, uint32_t changes, off_t start, off_t length)
{
	// Optimization: if nothing is linked to us, don't go through all the effort.
	if (!IsLinked()) return;

	PushValue(ValueLocked());

	if (changes&TYPE_CHANGED) PushValueType(ValueTypeLocked());
	if (changes&SIZE_CHANGED) PushSize(SizeLocked());
	if (changes&DATA_CHANGED) PushDatumChanged(this, editor, start, length);
}

ssize_t BStreamDatum::ReadLocked(const sptr<Stream>& stream, off_t position,
	const struct iovec *vector, ssize_t count, uint32_t flags) const
{
	ssize_t total = sum_iovec(vector, count);
	if (total < 0) return total;

	const void* data = StartReadingLocked(stream, position, &total, flags);
	if (!data) return 0;

	ssize_t remain = total;
	const char* pos = (const char*)data;
	while (--count >= 0) {
		if ((ssize_t)vector->iov_len <= remain) {
			memcpy(vector->iov_base, pos, vector->iov_len);
			pos += vector->iov_len;
			vector++;
			continue;
		}

		if (remain > 0) memcpy(vector->iov_base, pos, remain);
		break;
	}

	FinishReadingLocked(stream, data);

	return total;
}

ssize_t BStreamDatum::WriteLocked(const sptr<Stream>& stream, off_t position,
	const struct iovec *vector, ssize_t count, uint32_t flags)
{
	ssize_t total = sum_iovec(vector, count);
	if (total < 0) return total;

	void* data = StartWritingLocked(stream, position, &total, flags);
	if (!data) return B_NO_MEMORY;

	ssize_t remain = total;
	char* pos = (char*)data;
	while (--count >= 0) {
		if ((ssize_t)vector->iov_len <= remain) {
			memcpy(pos, vector->iov_base, vector->iov_len);
			pos += vector->iov_len;
			vector++;
			continue;
		}

		if (remain > 0) memcpy(pos, vector->iov_base, remain);
		break;
	}

	FinishWritingLocked(stream, data);

	return total;
}

status_t BStreamDatum::SyncLocked()
{
	return B_OK;
}

const void* BStreamDatum::StartReadingLocked(const sptr<Stream>& /*stream*/, off_t /*position*/,
	ssize_t* /*inoutSize*/, uint32_t /*flags*/) const
{
	DbgOnlyFatalError("BStreamDatum::StartReadingLocked() not implemented.");
	return NULL;
}

void BStreamDatum::FinishReadingLocked(const sptr<Stream>& /*stream*/, const void* /*data*/) const
{
}

void* BStreamDatum::StartWritingLocked(const sptr<Stream>& /*stream*/, off_t /*position*/,
	ssize_t* /*inoutSize*/, uint32_t /*flags*/)
{
	DbgOnlyFatalError("BStreamDatum::StartWritingLocked() not implemented.");
	return NULL;
}

void BStreamDatum::FinishWritingLocked(const sptr<Stream>& /*stream*/, void* /*data*/)
{
}

ssize_t BStreamDatum::write_wrapper_l(const sptr<Stream>& stream, off_t position,
	const struct iovec *vector, ssize_t count, uint32_t flags)
{
	const off_t oldSize = SizeLocked();
	const ssize_t total = WriteLocked(stream, position, vector, count, flags);

	if (total > 0) {
		uint32_t changes = DATA_CHANGED;
		const off_t newSize = SizeLocked();
		if (oldSize != newSize) changes |= SIZE_CHANGED;
		sptr<IBinder> editor;
		if (stream != NULL) editor = stream->Editor();
		ReportChangeLocked(editor, changes, position, total);
	}

	return total;
}

// =================================================================================
// =================================================================================
// =================================================================================

BStreamDatum::Stream::Stream(const SContext& context, const sptr<BStreamDatum>& datum,
	uint32_t mode, uint32_t type, const sptr<IBinder>& editor)
	: BnByteInput(context)
	, BnByteOutput(context)
	, BnByteSeekable(context)
	, m_datum(datum)
	, m_mode(mode)
	, m_type(type)
	, m_editor(editor)
{
}

BStreamDatum::Stream::~Stream()
{
	SAutolock _l(m_datum->Lock());
	m_datum->m_streams.RemoveItemFor(this);
}

void BStreamDatum::Stream::InitAtom()
{
	SAutolock _l(m_datum->Lock());

	m_datum->m_streams.AddItem(this);

	uint32_t changes = 0;
	off_t length = -1;

	// Handle special mode flags.
	if ((m_mode&IDatum::ERASE_DATUM) != 0) {
		length = m_datum->SizeLocked();
		if (length != 0) {
			if (m_datum->StoreSizeLocked(0) == B_OK) {
				changes = SIZE_CHANGED|DATA_CHANGED;
			}
		}
	}
	if ((m_mode&IDatum::OPEN_AT_END) != 0) m_pos = m_datum->SizeLocked();
	else m_pos = 0;

	// Change the datum type if so requested.
	if (m_type != 0) {
		if (m_datum->ValueTypeLocked() != m_type
				&& m_datum->StoreValueTypeLocked(m_type) == B_OK) {
			changes |= TYPE_CHANGED;
		}
	}

	// Report any change that happened.
	if (changes) {
		m_datum->ReportChangeLocked(m_editor, changes, 0, length);
	}
}

status_t BStreamDatum::Stream::FinishAtom(const void* )
{
	return FINISH_ATOM_ASYNC;
}

SValue BStreamDatum::Stream::Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags)
{
	SValue result(BnByteSeekable::Inspect(caller, which, flags));
	result.Join(BnStorage::Inspect(caller, which, flags));
	const uint32_t mode = m_mode&IDatum::READ_WRITE_MASK;
	if (mode == IDatum::READ_ONLY || mode == IDatum::READ_WRITE)
		result.Join(BnByteInput::Inspect(caller, which, flags));
	if (mode == IDatum::WRITE_ONLY || mode == IDatum::READ_WRITE)
		result.Join(BnByteOutput::Inspect(caller, which, flags));
	return result;
}

ssize_t BStreamDatum::Stream::ReadV(const struct iovec *vector, ssize_t count, uint32_t flags)
{
	SAutolock _l(m_datum->Lock());
	ssize_t amt = m_datum->ReadLocked(this, m_pos, vector, count, flags);
	if (amt > 0) m_pos += amt;
	return amt;
}

ssize_t BStreamDatum::Stream::WriteV(const struct iovec *vector, ssize_t count, uint32_t flags)
{
	SAutolock _l(m_datum->Lock());
	const off_t curPos = m_pos;
	ssize_t amt = m_datum->write_wrapper_l(this, m_pos, vector, count, flags);
	if (amt > 0) m_pos = curPos+amt;
	return amt;
}

status_t BStreamDatum::Stream::Sync()
{
	SAutolock _l(m_datum->Lock());
	return m_datum->SyncLocked();
}

off_t BStreamDatum::Stream::Seek(off_t position, uint32_t seek_mode)
{
	SAutolock _l(m_datum->Lock());

	if (seek_mode == SEEK_SET) return m_pos = position;
	if (seek_mode == SEEK_END) return m_pos = m_datum->SizeLocked() - position;
	if (seek_mode == SEEK_CUR) return m_pos += position;

	return m_pos;
}

off_t BStreamDatum::Stream::Position() const
{
	SAutolock _l(m_datum->Lock());
	return m_pos;
}

off_t BStreamDatum::Stream::Size() const
{
	SAutolock _l(m_datum->Lock());
	return m_datum->SizeLocked();
}

status_t BStreamDatum::Stream::SetSize(off_t size)
{
	SAutolock _l(m_datum->Lock());
	const uint32_t mode = m_mode&IDatum::READ_WRITE_MASK;
	if (mode == IDatum::WRITE_ONLY || mode == IDatum::READ_WRITE)
		return m_datum->SetSizeLocked(size);
	return B_PERMISSION_DENIED;
}

ssize_t BStreamDatum::Stream::ReadAtV(off_t position, const struct iovec *vector, ssize_t count)
{
	SAutolock _l(m_datum->Lock());
	const uint32_t mode = m_mode&IDatum::READ_WRITE_MASK;
	if (mode == IDatum::READ_ONLY || mode == IDatum::READ_WRITE)
		return m_datum->ReadLocked(this, position, vector, count, 0);
	return B_PERMISSION_DENIED;
}

ssize_t BStreamDatum::Stream::WriteAtV(off_t position, const struct iovec *vector, ssize_t count)
{
	SAutolock _l(m_datum->Lock());
	const uint32_t mode = m_mode&IDatum::READ_WRITE_MASK;
	if (mode == IDatum::WRITE_ONLY || mode == IDatum::READ_WRITE)
		return m_datum->WriteLocked(this, position, vector, count, 0);
	return B_PERMISSION_DENIED;
}

sptr<BStreamDatum> BStreamDatum::Stream::Datum() const
{
	return m_datum;
}

uint32_t BStreamDatum::Stream::Mode() const
{
	return m_mode;
}

uint32_t BStreamDatum::Stream::Type() const
{
	return m_type;
}

sptr<IBinder> BStreamDatum::Stream::Editor() const
{
	return m_editor;
}

sptr<IBinder> BStreamDatum::Stream::DefaultInterface() const
{
	const uint32_t mode = m_mode&IDatum::READ_WRITE_MASK;
	if (mode == IDatum::READ_ONLY || mode == IDatum::READ_WRITE)
		return (BnByteInput*)this;
	if (mode == IDatum::WRITE_ONLY)
		return (BnByteOutput*)this;
	return (BnByteSeekable*)this;
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
