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

#ifndef _SERVICES_INFORMANT_H
#define _SERVICES_INFORMANT_H

#include <services/IInformant.h>
#include <support/Locker.h>
#include <support/Handler.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::services;
#endif

struct _InformantData;


/*!	A class that lets you notify others when something happens.  This is to
	replace sublaunching.  The basic idea here is that a service can subclass
	BInformant, and call Inform() when something needs broadcasting.  Clients
	use the IInformant interface to register for events.  They can notified
	by having a method called on an already existing object (RegisterForCallback())
	or by having a component object created, and then having a particular method
	called on that newly created object (RegisterForCreation()).
*/
class BInformant : public BnInformant, public BHandler
{
public:

						BInformant();
						BInformant(const SContext& context);
	
	/*!	Register to be called when key is pushed.

		@param key The key for which to register.  Each subclass of BInformant will most likely push different keys.
		@param target The object on which to call the supplied method.
		@param method The method that will be called on your object when the key is pushed.  The method is expressed as an SValue key, which will be a method if the target object is an interface, or it will be passed as the key parameter to Observed if target is a BObserver.
		@param flags The flags to apply.  Currently unused, defaults to 0.
		@param cookie An SValue you will receive back when your method is called.
	 */
	virtual	status_t	RegisterForCallback(	const SValue &key,
												const sptr<IBinder>& target,
												const SValue &method,
												uint32_t flags = 0,
												const SValue &cookie = B_UNDEFINED_VALUE);
	
	/*!	Register to have your component created when key is pushed.
	  	
		See the descriptions in RegisterForCallback for the key, method, flags and cookie parameters.
	  	
		@param key The key for which to register.  Each subclass of BInformant will most likely push different keys.
		@param context The context to use when your component is created.  Get this from the Root() method on your context, or use get_default_context() if you're in protein land and you don't need elevated security privileges.
		@param process The process in which to create your component.  If you pass NULL, a new process will be created just for you.  Careful, this might be slow if your key is pushed a lot.  WARNING: This passing NULL to get a new process is not yet implemented yet.  If you do this, the component will currently end up in the same process as the service.
		@param component The component name to instantiate, for example com.palmsource.apps.Address.Listener
		@param interface The interface to inspect to on your object.
		@param method The method that will be called on your object when the key is pushed.  The method is expressed as an SValue key, which will be a method if the target object is an interface, or it will be passed as the key parameter to Observed if target is a BObserver.
		@param flags The flags to apply.  Currently unused, defaults to 0.
		@param cookie An SValue you will receive back when your method is called.
	 */
	virtual	status_t	RegisterForCreation(	const SValue &key,
												const sptr<INode>& context,
												const sptr<IProcess>& process,
												const SString &component,
												const SValue &interface,
												const SValue &method,
												uint32_t flags,
												const SValue &cookie);
	
	virtual status_t	UnregisterForCallback(	const SValue &key,
												const sptr<IBinder>& target,
												const SValue &method,
												uint32_t flags);
												
	virtual status_t	UnregisterForCreation(	const SValue &key,
												const sptr<INode>& context,
												const sptr<IProcess>& process,
												const SString &component,
												const SValue &inspect,
												const SValue &method,
												uint32_t flags);
	
protected:
	/*! Call this to push out the key and the information */
	virtual status_t	Inform(const SValue &key, const SValue &information);


	// no user servicable parts below
	virtual				~BInformant();
	
	virtual void		InitAtom();
	virtual status_t	FinishAtom(const void *id);
			
public: // so as to not have friends, don't use this
	virtual	status_t	HandleMessage(const SMessage &msg);

private:
	explicit BInformant(const BInformant &); // unimplemented
	
	_InformantData * m_data;
};


#endif // _SERVICES_INFORMANT_H

