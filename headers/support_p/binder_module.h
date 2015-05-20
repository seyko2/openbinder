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

#ifndef _BINDER_MODULE_H_
#define _BINDER_MODULE_H_

#include <support/TypeConstants.h>

#if TARGET_HOST == TARGET_HOST_PALMOS
#include <linux/types.h>
#include <asm/ioctl.h>
#endif

#ifdef __cplusplus
#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif
#endif

// These are pre-packed type constants for the object type codes.
enum {
	kPackedLargeBinderType = B_PACK_LARGE_TYPE(B_BINDER_TYPE),
	kPackedLargeBinderWeakType = B_PACK_LARGE_TYPE(B_BINDER_WEAK_TYPE),
	kPackedLargeBinderHandleType = B_PACK_LARGE_TYPE(B_BINDER_HANDLE_TYPE),
	kPackedLargeBinderWeakHandleType = B_PACK_LARGE_TYPE(B_BINDER_WEAK_HANDLE_TYPE),
	kPackedLargeBinderNodeType = B_PACK_LARGE_TYPE(B_BINDER_NODE_TYPE),
	kPackedLargeBinderWeakNodeType = B_PACK_LARGE_TYPE(B_BINDER_WEAK_NODE_TYPE),
};

// Internal data structure used by driver.
struct binder_node;

// This is the flattened representation of a Binder object for transfer
// between processes.  The 'offsets' supplied as part of a binder transaction
// contains offsets into the data where these structures occur.  The Binder
// driver takes care of re-writing the structure type and data as it moves
// between processes.
//
// Note that this is very intentionally designed to be the same as a user-space
// large_flat_data structure holding 8 bytes.  The IPC mechanism requires that
// this structure be at least 8 bytes large.
typedef struct flat_binder_object
{
	// 8 bytes for large_flat_header.
	unsigned long		type;
	unsigned long		length;
	
	// 8 bytes of data.
	union {
		void*				binder;		// local object
		signed long			handle;		// remote object
		struct binder_node*	node;		// driver node
	};
	void*				cookie;			// extra data associated with local object
} flat_binder_object_t;

/*
 * On 64-bit platforms where user code may run in 32-bits the driver must
 * translate the buffer (and local binder) addresses apropriately.
 */

typedef struct binder_write_read {
	signed long		write_size;		// bytes to write
	signed long		write_consumed; // bytes consumed by driver (for ERESTARTSYS)
	unsigned long	write_buffer;
	signed long		read_size;		// bytes to read
	signed long		read_consumed;	// bytes consumed by driver (for ERESTARTSYS)
	unsigned long	read_buffer;
} binder_write_read_t;

// Use with BINDER_VERSION, driver fills in fields.
typedef struct binder_version {
	signed long		protocol_version;	// driver protocol version -- increment with incompatible change
} binder_version_t;

// This is the current protocol version.
#define BINDER_CURRENT_PROTOCOL_VERSION 4

#define BINDER_IOC_MAGIC 'b'
#define BINDER_WRITE_READ _IOWR(BINDER_IOC_MAGIC, 1, binder_write_read_t)
#define BINDER_SET_WAKEUP_TIME	_IOW(BINDER_IOC_MAGIC, 2, binder_wakeup_time_t)
#define	BINDER_SET_IDLE_TIMEOUT	_IOW(BINDER_IOC_MAGIC, 3, bigtime_t)
#define	BINDER_SET_REPLY_TIMEOUT	_IOW(BINDER_IOC_MAGIC, 4, bigtime_t)
#define	BINDER_SET_MAX_THREADS	_IOW(BINDER_IOC_MAGIC, 5, size_t)
#define	BINDER_SET_IDLE_PRIORITY	_IOW(BINDER_IOC_MAGIC, 6, int)
#define	BINDER_SET_CONTEXT_MGR	_IOW(BINDER_IOC_MAGIC, 7, int)
#define	BINDER_THREAD_EXIT	_IOW(BINDER_IOC_MAGIC, 8, int)
#define BINDER_VERSION _IOWR(BINDER_IOC_MAGIC, 9, binder_version_t)
#define BINDER_IOC_MAXNR 9

// NOTE: Two special error codes you should check for when calling
// in to the driver are:
//
// EINTR -- The operation has been interupted.  This should be
// handled by retrying the ioctl() until a different error code
// is returned.
//
// ECONNREFUSED -- The driver is no longer accepting operations
// from your process.  That is, the process is being destroyed.
// You should handle this by exiting from your process.  Note
// that once this error code is returned, all further calls to
// the driver from any thread will return this same code.

typedef int64_t bigtime_t;

enum transaction_flags {
	tfInline = 0x01,			// not yet implemented
	tfSynchronous = 0x02,		// obsolete
	tfRootObject = 0x04,		// contents are the component's root object
	tfStatusCode = 0x08			// contents are a 32-bit status code
};

typedef struct binder_transaction_data
{
	// The first two are only used for bcTRANSACTION and brTRANSACTION,
	// identifying the target and contents of the transaction.
	union {
		unsigned long	handle;		// target descriptor of command transaction
		void			*ptr;		// target descriptor of return transaction
	} target;
	unsigned int	code;			// transaction command
	
	// General information about the transaction.
	unsigned int	flags;
	int	priority;					// requested/current thread priority
	size_t	data_size;				// number of bytes of data
	size_t	offsets_size;			// number of bytes of flat_binder_object offsets
	
	// If this transaction is inline, the data immediately
	// follows here; otherwise, it ends with a pointer to
	// the data buffer.
	union {
		struct {
			const void	*buffer;	// transaction data
			const void	*offsets;	// offsets to flat_binder_object structs
		} ptr;
		uint8_t	buf[8];
	} data;
} binder_transaction_data_t;

typedef struct binder_wakeup_time
{
	bigtime_t time;
	int priority;
} binder_wakeup_time_t;

enum BinderDriverReturnProtocol {
	brERROR = -1,
	/*
		int: error code
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
		int: 0 if the last bcATTEMPT_ACQUIRE was not successful.
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
		void *: cookie for binder
	*/
	
	brATTEMPT_ACQUIRE,
	/*
		int:	priority
		void *: ptr to binder
		void *: cookie for binder
	*/
	
	brEVENT_OCCURRED,
	/*
		This is returned when the bcSET_NEXT_EVENT_TIME has elapsed.
		At this point the next event time is set to B_INFINITE_TIMEOUT,
		so you must send another bcSET_NEXT_EVENT_TIME command if you
		have another event pending.
	*/

	brNOOP,
	/*
	 * No parameters.  Do nothing and examine the next command.  It exists
	 * primarily so that we can replace it with a brSPAWN_LOOPER command.
	 */

	brSPAWN_LOOPER,
	/*
	 * No parameters.  The driver has determined that a process has no threads
	 * waiting to service incomming transactions.  When a process receives this
	 * command, it must spawn a new service thread and register it via
	 * bcENTER_LOOPER.
	 */

	brFINISHED
};

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
		int:  0 if the last brATTEMPT_ACQUIRE was not successful.
		Else you have acquired a primary reference on the object.
	*/
	
	bcFREE_BUFFER,
	/*
		void *: ptr to transaction data received on a read
	*/
	
	bcTRANSACTION_COMPLETE,
	/*
		No parameters... send when finishing an asynchronous transaction.
	*/
	
	bcINCREFS,
	bcACQUIRE,
	bcRELEASE,
	bcDECREFS,
	/*
		int:	descriptor
	*/
	
	bcINCREFS_DONE,
	bcACQUIRE_DONE,
	/*
		void *:	ptr to binder
		void *: cookie for binder
	*/
	
	bcATTEMPT_ACQUIRE,
	/*
		int:	priority
		int:	descriptor
	*/
	
	bcRETRIEVE_ROOT_OBJECT,
	/*
		int:	process ID
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
	
	bcSYNC,
	/*
		No parameters.
		Upon receiving this command, the driver waits until all
		pending asynchronous transactions have completed.
	*/
	
#if 0
	bcCATCH_ROOT_OBJECTS,
	/*
		No parameters.
		Call this to have your team start catching root objects
		published by other teams that are spawned outside of the binder.
		When this happens, you will receive a brTRANSACTION with the
		tfRootObject flag set.  (Note that this is distinct from receiving
		normal root objects, which are a brREPLY.)
	*/
#endif
	
	bcSTOP_PROCESS,
	/*
		int: descriptor of process's root object
		int: 1 to stop immediately, 0 when root object is released
	*/

	bcSTOP_SELF
	/*
		int: 1 to stop immediately, 0 when root object is released
	*/
};

#if 0
/*	Parameters for BINDER_READ_WRITE ioctl. */
#if BINDER_DEBUG_LIB

struct binder_write_read
{
	ssize_t		write_size;
	const void*	write_buffer;
	ssize_t		read_size;
	void*		read_buffer;
};


/*	Below are calls to access the binder when debugging the driver from
	user space by compiling it as libbinderdbg and linking libbe2 with it. */

extern int			open_binder(int teamID=0);
extern status_t		close_binder(int desc);
extern status_t		ioctl_binder(int desc, int cmd, void *data, int len);
extern ssize_t		read_binder(int desc, void *data, size_t numBytes);
extern ssize_t		write_binder(int desc, void *data, size_t numBytes);

#else

#include <unistd.h>
inline int			open_binder(int ) { return open("/dev/misc/binder2",O_RDWR|O_CLOEXEC); };
inline status_t		close_binder(int desc) { return close(desc); };
inline status_t		ioctl_binder(int desc, int cmd, void *data, int len) { return ioctl(desc,cmd,data,len); };
inline ssize_t		read_binder(int desc, void *data, size_t numBytes) { return read(desc,data,numBytes); };
inline ssize_t		write_binder(int desc, void *data, size_t numBytes) { return write(desc,data,numBytes); };

#endif
#endif

#ifdef __cplusplus
#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
#endif

#endif // _BINDER_MODULE_H_
