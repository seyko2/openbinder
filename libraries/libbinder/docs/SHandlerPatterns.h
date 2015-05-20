/*!
@page SHandlerPatterns SHandler Patterns

<div class="header">
<center>\< @ref SAtomDebugging | @ref SupportKit | @ref BinderKit \></center>
<hr>
</div>

The Binder's SHandler class is used extensively to 
control multithreading in the system.  It is essentially a message 
queue, allowing you to place messages into the queue (by calling 
SHandler::PostMessage() et al) and to perform an action upon receiving that 
message (by overriding the SHandler::HandleMessage() method).

Unlike a typical message queue, there is no thread dedicated to 
servicing a particular SHandler.  Instead, the Binder dispatches 
threads from its thread pool to SHandler objects as needed.  When doing 
this, it ensures that the SHandler's semantics are properly observed 
(only one message being executed at a time), but may use a different 
thread each time a message is processed.

This design means that SHandler is very light-weight &mdash; it is 
essentially a list of pending messages and little more.  As such, it is 
common to just mix in a SHandler object with a particular class 
whenever multithreading is needed.  The flexible and light-weight 
nature of SHandler make it a useful tool for solving a wide variety of 
multithreading and timing-related problems.

-# @ref Asynchronicity
-# @ref Serialization
-# @ref Timeouts
-# @ref RepeatedEvents

@section Asynchronicity Asynchronicity

The basic use of SHandler is to introduce asynchronicity into a code 
path.  For example, you may be implementing a method that requires a 
significant amount of time to complete so want to return to the caller 
without waiting for the operation to be done.  To solve this, you can simply mix a 
SHandler in to your class and post a message to it.  You can then 
immediately return to the caller, and do your work when you receive the 
message.

@code
class MyCommand : public BCommand, public SHandler
{
public:
	virtual	SValue			Run(const SValue& args);

	virtual	status_t		HandleMessage(const SMessage &msg);

	...
}

SValue MyCommand::Run(const SValue& args)
{
	// Create a message.  The data will be set to
	// our incoming arguments.
	SMessage msg('doit');
	msg.SetData(args);

	// Post the message in our handler to be processed later.
	status_t err = PostMessage(msg);

	// Nothing interesting to return to the caller.
	return SValue::Status(err);
}

status_t MyCommand::HandleMessage(const SMessage &msg)
{
	if (msg.What() == 'doit') {
		// We got our message.  Do the work.

		SValue args = msg.Data();

		// ... do stuff ...

		return B_OK;
	}

	return SHandler::HandleMessage(msg);
}
@endcode

@section Serialization Serialization

Very related to asynchronicity is operation serialization.  This is the 
use of a SHandler as a message queue to ensure that a set of operations 
are executed in a specified order (determined by the order of messages 
in the queue), without requiring that we hold a lock while executing 
them.

This is a very important pattern because of our locking policies 
described in @ref BinderThreading, and in particular that we should not 
be holding any locks when calling out of an object.  If we can't hold 
any locks, and have no guarantees of the orders that calls will be made 
on our object, how can we ensure external calls are made in the proper 
order?

First, let's look at an example of the problem.  Here is a class with a 
variable that clients can set, which would then like to call on to another 
object when the variable changes.  As per our locking rules, we do not 
hold a lock when calling the target.

@code
class MyEnabler
{
public:
	MyEnabler(sptr<Target>& target)
		: m_target(target)
	{
	}

	void Enable()
	{
		m_lock.Lock();
		bool changed = !m_enabled;
		m_enabled = true;
		m_lock.Unlock();

		// ###1: RACE CONDITION!

		if (changed) m_target->NewValue(true);
	}

	void Disable()
	{
		m_lock.Lock();
		bool changed = m_enabled;
		m_enabled = false;
		m_lock.Unlock();

		// ###2: RACE CONDITION!

		if (changed) m_target->NewValue(false);
	}

private:
	const sptr<Target> m_target;

	SLocker m_lock;
	bool m_enabled;
};
@endcode

Notice the race conditions at points ###1 and ###2: if a context switch 
happens at that point, after we have updated m_enabled but before we 
have called NewValue(), then a second thread could enter the opposite 
function and completely execute, leaving m_target with the wrong value.

Here is a solution using SHandler.  The trick here is that we can 
safely call PostMessage() with the lock held, knowing that we can count 
on the handler processing the resulting messages in the same order that 
the were posted.

@attention
<em>For this specific kind of situation &mdash; the propagation of data 
changes &mdash; you are better off using BBinder's standard Link()/Push() 
facility.  This uses a SHandler internally 
to ensure correct ordering without holding locks, and has well-defined 
mechanisms for clients to bypass that overhead when it is not needed.</em>

@code
class MyEnabler : public SHandler
{
public:
	MyEnabler(sptr<Target>& target)
		: m_target(target)
	{
	}

	void Enable()
	{
		SLocker::Autolock _l(m_lock);
		if (!m_enabled) {
			SMessage msg('rprt');
			msg.SetData(SValue::Bool(true));
			status_t err = PostMessage(msg);
		}
		m_enabled = true;
	}

	void Disable()
	{
		SLocker::Autolock _l(m_lock);
		if (m_enabled) {
			SMessage msg('rprt');
			msg.SetData(SValue::Bool(false));
			status_t err = PostMessage(msg);
		}
		m_enabled = false;
	}

	status_t HandleMessage(const SMessage &msg)
	{
		if (msg.What() == 'rprt') {
			m_target->ValueChanged(msg.Data().AsBool());
			return B_OK;
		}

		return SHandler::HandleMessage(msg);
	}

private:
	const sptr<Target> m_target;

	SLocker m_lock;
	bool m_enabled;
};
@endcode

@section Timeouts Timeouts

Another feature of SHandler is that the messages in its queue are time
stamped, and not delivered until that time arrives.  When calling 
PostMessage(), the time stamp is set to the time of the call, resulting 
in the message being processed as soon as possible, as ordered by other 
messages in the queue.

You can use the alternative APIs SHandler::PostDelayedMessage() and 
SHandler::PostMessageAtTime() to place a message in the queue with a time in the 
future, either relative from the time it was posted or at an absolute 
time, respectively.  In addition, the API SHandler::RemoveMessages() allows you 
to get rid of selected messages currently waiting in the queue, which 
can be used as here to cancel a pending timeout.

@code
class Timeout : public SHandler
{
public:
	void StartTimer(nsecs_t duration)
	{
		// Here we place a message in the queue to be
		// delivered when the timeout will expire.  Note
		// the use of SHandler::POST_REMOVE_DUPLICATES to ensure
		// that any existing timeout is cancelled.
		PostDelayedMessage(
			SMessage('time'), duration, POST_REMOVE_DUPLICATES);
	}

	void CancelTimer()
	{
		// We cancel the timer by removing a pending timeout
		// message from the queue.  Note that we need to use
		// the SHandler::FILTER_FUTURE_FLAG or else any messages
		// whose time stamp is in the future will not be
		// removed.
		//
		// There is a kind of race condition here, where the
		// handler may already be pulling the message out of
		// the queue for processing when this function is
		// called.  In a way this is just a natural expected
		// outcome of the timeout being asynchronous &mdash; at
		// some point, it will be too late for CancelTimer()
		// to cancel the timeout.  There are many ways this
		// could be changed, for example having CancelTimer()
		// remove an alert if it is being shown by the timeout
		// handler.
		RemoveMesages('time', FILTER_FUTURE_FLAG);
	}

	status_t HandleMessage(const SMessage &msg)
	{
		if (msg.What() == 'time') {
			ErrFatalError(
				"The application is not responding!");
			return B_OK;
		}

		return SHandler::HandleMessage(msg);
	}
}
@endcode

@section RepeatedEvents Repeated Events

Often you want to do something over and over again at a regular 
interval.  For example, this is useful when displaying an animation on 
the screen.  You can use a SHandler to accomplish this by posting a 
timed message to the handler and, upon receiving that message, posting 
a new message for a later time.  For example, here we can see an 
SHandler being mixed in with a BView to run an animation at 20 
frames/sec.

@code
class MyView : public BView, public SHandler
{
public:
	virtual void		AttachedToGraphicPlane(const SGraphicPlane& plane, uint32_t flags);

	virtual	status_t	HandleMessage(const SMessage &msg);

	...
}

void MyView::AttachedToGraphicPlane(const SGraphicPlane& plane, 
uint32_t flags)
{
	BView::AttachedToGraphicPlane(plane, flags);

	if ((flags&B_FIRST_GRAPHIC_PLANE) != 0) {
		// Now attached to screen, start the update messages
		PostMessage(SMessage('updt', sysThreadPriorityUrgentDisplay),
			POST_REMOVE_DUPLICATES);
	}
}

status_t MyView::HandleMessage(const SMessage &msg)
{
	if (msg.What() == 'updt') {
		// We got our update message.  Invalidate the view.
		Invalidate();

		// If still attached to the screen, enqueue a new update
		// message to execute 1/20 second from now.
		if (GraphicPlane().InitCheck() == B_OK) {
			PostDelayedMessage(SMessage('updt', sysThreadPriorityUrgentDisplay),
				B_MS2NS(20), POST_REMOVE_DUPLICATES);
		}
		return B_OK;
	}

	return SHandler::HandleMessage(msg);
}
@endcode

*/
