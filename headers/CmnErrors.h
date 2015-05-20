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

/*!
	@file CmnErrors.h
	@ingroup CoreErrors
	@brief Definition of all error classes and common error codes.
*/

/*!	@addtogroup CoreErrors Error Codes
	@ingroup Core
	@brief How error codes are constructed, enumeration of
	all error classes in the system, and some pre-defined common
	error codes.

	An error code is a 32-bit value, structured as: 0x8000CCEE.  The CC
	section is an 8-bit error class, and the EE section is the 8-bit error
	value for that class.  For example, all system generic error codes are
	of the form 0x800005EE, and the "out of memory" error is a part of
	the system error class with the number 0x80000503.

	This definition means that all error codes are negative
	values, so you can multiplex an integer containing either a positive
	ssize_t non-error or an error code.

	There are a standard set of generic error codes that are defined, which
	should be used wherever applicable instead of creating your own error code.
	For example, unlike in the past where there was a different out-of-memory
	error code for each error class, you should now return the generic
	sysErrNoFreeRAM instead of defining your own.
	@{
*/

#ifndef _CMNERRORS_H_
#define _CMNERRORS_H_

#include <sys/types.h>
#include <errno.h>

/*!	@name Error Reporting
	Definitions for program error reporting facilities. */
//@{

// Define some error constants not already defined for WIN32
#if (TARGET_HOST == TARGET_HOST_WIN32)
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

#ifndef ENOTSUP
#define ENOTSUP 524
#endif
#endif // TARGET_HOST_WIN32


// Error reporting options
#define	errReportStripContext	0x00000001	// Originating file and line not available

// Possible responses
#define errResponseTryHighLevel	0x00000001	// Low level code recommends trying high level UI first
#define errResponseBreakNative	0x00000002	// Break in the native code debugger
#define errResponseBreak68K		0x00000004	// Break in the 68000 code debugger
#define errResponseBreakBoth	(errResponseBreakNative + errResponseBreak68K)
#define	errResponseIgnore		0x00000008	// Resume offending code
#define errResponseKillProcess	0x00000010	// Kill the process hosting the offending code
#define errResponseSoftReset	0x00000020	// Soft reset the Palm OS
#define errResponseShutdown		0x00000040	// Shutdown whole system (for the simulation this means: exit it)

#if BUILD_TYPE == BUILD_TYPE_DEBUG
#	define errResponseDefaultSet	(errResponseBreakNative | errResponseKillProcess |\
										errResponseSoftReset | errResponseShutdown | errResponseIgnore)
#else
#	define errResponseDefaultSet	(errResponseBreakNative | errResponseKillProcess |\
										errResponseSoftReset | errResponseShutdown )
#endif

//@}

/************************************************************
 * Error Classes for each manager
 *************************************************************/

/*! @name Error Classes
	Base values for all error classes. */
//@{

#define errorClassMask        	0xFFFFFF00	// Mask for extracting class code

#define	memErrorClass			0x80000100	// Memory Manager
#define	dmErrorClass			0x80000200	// Data Manager
#define	serErrorClass			0x80000300	// Serial Manager
#define	slkErrorClass			0x80000400	// Serial Link Manager
#define	sysErrorClass			0x80000500	// System Manager
#define	fplErrorClass			0x80000600	// Floating Point Library
#define	flpErrorClass			0x80000680	// New Floating Point Library
#define	evtErrorClass			0x80000700  // System Event Manager
#define	sndErrorClass			0x80000800  // Sound Manager
#define	almErrorClass			0x80000900  // Alarm Manager
#define	timErrorClass			0x80000A00  // Time Manager
#define	penErrorClass			0x80000B00  // Pen Manager
#define	ftrErrorClass			0x80000C00  // Feature Manager
#define	cmpErrorClass			0x80000D00  // Connection Manager (HotSync)
#define	dlkErrorClass			0x80000E00	// Desktop Link Manager
#define	padErrorClass			0x80000F00	// PAD Manager
#define	grfErrorClass			0x80001000	// Graffiti Manager
#define	mdmErrorClass			0x80001100	// Modem Manager
#define	netErrorClass			0x80001200	// Net Library
#define	htalErrorClass			0x80001300	// HTAL Library
#define	inetErrorClass			0x80001400	// INet Library
#define	exgErrorClass			0x80001500	// Exg Manager
#define	fileErrorClass			0x80001600	// File Stream Manager
#define	rfutErrorClass			0x80001700	// RFUT Library
#define	txtErrorClass			0x80001800	// Text Manager
#define	tsmErrorClass			0x80001900	// Text Services Library
#define	webErrorClass			0x80001A00	// Web Library
#define	secErrorClass			0x80001B00	// Security Library
#define	emuErrorClass			0x80001C00	// Emulator Control Manager
#define	flshErrorClass			0x80001D00	// Flash Manager
#define	pwrErrorClass			0x80001E00	// Power Manager
#define	cncErrorClass			0x80001F00	// Connection Manager (Serial Communication)
#define	actvErrorClass			0x80002000	// Activation application
#define	radioErrorClass		   	0x80002100	// Radio Manager (Library)
#define	dispErrorClass			0x80002200	// Display Driver Errors.
#define	bltErrorClass			0x80002300	// Blitter Driver Errors.
#define	winErrorClass			0x80002400	// Window manager.
#define	omErrorClass			0x80002500	// Overlay Manager
#define	menuErrorClass			0x80002600	// Menu Manager
#define	lz77ErrorClass			0x80002700	// Lz77 Library
#define	smsErrorClass			0x80002800	// Sms Library
#define	expErrorClass			0x80002900	// Expansion Manager and Slot Driver Library
#define	vfsErrorClass			0x80002A00	// Virtual Filesystem Manager and Filesystem library
#define	lmErrorClass			0x80002B00	// Locale Manager
#define	intlErrorClass			0x80002C00	// International Manager
#define pdiErrorClass			0x80002D00	// PDI Library
#define	attnErrorClass			0x80002E00	// Attention Manager
#define	telErrorClass			0x80002F00	// Telephony Manager
#define hwrErrorClass			0x80003000	// Hardware Manager (HAL)
#define	blthErrorClass			0x80003100	// Bluetooth Library Error Class
#define	udaErrorClass			0x80003200	// UDA Manager Error Class
#define	tlsErrorClass			0x80003300	// Thread Local Storage
#define em68kErrorClass			0x80003400	// 68K appl emulator
#define grmlErrorClass			0x80003500	// Gremlins
#define IrErrorClass			0x80003600	// IR Library
#define IrCommErrorClass		0x80003700	// IRComm Serial Mgr Plugin
#define cpmErrorClass			0x80003800  // Crypto Manager
#define sslErrorClass			0x80003900  // SSL (from RSA)
#define kalErrorClass			0x80003A00  // KAL errors
#define halErrorClass			0x80003B00  // HAL errors
#define azmErrorClass			0x80003C00  // Authorization Manager (AZM)
#define amErrorClass			0x80003D00  // Authentication Manager (AM)
#define dirErrorClass			0x80003E00  // Directory
#define svcErrorClass			0x80003F00  // Service Manager
#define appMgrErrorClass		0x80004000  // Application Manager
#define ralErrorClass			0x80004100  // RAL errors
#define iosErrorClass			0x80004200  // IOS errors
#define signErrorClass			0x80004300  // Digital Signature Verification shared library
#define perfErrorClass			0x80004400  // Performance Manager
#define drvrErrorClass			0x80004500  // Hardware Driver errors
#define mediaErrorClass			0x80004600  // Multimedia Error Class
#define catmErrorClass			0x80004700  // Category Mgr Errors
#define certErrorClass			0x80004800	// Certificate Manager Error Class
#define secSvcsErrorClass		0x80004900  // Security Services Errors
#define bndErrorClass			0x80004A00  // Binder Error Class
#define syncMgrErrorClass		0x80004B00	// Sync Manager Errors
#define HttpErrorClass			0x80004C00	// Http Lib errors
//#define xSyncErrorClass 		0x80004D00	// Exchange Sync Library Errors
#define hsExgErrorClass			0x80004E00	// HotSync Exchange library errors
#define pppErrorClass			0x80004F00	// PPP lib errors
#define pinsErrorClass			0x80005000	// Pen Input Services errors
#define statErrorClass			0x80005100	// Status Bar Service errors
#define regexpErrorClass		0x80005200	// Regular Expression errors
//#define posixErrorClass			0x80005300	// All those nice POSIX errors  XXX now -ERR
#define uilibErrorClass			0x80005400	// UI Library (Forms, Controls, etc)
#define	rimErrorClass			0x80005500	// RIM Manager (BlackBerry Communication)
#define dtErrorClass			0x80005600	// DataTransform Library errors
#define mimeErrorClass			0x80005700	// MIME Library errors
#define dtIntProtClass			0x80005800	// Internet protocole error class in DataTransform Library
#define xmlErrorClass			0x80005900	// XML parser errors
#define postalErrorClass		0x80006000	// Postal Kit errors
#define mobileErrorClass		0x80006100	// Mobile Kit errors
#define vaultErrorClass			0x80006200  // Vault Manager errors

#define	oemErrorClass			0x80007000	// OEM/Licensee errors (0x80007000-0x80007EFF shared among ALL partners)
#define errInfoClass			0x80007F00	// special class shows information w/o error code
#define	appErrorClass			0x80008000	// Application-defined errors

#define	dalErrorClass			0x8000FF00	// DAL error class

//@}

/*******************************************************************
 *	Error Codes
 *******************************************************************/

#define	kDALError				((status_t)(dalErrorClass | 0x00FF))
#define	kDALTimeout				((status_t)(sysErrorClass | 1))	// compatible with sysErrTimeout

/************************************************************
 * System Errors
 *************************************************************/

/*!	@name Generic Error Codes
	You should use one of these error codes if it is applicable to your situation. */
//@{

//!	Non-error condition.
/*!	@note The ssize to type is used to indicate that positive values may be
	returning in the non-error case.  You should always check for success
	with the expression "error >= errNone". */
#define	errNone							0x00000000

//!	The requested operation took too long to complete.
#define sysErrTimeout					((status_t)(sysErrorClass | 1))
//!	The input parameters were invalid or corrupt.
/*!	This error means that the given parameters made no sense.  You will
	usually want to use a more specific error code than this such as
	sysErrBadIndex, sysErrBadType, sysErrMismatchedValues, or
	sysErrBadData. */
#define sysErrParamErr					((status_t)(sysErrorClass | 2))
//!	The requested resource could not be allocated.
/*!	The code sysErrNoFreeRAM should be used for memory. */
#define sysErrNoFreeResource			((status_t)(sysErrorClass | 3))
//!	Not of enough RAM to perform needed allocation.
#define sysErrNoFreeRAM					((status_t)(sysErrorClass | 4))
//!	The requested operation is not allowed.
/*!	Generally you should use the more specific errors sysErrUnsupported
	or sysErrPermissionDenied. */
#define sysErrNotAllowed				((status_t)(sysErrorClass | 5))
#define	sysErrOutOfOwnerIDs				((status_t)(sysErrorClass | 8))
#define	sysErrNoFreeLibSlots			((status_t)(sysErrorClass | 9))
#define	sysErrLibNotFound				((status_t)(sysErrorClass | 10))
#define sysErrModuleNotFound			sysErrLibNotFound
#define	sysErrRomIncompatible			((status_t)(sysErrorClass | 12))
#define sysErrBufTooSmall				((status_t)(sysErrorClass | 13))
#define	sysErrPrefNotFound				((status_t)(sysErrorClass | 14))

//!	NotifyMgr: Could not find registration entry in the list.
#define	sysNotifyErrEntryNotFound		((status_t)(sysErrorClass | 16))
//!	NotifyMgr: Identical entry already exists.
#define	sysNotifyErrDuplicateEntry		((status_t)(sysErrorClass | 17))
//!	NotifyMgr: A broadcast is already in progress - try again later.
#define	sysNotifyErrBroadcastBusy		((status_t)(sysErrorClass | 19))
//!	NotifyMgr: A handler cancelled the broadcast.
#define	sysNotifyErrBroadcastCancelled	((status_t)(sysErrorClass | 20))
//!	NotifyMgr: Can't find the notification server.
#define sysNotifyErrNoServer			((status_t)(sysErrorClass | 21))

//!	NotifyMgr Phase 2: Deferred queue is full.
#define	sysNotifyErrQueueFull			((status_t)(sysErrorClass | 27))
//!	NotifyMgr Phase 2: Deferred queue is empty.
#define	sysNotifyErrQueueEmpty			((status_t)(sysErrorClass | 28))
//!	NotifyMgr Phase 2: Not enough stack space for a broadcast.
#define	sysNotifyErrNoStackSpace		((status_t)(sysErrorClass | 29))
//!	NotifyMgr Phase 2: Manager is not initialized.
#define	sysErrNotInitialized			((status_t)(sysErrorClass | 30))

// Loader error codes:
#define sysErrModuleInvalid				((status_t)(sysErrorClass | 31))  // following sysErrNotInitialized
#define sysErrModuleIncompatible		((status_t)(sysErrorClass | 32))
//!	Loader: ARM module not found, but 68K code resource was.
#define sysErrModuleFound68KCode		((status_t)(sysErrorClass | 33))
//!	Loader: Failed to apply relocation while loading a module.
#define sysErrModuleRelocationError		((status_t)(sysErrorClass | 34))
//!	Loader: Module has no global structure.
#define sysErrNoGlobalStructure			((status_t)(sysErrorClass | 35))
//!	Loader: Module has no valid digital signuture.
#define sysErrInvalidSignature			((status_t)(sysErrorClass | 36))
//!	Loader: System encountered unexpected internal error.
#define sysErrInternalError				((status_t)(sysErrorClass | 37))
//!	Loader: Error occured during dynamic linking.
#define sysErrDynamicLinkerError		((status_t)(sysErrorClass | 38))
//!	Loader: RAM-based module cannot be loaded when device is booted into ROM-only mode.
#define sysErrRAMModuleNotAllowed		((status_t)(sysErrorClass | 39))
//!	Loader: Program needs different architecture of the CPU to run.
#define sysErrCPUArchitecture			((status_t)(sysErrorClass | 40))

//!	Out-of-range index supplied to function.
#define sysErrBadIndex					((status_t)(sysErrorClass | 41))
//!	Bad argument type supplied to function.
#define sysErrBadType					((status_t)(sysErrorClass | 42))
//!	Conflicting values supplied to the function.
#define sysErrMismatchedValues			((status_t)(sysErrorClass | 43))
//!	The requested name does not exist.  Also see sysErrEntryNotFound.
/*!	This error indicates that a plain name lookup did not succeed.  You
	should use instead sysErrEntryNotFound for a lookup failure on a
	filesystem or other structured storage. */
#define sysErrNameNotFound				((status_t)(sysErrorClass | 44))
//!	The requested name already exists.  Also see sysErrEntryExists.
/*!	This error indicates that there is already a plain item with the
	requested name.  You should use instead sysErrEntryExists for a name
	that already exists on a filesystem or other structured storage. */
#define sysErrNameInUse					((status_t)(sysErrorClass | 45))
//!	Target not initialized.
/*!	An operation on object could not be completed because that object
	has not been initialized.  This error should only be returned when
	an object needs something besides just construction to be
	initialized.  If initializion was attempted but could not be done,
	another more specific error should be returned. */
#define sysErrNoInit					((status_t)(sysErrorClass | 46))
//!	Input data is corrupt.
/*!	This indicates that the contents of structured input data is corrupt.
	It is used when the parameters are valid (unlike sysErrParamErr and
	friends), but the data they reference is bad.  For example, calling
	a function to read a PNG image, where the image data is corrupt. */
#define sysErrBadData					((status_t)(sysErrorClass | 47))
//!	Not all of the data got through.
/*!	Some data was returned, but it is not all that was available and/or
	expected. */
#define sysErrDataTruncated				((status_t)(sysErrorClass | 48))
//!	General IO error.
/*!	Very generic input/output error.  Generally shouldn't be used,
	instead preferring more specific errors such as sysErrTimeout,
	sysErrCancelled, sysErrPermissionDenied, sysErrBrokenPipe,
	sysErrWouldBlock, sysErrBusy, sysErrInterrupted, etc. */
#define sysErrIO						((status_t)(sysErrorClass | 49))
//!	My bad.  Will be fixed in the next version.  Really.
#define sysErrBadDesignEncountered		((status_t)(sysErrorClass | 50))

//!	AppMgr: Process has faulted during execution.
#define sysErrProcessFaulted			((status_t)(sysErrorClass | 51))

//!	I was holding a weak reference, and that object is gone now.
#define sysErrWeakRefGone				((status_t)(sysErrorClass | 52))

//!	AppMgr: Program needs higher OS version to run.
#define sysErrOSVersion					((status_t)(sysErrorClass | 53))

//!	Reached the end of data, no more data available.
/*!	This is a pseudo-error, returned by iterators and other things
	that let you step through there data.  It is returned when requesting
	more data when you have reached the end of the iterator. */
#define sysErrEndOfData					((status_t)(sysErrorClass | 54))

//We may be building the support kit for windows so make sure some of these are defined
#ifndef ECANCEL
#define	ECANCEL		5
#endif

#ifndef ENOTSUP
#define	ENOTSUP		45
#endif

#ifndef EWOULDBLOCK
#define	EWOULDBLOCK	EAGAIN
#endif

// Nice names for some POSIX error codes:

//!	POSIX: The operation was cancelled by another entity such as the user.
#define sysErrCanceled					((status_t)(-ECANCEL))
//!	POSIX: You do not have permission to access the given item.
/*!	This error specifically indicates that the requested operation is possible,
	you simply aren't to do it.  Compare with sysErrEntryNotFound (the thing
	being operated on does not exist) and sysErrUnsupported (nobody can do what
	is being asked). */
#define sysErrPermissionDenied			((status_t)(-EPERM))
//!	POSIX: The requested operation is not supported.
/*!	The implementation you are calling on does not support what was asked.
	Compare with sysErrPermissionDenied. */
#define sysErrUnsupported				((status_t)(-ENOTSUP))
//!	POSIX: The IO pipe you are using is no longer working.
#define sysErrBrokenPipe				((status_t)(-EPIPE))
//!	POSIX: There is no entry with the requested name.
/*!	For example, trying to open a file that does not exist.  Compare with
	sysErrPermissionDenied. */
#define sysErrEntryNotFound				((status_t)(-ENOENT))
//!	POSIX: Attempted to create an entry that already exists.
/*!	You have attempted to perform an operation that would create an entry
	with a specific name, where there is already such an entry. */
#define sysErrEntryExists				((status_t)(-EEXIST))
//!	POSIX: A name string is longer than supported by the target.
#define sysErrNameTooLong				((status_t)(-E2BIG))
//!	POSIX: The IO operation would need to block for some amount of time to complete.
#define sysErrWouldBlock				((status_t)(-EWOULDBLOCK))
//!	POSIX: The target object is currently in use by someone else.
#define sysErrBusy						((status_t)(-EBUSY))
//!	POSIX: The input is not within a valid range.
#define sysErrOutOfRange				((status_t)(-ERANGE))
//!	POSIX: That's not a good idea.
#define sysErrDontDoThat				((status_t)(-EACCES))
//!	POSIX: The given address is not valid.
#define sysErrBadAddress				((status_t)(-EBADADDR))
//!	POSIX: Your operation was rudely interrupted by something else in the system.
#define sysErrInterrupted				((status_t)(-EINTR))

//@}

/************************************************************
 * Binder Errors
 *************************************************************/

/*!	@name Binder Error Codes
	Errors that are specific to the Binder infrastructure. */
//@{

#define bndErrMissingArg				((status_t)(bndErrorClass | 1))  //!< Required argument not supplied.
#define bndErrBadType					((status_t)(bndErrorClass | 2))  //!< Given argument has invalid type.  @todo Should this go away and just use B_BAD_TYPE?
#define bndErrDead						((status_t)(bndErrorClass | 3))  //!< Target Binder is no longer with us.
#define bndErrUnknownTransact			((status_t)(bndErrorClass | 4))  //!< Don't know the given transaction code.
#define bndErrBadTransact				((status_t)(bndErrorClass | 5))  //!< Transaction data is corrupt.
#define bndErrTooManyLoopers			((status_t)(bndErrorClass | 6))  //!< No more asynchronicity for you!
#define bndErrBadInterface				((status_t)(bndErrorClass | 7))  //!< Requested interface is not implemented.
#define bndErrUnknownMethod				((status_t)(bndErrorClass | 8))  //!< Binder does not implement requested method.
#define bndErrUnknownProperty			((status_t)(bndErrorClass | 9))  //!< Binder does not implement requested property.
#define bndErrOutOfStack				((status_t)(bndErrorClass | 10)) //!< No more recursion for you!
#define bndErrIncStrongFailed			((status_t)(bndErrorClass | 11)) //!< AttemptIncStrong() -- no more strong refs.
#define bndErrReadNullValue				((status_t)(bndErrorClass | 11)) //!< Reading NULL value type from SParcel.

//@}

/************************************************************
 * RegExp Errors
 *************************************************************/

/*!	@name RegExp Error Codes
	Errors that are specific to regular expression parsing. */
//@{

#define regexpErrUnmatchedParenthesis			((status_t)(regexpErrorClass | 1))  //!< Such as ())
#define regexpErrTooBig							((status_t)(regexpErrorClass | 2))  //!< Not that we like hard-coded limits.
#define regexpErrTooManyParenthesis				((status_t)(regexpErrorClass | 3))  //!< No, really, we don't like hard-coded limits.
#define regexpErrJunkOnEnd						((status_t)(regexpErrorClass | 4))  //!< Junk isn't good for you.
#define regexpErrStarPlusOneOperandEmpty		((status_t)(regexpErrorClass | 5))  //!< As it says.
#define regexpErrNestedStarQuestionPlus			((status_t)(regexpErrorClass | 6))  //!< Likewise.
#define regexpErrInvalidBracketRange			((status_t)(regexpErrorClass | 8))  //!< Bad stuff inside [].
#define regexpErrUnmatchedBracket				((status_t)(regexpErrorClass | 9))  //!< Such as []].
#define regexpErrInternalError					((status_t)(regexpErrorClass | 10))  //!< Uh oh!
#define regexpErrQuestionPlusStarFollowsNothing	((status_t)(regexpErrorClass | 11))  //!< That's a mouthful.
#define regexpErrTrailingBackslash				((status_t)(regexpErrorClass | 12))  //!< This isn't C.
#define regexpErrCorruptedProgram				((status_t)(regexpErrorClass | 13))  //!< Poor program.
#define regexpErrMemoryCorruption				((status_t)(regexpErrorClass | 14))  //!< Poor memory.
#define regexpErrCorruptedPointers				((status_t)(regexpErrorClass | 15))  //!< Poor pointer.
#define regexpErrCorruptedOpcode				((status_t)(regexpErrorClass | 16))  //!< Poor opcode.

//@}

/************************************************************
 * Media Errors
 *************************************************************/

/*!	@name Media Error Codes
	Errors that are specific to multimedia operations. */
//@{

#define mediaErrFormatMismatch			((status_t)(mediaErrorClass | 1))
#define mediaErrAlreadyVisited			((status_t)(mediaErrorClass | 2))
#define mediaErrStreamExhausted			((status_t)(mediaErrorClass | 3))
#define mediaErrAlreadyConnected		((status_t)(mediaErrorClass | 4))
#define mediaErrNotConnected			((status_t)(mediaErrorClass | 5))
#define mediaErrNoBufferSource			((status_t)(mediaErrorClass | 6))
#define mediaErrBufferFlowMismatch		((status_t)(mediaErrorClass | 7))

//@}

/************************************************************
 * Exchange / Web Errors
 *************************************************************/

#define exgMemError	 			(exgErrorClass | 1)
#define exgErrStackInit 		(exgErrorClass | 2)  // stack could not initialize
#define exgErrUserCancel 		(exgErrorClass | 3)
#define exgErrNoReceiver 		(exgErrorClass | 4)	// receiver device not found
#define exgErrNoKnownTarget		(exgErrorClass | 5)	// can't find a target app
#define exgErrTargetMissing		(exgErrorClass | 6)  // target app is known but missing
#define exgErrNotAllowed		(exgErrorClass | 7)  // operation not allowed
#define exgErrBadData			(exgErrorClass | 8)  // internal data was not valid
#define exgErrAppError			(exgErrorClass | 9)  // generic application error
#define exgErrUnknown			(exgErrorClass | 10) // unknown general error
#define exgErrDeviceFull		(exgErrorClass | 11) // device is full
#define exgErrDisconnected		(exgErrorClass | 12) // link disconnected
#define exgErrNotFound			(exgErrorClass | 13) // requested object not found
#define exgErrBadParam			(exgErrorClass | 14) // bad parameter to call
#define exgErrNotSupported		(exgErrorClass | 15) // operation not supported by this library
#define exgErrDeviceBusy		(exgErrorClass | 16) // device is busy
#define exgErrBadLibrary		(exgErrorClass | 17) // bad or missing ExgLibrary
#define exgErrNotEnoughPower	(exgErrorClass | 18) // Device has not enough power to perform the requested operation
#define exgErrNoHardware		(exgErrorClass | 19) // Device does has not have corresponding hardware
#define exgErrAuthRequired		(exgErrorClass | 20) // Need authentication - username/passwd
#define exgErrRedirect			(exgErrorClass | 21) // Redirected url
#define exgErrLibError			(exgErrorClass | 22) // Library specific error - usually the library can provide more details
#define exgErrConnection		(exgErrorClass | 23) // There was an error in making the connection
#define exgErrInvalidURL		(exgErrorClass | 24) // Bad URL
#define exgErrInvalidScheme		(exgErrorClass | 25) // Bad scheme in URL

#endif	// _CMNERRORS_H_

/*! @} */
