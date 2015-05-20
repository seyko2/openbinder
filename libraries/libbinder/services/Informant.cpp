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

#include <services/Informant.h>
#include <support/Autolock.h>
#include <support/Looper.h>
#include <support/StdIO.h>
#include "InformList.h"

B_STATIC_STRING_VALUE_LARGE (BV_INFORMANT, "informant",)

// two levels of debugging -- uncomment the _x to see it
#define DB(_x) //_x
#define DBG(_x) //_x

struct _InformantData
{
	SLocker						lock;
	InformList<CallbackInfo>	callbacks;
	InformList<CreationInfo>	creations;
};

// return true if should remove this one.
static bool do_callback(const sptr<BInformant> &This, const CallbackInfo &info, const SValue &key, const SValue &information);
static bool do_creation(const sptr<BInformant> &This, const CreationInfo &info, const SValue &key, const SValue &information);
	


BInformant::BInformant(const SContext& context)
	:BnInformant(context)
{
	DB(bout << "BInformant constructed" << endl);
	
	m_data = new _InformantData;
}

BInformant::~BInformant()
{
	delete m_data;
}

void
BInformant::InitAtom()
{
	BnInformant::InitAtom();
	BHandler::InitAtom();
}

status_t
BInformant::FinishAtom(const void *id)
{
	BnInformant::FinishAtom(id);
	BHandler::FinishAtom(id);
	return B_OK;
}

status_t
BInformant::RegisterForCallback(	const SValue &key,
								const BNS(::palmos::support::)sptr<BNS(palmos::support)::IBinder>& target,
								const SValue &method,
								uint32_t flags,
								const SValue &cookie)
{
	status_t err;
	SAutolock _l(m_data->lock.Lock());
	CallbackInfo info(target, method, flags, cookie);
	err = m_data->callbacks.Add(key, info);
	DB(bout << "BInformant::RegisterForCallback .. called with: " << info << endl;)
	DB(bout << "BInformant::RegisterForCallback ... m_data->callbacks is now:" << endl << indent << m_data->callbacks << dedent;)
	return err;
}

status_t
BInformant::RegisterForCreation(	const SValue &key,
								const sptr<INode>& context,
								const sptr<IProcess>& process,
								const SString &component,
								const SValue &interface,
								const SValue &method,
								uint32_t flags,
								const SValue &cookie)
{
	status_t err;
	SAutolock _l(m_data->lock.Lock());
	CreationInfo info(context, process, component, interface, method, flags, cookie);
	err = m_data->creations.Add(key, info);
	DB(bout << "BInformant::RegisterForCreation .. called with: " << info << endl;)
	DB(bout << "BInformant::RegisterForCreation ... m_data->creations is now:" << endl << indent << m_data->creations << dedent;)
	return err;
}

status_t
BInformant::UnregisterForCallback(	const SValue &key,
									const BNS(::palmos::support::)sptr<BNS(palmos::support)::IBinder>& target,
									const SValue &method,
									uint32_t flags)
{
	status_t err;
	SAutolock _l(m_data->lock.Lock());
	CallbackInfo info(target, method, flags);
	err = m_data->callbacks.Remove(key, info);
	DB(bout << "BInformant::UnregisterForCallback .. called with: " << info << endl;)
	DB(bout << "BInformant::UnregisterForCallback ... m_data->callbacks is now:" << endl << indent << m_data->callbacks << dedent;)
	return err;
}

status_t
BInformant::UnregisterForCreation(	const SValue &key,
									const sptr<INode>& context,
									const sptr<IProcess>& process,
									const SString &component,
									const SValue &interface,
									const SValue &method,
									uint32_t flags)
{
	status_t err;
	SAutolock _l(m_data->lock.Lock());
	CreationInfo info(context, process, component, interface, method, flags);
	err = m_data->creations.Remove(key, info);
	DB(bout << "BInformant::UnregisterForCreation .. called with: " << info << endl;)
	DB(bout << "BInformant::UnregisterForCreation ... m_data->creations is now:" << endl << indent << m_data->creations << dedent;)
	return err;
}

status_t
BInformant::Inform(const SValue &key, const SValue &information)
{
	DB(bout << "BInformant::Inform .... called with key: " << key << "  information: " << information << endl;)

	SSortedVector<CallbackInfo> callbacks;
	SSortedVector<CreationInfo> creations;
	size_t i, count;
	
	status_t callbacksFound, creationsFound;
	{
		SAutolock _l(m_data->lock.Lock());

		DB(bout << "BInformant::m_data->callbacks is now:" << endl << indent << m_data->callbacks << dedent;)
		DB(bout << "BInformant::m_data->creations is now:" << endl << indent << m_data->creations << dedent;)

		callbacksFound = m_data->callbacks.Find(key, &callbacks);
		creationsFound = m_data->creations.Find(key, &creations);
	}
	
	if (callbacksFound == B_OK) {
		count = callbacks.CountItems();
		for (i=0; i<count; i++) {
			const CallbackInfo &info = callbacks.ItemAt(i);
			if (do_callback(this, info, key, information)) {
				SAutolock _l(m_data->lock.Lock());
				m_data->callbacks.Remove(key, info);
			}
		}
	}

	if (creationsFound == B_OK) {
		count = creations.CountItems();
		for (i=0; i<count; i++) {
			const CreationInfo &info = creations.ItemAt(i);
			if (do_creation(this, info, key, information)) {
				SAutolock _l(m_data->lock.Lock());
				m_data->creations.Remove(key, info);
			}
		}
	}
	
	return B_OK;
}

SLocker slowdown;

static bool
do_callback(const sptr<BInformant> &This, const CallbackInfo &info, const SValue &key, const SValue &information)
{
	DB(bout << "BInformant::Performing callback: " << info << endl);

	sptr<IBinder> obj = info.Target();
	
	// if we couldn't promote the reference, remove 
	if (obj == NULL) return true;

	SValue args(B_0_INT32, information);
	args.JoinItem(B_1_INT32, info.Cookie());
	args.JoinItem(B_2_INT32, key);

	if (info.Flags() & B_SYNC_BINDER_LINK) {
		// Synchronous
		DB(bout << " ++++ BInformant -- invoking " << info.Method().AsString() << endl;)
		obj->Invoke(info.Method(), args);
		DB(bout << " ++++ BInformant -- invoked " << info.Method().AsString() << endl;)
	} else {
		DB(bout << "Async!" << endl);
		// Asynchronous
		SMessage msg('call', sysThreadPriorityRaised);
		msg.JoinItem(B_0_INT32, obj);
		msg.JoinItem(B_1_INT32, info.Method());
		msg.JoinItem(B_2_INT32, args);
		//This->HandleMessage(msg);
		This->PostMessage(msg);
	}

	return false;
}

static void
create(const SValue &key, const SValue &information, const SContext& context, const sptr<IProcess>& process,
		const SString &component, const SValue &interface, const SValue &method, const SValue &cookie)
{
	// be safe
	if (context.Root() == NULL) return ;

	// if process is NULL, create a new process
	sptr<IProcess> createProcess = (process != NULL) ? process.ptr() : context.NewProcess(BV_INFORMANT);
	
	// Create the object
	sptr<IBinder> obj = context.RemoteNew(SValue::String(component), createProcess);
	if (obj == NULL) return ;

	// If the interface is undefined, use whatever we have.  Otherwise, pick that one
	if (interface.IsDefined()) {
		obj = obj->Inspect(obj, interface).AsBinder();
		if (obj == NULL) return ;
	}

	// Set up args
	SValue args(B_0_INT32, information);
	args.JoinItem(B_1_INT32, cookie);
	args.JoinItem(B_2_INT32, key);
	
	// Invoke!
	obj->Invoke(method, args);
}

static bool
do_creation(const sptr<BInformant> &This, const CreationInfo &info, const SValue &key, const SValue &information)
{
	DB(bout << "BInformant Performing creation: " << info << endl;)
	
	sptr<IProcess> process = info.Process();

	// if we couldn't promote the reference, remove 
	if (process == NULL && info.Flags() & B_WEAK_BINDER_LINK) return true;
	
	if (info.Flags() & B_SYNC_BINDER_LINK) {
		create(key, information, info.Context(), info.Process(), info.Component(), info.Interface(), info.Method(), info.Cookie());
	} else {
		SMessage msg('crea');
		msg.JoinItem(B_0_INT32, info.Process()->AsBinder());
		msg.JoinItem(B_1_INT32, SValue::String(info.Component()));
		msg.JoinItem(B_2_INT32, info.Interface());
		msg.JoinItem(B_3_INT32, info.Method());
		msg.JoinItem(B_5_INT32, info.Cookie());
		msg.JoinItem(B_6_INT32, key);
		msg.JoinItem(B_7_INT32, information);
		msg.JoinItem(B_8_INT32, info.Context()->AsBinder());
		This->HandleMessage(msg);
		//This->PostMessage(msg);
	}
	
		
	return false;
}


status_t
BInformant::HandleMessage(const SMessage &msg)
{
	switch (msg.What())
	{
		case 'call':
		{
			DB(bout << " **** BInformant::HandleMessage -- \'call\' " << msg[B_1_INT32].AsString() << endl;)
			sptr<IBinder> obj = msg[B_0_INT32].AsBinder();
			if (obj != NULL) {
				obj->Invoke(msg[B_1_INT32], msg[B_2_INT32]);
			}
			DB(bout << " **** BInformant invoked " << msg[B_1_INT32].AsString() << endl;)
			break;
		}
		case 'crea':
		{
			sptr<IProcess> process = IProcess::AsInterface(msg[B_0_INT32]);
			SString component = msg[B_1_INT32].AsString();
			SValue interface = msg[B_2_INT32];
			SValue method = msg[B_3_INT32];
			SValue cookie = msg[B_5_INT32];
			SValue key = msg[B_6_INT32];
			SValue information = msg[B_7_INT32];
			SContext context(interface_cast<INode>(msg[B_8_INT32]));
			create(key, information, context, process, component, interface, method, cookie);
		}
		default:
			break;
	}
	return B_OK;
}



