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

#ifndef	_SUPPORT_CONDITIONVARIABLE_H
#define	_SUPPORT_CONDITIONVARIABLE_H

/*!	@file support/ConditionVariable.h
	@ingroup CoreSupportUtilities
	@brief Lightweight condition variable synchronization class.
*/

#include <support/SupportDefs.h>
#include <support/Atom.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SLocker;

/*-------------------------------------------------------------*/
/*----- SConditionVariable class ------------------------------*/

//!	Lightweight condition variable synchronization.
class SConditionVariable
{
public:
							SConditionVariable();
							SConditionVariable(const char* name);
							~SConditionVariable();	

		//!	Block until this condition is open.
		void				Wait();

		//!	Block until this condition is open, protected by 'locker'.
		/*!	The SLocker must be held when this method
			is called.  It will be atomically released if/when
			the thread blocks, and returns with the lock again held. */
		void				Wait(SLocker& locker);

		//! Open the condition.
		/*!	All waiting threads will be allowed to run.  Any future
			threads that call Wait() will not block. */
		void				Open();

		//! Close the condition.
		/*!	Any future threads that call Wait() will block until it opens. */
		void				Close();

		//! Pulse the condition.
		/*!	Allow all waiting threads to run, be return with the condition closed. */
		void				Broadcast();

private:
		// SConditionVariable can't be copied
							SConditionVariable(const SConditionVariable&);
		SConditionVariable& operator = (const SConditionVariable&);

		volatile int32_t				m_var;
};

/*-------------------------------------------------------------*/
/*----- SConditionAtom class ----------------------------------*/

class SConditionAtom : public virtual SAtom
{
public:
	inline 			SConditionAtom(SConditionVariable *cv) :m_cv(cv) { }
	inline virtual	~SConditionAtom()	{ m_cv->Open(); }
	
	inline void	Wait()					{ m_cv->Wait(); }
	inline void Wait(SLocker &locker)	{ m_cv->Wait(locker); }
	inline void Open()					{ m_cv->Open(); }
	inline void Close()					{ m_cv->Close(); }
	inline void Broadcast()				{ m_cv->Broadcast(); }
	
private:
	SConditionAtom();
	SConditionAtom(const SConditionAtom &);
	SConditionVariable *m_cv;
};

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif /* _SUPPORT_CONDITIONVARIABLE_H */
