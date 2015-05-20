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

#include <support_p/RBinder.h>

#include <support/atomic.h>
#include <support/Autolock.h>
#include <support/Looper.h>
#include <support/StdIO.h>
#include <support/Parcel.h>
#include <support_p/BinderKeys.h>
#include <support_p/SupportMisc.h>

#include <support_p/WindowsCompatibility.h>

#include <stdio.h>

#define RBINDER_DEBUG_MSGS 0
#define BINDER_DEBUG_MSGS 0
#define OBIT_DEBUG_MSGS 0

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {
#endif

#if _SUPPORTS_NAMESPACE
using namespace palmos::osp;
using namespace palmos::support;
#endif

BpBinder::BpBinder(int32_t handle)
	: m_handle(handle), m_lock("BpBinder"), m_obituaries(NULL), m_alive(true)
{
#if RBINDER_DEBUG_MSGS
	printf("*** BpBinder(): IncRefs %p descriptor %ld\n", this, m_handle);
#endif
	SLooper::This()->IncrefsHandle(m_handle);
}

BpBinder::~BpBinder()
{
#if RBINDER_DEBUG_MSGS
	printf("*** ~BpBinder(): DecRefs %p descriptor %ld\n", this, m_handle);
#endif
	SLooper* loop = SLooper::This();
	loop->Process()->StrongHandleGone(this);
	loop->ExpungeHandle(m_handle, this);
	loop->DecrefsHandle(m_handle);

	SVector<Obituary>* obits = m_obituaries;
	m_obituaries = NULL;
	//if (obits) bout << "BpBinder " << this << " deleted with death links" << endl;
	delete obits;
}

status_t 
BpBinder::Link(const sptr<IBinder>& node, const SValue &binding, uint32_t flags)
{
	SParcel *data = SParcel::GetParcel();
	SParcel *reply = SParcel::GetParcel();
	SValue result;

	data->WriteInt32(3);
	data->WriteBinder(node);
	data->WriteValue(binding);
	data->WriteInt32(flags);

	status_t error = Transact(B_LINK_TRANSACTION, *data, reply);

	if (error == B_OK) {
		error = reply->ReadInt32();
	}
	SParcel::PutParcel(reply);
	SParcel::PutParcel(data);

	return error;
}

status_t 
BpBinder::Unlink(const wptr<IBinder>& node, const SValue &binding, uint32_t flags)
{
	SParcel *data = SParcel::GetParcel();
	SParcel *reply = SParcel::GetParcel();
	SValue result;

	data->WriteInt32(3);
	data->WriteWeakBinder(node);
	data->WriteValue(binding);
	data->WriteInt32(flags);

	status_t error = Transact(B_UNLINK_TRANSACTION, *data, reply);

	if (error == B_OK) {
		error = reply->ReadInt32();
	}
	SParcel::PutParcel(reply);
	SParcel::PutParcel(data);

	return error;
}

SValue 
BpBinder::Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags)
{
	SParcel *data = SParcel::GetParcel();
	SParcel *reply = SParcel::GetParcel();
	SValue result;

	data->WriteInt32(3);
	data->WriteBinder(caller);
	data->WriteValue(which);
	data->WriteInt32(flags);

	status_t error = Transact(B_INSPECT_TRANSACTION, *data, reply);

	if (error == B_OK) {
		result = reply->ReadValue();
	} else {
		result.SetError(error);
	}
	SParcel::PutParcel(reply);
	SParcel::PutParcel(data);

	return result;
}

sptr<IInterface>
BpBinder::InterfaceFor(const SValue &/*descriptor*/, uint32_t /*flags*/)
{
	return sptr<IInterface>(NULL);
}

status_t 
BpBinder::Effect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)
{
	SParcel *buffer = SParcel::GetParcel();
	
	const SValue *outBindingsP = outBindings.IsDefined() ? &outBindings : NULL;
	const SValue *inBindingsP = (outBindingsP || (!inBindings.IsWild())) ? &inBindings : NULL;
	ssize_t result = buffer->SetValues(&in,inBindingsP,outBindingsP,NULL);
	if (result >= B_OK) {
		SParcel *reply = SParcel::GetParcel();
#if BINDER_DEBUG_MSGS
		berr << "BpBinder::Effect " << *buffer << endl;
#endif
		result = Transact(B_EFFECT_TRANSACTION,*buffer,out ? reply : NULL,0);
		if (result >= B_OK) {
			if (out && (result=reply->GetValues(1, out)) >= 1) result = out->ErrorCheck();
			else if (result >= B_OK) result = reply->ErrorCheck();
		} else if (out) {
			out->SetError(result);
		}
#if BINDER_DEBUG_MSGS
		berr << "BpBinder::Effect reply {" << endl << indent
			<< *reply << endl;
		if (out) berr << *out << endl;
		berr << dedent;
#endif
		SParcel::PutParcel(reply);
	}
	SParcel::PutParcel(buffer);
	return result;
}

status_t
BpBinder::AutobinderPut(const BAutobinderDef *def, const void *value)
{
	ssize_t amt;
	SParcel *parcel = SParcel::GetParcel();

	// index of property
	parcel->WriteInt32(def->index);

	// name of the property
	parcel->WriteValue(def->key());

	// value of the property
	if (def->put->returnMarshaller) {
		amt = def->put->returnMarshaller->marshal_parcel(*parcel, value);
	} else {
		amt = parcel->WriteTypedData(def->put->returnType, value);
	}
	if (amt < 0) goto clean_up;

	amt = Transact(B_PUT_TRANSACTION, *parcel);
clean_up:
	SParcel::PutParcel(parcel);
	// put it!
	return amt;
}

status_t
BpBinder::AutobinderGet(const BAutobinderDef *def, void *result)
{
	status_t err;
	SParcel *parcel = SParcel::GetParcel();
	SParcel *reply = SParcel::GetParcel();
	
	// index of property
	parcel->WriteInt32(def->index);

	// name of the property
	parcel->WriteValue(def->key());

	// get it!
	err = Transact(B_GET_TRANSACTION, *parcel, reply);
	if (err != B_OK) goto clean_up;
	
	// value of the property
	if (def->get->returnMarshaller) {
		err = def->get->returnMarshaller->unmarshal_parcel(*reply, result);
	} else {
		err = reply->ReadTypedData(def->get->returnType, result);
	}
	if (err == B_BINDER_READ_NULL_VALUE) err = B_OK;

clean_up:
	SParcel::PutParcel(reply);
	SParcel::PutParcel(parcel);
	return err;
}

status_t
BpBinder::AutobinderInvoke(const BAutobinderDef* def, void** params, void *result)
{
	status_t err = B_OK;
	ssize_t amt;
	size_t i;
	uint32_t dirs = 0;

	const BEffectMethodDef* const inv = def->invoke;
	const BParameterInfo* const pi = inv->paramTypes;
	const BParameterInfo* const pi_end = inv->paramTypes+inv->paramCount;

	SParcel *parcel = SParcel::GetParcel();
	SParcel *reply = SParcel::GetParcel();

	// write index and name of the function
	parcel->WriteInt32(def->index);
	parcel->WriteValue(def->key());
	//bout << "Wrote function key " << def->key() << ": " << parcel << endl;

	// in and inout parameters
	err = autobinder_marshal_args(pi, pi_end, B_IN_PARAM, params, *parcel, &dirs);
	if (err < 0) goto clean_up;
	
	// invoke it!
	err = Transact(B_INVOKE_TRANSACTION, *parcel, reply);
	if (err != B_OK) goto clean_up;
	
	if (inv->returnType != B_UNDEFINED_TYPE || (dirs&B_OUT_PARAM) != 0) {

		// return value
		if (inv->returnMarshaller) {
			err = inv->returnMarshaller->unmarshal_parcel(*reply, result);
		} else {
			err = reply->ReadTypedData(inv->returnType, result);
		}
		if (err == B_BINDER_READ_NULL_VALUE) err = B_OK;

		// unmarshal output params if needed.
		if (err == B_OK && (dirs&B_OUT_PARAM) != 0) {
			err = autobinder_unmarshal_args(pi, pi_end, B_OUT_PARAM, *reply, params, &dirs);
		}
	}

clean_up:
	SParcel::PutParcel(reply);
	SParcel::PutParcel(parcel);

	return err;
}

status_t
BpBinder::Transact(uint32_t code, SParcel& data, SParcel* reply, uint32_t flags)
{
	// Once a binder has died, it will never come back to life.
	if (m_alive) {
		status_t status = SLooper::This()->Transact(m_handle, code, data, reply, flags);
		if (status == B_BINDER_DEAD) SendObituary();
		return status;
	}
	
	return B_BINDER_DEAD;
}

status_t
BpBinder::LinkToDeath(	const sptr<BBinder>& target,
						const SValue &method,
						uint32_t flags)
{
	Obituary obit;
	obit.target = target.ptr();
	obit.method = method;
	obit.flags = flags;

#if OBIT_DEBUG_MSGS
	disable_thread_syslog();
	bout << "LinkToDeath: " << this << " -> " << target << " / " << method << endl;
	enable_thread_syslog();
#endif

	{
		SAutolock _l(m_lock.Lock());

		if (m_alive) {
			if (m_obituaries == NULL) {
				m_obituaries = new B_NO_THROW SVector<Obituary>;
			}
			if (m_obituaries == NULL) {
				return B_NO_MEMORY;
			}

#if OBIT_DEBUG_MSGS
			disable_thread_syslog();
			bout << "Adding death link " << method << " from " << this << " to " << target << endl;
			enable_thread_syslog();
#endif

			return m_obituaries->AddItem(obit);
		}
	}

#if OBIT_DEBUG_MSGS
	disable_thread_syslog();
	bout << "Immediately reporting death link " << method << " from " << this << " to " << target << endl;
	enable_thread_syslog();
#endif
	report_one_death(obit);
	return B_BINDER_DEAD;
}

status_t
BpBinder::UnlinkToDeath(const wptr<BBinder>& target, const SValue &method, uint32_t flags)
{
	SAutolock _l(m_lock.Lock());

	bool found = false;

#if OBIT_DEBUG_MSGS
	disable_thread_syslog();
	bout << "UnlinkToDeath: " << this << " -> " << target << " / " << method << endl;
	enable_thread_syslog();
#endif

	if (m_obituaries) {
		size_t N = m_obituaries->CountItems();
		for (size_t i=0; i<N; i++) {
			const Obituary& obit = m_obituaries->ItemAt(i);
			if (target.unsafe_ptr_access() == obit.target.unsafe_ptr_access()
					&& ((obit.method == method && (obit.flags&~kPrivateLinkFlagMask) == flags) || (flags&B_UNLINK_ALL_TARGETS) != 0)) {
				found = true;
#if OBIT_DEBUG_MSGS
				disable_thread_syslog();
				bout << "FOUND!  At index " << i << endl;
				enable_thread_syslog();
#endif
				if (N == 1) {
					delete m_obituaries;
					m_obituaries = NULL;
					break;
				} else {
					m_obituaries->RemoveItemsAt(i);
					i--;
					N--;
				}
			}
		}
	}

	return found ? B_OK : B_NAME_NOT_FOUND;
}

void
BpBinder::SendObituary()
{
	if (!m_alive) return;

	m_lock.Lock();
	SVector<Obituary>* obits = m_obituaries;
	m_obituaries = NULL;
	m_alive = false;
	m_lock.Unlock();

#if OBIT_DEBUG_MSGS
	disable_thread_syslog();
	bout << "BpBinder(" << this << SPrintf(":%04x) is sending obituaries ", m_handle) << obits << endl;
	enable_thread_syslog();
#endif

	if (obits == NULL) return;

	const size_t N = obits->CountItems();
	for (size_t i=0; i<N; i++) {
		report_one_death(obits->ItemAt(i));
	}
	
	delete obits;
}

void
BpBinder::report_one_death(const Obituary& obit)
{
	sptr<BBinder> target = obit.target.promote();
	if (target == NULL) return;

	if ((obit.flags&kIsALink) == 0) {
		void* cookie = 0;
		SValue key, value;

		while (obit.method.GetNextItem(&cookie, &key, &value) == B_OK) {
			if (!key.IsWild()) {
				if ((obit.flags&B_NO_TRANSLATE_LINK) == 0) {
					value.JoinItem(B_0_INT32, SValue::WeakBinder(this));
				}
#if OBIT_DEBUG_MSGS
				disable_thread_syslog();
				bout << "Obit to " << target << ": method=" << key << ", params=" << value << endl;
				enable_thread_syslog();
#endif
				target->Invoke(key, value);
			} else {
#if OBIT_DEBUG_MSGS
				disable_thread_syslog();
				bout << "Obit to " << target << ": method=" << value << endl;
				enable_thread_syslog();
#endif
				if ((obit.flags&B_NO_TRANSLATE_LINK) == 0) {
					target->Invoke(value, SValue(B_0_INT32, SValue::WeakBinder(this)));
				} else {
					target->Invoke(key, value);
				}
			}
		}
	} else {
		target->Unlink(this, obit.method, obit.flags&(~kIsALink));
	}
}

bool
BpBinder::IsBinderAlive() const
{
	return m_alive;
}

status_t
BpBinder::PingBinder()
{
	SParcel *data = SParcel::GetParcel();
	SParcel *reply = SParcel::GetParcel();
	status_t result = Transact(B_PING_TRANSACTION, *data, reply);
	SParcel::PutParcel(reply);
	SParcel::PutParcel(data);
	return result;
}

BBinder*
BpBinder::LocalBinder()
{
	return NULL;
}

BpBinder*
BpBinder::RemoteBinder()
{
	return this;
}

void
BpBinder::InitAtom()
{
#if RBINDER_DEBUG_MSGS
	printf("*** BpBinder::InitAtom(): Acquire %p descriptor %ld\n", this, m_handle);
#endif
	SLooper::This()->AcquireHandle(m_handle);
}

status_t
BpBinder::FinishAtom(const void* )
{
#if RBINDER_DEBUG_MSGS
	printf("*** BpBinder::FinishAtom(): Release %p descriptor %ld\n", this, m_handle);
#endif
	SLooper* loop = SLooper::This();
	loop->Process()->StrongHandleGone(this);
	loop->ReleaseHandle(m_handle);
	return B_ERROR;
}

status_t
BpBinder::IncStrongAttempted(uint32_t, const void* )
{
#if RBINDER_DEBUG_MSGS
	printf("*** BpBinder::IncStrongAttempted(): AttemptIncStrong %p descriptor %ld\n", this, m_handle);
#endif
	return SLooper::This()->AttemptAcquireHandle(m_handle);
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
