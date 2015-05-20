/*!
@page SAtomDebugging SAtom Debugging

<div class="header">
<center>\< @ref SupportKit | @ref SupportKit | @ref SHandlerPatterns \></center>
<hr>
</div>

The Atom Debug facilities in Palm OS can be extremely useful when trying
to track down reference count leaks.  Generally a normal malloc leak
tracking tool is useless for these, because the code that originally
allocated the object no longer owns it &mdash; instead there can be any number
of other references on it that are causing it to leak.

Using Atom Debug, you can find out about what objects are leaking and
get information about who in the system is holding references on those
objects.  This tool works for both SAtom and SLightAtom objects.  It
is currently only useful in the Simulator, since it relies on being
able to get stack crawls.


-# @ref GettingStarting
-# @ref FindingLeaks
  -# @ref Processes
  -# @ref DebuggingSteps
  -# @ref DebuggingALeak
-# @ref ExaminingObjects

@section GettingStarting Getting Started

To debug SAtom leaks, you will first need to enable the debugging
facility before running the system.

Before running a process, you will need to set the ATOM_DEBUG and
ATOM_REPORT environment variables as desired.

ATOM_DEBUG is used to control the amount of debugging that is
enabled, and can be one of:

- "5" does simple validation of SAtom reference counts (checking
  for releasing more times and than acquires for example) and
  a count of the total number of objects created and destroyed
  in a process.
- "10" enables the full SAtom debugging infrastructure.  This is
  what you will normally use.

ATOM_REPORT is used to control how much information is collected
and reported about references held on an SAtom.  The two values
you will want to use are:

- "5" collects stack crawls, but only displays their addresses.
- "10" displays full symbolic information (function names) for
  each line in a stack crawl when it is printed.

Note that the ATOM_DEBUG variable is examined by each process
as it starts up, so you will need to set it for any processes
to be debugged.  In particular, if leaks are happening in the
system process, you will need to set this variable when starting
smooved.

When using atom debugging, you will probably want to turn off
lock debugging by setting LOCK_DEBUG=0.  Both of these facilities
are quite heavy-weight, so together they can slow down the
system significantly.

Your interaction with the atom debugger will often be through the
"atom" Binder Shell command, though there are also APIs on SAtom that
you work with the debugging facilities programmatically.  (In fact,
the atom command is just a tool to call these APIs from the shell.)
You might need to add atom.prc to your extra packages if it is not
in the default build.

@section FindingLeaks Finding Leaks

Usually when debugging atom leaks you will use the atom command's
"mark" and "leaks" options.  The idea here is that when an atom
object is allocated, it is placed in to a bin.  An "atom mark"
increments the bin number, returning the new bin.  The "atom leaks"
command gives you a list of existing atom object &mdash; including stack
crawls of all references on them &mdash; which can be filtered by bin.

@subsection Processes

The atom "mark" and "leaks" commands require that you specify the
process they are to operate on.  In a debug build, these processes are
easily available from any Binder Shell session:

@verbatim
/$ su

/# ls /processes
background
system
top_ui
ui
@endverbatim

You can also use the Binder Shell environment variable $PROCESS to
access the process that the shell is running in.

@subsection DebuggingSteps Debugging Steps

Here is a sketch of how a typical atom debugging session will go, assuming
we are debugging a leak in the background process.

@subsubsection FlowStep1 1. Start the process to be debugged.

Debugging output will go to whatever terminal the process was started in.

@subsubsection FlowStep2 2. Start a debug terminal.

Use whatever approach you want to start a Binder Shell session.

@subsubsection FlowStep3 3. Perform the operation that is leaking

For example, opening and then closing a slip.  You do it here to get
the code to load in any data that it caches, so that won't get mixed
in with the real leak.

@subsubsection FlowStep4 4. Mark the current state

@verbatim
$ atom mark /processes/background
Result: int32_t(2 or 0x2)
@endverbatim

@subsubsection FlowStep5 5. Perform the operation that is leaking

All objects created will go into bin that you just marked.

@subsubsection FlowStep6 6. Mark the next state

@verbatim
$ atom mark /processes/background
Result: int32_t(3 or 0x3)
@endverbatim

@subsubsection FlowStep7 7. Perform the operation that is leaking

You'll want to do it one more time, to make sure your code has had
a chance to release all of the references it is going to.

@subsubsection FlowStep8 8. Get a leak report for the first mark

@verbatim
$ atom leaks /processes/background 2 2
@endverbatim

This will print out all of the SAtom and SLightAtom objects that were
created in the first mark (whose returned ID was 2) and that still exist.

@subsection DebuggingALeak Debugging a Leak

Let's simulate a leak, with the following code.  Pretend that the
shell code to instantiate a component is actually some code that is
happening when we do something with the UI that is leaking.

@verbatim
### Perform our leaking operation for the first time.
$ C=$[new -r /processes/background org.openbinder.services.StatusBar.ClockSlipView]
Result: IBinder::shnd(0x132)
$ invoke /services/window AddView $C
Result: int32_t(0 or 0x0)

### Start a new SAtom bin.
$ atom mark /processes/background
Result: int32_t(2 or 0x2)

### Perform our leaking operation for the second time.
### (Everything done here are the objects for which we will be tracking leaks.)
$ C=$[new -r /processes/background org.openbinder.services.StatusBar.ClockSlipView]
Result: IBinder::shnd(0x132)
$ invoke /services/window AddView $C
Result: int32_t(0 or 0x0)

### Start a new SAtom bin; all operations between this and the
### last mark were placed in bin 2, and at this point nothing else
### will go in that bin.
$ atom mark /processes/background
Result: int32_t(3 or 0x3)

### Perform our leaking operations for the last time.
### (Flush out any temporary references held from the previous run.)
$ C=$[new -r /processes/background org.openbinder.services.StatusBar.ClockSlipView]
Result: IBinder::shnd(0x100)
$ invoke /services/window AddView $C
Result: int32_t(0 or 0x0)

### Print all of the leaks objects.
$ atom leaks /processes/background 2 2
@endverbatim

Upon executing the last command, we will get a dump of the leaked objects.
The first thing you will notice is that there are a lot more objects listed
here than just the one we actually leaked!  Here is the list with most of
the details removed:

@verbatim
Active atoms from mark 2 to 2:
	Report for atom 0x6b156d90 (class BpBinder) at mark #2:
	...
	Report for atom 0x6b14ff70 (class BpViewParent) at mark #2:
	...
	Report for atom 0x6b1502f0 (class ClockSlipView) at mark #2:
	...
	Report for atom 0x6b14fc48 (class BpCatalog) at mark #2:
	...
Result: int32_t(0 or 0x0)
@endverbatim

Usually, because of references between objects, when one object
is leaked a whole host of related objects will be leaked as well.
Your main task in debugging leaks is dealing with these dependencies:
finding the one reference amongst these that actually caused the leak.

In our example here, we know that the second object listed (class
ClockSlipView) is the one being leaked because we created it.
But how would we otherwise focus on that particular object?

Let's look at all of the details of the last object in the list, line
by line:

@verbatim
	Report for atom 0x6b14fc48 (class BpCatalog) at mark #2:
@endverbatim

This tells you that an SAtom object at address 0x6b10ce48 has been
leaked.  C++ RTTI information says it is "class BpBinder", and it
was allocated in mark bin 2.

@verbatim
	2 IncStrong Remain:
@endverbatim

There are two strong pointers on this object that are keeping it
from being destroyed.

@verbatim
		0x6b150194 107 4043971000000us --
		0x6b150174 107 4043970000000us --
@endverbatim

These are the strong pointers.  The first three numbers tell us,
respectively, the owner ID (the void* passed to IncStrong() that
identifies the owner of the reference), the thread ID that acquired
the reference, and the time it was acquired.  The remaining numbers
(not shown here) are the stack crawl when the reference of the acquired,
starting at the leaf.

@verbatim
	2 IncWeak Remain:
		0x6b150194 107 4043971000000us --
		0x6b150174 107 4043970000000us --
@endverbatim

There are 2 weak pointers on the object.  Weak pointers generally
don't keep an object from being destroyed, so for the purpose of
debugging a leak you can generally ignore these.  They give the same
information about each reference as the strong pointers.  Note that
you will always have at least the same number of weak pointers as
strong pointers, since acquiring a strong pointer also implicitly
acquires a weak pointer.

@verbatim
	DecStrong:
	DecWeak:
@endverbatim

These tell you about any strong or weak pointer decrements that do
not have a matching increment.  They should generally be empty. 
The atom debugging code keeps track of references by their owner ID
&mdash; when a reference is removed, it looks for the given owner ID in
the list of current references and, if found, removes that reference
from the object.  If the owner ID isn't found, then the decrement
operation will show up in one of the lists here.

So is this object the one that is being leaked?  You generally figure
this out by looking at the stack crawls of its strong references, and
determining if any of the other objects in the list are holding that
reference.  If they are all held by other leaked objects, then you can
assume this one is being leaked as a side-effect of the real leak.

If you are running Windows XP, you can quickly eliminate many of the
objects by just looking at the stack crawl.  If you aren't that
fortunate &mdash; or need to look more in-depth at a particular stack crawl,
you can run PalmSim in the debugger and, after pausing it, set a
breakpoint at the address in the stack crawl to find out exactly where
that is.  (The console is still active even when the Simulator is
stopped in the debugger.)

Here is the full first stack crawl:

@verbatim
SAtom::add_incstrong(const void *, long)
SAtom::IncStrong(const void *)
sptr<ICatalog>::sptr<ICatalog>(const sptr<ICatalog> &)
SContext::SContext(const SContext &)
BBinder::BBinder(const SContext &)
BnView::BnView(const SContext &)
BView::BView(const SContext &, const SValue &)
ClockSlipView::ClockSlipView(const SContext &, const SValue &)
@endverbatim

And there is no need to go any further, because we see ClockSlipView
involved in this leak and that it is itself being leaked, so we know
where this reference came from.

Likewise the second reference on the BpCatalog is this:

@verbatim
SAtom::add_incstrong(const void *, long)
SAtom::IncStrong(const void *)
sptr<ICatalog>::sptr<ICatalog>(const sptr<ICatalog> &)
SContext::SContext(const SContext &)
BBinder::BBinder(const SContext &)
BnStatusBarSlip::BnStatusBarSlip(const SContext &)
BStatusBarSlip::BStatusBarSlip(const SContext &, const SValue &)
ClockSlipView::ClockSlipView(const SContext &, const SValue &)
@endverbatim

And thus it is not the leak we are looking for.

We can also immediately discount the first two objects, since there are no
strong pointers to them:

@verbatim
	Report for atom 0x6b156d90 (class BpBinder) at mark #2:
	0 IncStrong Remain:
	1 IncWeak Remain:
		0x6b14ff5c 109 4056701000000us --
	DecStrong:
	DecWeak:
	Report for atom 0x6b14ff70 (class BpViewParent) at mark #2:
	0 IncStrong Remain:
	2 IncWeak Remain:
		0x6b112b98 109 4056702000000us --
		0x6b15020c 109 4056702000000us --
@endverbatim

That brings us to our culprit, "class ClockSlipView".  But why did it
leak?  Let's look at its references.

@verbatim
	Report for atom 0x6b1502f0 (class ClockSlipView) at mark #2:
	2 IncStrong Remain:
		0x00000003 107 4043980000000us --
		0x6b1017cc 94 4043976000000us --
@endverbatim

The first reference is:

@verbatim
SAtom::add_incstrong(const void *, long)
SAtom::IncStrong(const void *)
BProcess::_IncrementReferenceCounts(unsigned int, bool, bool)
BProcess::PackBinderArgs(unsigned short, unsigned long, const SParcel &, KALBinderIPCArgsType *)
SLooper::_HandleResponse(KALBinderIPCArgsType *, unsigned char *, SParcel *, bool)
SLooper::_LoopTransactionSelf(void)
SLooper::_EnterTransactionLoop(BProcess *)
SysThreadWrapper(long)
@endverbatim

The second reference is:

@verbatim
SAtom::add_incstrong(const void *, long)
SAtom::attempt_inc_strong(const void *, bool)
SAtom::AttemptIncStrong(const void *)
SAtom::AttemptAcquire(const void *)
BProcess::DispatchMessage(SLooper *)
SLooper::_HandleResponse(KALBinderIPCArgsType *, unsigned char *, SParcel *, bool)
SLooper::_LoopTransactionSelf(void)
SLooper::_EnterTransactionLoop(BProcess *)
SysThreadWrapper(long)
@endverbatim

The second stack crawl can probably be ignored &mdash; what we are seeing is a
message in the process of being dispatched to the ClockSlipView (it includes
a BHandler).  Messages are transient, and you can rely on that reference being
removed once the message processing is complete.

The first reference, however, gives us a complication: it is coming from another
process.  In fact what we are seeing here is the reference the window manager
(in the system process) holds on the ClockSlipView (in the background process).
If this doesn't give you enough information to figure out the leak, you will
need to broaden your scope and look for leaks in other processes.  In fact,
if we were to do this same thing in the system process, we would see a leak of
the BpView proxy for our ClockSlipView, the window manager's WindowDecor view,
and various other objects.

@section ExaminingObjects Examining Objects

You can use the atom command's find/report to get a report on a specific object.
(If you use the mark option, <tt>$PROCESS</tt> is your current process.)

For example:
@verbatim
atom report @{$[atom find "class CountryList"][0] }
@endverbatim

With symbols turned on, this gives you something like this:
@verbatim
Report for atom 0x4e7b4f08 (class CountryList) at mark #1:
1 IncStrong Remain:
	ID: 0x4e91c4fc, Thread: 91, When:128496000000
		0x50e7c651: <SAtom::add_incstrong>+0x00000021
		0x50e7b08f: <SAtom::IncStrong>+0x000000ff
		0x55d7dac9: <sptr<IActivity>::operator=>+0x00000039
		0x55d985ed: <BActivityContainer::BActivityContainer>+0x0000016d
		0x55d98871: <BMainActivityContainer::BMainActivityContainer>+0x000000b1
		0x55d917c5: <BInteractiveActivity::CreateContainer>+0x000000a5
		0x55d90f3f: <BInteractiveActivity::GenerateView>+0x0000001f
		0x55d95454: <BVisualActivity::OnStart>+0x00000024
		0x50e632b9: <BActivity::HandleMessage>+0x00000089
		0x55d91716: <BInteractiveActivity::HandleMessage>+0x000000d6
		0x50e9860c: <BHandler::DispatchMessage>+0x0000006c
		0x50ebb39b: <BProcess::DispatchMessage>+0x0000014b
		0x50eaba93: <SLooper::_HandleResponse>+0x00000333
		0x50eab542: <SLooper::_LoopTransactionSelf>+0x00000062
		0x50eaae00: <SLooper::_EnterTransactionLoop>+0x00000050
		0x00fb8f37: <SysThreadWrapper>+0x00000067
		0x00458a87: <kHAL_ThreadWrapper_continuation>+0x00000087
		0x00458631: <kHAL_ThreadWrapper_switch_stack>+0x00000011
		0x004588b8: <kHAL_ThreadWrapper>+0x00000108
		0x0045faa6: <_threadstartex>+0x000000b6
		0x77e7d28e: <RegisterWaitForInputIdle>+0x00000043
@endverbatim

*/
