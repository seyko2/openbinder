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

#ifndef _SUPPORT_MESSAGELIST_H
#define _SUPPORT_MESSAGELIST_H

/*!	@file support/MessageList.h
	@ingroup CoreSupportUtilities
	@brief List of SMessage objects, to implement message queues.
*/

#include <support/ITextStream.h>
#include <support/SupportDefs.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SMessage;

enum {
	B_ANY_WHAT = 0
};

/*----------------------------------------------------------------------*/

//!	List of SMessage objects, used to implement message queues.
class SMessageList 
{
public:
		/*! @name Creation and Destruction */
		//@{

						SMessageList();
						~SMessageList();

		//!	Transfer ownership of messages from argument to this message list.
		/*!	Messages are merged to @a this in timestamp order.  The @a other list
			is empty upon return. */
		SMessageList&	Adopt(SMessageList& other);

		//@}

	
		/*!	@name Queue Operations
			These manage the message list as a queue, keeping the messages
			sorted by their timestamp order. */
		//@{

		//!	Place a new message into the queue.
		/*!	The input @a msg is added to the linked list (the message takes
			ownership of the object).  The message's timestamp is set to
			SysGetRunTime() if it does not yet have a time. */
		void			EnqueueMessage(SMessage* msg);

		//!	Like EnqueueMessage(), but first removes all other messages with the same code.
		/*!	Adds a @b copy of @a msg to the list, placed at the given @a time in the
			list.  Any other messages in the list with the same time stamp are removed
			and placed in to @a outRemoved.
			@param[in] msg The message being added, a copy is made to place in the queue.
			@param[in] time Time at which the message should be placed in the queue.
			@param[out] outPrevData If a message in the queue was replaced with this message,
				this is the data that was in the old message.
			@param[out] outRemoved Filled in with any message objects that were removed
				from the queue.
			@return Address of the message that was placed in the queue.
		*/
		SMessage*		EnqueueMessageRemoveDups(	const SMessage& msg, nsecs_t time,
													SValue* outPrevData, SMessageList* outRemoved);

		//!	Like EnqueueMessage(), but ensures the message code is unique in the list.
		/*!	Adds a @b copy of @a msg to the list.  The message will be placed either at
			the @a time given, or the earliest time for which there currently exists a
			message with the same code.  This is unlike EnueueMessageRemoveDups(), which
			always returns with the enqueued message having the same timestamp that was
			passed in.
			@param[in] msg The message being added, a copy is made to place in the queue.
			@param[in] time The @b oldest time at which the message should be placed in the queue.
				If there is already a message in the queue with the same code but an earlier
				time, that time is used instead.
			@param[out] outPrevData If a message in the queue was replaced with this message,
				this is the data that was in the old message.
			@param[out] outRemoved Filled in with any message objects that were removed
				from the queue.
			@return Address of the message that was placed in the queue.
			
			@note If there are messages in the list with the same code
			but @e before this one in time, the contents of @a msg will replace the
			first of them and take that timestamp instead, removing all others.  In
			this case @a outPrevData will contain the contents of the previous message.
		*/
		SMessage*		EnqueueUniqueMessage(	const SMessage& msg, nsecs_t time,
												SValue* outPrevData, SMessageList* outRemoved);

		//!	Remove the next message from the queue.
		/*!	If @a what is specified, this will remove the next message of the specific
			code; otherwise the next message of any code is removed.  If @a more is
			supplied, it will be set to true if more messages remain in the list. */
		SMessage*		DequeueMessage(uint32_t what = B_ANY_WHAT, bool *more = NULL);

		//!	Find information about the next message in the list.
		/*!	Returns the time at which the next message should be executed, and
			optionally @a out_priority is filled in with the priority of that message. */
		nsecs_t 		OldestMessage(int32_t* out_priority) const;

		//@}

		/*!	@name Iteration
			These APIs allow you to step over the linked list of messages. */
		//@{

		//! Retrieve first message in the list.
		const SMessage*	Head() const;
		//!	Retrieve last message in the list.
		const SMessage*	Tail() const;
		//!	Return the next message after @a current in the list.
		const SMessage*	Next(const SMessage* current) const;
		//!	Return the previous message before @a current in the list.
		const SMessage*	Previous(const SMessage* current) const;

		//@}

		/*!	@name Adding
			These are low-level APIs for placing messages into the list. */
		//@{

		//!	Place the given message at the front of the list.
		void			AddHead(SMessage* message);
		//!	Place the given message at the end of the list.
		void			AddTail(SMessage* message);
		//!	Place the given message immediately before @a position in the list.
		/*!	If @a position is NULL, the message is added to the end
			of the list (the same as calling AddTail(message)). */
		void			InsertBefore(SMessage* message, const SMessage* position);
		//!	Place the given message immediately after @a position in the list.
		/*!	If @a position is NULL, the message is added to the front
			of the list (the same as calling AddHead(message)). */
		void			InsertAfter(SMessage* message, const SMessage* position);

		//@}
		
		/*!	@name Removing
			Low-level APIs for removing messages from the list. */
		//@{

		//!	Remove the first message from the list and return it.
		SMessage*		RemoveHead();
		//!	Remove the last message from the list and return it.
		SMessage*		RemoveTail();
		//!	Remove the given message from the list and return it.
		SMessage*		Remove(const SMessage* message);

		//@}

		/*!	@name Global Operations
			Functionality that manipulates the message list as a whole. */
		//@{

		//!	Return the total number of messages in the list.
		/*!	If @a what is supplied, only messages with that code are
			counted. */
		int32_t			CountMessages(uint32_t what = B_ANY_WHAT) const;
		//!	Return true if there are no messages in the list.
		bool			IsEmpty() const;
		//!	Remove all messages from the list, deleting the objects.
		void			MakeEmpty();

		//@}
		
/*----- Private or reserved -----------------------------------------*/

private:
						SMessageList(const SMessageList &);
		SMessageList&	operator=(const SMessageList &);

		SMessage		*fHead;
		SMessage		*fTail;
		int32_t			fCount;
		uint32_t		_reserved[2];
};

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SMessageList& list);

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_MESSAGELIST_H */
