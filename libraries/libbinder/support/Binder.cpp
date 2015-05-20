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

#include <support/atomic.h>
#include <support/Binder.h>
#include <support/Parcel.h>
#include <support/StdIO.h>
#include <support/SupportDefs.h>
#include <support/TLS.h>
#include <support/HashTable.h>
#include <support/Bitfield.h>
#include <support/Looper.h>
#include <support/KeyedVector.h>
#include <support/Autolock.h>
#include <support/Autobinder.h>
#include <support/Errors.h>
#include <support/Handler.h>
#include <support_p/BinderKeys.h>
#include <support_p/RBinder.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define BPATOM_DEBUG_MSGS 0
#define BINDER_DEBUG_MSGS 0
#define BINDER_DEBUG_PUSH_MSGS 0
#define LINK_DEBUG_MSGS 0

#if BINDER_DEBUG_MSGS
#include <support/StringIO.h>
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {

using namespace palmos::osp;
#endif

struct links_rec {
	links_rec()
		:	targetAtom(NULL), target(NULL)
	{
	};
	
	links_rec(const links_rec &orig)
		:	value(orig.value), targetAtom(orig.targetAtom), target(orig.target), flags(orig.flags)
	{
		if (targetAtom) {
#if LINK_DEBUG_MSGS
			bout << "links_rec " << this << ": adding a copy reference to " << target << endl;
#endif
			if (flags&B_WEAK_BINDER_LINK) targetAtom->IncWeak(this);
			else targetAtom->IncStrong(this);
		}
	};
	
	links_rec(const SValue & k, const sptr<IBinder> &t, uint32_t f)
		:	value(k), targetAtom(t.ptr()), target(t.ptr()), flags(f)
	{
		if (targetAtom) {
#if LINK_DEBUG_MSGS
			bout << "links_rec " << this << ": adding a new reference to " << target << endl;
#endif
			if (flags&B_WEAK_BINDER_LINK) targetAtom->IncWeak(this);
			else targetAtom->IncStrong(this);
		}
	};
	
	~links_rec()
	{
		if (targetAtom) {
#if LINK_DEBUG_MSGS
			bout << "links_rec " << this << ": removing a reference from " << target << endl;
#endif
			if (flags&B_WEAK_BINDER_LINK) targetAtom->DecWeak(this);
			else targetAtom->DecStrong(this);
		}
	}
	
	bool operator==(const links_rec & other) const
	{
		return value == other.value && target == other.target && flags == other.flags;
	}
	
	links_rec& operator=(const links_rec & other)
	{
		if (other.targetAtom) {
#if LINK_DEBUG_MSGS
			bout << "links_rec " << this << ": adding an assign reference to " << target << endl;
#endif
			if (other.flags&B_WEAK_BINDER_LINK) other.targetAtom->IncWeak(this);
			else other.targetAtom->IncStrong(this);
		}
		if (targetAtom) {
#if LINK_DEBUG_MSGS
			bout << "links_rec " << this << ": removing assign reference from " << target << endl;
#endif
			if (flags&B_WEAK_BINDER_LINK) targetAtom->DecWeak(this);
			else targetAtom->DecStrong(this);
		}
		value = other.value;
		targetAtom = other.targetAtom;
		target = other.target;
		flags = other.flags;
		return *this;
	}
	
	SValue value;
	SAtom* targetAtom;
	IBinder* target;
	uint32_t flags;
};

//B_IMPLEMENT_SIMPLE_TYPE_FUNCS(links_rec);

static void do_push(const SKeyedVector<sptr<IBinder>, SValue>& sendThese)
{
	const size_t count = sendThese.CountItems();
	for (size_t i = 0 ; i < count ; i++)
	{
		SValue dummiResult;
		const sptr<IBinder>& target = sendThese.KeyAt(i);
		const SValue & value = sendThese.ValueAt(i);
#if BINDER_DEBUG_PUSH_MSGS 
		bout << "Pushing to target: " << SValue::Binder(target) << endl;
#endif
		target->Effect(value, B_WILD_VALUE, B_UNDEFINED_VALUE, &dummiResult);
	}		
}

class AsyncHandler : public BHandler
{
public:
	AsyncHandler()
	{
	}
	
	virtual status_t HandleMessage(const SMessage& msg)
	{
		if (msg.What() == 'asyn')
		{
			SKeyedVector<sptr<IBinder>, SValue>* sendThese = (SKeyedVector<sptr<IBinder>, SValue>*)((void*)msg[B_0_INT32].AsInt32());
			do_push(*sendThese);
			delete sendThese;
			sendThese = NULL;
		}

		return B_OK;
	}
};

struct BBinder::extensions {
	extensions();
	extensions(const extensions& other);
	~extensions();
	SLocker lock;
	SKeyedVector<SValue,SVector<links_rec> > links;
	SVector<links_rec> other_links;
	sptr<AsyncHandler> handler;			// unused if LIBBE_BOOTSTRAP
};

struct EffectCache {
	const SValue *in;
	const SValue *inBindings;
	const SValue *outBindings;
	SValue *out;
};

B_STATIC_STRING_VALUE_SMALL (key_result, "res", );

#define EFFECT_CACHE ((EffectCache*)tls_get(gBinderTLS))

BBinder::extensions::extensions()
	:	lock("BBinder::extensions access")
{
}

BBinder::extensions::extensions(const extensions& other)
	:	lock("BBinder::extensions access"),
		other_links(other.other_links)
{
	links.SetTo(other.links);
}

BBinder::extensions::~extensions()
{
}

/**************************************************************************************/

IInterface::IInterface()
{
}

IInterface::~IInterface()
{
}

sptr<IBinder>
IInterface::AsBinder()
{
	if (this != NULL)
		return AsBinderImpl();
	
	return NULL;
}

sptr<IBinder>
IInterface::AsBinderImpl()
{
	return NULL;
}

sptr<const IBinder>
IInterface::AsBinder() const
{
	if (this != NULL)
		return AsBinderImpl();
	
	return NULL;
}

sptr<const IBinder>
IInterface::AsBinderImpl() const
{
	return NULL;
}

sptr<IInterface>
IInterface::ExecAsInterface(const sptr<IBinder>& binderIn,
							const SValue& descriptor,
							instantiate_proxy_func proxy_func,
							status_t* out_error)
{
	sptr<IInterface> interface;
	
	if (binderIn != NULL) {
		sptr<IBinder> binder = NULL;
		SValue inspected = binderIn->Inspect(binderIn, descriptor);
		// if we are dealing with a binder reported dead, then
		// mimic AsInterfaceNoInspect actions, just create proxy
		binder = inspected.AsBinder();
		if (binder == NULL) {
			if (inspected.ErrorCheck() == B_BINDER_DEAD) {
				binder = binderIn;
			}
		}
		if (binder != NULL) {
			interface = binder->InterfaceFor(descriptor);
			if (interface == NULL) interface = proxy_func(binder);
			if (out_error) *out_error = B_OK;
		} else if (out_error) {
			*out_error = B_BINDER_BAD_INTERFACE;
		}
	} else if (out_error) {
		// Note that AsInterface on a NULL object is NOT counted as an error!!!
		// This is because NULL is not typed -- or rather, it is an acceptable
		// value for any interface.  Another way to look at it is what happens
		// in unmarshalling -- if someone gives us a NULL IBinder, and we do
		// an AsInterface to get the interface the function expects, we should
		// pass this through without creating a type error.
		*out_error = B_OK;
	}

	return interface;
}

sptr<IInterface>
IInterface::ExecAsInterfaceNoInspect(const sptr<IBinder>& binder,
									 const SValue& descriptor,
									 instantiate_proxy_func proxy_func,
									 status_t* out_error)
{
	sptr<IInterface> interface;
	
	if (binder != NULL) {
		interface = binder->InterfaceFor(descriptor);
		if (interface == NULL) interface = proxy_func(binder);
	}
	
	if (out_error) {
		// No type checking -- always succeed.
		*out_error = B_OK;
	}

	return interface;
}

/**************************************************************************************/

status_t 
IBinder::Put(const SValue &in)
{
	return Effect(in,B_WILD_VALUE,B_UNDEFINED_VALUE,NULL);
}

SValue 
IBinder::Get(const SValue &bindings) const
{
	SValue out;
	const_cast<IBinder*>(this)->Effect(B_UNDEFINED_VALUE,B_UNDEFINED_VALUE,bindings,&out);
#if BINDER_DEBUG_MSGS
	berr << "IBinder::Get " << out << endl;
#endif
	return out;
}

SValue 
IBinder::Get() const
{
	return Get(B_WILD_VALUE);
}

SValue 
IBinder::Invoke(const SValue &func, const SValue &args)
{
	SValue out;
	Effect(SValue(func,args),B_WILD_VALUE,func,&out);
	return out;
}

SValue 
IBinder::Invoke(const SValue &func) // thunk!
{
	SValue out;
	// XXX The parameters used to be B_WILD_VALUE.  However,
	// that is REALLY NOT GOOD, because it means when looking
	// up during unmarshalling we always get results back.
	// Can't use undefined, because we need the argument name
	// mapping here.  So maybe there should be a "null" value.
	// There isn't right now, however, so how about...  0.
	// Whatever.
	Effect(SValue(func,SValue::Int32(0)),B_WILD_VALUE,func,&out);
	return out;
}

BBinder*
IBinder::LocalBinder()
{
	return NULL;
}

BNS(::palmos::osp::) BpBinder*
IBinder::RemoteBinder()
{
	return NULL;
}


/**************************************************************************************/

#define SUPPORTS_AUTOBINDER_DIRECT_CALL 0

struct effect_call_state
{
	SValue args, outKey;
	uint32_t which;
};

static inline status_t call_effect_func(const effect_action_def& action,
		const effect_call_state& state, const sptr<IInterface>& target, SValue* out)
{
	status_t err = B_OK;
	SValue returnValue;
	unsigned char * object = NULL;
	if (action.parameters) {
		object = reinterpret_cast<unsigned char *>(target.ptr());
		object += action.parameters->classOffset;
	}
	
	switch (state.which)
	{
		case B_INVOKE_ACTION:
			if (action.invoke) {
				returnValue = action.invoke(target, state.args);
			}
#if SUPPORTS_AUTOBINDER_DIRECT_CALL
			else if (action.parameters && action.parameters->invoke) {
				err = InvokeAutobinderFunction(	action.parameters->invoke,
												object,
												state.args,
												&returnValue);
			}
#endif
			else {
				return B_MISMATCHED_VALUES;
			}
			if (out) out->JoinItem(state.outKey, returnValue);
			break;
		
		case B_PUT_ACTION:
			if (action.put) {
				err = action.put(target, state.args);
			}
#if SUPPORTS_AUTOBINDER_DIRECT_CALL
			else if (action.parameters && action.parameters->put) {
				err = InvokeAutobinderFunction(	action.parameters->put,
												object,
												SValue(SValue::Int32(0), state.args),
												&returnValue);
			}
#endif
			else {
				return B_MISMATCHED_VALUES;
			}
			break;
		
		case B_GET_ACTION:
			if (action.get) {
				returnValue = action.get(target);
			}
#if SUPPORTS_AUTOBINDER_DIRECT_CALL
			else if (action.parameters && action.parameters->get) {
				err = InvokeAutobinderFunction(	action.parameters->get,
												object,
												B_UNDEFINED_VALUE,
												&returnValue);
			}
#endif
			else {
				return B_MISMATCHED_VALUES;
			}
			out->JoinItem(state.outKey, returnValue);
			break;
		default:
			return B_BAD_VALUE;
	}
	
//	SValue nullll("NULL");
//	bout << "out is " << (out ? *out : nullll) << endl;
	
	return err;
}

static uint32_t finish_logic(bool hasIn, bool hasOut, const effect_action_def &action)
{
	bool hasPut, hasGet, hasInvoke;
	if (action.parameters) {
		hasPut = action.parameters->put != NULL;
		hasGet = action.parameters->get != NULL;
		hasInvoke = action.parameters->invoke != NULL;
	} else {
		hasPut = action.put != NULL;
		hasGet = action.get != NULL;
		hasInvoke = action.invoke != NULL;
	}
					
	// Select the appropriate action to be performed.  Only one 
	// hook will be called, whichever best matches the effect arguments. 

	// Do an invoke if it is defined and we have both an input 
	// and output, or only an input and no "put" action. 
	if (hasInvoke && hasIn && (hasOut || !hasPut)) 
		return B_INVOKE_ACTION;

	// Do a put if it is defined and we have an input. 
	else if (hasPut && hasIn) 
		return B_PUT_ACTION;
	
	// Do a get if it is defined and we have an output. 
	else if (hasGet && hasOut) 
		return B_GET_ACTION;
		
	else return B_NO_ACTION;
}

status_t execute_effect(const sptr<IInterface>& target,
						const SValue &in, const SValue &inBindings,
						const SValue &outBindings, SValue *out,
						const effect_action_def* actions, size_t num_actions,
						uint32_t flags)
{
	const SValue told(inBindings * in);
	const size_t inN = told.CountItems();
	const size_t outN = outBindings.CountItems();
	effect_call_state state;
	bool hasIn, hasOut;
	status_t result = B_OK;
	
	// Optimize for the common case of the actions being in value-sorted order,
	// and only one action is being performed.
	if (	(flags&B_ACTIONS_SORTED_BY_KEY) != 0 &&
			(hasIn=(inN <= 1)) &&
			(hasOut=(outN <= 1)) ) {
		SValue key;
		void* cookie;
		if (inN) {
			cookie = NULL;
			told.GetNextItem(&cookie, &key, &state.args);
		}
		if (outN) {
			SValue tmpKey;
			cookie = NULL;
			outBindings.GetNextItem(&cookie, &state.outKey, &tmpKey);
			// If this is a get action, store the key; if it could be an
			// invoke, make sure the output key matches the input.
			if (!inN) key = tmpKey;
			else if (key != tmpKey) key.Undefine();
		}
		
		#if 0
		bout	<< "execute_effect() short path:" << endl << indent
				<< "Told: " << told << endl
				<< "outBindings: " << outBindings << endl
				<< "key: " << key << endl
				<< "args: " << state.args << endl
				<< "outKey: " << state.outKey << endl << dedent;
		#endif
		
		if (key.IsDefined()) {
			ssize_t mid, low = 0, high = ((ssize_t)num_actions)-1;
			while (low <= high) {
				mid = (low + high)/2;
				const int32_t cmp = actions[mid].key().Compare(key);
				if (cmp > 0) {
					high = mid-1;
				} else if (cmp < 0) {
					low = mid+1;
				} else {
					// Found the action; determine which function to call.
					if (!actions[mid].put && !actions[mid].invoke
							 && !(actions[mid].parameters && actions[mid].parameters->put)
							 && !(actions[mid].parameters && actions[mid].parameters->invoke))
						hasIn = false;
					if (!actions[mid].get && !actions[mid].invoke
							 && !(actions[mid].parameters && actions[mid].parameters->get)
							 && !(actions[mid].parameters && actions[mid].parameters->invoke))
						hasOut = false;
					#if 0
					bout	<< "Found action #" << mid << ": in=" << hasIn
							<< ", out=" << hasOut << endl;
					#endif
					
					state.which = finish_logic(hasIn && inN>0, hasOut && outN>0, actions[mid]);
					return call_effect_func(actions[mid], state, target, out);
				}
			}
		}
		
		#if 0
		bout	<< "execute_effect() short path FAILED:" << endl << indent
				<< "Told: " << told << endl
				<< "outBindings: " << outBindings << endl
				<< "key: " << key << endl
				<< "args: " << state.args << endl
				<< "outKey: " << state.outKey << endl << dedent;
		#endif
	}
	
	// Unoptimized version when effect_action_def array is not
	// in sorted order or multiple bindings are supplied.
	
	// Iterate through all actions and execute in order.

	while (num_actions > 0 && result == B_OK) {

	#if 0
	bout	<< "execute_effect() long path:" << endl << indent
			<< "action.key: " << actions->key() << endl
			<< "Told: " << told << endl
			<< "outBindings: " << outBindings << endl
			<< "args: " << state.args << endl
			<< "outKey: " << state.outKey << endl << dedent;
	#endif
	
		// First check to see if there are any inputs or outputs in
		// the effect for this action binding.
		hasIn		= (	(actions->put || actions->invoke
								|| (actions->parameters && actions->parameters->put)
								|| (actions->parameters && actions->parameters->invoke))
							&& (state.args = told[actions->key()]).IsDefined() );
		hasOut	= (actions->get || actions->invoke
								|| (actions->parameters && actions->parameters->get)
								|| (actions->parameters && actions->parameters->invoke));
		
		if (hasOut) {
			state.outKey = (outBindings * SValue(actions->key(), B_WILD_VALUE)); // ??? outBindings[actions->key()]; 
			if (state.outKey.IsDefined()) {
				void* cookie = NULL;
				SValue key, val;
				state.outKey.GetNextItem(&cookie, &key, &val);
				state.outKey = key;
			} else {
				hasOut = false;
			}
		}
		
		state.which = finish_logic(hasIn, hasOut, *actions);
		if (state.which != B_NO_ACTION) {
			result = call_effect_func(*actions, state, target, out);
		}
		
		actions = reinterpret_cast<const effect_action_def*>(
			reinterpret_cast<const uint8_t*>(actions) + actions->struct_size);
		num_actions--;
	}
	
	return result;
}



/**************************************************************************************/

BBinder::BBinder()
	:	m_context(SContext()),
		m_extensions(NULL)
{
}

BBinder::BBinder(const SContext& context)
	: m_context(context)
	, m_extensions(NULL)
{
}

BBinder::BBinder(const BBinder& other)
	: SAtom()
	, IBinder()
	, m_context(other.Context())
	, m_extensions(NULL)
{
	if (other.m_extensions) {
		other.m_extensions->lock.Lock();
		if (other.m_extensions->links.CountItems() > 0 ||
				other.m_extensions->other_links.CountItems() > 0) {
			m_extensions = new extensions(*other.m_extensions);
		}
		other.m_extensions->lock.Unlock();
	}
}

BBinder::~BBinder()
{
	if (m_extensions) {
		// XXX Don't acquire lock.  This is the last reference, so
		// nobody else can come in here...  and we don't want to be
		// holding the lock while calling out into destructors.
		// (This is actually a real deadlock case that has been seen.)
		size_t N = m_extensions->links.CountItems();
		size_t i;
		for (i=0; i<N; i++) {
			const SVector<links_rec>& links = m_extensions->links.ValueAt(i);
			const size_t N2 = links.CountItems();
			for (size_t i2=0; i2<N2; i2++) {
				if (links[i2].targetAtom->AttemptIncStrong(this)) {
					links[i2].target->UnlinkToDeath(this, B_UNDEFINED_VALUE, B_UNLINK_ALL_TARGETS);
					links[i2].targetAtom->DecStrong(this);
				}
			}
		}
		N = m_extensions->other_links.CountItems();
		for (i=0; i<N; i++) {
			if (m_extensions->other_links[i].targetAtom->AttemptIncStrong(this)) {
				m_extensions->other_links[i].target->UnlinkToDeath(this, B_UNDEFINED_VALUE, B_UNLINK_ALL_TARGETS);
				m_extensions->other_links[i].targetAtom->DecStrong(this);
			}
		}
		delete m_extensions;
	}
}

void
BBinder::BeginEffectContext(const EffectCache &cache)
{
	tls_set(gBinderTLS,const_cast<EffectCache*>(&cache));
}

void
BBinder::EndEffectContext()
{
	tls_set(gBinderTLS,NULL);
}

SValue
BBinder::Inspect(const sptr<IBinder>&, const SValue &, uint32_t /*flags*/)
{
	return SValue::Undefined();
}

sptr<IInterface>
BBinder::InterfaceFor(const SValue &/*descriptor*/, uint32_t /*flags*/)
{
	return sptr<IInterface>(NULL);
}

status_t
BBinder::Link(const sptr<IBinder>& node, const SValue &binding, uint32_t flags)
{
//	bout << "Adding Link: " << (void*) flags << ", binding: " << binding << endl;

	ssize_t result = B_OK;
	extensions *e;

	if (!m_extensions) {
		e = new extensions;
		if (!compare_and_swap32(reinterpret_cast<volatile int32_t*>(&m_extensions),
								0,
								reinterpret_cast<int32_t>(e))) {
			delete e;
		}
	}
	
	if ((e=m_extensions) == NULL) return B_NO_MEMORY;
	
	if (!binding.IsWild()) {
		SValue senderKey, targetKey;
		SValue remnants;
		void * cookie = NULL;
		bool found;
		while (B_OK == binding.GetNextItem(&cookie, &senderKey, &targetKey)) {
			if (senderKey.IsSimple()) {
				uint32_t finalFlags = flags;
				if (HoldRefForLink(senderKey, flags)) {
					IncStrong(this);
					finalFlags |= kLinkHasRef;
				}

				e->lock.Lock();
				SVector<links_rec> *linkRec = &e->links.EditValueFor(senderKey, &found);
				if (!found) {
					result = e->links.AddItem(senderKey, SVector<links_rec>());
					if (result < B_OK) break;
					linkRec = &e->links.EditValueAt(result);
				}
#if LINK_DEBUG_MSGS
				bout << "Adding Link: " << this << " (a=" << (SAtom*)this
					<< ") -> " << node << " (a=" << (SAtom*)(node.ptr())
					<< ") / " << targetKey << endl;
#endif
				result = linkRec->AddItem(links_rec(targetKey, node, finalFlags));
				e->lock.Unlock();

				if (result < B_OK && (finalFlags&kLinkHasRef)) {
					DecStrong(this);
				}
			} else {
				remnants.JoinItem(senderKey, targetKey, B_NO_VALUE_FLATTEN);
			}
		}
		if (remnants.IsDefined()) {
			uint32_t finalFlags = flags;
			if (HoldRefForLink(B_UNDEFINED_VALUE, flags)) {
				IncStrong(this);
				finalFlags |= kLinkHasRef;
			}

			e->lock.Lock();
			result = e->other_links.AddItem(links_rec(remnants, node, finalFlags));
			e->lock.Unlock();

			if (result < B_OK && (finalFlags&kLinkHasRef)) {
				DecStrong(this);
			}
		}
	} else {
#if LINK_DEBUG_MSGS
		bout << "Adding Wild Link: " << this << " (a=" << (SAtom*)this
			<< ") -> " << node << " (a=" << (SAtom*)(node.ptr())
			<< ")" << endl;
#endif
		uint32_t finalFlags = flags;
		if (HoldRefForLink(B_UNDEFINED_VALUE, flags)) {
			IncStrong(this);
			finalFlags |= kLinkHasRef;
		}

		e->lock.Lock();
		result = e->other_links.AddItem(links_rec(binding, node, finalFlags));
		e->lock.Unlock();

		if (result < B_OK && (finalFlags&kLinkHasRef)) {
			DecStrong(this);
		}
	}
	
	// XXX This could get out of sync with the list of links,
	// if clients call Unlink() differently.
	if (result >= B_OK) {
		node->LinkToDeath(this, binding, flags|kIsALink);
	}

	return result >= B_OK ? B_OK : result;
}

status_t 
BBinder::Unlink(const wptr<IBinder>& node, const SValue &binding, uint32_t flags)
{
//	bout << "Unlink: node=" << node << ", binding=" << binding
//		<< ", flags=" << (void*)flags << endl;
	
	if (!m_extensions) return B_ERROR;
	status_t err = B_OK;
	size_t i;

	extensions *e = m_extensions;
	e->lock.Lock();

	node.unsafe_ptr_access()->UnlinkToDeath(this, binding, flags|kIsALink);

	if ((flags&B_UNLINK_ALL_TARGETS) != 0) {
		i = e->links.CountItems();
		while (i > 0) {
			--i;
			SVector<links_rec> &linkRec = e->links.EditValueAt(i);
			size_t j = linkRec.CountItems();
			while (j > 0) {
				--j;
				const links_rec& link = linkRec[j];
				if (node == link.target) {
#if LINK_DEBUG_MSGS
					bout << "Removing Target: " << this << " -> " << node << " / " << link.value << endl;
#endif
					if (link.flags&kLinkHasRef) DecStrong(this);
					linkRec.RemoveItemsAt(j);
				}
			}
			if (linkRec.CountItems() == 0) {
#if LINK_DEBUG_MSGS
				bout << "Removed last target for " << e->links.KeyAt(i) << "!!" << endl;
#endif
				e->links.RemoveItemsAt(i);
			}
		}
		
		i = e->other_links.CountItems();
		while (i > 0) {
			--i;
			const links_rec& link = e->other_links[i];
			if (node == link.target) {
#if LINK_DEBUG_MSGS
				bout << "Removing Wild Target: " << this << " -> " << node << endl;
#endif
				if (link.flags&kLinkHasRef) DecStrong(this);
				e->other_links.RemoveItemsAt(i);
			}
		}
		
	} else if (!binding.IsWild()) {
		SValue senderKey, targetKey;
		SValue remnants;
		void * cookie = NULL;
		bool found;
		while (B_OK == binding.GetNextItem(&cookie, &senderKey, &targetKey)) {
			if (senderKey.IsSimple()) {
				SVector<links_rec> &linkRec = e->links.EditValueFor(senderKey, &found);
				if (found) {
					i = linkRec.CountItems();
					while (i > 0) {
						--i;
						const links_rec& link = linkRec[i];
						if (node == link.target && (link.flags&~kPrivateLinkFlagMask) == flags &&
								link.value == targetKey) {
#if LINK_DEBUG_MSGS
							bout << "Removing Link: " << this << " -> " << node << " / " << targetKey << endl;
#endif
							if (link.flags&kLinkHasRef) DecStrong(this);
							linkRec.RemoveItemsAt(i);
						}
					}
					if (linkRec.CountItems() == 0) {
#if LINK_DEBUG_MSGS
						bout << "Removed last link for " << senderKey << "!!" << endl;
#endif
						e->links.RemoveItemsAt(e->links.IndexOf(senderKey));
					}
				}
			} else {
				remnants.JoinItem(senderKey, targetKey, B_NO_VALUE_FLATTEN);
			}
		}
		if (remnants.IsDefined()) {
			// XXX TO DO: Remove these things.
			ErrFatalError("Hey, why don't YOU implement this?  It'll be fun!");
		}
	} else {
		i = e->other_links.CountItems();
		while (i > 0) {
			--i;
			const links_rec& link = e->other_links[i];
			if (node == link.target && (link.flags&~kPrivateLinkFlagMask) == flags && binding == link.value) {
#if LINK_DEBUG_MSGS
				bout << "Removing Wild Link: " << this << " -> " << node << endl;
#endif
				if (link.flags&kLinkHasRef) DecStrong(this);
				e->other_links.RemoveItemsAt(i);
			}
		}
	}
	
	e->lock.Unlock();
	
	return err;
}

bool
BBinder::IsLinked() const
{
	if (!m_extensions) return false;
	// Don't need to lock, since calling CountItems() will not crash if
	// done while someone else is accessing the vector, and while its result
	// may be wrong in that case...  well, it could be wrong as soon as we
	// unlock, anyway.
	//m_extensions->lock.Lock();
	bool linked = m_extensions->links.CountItems() > 0 || m_extensions->other_links.CountItems() > 0;
	//m_extensions->lock.Unlock();
	return linked;
}

status_t 
BBinder::Effect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)
{
	EffectCache cache;

	// Remember any previous effect cache context.
	EffectCache *lastCache = (EffectCache*)tls_get(gBinderTLS);
	
	if (out) {
		// Start a new effect cache context where Push() should go.
		cache.in = &in;
		cache.inBindings = &inBindings;
		cache.outBindings = &outBindings;
		cache.out = out;
		BeginEffectContext(cache);
	} else {
		// We are not expecting any outputs to Push() in to, but we don't
		// want a Push() call here to modify the caller's effect cache.
		EndEffectContext();
	}
	
	const status_t result(HandleEffect(in, inBindings, outBindings, out));

	// Restore the previous effect context.
	if (lastCache) BeginEffectContext(*lastCache);
	else EndEffectContext();
	
	return result;
}

status_t BBinder::AutobinderPut(const BAutobinderDef* def, const void* value)
{
	SValue v;
	status_t err = parameter_to_value(def->put->returnType, def->put->returnMarshaller, value, &v);
	if (err != B_OK) return err;
	return Put(SValue(def->key(), v));
}

status_t BBinder::AutobinderGet(const BAutobinderDef* def, void *result)
{
	SValue v = Get(def->key());
	return parameter_from_value(def->get->returnType, def->get->returnMarshaller, v, result);
}

status_t BBinder::AutobinderInvoke(const BAutobinderDef* def, void** params, void *result)
{
	SValue args;
	status_t err;
	SValue v;
	size_t i;
	
	// in and inout parameters
	for (i=0; i<def->invoke->paramCount; i++) {
		const BParameterInfo &p = def->invoke->paramTypes[i];
		if (p.direction & B_IN_PARAM) {
			err = parameter_to_value(p.type, p.marshaller, params[i], &v);
			if (err != B_OK) return err;
			args.JoinItem(SValue::Int32(i), v);
		}
	}
	
	// invoke it!
	v = Invoke(def->key(), args);

	// return value
	err = parameter_from_value(def->invoke->returnType, def->invoke->returnMarshaller, v[key_result], result);
	if (err != B_OK) return err;

	// inout and out parameters
	for (i=0; i<def->invoke->paramCount; i++) {
		const BParameterInfo &p = def->invoke->paramTypes[i];
		if (p.direction & B_OUT_PARAM) {
			err = parameter_from_value(p.type, p.marshaller, v[SValue::Int32(i)], params[i]);
			if (err != B_OK) return err;
		}
	}
	
	return B_OK;
}


status_t
BBinder::Transact(uint32_t code, SParcel& data, SParcel* reply, uint32_t /*flags*/)
{
	if (code == B_INVOKE_TRANSACTION)
	{
		SValue name;
		SValue arg;
		int32_t count = 0;
		SValue args;
		SValue out;
		status_t err;
		
		// skip the index
		data.ReadInt32();

		// the function name
		name.Unarchive(data);

		// in and inout parameters
		while (arg.Unarchive(data) >= 0) {
			args.JoinItem(SValue::Int32(count), arg);
			count++;
		}
		if (count == 0) args = B_0_INT32;

		//if (name.AsString() == "Interface")
		//	bout << "BBinder::Transact/INVOKE calling Effect with name: "
		//			<< name << "  and args: " << args << endl;
		err = Effect(SValue(name,args), B_WILD_VALUE, name, &out);
		//if (name.AsString() == "Interface")
		//	bout << "BBinder::Transact/INVOKE out value: " << out << endl;
		
		if (reply && err == B_OK) {
			size_t required = 0;

			// 1. calculate how much space is needed in the parcel
			// return value
			SValue returned = out[key_result];
			required += returned.ArchivedSize();
				
			// inout and out parameters
			SValue out_key, out_value;
			int32_t i, j, out_count = out.CountItems()-(returned.IsDefined()?1:0);
			for (i=0, j=0; j<out_count; i++) {
				SValue out_param = out[SValue::Int32(i)];
				if (out_param.IsDefined()) {
					j++;
				}
				required += out_param.ArchivedSize();
			}

			// 2. allocate in the parcel
			err = reply->Reserve(required);
			if (err != B_OK) return err;
			
			// 3. write the parcel
			// return value
			reply->WriteValue(returned);
				
			// inout and out parameters
			for (i=0, j=0; j<out_count; i++) {
				SValue out_param = out[SValue::Int32(i)];
				if (out_param.IsDefined()) {
					j++;
				}
				reply->WriteValue(out_param);
				//if (name.AsString() == "Interface")
				//	bout << "BBinderTransact/INVOKE writing reply " << i
				//			<< " as " << out_param << endl;
			}

			
			// Send it back.
			reply->Reply();
		}

		return err;	
	}
	else if (code == B_INSPECT_TRANSACTION)
	{
		data.ReadInt32();	// skip header.
		sptr<IBinder> caller = data.ReadBinder();
		SValue which = data.ReadValue();
		int32_t flags = data.ReadInt32();

		const SValue result = Inspect(caller, which, flags);
		reply->Reserve(result.ArchivedSize());
		return reply->WriteValue(result);
	}
	else if (code == B_GET_TRANSACTION)
	{
		// the parcel contains a string for the name of the property to put
		SValue name;
		SValue replyValue;

		// skip the index
		data.ReadInt32();

		name.Unarchive(data);

		status_t status = Effect(B_UNDEFINED_VALUE, B_UNDEFINED_VALUE, name, &replyValue);
		
		if (reply) {
			if (status >= B_OK) {
				status = reply->SetValues(&replyValue, NULL);
			}
			// Return any error code.
			if (status < B_OK) reply->Reference(NULL, status);
			// Send it back.
			reply->Reply();
		}

		return status;
	}
	else if (code == B_PUT_TRANSACTION)
	{
		// the parcel contains a string for the name of the property to put and then
		// a value that is the 'value' for that property put
		SValue name;
		SValue val;
		SValue replyValue;
		
		// skip the index
		data.ReadInt32();

		name.Unarchive(data);
		val.Unarchive(data);
		
		SValue in(name, val);
				
		return Effect(in, B_WILD_VALUE, B_UNDEFINED_VALUE, &replyValue);
	}
	else if (code == B_EFFECT_TRANSACTION)
	{
		SValue values[3];
		SValue replyValue;
		SValue val;
		
		ssize_t status = data.GetValues(3, values);
		if (status < B_OK) return status;
		
#if BINDER_DEBUG_MSGS
		berr << "BBinder::Transact " << this << " {" << endl << indent;
		if (status >= 1) berr << "Value 1: " << values[0] << endl;
		if (status >= 2) berr << "Value 2: " << values[1] << endl;
		if (status >= 3) berr << "Value 3: " << values[2] << endl;
		berr << dedent << "}" << endl;
#endif

		if (status == 1)
			status = Effect(values[0],B_WILD_VALUE,B_UNDEFINED_VALUE,&replyValue);
		else if (status == 2)
			status = Effect(values[0],values[1],B_UNDEFINED_VALUE,&replyValue);
		else if (status == 3)
			status = Effect(values[0],values[1],values[2],&replyValue);
	
		if (reply) {
			if (status >= B_OK) {
				status = reply->SetValues(&replyValue, NULL);
#if BINDER_DEBUG_MSGS
				berr << "BBinder::Transact reply: " << endl << indent;
				berr << replyValue << endl << reply << endl << dedent;
#endif
			}
			// Return any error code.
			if (status < B_OK) reply->Reference(NULL, status);
			// Send it back.
			reply->Reply();
		}
		data.Free();
		return status >= B_OK ? B_OK : status;

	}
	else if (code == B_LINK_TRANSACTION)
	{
		data.ReadInt32();	// skip header.
		sptr<IBinder> node = data.ReadBinder();
		SValue binding = data.ReadValue();
		int32_t flags = data.ReadInt32();

		reply->Reserve(sizeof(int32_t));
		return reply->WriteInt32(Link(node, binding, flags));
	}
	else if (code == B_UNLINK_TRANSACTION)
	{
		data.ReadInt32();	// skip header.
		wptr<IBinder> node = data.ReadWeakBinder();
		SValue binding = data.ReadValue();
		int32_t flags = data.ReadInt32();

		reply->Reserve(sizeof(int32_t));
		return reply->WriteInt32(Unlink(node, binding, flags));
	}
	else if (code == B_PING_TRANSACTION)
	{
		status_t status = PingBinder();
		if (reply) {
			if (status < B_OK) reply->Reference(NULL, status);
			reply->Reply();
		}
		return status;
	}
	
	return B_BINDER_UNKNOWN_TRANSACT;
}

status_t
BBinder::LinkToDeath(	const sptr<BBinder>& /*target*/,
						const SValue &/*method*/,
						uint32_t /*flags*/)
{
	/* As far as the local world is concerned, we can never die. */
	return B_OK;
}

status_t
BBinder::UnlinkToDeath(	const wptr<BBinder>& /*target*/,
						const SValue &/*method*/,
						uint32_t /*flags*/)
{
	return B_OK;
}

bool
BBinder::IsBinderAlive() const
{
	return true;
}

status_t
BBinder::PingBinder()
{
	return B_OK;
}

BBinder*
BBinder::LocalBinder()
{
	return this;
}

BNS(::palmos::osp::) BpBinder*
BBinder::RemoteBinder()
{
	return NULL;
}

status_t 
BBinder::HandleEffect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out)
{
	SValue told = inBindings * in;
	SValue outRemains = outBindings;

	SValue key, value;
	void* cookie;

	cookie = NULL;
	while (told.GetNextItem(&cookie, &key, &value) == B_OK) {
		bool isCalled = false;
		if (outRemains.IsDefined()) {
			SValue map, result;
			if (outRemains == key) {
				if (Called(key, value, &result) == B_OK) {
					out->JoinItem(B_WILD_VALUE, result);
					outRemains.Undefine();
					isCalled = true;
				}
			} else {
				// This really sucks.  We should probably try to iterate over
				// "outBindings" at the top level, instead of "told".
				SValue key2, value2;
				void* cookie2 = NULL;
				while (!isCalled && outRemains.GetNextItem(&cookie2, &key2, &value2) == B_OK) {
					if (value2 == key) {
						if (Called(key, value, &result) == B_OK) {
							out->JoinItem(key2, result);
							outRemains.Remove(SValue(key, B_WILD_VALUE));
							isCalled = true;
						}
					}
				}
			}
		}
		if (!isCalled) {
			Told(key, value);
		}
	}

	cookie = NULL;
	while (outRemains.GetNextItem(&cookie, &key, &value) == B_OK) {
		SValue result;
		if (Asked(value, &result) == B_OK) {
			out->JoinItem(key, result);
		}
	}
	
	return B_OK;
}

status_t
BBinder::Told(const SValue&, const SValue&)
{
	return B_OK;
}

status_t
BBinder::Asked(const SValue&, SValue*)
{
	return B_OK;
}
 
status_t
BBinder::Called(const SValue&, const SValue&, SValue*)
{
	return B_OK;
}

class PushMaps
{
public:
	PushMaps() { m_asyncGroup = NULL; }

	// Add a target and resolved binding to the set to push to.
	status_t Add(const sptr<IBinder>& target, const SValue& key, const SValue& binding, uint32_t flags, sptr<AsyncHandler>* handler);

	/*! After all targets have been set up, call DoPush */
	status_t DoPush(const sptr<AsyncHandler>& handler);

private:
	// The async group needs to be dynamically allocated
	// so that it can be deleted by the handler
	SKeyedVector<sptr<IBinder>, SValue> m_syncGroup;
	SKeyedVector<sptr<IBinder>, SValue>* m_asyncGroup;
};

status_t 
PushMaps::Add(const sptr<IBinder>& target, const SValue& key, const SValue& binding, uint32_t flags, sptr<AsyncHandler>* handler) 
{
	SKeyedVector<sptr<IBinder>, SValue>* group;
	if (flags&B_SYNC_BINDER_LINK) {
		group = &m_syncGroup;
	}
	else {
		// If we are dealing with an async link, then 
		// make sure we have allocated the async group,
		// and that we have an asyc handler
		if (m_asyncGroup == NULL) {
			m_asyncGroup = new SKeyedVector<sptr<IBinder>, SValue>();
			if (m_asyncGroup == NULL) {
				return B_NO_MEMORY;
			}
#if !LIBBE_BOOTSTRAP
			if (*handler == NULL) {
				*handler = new AsyncHandler();
			}
#endif
		}
		group = m_asyncGroup;
	}

	// Now add the binding to our set of links
	bool found;
	SValue& targetOut = group->EditValueFor(target, &found);
	if (found) {
		// found - join in current key/value depending on translate flag
		if (flags & B_NO_TRANSLATE_LINK) {
#if BINDER_DEBUG_PUSH_MSGS 
			bout << "Pushing Notification: " << key << endl;
#endif
			targetOut.Join(key);
		} else {
#if BINDER_DEBUG_PUSH_MSGS
			bout	<< "Pushing Link:" << endl
					<< indent << "link=" << key << endl
					<< "val=" << binding[key] << endl
					<< "effect=" << (SValue().Join(binding)) << endl
					<< dedent;
#endif
			targetOut.Join(binding);
		}
	} else {
		// not found - add key/value depending on translate flag
		if (flags & B_NO_TRANSLATE_LINK) {
#if BINDER_DEBUG_PUSH_MSGS 
			bout << "Pushing Notification: " << key << endl;
#endif
			group->AddItem(target, key);
		} else {
#if BINDER_DEBUG_PUSH_MSGS
			bout	<< "Pushing Link:" << endl
					<< indent << "link=" << key << endl
					<< "val=" << binding[key] << endl
					<< "effect=" << (SValue().Join(binding)) << endl
					<< dedent;
#endif
			group->AddItem(target, binding);
		}
	}
	
	return B_OK;
}

status_t
PushMaps::DoPush(const sptr<AsyncHandler>& handler) 
{
	status_t err = B_OK;
	// Push both the sync group and the async group
	if (m_syncGroup.CountItems() > 0) {
		do_push(m_syncGroup);
	}

	if (m_asyncGroup != NULL) {
#if !LIBBE_BOOTSTRAP			
		SMessage msg('asyn', SLooper::ThreadPriority());
		// This is the stuff to push.
		msg.JoinItem(B_0_INT32, SSimpleValue<int32_t>((int32_t)m_asyncGroup));
		// Make sure the handler doesn't go away until this message is processed.
		msg.JoinItem(B_1_INT32, SValue::Atom(handler.ptr()));
		if (handler == NULL || (err=handler->PostMessage(msg)) != B_OK) {
			// Error!
			if (handler == NULL) err = B_NO_MEMORY;
			delete m_asyncGroup;
		}
#else
		do_push(*m_asyncGroup);
		delete m_asyncGroup;
#endif
	}
	
	return err;
}

status_t
BBinder::Push(const SValue &out)
{
#if BINDER_DEBUG_PUSH_MSGS 
	bout << "<BBinder::Push>" << endl << indent;
#endif

	// If we're in Effect, add what we're pushing into our effect
	EffectCache *cache = (EffectCache*)tls_get(gBinderTLS);
	if (cache && cache->out) {
#if BINDER_DEBUG_MSGS
		berr << "BBinder::out " << out << endl;
		berr << "BBinder::outbindings " << cache->outBindings << " " << *cache->outBindings << endl;
#endif
		if (*cache->outBindings == B_WILD_VALUE) {
			*cache->out += out;
#if BINDER_DEBUG_MSGS
			berr << "BBinder::Push " << out << endl;
			berr << "BBinder::Push2 " << *cache->out << endl;
#endif
		} else {
			*cache->out += *cache->outBindings * out;
#if BINDER_DEBUG_MSGS
			berr << "BBinder::Push3 " << *cache->out << endl;
#endif
		}
	}
	
	status_t err = B_OK;
	// Notify our links about this Push()
	// Right now we perform the push sychronously if the link
	// is synchronous.  We also really need to have a synchronous
	// flag pass to Push() here and require that as well in order
	// for this to be synchronous (both sides must promise that
	// they are doing the right thing for it).  Or maybe it can
	// be synchronous if -either- side says it is okay?  That is,
	// if you ask for it to be synchronous, the sender is saying
	// that they are not holding locks when calling Push(), and
	// the recipient is saying their implementation will not
	// acquire any locks that they hold when calling back to the
	// sender.
	extensions *e = m_extensions;
	if (e) {
		PushMaps pushList;
		e->lock.Lock();
		if (e->links.CountItems() > 0) {
			
			SValue senderKey, senderValue;
			void * cookie = NULL;
			bool found;
			while (B_OK == out.GetNextItem(&cookie, &senderKey, &senderValue)) {
				const SVector<links_rec> & rec = e->links.ValueFor(senderKey, &found);
				if (found) {
					size_t i, count = rec.CountItems();
					for (i=0; i<count; i++) {
						const links_rec & ent = rec[i];
						
						sptr<IBinder> target;
						if (ent.flags&B_WEAK_BINDER_LINK) {
							if (ent.target->AttemptIncStrong(this)) {
								target = ent.target;
								target->DecStrong(this);
							}
							// TO DO: Remove dead entries.
						} else {
							target = ent.target;
						}
						
						if (target != NULL) {
							// add current target to push list
							err = pushList.Add(target, ent.value, SValue(ent.value, senderValue), ent.flags, &e->handler);
						}
					}
				}
			}
		}
		if (e->other_links.CountItems() > 0) {
			SValue binding;
			size_t i, count = e->other_links.CountItems();
			
			for (i=0; i<count; i++) {
				const links_rec & ent = e->other_links[i];
				
				if (ent.value.IsWild() || (ent.flags&B_NO_TRANSLATE_LINK)) {
					binding = out;
				} else {
					// Do a "map keys" operation.  This should probably
					// be moved into SValue.
					binding.Undefine();
					SValue senderKey, targetKey;
					void * cookie = NULL;
					while (B_OK == ent.value.GetNextItem(&cookie, &senderKey, &targetKey)) {
						SValue value(out.ValueFor(senderKey,0));
						if (value.IsDefined()) binding.JoinItem(targetKey, value);
					}
					if (!binding.IsDefined()) continue;
				}
				
				sptr<IBinder> target;
				if (ent.flags&B_WEAK_BINDER_LINK) {
					if (ent.target->AttemptIncStrong(this)) {
						target = ent.target;
						target->DecStrong(this);
					}
					// TO DO: Remove dead entries.
				} else {
					target = ent.target;
				}
				
				if (target != NULL) {
					err = pushList.Add(target, ent.value, binding, ent.flags, &e->handler);
				}
			}
		}
		e->lock.Unlock();

		err = pushList.DoPush(e->handler);
	}

#if BINDER_DEBUG_PUSH_MSGS
	bout << dedent << "</BBinder::Push>" << endl;
#endif
		
	return err;
}

status_t 
BBinder::Pull(SValue *)
{
	#if _SUPPORTS_WARNING
	#warning Implement BBinder::Pull()
	#endif
	return B_UNSUPPORTED;
}

bool
BBinder::HoldRefForLink(const SValue& , uint32_t )
{
	return false;
}

/**************************************************************************************/

enum {
	// This is used to transfer ownership of the remote binder from
	// the BpAtom object holding it (when it is constructed), to the
	// owner of the BpAtom object when it first acquires that BpAtom.
	kRemoteAcquired = 0x00000001
};

BpAtom::BpAtom(const sptr<IBinder>& o)
	:	m_remote(o.ptr()), m_state(0)
{
	if (m_remote) {
#if BPATOM_DEBUG_MSGS
		printf("*** BpAtom(): IncStrong() and IncWeak() remote %p\n", m_remote);
#endif
		m_remote->IncStrong(this);	// Removed on first IncStrong().
		m_remote->IncWeak(this);	// Held for life of BpAtom.
	}
}

BpAtom::~BpAtom()
{
	if (m_remote) {
		if (!(m_state&kRemoteAcquired)) {
#if BPATOM_DEBUG_MSGS
			printf("*** ~BpAtom(): DecStrong() remote %p\n", m_remote);
#endif
			m_remote->DecStrong(this);
		}
#if BPATOM_DEBUG_MSGS
		printf("*** ~BpAtom(): DecWeak() remote %p\n", m_remote);
#endif
		m_remote->DecWeak(this);
	}
}

void BpAtom::InitAtom()
{
#if BPATOM_DEBUG_MSGS
	printf("*** BpAtom::InitAtom(): Transfering acquire ownership from %p\n", m_remote);
#endif
	atomic_or(&m_state, kRemoteAcquired);
}

status_t BpAtom::FinishAtom(const void* /*id*/)
{
	if (m_remote) {
#if BPATOM_DEBUG_MSGS
		printf("*** BpAtom::FinishAtom(): Calling DecStrong() on %p\n", m_remote);
#endif
		m_remote->DecStrong(this);
	}
	return B_ERROR;
}

status_t BpAtom::IncStrongAttempted(uint32_t /*flags*/, const void* /*id*/)
{
#if BPATOM_DEBUG_MSGS
	printf("*** BpAtom::IncStrongAttempted() of %p\n", m_remote);
#endif
	return m_remote ? (m_remote->AttemptIncStrong(this) ? B_OK : B_NOT_ALLOWED)
					: B_NOT_ALLOWED;
}

status_t BpAtom::DeleteAtom(const void* /*id*/)
{
	return B_OK;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
