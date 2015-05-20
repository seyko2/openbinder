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

#ifndef _SUPPORT_BINDER_INTERFACE_H_
#define _SUPPORT_BINDER_INTERFACE_H_

/*!	@file support/IBinder.h
	@ingroup CoreSupportBinder
	@brief Abstract interface to a Binder object.
*/

#include <support/Atom.h>

#if _SUPPORTS_NAMESPACE
namespace palmos { 
namespace osp { 
#endif
	class BpBinder; 
#if _SUPPORTS_NAMESPACE
} } // namespace palmos::osp
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportBinder Binder
	@ingroup CoreSupport
	@brief Component-based framework for implementing application and system-level code.

	The @ref BinderKit defines a basic object model (built on C++) that
	allows instantiated objects to be distributed across any processes
	desired, taking care of the details of IPC and tracking object
	references between them.
	@{
*/

struct BAutobinderDef;
class IInterface;
class BBinder;
class SParcel;
class SValue;

/*-------------------------------------------------------------*/
/*------- IBinder Interface -----------------------------------*/

//!	Standard IBinder transaction codes.
enum {
	B_EFFECT_TRANSACTION	= 'efct',		//!< Generic IBinder::Effect()
	B_INSPECT_TRANSACTION	= 'insp',		//!< IBinder::Inspect()
	B_LINK_TRANSACTION		= 'link',		//!< IBinder::Link()
	B_UNLINK_TRANSACTION	= 'unlk',		//!< IBinder::Unlink()
	B_PING_TRANSACTION		= 'ping',		//!< IBinder::PingBinder()

	B_PUT_TRANSACTION		= '_put',		//!< IBinder::AutobinderPut()
	B_GET_TRANSACTION		= '_get',		//!< IBinder::AutobinderGet()
	B_INVOKE_TRANSACTION	= 'invk'		//!< IBinder::AutobinderInvoke()
};

//!	Options for Binder links.
/*!	Use with IBinder::Link(), IBinder::Unlink(), IBinder::LinkToDeath(),
	and IBinder::UnlinkToDeath(). */
enum {
	//!	The created link holds a weak pointer on the supplied target.
	/*!	This allows your linked-to object to be destroyed while the link
		still exists.  Be sure to still clean up the link, for example
		calling Unlink() (with B_WEAK_BINDER_LINK!) in your destructor.
		
		@todo Should this be the default for links?  It is safer
		than holding a strong reference, and usually what you want. */
	B_WEAK_BINDER_LINK		= 0x00000001,

	//!	Your object will process the link without unsafely acquiring locks.
	/*!	Links are normally pushed asynchronously, to avoid deadlocks if
		a lock is held at the time of the push.  By setting this flag, you
		say that the sender doesn't need to push asychronously to your
		target, avoiding that overhead.  When this flag is set, you must
		not acquire any locks during processing that could cause a deadlock
		with the sender.  For example, just calling PostMessage() on an
		SHandler would always be safe to do synchronously. */
	B_SYNC_BINDER_LINK		= 0x00000002,

	//!	The link mapping will be pushed directly, without translation.
	/*!	The property/arguments being pushed by the sender are ignored. */
	B_NO_TRANSLATE_LINK		= 0x00000004,

	//!	Unlink all links to the given target.
	/*!	Use with IBinder::Unlink() and IBinder::UnlinkToDeath().  The
		given bindings and other flags will be ignored, simply removing
		every link that has the given target. */
	B_UNLINK_ALL_TARGETS	= 0x00000008
};

//!	Abstract generic (scripting) interface to a Binder object.
class IBinder : virtual public SAtom
{
public:

			//!	Probe binder for interface information.
			/*!	Return interfaces implemented by this binder object
				that are requested by \a which.  This is a composition
				of all interfaces, expressed as { descriptor -> binder }
				mappings, which are selected through \a which.

				Much more information on Inspect() can be found at
				@ref BinderInspect.
			*/
			virtual	SValue				Inspect(const sptr<IBinder>& caller,
												const SValue &which,
												uint32_t flags = 0) = 0;
			
			//!	Retrieve direct interface for this binder.
			/*!	Given a SValue interface descriptor, return an IInterface
				implementing it.  The default implementation of this
				function returns NULL, meaning it does not implement a
				direct interface to it.  If the return is non-NULL, you
				are guaranteed to be able to static_cast<> the returned
				interface into the requested subclass and have it work.
				
				Note that this is NOT the same as calling Inspect(),
				which performs conversion between different IBinder objects.
				This method converts to an IInterface only for @e this
				binder object.
			*/
	virtual	sptr<IInterface>		InterfaceFor(	const SValue &descriptor,
														uint32_t flags = 0) = 0;
			
			//!	Link registers the IBinder "target" for notification of events.
			/*!	The \a bindings is a mapping of keys that will get pushed on this
				IBinder, to keys that will get pushed on the "target" IBinder.
				e.g.
					- If you call link thus:
						binder1->Link(binder2, SValue("A", "B"));
					- When "A" is pushed on binder1:
						binder1->Push(SValue("A", "C"));
					- The following will get called on binder2:
						binder2->Effect(SValue("B", "C"), SValue::wild, SValue::undefined, NULL);
			*/
	virtual	status_t					Link(	const sptr<IBinder>& target,
												const SValue &bindings,
												uint32_t flags = 0) = 0;
			//! Remove a mapping previously added by Link().
	virtual	status_t					Unlink(	const wptr<IBinder>& target,
												const SValue &bindings,
												uint32_t flags = 0) = 0;
	
			//!	Perform an action on the binder.
			/*!	Either a get, put, or invocation, depending on the supplied
				and requested bindings. */
	virtual	status_t					Effect(	const SValue &in,
												const SValue &inBindings,
												const SValue &outBindings,
												SValue *out) = 0;

		virtual status_t 				AutobinderPut(	const BAutobinderDef* def,
														const void* value) = 0;
		virtual status_t 				AutobinderGet(	const BAutobinderDef* def,
														void* result) = 0;
		virtual status_t 				AutobinderInvoke(	const BAutobinderDef* def,
															void** params,
															void* result) = 0;

			//!	Synonym for Effect(in, SValue::wild, SValue::undefined, NULL).
			status_t					Put(const SValue &in);
			//!	Synonym for Effect(SValue::undefined, SValue::undefined, bindings, [result]).
			SValue						Get(const SValue &bindings) const;
			//!	Synonym for Effect(SValue::undefined, SValue::undefined, SValue::wild, [result]).
			SValue						Get() const;
			//!	Synonym for Effect(SValue(func,args), SValue::wild, func, [result]).
			SValue						Invoke(const SValue &func, const SValue &args);
			//!	Synonym for Effect(SValue(func,SValue::wild), SValue::wild, func, [result]).
			SValue						Invoke(const SValue &func);
	
			//!	Low-level data transfer.
			/*!	This is the Binder's IPC primitive.  It allows you to send a parcel of
				data to another Binder (possibly in another process or language) and
				get a parcel of data back.  The parcel can contain IBinder objects to
				transfer references between environments.  The @a code can be any arbitrary
				value, though some standard codes are defined for parts of the
				higher-level IBinder protocol (B_EFFECT_TRANSACTION, B_INSPECT_TRANSACTION,
				etc).  The flags are used for internal IPC implementation and must always
				be set to 0 when calling.  If your implementation of Transact() returns
				an error code (instead of B_OK), that code will be propagated back
				to the caller WITHOUT any @a reply data. */
	virtual	status_t					Transact(	uint32_t code,
													SParcel& data,
													SParcel* reply = NULL,
													uint32_t flags = 0) = 0;
	
			//! Register the IBinder "target" for a notification if this binder goes away.
			/*! The \a method is the name of a method to call if this binder unexpectedly
				goes away.  This is accomplished by performing an Effect() on \a target
				with the given \a method.  Prior to doing so, a mapping <code>{ 0 -> this }</code> is added
				to the method name, so that the first parameter of the function is the
				dying binder.  I.e., your method should look like:

				@code
SomethingDied(const wptr<IBinder>& who)
				@endcode

				@note You will only receive death notifications for remote binders,
				as local binders by definition can't die without you dying as well.

				@note This link always holds a weak reference to its target.

				@note You will only receive a weak reference to the dead
				binder.  You should not try to promote this to a strong reference.  (Nor
				should you need to, as there is nothing useful you can directly do with
				it now that it has passed on.)
			*/
	virtual	status_t					LinkToDeath(	const sptr<BBinder>& target,
														const SValue &method,
														uint32_t flags = 0) = 0;

			//!	Remove a previously registered death notification.
			/*! The \a target and \a method must exactly match the values passed
				in to LinkToDeath().
			*/
	virtual	status_t					UnlinkToDeath(	const wptr<BBinder>& target,
														const SValue &method,
														uint32_t flags = 0) = 0;

			//!	Return true if this binder still existed as of the last executed operation.
	virtual	bool						IsBinderAlive() const = 0;
	
			//!	Send a ping to the remote binder, and return status.
			/*!	If this is a local binder, ping always returns B_OK.
				If this is a remote binder, it performs a Transact()
				to the local binder and returns that status.  This
				should be either B_OK or B_BINDER_DEAD. */
	virtual	status_t					PingBinder() = 0;
	
			//!	Use this function instead of a dynamic_cast<> to up-cast to a BBinder.
			/*!	Since multiple BBinder instances can appear in a
				single object, a regular dynamic_cast<> is ambiguous.
				Note that there is a default implementation of this
				(which returns NULL) so that we can call the method
				even if the object has been destroyed.  (That is, if
				we only have a weak reference on it.)
			*/
	virtual	BBinder*					LocalBinder();
			//!	Internal function to retrieve remote proxy object.
			/*!	This is for internal use by the system.  Use LocalBinder()
				to determine if this IBinder is a local object. */
	virtual	BNS(palmos::osp::) BpBinder*	RemoteBinder();

protected:
	inline								IBinder() { }
	inline virtual						~IBinder() { }

private:
										IBinder(const IBinder& o);
};

/*-------------------------------------------------------------*/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif	// _SUPPORT_BINDER_INTERFACE_H_
