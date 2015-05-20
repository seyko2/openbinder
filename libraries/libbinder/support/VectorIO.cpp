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

#include <support/Value.h>
#include <support/VectorIO.h>
#include <support/Vector.h>

#include <string.h>
#include <stdlib.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

SVectorIO::SVectorIO()
{
	m_heapVector = NULL;
	m_vectorCount = 0;
	m_totalLength = 0;
	m_localData.size = sizeof(m_localDataBuffer);
	m_localData.next_pos = 0;
	m_heapData = NULL;
}

SVectorIO::~SVectorIO()
{
	MakeEmpty();
}

const iovec* SVectorIO::Vectors() const
{
	return m_vectorCount <= MAX_LOCAL_VECTOR
		? m_localVector
		: (&m_heapVector->ItemAt(0));
}

ssize_t SVectorIO::CountVectors() const
{
	return m_vectorCount;
}

size_t SVectorIO::DataLength() const
{
	return m_totalLength;
}

void SVectorIO::MakeEmpty()
{
	if (m_heapVector) {
		delete m_heapVector;
		m_heapVector = NULL;
	}
	if (m_heapData) {
		const int32_t ND = m_heapData->CountItems();
		for (int32_t i=0; i<ND; i++) free(m_heapData->ItemAt(i));
		delete m_heapData;
		m_heapData = NULL;
	}
	
	m_vectorCount = 0;
	m_totalLength = 0;
	m_localData.size = sizeof(m_localDataBuffer);
	m_localData.next_pos = 0;
}

void SVectorIO::SetError(status_t err)
{
	m_vectorCount = err;
}

ssize_t SVectorIO::AddVector(void* base, size_t length)
{
	if (m_vectorCount < B_OK || length == 0) return m_vectorCount;
	
	const int32_t i = m_vectorCount++;
	if (i < MAX_LOCAL_VECTOR) {
		m_localVector[i].iov_base = base;
		m_localVector[i].iov_len = length;
	} else {
		if (!m_heapVector) {
			ssize_t result = B_OK;
			m_heapVector = new SVector<iovec>;
			if (m_heapVector) {
				m_heapVector->SetCapacity(MAX_LOCAL_VECTOR*2);
				for (int32_t j=0; j<MAX_LOCAL_VECTOR && result >= B_OK; j++)
					result = m_heapVector->AddItem(m_localVector[j]);
			} else {
				result = B_NO_MEMORY;
			}
			if (result < B_OK) {
				m_vectorCount = result;
				return result;
			}
		}
		iovec v;
		v.iov_base = base;
		v.iov_len = length;
		const ssize_t result = m_heapVector->AddItem(v);
		if (result < B_OK) {
			m_vectorCount = result;
			return result;
		}
	}
	
	m_totalLength += length;
	return i;
}

ssize_t SVectorIO::AddVector(const iovec* vector, size_t count)
{
	ssize_t pos = m_vectorCount;
	while (count-- > 0) {
		pos = AddVector(vector->iov_base, vector->iov_len);
		if (pos < B_OK) break;
		vector++;
	}
	return pos;
}

void* SVectorIO::Allocate(size_t length)
{
	ssize_t result = B_NO_MEMORY;
	alloc_data* ad = &m_localData;
	if ((ad->next_pos+length) > ad->size) {
		ad = NULL;
		if (!m_heapData) m_heapData = new SVector<alloc_data*>;
		if (m_heapData) {
			if (length < (MAX_LOCAL_DATA/2)) {
				const int32_t N = m_heapData->CountItems();
				ad = m_heapData->ItemAt(N-1);
				if ((ad->next_pos+length) > ad->size) {
					ad = static_cast<alloc_data*>(
						malloc(sizeof(alloc_data) + MAX_LOCAL_DATA));
					if (ad) result = m_heapData->AddItem(ad);
					if (result >= B_OK) {
						ad->size = MAX_LOCAL_DATA;
						ad->next_pos = 0;
					} else {
						free(ad);
						ad = NULL;
					}
				}
			}
			if (!ad) {
				ad = static_cast<alloc_data*>(
					malloc(sizeof(alloc_data) + length));
				if (ad) result = m_heapData->AddItemAt(ad, 0);
				if (result >= B_OK) {
					ad->size = length;
					ad->next_pos = 0;
				} else {
					free(ad);
					ad = NULL;
				}
			}
		}
	}
	
	if (ad) {
		void* data = reinterpret_cast<uint8_t*>(ad+1)+ad->next_pos;
		result = AddVector(data, length);
		if (result >= B_OK) {
			ad->next_pos += length;
			return data;
		}
	}
	
	m_vectorCount = result;
	return NULL;
}

ssize_t SVectorIO::Write(void* buffer, size_t avail) const
{
	const iovec* vec = Vectors();
	ssize_t count = CountVectors();
	size_t total = 0;
	while (--count >= 0 && total < avail) {
		total += vec->iov_len;
		if (total <= avail) memcpy(buffer, vec->iov_base, vec->iov_len);
		else {
			memcpy(buffer, vec->iov_base, vec->iov_len-(total-avail));
			total = avail;
		}
		buffer = static_cast<uint8_t*>(buffer) + vec->iov_len;
		vec++;
	}
	return total;
}

ssize_t SVectorIO::Read(const void* buffer, size_t avail) const
{
	const iovec* vec = Vectors();
	ssize_t count = CountVectors();
	size_t total = 0;
	while (--count >= 0 && total < avail) {
		total += vec->iov_len;
		if (total <= avail) memcpy(vec->iov_base, buffer, vec->iov_len);
		else {
			memcpy(vec->iov_base, buffer, vec->iov_len-(total-avail));
			total = avail;
		}
		buffer = static_cast<const uint8_t*>(buffer) + vec->iov_len;
		vec++;
	}
	return total;
}

status_t SVectorIO::_ReservedVectorIO1()	{ return B_UNSUPPORTED; }
status_t SVectorIO::_ReservedVectorIO2()	{ return B_UNSUPPORTED; }
status_t SVectorIO::_ReservedVectorIO3()	{ return B_UNSUPPORTED; }
status_t SVectorIO::_ReservedVectorIO4()	{ return B_UNSUPPORTED; }
status_t SVectorIO::_ReservedVectorIO5()	{ return B_UNSUPPORTED; }
status_t SVectorIO::_ReservedVectorIO6()	{ return B_UNSUPPORTED; }
status_t SVectorIO::_ReservedVectorIO7()	{ return B_UNSUPPORTED; }
status_t SVectorIO::_ReservedVectorIO8()	{ return B_UNSUPPORTED; }
status_t SVectorIO::_ReservedVectorIO9()	{ return B_UNSUPPORTED; }
status_t SVectorIO::_ReservedVectorIO10()	{ return B_UNSUPPORTED; }

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
