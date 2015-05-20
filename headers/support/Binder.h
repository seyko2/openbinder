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

#ifndef _SUPPORT_BINDER_H_
#define _SUPPORT_BINDER_H_

/*!	@file support/Binder.h
	@ingroup CoreSupportBinder
	@brief Basic functionality of all Binder objects.
*/

#include <support/Value.h>
#include <support/StaticValue.h>
#include <support/SupportDefs.h>
#include <support/Value.h>
#include <support/Context.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportBinder
	@{
*/

// ------------------------------------------------------------------------
/*!	@name BBinder::HandleEffect() Helpers
	These are helper functions for implementing BBinder::HandleEffect(). */

//@{

struct BAutobinderDef;

struct effect_action_def {
	typedef status_t	(*put_func)		(const sptr<IInterface>& target,
										 const SValue& value);
	typedef SValue		(*get_func)		(const sptr<IInterface>& target);
	typedef SValue		(*invoke_func)	(const sptr<IInterface>& target,
										 const SValue& value);
	
	size_t				struct_size;	//!< always sizeof(effect_action_def)

	const void*			raw_key;		//!< the binder key this action is for
	const SValue&		key() const;	//!< convenience for retrieving key
	BAutobinderDef*		parameters;		//!< parameters for function
	
	put_func			put;			//!< function for put action, or NULL
	get_func			get;			//!< function for get action, or NULL
	invoke_func			invoke;			//!< function for invoke action, or NULL
};

//! Flags for execute_effect()
enum {
	B_ACTIONS_SORTED_BY_KEY		= 0x00000001
};

enum {
	B_NO_ACTION					= 0x00000000,
	B_INVOKE_ACTION				= 0x00000001,
	B_GET_ACTION,
	B_PUT_ACTION
};

status_t _IMPEXP_SUPPORT
execute_effect(	const sptr<IInterface>& target,
				const SValue &in, const SValue &inBindings,
				const SValue &outBindings, SValue *out,
				const effect_action_def* actions, size_t num_actions,
				uint32_t flags = 0);
//@}

// ------------------------------------------------------------------------

//!	Standard implementation of IBinder for a Binder object.
class BBinder : public IBinder
{
	public:
		inline	const SContext&			Context() const;
		
		virtual	SValue					Inspect(const sptr<IBinder>& caller,
												const SValue &which,
												uint32_t flags = 0);
		virtual	sptr<IInterface>		InterfaceFor(	const SValue &descriptor,
														uint32_t flags = 0);
		virtual	status_t				Link(	const sptr<IBinder>& target,
												const SValue &bindings,
												uint32_t flags = 0);
		virtual	status_t				Unlink(	const wptr<IBinder>& target,
												const SValue &bindings,
												uint32_t flags = 0);
		virtual	status_t				Effect(	const SValue &in,
												const SValue &inBindings,
												const SValue &outBindings,
												SValue *out);
		
		virtual	status_t				Transact(	uint32_t code,
  													SParcel& data,
  													SParcel* reply = NULL,
  													uint32_t flags = 0);

		virtual status_t 				AutobinderPut(	const BAutobinderDef* def,
														const void* value);
		virtual status_t 				AutobinderGet(	const BAutobinderDef* def,
														void* result);
		virtual status_t 				AutobinderInvoke(	const BAutobinderDef* def,
															void** params,
															void* result);
		
		virtual	status_t				LinkToDeath(	const sptr<BBinder>& target,
														const SValue &method,
														uint32_t flags = 0);
		virtual	status_t				UnlinkToDeath(	const wptr<BBinder>& target,
														const SValue &method,
														uint32_t flags = 0);
		virtual	bool					IsBinderAlive() const;
		virtual	status_t				PingBinder();
		
		virtual	BBinder*				LocalBinder();
#if _SUPPORTS_NAMESPACE
		virtual	palmos::osp::BpBinder*	RemoteBinder();
#else
		virtual	BpBinder*				RemoteBinder();
#endif

		// Local data flow implementation.  Call Push() to transfer data to
		// anything linked to you.
				bool					IsLinked() const;
				status_t				Push(const SValue &out);

	protected:
										BBinder();
										BBinder(const SContext& context);
										BBinder(const BBinder& other);
		virtual							~BBinder();

		virtual	status_t				HandleEffect(	const SValue &in,
														const SValue &inBindings,
														const SValue &outBindings,
														SValue *out);
		
		// These functions are called by the default implementation of
		// HandleEffect(), interpreting an Effect() calls as Put(), Get(),
		// and Invoke(), respectively.  These are for use by scripting
		// languages.  Static languages should use execute_effect() and/or
		// the autobinder.
		virtual	status_t				Told(const SValue& what, const SValue& in);
		virtual	status_t				Asked(const SValue& what, SValue* out);
		virtual	status_t				Called(	const SValue& func,
												const SValue& args,
												SValue* out);

		virtual	status_t				Pull(SValue *inMaskAndDefaultsOutData);

		//!	Control when links from this object will also hold references on the object.
		/*!	The default implementation always returns false, meaning references are
			never held.  You can override this to return true when you would like a
			new linking being adding to this BBinder to also cause a reference to
			be added to it.  This is useful, for example, when dynamically creating
			objects on demand -- in that case, you usually want the object to stay
			around as long as others have references on them @e or there are active
			links from it.  If you allow the object to be destroyed while links
			exist, the next time someone accesses the object a new one will be
			created and any changes to it will not push the links on the old
			object. */
		virtual	bool					HoldRefForLink(const SValue& binding, uint32_t flags);

	private:
				void					BeginEffectContext(const struct EffectCache &cache);
				void					EndEffectContext();

				
				BBinder&				operator=(const BBinder& o);	// No implementation

		static	int32_t					gBinderTLS;
		
		struct extensions;
		
		const SContext					m_context;
		extensions *					m_extensions;
};

/*-------------------------------------------------------------*/
/*------- BnInterface Implementation ---------------------------*/

//! This is the base implementation for a native (local) IInterface.
/*!	You will not normally use this, instead letting pidgen generate
	similar code from an IDL file. */
template<class INTERFACE>
class BnInterface : public INTERFACE, public BBinder
{
public:
		virtual	SValue					Inspect(const sptr<IBinder>&, const SValue &which, uint32_t flags = 0)
    { (void)flags; return which * SValue(INTERFACE::Descriptor(),SValue::Binder(this)); };
		virtual	sptr<IInterface>	InterfaceFor(const SValue &desc, uint32_t flags = 0)
                { return (desc == INTERFACE::Descriptor()) ? sptr<IInterface>(this) : BBinder::InterfaceFor(desc, flags); }
	
protected:

#if defined(_MSC_VER) && _MSC_VER
		inline							BnInterface() { };
		inline							BnInterface(const SContext& context) : BBinder(context) { };
		inline							BnInterface(const BnInterface<INTERFACE>& other) : INTERFACE(), BBinder(other) { };
		inline virtual					~BnInterface() { };
#else
										BnInterface();
										BnInterface(const SContext& context);
										BnInterface(const BnInterface<INTERFACE>& other);
		virtual							~BnInterface();
#endif

		
		
		virtual	sptr<IBinder>			AsBinderImpl()			{ return this; }
		virtual	sptr<const IBinder>		AsBinderImpl() const	{ return this; }
};

/*-------------------------------------------------------------*/
/*------- BpAtom Implementation --------------------------------*/

//! This is the SAtom protocol implementation for a proxy interface
/*!	You don't normally use this directly -- it is included as part of the
	BpInterface<> implementation below.
*/
class BpAtom : public virtual SAtom
{
	protected:
										BpAtom(const sptr<IBinder>& o);
		virtual							~BpAtom();
		virtual	void					InitAtom();
		virtual	status_t				FinishAtom(const void* id);
		virtual	status_t				IncStrongAttempted(uint32_t flags, const void* id);
		virtual	status_t				DeleteAtom(const void* id);
		
		inline	IBinder*				Remote()				{ return m_remote; }
		// NOTE: This removes constness from the remote binder.
		inline	IBinder*				Remote() const			{ return m_remote; }
				
	private:
										BpAtom(const BpAtom& o);	// no implementation
		BpAtom&							operator=(const BpAtom& o);	// no implementation

		IBinder* const					m_remote;
		int32_t							m_state;
		size_t							_reserved;
};

/*-------------------------------------------------------------*/
/*------- BpInterface Implementation ---------------------------*/

//! This is the base implementation for a remote IInterface.
/*!	You will not normally use this, instead letting pidgen generate
	similar code from an IDL file. */
template<class INTERFACE>
class BpInterface : public INTERFACE, public BpAtom
{
	protected:
		inline							BpInterface(const sptr<IBinder>& o)	: BpAtom(o) { }
		inline virtual					~BpInterface() { }
		
		virtual	sptr<IBinder>			AsBinderImpl()
											{ return sptr<IBinder>(Remote()); }
		virtual	sptr<const IBinder>		AsBinderImpl() const
											{ return sptr<const IBinder>(Remote()); }

		virtual	void					InitAtom()
											{ BpAtom::InitAtom(); }
		virtual	status_t				FinishAtom(const void* id)
											{ return BpAtom::FinishAtom(id); }
		virtual	status_t				IncStrongAttempted(uint32_t flags, const void* id)
											{ return BpAtom::IncStrongAttempted(flags, id); }
		virtual	status_t				DeleteAtom(const void* id)
											{ return BpAtom::DeleteAtom(id); }
};

/*-------------------------------------------------------------*/
/*------- B_IMPLEMENT_META_INTERFACE --------------------------*/

//!	Use the B_IMPLEMENT_META_INTERFACE macro inside of your IInterface subclass's .cpp file to implement the Binder meta-API.
/*!	You will not normally use this, instead letting pidgen generate
	similar code from an IDL file.

	Note that you must have defined your remote proxy class 
	(with an "Bp" prefix) before using the macro.
	B_IMPLEMENT_META_INTERFACE_NO_INSTANTIATE implements the
	internals of B_IMPLEMENT_META_INTERFACE, which is what
	should be used 99% of the time.  B_IMPLEMENT_META_INTERFACE_LOCAL
	can be used for those cases what want binder functionality
	without the cross-process or multi-language support.
	(See documentation for the [local] attribute in PIDgen.)
	The leftmostbase casts are needed when the interface has multiple bases.
	Rather than have 2 macro versions, we just always insert the extra cast.  
	In the case of single (or no) inheritance, the leftmostbase will be the same
	as I ## iname, which results in code like this:
	static_cast<IFoo*>(static_cast<IFoo*> blah))
*/
#define B_IMPLEMENT_META_INTERFACE_NO_INSTANTIATE(iname, descriptor, leftmostbase)			\
		B_STATIC_STRING_VALUE_LARGE(descriptor_ ## iname, descriptor,)						\
		const BNS(::palmos::support::) SValue& I ## iname :: Descriptor()					\
		{																					\
			return descriptor_ ## iname;													\
		}																					\
		BNS(::palmos::support::) sptr<I ## iname> I ## iname :: AsInterface(				\
			const BNS(::palmos::support::) sptr< BNS(::palmos::support::) IBinder> &o,		\
			status_t* out_error)															\
		{																					\
			return static_cast<I ## iname*>(												\
				static_cast<leftmostbase*>(													\
				IInterface::ExecAsInterface(												\
						o,																	\
						descriptor_ ## iname,												\
						instantiate_proxy_ ## iname,										\
						out_error)															\
					.ptr() ));																\
		}																					\
		BNS(::palmos::support::) sptr<I ## iname> I ## iname :: AsInterface(				\
			const BNS(::palmos::support::) SValue &v,										\
			status_t* out_error)															\
		{																					\
			sptr<IBinder> obj = v.AsBinder(out_error);										\
			if (out_error && *out_error != B_OK) return NULL;								\
			return static_cast<I ## iname*>(												\
				static_cast<leftmostbase*>(													\
					IInterface::ExecAsInterface(											\
						obj,																\
						descriptor_ ## iname,												\
						instantiate_proxy_ ## iname,										\
						out_error)															\
					.ptr() ));																\
		}																					\
		BNS(::palmos::support::) sptr<I ## iname> I ## iname :: AsInterfaceNoInspect(		\
			const BNS(::palmos::support::) sptr< BNS(::palmos::support::) IBinder> &o,		\
			status_t* out_error)															\
		{																					\
			return static_cast<I ## iname*>(												\
				static_cast<leftmostbase*>(													\
					IInterface::ExecAsInterfaceNoInspect(									\
						o,																	\
						descriptor_ ## iname,												\
						instantiate_proxy_ ## iname,										\
						out_error)															\
					.ptr() ));																\
		}																					\
		BNS(::palmos::support::) sptr<I ## iname> I ## iname :: AsInterfaceNoInspect(		\
			const BNS(::palmos::support::) SValue &v,										\
			status_t* out_error)															\
		{																					\
			sptr<IBinder> obj = v.AsBinder(out_error);										\
			if (out_error && *out_error != B_OK) return NULL;								\
			return static_cast<I ## iname*>(												\
				static_cast<leftmostbase*>(													\
				IInterface::ExecAsInterfaceNoInspect(										\
					obj,																	\
					descriptor_ ## iname,													\
					instantiate_proxy_ ## iname,											\
					out_error)																\
				.ptr() ));																	\
		}																					\

//!	B_IMPLEMENT_META_INTERFACE should be used in almost all cases.
#define B_IMPLEMENT_META_INTERFACE(iname, descriptor, leftmostbase)							\
		static BNS(::palmos::support::) sptr<BNS(::palmos::support::) IInterface>			\
		instantiate_proxy_ ## iname (const BNS(::palmos::support::)sptr<BNS(::palmos::support::) IBinder>& b) {		\
			return static_cast<leftmostbase*>(new Bp ## iname(b));							\
		}																					\
		B_IMPLEMENT_META_INTERFACE_NO_INSTANTIATE(iname, descriptor, leftmostbase)			\

//!	B_IMPLEMENT_META_INTERFACE_LOCAL is used for [local] interfaces.
/*! (Local to one process and not callable by other languages.) */
#define B_IMPLEMENT_META_INTERFACE_LOCAL(iname, descriptor, leftmostbase)					\
		static BNS(::palmos::support::) sptr<BNS(::palmos::support::) IInterface>			\
		instantiate_proxy_ ## iname (const BNS(::palmos::support::)sptr<BNS(::palmos::support::) IBinder>& b) {		\
			return NULL;																	\
		}																					\
		B_IMPLEMENT_META_INTERFACE_NO_INSTANTIATE(iname, descriptor, leftmostbase)			\

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

// The first field of a BBinder is its containing context.
inline const SContext& BBinder::Context() const
{
	return m_context;
}

#if !defined(_MSC_VER) || !_MSC_VER
template<class INTERFACE>
inline BnInterface<INTERFACE>::BnInterface()
{
}

template<class INTERFACE>
inline BnInterface<INTERFACE>::BnInterface(const SContext& context) : BBinder(context)
{
}

template<class INTERFACE>
inline BnInterface<INTERFACE>::BnInterface(const BnInterface<INTERFACE>& other) : INTERFACE(), BBinder(other)
{
}

template<class INTERFACE>
inline BnInterface<INTERFACE>::~BnInterface()
{
}
#endif

inline const SValue& effect_action_def::key() const
{
	return *reinterpret_cast<const SValue*>(raw_key);
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif	/* _SUPPORT_BINDER_H_ */
