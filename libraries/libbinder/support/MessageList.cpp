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

#include <support/MessageList.h>

#include <support/Handler.h>
#include <support/Message.h>
#include <support/StdIO.h>
#include <support/ITextStream.h>
#include <support/StdIO.h>
#include <support/String.h>
#include <support/Debug.h>

#include <DebugMgr.h>
#include <ErrorMgr.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*----------------------------------------------------------------*/

SMessageList::SMessageList()
	: fHead(NULL), fTail(NULL), fCount(0)
{
}

SMessageList::~SMessageList()
{
	MakeEmpty();
}

#if 0
SMessageList::SMessageList(const SMessageList &o)
	: fHead(NULL), fTail(NULL), fCount(0)
{
	*this = o;
}

SMessageList& SMessageList::operator=(const SMessageList &o)
{
	MakeEmpty();
	SMessage* p = o.fHead;
	SMessage* last = NULL;
	while (p) {
		SMessage* copy = new SMessage(*p);
		if (!last) {
			fHead = copy;
		} else {
			last->m_next = copy;
			copy->m_prev = last;
		}
		last = copy;
		p = p->m_next;
	}
	fTail = last;
	return *this;
}
#endif

SMessageList& SMessageList::Adopt(SMessageList &o)
{
	MakeEmpty();
	fHead = o.fHead;
	fTail = o.fTail;
	fCount = o.fCount;
	o.fHead = o.fTail = NULL;
	o.fCount = 0;
	return *this;
}

/*----------------------------------------------------------------*/

void SMessageList::EnqueueMessage(SMessage* msg)
{
	nsecs_t thisStamp = msg->When();
	if (thisStamp <= 0) {
		thisStamp = approx_SysGetRunTime();
		msg->SetWhen(thisStamp);
	}
	
	SMessage* pos = fTail;
	while (pos && pos->When() > thisStamp) {
		pos = pos->m_prev;
	}
	
	InsertAfter(msg, pos);
}

SMessage* SMessageList::EnqueueMessageRemoveDups(const SMessage& inmsg, nsecs_t thisStamp, SValue* outPrevData, SMessageList* outRemoved)
{
	if (thisStamp <= 0) thisStamp = approx_SysGetRunTime();
	uint32_t thisWhat = inmsg.What();
	
	//bout << "Enqueue Message Remove Dups " << STypeCode(thisWhat) << " @ " << thisStamp << endl;

	SMessage* pos = fTail;
	SMessage* prev = NULL;
	bool removeOldMsgs = false;
	while (pos && pos->When() > thisStamp) {
		if (prev != NULL && prev->What() == thisWhat) {
			removeOldMsgs = true;
		}
		prev = pos;
		pos = pos->m_prev;
	}
	
	SMessage* msg;
	if (prev && prev->What() == thisWhat) {
		// There is already a message at this location with the same
		// code, so we can just replace it.
		msg = prev;
		//bout << "Replacing existing message " << msg << endl;
		*outPrevData = msg->Data();
		*msg = inmsg;
	} else {
		msg = new B_NO_THROW SMessage(inmsg);
		if (msg == NULL) return NULL;
		//bout << "Adding new message " << msg << " at " << pos << endl;
		InsertAfter(msg, pos);
	}
	msg->SetWhen(thisStamp);
	
	// Now remove any others.
	if (removeOldMsgs) {
		prev = msg->m_next;
		while (prev) {
			SMessage* next = prev->m_next;
			if (prev->What() == thisWhat) {
				//bout << "Removing dup later message " << prev << endl;
				Remove(prev);
				outRemoved->AddTail(prev);
			}
			prev = next;
		}
	}
	while (pos) {
		prev = pos->m_prev;
		if (pos->What() == thisWhat) {
			//bout << "Removing dup earlier message " << pos << endl;
			Remove(pos);
			outRemoved->AddHead(pos);
		}
		pos = prev;
	}

	return msg;
}

SMessage* SMessageList::EnqueueUniqueMessage(const SMessage& inmsg, nsecs_t thisStamp, SValue* outPrevData, SMessageList* outRemoved)
{
	if (thisStamp <= 0) thisStamp = approx_SysGetRunTime();
	uint32_t thisWhat = inmsg.What();
	
	//bout << "Enqueue Unique Message " << STypeCode(thisWhat) << " @ " << thisStamp << endl;

	SMessage* pos = fHead;
	SMessage* msg = NULL;

	// Look for the first message in the list...
	while (pos && pos->When() <= thisStamp) {
		if (pos->What() == thisWhat) {
			// There is already a previous message with the given
			// code.  Repurpose it, and then remove the remaining
			// messages.
			msg = pos;
			*outPrevData = pos->Data();
			pos->SetData(inmsg.Data());
			pos->SetPriority(inmsg.Priority());
			pos = pos->m_next;
			break;
		}
		pos = pos->m_next;
	}

	// Do we still need to add our message?
	if (!msg) {
		// Small optimization -- 'pos' is now after the place we
		// would like to add the message.  If it has the same code,
		// then we can replace it instead of inserting a new
		// message and then removing it.
		if (pos && pos->What() == thisWhat) {
			msg = pos;
			*outPrevData = pos->Data();
			pos->SetData(inmsg.Data());
			pos->SetPriority(inmsg.Priority());
			pos = pos->m_next;

		// If we didn't replace an existing message, add the new
		// message at the correct place.
		} else {
			msg = new B_NO_THROW SMessage(inmsg);
			if (msg == NULL) return NULL;
			msg->SetWhen(thisStamp);
			//bout << "Adding new message " << msg << " at " << pos << endl;
			InsertBefore(msg, pos);
		}
	}

	// Now remove all remaining messages from list.
	while (pos) {
		SMessage* const next = pos->m_next;
		if (pos->What() == thisWhat) {
			Remove(pos);
			outRemoved->AddTail(pos);
		}
		pos = next;
	}

	return msg;
}

SMessage* SMessageList::DequeueMessage(uint32_t what, bool *more)
{
	if (what == B_ANY_WHAT) {
		SMessage* msg = RemoveHead();
		if (more) *more = fCount ? true : false;
		return msg;
	}

	SMessage* msg = fHead;
	while (msg && (what != msg->What())) {
		msg = msg->m_next;
	}

	if (msg) Remove(msg);

	if (more) *more = fCount ? true : false;
	return msg;
}

nsecs_t SMessageList::OldestMessage(int32_t* out_priority) const
{
	if (fHead) {
		if (out_priority) *out_priority = fHead->Priority();
		return fHead->When();
	}
	if (out_priority) *out_priority = 10;
	return B_INFINITE_TIMEOUT;
};

/*----------------------------------------------------------------*/

const SMessage* SMessageList::Head() const
{
	return fHead;
}

const SMessage* SMessageList::Tail() const
{
	return fTail;
}

const SMessage* SMessageList::Next(const SMessage* current) const
{
	return current->m_next;
}

const SMessage* SMessageList::Previous(const SMessage* current) const
{
	return current->m_prev;
}

/*----------------------------------------------------------------*/

void SMessageList::AddHead(SMessage* message)
{
	DbgOnlyFatalErrorIf(message->m_next || message->m_prev, "SMessageList::AddHead: Message already in list");
	message->m_next = fHead;
	if (fHead) fHead->m_prev = message;
	fHead = message;
	if (!fTail) fTail = message;
	fCount++;
}

void SMessageList::AddTail(SMessage* message)
{
	DbgOnlyFatalErrorIf(message->m_next || message->m_prev, "SMessageList::AddTail: Message already in list");
	message->m_prev = fTail;
	if (fTail) fTail->m_next = message;
	fTail = message;
	if (!fHead) fHead = message;
	fCount++;
}

void SMessageList::InsertBefore(SMessage* message, const SMessage* position)
{
	if (!position) {
		AddTail(message);
		return;
	}
	DbgOnlyFatalErrorIf(message->m_next || message->m_prev, "SMessageList::InsertBefore: Message already in list");
	if (position->m_prev) {
		message->m_prev = position->m_prev;
		position->m_prev->m_next = message;
	} else {
		DbgOnlyFatalErrorIf(fHead != position, "SMessageList::InsertBefore: Found NULL m_prev that is not head");
		fHead = message;
	}
	message->m_next = const_cast<SMessage*>(position);
	const_cast<SMessage*>(position)->m_prev = message;
	fCount++;
}

void SMessageList::InsertAfter(SMessage* message, const SMessage* position)
{
	if (!position) {
		AddHead(message);
		return;
	}
	DbgOnlyFatalErrorIf(message->m_next || message->m_prev, "SMessageList::InsertAfter: Message already in list");
	if (position->m_next) {
		message->m_next = position->m_next;
		position->m_next->m_prev = message;
	} else {
		DbgOnlyFatalErrorIf(fTail != position, "SMessageList::InsertBefore: Found NULL m_next that is not tail");
		fTail = message;
	}
	message->m_prev = const_cast<SMessage*>(position);
	const_cast<SMessage*>(position)->m_next = message;
	fCount++;
}

/*----------------------------------------------------------------*/

SMessage* SMessageList::RemoveHead()
{
	SMessage* ret = fHead;
	if (!ret) return NULL;
	fHead = ret->m_next;
	if (fTail == ret) fTail = NULL;
	if (fHead) fHead->m_prev = NULL;
	ret->m_next = ret->m_prev = NULL;
	fCount--;
	return ret;
}

SMessage* SMessageList::RemoveTail()
{
	SMessage* ret = fTail;
	if (!ret) return NULL;
	fTail = ret->m_prev;
	if (fHead == ret) fHead = NULL;
	if (fTail) fTail->m_next = NULL;
	ret->m_next = ret->m_prev = NULL;
	fCount--;
	return ret;
}

SMessage* SMessageList::Remove(const SMessage* message)
{
	if (message->m_next) {
		message->m_next->m_prev = message->m_prev;
	} else {
		if (fTail != message) {
			return NULL;
		}
		fTail = message->m_prev;
	}
	if (message->m_prev) {
		message->m_prev->m_next = message->m_next;
	} else {
		if (fHead != message) {
			return NULL;
		}
		fHead = message->m_next;
	}
	const_cast<SMessage*>(message)->m_next
		= const_cast<SMessage*>(message)->m_prev = NULL;
	fCount--;
	return const_cast<SMessage*>(message);
}

/*----------------------------------------------------------------*/

int32_t SMessageList::CountMessages(uint32_t what) const
{
#if DEBUG
	int32_t i=0;
	const SMessage* m = Head();
	while (m) {
		m = Next(m);
		i++;
	}
	if (i != fCount) {
		bout << "SMessageList: count out of sync, have " << fCount
			 << " but should be " << i << endl;
	}
#endif

	if (what == B_ANY_WHAT) return fCount;
	
	int32_t count = 0;
	SMessage *msg = fHead;
	while (msg) {
		if (what == msg->What()) count++;
		msg = msg->m_next;
	};
	
	return count;
}

bool SMessageList::IsEmpty() const
{
	return !fHead;
}

void SMessageList::MakeEmpty()
{
	SMessage* cur = fHead;
	while (cur) {
		SMessage* next = cur->m_next;
		cur->m_next = cur->m_prev = NULL;
		delete cur;
		cur = next;
	}
	fHead = fTail = NULL;
	fCount = 0;
}

/* ---------------------------------------------------------------- */

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SMessageList& list)
{
#if SUPPORTS_TEXT_STREAM
	const SMessage* pos = list.Head();
	int32_t index = 0;
	SString tmp;
	while (pos) {
		io << "Message #" << index << " (what=" << STypeCode(pos->What())
		   << " at time=" << pos->When() << ")";
		pos = list.Next(pos);
		if (pos) io << endl;
		index++;
	}
#else
	(void)list;
#endif

	return io;
}

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
