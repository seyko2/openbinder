/*!
@page BinderThreading Threading Conventions

<div class="header">
<center>\< @ref APIConventions | @ref OpenBinder | @ref EnvironmentVars \></center>
<hr>
</div>

The Binder is designed to enable rich multithreaded programming.  It does not, however,
define a concrete threading model itself; instead, it serves as a generic framework and
toolkit through which specific threading policies can be implemented.  This document will
look at the basic Binder threading features and how to use them to solve specific
problems.

-# @ref BinderThreadingModel
  -# @ref BinderThreadingSafety
    -# @ref BinderThreadingGlobalSafety
    -# @ref BinderThreadingInstanceSafety
    -# @ref BinderThreadingInterfaceSafety
  -# @ref BinderThreadingManaging
    -# @ref BinderThreadingSHandler
    -# @ref BinderThreadingSThread
  -# @ref BinderThreadingSynchronization
-# @ref BinderThreadingSystemPolicies
  -# @ref BinderThreadingFineGrained
    -# @ref BinderThreadingBasicLocking
    -# @ref BinderThreadingCopyTypes
    -# @ref BinderThreadingReferenceCounting
  -# @ref BinderThreadingRelease
  -# @ref BinderThreadingSubclass
  -# @ref BinderThreadingView

@section BinderThreadingModel Binder Threading Model

The only threading model that the Binder provides is what would be called "free threading"
in COM.  That is, individual threads are free to call across Binder objects as needed.
The threading policies are defined entirely by what thread calls where when; the Binder
is simply a mechanism to allow them to do what they want.

This threading model extends across processes, where the Binder makes operations on
remote objects look almost exactly like a call on to a local object &mdash; as if the thread
had moved into the process of the other object, executed there, and then returned with
the result.  For example, when you call into a remote object, the thread in the remote
process executing your call will have the same priority as your own thread.  (See
@ref BinderProcessModel for more detail on how the Binder manages processes.)

For the developer this means that, without any additional constraints, all Binder objects
must be able to deal with any arbitrary threads calling their API at any time.  You must
take care of correctly locking your state and serializing operations where appropriate.
In general application developers can use whatever locking policy they would like; system
developers should follow the system locking policies (described below) which are designed
to allow this flexibility for developers and help us avoid deadlocks in system code.

@subsection BinderThreadingSafety What Is Thread Safe?

When using an API with multiple threads, it is essential to know which parts of it
are thread safe and in what way.  There are three broad categories of thread safety
that we use.  Starting from the least thread safe to the most, here are the kinds
of classes and functions you will deal with:

@subsubsection BinderThreadingGlobalSafety Global (or Class) Safety

This is a class or function that makes sure to protect any of its global state.
For example, in the Binder, two threads can be using two @e different instances
of the SString class, and we guarantee that no normal operation of one thread
will disrupt the operation of the other.  The guarantee is made even though
SString uses copy-on-write, so two threads could be sharing the same data
buffer &mdash; the implementation ensures that with normal use of the SString API
this sharing will not cause problems between threads.

<b>Every API in the Binder is at @e least globally safe.</b>  You do not need
to worry about two threads using the same class, as long as they are using
different instances of the class.

@subsubsection BinderThreadingInstanceSafety Instance Safety

There are two broad types of classes in the Binder: those that protect their
instance data from multiple threads using it at the same time, and those that
don't.

For the most part, this aspect of a class corresponds directly to the memory
management of the class &mdash; almost all classes that are reference counted are
thread safe, while almost all classes that are not reference counted are not
thread safe.

In other words, basic types that are passed by value (such as
SString, SValue, SVector, and SMessage) are not thread safe.  Sharing such
classes between threads will require that you either implement the thread
safety yourself (through your own lock, probably protecting other member
variables in your class as well), or give the threads two different copies
of the same object.  Note that the latter is often a quite practical
solution, since for many reasons these types of classes are designed to
be cheap to copy, so they will often end up simply referring to the same
underlying data when you make a copy.

Classes that are reference counted &mdash; usually easy to determine because
they derive from SAtom or occasionally SLightAtom &mdash; are almost always
thread safe.  This is not surprising, because one of the reasons to do
reference counting is to make it much easier to manage memory between
multiple threads.  Examples of such classes are BBinder, BStreamDatum,
and SHandler.

There are some exceptions to these rules, and when that happens the
exception will be explicitly noted by the class.  This means that, except were
explicitly called out, any class whose prefix is "B" or "I" will be thread safe
(since all such classes must be reference counted via SAtom), while classes
that begin with "S" may or may not be thread safe depending on the above
discussion.

@subsubsection BinderThreadingInterfaceSafety Interface Safety

A final kind of safety is interface thread safety.  It is possible to
have a class that is internally thread safe, but whose interface is
not thread safe.  A good example of this is the IByteInput, IByteOutput,
and IByteSeekable interfaces.  Consider the complete API that these
interfaces define:

@code
virtual	ssize_t		ReadV(const iovec *vector, ssize_t count, uint32_t flags = 0);
virtual	ssize_t		WriteV(const iovec *vector, ssize_t count, uint32_t flags = 0);
virtual	off_t		Position() const;
virtual off_t		Seek(off_t position, uint32_t seek_mode);
@endcode

An implementation of this API may be entirely thread safe, such that if one
thread is calling ReadV() at the same time as another thread is calling
WriteV(), they will not corrupt the state of the class.  Note, however, that
what one thread does has unintended side-effects (by changing the current
stream position) that can break the behavior of the other thread.

For example, if one thread is calling the byte stream interfaces like this:

@code
stream->Seek(0, SEEK_SET);
stream->Read(&vec, 1);
@endcode

Another thread could call Seek(0, SEEK_END) between those two statements,
causing the first thread to do the wrong thing.

As a contrast, the IStorage interface represents an alternative API that
allows you to do the same thing as byte streams, but in a thread safe way:

@code
virtual	off_t		Size() const;
virtual	status_t	SetSize(off_t size);

virtual	ssize_t		ReadAtV(off_t position, const struct iovec *vector, ssize_t count);
virtual	ssize_t		WriteAtV(off_t position, const struct iovec *vector, ssize_t count);
@endcode

In general, all Binder interfaces are designed to be thread safe, and it
is important for that to be the case.  There are a few exceptions to this rule
(byte streams and IIterable being the most significant), which will be
explicitly called out.

@subsection BinderThreadingManaging Managing Threads

So what thread is your code running in?  Generally, this is not something you care about.
The Binder maintains a pool of threads per process, which it uses to dispatch operations.
These threads will call into your object as needed; they are managed as anonymous units
of execution, with no identity of their own.

Some of the frameworks built on top of the Binder impose particular threading models.
For example, the BSerialObserver object ensures that all calls to its Observed() are
serialized.  Even here, however, there is not a particular thread dedicated to
executing these functions; instead the class uses a Binder facility called SHandler to
pull threads out of the thread pool in the desired way.

@subsubsection BinderThreadingSHandler SHandler

The page @ref SHandlerPatterns describes uses of SHandler in fair detail.  This class
is the primary
mechanism used in Binder programming to manage threads.  Because it uses threads from the
Bindrer thread pool instead of having its own dedicated thread, it is a very light-weight
threading mechanism that is useful for a wide variety of threading needs.

@subsubsection BinderThreadingSThread SThread

SThread is a more traditional threading class.  It is basically an object wrapper around
a traditional kernel thread.  After creating an SThread instance and calling SThread::Run() on it,
the class will make a new dedicated thread for it.  You must implement a subclass of
SThread that implements SThread::ThreadEntry() to provide the code its thread should run.

You should rarely use SThread, preferring instead the much lighter-weight SHandler.  The
main place where SThread is used is for creating threads that will do blocking IO
operations &mdash; for example the threads that read events from the keyboard and pen devices.
In this case it is better to have a dedicated thread blocked in the IO call, rather then
pulling a thread out of the thread pool only to have it sit blocked for an arbitrary
amount of time.

@subsection BinderThreadingSynchronization Synchronization

The two main classes the Binder provides for synchronization are SLocker (a mutex) and
SConditionVariable.  These are built on top of the kernel's critical section and
condition variable primitives, respectively, and inherit all of their features: a kernel
call not required when no blocking happens, they very light-weight (4 bytes per instance),
etc.

The class you will use the most, usually to protect your own class's state variables, is SLocker.
The system APIs are designed to not enforce any particular locking regimen, except that
in most cases you will be multithreaded and so need to use locks to protect yourself.
Thus the approach you want to take to locking is mostly up to you, and can be as simple
as a big global lock.

One of the main difficulties of multithreaded programming is the creation of deadlocks, and
this is something you must be aware of whenever using these synchronization primitives.  You
can read http://en.wikipedia.org/wiki/Deadlock to learn more about deadlocks if you
don't already understand what they are.

To assist in dealing with deadlocks, the SLocker class includes a "potention deadlock"
detection mechanism, described more in the @ref SLockerSummary "SLocker summary"
text.

@section BinderThreadingSystemPolicies System Locking Policies

In order for code to be thread safe, it must acquire locks while manipulating its
state or performing calls on other objects that must be correctly ordered.
This must be done in a way that ensures multiple threads will not
cause destructive interference with each other (modifying the same program state
in a way that interfers with another thread, a.k.a. a race condition) while at the
same time making sure that the way it uses locks to protect itself will not lead to
deadlocks.

In general the way to avoid race conditions are simple to understand: just don't let
two threads touch the same state at the same time.

Deadlocks, however, are more complicated, because they usually arise from unexpected
interactions across independent system components.  Thus we need to set some policies and
techniques in place to avoid them.  This section describes a lot of the techniques
we use inside of the Binder to deal with locking.  One of the goals of our approach
is to allow clients to use whatever locking policy of their own that they would like.
Thus you can ignore this if you are writing application-level code that others won't
use; however, system-level code that will be supplied to other developers should
try to use the locking approach described here.

In the specific case we are concerned with here, a deadlock happens when different
threads are acquiring @e multiple locks (usually our SLocker class) in ways that can
result in deadlocks.  In general the best way to avoid this kind of deadlock is to
carefully control the order of these nested locks: that is, if some thread acquires lock A and
while holding that then acquires lock B, if no other thread will first acquire B and then
while holding B acquire A, then we are guaranteed there will be no deadlock between those
two specific locks.

So to ensure we have no deadlocks in our system, we must ensure that there is no possible
combination of any locks in the system that will be acquired in different orders.  For a
system as complicated as ours, it is impossible to go through and analyize all of the
combination of locks and understand their acquisition order.  Instead, we define a set of
locking policies for when you can hold locks that, if followed, will result in a system
with no deadlocks.

These locking policies are also intended to allow, as much as possible, flexibility for
clients of our APIs to use whatever locking policy of their own that may be most
convenient.

@subsection BinderThreadingFineGrained Fine-Grained Locking

The most common locking policy we have in the system is something called fine-grained
locking.  What this means is that a particular lock generally protects only a few pieces of
information, the goal being that the implementation only needs to hold the lock for brief
periods of time and thus has a clear idea of the scope of that lock.

This policy is enabled by the SLocker class (or in fact the critical section kernel
primitive it uses), which allows us to have extremely lightweight mutex locks: only 4
bytes per lock.  In practice each time you write a new class, you will include a new
SLocker in it to protect your new state.  (Resulting in multiple SLockers per object,
matching the depth of its inheritance hierarchy.)  For example, consider someone
implementing this interface:

@code
interface IPerson
{
properties:
	SString name;
	IPerson bestFriend;
	[readonly]int32_t age;
	[readonly]float height;

methods:
	DoBirthday();
}
@endcode

A class implementating the interface could protect its state with a lock like this:

@code
class Person : public BnPerson
{
public:
	SString Name() const
	{
		SLocker::Autolock _l(m_lock);
		return m_name;
	}

	void SetName(const SString& value)
	{
		SLocker::Autolock _l(m_lock);
		m_name = value;
	}

	sptr<IPerson> BestFriend() const
	{
		SLocker::Autolock _l(m_lock);
		return m_bestFriend.promote();
	}

	void SetBestFriend(const sptr<IPerson>& bestFriend)
	{
		SLocker::Autolock _l(m_lock);
		m_bestFriend = bestFriend;
	}

	int32_t Age() const
	{
		SLocker::Autolock _l(m_lock);
		return m_age;
	}

	float Height() const
	{
		SLocker::Autolock _l(m_lock);
		return m_height;
	}

	void DoBirthday()
	{
		SLocker::Autolock _l(m_lock);
		m_age++;
		if (m_age < 16) m_height += 10;
	}


private:
	mutable SLocker m_lock;

	SString m_name;
	wptr<IPerson> m_bestFriend;
	int32_t m_age;
	float m_height;
};
@endcode

This implementation demonstrates a lot of very common patterns we use with
the Binder to deal with threading issues.

@subsubsection BinderThreadingBasicLocking Protect Critical Sections With Locks

The implementation of DoBirthday() represents a fairly traditional
multithreading technique, where one needs to atomically modify two variables.
To make sure this is thread safe, we hold our lock while executing the code,
to ensure that only one thread at a time can be accessing those variables.

Also note the use of the lock when modifying and retrieving m_name &mdash; in general
the implementation of classes that begin with the 'S' prefix are @e not thread
safe (they do not have their own lock), so you must explicitly protect them
yourself if you are using them in a Binder object or other multithreaded situation.

@note The example code here does not need to acquire m_lock in its Age() and Height()
methods.  This is because int32_t and float are 4-byte types, which on ARM can be read
from memory atomically.  This is a useful optimization to know, but in general it is best
to be safe and acquire your lock whenever in doubt.

@subsubsection BinderThreadingCopyTypes Copy Types

The Name() method returns the current name of the object.  In order to avoid
exposing our lock to the public, we return a @e copy of the name.  That way we
only need to hold our internal lock when copying the string; once we have a
copy, we know there is only one thread (our own) with access to the copy, so
we can safely return it to the caller without our lock held.

This locking technique requires that it be relatively cheap to make copies of
type classes.  For simple types (such as SPoint, SRect, or SColor32) that is
naturally the case; however, more often a type object may have a significant
amount of data associated with it so making a copy can become a fairly
heavy-weight operation.

Any type class in the Binder system that does not naturally have a cheap copy
operation will use some form of "copy-on-write" semantics to provide a cheap
copy.  Examples of classes that do this are SString, SValue, and SGlyphMap.
You can easily implement your own copy-on-write class using the SSharedBuffer
utility object.

@subsubsection BinderThreadingReferenceCounting Use Reference Counting

The BestFriend() method gives us another approach to dealing with return
values.  Here we explicitly want to return the object itself, and not
just a copy.  For this kind of situation, we use reference counting on
the object.

Any class that derives from SAtom (including all classes
with the 'I' and 'B' prefix) will be reference counted.  The sptr and wptr
templates work like plain C++ pointers, but automatically hold a reference
on the object for us.  Since sptr and wptr don't have their own lock, we
need to protect access to them like we did with the SString above.

In this case, however, the 'type' sptr is simply holding a reference count
on some other object.  Once we make a copy of that type and thus have our
own sptr on the same object (here obtained with the wptr::promote() call),
we can safely return that to the caller without holding our lock.

One other thing that is worth noting here is the use of a wptr member variable
to hold the bestFriend property.  A wptr holds a weak reference, meaning
the target object is allowed to be destroyed even while there are still
such references on it.  This is done so that, for example, if two IPerson
objects were each their own best friends, we would not create a cycle in the
reference counts &mdash; if nobody else is holding on to one of those objects,
we allow it to be destroyed even if the other has a bestFriend pointer to
it.

@subsection BinderThreadingRelease Release Locks When Calling Out

Never hold a lock when calling into user code.  This rule protects against many
protential deadlocks.

Here is an example that comes up frequently: it okay to hold a lock when calling AddView(),
RemoveView(), etc.  This is because the implementation of those methods only holds the
view's lock while manipulating some internal state.  Most of the work of actually adding
and removing the view &mdash; such as calling its Layout() method and having it calculate
its constraints and draw &mdash; is performed asynchronously from the AddView() call.  This
is a powerful technique to avoid the need to hold locks when calling other code,
which is described in more detail in @ref SHandlerPatterns.

The key to this is knowing where your code sits in the layering of the system.  Another
way of phrasing the rule is "never hold a lock when calling in to higher-level code."
Here are some examples of how the system is layered:

 - Kits/libraries are layered on top of each other.  For example, the View Kit sits on
 top of the Render Kit.  This means that it is always okay for code in the View Kit to
 call any Render Kit API while it is holding one of its locks.  Not allowing a
 lower-level kit to see the APIs of a higher-level kit is an easy way to enforce
 this rule.

 - Subclasses are layered on top of their base classes.  For example, when MyView derives
 from BView, MyView is higher-level code than BView.  This means it is okay for MyView to
 call BView APIs with locks held, while the code in BView must never be holding a lock
 when it calls a virtual that a derived class can override.  (There is a contradiction
 here, that we'll talk about later.)

 - Child views are layered on top of their parents.  This means the child is free to call
 any APIs on IViewParent while holding its locks, but a parent must not make calls to its
 children's IView API while it holds any of its own locks.

 - Clients are layered on top of services.  Thus it is okay for clients to hold locks
 while calling the service, but the service must not be holding a lock when calling back
 to client code.

@subsection BinderThreadingSubclass Subclass Complications

Recall that we mentioned there being a contradiction in the definition of when it is okay
for subclasses to hold locks.  This is because there are really two kinds of subclasses:

 - <em>Leaf classes</em> are generally part of an application and not a shared library.
 No (unknown) classes will further derive from them, so they get to define the final
 threading semantics to be whatever they want.  If they want to have one big lock that is
 held whenever their code in the view is doing stuff (including calling virtuals), that
 is okay.

 - <em>System classes</em> are generally included in a shared library, and as such need
 to allow any arbitrary developer to create a subclass of them.  In this case calling
 virtual function must be considered the case of calling into user code, because a
 deriving class can override any of those functions.  If at all possible, you must avoid
 holding locks while calling virtuals; if you can't avoid it, this absolutely must be
 documented in class's API.

In other words, top-level clients are still allowed to use whatever locking approach
they want, as we mentioned earlier.  However, if you are writing a class that other
people will be using (one that is part of a system shared library), then you should
follow the rule and not hold locks when calling virtuals.

@subsection BinderThreadingView More on threading and the view hierarchy

First, re-read the section @ref BinderThreadingRelease.  The view hierarchy
goes to great lengths to follow these rules.  When you read comments like "we can't call
IView::TakeFocus() with the lock held", this is exactly because of the rules given above.
(In this case, TakeFocus() is a call down to children views, which you cannot do with
the lock held.  However, you can make calls up to your parent view with your lock held.)
Generally the view hierarchy is very much about either going up or going down, but not
doing both at the same time.  When going up or going down the locking rules must be
followed.

@section BinderThreadingDesigning Designing For Thread Safety

@subsection BinderThreadingAPISafety API Thread Safety

@todo Talk about examples such as IViewManager and SVector, byte streams
and IDatum::Open() and IStorage, IIterator and IIterable::NewIterator().
*/
