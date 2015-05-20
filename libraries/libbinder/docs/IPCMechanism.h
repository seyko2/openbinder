/*!
@page BinderIPCMechanism Binder IPC Mechanism

<div class="header">
<center>\< @ref BinderProcessModel | @ref BinderKit | @ref pidgen \></center>
<hr>
</div>

The Binder communicates between processes using a small custom
kernel module.  This is used instead of standard Linux IPC
facilities so that we can efficiently model our IPC operations
as "thread migration".  That is, an IPC between processes looks
as if the thread instigating the IPC has hopped over to the
destination process to execute the code there, and then hopped
back with the result.

The Binder IPC mechanism itself, however, is not actually implemented
using thread migration.  Instead, the Binder's user-space code
maintains a pool of available threads in each process, which are
used to process incoming IPCs and execute local events in that process.
The kernel module emulates a thread migration model
by propagating thread priorities across processes as IPCs are
dispatched and ensuring that, if an IPC recurses back into an
originating process, the IPC is handled by its originating thread.

In addition to IPC itself, the Binder's kernel module is also
resposible for tracking object references across processes.  This
involves mapping from remote object references in one process to
the real object in its host process, and making sure that objects
are not destroyed as long as other processes hold references on them.

The rest of this document will describe in detail how Binder IPC
works.  These details are not exposed to application developers,
so they can be safely ignored.

@section GettingStarted Getting Started

When a user-space thread wants to participate in Binder IPC (either
to send an IPC to another process or to receiving an incoming IPC),
the first thing it must do is open the driver supplied by the Binder
kernel module.  This associates a file descriptor with that thread,
which the kernel module uses to identify the initiators and recipients
of Binder IPCs.

It is through this file descriptor that all
interaction with the IPC mechanism will happen, through a small set
of ioctl() commands.  The main commands are:

- @b BINDER_WRITE_READ sends zero or more Binder operations, then
  blocks waiting to receive incoming operations and return with a result.
  (This is the same as doing a normal write() followed by a read()
  on the file descriptor, just a little more efficient.)
- @b BINDER_SET_WAKEUP_TIME sets the time at which the next user-space
  event is scheduled to happen in the calling process.
- @b BINDER_SET_IDLE_TIMEOUT sets the time threads will remain idle
  (waiting for a new incoming transaction) before they time out.
- @b BINDER_SET_REPLY_TIMEOUT sets the time threads will block waiting
  for a reply until they time out. 
- @b BINDER_SET_MAX_THREADS sets the maximum number of threads that the
  driver is allowed to create for that process's thread pool.

The key command is BINDER_WRITE_READ, which is the basis for all IPC
operations.  Before going into detail about that, however, it should be
mentioned that the driver expects the user code to maintain a pool of
threads waiting for incoming transactions.  You need to ensure that
there is always a thread available (up to the maximum number of threads
you would like) so that IPCs can be processed.  The driver also takes
care of waking up threads in the pool when it is time to process new
asynchronous events (from SHandler) in the local process.

@subsection BINDER_WRITE_READ

As mentioned, the core functionality of the driver is encapsulated in
the BINDER_WRITE_READ operation.  The ioctl's data is this structure:

@code
struct binder_write_read
{
	ssize_t		write_size;
	const void*	write_buffer;
	ssize_t		read_size;
	void*		read_buffer;
};
@endcode

Upon calling the driver, @e write_buffer contains a series of commands
for it to perform, and upon return @e read_buffer is filled in with a
series of responses for the thread to execute.  In general the write
buffer will consist of zero or more book-keeping commands (usually
incrementing/decrementing object references) and ending with a command
requiring a response (such as sending an IPC transaction or attempt to
acquire a strong reference on a remote object).  Likewise, the receive
buffer will be filled with a series of book-keeping commands and end
with either the result for the last written command, or a command to
perform a new nested operation.

Here is a list of the commands that can be sent by a process to the
driver, with comments describing the data that follows each command in
the buffer:

@code
enum BinderDriverCommandProtocol {
	bcNOOP = 0,
	/*	No parameters! */

	bcTRANSACTION,
	bcREPLY,
	/*
		binder_transaction_data: the sent command.
	*/
	
	bcACQUIRE_RESULT,
	/*
		int32:  0 if the last brATTEMPT_ACQUIRE was not successful.
		Else you have acquired a primary reference on the object.
	*/
	
	bcFREE_BUFFER,
	/*
		void *: ptr to transaction data received on a read
	*/
	
	bcINCREFS,
	bcACQUIRE,
	bcRELEASE,
	bcDECREFS,
	/*
		int32:	descriptor
	*/
	
	bcATTEMPT_ACQUIRE,
	/*
		int32:	priority
		int32:	descriptor
	*/
	
	bcRESUME_THREAD,
	/*
		int32:	thread ID
	*/
	
	bcSET_THREAD_ENTRY,
	/*
		void *:	thread entry function for new threads created to handle tasks
		void *: argument passed to those threads
	*/
	
	bcREGISTER_LOOPER,
	/*
		No parameters.
		Register a spawned looper thread with the device.  This must be
		called by the function that is supplied in bcSET_THREAD_ENTRY as
		part of its initialization with the binder.
	*/
	
	bcENTER_LOOPER,
	bcEXIT_LOOPER,
	/*
		No parameters.
		These two commands are sent as an application-level thread
		enters and exits the binder loop, respectively.  They are
		used so the binder can have an accurate count of the number
		of looping threads it has available.
	*/
	
	bcCATCH_ROOT_OBJECTS,
	/*
		No parameters.
		Call this to have your team start catching root objects
		published by other teams that are spawned outside of the binder.
		When this happens, you will receive a brTRANSACTION with the
		tfRootObject flag set.  (Note that this is distinct from receiving
		normal root objects, which are a brREPLY.)
	*/
	
	bcKILL_TEAM
	/*
		No parameters.
		Simulate death of a kernel team.  For debugging only.
	*/
};
@endcode

The most interesting commands here are bcTRANSACTION and bcREPLY, which
initiate an IPC transaction and return a reply for a transaction,
respectively.  The data structure following these commands is:

@code
enum transaction_flags {
	tfInline = 0x01,			// not yet implemented
	tfRootObject = 0x04,		// contents are the component's root object
	tfStatusCode = 0x08			// contents are a 32-bit status code
};

struct binder_transaction_data
{
	// The first two are only used for bcTRANSACTION and brTRANSACTION,
	// identifying the target and contents of the transaction.
	union {
		size_t	handle;		// target descriptor of command transaction
		void	*ptr;		// target descriptor of return transaction
	} target;
	uint32	code;			// transaction command
	
	// General information about the transaction.
	uint32	flags;
	int32	priority;		// requested/current thread priority
	size_t	data_size;		// number of bytes of data
	size_t	offsets_size;	// number of bytes of object offsets
	
	// If this transaction is inline, the data immediately
	// follows here; otherwise, it ends with a pointer to
	// the data buffer.
	union {
		struct {
			const void	*buffer;	// transaction data
			const void	*offsets;	// binder object offsets
		} ptr;
		uint8	buf[8];
	} data;
};
@endcode

Thus, to initiate an IPC transaction, you will essentially perform a
BINDER_READ_WRITE ioctl with the write buffer containing bcTRANSACTION
follewed by a binder_transaction_data.  In this structure @e target is
the handle of the object that should receive the transaction (we'll talk
about handles later), @e code tells the object what to do when it
receives the transaction, @e priority is the thread priority to run the
IPC at, and there is a @e data buffer containing the transaction data,
as well as an (optional) additional @e offsets buffer of meta-data.

Given the @e target handle, the driver determines which process that
object lives in and dispatches this transaction to one of the waiting
threads in its thread pool (spawning a new thread if needed).  That
thread is waiting in a BINDER_WRITE_READ ioctl() to the driver, and so
returns with its read buffer filled in with the commands it needs to
execute.  These commands a very similar to the write commands, for the
most part corresponding to write operations on the other side:

@code
enum BinderDriverReturnProtocol {
	brERROR = -1,
	/*
		int32: error code
	*/
	
	brOK = 0,
	brTIMEOUT,
	brWAKEUP,
	/*	No parameters! */
	
	brTRANSACTION,
	brREPLY,
	/*
		binder_transaction_data: the received command.
	*/
	
	brACQUIRE_RESULT,
	/*
		int32: 0 if the last bcATTEMPT_ACQUIRE was not successful.
		Else the remote object has acquired a primary reference.
	*/
	
	brDEAD_REPLY,
	/*
		The target of the last transaction (either a bcTRANSACTION or
		a bcATTEMPT_ACQUIRE) is no longer with us.  No parameters.
	*/
	
	brTRANSACTION_COMPLETE,
	/*
		No parameters... always refers to the last transaction requested
		(including replies).  Note that this will be sent even for asynchronous
		transactions.
	*/
	
	brINCREFS,
	brACQUIRE,
	brRELEASE,
	brDECREFS,
	/*
		void *:	ptr to binder
	*/
	
	brATTEMPT_ACQUIRE,
	/*
		int32:	priority
		void *: ptr to binder
	*/
	
	brEVENT_OCCURRED,
	/*
		This is returned when the bcSET_NEXT_EVENT_TIME has elapsed.
		At this point the next event time is set to B_INFINITE_TIMEOUT,
		so you must send another bcSET_NEXT_EVENT_TIME command if you
		have another event pending.
	*/
	
	brFINISHED
};
@endcode

Continuing our example, the receiving thread will come back with a
brTRANSACTION command at the end of its buffer.  This command uses the
same binder_transaction_data structure that was used to send the data,
basically containing the same information that was sent but now available
in the local process space.

The recipient, in user space will then hand this transaction over to the
target object for it to execute and return its result.  Upon getting the
result, a new write buffer is created containing the bcREPLY reply
command with a binder_transaction_data structure containing the resulting
data.  This is returned with a BINDER_WRITE_READ ioctl() on the driver,
sending the reply back to the original process and leaving the thread
waiting for the next transaction to perform.

The original thread finally returns back from its own BINDER_WRITE_READ
with a brREPLY command containing the reply data.

Note that the original thread may also receive brTRANSACTION commands while
it is waiting for a reply.  This represents a recursion across processes &mdash;
the receiving thread making a call on to an object back in the original
process.  It is the responsibility of the driver to keep track of all
active transactions, so it can dispatch transactions to the correct thread
when recursion happens.

@subsection ObjectMappingAndReferencing Object Mapping and Referencing

One of the important responsibilities of the driver is to perform mapping
of objects from one process to another.  This is key to both the
communication mechanism (targetting and referencing objects) as well as
the capability model (only allowing a particular process to perform
operations on remote objects that it has been explicitly given knowledge
about).

There are two distinct forms of an object reference: as an address in a
processes's memory space, or as an abstract 32-bit handle.  These
representations are mutually exclusive: @b all references in a process
to objects local to that process are in the form of an address, while
all references to objects in another process are always in the form of
a handle.

For example, note the @e target field of binder_transaction_data.  When
sending a transaction, this contains a handle to the destination object
(because you always send transactions to other processes).  The recipient
of the transaction, however, sees this as a point to the object in its
local address space.  The driver maintains mappings of pointers and
handles between processes so that it can perform this translation.

We also must be able to send references to objects through transactions.
This is done by placing the object reference (either a local pointer or
remote handle) in to the transaction buffer.  The driver must translate
this reference to the corresponding reference in the receiving process,
however, just like we do with the transaction target.

In order to do reference translation, the driver needs to know where these
references appear in the transaction data.  This is where the additional
@e offsets buffer comes in.  It contains of a series of indicies into the
data buffer, describing where objects appear.  The driver can then rewrite
the buffer data, translating each of those objects from the sending
process reference to the correct reference in the receiving process.

Note that the driver does not know anything about a particular Binder
object until that object is sent through the driver to another process.
At that point the driver adds the object's address to its mapping table
and asks the owning process to hold a reference on it.  When no other
processes know about the object, it is removed from the mapping table
and its owning process is told to release the driver's reference.  This
avoids maintaining the (relatively significant) driver state for an
object as long as it is only used in its local process.
*/
