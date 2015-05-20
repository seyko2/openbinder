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

#ifndef _SUPPORT_RBINDER_H_
#define _SUPPORT_RBINDER_H_

#include <support/Autobinder.h>
#include <support/SupportDefs.h>
#include <support/Binder.h>
#include <support/Locker.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {
#endif

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

/**************************************************************************************/

// Private link flags.
enum {
	// Flag bits that will not be used when matching against unlink flags,
	// Note: deliberately does NOT include kIsALink.
	kPrivateLinkFlagMask	= 0x70000000,

	// This is used in LinkToDeath() for handling notifications to normal links.
	kIsALink				= 0x80000000,
	// Link is holding a ref on the BBinder.
	kLinkHasRef				= 0x40000000	
};

class BpBinder : public IBinder
{
	public:

										BpBinder(int32_t handle);
		virtual							~BpBinder();

				int32_t					Handle() { return m_handle; };

		virtual	SValue					Inspect(const sptr<IBinder>& caller,
												const SValue &which,
												uint32_t flags = 0);
		virtual	sptr<IInterface>	InterfaceFor(	const SValue &descriptor,
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

		virtual status_t 				AutobinderPut(	const BAutobinderDef* def,
														const void* value);
		virtual status_t 				AutobinderGet(	const BAutobinderDef* def,
														void* result);
		virtual status_t 				AutobinderInvoke(	const BAutobinderDef* def,
															void** params,
															void* result);


		virtual	status_t				Transact(	uint32_t code,
													SParcel& data,
													SParcel* reply = NULL,
													uint32_t flags = 0);
		
		virtual	status_t				LinkToDeath(	const sptr<BBinder>& target,
														const SValue &method,
														uint32_t flags = 0);
		virtual	status_t				UnlinkToDeath(	const wptr<BBinder>& target,
														const SValue &method,
														uint32_t flags = 0);

				void					SendObituary();

		virtual	bool					IsBinderAlive() const;
		virtual	status_t				PingBinder();
		
		virtual	BBinder*				LocalBinder();
		virtual	BpBinder*				RemoteBinder();
	
	protected:

		virtual	void					InitAtom();
		virtual	status_t				FinishAtom(const void* id);
		virtual	status_t				IncStrongAttempted(uint32_t flags, const void* id);

	private:

				struct Obituary {
					wptr<BBinder> target;
					SValue method;
					uint32_t flags;
				};

				void					report_one_death(const Obituary& obit);

		const	int32_t					m_handle;

				SLocker					m_lock;
				SVector<Obituary>*		m_obituaries;
				bool					m_alive;
	// XXX : FIXME : HACK ALERT!
	friend class BProcess;
};

/**************************************************************************************/

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::osp
#endif

#endif	/* _SUPPORT_BINDERIPC_H_ */
