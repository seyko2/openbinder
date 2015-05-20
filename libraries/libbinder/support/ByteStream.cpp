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

#include <support/IByteStream.h>
#include <support/ByteStream.h>
#include <support/Parcel.h>
#include <support/StaticValue.h>
#include <support/StdIO.h>

#include <support/Debug.h>

#include <string.h>
#include <unistd.h>

#include <new>

#include <stdlib.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

B_STATIC_STRING_VALUE_8	(key_Read,			"Read",);
B_STATIC_STRING_VALUE_8	(key_Write,			"Write",);
B_STATIC_STRING_VALUE_8	(key_Sync,			"Sync",);
B_STATIC_STRING_VALUE_12(key_Position,		"Position",);
B_STATIC_STRING_VALUE_8	(key_Seek,			"Seek",);

/*-----------------------------------------------------------------*/

class BpByteInput : public BpInterface<IByteInput>
{
	public:

		BpByteInput(const sptr<IBinder>& o)
			:	BpInterface<IByteInput>(o),
				m_canTransact(true)
		{
		}
		
		virtual ssize_t ReadV(const iovec *vector, ssize_t count, uint32_t flags)
		{
			SParcel *replyParcel = SParcel::GetParcel();
			SValue replyValue;
			
			ssize_t thisSize,size = 0;
			ssize_t status = B_OK;
			ssize_t sizeLeft = 0;
			const uint8_t* buf=NULL, *pos=NULL;
			
			// Count total number of bytes being requested.
			for (int32_t i=0;i<count;i++) size += vector[i].iov_len;
			
			// If we think the remote binder can handle low-level
			// transactions, go for it.
			if (m_canTransact) {
				// Command tell the target how many bytes to read.
				SParcel *command = SParcel::GetParcel();
				command->Reserve(sizeof(size)+sizeof(flags));
				command->WriteInt32(B_HOST_TO_LENDIAN_INT32(size));
				command->WriteInt32(B_HOST_TO_LENDIAN_INT32(flags));
				
				// Execute the read operation.
				status = Remote()->Transact(B_READ_TRANSACTION, *command, replyParcel);
				sizeLeft = replyParcel->Length();
				
				// Retrieve status and setup to read data.
				if (status >= B_OK && sizeLeft >= static_cast<ssize_t>(sizeof(ssize_t))) {
					buf = static_cast<const uint8_t*>(replyParcel->Data());
					status = B_LENDIAN_TO_HOST_INT32(*reinterpret_cast<const ssize_t*>(buf));
					if (status >= 0) {
						status = B_OK;
						buf += sizeof(ssize_t);
						sizeLeft -= sizeof(ssize_t);
						pos = buf;
					}
				} else if (status >= B_OK) {
					// Return error code.
					status = B_NO_MEMORY;
				}
				
				// If the remote didn't understand our transaction, revert
				// to the Effect() API and try again.
				if (status == B_BINDER_UNKNOWN_TRANSACT) {
					m_canTransact = false;
				}
				SParcel::PutParcel(command);
			}
			
			// If we know the remote binder can't handle remote transactions
			// (either from an attempt we just did or a previous one), talk
			// with it through the high-level Effect() API.
			if (!m_canTransact) {
				// Execute the operation and setup to read data.
				SValue args(SValue::Int32(0), SValue::Int32(size));
				if (flags != 0) args.JoinItem(SValue::Int32(1), SValue::Int32(flags));
				replyValue = Remote()->Invoke(key_Read, args);
				status = replyValue.ErrorCheck();
				sizeLeft = replyValue.Length();
				pos = buf = static_cast<const uint8_t*>(replyValue.Data());
				if (status >= 0 && pos == NULL) status = B_NO_MEMORY;
			}
			
			// Retrieve data and write it out.
			if (status >= B_OK) {
				for (int32_t i=0;i<count && sizeLeft>0;i++) {
					thisSize = vector[i].iov_len;
					if (thisSize > sizeLeft) thisSize = sizeLeft;
					memcpy(vector[i].iov_base,pos,thisSize);
					sizeLeft -= thisSize;
					pos += thisSize;
				}
				status = pos-buf;
			}
			SParcel::PutParcel(replyParcel);
			return status;
		}
		
		bool m_canTransact;
};

class BpByteOutput : public BpInterface<IByteOutput>
{
	public:

		BpByteOutput(const sptr<IBinder>& o)
			:	BpInterface<IByteOutput>(o),
				m_canTransact(true)
		{
		}
		
		virtual ssize_t WriteV(const iovec *vector, ssize_t count, uint32_t flags)
		{
			ssize_t status(B_OK);
				
			// If we think the remote binder can handle low-level
			// transactions, go for it.
			if (m_canTransact) {
				SParcel *command = SParcel::GetParcel();
				SParcel *reply = SParcel::GetParcel();
				uint32_t code;
				
				if (count == 1 && flags == 0) {
					// Optimize the common case to avoid a memcpy().
					command->Reference(vector->iov_base, vector->iov_len);
					code = B_WRITE_TRANSACTION;
				} else {
					ssize_t size = 0, i;
					
					// Count number of bytes being written.
					for (i=0;i<count;i++) size += vector[i].iov_len;
					// Allocate space for total bytes plus the flags parameter.
					uint8_t *buf = static_cast<uint8_t*>(command->Alloc(size+sizeof(flags)));
					if (buf) {
						*reinterpret_cast<int32_t*>(buf) = B_HOST_TO_LENDIAN_INT32(flags);
						// Copy in flags.
						buf += sizeof(flags);
						// Collect and copy in data.
						for (i=0;i<count;i++) {
							memcpy(buf,vector[i].iov_base,vector[i].iov_len);
							buf += vector[i].iov_len;
						}
					} else {
						status = B_NO_MEMORY;
					}
					code = B_WRITE_FLAGS_TRANSACTION;
				}
				
				// Execute the transaction created above.
				if (status >= B_OK) {
					status = Remote()->Transact(code, *command, reply);
					if (status >= B_OK) {
						if (reply->Length() >= static_cast<ssize_t>(sizeof(ssize_t)))
							status = B_LENDIAN_TO_HOST_INT32(*static_cast<const ssize_t*>(reply->Data()));
						else
							status = B_ERROR;
					}
				}
			
				// If the remote didn't understand our transaction, revert
				// to the Effect() API and try again.
				if (status == B_BINDER_UNKNOWN_TRANSACT) {
					m_canTransact = false;
				}
				SParcel::PutParcel(reply);
				SParcel::PutParcel(command);
			}
			
			// If we know the remote binder can't handle remote transactions
			// (either from an attempt we just did or a previous one), talk
			// with it through the high-level Effect() API.
			if (!m_canTransact) {
				SValue data;
				ssize_t size = 0, i;
				
				// Count number of bytes being written.
				for (i=0;i<count;i++) size += vector[i].iov_len;
				// Copy data into value.
				uint8_t *buf = static_cast<uint8_t*>(data.BeginEditBytes(B_RAW_TYPE, size));
				if (buf) {
					// Collect and copy in data.
					for (i=0;i<count;i++) {
						memcpy(buf,vector[i].iov_base,vector[i].iov_len);
						buf += vector[i].iov_len;
					}
					
					// Invoke write command on remote binder.
					status = Remote()->Invoke(
							key_Write,
							SValue(SValue::Int32(0), data).
								JoinItem(SValue::Int32(1), SValue::Int32(flags))
						).AsSSize();
				} else {
					status = B_NO_MEMORY;
				}
				
			}
			
			return status;
		}

		virtual status_t Sync()
		{
			return Remote()->Invoke(key_Sync, B_WILD_VALUE).AsStatus();
		}
		
		bool m_canTransact;
};


class BpByteSeekable : public BpInterface<IByteSeekable>
{
	public:

		BpByteSeekable(const sptr<IBinder>& o) : BpInterface<IByteSeekable>(o) {};

		virtual	off_t Position() const
		{
			SValue v(Remote()->Get(key_Position));
			int64_t off;
			if (v.GetInt64(&off) != B_OK) off = B_ERROR;
			return static_cast<off_t>(off);
		}
		
		virtual off_t Seek(off_t position, uint32_t seek_mode)
		{
			SValue v(Remote()->Invoke(
					key_Seek,
					SValue(SValue::Int32(0), SValue::Int64(position))
						.JoinItem(SValue::Int32(1), SValue::Int32(seek_mode))
				));
			int64_t off; 
			if (v.GetInt64(&off) != B_OK) off = B_ERROR;
			return static_cast<off_t>(off);
		}
};


/*-----------------------------------------------------------------*/

B_IMPLEMENT_META_INTERFACE(ByteInput, "org.openbinder.support.IByteInput", IByteInput)
B_IMPLEMENT_META_INTERFACE(ByteOutput, "org.openbinder.support.IByteOutput", IByteOutput)
B_IMPLEMENT_META_INTERFACE(ByteSeekable, "org.openbinder.support.IByteSeekable", IByteSeekable)

/*-----------------------------------------------------------------*/

status_t
BnByteInput::Link(const sptr<IBinder>& to, const SValue &bindings, uint32_t flags)
{
	return BBinder::Link(to, bindings, flags);
}

status_t
BnByteInput::Unlink(const wptr<IBinder>& from, const SValue &bindings, uint32_t flags)
{
	return BBinder::Unlink(from, bindings, flags);
}

status_t
BnByteInput::Effect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)
{
	return BBinder::Effect(in, inBindings, outBindings, out);
}

status_t
BnByteInput::Transact(uint32_t code, SParcel& data, SParcel* reply, uint32_t flags)
{
	if (code == B_READ_TRANSACTION) {
		if (data.Length() >= static_cast<ssize_t>(sizeof(ssize_t))) {
			const ssize_t* params = static_cast<const ssize_t*>(data.Data());
			ssize_t size = B_LENDIAN_TO_HOST_INT32(*params);
			uint32_t flags = 0;
			if (data.Length() >= static_cast<ssize_t>(sizeof(uint32_t)+sizeof(ssize_t))) {
				flags = B_LENDIAN_TO_HOST_INT32(*reinterpret_cast<const int32_t*>(params+1));
			}
			void* out;
			
			// Try to allocate a buffer to read data in to (plus room for
			// a status code at the front).
			// If an error occurred trying to allocate the buffer,
			// just return.  The caller will intepret an empty buffer
			// as being a B_NO_MEMORY error.
			
			if (size > 0 && reply && (out=reply->Alloc(size+sizeof(ssize_t))) != NULL) {
				const ssize_t amt = Read(static_cast<uint8_t*>(out)+sizeof(ssize_t), size, flags);
				*static_cast<ssize_t*>(out) = B_HOST_TO_LENDIAN_INT32(amt);
				if (amt >= 0) reply->ReAlloc(amt+sizeof(ssize_t));
				else reply->ReAlloc(sizeof(ssize_t));
			}
			return B_OK;
		}
		
		return B_BINDER_BAD_TRANSACT;
	} else {
		return BBinder::Transact(code, data, reply, flags);
	}
}

static SValue
byteinput_hook_Read(const sptr<IInterface>& This, const SValue& args)
{
	size_t readSize;
	uint32_t flags;
	SValue param;
	status_t error;
	
	if (!(param = args[SValue::Int32(0)]).IsDefined()) return SValue::Int32(B_BINDER_MISSING_ARG);
	readSize = param.AsInt32(&error);
	if (error != B_OK) return SValue::Int32(B_BINDER_BAD_TYPE);

	flags = args[SValue::Int32(1)].AsInt32();
	
	SValue result;
	void* data = result.BeginEditBytes(B_RAW_TYPE, readSize);
	if (data) {
		ssize_t s = static_cast<IByteInput*>(This.ptr())->Read(data, readSize);
		result.EndEditBytes(s);
		if (s < B_OK) result = SValue::Int32(s);
	} else {
		result = SValue::Int32(B_NO_MEMORY);
	}
	
	return result;
}

static const struct effect_action_def byteinput_actions[] = {
	{	sizeof(effect_action_def), &key_Read,
		NULL, NULL, NULL, byteinput_hook_Read }
};

status_t
BnByteInput::HandleEffect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)
{
	return execute_effect(	sptr<IInterface>(this),
							in, inBindings, outBindings, out,
							byteinput_actions, sizeof(byteinput_actions)/sizeof(byteinput_actions[0]));
}

/*-----------------------------------------------------------------*/

status_t
BnByteOutput::Link(const sptr<IBinder>& to, const SValue &bindings, uint32_t flags)
{
	return BBinder::Link(to, bindings, flags);
}

status_t
BnByteOutput::Unlink(const wptr<IBinder>& from, const SValue &bindings, uint32_t flags)
{
	return BBinder::Unlink(from, bindings, flags);
}

status_t
BnByteOutput::Effect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)
{
	return BBinder::Effect(in, inBindings, outBindings, out);
}

status_t
BnByteOutput::Transact(uint32_t code, SParcel& data, SParcel* reply, uint32_t flags)
{
	if (code == B_WRITE_TRANSACTION) {
		iovec iov;
		iov.iov_base = const_cast<void*>(data.Data());
		iov.iov_len = data.Length();
		ssize_t result = B_BINDER_BAD_TRANSACT;
		if (iov.iov_base && iov.iov_len > 0) result = WriteV(&iov,1);
		if (reply) {
			result = B_HOST_TO_LENDIAN_INT32(result);
			reply->Copy(&result, sizeof(result));
		}
		return B_OK;
	} else if (code == B_WRITE_FLAGS_TRANSACTION) {
		const uint32_t* wflags = static_cast<const uint32_t*>(data.Data());
		iovec iov;
		iov.iov_base = (void*)(wflags+1);
		iov.iov_len = data.Length();
		ssize_t result = B_BINDER_BAD_TRANSACT;
		if (wflags && iov.iov_len >= sizeof(uint32_t)) {
			iov.iov_len -= sizeof(uint32_t);
			result = WriteV(&iov,1,B_LENDIAN_TO_HOST_INT32(*wflags));
		}
		if (reply) {
			result = B_HOST_TO_LENDIAN_INT32(result);
			reply->Copy(&result, sizeof(result));
		}
		return B_OK;
	} else {
		return BBinder::Transact(code, data, reply, flags);
	}
}

static SValue
byteoutput_hook_Write(const sptr<IInterface>& This, const SValue& args)
{
	SValue param;
	uint32_t flags;
	status_t error;
	
	// 'flags' parameter is optional
	if ((param = args[SValue::Int32(1)]).IsDefined()) {
		flags = param.AsInt32(&error);
		if (error != B_OK) return SValue::Int32(B_BINDER_BAD_TYPE);
	} else {
		flags = 0;
	}
	
	if (!(param = args[SValue::Int32(0)]).IsDefined()) return SValue::Int32(B_BINDER_MISSING_ARG);
	if (!param.IsSimple() && param.Data()) return SValue::Int32(B_BINDER_BAD_TYPE);
	
	return SValue::Int32(static_cast<IByteOutput*>(This.ptr())
		->Write(param.Data(), param.Length(), flags));
}

static SValue
byteoutput_hook_Sync(const sptr<IInterface>& This, const SValue& /*args*/)
{
	return SValue::Int32(static_cast<IByteOutput*>(This.ptr())->Sync());
}

static const struct effect_action_def byteoutput_actions[] = {
	{	sizeof(effect_action_def), &key_Write,
		NULL, NULL, NULL, byteoutput_hook_Write },
	{	sizeof(effect_action_def), &key_Sync,
		NULL, NULL, NULL, byteoutput_hook_Sync }
};

status_t
BnByteOutput::HandleEffect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)
{
	return execute_effect(	sptr<IInterface>(this),
							in, inBindings, outBindings, out,
							byteoutput_actions, sizeof(byteoutput_actions)/sizeof(byteoutput_actions[0]));
}

/*-----------------------------------------------------------------*/

status_t
BnByteSeekable::Link(const sptr<IBinder>& to, const SValue &bindings, uint32_t flags)
{
	return BBinder::Link(to, bindings, flags);
}

status_t
BnByteSeekable::Unlink(const wptr<IBinder>& from, const SValue &bindings, uint32_t flags)
{
	return BBinder::Unlink(from, bindings, flags);
}

status_t
BnByteSeekable::Effect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)
{
	return BBinder::Effect(in, inBindings, outBindings, out);
}

status_t
BnByteSeekable::Transact(uint32_t code, SParcel& data, SParcel* reply, uint32_t flags)
{
	return BBinder::Transact(code, data, reply, flags);
}

static SValue
byteseekable_hook_Seek(const sptr<IInterface>& This, const SValue& args)
{
	SValue param;
	off_t position;
	uint32_t seek_mode;
	status_t error;
	
	if (!(param = args[SValue::Int32(0)]).IsDefined()) return SValue::Int32(B_BINDER_MISSING_ARG);
	position = static_cast<off_t>(param.AsInt64(&error));
	if (error != B_OK) return SValue::Int32(B_BINDER_BAD_TYPE);
	
	if (!(param = args[SValue::Int32(1)]).IsDefined()) return SValue::Int32(B_BINDER_MISSING_ARG);
	seek_mode = param.AsInt32(&error);
	if (error != B_OK) return SValue::Int32(B_BINDER_BAD_TYPE);
	
	return SValue::Int64(static_cast<IByteSeekable*>(This.ptr())->Seek(position, seek_mode));
}

static SValue
byteseekable_hook_Position(const sptr<IInterface>& i)
{
	return SValue::Int64(static_cast<IByteSeekable*>(i.ptr())->Position());
}

static const struct effect_action_def byteseekable_actions[] = {
	{	sizeof(effect_action_def), &key_Seek,
		NULL, NULL, NULL, byteseekable_hook_Seek },
	{	sizeof(effect_action_def), &key_Position,
		NULL, NULL, byteseekable_hook_Position, NULL }
};

status_t
BnByteSeekable::HandleEffect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)
{
	return execute_effect(	sptr<IInterface>(this),
							in, inBindings, outBindings, out,
							byteseekable_actions, sizeof(byteseekable_actions)/sizeof(byteseekable_actions[0]));
}

// ----------------------------------------------------------------- //
// ----------------------------------------------------------------- //
// ----------------------------------------------------------------- //

BByteStream::BByteStream(IStorage *This)
	:	m_pos(0), m_store(This)
{
}

BByteStream::BByteStream(const sptr<IStorage>& store)
	:	m_pos(0), m_store(store.ptr())
{
	m_store->Acquire(this);
}

// ----------------------------------------------------------------- //

BByteStream::~BByteStream()
{
	m_store->AttemptRelease(this);
}

// ----------------------------------------------------------------- //

SValue
BByteStream::Inspect(const sptr<IBinder>& caller, const SValue &v, uint32_t flags)
{
	return BnByteInput::Inspect(caller, v, flags)
			.Join(BnByteOutput::Inspect(caller, v, flags))
			.Join(BnByteSeekable::Inspect(caller, v, flags));
}

// ----------------------------------------------------------------- //

ssize_t
BByteStream::WriteV(const struct iovec *vector, ssize_t count, uint32_t flags)
{
	ssize_t res;
	res = count > 0 ? m_store->WriteAtV(m_pos,vector,count) : count;
	if (res >= 0) {
		m_pos += res;
		if ((flags&B_WRITE_END) != 0) {
			ssize_t tot;
			if (count > 0) {
				tot = vector->iov_len;
				while (--count > 0) {
					tot += vector[count].iov_len;
				}
			} else {
				tot = count;
			}
			if (res == tot) {
				status_t err = m_store->SetSize(m_pos);
				if (err < B_OK) res = err;
			}
		}
	}
	return res;
}

status_t 
BByteStream::Sync()
{
	return m_store->Sync();
}

// ----------------------------------------------------------------- //

ssize_t
BByteStream::ReadV(const struct iovec *vector, ssize_t count, uint32_t /*flags*/)
{
	ssize_t res;
	res = count > 0 ? m_store->ReadAtV(m_pos,vector,count) : count;
	if (res > 0) m_pos += res;
	return res;
}

// ----------------------------------------------------------------- //

off_t 
BByteStream::Seek(off_t position, uint32_t seek_mode)
{
	if (seek_mode == SEEK_SET)
		m_pos = position;
	else if (seek_mode == SEEK_END)
		m_pos = m_store->Size() - position;
	else if (seek_mode == SEEK_CUR)
		m_pos += position;
	return m_pos;
}

off_t 
BByteStream::Position() const
{
	return m_pos;
}


// ----------------------------------------------------------------- //
// ----------------------------------------------------------------- //
// ----------------------------------------------------------------- //

BReadOnlyStream::BReadOnlyStream(IStorage *This)
	:	m_pos(0),
		m_store(This)
{
}

BReadOnlyStream::BReadOnlyStream(const sptr<IStorage>& store)
	:	m_pos(0),
		m_store(store.ptr())
{
	m_store->Acquire(this);
}

// ----------------------------------------------------------------- //

BReadOnlyStream::~BReadOnlyStream()
{
	m_store->AttemptRelease(this);
}

// ----------------------------------------------------------------- //

SValue BReadOnlyStream::Inspect(const sptr<IBinder>& caller, const SValue &v, uint32_t flags)
{
	return BnByteInput::Inspect(caller, v, flags)
			.Join(BnByteSeekable::Inspect(caller, v, flags));
}

// ----------------------------------------------------------------- //

ssize_t BReadOnlyStream::ReadV(const struct iovec *vector, ssize_t count, uint32_t /*flags*/)
{
	ssize_t res;
	res = count > 0 ? m_store->ReadAtV(m_pos,vector,count) : count;
	if (res > 0) m_pos += res;
	return res;
}

// ----------------------------------------------------------------- //

off_t BReadOnlyStream::Seek(off_t position, uint32_t seek_mode)
{
	if (seek_mode == SEEK_SET)
		m_pos = position;
	else if (seek_mode == SEEK_END)
		m_pos = m_store->Size() - position;
	else if (seek_mode == SEEK_CUR)
		m_pos += position;
	return m_pos;
}

off_t BReadOnlyStream::Position() const
{
	return m_pos;
}

// ----------------------------------------------------------------- //
// ----------------------------------------------------------------- //
// ----------------------------------------------------------------- //

BWriteOnlyStream::BWriteOnlyStream(IStorage *This)
	:	m_pos(0), m_store(This)
{
}

BWriteOnlyStream::BWriteOnlyStream(const sptr<IStorage>& store)
	:	m_pos(0), m_store(store.ptr())
{
	m_store->Acquire(this);
}

// ----------------------------------------------------------------- //

BWriteOnlyStream::~BWriteOnlyStream()
{
	m_store->AttemptRelease(this);
}

// ----------------------------------------------------------------- //

SValue BWriteOnlyStream::Inspect(const sptr<IBinder>& caller, const SValue &v, uint32_t flags)
{
	return BnByteOutput::Inspect(caller, v, flags)
			.Join(BnByteSeekable::Inspect(caller, v, flags));
}

// ----------------------------------------------------------------- //

ssize_t BWriteOnlyStream::WriteV(const struct iovec *vector, ssize_t count, uint32_t flags)
{
	ssize_t res;
	res = count > 0 ? m_store->WriteAtV(m_pos,vector,count) : count;
	if (res >= 0) {
		m_pos += res;
		if ((flags&B_WRITE_END) != 0) {
			ssize_t tot;
			if (count > 0) {
				tot = vector->iov_len;
				while (--count > 0) {
					tot += vector[count].iov_len;
				}
			} else {
				tot = count;
			}
			if (res == tot) {
				status_t err = m_store->SetSize(m_pos);
				if (err < B_OK) res = err;
			}
		}
	}
	return res;
}

status_t BWriteOnlyStream::Sync()
{
	return m_store->Sync();
}

// ----------------------------------------------------------------- //

off_t BWriteOnlyStream::Seek(off_t position, uint32_t seek_mode)
{
	if (seek_mode == SEEK_SET)
		m_pos = position;
	else if (seek_mode == SEEK_END)
		m_pos = m_store->Size() - position;
	else if (seek_mode == SEEK_CUR)
		m_pos += position;
	return m_pos;
}

off_t BWriteOnlyStream::Position() const
{
	return m_pos;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
