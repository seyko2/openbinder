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

#ifndef SUPPORT_CONTEXT_H_
#define SUPPORT_CONTEXT_H_

/*!	@file support/Context.h
	@ingroup CoreSupportBinder
	@brief Global context that a Binder object runs in.
*/

#include <support/String.h>
#include <support/Value.h>
#include <support/Vector.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportBinder
	@{
*/

class INode;
class IProcess;

//!	Representation of a global context (namespace).
class SContext
{
public:
// SContext must exist, but only by name in bootstrap code
#if !LIBBE_BOOTSTRAP
	SContext();
	SContext(const sptr<INode>& node);
	SContext(const SContext& context);
	~SContext();
	
	SContext& operator=(const SContext& o);
	
	bool operator==(const SContext& o) const;
	bool operator!=(const SContext& o) const;
	bool operator<(const SContext& o) const;
	bool operator<=(const SContext& o) const;
	bool operator>=(const SContext& o) const;
	bool operator>(const SContext& o) const;

	status_t InitCheck() const;

	//!	Flags given to New(), RemoteNew(), and NewProcess() to control process assignment.
	enum new_flags_enum {
		//!	All bits specifying the process preference.
		PROCESS_MASK	= 0x000f,
		
		//!	We would like to use a local process, if that is possible.
		/*!	This will always use the local process, unless that conflicts with the
			preferences of the component or is prevented by security restrictions. */
		PREFER_LOCAL	= 0x0000,
		//!	We would like to use a remote process, if that is possible.
		/*!	This will always use a remote process, unless prevented by the capabilities of
			the system.  Primarily the only way this will not be honored is if
			BINDER_SINGLE_PROCESS is set or the binder kernel module is not available.
			Note that this may result in a child process being created from your own process
			in which to host the component. */
		PREFER_REMOTE	= 0x0001,
		//!	We must always use a local process, without exception.
		/*!	This option should be used rarely.  If the local process can not be used
			(probably due to security restrictions), the instantiation will fail. */
		REQUIRE_LOCAL	= 0x0002,
		//!	We must always use a remote process, without exception.
		/*!	This option should be used rarely.  If a remote process can not be used
			(probably because the binder kernel module is not available), the
			instantiation will fail. */
		REQUIRE_REMOTE	= 0x0003
	};
	
	//!	Instantiate a new component, possibly hosted by the local process.
	/*!	@param[in]	component	Name of the component to instantiate.  This
								may be a complex SValue combining the name
								and arguments as the mapping { component -> args }.
		@param[in]	args		Additional component arguments.
		@param[in]	flags		Behavior modifiers, selected from @ref new_flags_enum.
		@param[out]	outError	Result code, B_OK if no error.
		
		@return Non-NULL if the component was instantiated.  If NULL, @a
		outError will contain the reason for the error. */
	sptr<IBinder> New(	const SValue& component,
						const SValue& args = B_UNDEFINED_VALUE,
						uint32_t flags = PREFER_LOCAL,
						status_t* outError = NULL) const;

	//!	Instantiate a new component, possibly hosted by a specified process.
	/*!	@param[in]	component	Name of the component to instantiate.  This
								may be a complex SValue combining the name
								and arguments as the mapping { component -> args }.
		@param[in]	process		The process object into which to instantiate the component.
		@param[in]	args		Additional component arguments.
		@param[in]	flags		Behavior modifiers, selected from @ref new_flags_enum.
		@param[out]	outError	Result code, B_OK if no error.
		
		@return Non-NULL if the component was instantiated.  If NULL, @a
		outError will contain the reason for the error.
		
		This is like New(), but allows you to specify a different process than your
		own into which the component should be instantiated.  Otherwise, the @a process
		is treated as if it is your own -- i.e., PREFER_LOCAL means that you would
		like the component to be instantiated in @a process. */
	sptr<IBinder> RemoteNew(	const SValue& component,
								const sptr<IProcess>& process,
								const SValue& args = B_UNDEFINED_VALUE,
								uint32_t flags = PREFER_LOCAL,
								status_t* outError = NULL) const;
	
	//!	Instantiate a new process.
	/*!	@param[in]	name		Name of the new process.
		@param[in]	flags		Behavior modifiers, selected from @ref new_flags_enum.
		@param[in]	env			Evironment variables for process (not currently implemented).
		@param[out]	outError	Result code, B_OK if no error.
		
		@return Non-NULL if a new process was created.  If NULL, @a outError
		will contain the reason for failure.
		
		This function creates a new, empty process, which you can then use
		with RemoteNew() to instantiate components inside of.  The name you
		give is currently just a convenience and not really used.  The
		PREFER_LOCAL and FORCE_LOCAL flags will result in always getting the
		local IProcess. */
	sptr<IProcess> NewProcess(	const SString& name,
								uint32_t flags = PREFER_REMOTE,
								const SValue& env = B_UNDEFINED_VALUE,
								status_t* outError = NULL) const;
	
	//!	 Low-level process instantiation function.  Use NewProcess() instead.					
	sptr<IBinder> NewCustomProcess(	const SString& executable,
									const SVector<SString>& args = SVector<SString>(),
									uint32_t flags = 0,
									const SValue& env = B_UNDEFINED_VALUE,
									status_t* outError = NULL) const;
	
	SValue Lookup(const SString& location) const;
	sptr<IBinder> LookupService(const SString& name) const;
	template<class INTERFACE> sptr<INTERFACE> LookupServiceAs(const SString& name) const;
	
	status_t Publish(const SString& location, const SValue& val) const;
	status_t PublishService(const SString& name, const sptr<IBinder>& object) const;
	status_t Unpublish(const SString& location) const;
	
	sptr<INode> Root() const;

	status_t LookupComponent(const SString& id, SValue* out_info) const;
	SValue LookupComponentProperty(const SString& component, const SString& property) const;

	status_t RunScript(const SString &filename) const;

	//!	Back-door for retrieving the global user context.
	/*!	@note You should @em only use this API if you are not
		running as a normal Binder components.  Binder components
		should always access their context through BBinder::Context().
		
		This returns the least-trusted "user" context.  This context
		is always guaranteed to exist, though the functionality offered
		through it may be quite limited.
	*/
	static SContext UserContext();
	
	//!	Back-door for retrieving the global system context.
	/*!	This returns the "root" or system context.  This is the initial
		context created by smooved, with full permissions.  Access to
		it may be quite restricted, so you can't in any way count on
		this API returning back a valid context. */
	static SContext SystemContext();

	//!	Back-door to retrieve a specific named context.
	/*!	The contexts that are available depends on how the system
		has been configured, though the "user" and "system" contexts
		should always exist (corresponding to the UserContext() and
		SystemContext() APIs here). */
	static SContext GetContext(const SString& name);
	
	//!	Low-level version of GetContext() taking an explicit process.
	/*!	The other context retrieval calls funnel down to this one,
		passing the local IProcess in to 'caller'. */
	static SContext GetContext(const SString& name, const sptr<IProcess>& caller);

private:
	mutable sptr<INode> m_root;

#endif
};

/*!	@} */

#if !LIBBE_BOOTSTRAP
template<class INTERFACE>
sptr<INTERFACE> SContext::LookupServiceAs(const SString& name) const
{
	return interface_cast<INTERFACE>(LookupService(name));
}	
#endif // !LIBBE_BOOTSTRAP

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif // SUPPORT_CONTEXT_H_
