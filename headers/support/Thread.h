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

#ifndef _SUPPORT_THREAD_H_
#define _SUPPORT_THREAD_H_

/*!	@file support/Thread.h
	@ingroup CoreSupportUtilities
	@brief Object-oriented thread creation.
*/

#include <support/Atom.h>
#include <support/Locker.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/*-------------------------------------------------------------*/
/*------- SThread Implementation ------------------------------*/

//!	Class that creates a new active thread.
/*!	Don't use this unless you have a good reason to do
	so.  Most threading needs can be served by sending messages
	to a BHandler, which (a) is easier to use; (b) consumes much
	less resources; (c) more flexible; and (d) will probably let
	you avoid some amount of pain down the road.  However, if
	you've really absolutely gotta have your own thread, here it
	is.  Have fun.
*/
class SThread : public virtual SAtom
{
public:

								SThread();

	//!	Start the thread.
	/*!	Returns B_OK if it is off and running.  The
		thread is assigned the given name, standard priority level at
		which to run, and number of bytes to allocate for its stack. */
	virtual	status_t			Run(const char* name, int32_t priority, size_t stackSize);

	//!	Request for the object's thread to exit.
	/*!	Returns immediately; does not wait for the thread to actually do so.
		Note that the thread itself must take care of handling this --
		either by looping or explicitly checking with ExitRequested(). */
	virtual	void				RequestExit();
	//!	Block until this object's thread exits.  BEWARE DEADLOCKS!
			void				WaitForExit();

protected:
	virtual						~SThread();

	//!	Main entry of thread.
	
	/*!	Two models of execution are supported.

		Looping: If you return true from this function, it will be
		called again if (a) someone still holds a reference on the
		object; and (b) RequestExit() has not been called.  Thus your
		thread will continually perform some action until the object goes
		away or someone requests for it to end.

		One-shot: If you return false from this function, the thread
		will immediately exit.
	*/
	virtual	bool				ThreadEntry() = 0;
	//!	Called after the thread is created but before started to give derived classes a chance to do stuff.
	virtual	status_t			AboutToRun(SysHandle thread);

	//!	Return true if RequestExit() has been called.
			bool				ExitRequested() const;

private:
#if TARGET_HOST == TARGET_HOST_PALMOS
	static	void				top_entry(void* object);
#else
	static	int32_t				top_entry(void* object);
#endif

			status_t			m_status;
			SysHandle			m_thread;

			SLocker				m_lock;
#if 0 // FIXME: Linux
			SysConditionVariableType	m_ended;	// condition var
#else
			void*				m_ended;	// condition var
#endif
			bool				m_ending : 1;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // end namespace palmos::support
#endif

#endif	/* _SUPPORT_THREAD_H_ */
