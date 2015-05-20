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

#ifndef _SUPPORT_HANDLER_H
#define _SUPPORT_HANDLER_H

/*!	@file support/Handler.h
	@ingroup CoreSupportUtilities
	@brief High-level thread dispatching/scheduling class
*/

#include <support/Atom.h>
#include <support/Locker.h>
#include <support/Message.h>
#include <support/MessageList.h>
#include <support/SupportDefs.h>

// XXX remove
#include <support/Context.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class BProcess;
class SHandler;

typedef SHandler BHandler;

//!	Optimization for retrieving the current time.
nsecs_t approx_SysGetRunTime();
//!	Retrieves exact time and updates approx_SysGetRunTime().
nsecs_t exact_SysGetRunTime();	

/*----------------------------------------------------------------*/
/*----- SHandler class --------------------------------------------*/

//!	SHandler is an event queue to deliver and process SMessage objects.
/*!	The SHandler is a base class for executing
	asynchronous operations and/or performing serialization of those
	operations.  It is a mechanism for implementing concurrency.

	SHandler provides a set of posting methods for placing SMessage
	data into an execution queue. Each message in the queue is timestamped,
	and the messages in the queue are consecutively
	processed by SHandler in their timestamped order.

	Message delivery is asynchronous: your thread can return from
	delivering a message before the message has even started execution.
	However, to ensure the correct delivery order of the messages, the
	handler only processes one at a time.

	Messages are sent to a SHandler by calling one of PostMessage(),
	PostMessageAtTime(), PostDelayedMessage(), or
	EnqueueMessage().  You subclass from SHandler and override
	HandleMessage() to process messages that have been posted to the handler.

	SHandler can also be used to serialize with external operations.
	To do this, pass in an external lock to the constructor.  This
	lock will then be acquired when calling HandleMessage.  In this way, 
	the serialization of SHandler message handling can be also
	synchronized with external operations.

	See @ref SHandlerPatterns for many examples of threading
	techniques you can use with SHandler.

	@nosubgrouping
*/
class SHandler : public virtual SAtom
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
			
							SHandler();
			//!	Supply an external lock to be held when dispatching messages.
							SHandler(SLocker* externalLock);

			//!	@deprecated SHandler is no longer a BBinder object.
							SHandler(const SContext& context);

protected:
	virtual					~SHandler();
public:

	//@}

	// --------------------------------------------------------------
	/*!	@name Posting
		APIs to place messages into the handler's message queue. */
	//@{

			//! Flags for PostMessage(), PostDelayedMessage(), PostMessageAtTime(), EnqueueMessage().
			enum {
				//! Remove any pending message with the same What() code.
				/*! Using this flag is a semantically the same as first calling
					RemoveMessages(msgCode) before posting the message, but more
					efficient.  This is as opposed to POST_KEEP_UNIQUE.
					@note be careful with POST_REMOVE_DUPLICATES, as the data for
					any existing messages in the queue will be released inside of
					the call.  That means, if you aren't absolutely sure about
					what that data is, you MUST NOT do this while holding a lock. */
				POST_REMOVE_DUPLICATES	= 0x0001,

				//! Ensure this message's event code is unique in the queue.
				/*! This flag ensures that only one message with the given
					message code exists in the queue.  If, at the time of posting,
					there are already messages with the same code, the first such
					message will be kept with its data changed (moving it up earlier
					in the queue if needed for the new timestamp), and all other
					messages removed.  Unlike POST_REMOVE_DUPLICATES, posting
					a new message when there is already a pending message with the
					same code will not cause that message code to be moved later
					in the queue.
					@note be careful with POST_KEEP_UNIQUE, as the data for
					any existing messages in the queue will be released inside of
					the call.  That means, if you aren't absolutely sure about
					what that data is, you MUST NOT do this while holding a lock. */
				POST_KEEP_UNIQUE			= 0x0002
			};

			//!	Post a message to the handler, with its timestamp set to the current time.
			/*!	@see PostMessageAtTime()
			*/
			status_t		PostMessage(		const SMessage &message,
												uint32_t flags = 0);
			//!	Post a message to the handler, with a timestamp set to the current time plus @a delay.
			/*!	@see PostMessageAtTime()
			*/
			status_t		PostDelayedMessage(	const SMessage &message,
												nsecs_t delay,
												uint32_t flags = 0);
			//!	Post a message to the handler, which is expected to arrive at the time @a absoluteTime.
			/*!	The handler will enqueue this message, but not deliver it
				until after the selected time.  If the timestamp is set to
				the current time, the message will be delivered as soon as
				possible, but not before any older pending messages have
				been handled.  Normally, a handler executes only one message
				at a time, in their timestamp order.

				This method ensures that \a absoluteTime is later than the
				current time, to protect against livelocks.  You can use
				EnqueueMessage() to avoid this constraint.

				The optional @a flags may be POST_REMOVE_DUPLICATES or
				POST_KEEP_UNIQUE.

				@see PostMessage(), PostDelayedMessage(), EnqueueMessage()
			*/
			status_t		PostMessageAtTime(	const SMessage &message,
												nsecs_t absoluteTime,
												uint32_t flags = 0);
			//!	Deposit a message a message into the handler's queue.
			/*! The message will be delivered at the given absolute time.
				@note Unlike PostMessageAtTime(), this function does not try
				to prevent starvation by ensuring the time is later than now.
			*/
			status_t		EnqueueMessage(		const SMessage& message,
												nsecs_t time,
												uint32_t flags = 0);

	//@}

	// --------------------------------------------------------------
	/*!	@name Handling
		APIs used to handle messages that are delivered to the handler. */
	//@{

			//!	Subclasses override this to receive messages.
	virtual	status_t		HandleMessage(const SMessage &msg);

			//!	Make handler ready to process the next message in its queue.
			/*!	You can call this function while in your HandleMessage()
				method to let the handler schedule itself for the next
				message in its queue, allowing it to continue processing
				even before you have returned from HandleMessage().
			*/
			void			ResumeScheduling();

	//@}

	// --------------------------------------------------------------
	/*!	@name Queue Manipulation
		APIs to manipulate message in the queue queue. */
	//@{

			//! Actions you can return to FilterMessages() from your filter_func.
			enum filter_action {
				FILTER_REMOVE			= 0,			//!< Remove message from queue
				FILTER_KEEP				= 1,			//!< Keep message in queue
				FILTER_STOP				= 2				//!< Do not filter any more messages
			};

			//! Flags for FilterMessages() and RemoveMessages().
			enum {
				FILTER_REVERSE_FLAG		= 0x0001,		//!< Filter in reverse time order
				FILTER_FUTURE_FLAG		= 0x0002		//!< Include messages after current time
			};

			//! Function callback for FilterMessages().
			/*!	@see FilterMessages() */
			typedef	filter_action	(*filter_func)(const SMessage* message, void* data);

			//! Functor callback for FilterMessages().
			/*!	@see FilterMessages() */
			class filter_functor_base
			{
			public:
				virtual filter_action operator()(const SMessage* message, void* user) const = 0;
				virtual ~filter_functor_base() { }
			};

			template<class T>
			class filter_functor : public filter_functor_base
			{
			public:
				typedef filter_action (T::*method_ptr_t)(const SMessage* message, void* user) const;
				filter_functor(const T& o, method_ptr_t m) : object(o), method(m) { }
				filter_functor(const T* o, method_ptr_t m) : object(*o), method(m) { }
				virtual filter_action operator()(const SMessage* message, void* user) const {
					return (object.*method)(message, user);
				}
			private:
				const T&		object;
				method_ptr_t	method;
			};

		
			//!	Iterate through this handler's message queueue, selecting whether to keep or remove messages.
			/*!	Your filter_func selects the action to perform for each message
				by the result it turns.  If it selects to remove a message, the
				message will either be placed into \a outRemoved if that is non-NULL,
				or immediately deleted.
				@note filter_func is called WITH THE HANDLER LOCKED.  You can not
				call back into the handler from it, and must be very careful about
				what other locks you acquire.
			*/
			void			FilterMessages(	filter_func func, uint32_t flags,
											void* data,
											SMessageList* outRemoved = NULL);

			void			FilterMessages(	const filter_functor_base& functor, uint32_t flags,
											void* data,
											SMessageList* outRemoved = NULL);
			
			//!	Perform FilterMessages() where all messages that match \a what are deleted.
			/*!	@see FilterMessages() */
			void			RemoveMessages(	uint32_t what, uint32_t flags,
											SMessageList* outRemoved = NULL);
			
			//!	Perform FilterMessages() where all messages are deleted.
			/*!	@see FilterMessages() */
			void			RemoveAllMessages(SMessageList* outRemoved = NULL);

			//!	Return the number of pending message that match \a what.
			int32_t			CountMessages(uint32_t what=B_ANY_WHAT);

	//@}

	// --------------------------------------------------------------
	/*!	@name Dispatching Primitives
		Low-level manipulation of the handler's message dispatching.
		@note These should not be used unless you absolutely know what you
		are doing. */
	//@{

			//!	Return the time of the next message in the queue.
			nsecs_t			NextMessageTime(int32_t* out_priority);
			
			//!	Remove next pending message with code \a what from the handler's message queue.
			/*!	You are responsible for deleting the returned message.
				If there are no matching pending message, NULL is returned.
			*/
			SMessage*		DequeueMessage(uint32_t what);
			
			//!	Immediately handle all messages currently in the queue.
			void 			DispatchAllMessages();
			
	//@}

private:
	friend	class			BProcess;

							SHandler(const SHandler&);
			SHandler		operator=(const SHandler&);
	
			enum scheduling {
				CANCEL_SCHEDULE = 0,
				DO_SCHEDULE,
				DO_RESCHEDULE
			};

			status_t		enqueue_unique_message(const SMessage& message, nsecs_t time, uint32_t flags);

			void			defer_scheduling();
			void			unschedule();
			scheduling		start_schedule();
			void			done_schedule();

			status_t		dispatch_message();

			sptr<BProcess>	m_team;
			SLocker			m_lock;
			SMessageList	m_msgQueue;
			uint32_t		m_state;
			
			SHandler *		m_next;
			SHandler *		m_prev;

			SLocker*		m_externalLock;
};

enum {
	B_POST_REMOVE_DUPLICATES	= SHandler::POST_REMOVE_DUPLICATES,	//!< @deprecated Use SHandler::POST_REMOVE_DUPLICATES instead.
	B_POST_KEEP_UNIQUE			= SHandler::POST_KEEP_UNIQUE,		//!< @deprecated Use SHandler::POST_KEEP_UNIQUE instead.

	//! @deprecated Synonym for B_POST_REMOVE_DUPLICATES
	B_POST_UNIQUE_MESSAGE		= B_POST_REMOVE_DUPLICATES
};

//! Flags for SHandler::FilterMessages() and SHandler::RemoveMessages().
enum {
	B_FILTER_REVERSE_FLAG		= SHandler::FILTER_REVERSE_FLAG,//!< @deprecated Use SHandler::FILTER_REVERSE_FLAG instead.
	B_FILTER_FUTURE_FLAG		= SHandler::FILTER_FUTURE_FLAG	//!< @deprecated Use SHandler::FILTER_FUTURE_FLAG instead.
};


/**************************************************************************************/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_HANDLER_H */
