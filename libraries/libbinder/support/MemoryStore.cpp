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

#include <support/MemoryStore.h>
#include <support/StdIO.h>

#include <stdlib.h>
#include <string.h>	// *cmp() functions
#include <stdio.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// ----------------------------------------------------------------- //

BMemoryStore::BMemoryStore() : BByteStream(this)
{
	m_data = NULL;
	m_length = 0;
	m_readOnly = false;
}

// ----------------------------------------------------------------- //

BMemoryStore::BMemoryStore(const BMemoryStore &o) : BnStorage(), BByteStream(this)
{
	m_data = NULL;
	m_length = 0;
	Copy(o);
}

// ----------------------------------------------------------------- //

BMemoryStore::BMemoryStore(void *data, size_t size) : BByteStream(this)
{
	m_data = (char*)data;
	m_length = size;
	m_readOnly = false;
}

// ----------------------------------------------------------------- //

BMemoryStore::BMemoryStore(const void *data, size_t size) : BByteStream(this)
{
	m_data = (char*)data;
	m_length = size;
	m_readOnly = true;
}

// ----------------------------------------------------------------- //

BMemoryStore::~BMemoryStore()
{
	Reset();
}

// ----------------------------------------------------------------- //

SValue BMemoryStore::Inspect(const sptr<IBinder>& caller, const SValue &v, uint32_t flags)
{
	return BnStorage::Inspect(caller, v, flags).Join(BByteStream::Inspect(caller, v, flags));
}

/*-------------------------------------------------------------*/

BMemoryStore &BMemoryStore::operator=(const BMemoryStore &o)
{
	if (this != &o) Copy(o);
	return *this;
}

// ----------------------------------------------------------------- //

void BMemoryStore::Reset()
{
	if (m_data) FreeCore(m_data);
	m_data = NULL;
	m_length = 0;
}

// ----------------------------------------------------------------- //

size_t BMemoryStore::BufferSize() const
{
	return m_length;
}

// ----------------------------------------------------------------- //

const void * BMemoryStore::Buffer() const
{
	return m_data;
}

// ----------------------------------------------------------------- //

status_t 
BMemoryStore::AssertSpace(size_t newSize)
{
	void *p = MoreCore(m_data,newSize);
	if (!p) return B_NO_MEMORY;
	m_data = (char*)p;
	return B_OK;
}

// ----------------------------------------------------------------- //

int32_t BMemoryStore::Compare(const BMemoryStore &o) const
{
	const void *obuf = o.Buffer();
	const size_t osize = static_cast<size_t>(o.Size());

	if (this == &o || m_data == obuf)
		return 0;
	if (m_data == NULL)
		return -1;
	if (o.m_data == NULL)
		return 1;
	const int cmp = memcmp(m_data, obuf, m_length <osize ? m_length : osize);
	if (cmp == 0 && m_length !=osize)
		return (m_length < osize ? -1 : 1);
	return cmp;
}

// ----------------------------------------------------------------- //

off_t 
BMemoryStore::Size() const
{
	return m_length;
}

// ----------------------------------------------------------------- //

ssize_t 
BMemoryStore::ReadAtV(off_t pos, const struct iovec *vector, ssize_t count)
{
	size_t size,totalSize = 0;

	while (--count >= 0) {
		if (pos >= static_cast<off_t>(m_length)) break;
		size = vector->iov_len;
		if (pos + size > m_length) size = static_cast<size_t>(m_length - pos);
		memcpy(vector->iov_base, m_data + pos, size);
		totalSize += size;
		pos += size;
		vector++;
	}

	return totalSize;
}

// ----------------------------------------------------------------- //

ssize_t 
BMemoryStore::WriteAtV(off_t pos, const struct iovec *vector, ssize_t count)
{
	size_t size,totalSize = 0;

	if (m_readOnly) return B_PERMISSION_DENIED;

	while (--count >= 0) {
		size = vector->iov_len;
		if (pos + size > m_length) {
			if ((pos+size) <= 0x7fffffff && AssertSpace(static_cast<size_t>(pos + size))) {
				return totalSize;
			}
			m_length = static_cast<size_t>(pos + size);
		}
		memcpy(m_data + pos, vector->iov_base, size);
		totalSize += size;
		pos += size;
		vector++;
	}

	return totalSize;
}

// ----------------------------------------------------------------- //

status_t BMemoryStore::Sync()
{
	return B_OK;
}

// ----------------------------------------------------------------- //

status_t BMemoryStore::SetSize(off_t size)
{
	if (m_readOnly) return B_PERMISSION_DENIED;

	const status_t result = (size <= 0x7fffffff ? AssertSpace(static_cast<size_t>(size)) : B_NO_MEMORY);
	if (result == B_OK) m_length = static_cast<size_t>(size);
	return result;
}
		
// ----------------------------------------------------------------- //

status_t BMemoryStore::Copy(const BMemoryStore &o)
{
	const void *obuf = o.Buffer();
	const size_t osize = static_cast<size_t>(o.Size());
	if (obuf && osize) {
		if (AssertSpace(osize)) return B_NO_MEMORY;
		m_length = osize;
		memcpy(m_data,obuf,osize);
	}
	return B_OK;
}

// ----------------------------------------------------------------- //

void * BMemoryStore::MoreCore(void *oldBuf, size_t newsize)
{
	if (newsize <= m_length) return oldBuf;
	return NULL;
}

void 
BMemoryStore::FreeCore(void *)
{
}

// ----------------------------------------------------------------- //

BMallocStore::BMallocStore()
{
	m_blockSize = 256;
	m_mallocSize = 0;
}

BMallocStore::BMallocStore(const BMemoryStore &o) : BMemoryStore(o)
{
	m_blockSize = 256;
	m_mallocSize = 0;
	Copy(o);
}

BMallocStore::~BMallocStore()
{
	Reset();
}

void BMallocStore::SetBlockSize(size_t newsize)
{
	m_blockSize = newsize;
}

void *BMallocStore::MoreCore(void *oldBuf, size_t size)
{
	if (size <= m_mallocSize) return oldBuf;

	char	*newdata;
	size_t	newsize;
	newsize = ((size + (m_blockSize-1)) / m_blockSize) * m_blockSize;
	newdata = (char *) realloc(oldBuf, newsize);
	if (!newdata) return NULL;
	m_mallocSize = newsize;
	return newdata;
}

void 
BMallocStore::FreeCore(void *oldBuf)
{
	free(oldBuf);
}


// ----------------------------------------------------------------- //

BValueStorage::BValueStorage(SValue* value, SLocker* lock, const sptr<SAtom>& object)
	:	BMemoryStore((void*)value->Data(), value->Length()),
		m_value(value),
		m_lock(lock),
		m_atom(object),
		m_editing(false)
{
}

BValueStorage::~BValueStorage()
{
	Reset();
}

SValue BValueStorage::Value() const
{
	SLocker::Autolock _l(*m_lock);
	return *m_value;
}

ssize_t BValueStorage::ReadAtV(off_t pos, const struct iovec *vector, ssize_t count)
{
	SLocker::Autolock _l(*m_lock);
	return BMemoryStore::ReadAtV(pos, vector, count);
}

ssize_t BValueStorage::WriteAtV(off_t pos, const struct iovec *vector, ssize_t count)
{
	SLocker::Autolock _l(*m_lock);
	return BMemoryStore::WriteAtV(pos, vector, count);
}

status_t BValueStorage::Sync()
{
	SLocker::Autolock _l(*m_lock);
	return BMemoryStore::Sync();
}

status_t BValueStorage::SetSize(off_t size)
{
	SLocker::Autolock _l(*m_lock);
	return BMemoryStore::SetSize(size);
}

void* BValueStorage::MoreCore(void *oldBuf, size_t size)
{
	if (oldBuf != NULL && size <= Size()) return oldBuf;

	if (oldBuf != NULL) m_value->EndEditBytes(Size());

	type_code type = m_value->Type();
	if (type == B_UNDEFINED_TYPE) type = B_RAW_TYPE;
	
	void* ptr = m_value->BeginEditBytes(type, size, B_EDIT_VALUE_DATA);
	if (ptr == NULL)
	{
		return NULL;
	}
	return ptr;
}

void BValueStorage::FreeCore(void* buf)
{
	m_value->EndEditBytes(Size());
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
