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

#include <support/DatumLord.h>

static sptr<IBinder> ChooseByteStream(uint32_t mode, const sptr<IStorage> &storage)
{
	switch (mode & IDatum::READ_WRITE_MASK)
	{
		case IDatum::READ_ONLY:
			return static_cast<BnByteInput*>(new BReadOnlyStream(storage));
		case IDatum::WRITE_ONLY:
			return static_cast<BnByteOutput*>(new BWriteOnlyStream(storage));
		case IDatum::READ_WRITE:
			return static_cast<BnByteOutput*>(new BByteStream(storage));
	}
	return NULL;
}

// =================================================================
SDatumLord::SDatumLord()
	:m_lock("SDatumLord::m_lock"),
	 m_data()
{
}

SDatumLord::~SDatumLord()
{
}

void
SDatumLord::InitAtom()
{
}

status_t
SDatumLord::RequestDatum(const SValue &key, sptr<IDatum> *result)
{
	m_lock.Lock();
	*result = m_data.ValueFor(key).promote();
	if (*result == NULL) {
		*result = new Datum(this, key);
		m_data.AddItem(key, *result);
	}
	m_lock.Unlock();
	return B_OK;
}


// =================================================================
SDatumLord::Datum::Datum(const sptr<SDatumLord> &owner, const SValue &key)
	:m_owner(owner),
	 m_key(key)
{
}

SDatumLord::Datum::~Datum()
{
}

void
SDatumLord::Datum::InitAtom()
{
}

uint32_t
SDatumLord::Datum::ValueType() const
{
	SValue value;
	status_t err = m_owner->GetValue(m_key, &value);
	if (err == B_OK) {
		return value.Type();
	}
	return B_UNDEFINED_TYPE;
}

void
SDatumLord::Datum::SetValueType(uint32_t type)
{
	// XXX what is this supposed to do?
}

off_t
SDatumLord::Datum::Size() const
{
	SValue value;
	status_t err = m_owner->GetValue(m_key, &value);
	if (err == B_OK) {
		return value.Length();
	}
	return 0;
}

void
SDatumLord::Datum::SetSize(off_t size)
{
	// XXX what is this supposed to do?
}

SValue
SDatumLord::Datum::Value() const
{
	SValue value;
	status_t err = m_owner->GetValue(m_key, &value);
	if (err == B_OK) {
		return value;
	}
	return SValue::Undefined();
}

void
SDatumLord::Datum::SetValue(const SValue& value)
{
	m_owner->SetValue(m_key, value);
}

sptr<IBinder>
SDatumLord::Datum::Open(uint32_t mode, const sptr<IBinder>& editor, uint32_t /*newType*/)
{
	// XXX do we want to allow them to change the type?
	
	m_lock.Lock();

	sptr<IStorage> storage = m_storage.promote();
	if (storage == NULL) {

		m_lock.Unlock();

		SValue value;
		status_t err = m_owner->GetValue(m_key, &value);
		if (err != B_OK) {
			return NULL;
		}

		m_lock.Lock();
		
		storage = m_storage.promote();
		if (storage == NULL) {
			// if someone got in here while we had the lock open, the return
			// that one instead, but otherwise (and normally) create one and
			// return that
			
			// only allow access to simple values through open
			if (value.IsSimple()) {
				m_storageValue = value;
				storage = new Storage(&m_storageValue, &m_storageLock, this);
			}

			// if the value is simple, still set m_storage... it's possible that
			// it did hold a weak reference, and so even though we might re-set
			// it to NULL, we allow the atom to release the memory.  it's just
			// being a little bit nice... not really required.
			m_storage = storage;
		}
		
	}

	m_lock.Unlock();

	if (storage != NULL) {
		return ChooseByteStream(mode, storage);
	} else {
		return NULL;
	}
}

void
SDatumLord::Datum::StorageDone()
{
	m_lock.Lock();
	m_storage = NULL; // clear out the wptr (releasing the atom's memory)
	SValue value = m_storageValue;
	m_lock.Unlock();
	
	m_owner->SetValue(m_key, value);
}

// SDatumLord::Storage
// =================================================================
SDatumLord::Storage::Storage(SValue *value, SLocker *lock, const sptr<Datum> &owner)
	:BValueStorage(value, lock, NULL),
	 m_owner(owner)
{
}

SDatumLord::Storage::~Storage()
{
	m_owner->StorageDone();
}

