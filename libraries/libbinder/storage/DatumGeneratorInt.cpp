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

#include <storage/DatumGeneratorInt.h>

#include <support/Autolock.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

/* The size_t value with all bits set */
#define MAX_SIZE_T           (~(size_t)0)

// *********************************************************************************
// ** SDatumGeneratorInt *******************************************************
// *********************************************************************************

SDatumGeneratorInt::SDatumGeneratorInt(uint32_t mode)
	: m_mode(mode)
	, m_lock("SDatumGeneratorInt")
	, m_datums(NULL)
{
}

SDatumGeneratorInt::SDatumGeneratorInt(const SContext& context, uint32_t mode)
	: m_context(context)
	, m_mode(mode)
	, m_lock("SDatumGeneratorInt")
	, m_datums(NULL)
{
}

SDatumGeneratorInt::~SDatumGeneratorInt()
{
	if (m_datums) delete m_datums;
}

lock_status_t SDatumGeneratorInt::Lock() const
{
	return m_lock.Lock();
}

void SDatumGeneratorInt::Unlock() const
{
	m_lock.Unlock();
}

sptr<IDatum> SDatumGeneratorInt::DatumAtLocked(size_t index)
{
	// If we don't already have the datum vector, create it now
	// because we know we will be putting one in it.
	if (!m_datums) m_datums = new SKeyedVector<size_t, wptr<IndexedDatum> >;
	if (!m_datums) return NULL;

	const wptr<IndexedDatum> weakdatum(m_datums->ValueFor(index));
	const sptr<IDatum> datum(weakdatum.promote().ptr());
	if (datum != NULL) return datum;

	sptr<IndexedDatum> newDatum(NewDatumLocked(index, m_mode));
	if (newDatum != NULL) {
		if (weakdatum != NULL) {
			// Someone is already here, but about to be removed.
			// We are now going to replace them, so let them know
			// that they no longer have this slot.
			weakdatum.unsafe_ptr_access()->m_index = B_ENTRY_NOT_FOUND;
		}
		m_datums->AddItem(index, newDatum);
		return newDatum.ptr();
	}

	// Error when creating datum -- delete datum vector if it is
	// empty.
	if (m_datums->CountItems() == 0) {
		delete m_datums;
		m_datums = NULL;
	}

	return NULL;
}

void SDatumGeneratorInt::UpdateDatumIndicesLocked(size_t index, ssize_t delta)
{
	if (!m_datums) return;
	ssize_t pos = m_datums->CeilIndexOf(index);
	if (pos < 0) return;

	size_t N(m_datums->CountItems());
	const size_t endDel=index-delta;
	while ((size_t)pos < N) {
		// Warning: nasty ugly const cast to avoid a lot of overhead!
		size_t& posIndex = const_cast<size_t&>(m_datums->KeyAt(pos));
		// NOTE: We know that all objects in the list are valid, because
		// they are removed as part of their destruction.  So we don't
		// need to do a promote here.  In fact, we don't WANT to do a
		// promote, because we absolutely need to update m_index even if
		// it is in the process of being destroyed.
		IndexedDatum* datum = m_datums->ValueAt(pos).unsafe_ptr_access();
		if (posIndex < index) {
			// This item's index hasn't changed.
			pos++;
		} else if (delta < 0 && posIndex < endDel) {
			// This item has been deleted.
			m_datums->RemoveItemsAt(pos);
			// should come up with a better error code.
			//! @todo Should we push something when the entry is deleted?
			if (datum != NULL) datum->m_index = B_ENTRY_NOT_FOUND;
			N--;
		} else {
			// This item's index has been moved.
			posIndex += delta;
			if (datum != NULL) datum->m_index = posIndex;
			pos++;
		}
	}
}

void SDatumGeneratorInt::SetValueAtLocked(size_t index, const SValue& value)
{
	SValue oldValue = ValueAtLocked(index);
	if (oldValue != value) {
		const uint32_t oldType = oldValue.IsDefined() ? oldValue.Type() : ValueTypeAtLocked(index);
		const off_t oldSize = oldValue.IsDefined() ? (off_t)oldValue.Length() : SizeAtLocked(index);
		oldValue.Undefine(); // no longer needed.
		const status_t err = StoreValueAtLocked(index, value);
		if (err == B_OK) {
			uint32_t changes = BStreamDatum::DATA_CHANGED;	// assume all data has changed; don't do memcmp().
			if (oldType != value.Type()) changes |= BStreamDatum::TYPE_CHANGED;
			if (oldSize != value.Length()) changes |= BStreamDatum::SIZE_CHANGED;
			ReportChangeAtLocked(index, NULL, changes, 0, oldSize >= value.Length() ? oldSize : value.Length());
		}
	}
}

uint32_t SDatumGeneratorInt::ValueTypeAtLocked(size_t index) const
{
	return ValueAtLocked(index).Type();
}

status_t SDatumGeneratorInt::StoreValueTypeAtLocked(size_t index, uint32_t type)
{
	// XXX NULL-terminate string values?
	SValue tmp(ValueAtLocked(index));
	status_t err = tmp.SetType(type);
	if (err == B_OK) {
		return StoreValueAtLocked(index, tmp);
	}
	return err;
}

off_t SDatumGeneratorInt::SizeAtLocked(size_t index) const
{
	return ValueAtLocked(index).Length();
}

status_t SDatumGeneratorInt::StoreSizeAtLocked(size_t index, off_t size)
{
	const size_t s = (size_t)size;
	if (s != size) return B_OUT_OF_RANGE;

	SValue tmp(ValueAtLocked(index));

	const size_t len = tmp.Length();
	void* d = tmp.BeginEditBytes(tmp.Type(), s, B_EDIT_VALUE_DATA);
	if (d) {
		if (s > len) memset(((char*)d)+len, 0, s-len);
		tmp.EndEditBytes(s);
		return StoreValueAtLocked(index, tmp);
	}

	return B_NO_MEMORY;
}

void SDatumGeneratorInt::ReportChangeAtLocked(size_t index, const sptr<IBinder>& editor, uint32_t changes, off_t start, off_t length)
{
	if (m_datums != NULL) {
		const sptr<IndexedDatum> datum(m_datums->ValueFor(index).promote());
		if (datum != NULL) {
			datum->BStreamDatum::ReportChangeLocked(editor, changes, start, length);
		}
	}
}

//!	The default implementation return uses ValueAtLocked() to retrieve the data.
const void* SDatumGeneratorInt::StartReadingAtLocked(size_t index, off_t position, ssize_t* inoutSize, uint32_t flags) const
{
	m_tmpValue = ValueAtLocked(index);

	const size_t len = m_tmpValue.Length();
	if (position > len) {
		*inoutSize = 0;
		return NULL;
	}

	const void* d = ((const char*)m_tmpValue.Data()) + (size_t)position;
	if (((size_t)position) + (*inoutSize) > len) {
		*inoutSize = len-((size_t)position);
	}

	return d;
}

void SDatumGeneratorInt::FinishReadingAtLocked(size_t index, const void* data) const
{
	m_tmpValue.Undefine();
}

void* SDatumGeneratorInt::StartWritingAtLocked(size_t index, off_t position, ssize_t* inoutSize, uint32_t flags)
{
	if ((position+*inoutSize) > MAX_SIZE_T) {
		*inoutSize = B_OUT_OF_RANGE;
		return NULL;
	}

	m_tmpValue = ValueAtLocked(index);

	const size_t wlen = (size_t)position + (size_t)*inoutSize;
	const size_t len = m_tmpValue.Length();
	m_writeLen = (len > wlen && (flags&B_WRITE_END) == 0) ? len : wlen;
	void* d = m_tmpValue.BeginEditBytes(m_tmpValue.Type(), m_writeLen, B_EDIT_VALUE_DATA);
	if (!d) {
		*inoutSize = m_tmpValue.IsSimple() ? B_NO_MEMORY : B_NOT_ALLOWED;
		return NULL;
	}

	return ((char*)d) + (size_t)position;
}

void SDatumGeneratorInt::FinishWritingAtLocked(size_t index, void* data)
{
	m_tmpValue.EndEditBytes(m_writeLen);
	StoreValueAtLocked(index, m_tmpValue);
	m_tmpValue.Undefine();
}

sptr<SDatumGeneratorInt::IndexedDatum> SDatumGeneratorInt::NewDatumLocked(size_t index, uint32_t mode)
{
	return new IndexedDatum(this->m_context, this, index, mode);
}

// =================================================================================
// =================================================================================
// =================================================================================

SDatumGeneratorInt::IndexedDatum::IndexedDatum(const SContext& context, const sptr<SDatumGeneratorInt>& owner, size_t index, uint32_t mode)
	: BStreamDatum(context, mode)
	, m_owner(owner)
	, m_index(index)
{
}

SDatumGeneratorInt::IndexedDatum::~IndexedDatum()
{
	SAutolock _l(Lock());
	if (m_index >= 0) {
		DbgOnlyFatalErrorIf(m_owner->m_datums == NULL, "SDatumGeneratorInt::IndexedDatum: destroyed after m_datums vector was deleted.");
		DbgOnlyFatalErrorIf(m_owner->m_datums->ValueFor(m_index) != this, "SDatumGeneratorInt::IndexedDatum: m_index is inconsistent with m_datums.");
		m_owner->m_datums->RemoveItemFor(m_index);
		if (m_owner->m_datums->CountItems() == 0) {
			delete m_owner->m_datums;
			m_owner->m_datums = NULL;
		}
	}
}

status_t SDatumGeneratorInt::IndexedDatum::FinishAtom(const void* )
{
	return FINISH_ATOM_ASYNC;
}

lock_status_t SDatumGeneratorInt::IndexedDatum::Lock() const
{
	return m_owner->Lock();
}

void SDatumGeneratorInt::IndexedDatum::Unlock() const
{
	m_owner->Unlock();
}

bool SDatumGeneratorInt::IndexedDatum::HoldRefForLink(const SValue& , uint32_t )
{
	return true;
}

uint32_t SDatumGeneratorInt::IndexedDatum::ValueTypeLocked() const
{
	return m_index >= 0 ? m_owner->ValueTypeAtLocked(m_index) : B_UNDEFINED_TYPE;
}

status_t SDatumGeneratorInt::IndexedDatum::StoreValueTypeLocked(uint32_t type)
{
	return m_index >= 0 ? m_owner->StoreValueTypeAtLocked(m_index, type) : m_index;
}

off_t SDatumGeneratorInt::IndexedDatum::SizeLocked() const
{
	return m_index >= 0 ? m_owner->SizeAtLocked(m_index) : 0;
}

status_t SDatumGeneratorInt::IndexedDatum::StoreSizeLocked(off_t size)
{
	return m_index >= 0 ? m_owner->StoreSizeAtLocked(m_index, size) : m_index;
}

SValue SDatumGeneratorInt::IndexedDatum::ValueLocked() const
{
	return m_index >= 0 ? m_owner->ValueAtLocked(m_index) : SValue::Undefined();
}

status_t SDatumGeneratorInt::IndexedDatum::StoreValueLocked(const SValue& value)
{
	return m_index >= 0 ? m_owner->StoreValueAtLocked(m_index, value) : m_index;
}

void SDatumGeneratorInt::IndexedDatum::ReportChangeLocked(const sptr<IBinder>& editor, uint32_t changes, off_t start, off_t length)
{
	if (m_index >= 0)
		m_owner->ReportChangeAtLocked(m_index, editor, changes, start, length);
}

const void* SDatumGeneratorInt::IndexedDatum::StartReadingLocked(const sptr<Stream>& stream, off_t position, ssize_t* inoutSize, uint32_t flags) const
{
	return m_index >= 0 ? m_owner->StartReadingAtLocked(m_index, position, inoutSize, flags) : NULL;
}

void SDatumGeneratorInt::IndexedDatum::FinishReadingLocked(const sptr<Stream>& stream, const void* data) const
{
	if (m_index >= 0) m_owner->FinishReadingAtLocked(m_index, data);
}

void* SDatumGeneratorInt::IndexedDatum::StartWritingLocked(const sptr<Stream>& stream, off_t position, ssize_t* inoutSize, uint32_t flags)
{
	return m_index >= 0 ? m_owner->StartWritingAtLocked(m_index, position, inoutSize, flags) : NULL;
}

void SDatumGeneratorInt::IndexedDatum::FinishWritingLocked(const sptr<Stream>& stream, void* data)
{
	if (m_index >= 0) m_owner->FinishWritingAtLocked(m_index, data);
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif
