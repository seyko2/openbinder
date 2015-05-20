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

#include <support/IStorage.h>
#include <support/Parcel.h>

#include <stdlib.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

B_STATIC_STRING_VALUE_8	(key_Size,		"Size",);
B_STATIC_STRING_VALUE_8	(key_SetSize,	"SetSize",);
B_STATIC_STRING_VALUE_8	(key_ReadAt,	"ReadAt",);
B_STATIC_STRING_VALUE_8	(key_WriteAt,	"WriteAt",);
B_STATIC_STRING_VALUE_8	(key_Sync,		"Sync",);

/*-----------------------------------------------------------------*/

class BpStorage : public BpInterface<IStorage>
{
	public:

		BpStorage(const sptr<IBinder>& o)
			:	BpInterface<IStorage>(o),
				m_canTransact(true)
		{
		}
		
		virtual	off_t Size() const
		{
			status_t err;
			const off_t off = Remote()->Get(key_Size).AsOffset(&err);
			return err >= B_OK ? off : err;
		}
		
		virtual	status_t SetSize(off_t size)
		{
			return Remote()->Invoke(
					key_Sync,
					SValue(B_0_INT32, SValue::Offset(size))
				).AsStatus();
		}
		
		virtual ssize_t ReadAtV(off_t position, const iovec *vector, ssize_t count)
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
				read_at_transaction* h = static_cast<read_at_transaction*>(
					command->Alloc(sizeof(read_at_transaction)));
				h->position = position;
				h->size = size;
				
				// Execute the read operation.
				status = Remote()->Transact(B_READ_AT_TRANSACTION, *command, replyParcel);
				sizeLeft = replyParcel->Length();
				
				// Retrieve status and setup to read data.
				if (status >= B_OK && sizeLeft >= static_cast<ssize_t>(sizeof(ssize_t))) {
					buf = static_cast<const uint8_t*>(replyParcel->Data());
					if (*reinterpret_cast<const ssize_t*>(buf) >= 0) {
						buf += sizeof(ssize_t);
						sizeLeft -= sizeof(ssize_t);
						pos = buf;
					} else {
						// Return error code.
						status = *reinterpret_cast<const ssize_t*>(buf);
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
				replyValue = Remote()->Invoke(
						key_ReadAt,
						SValue(B_0_INT32, SValue::Int64(position)).
							JoinItem(B_1_INT32, SValue::SSize(size))
					);
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
		
		virtual ssize_t WriteAtV(off_t position, const iovec *vector, ssize_t count)
		{
			ssize_t status(B_OK);
				
			// If we think the remote binder can handle low-level
			// transactions, go for it.
			if (m_canTransact) {
				SParcel *command = SParcel::GetParcel();
				SParcel *reply = SParcel::GetParcel();
				
				ssize_t size = 0, i;
				
				// Count number of bytes being written.
				for (i=0;i<count;i++) size += vector[i].iov_len;
				// Allocate space for total bytes plus the flags parameter.
				uint8_t *buf = static_cast<uint8_t*>(command->Alloc(size+sizeof(write_at_transaction)));
				if (buf) {
					reinterpret_cast<write_at_transaction*>(buf)->position = position;
					buf += sizeof(write_at_transaction);
					// Collect and copy in data.
					for (i=0;i<count;i++) {
						memcpy(buf,vector[i].iov_base,vector[i].iov_len);
						buf += vector[i].iov_len;
					}
				} else {
					status = B_NO_MEMORY;
				}
				
				// Execute the transaction created above.
				if (status >= B_OK) {
					status = Remote()->Transact(B_WRITE_AT_TRANSACTION, *command, reply);
					if (status >= B_OK) {
						if (reply->Length() >= static_cast<ssize_t>(sizeof(ssize_t)))
							status = *static_cast<const ssize_t*>(reply->Data());
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
					const SValue reply = Remote()->Invoke(
							key_WriteAt,
							SValue(B_0_INT32, SValue::Int64(position)).
								JoinItem(B_0_INT32, data)
						);
					status = reply.AsSSize();
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

B_IMPLEMENT_META_INTERFACE(Storage, "org.openbinder.support.IStorage", IStorage)

/*-----------------------------------------------------------------*/

status_t
BnStorage::Transact(uint32_t code, SParcel& data, SParcel* reply, uint32_t flags)
{
	if (code == B_READ_AT_TRANSACTION) {
		ssize_t amt = B_BINDER_BAD_TRANSACT;
		if (data.Length() >= static_cast<ssize_t>(sizeof(read_at_transaction))) {
			const read_at_transaction* rd =
				static_cast<const read_at_transaction*>(data.Data());
			void* out;
			
			// Try to allocate a buffer to read data in to (plus room for
			// a status code at the front).
			
			if (reply && (out=reply->Alloc(rd->size+sizeof(ssize_t))) != NULL) {
				amt	= rd->size > 0
					? ReadAt(rd->position, static_cast<uint8_t*>(out)+sizeof(ssize_t), rd->size)
					: 0;
				*static_cast<ssize_t*>(out) = amt;
				if (amt >= 0) reply->ReAlloc(amt+sizeof(ssize_t));
				else reply->ReAlloc(sizeof(ssize_t));
			} else {
				amt = B_NO_MEMORY;
			}
		}
		return amt >= B_OK ? B_OK : amt;
		
	} else if (code == B_WRITE_AT_TRANSACTION) {
		const write_at_transaction* wr = static_cast<const write_at_transaction*>(data.Data());
		iovec iov;
		iov.iov_base = (void*)(wr+1);
		iov.iov_len = data.Length();
		ssize_t result = B_BINDER_BAD_TRANSACT;
		if (wr && iov.iov_len >= sizeof(*wr)) {
			iov.iov_len -= sizeof(wr);
			result = WriteAtV(wr->position,&iov,1);
		}
		if (reply) reply->Copy(&result, sizeof(result));
		return result >= B_OK ? B_OK : result;
		
	} else {
		return BBinder::Transact(code, data, reply, flags);
	}
}

static SValue
storage_hook_Size(const sptr<IInterface>& i)
{
	return SValue::Offset(static_cast<IStorage*>(i.ptr())->Size());
}

static SValue
storage_hook_SetSize(const sptr<IInterface>& This, const SValue& args)
{
	SValue param;
	off_t size;
	status_t error;
	
	if (!(param = args[SValue::Int32(0)]).IsDefined()) return SValue::SSize(B_BINDER_MISSING_ARG);
	size = param.AsOffset(&error);
	if (error != B_OK) return SValue::SSize(B_BINDER_BAD_TYPE);
	
	return SValue::SSize(static_cast<IStorage*>(This.ptr())->SetSize(size));
}

static SValue
storage_hook_ReadAt(const sptr<IInterface>& This, const SValue& args)
{
	SValue param;
	status_t error;
	
	off_t position;
	iovec iov;
	
	if (!(param = args[SValue::Int32(0)]).IsDefined()) return SValue::SSize(B_BINDER_MISSING_ARG);
	position = param.AsOffset(&error);
	if (error != B_OK) return SValue::SSize(B_BINDER_BAD_TYPE);
	
	if (!(param = args[SValue::Int32(1)]).IsDefined()) return SValue::SSize(B_BINDER_MISSING_ARG);
	iov.iov_len = param.AsInt32(&error);
	if (error != B_OK) return SValue::SSize(B_BINDER_BAD_TYPE);
	
	SValue result;
	iov.iov_base = result.BeginEditBytes(B_RAW_TYPE, iov.iov_len);
	if (iov.iov_base) {
		ssize_t s = static_cast<IStorage*>(This.ptr())->ReadAtV(position, &iov, 1);
		result.EndEditBytes(s);
		if (s < B_OK) result = SValue::SSize(s);
	} else {
		result = SValue::SSize(B_NO_MEMORY);
	}
	
	return result;
}

static SValue
storage_hook_WriteAt(const sptr<IInterface>& This, const SValue& args)
{
	SValue param;
	status_t error;
	
	off_t position;
	
	if (!(param = args[SValue::Int32(0)]).IsDefined()) return SValue::SSize(B_BINDER_MISSING_ARG);
	position = param.AsOffset(&error);
	if (error != B_OK) return SValue::SSize(B_BINDER_BAD_TYPE);
	
	if (!(param = args[SValue::Int32(1)]).IsDefined()) return SValue::SSize(B_BINDER_MISSING_ARG);
	if (!param.IsSimple() && param.Data()) return SValue::SSize(B_BINDER_BAD_TYPE);
	
	return SValue::SSize(static_cast<IStorage*>(This.ptr())
		->WriteAt(position, param.Data(), param.Length()));
}

static SValue
storage_hook_Sync(const sptr<IInterface>& This, const SValue& /*args*/)
{
	return SValue::SSize(static_cast<IStorage*>(This.ptr())->Sync());
}

static const struct effect_action_def storage_actions[] = {
	{	sizeof(effect_action_def), &key_Size,
		NULL, NULL, storage_hook_Size, NULL },
	{	sizeof(effect_action_def), &key_SetSize,
		NULL, NULL, NULL, storage_hook_SetSize },
	{	sizeof(effect_action_def), &key_ReadAt,
		NULL, NULL, NULL, storage_hook_ReadAt },
	{	sizeof(effect_action_def), &key_WriteAt,
		NULL, NULL, NULL, storage_hook_WriteAt },
	{	sizeof(effect_action_def), &key_Sync,
		NULL, NULL, NULL, storage_hook_Sync }
};

status_t
BnStorage::HandleEffect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)
{
	return execute_effect(	sptr<IInterface>(this),
							in, inBindings, outBindings, out,
							storage_actions, sizeof(storage_actions)/sizeof(storage_actions[0]));
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
