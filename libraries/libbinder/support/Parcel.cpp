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

#include <support/Parcel.h>

#include <support/ByteOrder.h>
#include <support/StdIO.h>
#include <support/Value.h>
#include <support/String.h>

#include <support_p/SupportMisc.h>
#include <support_p/ValueMapFormat.h>

#include "ValueInternal.h"

// Binder driver definitions.
#include <support_p/binder_module.h>

#include <ErrorMgr.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

// ======================================================================
// Macros
// ======================================================================

#ifndef BINDER_DEBUG_MSGS
#define BINDER_DEBUG_MSGS 0
#endif

#if BUILD_TYPE != BUILD_TYPE_RELEASE
#define BAD_THINGS_HAPPEN(expected)	ErrFatalError("Autobinder type mismatch. Expected " #expected ".")
#else
#define BAD_THINGS_HAPPEN(expected) return B_BAD_TYPE
#endif

#define CHECK_RETURNED_TYPE(type1, expected) if ((type1) != (expected)) BAD_THINGS_HAPPEN(expected)


#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#define USE_PARCEL_POOL 0
#define PARCEL_POOL_STATS 0

// Class Members
static SParcel*	g_parcel_pool = 0;
static size_t	g_parcel_pool_count = 0;
#define MAX_TRANSACTION_SIZE	4096
#define MAX_POOL_SIZE			16
#if PARCEL_POOL_STATS
static size_t	g_parcel_pool_hits = 0;
static size_t	g_parcel_pool_trys = 0;
static size_t	g_parcel_pool_high = 0;
#endif

parcel_pool_cleanup::~parcel_pool_cleanup()
{
	while (g_parcel_pool) {
		SParcel *p = g_parcel_pool;
		g_parcel_pool = *((SParcel **)p->EditData());
		delete p;
	}
#if PARCEL_POOL_STATS
	bout << SPrintf("SParcel pool: %lu/%lu = %2.6f, high water mark: %lu", g_parcel_pool_hits, g_parcel_pool_trys, (double)g_parcel_pool_hits/g_parcel_pool_trys, g_parcel_pool_high) << endl;
#endif
}

parcel_pool_cleanup::parcel_pool_cleanup()
{
}

static void standard_free(const void* data, ssize_t, void*)
{
	free(const_cast<void*>(data));
}

SParcel *
SParcel::GetParcel(void)
{
#if USE_PARCEL_POOL
	SParcel *p = 0;
	g_parcel_pool_lock.LockQuick();
#if PARCEL_POOL_STATS
		g_parcel_pool_trys++;
#endif
		if (g_parcel_pool) {
#if PARCEL_POOL_STATS
			g_parcel_pool_hits++;
#endif
			p = g_parcel_pool;
			g_parcel_pool = *((SParcel **)p->EditData());
			g_parcel_pool_count--;
		} else p = new SParcel(MAX_TRANSACTION_SIZE);
	g_parcel_pool_lock.Unlock();
	return p;
#else
	return new SParcel(-1);
#endif
}

void
SParcel::PutParcel(SParcel *parcel)
{
#if USE_PARCEL_POOL
	// We don't test g_parcel_pool_count atomically.  In theory, we could blow the
	// pool size limit, but practically speaking it shouldn't matter.
	if ((g_parcel_pool_count >= MAX_POOL_SIZE) || (!parcel->IsCacheable())) {
		//printf("Not Cached!\n");
		delete parcel;
		return;
	}

	//printf("Cached!\n");

	parcel->Reset();

	g_parcel_pool_lock.LockQuick();
		*((SParcel **)parcel->EditData()) = g_parcel_pool;
		g_parcel_pool = parcel;
		g_parcel_pool_count++;
#if PARCEL_POOL_STATS
		if (g_parcel_pool_count > g_parcel_pool_high) g_parcel_pool_high = g_parcel_pool_count;
#endif
	g_parcel_pool_lock.Unlock();
#else
	delete parcel;
#endif
}

SParcel::SParcel(ssize_t bufferSize)
	:	m_data(NULL), m_free(NULL), m_freeContext(NULL),
		m_reply(NULL), m_replyContext(NULL),
		m_dirty(false), m_ownsBinders(false)
{
	if (bufferSize < 0) bufferSize = sizeof(m_inline);
	if (bufferSize > 0) Reserve(bufferSize);
	else do_free();
}

SParcel::SParcel(sptr<IByteOutput> output, sptr<IByteInput> input,
	sptr<IByteSeekable> seek, ssize_t bufferSize)
	:	m_data(NULL), m_free(NULL), m_freeContext(NULL),
		m_reply(NULL), m_replyContext(NULL),
		m_out(output), m_in(input), m_seek(seek),
		m_dirty(false), m_ownsBinders(false)
{
	if (bufferSize < 0) bufferSize = sizeof(m_inline);
	if (bufferSize > 0) Reserve(bufferSize);
	else do_free();
}

SParcel::SParcel(reply_func replyFunc, void* replyContext)
	:	m_data(NULL), m_length(0), m_avail(0),
		m_free(NULL), m_freeContext(NULL),
		m_reply(replyFunc), m_replyContext(replyContext),
		m_base(0), m_pos(0),
		m_dirty(false), m_ownsBinders(false)
{
	Reserve(sizeof(m_inline));
}

SParcel::SParcel(const void* data, ssize_t len,
	free_func freeFunc, void* freeContext, reply_func replyFunc, void* replyContext)
	:	m_data(static_cast<uint8_t*>(const_cast<void*>(data))),
		m_length(len), m_avail(len),
		m_free(freeFunc), m_freeContext(freeContext),
		m_reply(replyFunc), m_replyContext(replyContext),
		m_base(0), m_pos(0),
		m_dirty(false), m_ownsBinders(false)
{
}

SParcel::~SParcel()
{
	Reply();
	do_free();
}

const void* SParcel::Data() const
{
	return m_data;
}

void* SParcel::EditData()
{
	return m_data;
}

ssize_t SParcel::Length() const
{
	return m_length;
}

ssize_t SParcel::Avail() const
{
	return m_avail;
}

// LINUX_DEMO_HACK from Cobalt... Maybe fixed with multiproc? --joeo
#define MAX_REF_TRANSFER 15   /* must be 2*n+1 */

ssize_t SParcel::AvailBinders() const
{
	// On linux we don't have an IPC size limit.
	return 10000;
}

status_t SParcel::ErrorCheck() const
{
	return m_length >= B_OK ? B_OK : m_length;
}

bool SParcel::IsCacheable() const
{
	return m_avail == MAX_TRANSACTION_SIZE && m_free == standard_free;
}

void SParcel::Reference(const void* data, ssize_t len, free_func freeFunc, void* context)
{
	do_free();
	m_data = static_cast<uint8_t*>(const_cast<void*>(data));
	m_length = m_avail = len;
	m_free = freeFunc;
	m_freeContext = context;
	m_base = 0;
	m_pos = 0;
}

status_t SParcel::Copy(const void* data, ssize_t len)
{
	void* d = Alloc(len);
	if (len > 0 && d) memcpy(d, data, len);
	return m_length >= 0 ? B_OK : m_length;
}

status_t SParcel::Copy(const SParcel& src)
{
	status_t result = Copy(src.Data(), src.Length());
	if (result == B_OK && src.m_binders.CountItems()) {
		m_binders = src.m_binders;
		m_ownsBinders = true;
		acquire_binders();
	}
	return result;
}

status_t SParcel::Reserve(ssize_t len)
{
	if (m_data && (m_avail >= len)) {
		Reset();
		return B_OK;
	}
	do_free();
	if (len <= (ssize_t)sizeof(m_inline)) {
		m_data = m_inline;
		m_avail = len;
		m_free = NULL;
	} else if ((m_data=static_cast<uint8_t*>(malloc(len))) != NULL) {
		m_avail = len;
		m_free = standard_free;
	} else {
		m_length = m_avail = B_NO_MEMORY;
		m_free = NULL;
		return B_NO_MEMORY;
	}
	return B_OK;
}

void* SParcel::Alloc(ssize_t len)
{
	status_t result = Reserve(len);
	m_length = result == B_OK ? len : result;
	return m_data;
}

void* SParcel::ReAlloc(ssize_t len)
{
	if (m_length < 0) {
		return NULL;
		
	} else if (len > m_avail) {
		if (len <= static_cast<ssize_t>(sizeof(m_inline))) {
			m_avail = len;
		} else {
			uint8_t* data = static_cast<uint8_t*>(malloc(len));
			if (data == NULL) return NULL;
			memcpy(data, m_data, m_length);
			if (m_free) {
				if (!m_ownsBinders && m_binders.CountItems() > 0) {
					// Previously didn't have references on the
					// binders, but now we need to.
					acquire_binders();
					m_ownsBinders = true;
				}
				m_free(m_data, m_avail, m_freeContext);
			}
			m_free = standard_free;
			m_data = data;
			m_avail = len;
		}
		if (len < m_pos) m_pos = len;
		m_length = len;
		return m_data;
	
	} else if (len < m_avail) {
		m_length = len;
		if (len < m_pos) m_pos = len;
	}
	
	return m_data;
}

void SParcel::Transfer(SParcel* src)
{
	if (src->m_data != src->m_inline) {
		// The data is stored outside of the buffer object.
		if (src->m_free || !src->m_data) {
			// The source buffer "owns" its data, so we can just
			// transfer ownership.
			do_free();
			m_data = src->m_data;
			m_length = src->m_length;
			m_avail = src->m_avail;
			m_free = src->m_free;
			m_freeContext = src->m_freeContext;
		} else {
			// The source buffer is only referencing its data, so someone
			// else will be deleting it.  In this case we must make a copy.
			Copy(src->m_data, src->m_length);
		}
	} else {
		// The data is stored inside of the buffer object -- just
		// copy it in to the new buffer.
		do_free();
		m_data = m_inline;
		m_length = src->m_length;
		m_avail = src->m_avail;
		m_free = NULL;
		if (src->m_length > 0) memcpy(m_inline, src->m_inline, src->m_length);
	}
	m_reply = src->m_reply;
	m_base = src->m_base;
	m_pos = src->m_pos;
	m_ownsBinders = src->m_ownsBinders;
	m_binders = src->m_binders;
	
	src->m_data = NULL;
	src->m_length = src->m_avail = 0;
	src->m_free = NULL;
	src->m_reply = NULL;
	src->m_base = 0;
	src->m_pos = 0;
	src->m_dirty = src->m_ownsBinders = false;
	src->m_binders.MakeEmpty();
}

status_t SParcel::SetValues(const SValue* value1, ...)
{
	SetBinderOffsets(NULL, 0);
	Reset();
	
	static const int32_t MAX_COUNT = 10;
	const SValue* values[MAX_COUNT];
	int32_t count = 0, i;
	ssize_t size = 0;
	
	//berr << "SParcel::SetValues {" << endl << indent;
	
	va_list vl;
	va_start(vl,value1);
	while (value1 && count < MAX_COUNT) {
		const ssize_t amt = value1->ArchivedSize();
		if (amt < B_OK) return (m_length=amt);
		//bout << "(" << amt << " bytes): " << *value1 << endl;
		size += amt;
		values[count++] = value1;
		value1 = va_arg(vl,SValue*);
	}
	va_end(vl);
	//berr << dedent << "}" << endl;

	Reserve(sizeof(int32_t)+size);
	if (m_length < B_OK) return m_length;
	SetPosition(0);
	
	WriteInt32(B_HOST_TO_LENDIAN_INT32(count));
	
#if BINDER_DEBUG_MSGS
	berr << "SParcel::SetValues {" << endl << indent;
#endif
	for (i=0;i<count;i++) {
		const ssize_t len = values[i]->Archive(*this);
		if (len < B_OK) {
			do_free();
			return (m_length=len);
		}
#if BINDER_DEBUG_MSGS
		berr
			<< "Flattened value to (" << ptr << "," << len << ") "
			<< indent
				<< SHexDump(ptr,len) << endl
				<< *values[i] << endl
			<< dedent;
#endif
	}
#if BINDER_DEBUG_MSGS
	berr << dedent << "}" << endl;
#endif

	return B_OK;
}

int32_t SParcel::CountValues() const
{
	ssize_t avail = Length();
	int32_t numValues = 0;
	if (Data() && avail > static_cast<ssize_t>(sizeof(int32_t))) {
		numValues = B_LENDIAN_TO_HOST_INT32(*reinterpret_cast<const int32_t*>(Data()));
	}
	return numValues;
}

int32_t SParcel::GetValues(int32_t maxCount, SValue* outValues) const
{
	int32_t i=0;
	if (ErrorCheck() != B_OK) {
		// Propagate errors.
		i = 1;
		outValues->SetError(ErrorCheck());
	} else {
		int32_t numValues = B_LENDIAN_TO_HOST_INT32(const_cast<SParcel*>(this)->ReadInt32());
		ssize_t tmp = B_OK;
		
		if (numValues > maxCount) numValues = maxCount;
		while (i < numValues && tmp >= B_OK) {
			tmp = outValues[i].Unarchive(*(const_cast<SParcel*>(this)));
#if BINDER_DEBUG_MSGS
			berr
				<< "Unflatten value from (" << m_data << "," << tmp << ") "
				<< indent
					<< SHexDump(buf,tmp) << endl
					<< outValues[i] << endl
				<< dedent;
#endif
			i++;
		}
	}
	
	return i;
}

bool SParcel::ReplyRequested() const
{
	return m_reply ? true : false;
}

status_t SParcel::Reply()
{
	const reply_func reply = m_reply;
	if (reply) {
		m_reply = NULL;
		return reply(*this, m_replyContext);
	}
	return B_NO_INIT;
}

void SParcel::Free()
{
	Reply();
	do_free();
}

off_t SParcel::Position() const
{
	return m_base+m_pos;
}

void SParcel::SetPosition(off_t pos)
{
	if (pos < m_base || pos > (m_base+m_length)) ErrFatalError("Not yet implemented");
	m_pos = (ssize_t)(pos-m_base);
}

status_t SParcel::SetLength(ssize_t len)
{
	if (len < 0) len = 0;
	if (len > m_avail) {
		if (ReAlloc(len) == NULL) return B_NO_MEMORY;
	}
	m_length = len;
	if (m_pos > m_length) m_pos = m_length;
	return B_OK;
}

ssize_t SParcel::finish_write(ssize_t amt)
{
	m_pos += amt;
	if (m_pos > m_length) m_length = m_pos;
	m_dirty = true;
	return amt;
}

ssize_t SParcel::Write(const void* buffer, size_t amount)
{
	if (ssize_t(m_pos+amount) <= m_avail) {
		// Common case: existing buffer can satisfy request.
		memcpy(m_data+m_pos, buffer, amount);
		return finish_write(amount);
	}
	
	size_t total;
	if (m_pos < m_length) {
		// Copy any data that will fit in to the buffer.
		total = m_length-m_pos;
		memcpy(m_data+m_pos, buffer, total);
		buffer = reinterpret_cast<const uint8_t*>(buffer) + total;
		amount -= total;
		m_pos = m_length;
		m_dirty = true;
	} else if (ErrorCheck() != B_OK) {
		// Return error state of buffer.
		return ErrorCheck();
	} else {
		total = 0;
	}
	
	// Now write out any pending data.
	const ssize_t written = Flush();
	if (written < B_OK) {
		return total > 0 ? total : written;
	}
	
	// If size being written is larger than our own buffer, then
	// write it directly.
	if (m_out != NULL && m_pos == 0 && (ssize_t)amount >= m_avail) {
		const ssize_t written = WriteBuffer(buffer, amount);
		if (written >= 0) return total+written;
		else if (total > 0) return total;
		return written;
	}
	
	// Place remaining data into buffer.
	const ssize_t need = amount+m_pos;
	if (need > m_avail) {
		// There isn't enough room in the buffer.  This is (always)
		// because the Write() didn't empty all of the buffer.  We'll
		// try to grow the buffer to contain more data.
		const ssize_t origLen = m_length;
		if (ReAlloc(need > ((m_avail*3)/2) ? need : ((m_avail*3)/2)) == NULL) {
			// Resize failed; just write as much as we have room for.
			amount = (m_avail-m_pos);
			if (amount == 0) {
				return B_NO_MEMORY;
			}
		}
		m_length = origLen;
	}
	memcpy(m_data+m_pos, buffer, amount);
	m_pos += amount;
	total += amount;
	if (m_length < m_pos) m_length = m_pos;
	m_dirty = true;
	
	// Return result.
	if (total > 0) return total;
	else if (written < 0) return written;
	else if (m_length < 0) return m_length;
	return 0;
}

void * SParcel::WriteInPlace(size_t amount)
{
	// XXX jham 8/19/05 This probably is wrong, since it doesn't take the IByteOutput thingy into account

	if (ssize_t(m_pos+amount) > m_avail) {
		// There isn't enough room in the buffer. We'll
		// try to grow the buffer to contain more data.
		const ssize_t origLen = m_length;
		if (ReAlloc(ssize_t(m_pos+amount) > ((m_avail*3)/2) ? (m_pos+amount) : ((m_avail*3)/2)) == NULL) {
			return NULL;
		}
		m_length = origLen;
	}

	// Common case: existing buffer can satisfy request.
	void * ptr = m_data+m_pos;
	finish_write(amount);
	return ptr;
}

ssize_t SParcel::WritePadded(const void* buffer, size_t amount)
{
	const size_t padded = value_data_align(amount);
	if (ssize_t(m_pos+padded) <= m_avail) {
		// Common case: existing buffer can satisfy request.
		memcpy(m_data+m_pos, buffer, amount);

		// Need to pad at end?
		if (padded != amount) {
#if CPU_ENDIAN == CPU_ENDIAN_BIG
			static const uint64_t mask[8] = {
				0x0000000000000000, 0xffffffffffffff00, 0xffffffffffff0000, 0xffffffffff000000,
				0xffffffff00000000, 0xffffff0000000000, 0xffff000000000000, 0xff00000000000000
			};
#else
			static const uint64_t mask[8] = {
				0x0000000000000000LL, 0x00ffffffffffffffLL, 0x0000ffffffffffffLL, 0x000000ffffffffffLL,
				0x00000000ffffffffLL, 0x0000000000ffffffLL, 0x000000000000ffffLL, 0x00000000000000ffLL
			};
#endif
			*reinterpret_cast<uint64_t*>(m_data+m_pos+padded-8) &= mask[padded-amount];
		}

		return finish_write(padded);
	}

	// Slow path...  simple and stupid.
	ssize_t written = Write(buffer, amount);
	if (written == ssize_t(amount)) {
		if (padded != amount) {
			static const uint64_t data = 0;
			written = Write(&data, padded-amount);
			return written == ssize_t(padded-amount) ? (padded) : (written < 0 ? written : B_ERROR);
		}
		return written;
	}

	return written < 0 ? written : B_ERROR;
}


ssize_t SParcel::WritePadding()
{
	const size_t padded = (value_data_align(m_pos) - m_pos);
	if (!padded) {
		// no padding necessary just now
		return 0;
	}

	if (ssize_t(m_pos+padded) <= m_avail) {
		// Common case: existing buffer can satisfy request.
#if CPU_ENDIAN == CPU_ENDIAN_BIG
		static const uint64_t mask[8] = {
			0x0000000000000000, 0xffffffffffffff00, 0xffffffffffff0000, 0xffffffffff000000,
			0xffffffff00000000, 0xffffff0000000000, 0xffff000000000000, 0xff00000000000000
		};
#else
		static const uint64_t mask[8] = {
			0x0000000000000000LL, 0x00ffffffffffffffLL, 0x0000ffffffffffffLL, 0x000000ffffffffffLL,
			0x00000000ffffffffLL, 0x0000000000ffffffLL, 0x000000000000ffffLL, 0x00000000000000ffLL
		};
#endif
		*reinterpret_cast<uint64_t*>(m_data+m_pos+padded-8) &= mask[padded];

		return finish_write(padded);
	}

	// Slow path...  simple and stupid.
	static const uint64_t data = 0;
	ssize_t written = Write(&data, padded);
	return written == ssize_t(padded) ? (padded) : (written < 0 ? written : B_ERROR);
}


ssize_t SParcel::WriteTypeHeader(type_code type, size_t amount)
{
	DbgOnlyFatalErrorIf(B_UNPACK_TYPE_CODE(type) != type, "Type code contains invalid bits");

	if (amount <= B_TYPE_LENGTH_MAX) {
		return WriteInt32(B_PACK_SMALL_TYPE(type, amount));
	}

	return WriteLargeData(B_PACK_LARGE_TYPE(type), amount, NULL);
}

ssize_t SParcel::WriteTypeHeaderAndData(type_code type, const void *data, size_t amount)
{
	if (amount <= B_TYPE_LENGTH_MAX) {
#if B_HOST_IS_LENDIAN
		return WriteSmallData(B_PACK_SMALL_TYPE(type, amount), *(uint32_t*)data);
#else
		DbgOnlyFatalError("Fix!");
#endif
	}

	return WriteLargeData(B_PACK_LARGE_TYPE(type), amount, data);
}

ssize_t SParcel::MarshalFixedData(type_code type, const void *data, size_t amount)
{
	if (data) return WriteTypeHeaderAndData(type, data, amount);
	return WriteSmallData(B_PACK_SMALL_TYPE(B_NULL_TYPE, 0), 0);
}

status_t SParcel::UnmarshalFixedData(type_code type, void *data, size_t amount)
{
	small_flat_data flat;
	ssize_t amt = ReadSmallData(&flat);

	if (amt == sizeof(flat)) {
		// See if we have a B_NULL_TYPE in the stream
		// if so, don't try to read more, that is the
		if (flat.type == B_PACK_SMALL_TYPE(B_NULL_TYPE, 0)) {
			return B_BINDER_READ_NULL_VALUE;
		}

		if (B_UNPACK_TYPE_CODE(flat.type) == type) {
			const size_t len = B_UNPACK_TYPE_LENGTH(flat.type);
			if (len <= B_TYPE_LENGTH_MAX) {
				memcpy(data, &flat.data, len);
				return len == amount ? B_OK : B_ERROR;
			}

			if (flat.data.uinteger == amount) {
				if ((amt=ReadPadded(data, amount)) >= B_OK) return B_OK;
			} else {
				amt = B_ERROR;
			}
		} else {
			amt = B_ERROR;
		}
	}

	return amt < B_OK ? amt : B_ERROR;
}

ssize_t SParcel::WriteBinder(const sptr<IBinder>& val)
{
	flat_binder_object data;
	flatten_binder(val, &data);
	return WriteBinder(data);
}

ssize_t SParcel::WriteWeakBinder(const wptr<IBinder>& val)
{
	flat_binder_object data;
	flatten_binder(val, &data);
	return WriteBinder(data);
}

ssize_t SParcel::WriteBinder(const small_flat_data& val)
{
	flat_binder_object data;
	data.type = B_PACK_LARGE_TYPE(B_UNPACK_TYPE_CODE(val.type));
	data.length = sizeof(flat_binder_object)-sizeof(large_flat_header);
	if ((data.binder=val.data.object) != NULL) {
		if (data.type == kPackedLargeBinderType) {
			data.cookie = static_cast<SAtom*>(static_cast<IBinder*>(data.binder));
		} else if (data.type == kPackedLargeBinderWeakType) {
			data.cookie = static_cast<SAtom::weak_atom_ptr*>(data.binder)->atom;
			data.binder = static_cast<SAtom::weak_atom_ptr*>(data.binder)->cookie;
		}
	}
	return WriteBinder(data);
}

ssize_t SParcel::WriteSmallData(uint32_t packedType, uint32_t packedData)
{
	if (ssize_t(m_pos+sizeof(small_flat_data)) <= m_avail) {
		small_flat_data* flat = reinterpret_cast<small_flat_data*>(m_data+m_pos);
		flat->type = packedType;
		flat->data.uinteger = packedData;
		return finish_write(sizeof(small_flat_data));
	}

	small_flat_data flat;
	flat.type = packedType;
	flat.data.uinteger = packedData;
	return Write(&flat, sizeof(flat));
}

ssize_t SParcel::WriteSmallData(const small_flat_data& flat)
{
	if (ssize_t(m_pos+sizeof(small_flat_data)) <= m_avail) {
		*reinterpret_cast<small_flat_data*>(m_data+m_pos) = flat;
		return finish_write(sizeof(small_flat_data));
	}

	return Write(&flat, sizeof(flat));
}

ssize_t SParcel::WriteLargeData(uint32_t packedType, size_t length, const void* data)
{
	DbgOnlyFatalErrorIf(sizeof(small_flat_data) != sizeof(large_flat_header), "Ooops!");
	ssize_t result = WriteSmallData(packedType, length);
	if (result == sizeof(large_flat_header)) {
		if (!data) return result;
		if ((result=WritePadded(data, length)) >= 0) return result + sizeof(large_flat_header);
	}

	return result >= 0 ? B_BAD_DATA : result;
}

ssize_t SParcel::WriteBinder(const flat_binder_object& val)
{
	ssize_t err;
	
	// Allocate enough for the binder in this parcel
	const ssize_t need = m_pos+sizeof(val);
	if (need > m_avail) {
		if ((err=Flush()) >= B_OK) {
			if (ReAlloc(need > ((m_avail*3)/2) ? need : ((m_avail*3)/2)) == NULL) {
				return B_NO_MEMORY;
			}
		} else {
			return err;
		}
	}
	
	*reinterpret_cast<flat_binder_object*>(m_data+m_pos) = val;
	
	if (val.binder == NULL) {
		// There is no need to write meta data for NULL binders.
		return finish_write(sizeof(val));
	}

	if (!m_ownsBinders && m_binders.CountItems() > 0) {
		acquire_binders();
	}
	m_ownsBinders = true;
	
	err = m_binders.AddItem(m_pos);
	if (err < B_OK) return err;

	acquire_object(val, this);

	return finish_write(sizeof(val));
}

ssize_t SParcel::WriteTypedData(type_code type, const void *value)
{
	// TypedDataSize, ReadTypedData, and WriteTypedData all
	// must deal with NULL pointers, and they all must agree
	// on reading/writing a B_NULL_TYPE
	// if value is NULL then we write a B_NULL_TYPE
	// into the parcel.
	if (type != B_UNDEFINED_TYPE && value == NULL) {
		type = B_NULL_TYPE;
	}

	switch (type)
	{
		case B_BOOL_TYPE:
		{
#if B_HOST_IS_LENDIAN
			return WriteSmallData(B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)), *reinterpret_cast<const bool*>(value));
#else
			DbgOnlyFatalError("Fix!");
#endif
		}

		case B_FLOAT_TYPE:
		{
			return WriteSmallData(B_PACK_SMALL_TYPE(B_FLOAT_TYPE, sizeof(float)), *reinterpret_cast<const uint32_t*>(value));
		}
		
		case B_DOUBLE_TYPE:
		{
			return WriteTypeHeaderAndData(B_DOUBLE_TYPE, value, sizeof(double));
		}
		
		case B_CHAR_TYPE:
		case B_INT8_TYPE:
		{
#if B_HOST_IS_LENDIAN
			return WriteSmallData(B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)), *reinterpret_cast<const int8_t*>(value));
#else
			DbgOnlyFatalError("Fix!");
#endif
		}

		case B_UINT8_TYPE:
		{
#if B_HOST_IS_LENDIAN
			return WriteSmallData(B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)), *reinterpret_cast<const uint8_t*>(value));
#else
			DbgOnlyFatalError("Fix!");
#endif
		}

		case B_INT16_TYPE:
		{
#if B_HOST_IS_LENDIAN
			return WriteSmallData(B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)), (int32_t)*reinterpret_cast<const int16_t*>(value));
#else
			DbgOnlyFatalError("Fix!");
#endif
		}

		case B_UINT16_TYPE:
		{
#if B_HOST_IS_LENDIAN
			return WriteSmallData(B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)), *reinterpret_cast<const uint16_t*>(value));
#else
			DbgOnlyFatalError("Fix!");
#endif
		}

		case B_INT32_TYPE:
		case B_SSIZE_T_TYPE:
		case B_UINT32_TYPE:
		case B_SIZE_T_TYPE:
		case B_WCHAR_TYPE:
		{
			return WriteSmallData(B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)), *reinterpret_cast<const uint32_t*>(value));
		}

		case B_INT64_TYPE:
		case B_UINT64_TYPE:
		case B_NSECS_TYPE:
		case B_BIGTIME_TYPE:
		case B_OFF_T_TYPE:
		{
			return WriteTypeHeaderAndData(B_INT64_TYPE, value, sizeof(int64_t));
		}
		
		case B_BINDER_TYPE:
		{
			return WriteBinder(*const_cast<sptr<IBinder>*>(reinterpret_cast<const sptr<IBinder>*>(value)));
		}
		
		case B_BINDER_WEAK_TYPE:
		{
			return WriteWeakBinder(*const_cast<wptr<IBinder>*>(reinterpret_cast<const wptr<IBinder>*>(value)));
		}
		
		case B_VALUE_TYPE:
			return reinterpret_cast<const SValue*>(value)->Archive(*this);
#if 0
		case B_CONSTCHAR_TYPE:
		{
			size_t size = strlen(reinterpret_cast<const char*>(value))+1;	// including null terminator
//			const size_t offset = (size <= B_TYPE_LENGTH_MAX)? 0 : sizeof(size_t);
			//char const *writeval = reinterpret_cast<const char*>(value);
			size_t w1;
			size_t w2;

			w1= WriteTypeHeader(B_CONSTCHAR_TYPE, size);
			w2= Write(reinterpret_cast<const char*>(value), size);

			if(w1> B_TYPE_LENGTH_MAX) {
                w2= value_data_align(w2);
			}
			return w1 + w2;
		}
#endif
		case B_CONSTCHAR_TYPE:
		case B_STRING_TYPE:
		{
			const SString *s = reinterpret_cast<const SString*>(value);
			size_t size = s->Length()+1;	// including null terminator
			return WriteTypeHeaderAndData(B_STRING_TYPE, s->String(), size);
		}

		case B_POINT_TYPE:
		{
			return WriteTypeHeaderAndData(B_POINT_TYPE, value, sizeof(float)*2);
		}
		
		case B_RECT_TYPE:
		{
			return WriteTypeHeaderAndData(B_RECT_TYPE, value, sizeof(float)*4);
		}
		
		case B_UNDEFINED_TYPE:
		{
			return WriteSmallData(B_PACK_SMALL_TYPE(B_UNDEFINED_TYPE, 0), 0);
		}

		case B_NULL_TYPE:
		{
			return WriteSmallData(B_PACK_SMALL_TYPE(B_NULL_TYPE, 0), 0);
		}

		default:
			DbgOnlyFatalError("Attempt to write unknown typed data to a parcel.");
			return B_BAD_TYPE;
	}	
}

ssize_t SParcel::ReadSmallData(small_flat_data* out)
{
	if (ssize_t(m_pos+sizeof(small_flat_data)) <= m_length) {
		*out = *reinterpret_cast<small_flat_data*>(m_data+m_pos);
		m_pos += sizeof(*out);
		return sizeof(*out);		
	}
	return Read(out, sizeof(*out));
}

ssize_t SParcel::ReadSmallDataOrObject(small_flat_data* out, const void* who)
{
	if (ssize_t(m_pos+sizeof(small_flat_data)) <= m_length) {
		*out = *reinterpret_cast<small_flat_data*>(m_data+m_pos);
		m_pos += sizeof(*out);
		if (!CHECK_IS_LARGE_OBJECT(out->type)) return sizeof(*out);		
	} else {
		ssize_t amt = Read(out, sizeof(*out));
		if (amt != sizeof(*out) || !CHECK_IS_LARGE_OBJECT(out->type)) return amt;
	}
	
	// Objects are always stored as flat_binder_object structures,
	// and at this point we have read the header for that structure.
	// Now we read the rest, and convert it to the public
	// small_flat_data representation used in user space.
	if (out->data.uinteger < (sizeof(flat_binder_object)-sizeof(small_flat_data))) {
		return B_BAD_TYPE;
	}
	if (ssize_t(m_pos+out->data.uinteger) <= m_length) {
		const flat_binder_object* obj
			= reinterpret_cast<const flat_binder_object*>(m_data+m_pos-sizeof(small_flat_data));
		const size_t dataSize = out->data.uinteger;
		m_pos += dataSize;
		if ((out->data.object=obj->binder) != NULL && out->type == kPackedLargeBinderWeakType) {
			// Weak binders are special -- we need to re-construct a weak_atom_ptr
			// from the large flattened representation.
			SAtom::weak_atom_ptr* weakp =
				static_cast<SAtom*>(obj->cookie)->CreateWeak(obj->binder);
			if (!weakp) return B_NO_MEMORY;
		}
		out->type = B_PACK_SMALL_TYPE(B_UNPACK_TYPE_CODE(out->type), sizeof(void*));
		acquire_object(*out, who);
		return sizeof(small_flat_data) + dataSize;
	}
	
	flat_binder_object obj;
	ssize_t amt = Read(&obj.binder, sizeof(flat_binder_object)-sizeof(small_flat_data));
	if (amt != sizeof(flat_binder_object)-sizeof(small_flat_data)) return amt < 0 ? amt : B_DATA_TRUNCATED;
	
	const size_t dataSize = out->data.uinteger;
	if ((out->data.object=obj.binder) != NULL && out->type == kPackedLargeBinderWeakType) {
		// Weak binders are special -- we need to re-construct a weak_atom_ptr
		// from the large flattened representation.
		SAtom::weak_atom_ptr* weakp =
			static_cast<SAtom*>(obj.cookie)->CreateWeak(obj.binder);
		if (!weakp) return B_NO_MEMORY;
	}
	out->type = B_PACK_SMALL_TYPE(B_UNPACK_TYPE_CODE(out->type), sizeof(void*));
	acquire_object(*out, who);
	if (dataSize > (sizeof(flat_binder_object)-sizeof(small_flat_data)))
		Drain(dataSize-(sizeof(flat_binder_object)-sizeof(small_flat_data)));
	return sizeof(small_flat_data) + dataSize;
}

ssize_t SParcel::ReadFlatBinderObject(flat_binder_object* out)
{
	if (ssize_t(m_pos+sizeof(flat_binder_object)) <= m_length) {
		*out = *reinterpret_cast<flat_binder_object*>(m_data+m_pos);
		m_pos += sizeof(*out);
		return sizeof(*out);
	}
	
	return Read(out, sizeof(*out));
}

status_t SParcel::ReadTypedData(type_code param_type, void* result)
{
	// TypedDataSize, ReadTypedData, and WriteTypedData all
	// must deal with NULL pointers, and they all must agree
	// on reading/writing a B_NULL_TYPE
	// Here we must deal with two issues:
	// 1. result can be NULL, in which case we don't do store
	// 2. contents of stream can be B_NULL_TYPE, in which case
	// we report that back to caller.

	ssize_t err;
	if (!result) {
		err = SkipValue();
		return err >= B_OK ? B_OK : err;
	}

	// Special types...
	switch (param_type)
	{
		case B_VALUE_TYPE: {
			SValue* resultValue = reinterpret_cast<SValue*>(result);
			ssize_t err = resultValue->Unarchive(*this);
			if (err >= 0 && *resultValue == B_NULL_VALUE)
				err = B_BINDER_READ_NULL_VALUE;
			return err >= 0 ? B_OK : err;
		}
		case B_BINDER_TYPE:
		case B_BINDER_WEAK_TYPE: {
			flat_binder_object flat;
			ssize_t amt=ReadFlatBinderObject(&flat);
			if (amt >= sizeof(flat_binder_object)) {
				if (param_type == B_BINDER_TYPE)
					return unflatten_binder(flat, reinterpret_cast<sptr<IBinder>*>(result));
				return unflatten_binder(flat, reinterpret_cast<wptr<IBinder>*>(result));
			}
			return amt < 0 ? amt : B_DATA_TRUNCATED;
		}
	}

	small_flat_data flat;
	ssize_t amt = ReadSmallData(&flat);

	if (amt == sizeof(flat)) {
		// See if we have a B_NULL_TYPE in the stream
		// if so, don't try to read more, that is the
		if (flat.type == B_PACK_SMALL_TYPE(B_NULL_TYPE, 0)) {
			return B_BINDER_READ_NULL_VALUE;
		}

		switch (param_type)
		{
			case B_BOOL_TYPE:
				CHECK_RETURNED_TYPE(flat.type, B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)));
				*reinterpret_cast<bool*>(result) = flat.data.integer ? true : false;
				return B_OK;
			
			case B_FLOAT_TYPE:
				CHECK_RETURNED_TYPE(flat.type, B_PACK_SMALL_TYPE(B_FLOAT_TYPE, sizeof(float)));
				*reinterpret_cast<float*>(result) = *reinterpret_cast<float*>(flat.data.bytes);
				return B_OK;
			
			case B_DOUBLE_TYPE:
				CHECK_RETURNED_TYPE(flat.type, B_PACK_LARGE_TYPE(B_FLOAT_TYPE));
				*reinterpret_cast<double*>(result) = ReadDouble();
				return B_OK;
			
			case B_CHAR_TYPE:
			case B_INT8_TYPE:
			case B_UINT8_TYPE:
				CHECK_RETURNED_TYPE(flat.type, B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)));
				*reinterpret_cast<uint8_t*>(result) = static_cast<uint8_t>(flat.data.uinteger);
				return B_OK;

			case B_INT16_TYPE:
			case B_UINT16_TYPE:
				CHECK_RETURNED_TYPE(flat.type, B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)));
				*reinterpret_cast<uint16_t*>(result) = static_cast<uint16_t>(flat.data.uinteger);
				return B_OK;

			case B_INT32_TYPE:
			case B_UINT32_TYPE:
			case B_SIZE_T_TYPE:
			case B_SSIZE_T_TYPE:
			case B_WCHAR_TYPE:
				CHECK_RETURNED_TYPE(flat.type, B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)));
				*reinterpret_cast<int32_t*>(result) = flat.data.integer;
				return B_OK;
			
			case B_INT64_TYPE:
			case B_UINT64_TYPE:
			case B_NSECS_TYPE:
			case B_BIGTIME_TYPE:
			case B_OFF_T_TYPE:
				CHECK_RETURNED_TYPE(flat.type, B_PACK_LARGE_TYPE(B_INT64_TYPE));
				*reinterpret_cast<int64_t*>(result) = ReadInt64();
				return B_OK;

#if 0
			case B_CONSTCHAR_TYPE:
			{
				CHECK_RETURNED_TYPE(B_UNPACK_TYPE_CODE(flat.type), B_CONSTCHAR_TYPE);
				      size_t len = B_UNPACK_TYPE_LENGTH(flat.type);
					  const size_t offset = (len <= B_TYPE_LENGTH_MAX) ? - sizeof(size_t) : 0;
				char const *retval = offset+reinterpret_cast<const char*>(m_data)+m_pos;

				if(len > B_TYPE_LENGTH_MAX) {
					len= flat.data.uinteger;
					Drain(value_data_align(len));
				} else {
				}

				size_t idx= len;
				while(idx) {
					idx-= 1;
					if(!retval[idx]) {
						*reinterpret_cast<const char**>(result)= retval;
						return B_OK;
					}
				}
				*reinterpret_cast<const char**>(result)= (char const *)(0x00000FE0); // Ugly!
				return B_ERROR; // no NULL terminator... someone is living dangerously
			} break;
#endif
			case B_CONSTCHAR_TYPE:
			case B_STRING_TYPE:
				{
				CHECK_RETURNED_TYPE(B_UNPACK_TYPE_CODE(flat.type), B_STRING_TYPE);
				const size_t len = B_UNPACK_TYPE_LENGTH(flat.type);
				SString *s = reinterpret_cast<SString*>(result);
				if (len <= B_TYPE_LENGTH_MAX) {
					char *buff = s->LockBuffer(len-1);
					memcpy(buff, flat.data.bytes, len);
					s->UnlockBuffer(len-1);
					return B_OK;
				}

				const size_t size = flat.data.uinteger;	// avoid aliased 'head'.

				char *buff = s->LockBuffer(size);
				amt = ReadPadded(buff, size);
				s->UnlockBuffer(size-1);
				if (amt >= ssize_t(size)) return B_OK;
			} break;

			case B_POINT_TYPE:
				CHECK_RETURNED_TYPE(flat.type, B_PACK_LARGE_TYPE(B_POINT_TYPE));
				amt = Read(result, sizeof(float)*2);
				if (amt == sizeof(float)*2) return B_OK;

			case B_RECT_TYPE:
				CHECK_RETURNED_TYPE(flat.type, B_PACK_LARGE_TYPE(B_RECT_TYPE));
				amt = Read(result, sizeof(float)*4);
				if (amt == sizeof(float)*4) return B_OK;
				return B_OK;

			case B_UNDEFINED_TYPE:
				return B_OK;
			
			default:
				DbgOnlyFatalError("Attempt to read unknown typed data from a parcel.");
				return B_BAD_TYPE;
		}
	}

	DbgOnlyFatalError("Error trying to unpack autobinder parcel!");

	return amt < 0 ? amt : B_BAD_DATA;
}

ssize_t SParcel::TypedDataSize(type_code type, const void *value)
{
	// TypedDataSize, ReadTypedData, and WriteTypedData all
	// must deal with NULL pointers, and they all must agree
	// on reading/writing a B_NULL_TYPE
	// if value is NULL then we write B_NULL_TYPE into parcel
	if (type != B_UNDEFINED_TYPE && value == NULL) {
		type = B_NULL_TYPE;
	}
	switch (type)
	{
		case B_BOOL_TYPE:
		case B_FLOAT_TYPE:
		case B_CHAR_TYPE:
		case B_INT8_TYPE:
		case B_UINT8_TYPE:
		case B_INT16_TYPE:
		case B_UINT16_TYPE:
		case B_INT32_TYPE:
		case B_UINT32_TYPE:
		case B_SIZE_T_TYPE:
		case B_SSIZE_T_TYPE:
		case B_WCHAR_TYPE:
			return sizeof(small_flat_data);
		
		case B_DOUBLE_TYPE:
			return sizeof(large_flat_header) + sizeof(double);
		
		case B_INT64_TYPE:
		case B_UINT64_TYPE:
		case B_NSECS_TYPE:
		case B_BIGTIME_TYPE:
		case B_OFF_T_TYPE:
			return sizeof(large_flat_header) + sizeof(int64_t);

		case B_BINDER_TYPE:
		case B_BINDER_WEAK_TYPE:
			return sizeof(flat_binder_object);
		
		case B_VALUE_TYPE:
			return reinterpret_cast<const SValue*>(value)->ArchivedSize();
#if 0
		case B_CONSTCHAR_TYPE:
		{   const size_t len = strlen(reinterpret_cast<const char*>(value))+1;
			if (len <= B_TYPE_LENGTH_MAX) return sizeof(small_flat_data);
			return sizeof(large_flat_header) + value_data_align(len);
		}
#endif
		case B_CONSTCHAR_TYPE:
		case B_STRING_TYPE:
		{
			// must include null terminator
			const size_t len = reinterpret_cast<const SString*>(value)->Length()+1;
			if (len <= B_TYPE_LENGTH_MAX) return sizeof(small_flat_data);
			return sizeof(large_flat_header) + value_data_align(len);
		} break;

		case B_UNDEFINED_TYPE:
		case B_NULL_TYPE:
			return sizeof(small_flat_data);
		
		default:
			return B_BAD_TYPE;
	}	
}

ssize_t SParcel::WriteValue(const SValue& value)
{
	return value.Archive(*this);
}

SValue SParcel::ReadValue()
{
	SValue value;
	value.Unarchive(*this);
	return value;
}

status_t SParcel::SkipValue()
{
	if (ssize_t(m_pos+sizeof(small_flat_data)) <= m_length) {
		const small_flat_data* flat = reinterpret_cast<small_flat_data*>(m_data+m_pos);
		m_pos += sizeof(small_flat_data);
		if (B_UNPACK_TYPE_LENGTH(flat->type) <= B_TYPE_LENGTH_MAX) {
			return B_OK;
		}

		return Drain((flat->data.uinteger+7) & ~7);
	}
	
	small_flat_data flat;
	ssize_t amt = Read(&flat, sizeof(flat));
	if (amt == sizeof(flat)) {
		if (B_UNPACK_TYPE_LENGTH(flat.type) <= B_TYPE_LENGTH_MAX) {
			return B_OK;
		}

		return Drain((flat.data.uinteger+7) & ~7);
	}

	return amt < B_OK ? amt : B_ERROR;
}

ssize_t SParcel::Flush()
{
	if (m_out == NULL) {
		return 0;
	}

	ssize_t written = WriteBuffer(m_data, m_length);
	if (written < m_length) {
		if (written < B_OK) return written;
		m_length -= written;
		m_base += written;
		m_pos = m_length;
		memcpy(m_data, m_data+written, m_length);
	} else {
		m_base += written;
		m_length = m_pos = 0;
	}
	m_dirty = false;
	return written;
}

ssize_t SParcel::Sync()
{
	ssize_t error = Flush();
	if (error >= 0) error = (m_out != NULL) ? m_out->Sync() : B_OK;
	return error;
}

ssize_t SParcel::WriteBool(bool val)
{
	if (ssize_t(m_pos+sizeof(val)) <= m_avail) {
		*reinterpret_cast<int8_t*>(m_data+m_pos) = val;
		return finish_write(sizeof(val));
	}
	return Write(&val, sizeof(val));
}

ssize_t SParcel::WriteInt8(int8_t val)
{
	if (ssize_t(m_pos+sizeof(val)) <= m_avail) {
		*reinterpret_cast<int8_t*>(m_data+m_pos) = val;
		return finish_write(sizeof(val));
	}
	return Write(&val, sizeof(val));
}

ssize_t SParcel::WriteInt16(int16_t val)
{
	if (ssize_t(m_pos+sizeof(val)) <= m_avail) {
		*reinterpret_cast<int16_t*>(m_data+m_pos) = val;
		return finish_write(sizeof(val));
	}
	return Write(&val, sizeof(val));
}

ssize_t SParcel::WriteInt32(int32_t val)
{
	if (ssize_t(m_pos+sizeof(val)) <= m_avail) {
		*reinterpret_cast<int32_t*>(m_data+m_pos) = val;
		return finish_write(sizeof(val));
	}
	return Write(&val, sizeof(val));
}

ssize_t SParcel::WriteInt64(int64_t val)
{
	if (ssize_t(m_pos+sizeof(val)) <= m_avail) {
		*reinterpret_cast<int64_t*>(m_data+m_pos) = val;
		return finish_write(sizeof(val));
	}
	return Write(&val, sizeof(val));
}

ssize_t SParcel::WriteFloat(float val)
{
	if (ssize_t(m_pos+sizeof(val)) <= m_avail) {
		*reinterpret_cast<float*>(m_data+m_pos) = val;
		return finish_write(sizeof(val));
	}
	return Write(&val, sizeof(val));
}

ssize_t SParcel::BinderSize(const sptr<IBinder>& /*val*/)
{
	return sizeof(flat_binder_object);
}

ssize_t SParcel::WeakBinderSize(const wptr<IBinder>& /*val*/)
{
	return sizeof(flat_binder_object);
}

#if TARGET_HOST == TARGET_HOST_PALMOS
ssize_t
SParcel::WriteKeyID(const sptr<SKeyID>& val)
{
	small_flat_data data;
	data.type = B_PACK_SMALL_TYPE(B_KEY_ID_TYPE, sizeof(void*));
	data.data.object = reinterpret_cast<void*>(&(*val));

	return WriteBinder(data);
}

sptr<SKeyID>
SParcel::ReadKeyID()
{
	small_flat_data flat;
	sptr<SKeyID> val;
	if (ReadSmallData(&flat) == sizeof(flat))
	{
		if (flat.type == B_PACK_SMALL_TYPE(B_KEY_ID_TYPE, sizeof(void*)))
			val = static_cast<SKeyID*>(flat.data.object);
	}
	return val;
}
#endif


ssize_t SParcel::Read(void* buffer, size_t amount)
{
	if (ssize_t(m_pos+amount) <= m_length) {
		// Common case: existing buffer can satisfy request.
		memcpy(buffer, m_data+m_pos, amount);
		m_pos += amount;
		return amount;
	}
	
	ssize_t total;
	if (m_pos < m_length) {
		// Drain any remaining data from buffer.
		total = m_length-m_pos;
		memcpy(buffer, m_data+m_pos, total);
		buffer = reinterpret_cast<uint8_t*>(buffer) + total;
		amount -= total;
		m_pos = m_length;
	} else if (ErrorCheck() != B_OK) {
		// Return error state of buffer.
		return ErrorCheck();
	} else {
		total = 0;
	}
	
	// If size being read is larger than our own buffer, then
	// read it directly.
	if ((ssize_t)amount >= m_avail) {
		ssize_t read = ReadBuffer(buffer, amount);
		if (read >= 0) {
			m_base += read;
			return total+read;
		} else if (total > 0) return total;
		return read;
	}
	
	// Now refresh our buffer.
	m_length = ReadBuffer(m_data, m_avail);
	if (m_length >= B_OK) {
		m_base += m_pos;
		if (m_length < (ssize_t)amount) amount = m_length;
		memcpy(buffer, m_data, amount);
		m_pos = amount;
		return total + amount;
	}
	
	m_pos = 0;
	return total > 0 ? total : m_length;
}

void const * SParcel::ReadInPlace(size_t amount)
{
	if (ssize_t(m_pos + amount) <= m_length) {
		void const * ptr = m_data + m_pos;
		m_pos += amount;
		return ptr;
	} else {
		return NULL;
	}
}

ssize_t SParcel::ReadPadded(void* buffer, size_t amount)
{
	const size_t padded = value_data_align(amount);
	if (ssize_t(m_pos+padded) <= m_length) {
		// Common case: existing buffer can satisfy request.
		memcpy(buffer, m_data+m_pos, amount);
		m_pos += padded;
		return padded;
	}

	ssize_t amt = Read(buffer, amount);
	if (amt == ssize_t(amount)) {
		if (amount != padded) {
			const size_t bytes = padded-amount;
			amt = Drain(bytes);
			return amt == ssize_t(bytes) ? padded : (amt < 0 ? amt : B_BAD_DATA);
		}
		return amt;
	}

	return amt < 0 ? amt : B_BAD_DATA;
}

ssize_t SParcel::Drain(size_t amount)
{
	if (ssize_t(m_pos+amount) <= m_length) {
		// Common case: existing buffer can satisfy request.
		m_pos += amount;
		return amount;
	}
	
	if (ErrorCheck() != B_OK) return ErrorCheck();
	
	#if 0
	// ATTEMPT SEEK HERE
	// Skip over remaining number of bytes.
	m_base += amount-(m_length-m_pos);
	m_pos = m_length;
	#endif
	ssize_t sofar = m_length-m_pos;
	while (sofar < (ssize_t)amount) {
		m_base += m_length;
		m_length = ReadBuffer(m_data, m_avail);
		if (m_length < B_OK) {
			return m_length;
		} else if (m_length == 0) {
			break;
		}
		sofar += m_length;
	}
	
	if (sofar >= (ssize_t)amount) {
		m_pos = sofar-amount;
		return amount;
	}
	m_pos = m_length;
	return sofar;
}

ssize_t SParcel::DrainPadding()
{
	const size_t padded = (value_data_align(m_pos) - m_pos);
	if (!padded) {
		// no padding at current position
		return 0;
	}

	return Drain(padded);
}

bool SParcel::ReadBool()
{
	if (ssize_t(m_pos+sizeof(int8_t)) <= m_length) {
		const int8_t* it = reinterpret_cast<int8_t*>(m_data+m_pos);
		m_pos += sizeof(int8_t);
		return (*it) ? true : false;
	}
	
	bool val;
	Read(&val, sizeof(val));
	return val;
}

int8_t SParcel::ReadInt8()
{
	if (ssize_t(m_pos+sizeof(int8_t)) <= m_length) {
		const int8_t* it = reinterpret_cast<int8_t*>(m_data+m_pos);
		m_pos += sizeof(*it);
		return *it;
	}
	
	int8_t val;
	Read(&val, sizeof(val));
	return val;
}

int16_t SParcel::ReadInt16()
{
	if (ssize_t(m_pos+sizeof(int16_t)) <= m_length) {
		const int16_t* it = reinterpret_cast<int16_t*>(m_data+m_pos);
		m_pos += sizeof(*it);
		return *it;
	}
	
	int16_t val;
	Read(&val, sizeof(val));
	return val;
}

int32_t SParcel::ReadInt32()
{
	if (ssize_t(m_pos+sizeof(int32_t)) <= m_length) {
		const int32_t* it = reinterpret_cast<int32_t*>(m_data+m_pos);
		m_pos += sizeof(*it);
		return *it;
	}
	
	int32_t val;
	Read(&val, sizeof(val));
	return val;
}

int64_t SParcel::ReadInt64()
{
	if (ssize_t(m_pos+sizeof(int64_t)) <= m_length) {
		const int64_t* it = reinterpret_cast<int64_t*>(m_data+m_pos);
		m_pos += sizeof(*it);
		return *it;
	}
	
	int64_t val;
	Read(&val, sizeof(val));
	return val;
}

float SParcel::ReadFloat()
{
	if (ssize_t(m_pos+sizeof(float)) <= m_length) {
		const float* it = reinterpret_cast<float*>(m_data+m_pos);
		m_pos += sizeof(*it);
		return *it;
	}
	
	float val;
	Read(&val, sizeof(val));
	return val;
}

double SParcel::ReadDouble()
{
	if (ssize_t(m_pos+sizeof(double)) <= m_length) {
		const double* it = reinterpret_cast<double*>(m_data+m_pos);
		m_pos += sizeof(*it);
		return *it;
	}
	
	double val;
	Read(&val, sizeof(val));
	return val;
}

sptr<IBinder> SParcel::ReadBinder()
{
	flat_binder_object flat;
	sptr<IBinder> val;
	if (ReadFlatBinderObject(&flat) == sizeof(flat)) {
		unflatten_binder(flat, &val);
	}
	return val;
}

wptr<IBinder> SParcel::ReadWeakBinder()
{
	flat_binder_object flat;
	wptr<IBinder> val;
	if (ReadFlatBinderObject(&flat) == sizeof(flat)) {
		unflatten_binder(flat, &val);
	}
	return val;
}

ssize_t SParcel::WriteString(char const * string)
{
	size_t strLen = strlen(string);
	return Write(string, (strLen + 1));
}

ssize_t SParcel::WriteString(SString const & string)
{
	return Write(string.String(), (string.Length() + 1));
}

SString SParcel::ReadString()
{
	SString string((char *)(m_data + m_pos));
	m_pos += (string.Length() + 1);
	return string;
}


ssize_t SParcel::WriteBuffer(const void* buffer, size_t len) const
{
	if (m_out != NULL) {
		ErrFatalErrorIf(m_binders.CountItems() > 0, "Parcel contains binders.");
		return m_out->Write(buffer, len);
	}
	return 0;
}

ssize_t SParcel::ReadBuffer(void* buffer, size_t len)
{
	if (m_in != NULL) return m_in->Read(buffer, len);
	return 0;
}

const void* SParcel::BinderOffsetsData() const
{
	return m_binders.CountItems() ? m_binders.Array() : 0;
}

size_t SParcel::BinderOffsetsLength() const
{
	return m_binders.CountItems() * sizeof(size_t);
}

#if TARGET_HOST == TARGET_HOST_PALMOS
status_t SParcel::SetBinderOffsets(binder_ipc_info const * const offsets, size_t count, bool takeRefs)
{
	entry e;
	ssize_t s;
	
	if (m_ownsBinders) release_binders();
	m_binders.MakeEmpty();
	m_binders.SetCapacity(count);
	
	for (size_t i = 0; i < count; i++)
	{
		e.type = offsets[i].type_offset;
		e.object = offsets[i].object_offset;
		if ((s=m_binders.AddItem(e)) < B_OK) {
			m_binders.MakeEmpty();
			m_ownsBinders = false;
			return s;
		}
	}
	
	// Passing in a false 'takeRefs' means that the caller has
	// already acquired references on these objects, but we will
	// still own them.  This is different than the BeOS situation
	// where the caller completely owns the references.
	m_ownsBinders = true;
	if (takeRefs) {
		acquire_binders();
	}
	
	return B_OK;
}

#else

status_t SParcel::SetBinderOffsets(const void* offsets, size_t length, bool takeRefs)
{
	ssize_t s;
	
	if (m_ownsBinders) release_binders();
	m_binders.MakeEmpty();
	if (m_binders.AddArray((const size_t*)offsets, length/sizeof(size_t)) < B_OK) {
		m_binders.MakeEmpty();
		m_ownsBinders = false;
		return s;
	}
	
	if ((m_ownsBinders=takeRefs) != false) {
		acquire_binders();
	}
	
	return B_OK;
}

#endif

void SParcel::PrintToStream(const sptr<ITextOutput>& io, uint32_t flags) const
{
	if (flags&B_PRINT_STREAM_HEADER) io << "SParcel(";
	
	if (Data() && Length() > 0) {
		io << indent << SHexDump(Data(), Length());
		const int32_t N = m_binders.CountItems();
		for (int32_t i=0; i<N; i++) {
			const flat_binder_object* flat
				= reinterpret_cast<flat_binder_object*>(m_data+m_binders[i]);
			io << endl << "Binder #" << i << " @" << (void*)(m_binders[i]) << ": "
				<< STypeCode(B_UNPACK_TYPE_CODE(flat->type)) << " = " << flat->binder;
		}
		io << dedent;
	} else if (Length() < 0) {
		io << strerror(Length());
	} else if (Data()) {
		io << "Data=" << Data() << ", Length=" << Length();
	} else {
		io << "NULL";
	}
	
	if (flags&B_PRINT_STREAM_HEADER) io << ")";
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SParcel& buffer)
{
	buffer.PrintToStream(io, B_PRINT_STREAM_HEADER);
	return io;
}

void SParcel::Reset()
{
	if (m_ownsBinders) release_binders();
	m_binders.MakeEmpty();
	if (m_data == m_inline) m_avail = sizeof(m_inline);
	m_dirty = false;
	m_ownsBinders = false;
	m_length = 0;
	m_base = 0;
	m_pos = 0;
}

void SParcel::do_free()
{
	if (m_dirty) Flush();
	if (m_ownsBinders) release_binders();
	if (m_free) m_free(m_data, m_avail, m_freeContext);
	m_binders.MakeEmpty();
	m_dirty = false;
	m_ownsBinders = false;
	m_data = NULL;
	m_length = m_avail = 0;
	m_free = NULL;
	m_base = 0;
	m_pos = 0;
}

void SParcel::acquire_binders()
{
	const int32_t N = m_binders.CountItems();
	for (int32_t i=0; i<N; i++) {
		const flat_binder_object* flat
			= reinterpret_cast<flat_binder_object*>(m_data+m_binders[i]);
		acquire_object(*flat, this);
	}
}

void SParcel::release_binders()
{
	const int32_t N = m_binders.CountItems();
	for (int32_t i=0; i<N; i++) {
		const flat_binder_object* flat
			= reinterpret_cast<flat_binder_object*>(m_data+m_binders[i]);
		release_object(*flat, this);
	}
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
