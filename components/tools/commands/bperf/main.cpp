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

#define SHOW_OBJS(x)
//#define SHOW_OBJS(x) x

#include <support/InstantiateComponent.h>
#include <support/Package.h>
#include <support/IProcess.h>
#include <support/Locker.h>
#include <support/Autolock.h>
#include <support/CallStack.h>
#include <support/Looper.h>
#include <app/BCommand.h>
#include <app/SGetOpts.h>
#include <math.h>
#include <stdlib.h>
#include <support/Iterator.h>
#include <SysThreadConcealed.h>

#if defined(LINUX_DEMO_HACK)
#define B_PERFCNT_MAX_NAME_SIZE 28
#endif

#if TEST_PALMOS_APIS
#include <render_p/PointInlines.h>
#endif

// For function call tests
#if TEST_PALMOS_APIS
#include <SystemMgr.h>
#include <Window.h>
#endif

#if TARGET_HOST == TARGET_HOST_BEOS
#define sysThreadPriorityLow				B_LOW_PRIORITY
#define sysThreadPriorityNormal				B_NORMAL_PRIORITY
#define sysThreadPriorityDisplay			B_DISPLAY_PRIORITY
#define sysThreadPriorityUrgentDisplay		B_URGENT_DISPLAY_PRIORITY
#define sysThreadPriorityHigh				B_DISPLAY_PRIORITY
#define sysThreadPriorityBestUser			B_DISPLAY_PRIORITY
#define sysThreadPriorityBestSystem			B_URGENT_DISPLAY_PRIORITY
#define KALGetSystemTime					SysGetRunTime
#endif

#include "TransactionTest.h"
#include "HandlerTest.h"
#include "EffectIPC.h"

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::app;
using namespace palmos::view;
#endif

#ifdef memcpy
#undef memcpy
extern "C" void *memcpy(void *d, const void *s, ssize_t l);
#endif


enum
{
	B_MAX_EVENT_COUNTERS = 8
};

const uint64_t kAddTestMask							= B_MAKE_UINT64(1) << 0;
const uint64_t kAtomicOpsTestMask					= B_MAKE_UINT64(1) << 1;
const uint64_t kFunctionTestMask					= B_MAKE_UINT64(1) << 2;
const uint64_t kSystemTimeCompatTestMask			= B_MAKE_UINT64(1) << 3;
const uint64_t kSystemTimeTestMask					= B_MAKE_UINT64(1) << 4;
const uint64_t kContextSwitchTestMask				= B_MAKE_UINT64(1) << 5;
const uint64_t kKeyTestMask							= B_MAKE_UINT64(1) << 6;
const uint64_t kMemPtrNewTestMask					= B_MAKE_UINT64(1) << 7;
const uint64_t kMemHandleLockTestMask				= B_MAKE_UINT64(1) << 8;
const uint64_t kMallocTestMask						= B_MAKE_UINT64(1) << 9;
const uint64_t kNewMessageTestMask					= B_MAKE_UINT64(1) << 10;
const uint64_t kNewBinderTestMask					= B_MAKE_UINT64(1) << 11;
const uint64_t kNewComponentTestMask				= B_MAKE_UINT64(1) << 12;
const uint64_t kCriticalSectionTestMask				= B_MAKE_UINT64(1) << 13;
const uint64_t kLockerTestMask						= B_MAKE_UINT64(1) << 14;
const uint64_t kAutolockTestMask					= B_MAKE_UINT64(1) << 15;
const uint64_t kNestedLockerTestMask				= B_MAKE_UINT64(1) << 16;
const uint64_t kIncStrongTestMask					= B_MAKE_UINT64(1) << 17;
const uint64_t kAtomPtrTestMask						= B_MAKE_UINT64(1) << 18;
const uint64_t kMutexTestMask						= B_MAKE_UINT64(1) << 19;

const uint64_t kValueSimpleTestMask					= B_MAKE_UINT64(1) << 20;
const uint64_t kValueJoin2TestMask					= B_MAKE_UINT64(1) << 21;
const uint64_t kValueJoinManyTestMask				= B_MAKE_UINT64(1) << 22;
const uint64_t kValueLookupIntTestMask				= B_MAKE_UINT64(1) << 23;
const uint64_t kValueLookupStrTestMask				= B_MAKE_UINT64(1) << 24;
const uint64_t kValueBuildIntTestMask				= B_MAKE_UINT64(1) << 25;
const uint64_t kValueBuildStrTestMask				= B_MAKE_UINT64(1) << 26;

const uint64_t kSingleHandlerTestMask				= B_MAKE_UINT64(1) << 27;
const uint64_t kDoubleHandlerTestMask				= B_MAKE_UINT64(1) << 28;

const uint64_t kLocalInstantiateTestMask			= B_MAKE_UINT64(1) << 30;
const uint64_t kRemoteInstantiateTestMask			= B_MAKE_UINT64(1) << 31;
const uint64_t kLocalTransactionTestMask			= B_MAKE_UINT64(1) << 32;
const uint64_t kRemoteTransactionTestMask			= B_MAKE_UINT64(1) << 33;
const uint64_t kRemoteLargeTransactionTestMask		= B_MAKE_UINT64(1) << 34;
const uint64_t kLocalOldBinderTestMask				= B_MAKE_UINT64(1) << 35;
const uint64_t kRemoteOldBinderTestMask				= B_MAKE_UINT64(1) << 36;
const uint64_t kLocalSameBinderTestMask				= B_MAKE_UINT64(1) << 37;
const uint64_t kRemoteSameBinderTestMask			= B_MAKE_UINT64(1) << 38;
const uint64_t kLocalNewBinderTestMask				= B_MAKE_UINT64(1) << 39;
const uint64_t kRemoteNewBinderTestMask				= B_MAKE_UINT64(1) << 40;
const uint64_t kLocalAttemptIncStrongTestMask		= B_MAKE_UINT64(1) << 41;
const uint64_t kRemoteAttemptIncStrongTestMask		= B_MAKE_UINT64(1) << 42;
const uint64_t kMultipleAttemptIncStrongTestMask	= B_MAKE_UINT64(1) << 43;
const uint64_t kPingPongTransactionMask				= B_MAKE_UINT64(1) << 44;

const uint64_t kFloatSimpleTestMask					= B_MAKE_UINT64(1) << 45;
const uint64_t kLocalEffectIPCTestMask				= B_MAKE_UINT64(1) << 46;
const uint64_t kRemoteEffectIPCTestMask				= B_MAKE_UINT64(1) << 47;
const uint64_t kLibcTestMask						= B_MAKE_UINT64(1) << 48;

const uint64_t kDmNextTestMask						= B_MAKE_UINT64(1) << 50;
const uint64_t kDmInfoTestMask						= B_MAKE_UINT64(1) << 51;
const uint64_t kDmOpenTestMask						= B_MAKE_UINT64(1) << 52;
const uint64_t kDmFindResourceTestMask				= B_MAKE_UINT64(1) << 53;
const uint64_t kDmGetResourceTestMask				= B_MAKE_UINT64(1) << 54;
const uint64_t kDmHandleLockTestMask				= B_MAKE_UINT64(1) << 55;
const uint64_t kDbCursorOpenTestMask				= B_MAKE_UINT64(1) << 56;
const uint64_t kDbCursorMoveTestMask				= B_MAKE_UINT64(1) << 57;
const uint64_t kDbGetColumnValueMask				= B_MAKE_UINT64(1) << 58;
const uint64_t kDmIsReadyTestMask					= B_MAKE_UINT64(1) << 59;
const uint64_t kDmReadTestMask						= B_MAKE_UINT64(1) << 60;

const uint64_t kICacheTestMask						= B_MAKE_UINT64(1) << 61;

enum
{
	kShowCallStack				= 1000,
	kProfile,
	kProfileRate,
	kUsePMU,
	kPMUEventType,
	kIncludeLoop
};

static const SLongOption options[] =
{
	{ sizeof(SLongOption), "arm-opts", B_NO_ARGUMENT, 'a',
		"Set to standard ARM profiling options." },
	{ sizeof(SLongOption), "qc", B_NO_ARGUMENT, 'q',
		"Run with standard options for quality control." },
	{ sizeof(SLongOption), "iterations", B_REQUIRED_ARGUMENT, 'i',
		"Set number of iterations for each test.\nDefault is 1000." },
	{ sizeof(SLongOption), "priority", B_REQUIRED_ARGUMENT, 'p',
		"Set thread priority at which tests run.\nDefault is 80 (normal)." },
	{ sizeof(SLongOption), "size", B_REQUIRED_ARGUMENT, 's',
		"Set number of bytes to send in payload.\nDefault is 0." },
	{ sizeof(SLongOption), "validate", B_NO_ARGUMENT, 'v',
		"Perform validation of some tests.\nUse to test correctness instead of performance." },
	{ sizeof(SLongOption), "concurrency", B_REQUIRED_ARGUMENT, 'c',
		"Specify amount of concurrency for some tests." },
	{ sizeof(SLongOption), "", B_REQUIRED_ARGUMENT, 'j',
		"Synonym for --concurrency." },
	{ sizeof(SLongOption), "remote", B_REQUIRED_ARGUMENT, 'r',
		"Use explicit remote process for tests, instead of creating a temporary process." },
	{ sizeof(SLongOption), "profile", B_NO_ARGUMENT, kProfile,
		"Perform profiling of selected tests." },
	{ sizeof(SLongOption), "profile-rate", B_REQUIRED_ARGUMENT, kProfileRate,
		"Set profiling sampling rate to given period (in ms).\nDefault is 1ms." },
	{ sizeof(SLongOption), "use-pmu", B_NO_ARGUMENT, kUsePMU,
		"Read performance counters around selected tests." },
	{ sizeof(SLongOption), "pmu-event", B_REQUIRED_ARGUMENT, kPMUEventType,
		"Specify events for performance counters" },
	{ sizeof(SLongOption), "include-loop", B_REQUIRED_ARGUMENT, kIncludeLoop,
		"Don't remove loop overhead from test time." },
	{ sizeof(SLongOption), "callstack", B_NO_ARGUMENT, kShowCallStack,
		"Show SCallStack (debugging)." },
	{ sizeof(SLongOption), "help", B_NO_ARGUMENT, 'h',
		"Display help" },

	{ sizeof(SLongOption), "all", B_NO_ARGUMENT, 'A',
		"Run all tests." },
	{ sizeof(SLongOption), "all-core", B_NO_ARGUMENT, 'C',
		"Run all core tests:\n"
		"add, atomic-ops, function\n"
		"system-time-compat, system-time,\n"
		"context-switch, mem-ptr-new, mem-handle-lock,\n"
		"malloc, new-message, new-binder, new-component,\n"
		"critical-section, locker, autolock,\n"
		"inc-strong, atom-ptr, mutex." },
	{ sizeof(SLongOption), "all-value", B_NO_ARGUMENT, 'V',
		"Run all SValue tests:\n"
		"value-simple, value-join-2, value-join-many,\n"
		"value-lookup-int, value-lookup-str,\n"
		"value-add-int, value-add-str." },
	{ sizeof(SLongOption), "all-handler", B_NO_ARGUMENT, 'H',
		"Run all handler tests:\n"
		"pulse-handler, pingpong-handler." },
	{ sizeof(SLongOption), "all-package", B_NO_ARGUMENT, 'P',
		"Run all package tests:\n"
		"local-instantiate, remote-instantiate." },
	{ sizeof(SLongOption), "all-binder", B_NO_ARGUMENT, 'B',
		"Run all binder tests:\n"
		"all-local and all-remote." },
	{ sizeof(SLongOption), "all-effect", B_NO_ARGUMENT, 'E',
		"Run all local and remote BBinder::Effect() tests:\n"
		"local-effect, remote-effect.\n" },
	{ sizeof(SLongOption), "all-local", B_NO_ARGUMENT, 'L',
		"Run all local tests:\n"
		"local-instantiate, local-transaction,\n"
		"local-old-binder, local-same-binder, local-new-binder,\n"
		"local-effect." },
	{ sizeof(SLongOption), "all-remote", B_NO_ARGUMENT, 'R',
		"Run all remote tests:\n"
		"remote-instantiate. remote-transaction,\n"
		"remote-large-transaction,\n"
		"remote-old-binder, remote-same-binder,\n"
		"remote-new-binder, remote-attempt-inc-strong,\n"
		"multiple-attempt-inc-strong,\n"
		"ping-pong-transaction, remote-effect.\n" },
	{ sizeof(SLongOption), "all-float", B_NO_ARGUMENT, 'F',
		"Run all floating point tests:\n"
		"float-simple" },
	{ sizeof(SLongOption), "all-dm", B_NO_ARGUMENT, 'D',
		"Run all Data Manager tests:\n"
		"dm-next, dm-info, dm-open,\n"
		"dm-find-resource, dm-get-resource,\n"
		"dm-handle-lock, db-cursor-open,\n"
		"db-cursor-iterate, dm-is-ready." },

	{ sizeof(SLongOption), "test-add", B_NO_ARGUMENT, 1000,
		"Test simple addition." },
	{ sizeof(SLongOption), "test-atomic-ops", B_NO_ARGUMENT, 1000,
		"Test atomic operations, including add." },
	{ sizeof(SLongOption), "test-function", B_NO_ARGUMENT, 1000,
		"Test various kinds of function calls." },
	{ sizeof(SLongOption), "test-system-time-compat", B_NO_ARGUMENT, 1000,
		"Test calling SysGetRunTime()." },
	{ sizeof(SLongOption), "test-system-time", B_NO_ARGUMENT, 1000,
		"Test calling KALGetTime(B_TIMEBASE_RUN_TIME)." },
	{ sizeof(SLongOption), "test-context-switch", B_NO_ARGUMENT, 1000,
		"Test context switch between two threads." },
	{ sizeof(SLongOption), "test-keys", B_NO_ARGUMENT, 1000,
		"Test allocate and free kernel keys." },
	{ sizeof(SLongOption), "test-mem-ptr-new", B_NO_ARGUMENT, 1000,
		"Test allocate and free using MemPtrNew()." },
	{ sizeof(SLongOption), "test-mem-handle-lock", B_NO_ARGUMENT, 1000,
		"Test calling MemHandleLock()." },
	{ sizeof(SLongOption), "test-malloc", B_NO_ARGUMENT, 1000,
		"Test allocate and free using malloc()." },
	{ sizeof(SLongOption), "test-new-message", B_NO_ARGUMENT, 1000,
		"Test creation and destruction of SMessage." },
	{ sizeof(SLongOption), "test-new-binder", B_NO_ARGUMENT, 1000,
		"Test creation and destruction of BBinder." },
	{ sizeof(SLongOption), "test-new-component", B_NO_ARGUMENT, 1000,
		"Test creation and destruction of component object." },
	{ sizeof(SLongOption), "test-critical-section", B_NO_ARGUMENT, 1000,
		"Test kernel critical sections." },
	{ sizeof(SLongOption), "test-locker", B_NO_ARGUMENT, 1000,
		"Test Lock() and Unlock() of SLocker." },
	{ sizeof(SLongOption), "test-autolock", B_NO_ARGUMENT, 1000,
		"Test SAutolock of SLocker." },
	{ sizeof(SLongOption), "test-nested-locker", B_NO_ARGUMENT, 1000,
		"Test Lock() and Unlock() of SNestedLocker." },
	{ sizeof(SLongOption), "test-inc-strong", B_NO_ARGUMENT, 1000,
		"Test IncStrong/DecStrong on a SAtom." },
	{ sizeof(SLongOption), "test-atom-ptr", B_NO_ARGUMENT, 1000,
		"Test creation of smart pointer on a SAtom." },
	{ sizeof(SLongOption), "test-mutex", B_NO_ARGUMENT, 1000,
		"Test locking and unlocking a Kernel mutex." },

	{ sizeof(SLongOption), "test-value-simple", B_NO_ARGUMENT, 1000,
		"Test creation of simple value." },
	{ sizeof(SLongOption), "test-value-join-2", B_NO_ARGUMENT, 1000,
		"Test joining two SValue mappings." },
	{ sizeof(SLongOption), "test-value-join-many", B_NO_ARGUMENT, 1000,
		"Test SValue joins." },
	{ sizeof(SLongOption), "test-value-lookup-int", B_NO_ARGUMENT, 1000,
		"Test looking up an integer in a value." },
	{ sizeof(SLongOption), "test-value-lookup-str", B_NO_ARGUMENT, 1000,
		"Test looking up a string in a value." },
	{ sizeof(SLongOption), "test-value-build-int", B_NO_ARGUMENT, 1000,
		"Test adding integers to an array value." },
	{ sizeof(SLongOption), "test-value-build-str", B_NO_ARGUMENT, 1000,
		"Test adding strings to an array value." },

	{ sizeof(SLongOption), "test-pulse-handler", B_NO_ARGUMENT, 1000,
		"Test messaging with a single BHandler." },
	{ sizeof(SLongOption), "test-pingpong-handler", B_NO_ARGUMENT, 1000,
		"Test messaging alternating two BHandlers." },

	{ sizeof(SLongOption), "test-local-instantiate", B_NO_ARGUMENT, 1000,
		"Test local instantiation of new binder component." },
	{ sizeof(SLongOption), "test-remote-instantiate", B_NO_ARGUMENT, 1000,
		"Test remote instantiation of new binder component." },
	{ sizeof(SLongOption), "test-local-transaction", B_NO_ARGUMENT, 1000,
		"Test BBinder::Transact() to local object." },
	{ sizeof(SLongOption), "test-remote-transaction", B_NO_ARGUMENT, 1000,
		"Test BBinder::Transact() to remote object." },
	{ sizeof(SLongOption), "test-remote-large-transaction", B_NO_ARGUMENT, 1000,
		"Test BBinder::Transact() of large parcels." },
	{ sizeof(SLongOption), "test-local-old-binder", B_NO_ARGUMENT, 1000,
		"Test giving known binder to local object." },
	{ sizeof(SLongOption), "test-remote-old-binder", B_NO_ARGUMENT, 1000,
		"Test giving known binder to remote object." },
	{ sizeof(SLongOption), "test-local-same-binder", B_NO_ARGUMENT, 1000,
		"Test giving unknown binder repeatedly to local object." },
	{ sizeof(SLongOption), "test-remote-same-binder", B_NO_ARGUMENT, 1000,
		"Test giving unknown binder repeatedly to remote object." },
	{ sizeof(SLongOption), "test-local-new-binder", B_NO_ARGUMENT, 1000,
		"Test giving new binder to local object." },
	{ sizeof(SLongOption), "test-remote-new-binder", B_NO_ARGUMENT, 1000,
		"Test giving new binder to remote object." },
	{ sizeof(SLongOption), "test-local-attempt-inc-strong", B_NO_ARGUMENT, 1000,
		"Test AttemptIncStrong on a local SAtom." },
	{ sizeof(SLongOption), "test-remote-attempt-inc-strong", B_NO_ARGUMENT, 1000,
		"Test AttemptIncStrong on a remote SAtom." },
	{ sizeof(SLongOption), "test-multiple-attempt-inc-strong", B_NO_ARGUMENT, 1000,
		"Test remote AttemptIncStrong from multiple threads." },
	{ sizeof(SLongOption), "test-ping-pong-transaction", B_NO_ARGUMENT, 1000,
		"Test ping-pong transaction between multiple processes." },
	{ sizeof(SLongOption), "test-local-effect", B_NO_ARGUMENT, 1000,
		"Test Effect() on a remote binder." },
	{ sizeof(SLongOption), "test-remote-effect", B_NO_ARGUMENT, 1000,
		"Test Effect() on a remote binder." },

	{ sizeof(SLongOption), "test-float-simple", B_NO_ARGUMENT, 1000,
		"Test floating point basic operations." },

	{ sizeof(SLongOption), "libc", B_NO_ARGUMENT, 500,
		"Test some libc functions (memcpy, etc...)." },

	{ sizeof(SLongOption), "test-dm-next", B_NO_ARGUMENT, 1000,
		"Test DmNextDatabaseByTypeCreator()." },
	{ sizeof(SLongOption), "test-dm-info", B_NO_ARGUMENT, 1000,
		"Test DmDatabaseInfo()." },
	{ sizeof(SLongOption), "test-dm-open", B_NO_ARGUMENT, 1000,
		"Test DmOpenDatabase()." },
	{ sizeof(SLongOption), "test-dm-find-resource", B_NO_ARGUMENT, 1000,
		"Test DmFindResource()." },
	{ sizeof(SLongOption), "test-dm-get-resource", B_NO_ARGUMENT, 1000,
		"Test DmGetResource()." },
	{ sizeof(SLongOption), "test-dm-handle-lock", B_NO_ARGUMENT, 1000,
		"Test DmHandleLock()." },
	{ sizeof(SLongOption), "test-db-cursor-open", B_NO_ARGUMENT, 1000,
		"Test DbCursorOpen()." },
	{ sizeof(SLongOption), "test-db-cursor-move", B_NO_ARGUMENT, 1000,
		"Test DbCursorMove()." },
	{ sizeof(SLongOption), "test-db-get-column-value", B_NO_ARGUMENT, 1000,
		"Test DbGetColumnValue()." },
	{ sizeof(SLongOption), "test-dm-is-ready", B_NO_ARGUMENT, 1000,
		"Test DmIsReady()." },
	{ sizeof(SLongOption), "test-dm-read", B_NO_ARGUMENT, 1000,
		"Test Dm Reads." },

	{ sizeof(SLongOption), "icache", B_NO_ARGUMENT, 500,
		"Test icache performance" },

	{ sizeof(SLongOption), NULL, 0, 0, NULL }
};

static const uint64_t optionTests[] =
{
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,

	B_MAKE_UINT64(0xffffffffffffffff),

	// Core
	kAddTestMask | kAtomicOpsTestMask | kFunctionTestMask
		| kSystemTimeCompatTestMask | kSystemTimeTestMask
		| kContextSwitchTestMask | kKeyTestMask
		| kMemPtrNewTestMask | kMemHandleLockTestMask | kMallocTestMask
		| kNewMessageTestMask | kNewBinderTestMask | kNewComponentTestMask
		| kCriticalSectionTestMask | kLockerTestMask | kAutolockTestMask | kNestedLockerTestMask
		| kIncStrongTestMask | kAtomPtrTestMask | kMutexTestMask,

	// Value
	kValueSimpleTestMask | kValueJoin2TestMask | kValueJoinManyTestMask
		| kValueLookupIntTestMask | kValueLookupStrTestMask
		| kValueBuildIntTestMask | kValueBuildStrTestMask,

	// Handler
	kSingleHandlerTestMask | kDoubleHandlerTestMask,

	// Package
	kLocalInstantiateTestMask | kRemoteInstantiateTestMask,

	// Binder
	kLocalTransactionTestMask | kRemoteTransactionTestMask | kRemoteLargeTransactionTestMask
		| kLocalOldBinderTestMask | kRemoteOldBinderTestMask
		| kLocalSameBinderTestMask | kRemoteSameBinderTestMask
		| kLocalNewBinderTestMask | kRemoteNewBinderTestMask
		| kLocalAttemptIncStrongTestMask | kRemoteAttemptIncStrongTestMask
		| kMultipleAttemptIncStrongTestMask
		| kPingPongTransactionMask,

	// Effect
	kLocalEffectIPCTestMask | kRemoteEffectIPCTestMask,

	// Local
	kLocalInstantiateTestMask | kLocalTransactionTestMask
		| kLocalOldBinderTestMask
		| kLocalAttemptIncStrongTestMask
		| kLocalEffectIPCTestMask,

	// Remote
	kRemoteInstantiateTestMask | kRemoteTransactionTestMask | kRemoteLargeTransactionTestMask
		| kRemoteOldBinderTestMask | kRemoteSameBinderTestMask | kRemoteNewBinderTestMask
		| kRemoteAttemptIncStrongTestMask | kMultipleAttemptIncStrongTestMask
		| kPingPongTransactionMask
		| kRemoteEffectIPCTestMask,

	// Float
	kFloatSimpleTestMask,

	// Data Manager
	kDmNextTestMask | kDmInfoTestMask | kDmOpenTestMask
		| kDmFindResourceTestMask | kDmGetResourceTestMask
		| kDmHandleLockTestMask | kDbCursorOpenTestMask
		| kDbCursorMoveTestMask | kDbGetColumnValueMask
		| kDmIsReadyTestMask | kDmReadTestMask,

	// Plain tests...
	kAddTestMask,
	kAtomicOpsTestMask,
	kFunctionTestMask,
	kSystemTimeCompatTestMask,
	kSystemTimeTestMask,
	kContextSwitchTestMask,
	kKeyTestMask,
	kMemPtrNewTestMask,
	kMemHandleLockTestMask,
	kMallocTestMask,
	kNewMessageTestMask,
	kNewBinderTestMask,
	kNewComponentTestMask,
	kCriticalSectionTestMask,
	kLockerTestMask,
	kAutolockTestMask,
	kNestedLockerTestMask,
	kIncStrongTestMask,
	kAtomPtrTestMask,
	kMutexTestMask,

	kValueSimpleTestMask,
	kValueJoin2TestMask,
	kValueJoinManyTestMask,
	kValueLookupIntTestMask,
	kValueLookupStrTestMask,
	kValueBuildIntTestMask,
	kValueBuildStrTestMask,

	kSingleHandlerTestMask,
	kDoubleHandlerTestMask,

	kLocalInstantiateTestMask,
	kRemoteInstantiateTestMask,
	kLocalTransactionTestMask,
	kRemoteTransactionTestMask,
	kRemoteLargeTransactionTestMask,
	kLocalOldBinderTestMask,
	kRemoteOldBinderTestMask,
	kLocalSameBinderTestMask,
	kRemoteSameBinderTestMask,
	kLocalNewBinderTestMask,
	kRemoteNewBinderTestMask,
	kLocalAttemptIncStrongTestMask,
	kRemoteAttemptIncStrongTestMask,
	kMultipleAttemptIncStrongTestMask,
	kPingPongTransactionMask,

	kLocalEffectIPCTestMask,
	kRemoteEffectIPCTestMask,
	
	kFloatSimpleTestMask,

	kLibcTestMask,

	kDmNextTestMask,
	kDmInfoTestMask,
	kDmOpenTestMask,
	kDmFindResourceTestMask,
	kDmGetResourceTestMask,
	kDmHandleLockTestMask,
	kDbCursorOpenTestMask,
	kDbCursorMoveTestMask,
	kDbGetColumnValueMask,
	kDmIsReadyTestMask,
	kDmReadTestMask,

	kICacheTestMask,

	0
};

/* This algorithm is mentioned in the ISO C standard, here extended
   for 32 bits.  */
static int32_t
myrand (uint32_t *seed)
{
  uint32_t next = *seed;
  int32_t result;

  next *= 1103515245;
  next += 12345;
  result = (uint32_t) (next / 65536) % 2048;

  next *= 1103515245;
  next += 12345;
  result <<= 11;
  result ^= (uint32_t) (next / 65536) % 1024;

  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (uint32_t) (next / 65536) % 1024;

  *seed = next;

  return result;
}

class BinderPerformance : public BCommand, public SPackageSptr
{
public:
	BinderPerformance(const SContext& context);

	SValue Run(const ArgList& args);

	static void StartProfiler();
	static void StopProfiler();
	static void ReadEventCounters(uint64_t counters[B_MAX_EVENT_COUNTERS]);
	static status_t GetEventCountName(int counter, char string[B_PERFCNT_MAX_NAME_SIZE]);
	static nsecs_t LoopOverhead();
	static nsecs_t RandomLoopOverhead();

protected:
	virtual ~BinderPerformance();

private:
	SValue RunTests();
	void ComputeOverhead();
	SValue RunAddTest();
	SValue RunAtomicOpsTest();
	SValue RunFunctionTest();
	SValue RunSystemTimeCompatTest();
	SValue RunSystemTimeTest();
	SValue RunContextSwitchTest();
	SValue RunSpawnThreadTest();
	SValue RunKeyTest();
	SValue RunMemPtrNewTest();
	SValue RunMemHandleLockTest();
	SValue RunMallocTest();
	SValue RunNewMessageTest();
	SValue RunNewBinderTest();
	SValue RunNewComponentTest();
	SValue RunCriticalSectionTest();
	SValue RunLockerTest();
	SValue RunAutolockTest();
	SValue RunMutexTest();	
	SValue RunNestedLockerTest();
	SValue RunIncStrongTest();
	SValue RunAtomPtrTest();
	SValue RunValueSimpleTest();
	SValue RunValueJoin2Test();
	SValue RunValueJoinManyTest();
	SValue RunValueIntegerLookupTest();
	SValue RunValueStringLookupTest();
	SValue RunValueIntegerBuildTest(size_t amount);
	SValue RunValueStringBuildTest(size_t amount);
	SValue RunHandlerTest(int32_t num);
	SValue RunInstantiateTest(bool remote);
	SValue RunTransactionTest(bool remote, size_t sizeFactor=1);
	SValue RunPingPongTransactionTest();
	SValue RunFloatSimpleTest();
	SValue RunLibcTest();
	SValue RunEffectIPCTest(bool remote);
	enum {
		kOldBinder, kOldWeakBinder, kWeakToStrongBinder,
		kSameBinder, kSameWeakBinder,
		kNewBinder, kNewWeakBinder
	};
	SValue RunBinderTransferTest(bool remote, int type);
	SValue RunAttemptIncStrongTest(bool remote, int32_t threads = 1);

	SValue RunDmNextTest();
	SValue RunDmInfoTest();
	SValue RunDmOpenTest();
	SValue RunDmFindResourceTest();
	SValue RunDmGetResourceTest();
	SValue RunDmHandleLockTest();
	SValue RunDbCursorOpenTest();
	SValue RunDbCursorMoveTest();
	SValue RunDbGetColumnValueTest();
	SValue RunDmIsReadyTest();
	SValue RunDmReadTest();
	SValue RunICacheTest();
	
	enum {
		MAX_DATA = 4096
	};
	void RestartDataSize();
	int32_t NextDataSize();
	int32_t NextDataSizePreferZero();
	int32_t AvgDataSize();
	int32_t MaxDataSize();

	const sptr<IProcess> BackgroundProcess();
	
	uint64_t m_which;
	int32_t m_iterations;
	int32_t m_priority;
	int32_t m_concurrency;
	int32_t m_dataSize;
	uint32_t m_seed;
	uint32_t m_origSeed;
	bool m_qc;
	bool m_includeLoop;
	bool m_validating;
	bool m_concurrencySpecified;

	static nsecs_t m_loopOverhead;
	static nsecs_t m_randomLoopOverhead;
	static bool m_profile;
	static nsecs_t m_profileRate;
	static int32_t m_profileFD;
	static bool m_use_pmu;
	static int32_t m_pmu_fd;
	static int m_pmu_event_count;
	static uint32_t m_pmu_config[B_MAX_EVENT_COUNTERS];

	sptr<IProcess> m_backgroundProcess;
	SString m_strings[MAX_DATA];
	SValue m_values[MAX_DATA];
};

nsecs_t BinderPerformance::m_loopOverhead = 0;
nsecs_t BinderPerformance::m_randomLoopOverhead = 0;
bool BinderPerformance::m_profile = false;
nsecs_t BinderPerformance::m_profileRate = 0;
int32_t BinderPerformance::m_profileFD = -1;
bool BinderPerformance::m_use_pmu = false;
int32_t BinderPerformance::m_pmu_fd = -1;
int BinderPerformance::m_pmu_event_count = 0;
uint32_t BinderPerformance::m_pmu_config[B_MAX_EVENT_COUNTERS];

BinderPerformance::BinderPerformance(const SContext& context)
	:	BCommand(context),
		m_which(0), m_iterations(1000), m_priority(sysThreadPriorityNormal), m_concurrency(1),
		m_dataSize(128), m_seed(0), m_origSeed(0),
		m_qc(false), m_includeLoop(false), m_validating(false), m_concurrencySpecified(false)
{
	m_loopOverhead = 0;
	m_randomLoopOverhead = 0;
	m_profile = false;
	m_profileRate = 100000;
	m_profileFD = -1;
	m_use_pmu = false;
	m_pmu_fd = -1;
	m_pmu_event_count = 0;
}

BinderPerformance::~BinderPerformance()
{
#if TARGET_HOST == TARGET_HOST_PALMOS
	if (m_profileFD >= 0) {
		IOSClose(m_profileFD);
	}
#endif
}

void BinderPerformance::StartProfiler()
{
	if (m_profile) {
#if TARGET_HOST == TARGET_HOST_PALMOS
		status_t err;
		if (m_profileFD < 0) {
			m_profileFD = IOSOpen(B_PROFILER_DRIVER_NAME, 0, &err);
			bout << "bperf is starting profiler with fd=" << m_profileFD << endl;
			IOSFastIoctl(m_profileFD, profilerStopProfiling, 0, NULL, 0, NULL, &err);
			IOSFastIoctl(m_profileFD, profilerResetSamples, 0, NULL, 0, NULL, &err);
			IOSFastIoctl(m_profileFD, profilerSetSampleRate, sizeof(nsecs_t), &m_profileRate, 0, NULL, &err);
		}
		IOSFastIoctl(m_profileFD, profilerStartProfiling, 0, NULL, 0, NULL, &err);
#elif TARGET_HOST == TARGET_HOST_LINUX
		bout << "bperf is starting profiler" << endl;
		::system("opcontrol --start");
#endif
	}
}

void BinderPerformance::StopProfiler()
{
#if TARGET_HOST == TARGET_HOST_PALMOS
	if (m_profile && m_profileFD >= 0) {
		status_t err;
		IOSFastIoctl(m_profileFD, profilerStopProfiling, 0, NULL, 0, NULL, &err);
	}
#elif TARGET_HOST == TARGET_HOST_LINUX
	if (m_profile) {
		::system("opcontrol --stop");
	}
#endif
}

void BinderPerformance::ReadEventCounters(uint64_t counters[B_MAX_EVENT_COUNTERS])
{
#if TARGET_HOST == TARGET_HOST_PALMOS
	if(m_use_pmu) {
		status_t err;
		if(m_pmu_fd < 0) {
			SContext context = get_default_context(); 

			SIterator dir(context, SString("/dev/" B_PERFORMANCE_COUNTER_CONTEXT_PATH), SValue::Undefined());
			if (B_OK != dir.ErrorCheck()) {
				return ;
			}

			SValue k, v;
			while (B_OK == dir.Next(&k, &v)) {
				SString devName = v.AsString();
				m_pmu_fd = IOSOpen(devName.String(), 0, &err);
				if(m_pmu_fd >= 0)
					break;
			}

			bout << "bperf is starting performance counter with fd=" << m_pmu_fd << endl;
			if(m_pmu_fd < 0) {
				m_use_pmu = false;
				return;
			}
			if(m_pmu_event_count == 0) {
				IOSFastIoctl(  m_pmu_fd, PERFCNT_GET_NUM_ACTIVE_COUNTERS,
				               0, NULL,
				               sizeof(m_pmu_event_count), &m_pmu_event_count,
				               &err );
				if(m_pmu_event_count < 0)
					m_pmu_event_count = 0;
				if(m_pmu_event_count > B_MAX_EVENT_COUNTERS)
					m_pmu_event_count = B_MAX_EVENT_COUNTERS;
				IOSFastIoctl(  m_pmu_fd,
				               PERFCNT_GET_CONFIG(m_pmu_event_count), 0, NULL,
				               sizeof(uint32_t) * m_pmu_event_count, m_pmu_config,
				               &err );
			}
			else {
				IOSFastIoctl(  m_pmu_fd,
				               PERFCNT_CONFIGURE_COUNTERS(m_pmu_event_count),
				               sizeof(uint32_t) * m_pmu_event_count, m_pmu_config,
				               0, NULL, &err );
			}
		}
		IOSFastIoctl(  m_pmu_fd, PERFCNT_READ_COUNTERS(m_pmu_event_count),
		               0, NULL,
		               sizeof(uint64_t) * m_pmu_event_count, counters, &err );
	}
#endif
}

status_t BinderPerformance::GetEventCountName(int counter, char string[B_PERFCNT_MAX_NAME_SIZE])
{
	status_t err = B_ERROR;
#if TARGET_HOST == TARGET_HOST_PALMOS
	if(m_use_pmu && counter < m_pmu_event_count) {
		PerfCntName name;
		name.type = m_pmu_config[counter];
		IOSFastIoctl(  m_pmu_fd, PERFCNT_GET_EVENT_NAME,
		               sizeof(name), &name, sizeof(name), &name, &err );
		if(err == B_OK)
			strcpy(string, name.name);
	}
#endif
	return err;
}

nsecs_t BinderPerformance::LoopOverhead()
{
	return m_loopOverhead;
}

nsecs_t BinderPerformance::RandomLoopOverhead()
{
	return m_randomLoopOverhead;
}

void BinderPerformance::RestartDataSize()
{
	m_seed = m_origSeed;
}

int32_t BinderPerformance::NextDataSize()
{
	if (m_dataSize >= 0) return m_dataSize;
	int32_t size = myrand(&m_seed);
	if (size < 0) size = -size;
	size %= MAX_DATA;
	return size+1;
}

int32_t BinderPerformance::NextDataSizePreferZero()
{
	if (m_qc) return 0;
	return NextDataSize();
}

int32_t BinderPerformance::AvgDataSize()
{
	if (m_dataSize >= 0) return m_dataSize;
	return MAX_DATA/2;
}

int32_t BinderPerformance::MaxDataSize()
{
	if (m_dataSize >= 0) return m_dataSize;
	return MAX_DATA;
}

const sptr<IProcess> BinderPerformance::BackgroundProcess()
{
	if (m_backgroundProcess == NULL) {
		m_backgroundProcess = Context().NewProcess(SString("bperf sub-process"), SContext::REQUIRE_REMOTE);
	}
	return m_backgroundProcess;
}

SValue BinderPerformance::Run(const ArgList& args)
{
	sptr<ITextOutput> out = TextOutput();

	SGetOpts getOpts(options);
	int32_t opt;
	bool helpRequested = false;

	const SString cmdName(args.CountItems() > 0 ? args[0].AsString() : SString());
	
	while ((opt=getOpts.Next(this, args)) >= 0) {
		if (opt < 0) {
			// All done!
			break;
		}
		if (opt == 0) {
			// A plain argument.  This command doesn't have any.
			TextError() << cmdName << ": does not take arguments" << endl;
			break;
		}
		if (optionTests[getOpts.OptionIndex()] != 0) {
			m_which |= optionTests[getOpts.OptionIndex()];
			continue;
		}
		switch(opt) {
			case 'a': {
				m_priority = sysThreadPriorityHigh;
				m_dataSize = -1;
				m_seed = 0;
			} break;
			case 'q': {
				m_priority = sysThreadPriorityHigh;
				m_iterations = 5000;
				m_dataSize = -1;
				m_seed = 0;
				m_qc = true;
			} break;
			case 'i': {
				status_t err;
				m_iterations = getOpts.ArgumentValue().AsInt32(&err);
				if (err != B_OK) {
					TextError() << cmdName << ": integer expected for --iterations" << endl;
					getOpts.TheyNeedHelp();
				}
			} break;
			case 'p': {
				m_priority = -1;
				if (getOpts.ArgumentValue().Type() == B_STRING_TYPE) {
					SString priName = getOpts.ArgumentValue().AsString();
					if (priName.ICompare("low") == 0) m_priority = sysThreadPriorityLow;
					else if (priName.ICompare("normal") == 0) m_priority = sysThreadPriorityNormal;
					else if (priName.ICompare("display") == 0) m_priority = sysThreadPriorityDisplay;
					else if (priName.ICompare("urgentdisplay") == 0) m_priority = sysThreadPriorityUrgentDisplay;
					else if (priName.ICompare("high") == 0) m_priority = sysThreadPriorityHigh;
					else if (priName.ICompare("best") == 0) m_priority = sysThreadPriorityBestUser;
					else if (priName.ICompare("bestuser") == 0) m_priority = sysThreadPriorityBestUser;
					else if (priName.ICompare("bestsystem") == 0) m_priority = sysThreadPriorityBestSystem;
				}
				if (m_priority < 0) {
					status_t err;
					m_iterations = getOpts.ArgumentValue().AsInt32(&err);
					if (err != B_OK) {
						TextError() << cmdName << ": integer expected for --priority" << endl;
						getOpts.TheyNeedHelp();
					}
				}
			} break;
			case 's': {
				status_t err;
				m_dataSize = getOpts.ArgumentValue().AsInt32(&err);
				if (m_dataSize > MAX_DATA) m_dataSize = MAX_DATA;
				if (err != B_OK) {
					SString sizeStr = getOpts.ArgumentValue().AsString();
					if (sizeStr == "random") m_dataSize = -1;
					else if (sizeStr.ICompare("random-", 7) == 0) {
						m_dataSize = -1;
						m_seed = m_origSeed = atoi(sizeStr.String()+7);
					} else {
						TextError() << cmdName << ": integer expected for --size" << endl;
						getOpts.TheyNeedHelp();
					}
				}
			} break;
			case 'v': {
				m_validating = true;
				m_dataSize = -1;
				m_seed = 0;
			} break;
			case 'c':
			case 'j': {
				status_t err;
				m_concurrency = getOpts.ArgumentValue().AsInt32(&err);
				m_concurrencySpecified = true;
				if (err != B_OK) {
					TextError() << cmdName << ": integer expected for --concurrency" << endl;
					getOpts.TheyNeedHelp();
				}
			} break;
			case 'r': {
				m_backgroundProcess = interface_cast<IProcess>(ArgToBinder(getOpts.ArgumentValue()));
				if (m_backgroundProcess == NULL) {
					TextError() << cmdName << ": process expected for --remote" << endl;
					getOpts.TheyNeedHelp();
				}
			} break;
			case kProfile: {
				m_profile = true;
			} break;
			case kProfileRate: {
				m_profile = true;
				status_t err;
				double rate = getOpts.ArgumentValue().AsDouble(&err);
				if (err != B_OK) {
					TextError() << cmdName << ": number expected for --profile-rate" << endl;
					getOpts.TheyNeedHelp();
				} else {
					m_profileRate = (nsecs_t)(rate*1000000);
				}
			} break;
			case kUsePMU: {
				m_use_pmu = true;
			} break;
			case kPMUEventType: {
				m_use_pmu = true;
				status_t err;
				int32_t event_type = getOpts.ArgumentValue().AsInt32(&err);
				if (err != B_OK) {
					TextError() << cmdName << ": number expected for --pmu-event" << endl;
					getOpts.TheyNeedHelp();
				} else {
					if(m_pmu_event_count < B_MAX_EVENT_COUNTERS)
						m_pmu_config[m_pmu_event_count++] = event_type;
				}
			} break;
			case kIncludeLoop: {
				m_includeLoop = true;
			} break;
			case kShowCallStack: {
				SCallStack stack;
				TextOutput() << "Running stack.Update()..." << endl;
				stack.Update();
				TextOutput() << "Stack crawl: ";
				stack.LongPrint(TextOutput());
				TextOutput() << endl;
			} break;
			case 'h': {
				helpRequested = true;
			} break;
			default:
				TextError() << cmdName << ": bad option -- " << (char)opt << endl;
				getOpts.TheyNeedHelp();
				break;
		}
	}

	if (helpRequested) {
		getOpts.PrintHelp(TextError(), args, "Run binder performance tests.");
		return SValue::Status(B_OK);
	}

	if (m_which == 0 && m_qc) m_which = B_MAKE_UINT64(0xffffffffffffffff);

	if (getOpts.IsHelpNeeded() || m_which == 0) {
		getOpts.PrintShortHelp(TextError(), args);
		return SValue::Status(B_BAD_VALUE);
	}

	return RunTests();
}

enum loop_type {
	kNormalLoop,
	kRandomLoop,
	_loop_type_is_an_int32 = 0x10000
};

struct Timer
{
public:
	int32_t N;
	loop_type type;
	nsecs_t start;
	nsecs_t elapsed;
	uint64_t start_event_counts[B_MAX_EVENT_COUNTERS];
	uint64_t end_event_counts[B_MAX_EVENT_COUNTERS];

	Timer(int32_t iterations, loop_type _type = kNormalLoop) : N(iterations), type(_type) { }
	void Start() {
		BinderPerformance::StartProfiler();
		BinderPerformance::ReadEventCounters(start_event_counts);
		start = SysGetRunTime();
	}
	void Stop() {
		nsecs_t end = SysGetRunTime();
		BinderPerformance::ReadEventCounters(end_event_counts);
		BinderPerformance::StopProfiler();
		elapsed = end-start;
		if (type == kRandomLoop) elapsed -= BinderPerformance::RandomLoopOverhead()*N;
		else elapsed -= BinderPerformance::LoopOverhead()*N;
	}
};

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const Timer& t)
{
	char event_name[B_PERFCNT_MAX_NAME_SIZE];
	int i = 0;
#if 0
	io << (double(t.elapsed)/double(t.N)/1000) << "us per op (" << (t.elapsed/1000) << "us for " << t.N << " iterations)";
#else
	io << (double(t.elapsed)/double(t.N)/1000) << "\t" << (t.elapsed/1000) << "\t" << t.N;
#endif
	while((BinderPerformance::GetEventCountName(i, event_name) == B_OK)) {
		io << " " << event_name << " " << t.end_event_counts[i] - t.start_event_counts[i];
		i++;
	}
	return io;
}

void WriteResult(const sptr<ITextOutput>& io, const char* label, const Timer& t)
{
	io << label << "\t" << t << endl;
}

void WriteHeader(const sptr<ITextOutput>& io)
{
	io << "Operation\tus/op\tTotal us\tIterations\n";
}

SValue BinderPerformance::RunTests()
{
	SValue result;
	int32_t i;
	uint8_t oldPri;

#if TARGET_HOST == TARGET_HOST_PALMOS
	{
		KALThreadInfoType info;
		if (KALThreadGetInfo((KeyID)KALTSDGet(kKALTSDSlotIDCurrentThreadKeyID),
				kKALThreadInfoTypeCurrentVersion, &info) == errNone) {
			oldPri = info.currentPriority;
		} else {
			oldPri = 0;
		}
	}
#else
	oldPri = sysThreadPriorityNormal;
#endif

#if (TARGET_HOST == TARGET_HOST_PALMOS) || (TARGET_HOST == TARGET_HOST_LINUX)
	if (SysThreadChangePriority(SysCurrentThread(), (uint8_t)m_priority) != errNone) {
		TextOutput() << "NOTE: Unable to use selected priority, lowering to normal." << endl;
		m_priority = sysThreadPriorityNormal;
		SysThreadChangePriority(SysCurrentThread(), (uint8_t)m_priority);
	}
#endif

	TextOutput() << "Running binder performance tests..." << endl << indent;
	if (m_qc) {
		TextOutput() << "USING QC OPTIONS" << endl;
	}
	if (m_validating) {
		TextOutput() << "VALIDATION ENABLED" << endl;
	}
	TextOutput() << "Iterations: " << m_iterations << endl
		<< "Thread priority: " << m_priority << endl
		<< "Payload size: ";
	if (m_dataSize >= 0) TextOutput() << m_dataSize << endl;
	else TextOutput() << "Random (seed " << m_seed << ")" << endl;
	if (m_profile) {
		TextOutput() << "Profile at rate: " << (double(m_profileRate)/1000000) << "ms ("
			<< m_profileRate << "us)" << endl;
	}
	TextOutput() << dedent << endl;

	for (i=0; i<MAX_DATA; i++) {
		char buf[64];
		static const char* padNums = "abcdefghijklmnopqrstuvwxyz0123456789";
		sprintf(buf, "Str #%d-%s", i, padNums+(i%32));
		m_strings[i] = buf;
		m_values[i] = SValue::String(m_strings[i]);
	}

	WriteHeader(TextOutput());

	if (!m_includeLoop) ComputeOverhead();
	else m_loopOverhead = m_randomLoopOverhead = 0;

	if ((m_which&kAddTestMask) != 0) result.Join(RunAddTest());
	if ((m_which&kAtomicOpsTestMask) != 0) result.Join(RunAtomicOpsTest());
	if (!m_concurrencySpecified && m_concurrency != 2) {
		int32_t oldC = m_concurrency;
		m_concurrency = 2;
		if ((m_which&kAtomicOpsTestMask) != 0) result.Join(RunAtomicOpsTest());
		m_concurrency = oldC;
	}
	if ((m_which&kFunctionTestMask) != 0) result.Join(RunFunctionTest());
	if ((m_which&kSystemTimeCompatTestMask) != 0) result.Join(RunSystemTimeCompatTest());
	if ((m_which&kSystemTimeTestMask) != 0) result.Join(RunSystemTimeTest());
	if ((m_which&kContextSwitchTestMask) != 0) {
		result.Join(RunContextSwitchTest());
		result.Join(RunSpawnThreadTest());
	}
	if ((m_which&kKeyTestMask) != 0) result.Join(RunKeyTest());
	if ((m_which&kMemPtrNewTestMask) != 0) result.Join(RunMemPtrNewTest());
	if ((m_which&kMemHandleLockTestMask) != 0) result.Join(RunMemHandleLockTest());
	if ((m_which&kMallocTestMask) != 0) result.Join(RunMallocTest());
	if ((m_which&kNewMessageTestMask) != 0) result.Join(RunNewMessageTest());
	if ((m_which&kNewBinderTestMask) != 0) result.Join(RunNewBinderTest());
	if ((m_which&kNewComponentTestMask) != 0) result.Join(RunNewComponentTest());
	if ((m_which&kCriticalSectionTestMask) != 0) result.Join(RunCriticalSectionTest());
	if ((m_which&kLockerTestMask) != 0) result.Join(RunLockerTest());
	if ((m_which&kAutolockTestMask) != 0) result.Join(RunAutolockTest());
	if ((m_which&kNestedLockerTestMask) != 0) result.Join(RunNestedLockerTest());
	if ((m_which&kIncStrongTestMask) != 0) result.Join(RunIncStrongTest());
	if ((m_which&kAtomPtrTestMask) != 0) result.Join(RunAtomPtrTest());
	if ((m_which&kMutexTestMask) != 0) result.Join(RunMutexTest());
	if ((m_which&kValueSimpleTestMask) != 0) result.Join(RunValueSimpleTest());
	if ((m_which&kValueJoin2TestMask) != 0) result.Join(RunValueJoin2Test());
	if ((m_which&kValueJoinManyTestMask) != 0) result.Join(RunValueJoinManyTest());
	if ((m_which&kValueLookupIntTestMask) != 0) result.Join(RunValueIntegerLookupTest());
	if ((m_which&kValueLookupStrTestMask) != 0) result.Join(RunValueStringLookupTest());
	if ((m_which&kValueBuildIntTestMask) != 0) {
		result.Join(RunValueIntegerBuildTest(2));
		result.Join(RunValueIntegerBuildTest(10));
		result.Join(RunValueIntegerBuildTest(100));
		result.Join(RunValueIntegerBuildTest(1000));
	}
	if ((m_which&kValueBuildStrTestMask) != 0) {
		result.Join(RunValueStringBuildTest(2));
		result.Join(RunValueStringBuildTest(10));
		result.Join(RunValueStringBuildTest(100));
		result.Join(RunValueStringBuildTest(1000));
	}
	if ((m_which&kFloatSimpleTestMask) != 0) result.Join(RunFloatSimpleTest());
	if ((m_which&kLibcTestMask) != 0) result.Join(RunLibcTest());
	if ((m_which&kSingleHandlerTestMask) != 0) result.Join(RunHandlerTest(1));
	if ((m_which&kDoubleHandlerTestMask) != 0) result.Join(RunHandlerTest(2));
	if ((m_which&kLocalInstantiateTestMask) != 0) result.Join(RunInstantiateTest(false));
	if ((m_which&kRemoteInstantiateTestMask) != 0) result.Join(RunInstantiateTest(true));
	if ((m_which&kLocalTransactionTestMask) != 0) result.Join(RunTransactionTest(false, 0));
	if ((m_which&kLocalTransactionTestMask) != 0) result.Join(RunTransactionTest(false));
	if ((m_which&kRemoteTransactionTestMask) != 0) result.Join(RunTransactionTest(true, 0));
	if ((m_which&kRemoteTransactionTestMask) != 0) result.Join(RunTransactionTest(true));
	if ((m_which&kRemoteLargeTransactionTestMask) != 0) result.Join(RunTransactionTest(true, 20));
	if ((m_which&kLocalOldBinderTestMask) != 0) {
		result.Join(RunBinderTransferTest(false, kOldBinder));
		result.Join(RunBinderTransferTest(false, kOldWeakBinder));
		result.Join(RunBinderTransferTest(false, kWeakToStrongBinder));
	}
	if ((m_which&kRemoteOldBinderTestMask) != 0) {
		result.Join(RunBinderTransferTest(true, kOldBinder));
		result.Join(RunBinderTransferTest(true, kOldWeakBinder));
		result.Join(RunBinderTransferTest(true, kWeakToStrongBinder));
	}
	if ((m_which&kLocalSameBinderTestMask) != 0) {
		result.Join(RunBinderTransferTest(false, kSameBinder));
		result.Join(RunBinderTransferTest(false, kSameWeakBinder));
	}
	if ((m_which&kRemoteSameBinderTestMask) != 0) {
		result.Join(RunBinderTransferTest(true, kSameBinder));
		result.Join(RunBinderTransferTest(true, kSameWeakBinder));
	}
	if ((m_which&kLocalNewBinderTestMask) != 0) {
		result.Join(RunBinderTransferTest(false, kNewBinder));
		result.Join(RunBinderTransferTest(false, kNewWeakBinder));
	}
	if ((m_which&kRemoteNewBinderTestMask) != 0) {
		result.Join(RunBinderTransferTest(true, kNewBinder));
		result.Join(RunBinderTransferTest(true, kNewWeakBinder));
	}
	if ((m_which&kLocalAttemptIncStrongTestMask) != 0) result.Join(RunAttemptIncStrongTest(false));
	if ((m_which&kRemoteAttemptIncStrongTestMask) != 0) result.Join(RunAttemptIncStrongTest(true));
	if ((m_which&kMultipleAttemptIncStrongTestMask) != 0) result.Join(RunAttemptIncStrongTest(true, 2));
	if ((m_which&kPingPongTransactionMask) != 0) result.Join(RunPingPongTransactionTest());

	if ((m_which&kLocalEffectIPCTestMask) != 0) result.Join(RunEffectIPCTest(false));
	if ((m_which&kRemoteEffectIPCTestMask) != 0) result.Join(RunEffectIPCTest(true));

	if ((m_which&kDmNextTestMask) != 0) result.Join(RunDmNextTest());
	if ((m_which&kDmInfoTestMask) != 0) result.Join(RunDmInfoTest());
	if ((m_which&kDmOpenTestMask) != 0) result.Join(RunDmOpenTest());
	if ((m_which&kDmFindResourceTestMask) != 0) result.Join(RunDmFindResourceTest());
	if ((m_which&kDmGetResourceTestMask) != 0) result.Join(RunDmGetResourceTest());
	if ((m_which&kDmHandleLockTestMask) != 0) result.Join(RunDmHandleLockTest());
	if ((m_which&kDbCursorOpenTestMask) != 0) result.Join(RunDbCursorOpenTest());
	if ((m_which&kDbCursorMoveTestMask) != 0) result.Join(RunDbCursorMoveTest());
	if ((m_which&kDbGetColumnValueMask) != 0) result.Join(RunDbGetColumnValueTest());
	if ((m_which&kDmIsReadyTestMask) != 0) result.Join(RunDmIsReadyTest());
	if ((m_which&kDmReadTestMask) != 0) result.Join(RunDmReadTest());

	if ((m_which&kICacheTestMask) != 0) result.Join(RunICacheTest());

	if ((m_which&kICacheTestMask) != 0) result.Join(RunICacheTest());

	//TextOutput() << "Done with tests...  m_profile=" << m_profile
	//	<< ", m_profileFD=" << m_profileFD << endl;

#if TARGET_HOST == TARGET_HOST_PALMOS
	if (m_profile && m_profileFD >= 0) {
		TextOutput() << "Sending profile results to PUD..." << endl;
		status_t err;
		IOSFastIoctl(m_profileFD, profilerSendSamplesToPOD, 0, NULL, 0, NULL, &err);
	}
#endif

#if (TARGET_HOST == TARGET_HOST_PALMOS) || (TARGET_HOST == TARGET_HOST_LINUX)
	if (oldPri != 0)
		SysThreadChangePriority(SysCurrentThread(), oldPri);
#endif

	return result;
}

void BinderPerformance::ComputeOverhead()
{
	m_loopOverhead = 0;
	m_randomLoopOverhead = 0;

	Timer t(m_iterations*10000);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
	}
	t.Stop();

	WriteResult(TextOutput(), "Overhead", t);

	nsecs_t loopOverhead = t.elapsed/t.N;

	t.N = m_iterations*1000;

	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		NextDataSize();
	}
	t.Stop();

	WriteResult(TextOutput(), "Random Overhead", t);

	m_loopOverhead = loopOverhead;
	m_randomLoopOverhead = t.elapsed/t.N;
}

static volatile float dummyFloat = 0.0f;
static volatile float initialFloat = 10.0f;

SValue BinderPerformance::RunFloatSimpleTest()
{
	Timer t1(m_iterations*1000);
	Timer t2(m_iterations*100);
	float c;

	c = 0;
	t1.Start();
	for (int32_t i=0; i<t1.N; i++) {
		c = c + 1.0f;	c = c + (-1.0f);
		c = c + 1.0f;	c = c + (-1.0f);
		c = c + 1.0f;	c = c + (-1.0f);
		c = c + 1.0f;	c = c + (-1.0f);
		c = c + 1.0f;	c = c + (-1.0f);
	}
	t1.Stop();
	dummyFloat = c;
	WriteResult(TextOutput(), "fadd", t1);

	c = 1;
	t1.Start();
	for (int32_t i=0; i<t1.N; i++) {
		c = c * 10.0f;	c = c * .1f;
		c = c * 10.0f;	c = c * .1f;
		c = c * 10.0f;	c = c * .1f;
		c = c * 10.0f;	c = c * .1f;
		c = c * 10.0f;	c = c * .1f;
	}
	t1.Stop();
	dummyFloat = c;
	WriteResult(TextOutput(), "fmul", t1);

	c = 1;
	t1.Start();
	for (int32_t i=0; i<t1.N; i++) {
		c = c / 10.0f;	c = c / .1f;
		c = c / 10.0f;	c = c / .1f;
		c = c / 10.0f;	c = c / .1f;
		c = c / 10.0f;	c = c / .1f;
		c = c / 10.0f;	c = c / .1f;
	}
	t1.Stop();
	dummyFloat = c;
	WriteResult(TextOutput(), "fdiv", t1);

#if TEST_PALMOS_APIS
	c = 2;
	t2.Start();
	for (int32_t i=0; i<t2.N; i++) {
		c = (float)sqrt(initialFloat);
		c = (float)sqrt(initialFloat);
		c = (float)sqrt(initialFloat);
		c = (float)sqrt(initialFloat);
		c = (float)sqrt(initialFloat);
		c = (float)sqrt(initialFloat);
		c = (float)sqrt(initialFloat);
		c = (float)sqrt(initialFloat);
		c = (float)sqrt(initialFloat);
		c = (float)sqrt(initialFloat);
	}
	t2.Stop();
	(void)c;
	WriteResult(TextOutput(), "sqrt", t2);

	c = 2;
	t2.Start();
	for (int32_t i=0; i<t2.N; i++) {
		c = 1.0f/(float)sqrt(initialFloat);
		c = 1.0f/(float)sqrt(initialFloat);
		c = 1.0f/(float)sqrt(initialFloat);
		c = 1.0f/(float)sqrt(initialFloat);
		c = 1.0f/(float)sqrt(initialFloat);
		c = 1.0f/(float)sqrt(initialFloat);
		c = 1.0f/(float)sqrt(initialFloat);
		c = 1.0f/(float)sqrt(initialFloat);
		c = 1.0f/(float)sqrt(initialFloat);
		c = 1.0f/(float)sqrt(initialFloat);
	}
	t2.Stop();
	(void)c;
	WriteResult(TextOutput(), "1/sqrt", t2);

	c = 2;
	t2.Start();
	for (int32_t i=0; i<t2.N; i++) {
		c = inv_sqrtf(initialFloat);
		c = inv_sqrtf(initialFloat);
		c = inv_sqrtf(initialFloat);
		c = inv_sqrtf(initialFloat);
		c = inv_sqrtf(initialFloat);
		c = inv_sqrtf(initialFloat);
		c = inv_sqrtf(initialFloat);
		c = inv_sqrtf(initialFloat);
		c = inv_sqrtf(initialFloat);
		c = inv_sqrtf(initialFloat);
	}
	t2.Stop();
	(void)c;
	WriteResult(TextOutput(), "inv_sqrtf", t2);
#endif

	return SValue::Status(B_OK);
}


SValue BinderPerformance::RunLibcTest()
{
	size_t const size = 1024*1024;
	void * r;
	void * const buffer = malloc(size+128*32+32+32);
	const void *src = (const void *)((((uint32_t)buffer + 31)&~31) + 8);
	void *dst = (int8_t*)src + size/2 + 127*32 + 4;

	if (!buffer) {
		return SValue::Int32(B_NO_MEMORY);
	}

	// Testing correctness of the return value
	r = memcpy(dst, src, 16);
	if (r == dst)	TextOutput() << "ARM's memcpy(16 bytes) return value is correct" << endl;
	else			TextOutput() << "ARM's memcpy(16 bytes) return value is incorrect : " << r << " instead of " << dst << endl;
#if __CC_ARM
	r = _PalmOS_memcpy(dst, src, 16);
	if (r == dst)	TextOutput() << "PalmOS's memcpy(16 bytes) return value is correct" << endl;
	else			TextOutput() << "PalmOS's memcpy(16 bytes) return value is incorrect : " << r << " instead of " << dst << endl;
#endif

	// performance
	Timer t(m_iterations);

	size_t const memcpy_sizes[] = { 1, 4, 8, 16,  32, 64, 128, 256,
						1024, 2048, 4096, 8192, 12288,
						32768, 49152,65536, 128*1024, 512*1024 };
	size_t const strlen_sizes[] = { 0, 1, 5, 6, 11, 17, 28, 45, 73, 118, 291, 409, 700, 1109, 1809, 2918, 4727 };
	size_t const strlen_alignments[] = { 0, 1, 4, 5 };

	size_t const num_memcpy_sizes = sizeof(memcpy_sizes)/sizeof(memcpy_sizes[0]);
	size_t const num_strlen_sizes = sizeof(strlen_sizes)/sizeof(strlen_sizes[0]);
	size_t const num_strlen_alignments = sizeof(strlen_alignments)/sizeof(strlen_alignments[0]);

	// memcpy tests

	TextOutput() << "ARM's memcpy (congruent), MB/s " << endl;
	for (size_t j=0 ; j<num_memcpy_sizes ; j++) {
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			memcpy(dst, src, memcpy_sizes[j]);
		}
		t.Stop();
		float result = (((((float)t.N*memcpy_sizes[j])/(1024*1024))/t.elapsed)*B_ONE_SECOND);
		TextOutput() << memcpy_sizes[j] << "\t" << result << endl;
	}

	TextOutput() << "PalmOS's memcpy (congruent), MB/s " << endl;
	for (size_t j=0 ; j<num_memcpy_sizes ; j++) {
#if __CC_ARM
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			_PalmOS_memcpy(dst, src, memcpy_sizes[j]);
		}
		t.Stop();
		float result = (((((float)t.N*memcpy_sizes[j])/(1024*1024))/t.elapsed)*B_ONE_SECOND);
		TextOutput() << memcpy_sizes[j] << "\t" << result << endl;
#endif
	}

	TextOutput() << "ARM's memcpy (non congruent), MB/s " << endl;
	for (size_t j=0 ; j<num_memcpy_sizes ; j++) {
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			memcpy(dst, (char*)src+1, memcpy_sizes[j]);
		}
		t.Stop();
		float result = (((((float)t.N*memcpy_sizes[j])/(1024*1024))/t.elapsed)*B_ONE_SECOND);
		TextOutput() << memcpy_sizes[j] << "\t" << result << endl;
	}

	TextOutput() << "PalmOS's memcpy (non congruent), MB/s " << endl;
	for (size_t j=0 ; j<num_memcpy_sizes ; j++) {
#if __CC_ARM
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			_PalmOS_memcpy(dst, (char*)src+1, memcpy_sizes[j]);
		}
		t.Stop();
		float result = (((((float)t.N*memcpy_sizes[j])/(1024*1024))/t.elapsed)*B_ONE_SECOND);
		TextOutput() << memcpy_sizes[j] << "\t" << result << endl;
#endif
	}

	// strlen uncached tests
	for(size_t i= 0; i< num_strlen_alignments; i++) {
		char *bbb= reinterpret_cast<char*>((31+uint32_t(buffer))&~31);
		char *base_ptr= bbb+strlen_alignments[i];

		TextOutput() << "strlen (uncached, alignment=" << strlen_alignments[i] << "), MB/s " << endl;
		for(size_t j= 0; j< num_strlen_sizes; j++) {
			size_t stride= (1+strlen_sizes[j]);

			// prepare the buffer
			memset(bbb, 0, size);
			for(size_t k= strlen_alignments[i]; k< size; k+= stride) {
				memset(bbb+k, 'a', strlen_sizes[j]);
			}

			Timer t(m_iterations/50);
			t.Start();
			for (int32_t k=0; k<t.N; k++) {
				for(size_t m= strlen_alignments[i]; m< size; m+= stride) {
					strlen(base_ptr+m);
				}
			}
			t.Stop();
			float result = (((((float)t.N*size)/(1024*1024))/t.elapsed)*B_ONE_SECOND);
			TextOutput() << strlen_sizes[j] << "\t" << result << endl;
		}
	}

	// strlen cached tests
	for(size_t i= 0; i< num_strlen_alignments; i++) {
		char *bbb= reinterpret_cast<char*>((31+uint32_t(buffer))&~31);
		char *base_ptr= bbb+strlen_alignments[i];

		TextOutput() << "strlen (cached, alignment=" << strlen_alignments[i] << "), MB/s " << endl;
		for(size_t j= 0; j< num_strlen_sizes; j++) {
			// prepare the buffer
			memset(bbb, 0, size);
			memset(bbb+strlen_alignments[i], 'a', strlen_sizes[j]);

			Timer t(m_iterations/50);
			t.Start();
			int32_t iters= t.N*size/(1+strlen_sizes[j]);
			for (int32_t k=0; k< iters; k++) {
				strlen(base_ptr+strlen_alignments[i]);
			}
			t.Stop();
			float result = (((((float)t.N*size)/(1024*1024))/t.elapsed)*B_ONE_SECOND);
			TextOutput() << strlen_sizes[j] << "\t" << result << endl;
		}
	}

	// clean up after the party
	free(buffer);
	return SValue::Status(B_OK);
}

static volatile int32_t dummyInt = 0;
extern volatile int32_t g_externInt; // see EffectIPC.cpp

SValue BinderPerformance::RunAddTest()
{
	Timer t(m_iterations*10000);
	volatile int32_t val = 0;
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		val += 1;
	}
	dummyInt = val;
	t.Stop();

	WriteResult(TextOutput(), "Add int32_t", t);

	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		val = dummyInt;
	}
	g_externInt = val;
	t.Stop();

	WriteResult(TextOutput(), "Global int32_t Access", t);

	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		val = g_externInt;
	}
	dummyInt = val;
	t.Stop();

	WriteResult(TextOutput(), "Extern Global int32_t Access", t);

	return SValue::Status(B_OK);
}

sysThreadDirectFuncs g_threadDirectFuncs;

struct atomic_op_state
{
	int32_t N;
	uint32_t whichTest;
	volatile int32_t val;
	SysTSDSlotID tsd;
	volatile int32_t numReady;
	SConditionVariable ready;
	SConditionVariable start;
	volatile int32_t numRunning;
	SConditionVariable finished;
	SConditionVariable exit;
};

struct atomic_op_thread_state
{
	atomic_op_state* state;
	int32_t dir;
};

static const char* kAtomicOpTestNames[] = {
	"SysAtomicInc32()",
	"sysThreadDirectFuncs.atomicInc32()",
	"SysAtomicDec32()",
	"sysThreadDirectFuncs.atomicDec32()",
	"SysAtomicAdd32()",
	"sysThreadDirectFuncs.atomicAdd32()",
	"SysAtomicCompareAndSwap32()",
	"sysThreadDirectFuncs.atomicCompareAndSwap32()",
	"SysTSDGet()",
	"sysThreadDirectFuncs.tsdGet()",
	"SysTSDSet()",
	"sysThreadDirectFuncs.tsdSet()",
	NULL
};

enum {
	kDirectionNone = 0,
	kDirectionInc,
	kDirectionDec,
	kDirectionIncDec
};

static int32_t kAtomicOpTestDir[] = {
	kDirectionInc,
	kDirectionInc,
	kDirectionDec,
	kDirectionDec,
	kDirectionIncDec,
	kDirectionIncDec,
	kDirectionNone,		// not clear what the final value will be.
	kDirectionNone,		// not clear what the final value will be.
	kDirectionNone,
	kDirectionNone,
	kDirectionNone,
	kDirectionNone
};

static void run_atomic_op_test(atomic_op_thread_state& tstate)
{
	atomic_op_state& state(*tstate.state);
	
	switch (state.whichTest) {
	case 0:
	{
		for (int32_t i=0; i<state.N; i++) {
			SysAtomicInc32(&state.val);
		}
	} break;

	case 1:
	{
		for (int32_t i=0; i<state.N; i++) {
			g_threadDirectFuncs.atomicInc32(&state.val);
		}
	} break;

	case 2:
	{
		for (int32_t i=0; i<state.N; i++) {
			SysAtomicDec32(&state.val);
		}
	} break;

	case 3:
	{
		for (int32_t i=0; i<state.N; i++) {
			g_threadDirectFuncs.atomicDec32(&state.val);
		}
	} break;

	case 4:
	{
		const int32_t dir=tstate.dir;
		for (int32_t i=0; i<state.N; i++) {
			SysAtomicAdd32(&state.val, dir);
		}
	} break;

	case 5:
	{
		const int32_t dir=tstate.dir;
		for (int32_t i=0; i<state.N; i++) {
			g_threadDirectFuncs.atomicAdd32(&state.val, dir);
		}
	} break;

	case 6:
	{
		const int32_t dir=tstate.dir;
		for (int32_t i=0; i<state.N; i++) {
			SysAtomicCompareAndSwap32((volatile uint32_t*)&state.val, i, i+dir);
		}
	} break;

	case 7:
	{
		const int32_t dir=tstate.dir;
		for (int32_t i=0; i<state.N; i++) {
			g_threadDirectFuncs.atomicCompareAndSwap32((volatile uint32_t*)&state.val, i, i+dir);
		}
	} break;

	case 8:
	{
		for (int32_t i=0; i<state.N; i++) {
			state.val = (int32_t)SysTSDGet(state.tsd);
		}
	} break;

	case 9:
	{
		for (int32_t i=0; i<state.N; i++) {
			state.val = (int32_t)g_threadDirectFuncs.tsdGet(state.tsd);
		}
	} break;

	case 10:
	{
		for (int32_t i=0; i<state.N; i++) {
			SysTSDSet(state.tsd, (void*)state.val);
		}
	} break;

	case 11:
	{
		for (int32_t i=0; i<state.N; i++) {
			g_threadDirectFuncs.tsdSet(state.tsd, (void*)state.val);
		}
	} break;
	}
}

static void atomic_op_func(void* argument)
{
	atomic_op_thread_state& tstate = *(atomic_op_thread_state*)argument;
	atomic_op_state& state = *tstate.state;
	
	//bout << "Thread " << SysCurrentThread() << " has entered." << endl;
	
	// Handshaking to keep as much work out of the timed
	// portion as possible.
	if (SysAtomicDec32(&state.numReady) == 1) state.ready.Open();
	state.start.Wait();

	run_atomic_op_test(tstate);
	
	// Now that we are done, decrement the number of running threads
	// and wake up the main thread when the last one finishes.
	if (SysAtomicDec32(&state.numRunning) == 1) state.finished.Open();
	
	// Wait for all threads to be finished before exiting.
	state.exit.Wait();

	//bout << "Thread " << SysCurrentThread() << " is leaving." << endl;
}

SValue BinderPerformance::RunAtomicOpsTest()
{
	SValue result(SValue::Status(B_OK));
	
	int32_t numThreads = m_concurrency;
	
	g_threadDirectFuncs.numFuncs = sysThreadDirectFuncsCount;
	SysThreadGetDirectFuncs(&g_threadDirectFuncs);

	atomic_op_state state;
	state.N = (m_iterations*10000)/numThreads;
	SysTSDAllocate(&state.tsd, NULL, sysTSDAnonymous);
	
	// This initial value is used so that, in the compare and swap
	// test, 1/2 of the time the compare fails, the other half it succeeds.
	const int32_t initialVal = state.N + state.N/2;

	uint32_t i=0;
	const char** testName=kAtomicOpTestNames;
	while (*testName != NULL) {
		
		state.whichTest = i;
		state.val = initialVal;
		state.numReady = numThreads;
		state.ready.Close();
		state.start.Close();
		state.numRunning = numThreads;
		state.finished.Close();
		state.exit.Close();
		
		Timer t(state.N*numThreads);
		if (numThreads == 1) {
			// For the case of no concurrency, we will try to be a little more accurate.
			atomic_op_thread_state tstate;
			tstate.state = &state;
			tstate.dir = 1;
			t.Start();
			run_atomic_op_test(tstate);
			t.Stop();
		} else {
			SVector<SysHandle> threads;
			SVector<atomic_op_thread_state> tstates;
			threads.SetSize(numThreads);
			tstates.SetSize(numThreads);
			for (int32_t j=0; j<numThreads; j++) {
				atomic_op_thread_state& tstate = tstates.EditItemAt(j);
				tstate.state = &state;
				tstate.dir = (j&1) ? 1 : -1;
				SysHandle& h = threads.EditItemAt(j);
				status_t err = SysThreadCreate(NULL, "Atomic op test thread",
											(uint8_t)m_priority, sysThreadStackBasic,
											atomic_op_func, &tstate, &h);
				if (err != errNone) {
					// XXX May leak threads.
					SValue result;
					result.SetError(err);
					return result;
				}
				SysThreadStart(h);
			}
			
			// Wait for all threads to get up and running.
			state.ready.Wait();
			
			// Start timer and run the test.
			t.Start();
			state.start.Open();
			
			// Wait for all threads to finish the test.
			state.finished.Wait();
			t.Stop();
			
			// Let the threads go away.
			state.exit.Open();
			SysThreadDelay(B_MS2NS(100), B_RELATIVE_TIMEOUT);
		}
		
		SString label;
		if (numThreads > 1) {
			label << numThreads;
			label << "x-";
		}
		label << *testName;
		
		int32_t expecting = -1;
		if (m_validating) {
			switch (kAtomicOpTestDir[i]) {
			case kDirectionInc:		expecting = initialVal + (state.N*numThreads);		break;
			case kDirectionDec:		expecting = initialVal - (state.N*numThreads);		break;
			case kDirectionIncDec:	expecting = initialVal + (state.N*(numThreads&1));	break;
			}
		}
		
		if (expecting > 0 && expecting != state.val) {
			TextOutput() << "Failed " << label << ": have " << state.val << ", expecting " << expecting << endl;
			result.Join(SValue::Status(B_BAD_VALUE));
		} else {
			WriteResult(TextOutput(), label.String(), t);
		}
		
		i++;
		testName++;
	}

	SysTSDFree(state.tsd);
		
#if 0	
	{
		Timer t(m_iterations*10000);
		t.Start();
		volatile int32_t val = 0;
		for (int32_t i=0; i<t.N; i++) {
			SysAtomicInc32(&val);
		}
		t.Stop();

		WriteResult(TextOutput(), "SysAtomicInc32()", t);
	}

	{
		Timer t(m_iterations*10000);
		t.Start();
		volatile int32_t val = 0;
		for (int32_t i=0; i<t.N; i++) {
			g_threadDirectFuncs.atomicInc32(&val);
		}
		t.Stop();

		WriteResult(TextOutput(), "sysThreadDirectFuncs.atomicInc32()", t);
	}

	{
		Timer t(m_iterations*10000);
		volatile int32_t val = 0;
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SysAtomicDec32(&val);
		}
		t.Stop();

		WriteResult(TextOutput(), "SysAtomicDec32()", t);
	}

	{
		Timer t(m_iterations*10000);
		t.Start();
		volatile int32_t val = 0;
		for (int32_t i=0; i<t.N; i++) {
			g_threadDirectFuncs.atomicDec32(&val);
		}
		t.Stop();

		WriteResult(TextOutput(), "sysThreadDirectFuncs.atomicDec32()", t);
	}

	{
		Timer t(m_iterations*10000);
		volatile int32_t val = 0;
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SysAtomicAdd32(&val, 1);
		}
		t.Stop();

		WriteResult(TextOutput(), "SysAtomicAdd32()", t);
	}

	{
		Timer t(m_iterations*10000);
		volatile int32_t val = 0;
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			g_threadDirectFuncs.atomicAdd32(&val, 1);
		}
		t.Stop();

		WriteResult(TextOutput(), "sysThreadDirectFuncs.atomicAdd32()", t);
	}

	{
		Timer t(m_iterations*10000);
		// initialize value so 1/2 of the time the compare
		// fails, the other half it succeeds.
		volatile uint32_t val = t.N/2;
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SysAtomicCompareAndSwap32(&val, i, i+1);
		}
		t.Stop();

		WriteResult(TextOutput(), "SysAtomicCompareAndSwap32()", t);
	}

	{
		Timer t(m_iterations*10000);
		// initialize value so 1/2 of the time the compare
		// fails, the other half it succeeds.
		volatile uint32_t val = t.N/2;
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			g_threadDirectFuncs.atomicCompareAndSwap32(&val, i, i+1);
		}
		t.Stop();

		WriteResult(TextOutput(), "sysThreadDirectFuncs.atomicCompareAndSwap32()", t);
	}

	{
		Timer t(m_iterations*10000);
		SysTSDSlotID tsd;
		SysTSDAllocate(&tsd, NULL, sysTSDAnonymous);
		void* volatile val = NULL;
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			val = SysTSDGet(tsd);
		}
		t.Stop();
		SysTSDFree(tsd);

		WriteResult(TextOutput(), "SysTSDGet()", t);
	}

	{
		Timer t(m_iterations*10000);
		SysTSDSlotID tsd;
		SysTSDAllocate(&tsd, NULL, sysTSDAnonymous);
		void* volatile val = NULL;
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			val = g_threadDirectFuncs.tsdGet(tsd);
		}
		t.Stop();
		SysTSDFree(tsd);

		WriteResult(TextOutput(), "sysThreadDirectFuncs.tsdGet()", t);
	}

	{
		Timer t(m_iterations*10000);
		SysTSDSlotID tsd;
		SysTSDAllocate(&tsd, NULL, sysTSDAnonymous);
		void* volatile val = NULL;
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SysTSDSet(tsd, val);
		}
		t.Stop();
		SysTSDFree(tsd);

		WriteResult(TextOutput(), "SysTSDFree()", t);
	}

	{
		Timer t(m_iterations*10000);
		SysTSDSlotID tsd;
		SysTSDAllocate(&tsd, NULL, sysTSDAnonymous);
		void* volatile val = NULL;
		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			g_threadDirectFuncs.tsdSet(tsd, val);
		}
		t.Stop();
		SysTSDFree(tsd);

		WriteResult(TextOutput(), "sysThreadDirectFuncs.tsdSet()", t);
	}
#endif

	return result;
}

SValue BinderPerformance::RunFunctionTest()
{
	ALocalClass* alc = new ALocalClass;
#if TEST_PALMOS_APIS
	BUISpecialEffects* fx = new BUISpecialEffects;
	ADerivedClass* adc = new ADerivedClass;
#endif
	void (*fp)(void) = ALocalFunction;
	void (*fpa)(void*) = ALocalFunctionWithParam;

	Timer t(m_iterations*10000);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		ALocalFunction();
	}
	t.Stop();

	WriteResult(TextOutput(), "Local Function", t);

	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		ALocalFunctionWithParam(alc);
	}
	t.Stop();

	WriteResult(TextOutput(), "Local Param Function", t);

#if TEST_PALMOS_APIS
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
#if !LINUX_DEMO_HACK
		WinTheRatRace();
#endif
	}
	t.Stop();
#endif

	WriteResult(TextOutput(), "Library Function", t);

	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		fp();
	}
	t.Stop();

	WriteResult(TextOutput(), "Function Pointer", t);

	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		fpa(alc);
	}
	t.Stop();

	WriteResult(TextOutput(), "Function Pointer With Param", t);

	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		alc->ThisCouldDoSomething();
	}
	t.Stop();

	WriteResult(TextOutput(), "Virtual Function", t);
	delete alc;

#if TEST_PALMOS_APIS
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		fx->ExplodeDevice();
	}
	t.Stop();

	WriteResult(TextOutput(), "Library Virtual Function", t);
	delete fx;

	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		adc->ExplodeDevice();
	}
	t.Stop();

	WriteResult(TextOutput(), "Library Derived Function", t);
	delete adc;
#endif

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunSystemTimeCompatTest()
{
	Timer t(m_iterations*100);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		SysGetRunTime();
	}
	t.Stop();

	WriteResult(TextOutput(), "SysGetRunTime()", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunSystemTimeTest()
{
	Timer t(m_iterations*100);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		SysGetRunTime();
	}
	t.Stop();

	WriteResult(TextOutput(), "SysGetRunTime()", t);

	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		SysGetRunTime();
	}
	t.Stop();

	WriteResult(TextOutput(), "SysGetRunTime()", t);

	return SValue::Status(B_OK);
}

struct context_switch_state
{
	int32_t N;
	int32_t i;
	SysHandle t1;
	SysHandle t2;
	SConditionVariable c1;
	SConditionVariable c2;
	volatile int32_t numRunning;
	SConditionVariable finished;
};

static void context_switch_func(void* argument)
{
	context_switch_state* state = (context_switch_state*)argument;
	const SysHandle self = SysCurrentThread();
	SConditionVariable* selfC;
	SConditionVariable* otherC;

	// XXX Note: should use a lower-level API instead of
	// SConditionVariable for causing the context switch, to
	// introduce as little overhead as possible.  Also really
	// need a version that context switches between processes.
	
	// Second thread must wait for first to wake it up.
	if (self == state->t2) {
		selfC = &state->c2;
		otherC = &state->c1;
		selfC->Wait();
	} else {
		selfC = &state->c1;
		otherC = &state->c2;
	}

	while (++state->i < state->N) {
		selfC->Close();
		otherC->Open();
		selfC->Wait();
	}

	selfC->Open();
	otherC->Open();
	if (SysAtomicDec32(&state->numRunning) == 1)
		state->finished.Open();
}

SValue BinderPerformance::RunContextSwitchTest()
{
	Timer t(m_iterations*10);
	context_switch_state state;
	state.N = t.N;
	state.i = 0;
	state.c1.Close();
	state.c2.Close();
	state.numRunning = 2;
	status_t err = SysThreadCreate(NULL, "Context Switch Test 1",
								(uint8_t)m_priority, sysThreadStackBasic,
								context_switch_func, &state, &state.t1);
	if (err != errNone) {
		SValue result;
		result.SetError(err);
		return result;
	}
	err = SysThreadCreate(NULL, "Context Switch Test 2",
								(uint8_t)m_priority, sysThreadStackBasic,
								context_switch_func, &state, &state.t2);
	if (err != errNone) {
		SValue result;
		result.SetError(err);
		return result;
	}

	state.finished.Close();
	t.Start();

	SysThreadStart(state.t1);
	SysThreadStart(state.t2);

	state.finished.Wait();
	t.Stop();

	WriteResult(TextOutput(), "Context Switch", t);

	return SValue::Status(B_OK);
}

static void spawn_thread_func(void* argument)
{
	context_switch_state* state = (context_switch_state*)argument;
	state->finished.Open();
}


static status_t do_one_spawn(context_switch_state& state, uint8_t pri)
{
	state.finished.Close();
	status_t err = SysThreadCreate(NULL, "tst1 Thread Spawn Test",
								pri, sysThreadStackBasic,
								spawn_thread_func, &state, &state.t1);
	if (err != errNone) return err;

	SysThreadStart(state.t1);
	state.finished.Wait();

	return errNone;
}

SValue BinderPerformance::RunSpawnThreadTest()
{
	// This can take a huge amount of time if connected to the
	// debugger, so try to account  for that.

	context_switch_state state;
	float factor = .01;

	{
		Timer t(m_iterations);

		t.Start();
		for (int32_t i=0; i<5; i++) {
			do_one_spawn(state, (uint8_t)m_priority);
		}
		t.Stop();

		// If it took more than 5000 us, assume we are talking
		// with the debugger.
		if (t.elapsed > B_US2NS(5000*5)) factor = .001;
	}

	{
		Timer t(int32_t(m_iterations*factor));

		t.Start();
		int32_t i;
		for (i=0; i<t.N; i++) {
			if (do_one_spawn(state, (uint8_t)m_priority) != errNone) {
				TextOutput() << "Error spawning thread!" << endl;
			}
		}
		t.Stop();

		WriteResult(TextOutput(), "Spawn+Destroy Thread", t);
	}

	return SValue::Status(B_OK);
}

#if TARGET_HOST == TARGET_HOST_PALMOS

SValue BinderPerformance::RunKeyTest()
{
	int32_t N = MaxDataSize();
	if (N < 16) N = 16;
	if (N > 1024) N = 1024;

	KeyID* keys = (KeyID*)malloc(sizeof(KeyID)*N);
	int32_t i;
	for (i=0; i<N; i++) keys[i] = kKALKeyIDNull;

	RestartDataSize();
	Timer t(m_iterations*100);
	t.Start();
	for (i=0; i<t.N; i++) {
		int32_t j = myrand(&m_seed);
		if (j < 0) j = -j;
		j %= N;
		if (keys[j] == kKALKeyIDNull) {
			KALProcessAllocateKeyVariable(kKALKeyIDProcess, &keys[j]);
		} else {
			KALProcessFreeKeyVariable(kKALKeyIDProcess, keys[j]);
			keys[j]= kKALKeyIDNull;
		}
	}
	t.Stop();

	for (i=0; i<N; i++) {
		if (keys[i] != kKALKeyIDNull) KALProcessFreeKeyVariable(kKALKeyIDProcess, keys[i]);
	}
	free(keys);

	WriteResult(TextOutput(), "Key Allocation", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunMutexTest()
{
	status_t err;
	KeyID mutex;

	err = KALProcessAllocateKeyVariable(kKALKeyIDProcess, &mutex);
	if (err) {
		return SValue::Int32(err);
	}

	err = KALMutexCreate(kKeyIDResourceBank, kKALSortByFIFO, mutex);
	if (err) {
		return SValue::Int32(err);
	}

	RestartDataSize();
	Timer t(m_iterations*100);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		err = KALMutexTimedLock(mutex, B_WAIT_FOREVER, 0);
		if (err) {
			return SValue::Int32(err);
		}
		err = KALMutexUnlock(mutex);
		if (err) {
			return SValue::Int32(err);
		}
	}
	t.Stop();

	KALMutexDestroy(mutex);
	KALProcessFreeKeyVariable(kKALKeyIDProcess, mutex);

	WriteResult(TextOutput(), "KALMutex Lock (pair)", t);

	return SValue::Status(B_OK);
}

#else //  TARGET_HOST == TARGET_HOST_PALMOS

SValue BinderPerformance::RunKeyTest()
{
	TextOutput() << "Key Allocation: only implemented on PalmOS" << endl;
	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunMutexTest()
{
	TextOutput() << "Mutex test: only implemented on PalmOS" << endl;
	return SValue::Status(B_OK);
}

#endif //  TARGET_HOST == TARGET_HOST_PALMOS

SValue BinderPerformance::RunMemPtrNewTest()
{
#if TEST_PALMOS_APIS
	RestartDataSize();
	Timer t(m_iterations*100, kRandomLoop);
	t.Start();
	void* lastMem = NULL;
	for (int32_t i=0; i<t.N; i++) {
		void* mem = MemPtrNew(NextDataSize());
		if (lastMem) MemPtrFree(lastMem);
		lastMem = mem;
	}
	t.Stop();

	if (lastMem) MemPtrFree(lastMem);

	WriteResult(TextOutput(), "MemPtrNew (pair)", t);
#endif

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunMemHandleLockTest()
{
#if TEST_PALMOS_APIS
	RestartDataSize();
	MemHandle hand = MemHandleNew(NextDataSize());

	Timer t(m_iterations*100);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		MemPtr mem = MemHandleLock(hand);
		MemHandleUnlock(hand);
	}
	t.Stop();

	WriteResult(TextOutput(), "MemHandleLock (pair)", t);
#endif

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunMallocTest()
{
	RestartDataSize();
	Timer t(m_iterations*100, kRandomLoop);
	t.Start();
	void* lastMem = NULL;
	for (int32_t i=0; i<t.N; i++) {
		void* mem = malloc(NextDataSize());
		if (lastMem) free(lastMem);
		lastMem = mem;
	}
	t.Stop();

	if (lastMem) free(lastMem);

	WriteResult(TextOutput(), "Malloc (pair)", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunNewMessageTest()
{
	Timer t(m_iterations*100);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		SMessage* msg = new SMessage('blah');
		delete msg;
	}
	t.Stop();

	WriteResult(TextOutput(), "New Message (pair)", t);

	return SValue::Status(B_OK);
}

class EmptyBinder : public BBinder
{
public:
	EmptyBinder(const SContext& context) : BBinder(context) { }
	virtual ~EmptyBinder() { }
};

SValue BinderPerformance::RunNewBinderTest()
{
	Timer t(m_iterations*100);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		sptr<IBinder> binder = new EmptyBinder(Context());
	}
	t.Stop();

	WriteResult(TextOutput(), "New Binder (pair)", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunNewComponentTest()
{
	Timer t(m_iterations*100);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		sptr<IBinder> binder = new TransactionTest(Context());
	}
	t.Stop();

	WriteResult(TextOutput(), "New Component (pair)", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunCriticalSectionTest()
{
	Timer t(m_iterations*100);
	SysCriticalSectionType cs = sysCriticalSectionInitializer;
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		SysCriticalSectionEnter(&cs);
		SysCriticalSectionExit(&cs);
	}
	t.Stop();

	WriteResult(TextOutput(), "SysCriticalSection (pair)", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunLockerTest()
{
	Timer t(m_iterations*100);
	SLocker lock;
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		lock.LockQuick();
		lock.Unlock();
	}
	t.Stop();

	WriteResult(TextOutput(), "SLocker (pair)", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunAutolockTest()
{
	Timer t(m_iterations*100);
	SLocker lock;
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		SLocker::Autolock _l(lock);
	}
	t.Stop();

	WriteResult(TextOutput(), "SLocker::Autolock (pair)", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunNestedLockerTest()
{
	Timer t(m_iterations*100);
	SNestedLocker lock;
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		lock.LockQuick();
		lock.Unlock();
	}
	t.Stop();

	WriteResult(TextOutput(), "SNestedLocker (pair)", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunIncStrongTest()
{
	sptr<IBinder> target = new TransactionTest(Context());
	Timer t(m_iterations*1000);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
#if BUILD_TYPE == BUILD_TYPE_DEBUG
		target->IncStrong(&target);
		target->DecStrong(&target);
#else
		target->IncStrongFast();
		target->DecStrongFast();
#endif
	}
	t.Stop();

	WriteResult(TextOutput(), "SAtom::Inc/DecStrong() (pair)", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunAtomPtrTest()
{
	sptr<IBinder> target = new TransactionTest(Context());
	Timer t(m_iterations*1000);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		sptr<IBinder> ptr(target);
	}
	t.Stop();

	WriteResult(TextOutput(), "sptr<> (pair)", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunValueSimpleTest()
{
	{
		RestartDataSize();
		Timer t(m_iterations*10000);
		t.Start();
		const SValue* result = NULL;
		for (int32_t i=0; i<t.N; i++) {
			SSimpleValue<int32_t> val(100);
			result = &(const SValue&)val;
		}
		t.Stop();

		WriteResult(TextOutput(), "SSimpleValue<int32_t>() (pair)", t);
	}

	{
		RestartDataSize();
		Timer t(m_iterations*1000);
		t.Start();
		const SValue* result = NULL;
		for (int32_t i=0; i<t.N; i++) {
			SValue val = SValue::Int32(100);
			result = &val;
		}
		t.Stop();

		WriteResult(TextOutput(), "SValue::Int32() (pair)", t);
	}

	{
		RestartDataSize();
		Timer t(m_iterations*100);
		t.Start();
		const SValue* result = NULL;
		for (int32_t i=0; i<t.N; i++) {
			SValue val = SValue::Int64(100);
			result = &val;
		}
		t.Stop();

		WriteResult(TextOutput(), "SValue::Int64() (pair)", t);
	}

	{
		RestartDataSize();
		Timer t(m_iterations*100, kRandomLoop);
		void* mem = malloc(MAX_DATA);
		t.Start();
		const SValue* result = NULL;
		for (int32_t i=0; i<t.N; i++) {
			SValue val(B_RAW_TYPE, mem, NextDataSize());
			result = &val;
		}
		t.Stop();
		free(mem);

		WriteResult(TextOutput(), "SValue Simple Blob (pair)", t);
	}

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunValueJoin2Test()
{
	Timer t(m_iterations*100);
	SValue map1(SValue::String("one"), SValue::String("two"));
	SValue map2(SValue::String("three"), SValue::String("four"));
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		SValue val(map1);
		val.Join(map2);
	}
	t.Stop();

	WriteResult(TextOutput(), "SValue Join 2", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunValueJoinManyTest()
{
	RestartDataSize();
	Timer t((m_iterations*100)/AvgDataSize(), kRandomLoop);
	const SValue data("some long data");
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		SValue val;
		const int32_t COUNT = NextDataSize();
		for (int32_t j=0; j<COUNT; j++) {
			val.JoinItem(SValue::Int32(j), data);
		}
	}
	t.Stop();

	WriteResult(TextOutput(), "SValue Join Many", t);

	return SValue::Status(B_OK);
}

static volatile int32_t dummyValueInt = 0;

SValue BinderPerformance::RunValueIntegerLookupTest()
{
	{
		RestartDataSize();
		Timer t(m_iterations*10000);
		int32_t i;

		int32_t* data = new int32_t[MAX_DATA];
		int32_t found;
		for (i=0; i<MAX_DATA; i++) {
			data[i] = i;
		}

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			found = data[i%MAX_DATA];
		}
		t.Stop();
		dummyValueInt = found;

		delete[] data;

		WriteResult(TextOutput(), "Array Lookup Int", t);
	}

	{
		RestartDataSize();
		Timer t(m_iterations*10000);
		int32_t i;

		SVector<int32_t> data;
		int32_t found;
		for (i=0; i<MAX_DATA; i++) {
			data.AddItem(i);
		}

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			found = data[i%MAX_DATA];
		}
		t.Stop();
		dummyValueInt = found;

		WriteResult(TextOutput(), "SVector Lookup Int", t);
	}

	{
		RestartDataSize();
		Timer t(m_iterations*100, kRandomLoop);
		int32_t i;

		SKeyedVector<int32_t, int32_t> data;
		int32_t found;
		for (i=0; i<MAX_DATA; i++) {
			data.AddItem(i, i);
		}

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			found = data[NextDataSize()-1];
		}
		t.Stop();

		WriteResult(TextOutput(), "SKeyedVector Lookup Int", t);
	}

	{
		RestartDataSize();
		Timer t(m_iterations*100, kRandomLoop);
		int32_t i;

		SValue data;
		SValue found;
		for (i=0; i<MAX_DATA; i++) {
			data.JoinItem(SValue::Int32(i), SValue::Int32(i));
		}

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			found = data[NextDataSize()-1];
		}
		t.Stop();

		WriteResult(TextOutput(), "SValue Lookup Int", t);
	}

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunValueStringLookupTest()
{
	{
		RestartDataSize();
		Timer t(m_iterations*100, kRandomLoop);
		int32_t i;

		SKeyedVector<SString, SString> data;
		SString found;
		for (i=0; i<MAX_DATA; i++) {
			data.AddItem(m_strings[i], m_strings[i]);
		}

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			found = data[m_strings[NextDataSize()-1]];
		}
		t.Stop();

		WriteResult(TextOutput(), "SKeyedVector Lookup Str", t);
	}

	{
		RestartDataSize();
		Timer t(m_iterations*100, kRandomLoop);
		int32_t i;

		SValue data;
		SValue found;
		for (i=0; i<MAX_DATA; i++) {
			data.JoinItem(m_values[i], m_values[i]);
		}

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			found = data[m_values[NextDataSize()-1]];
		}
		t.Stop();

		WriteResult(TextOutput(), "SValue Lookup Str", t);
	}

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunValueIntegerBuildTest(size_t amount)
{
	{
		RestartDataSize();
		Timer t((m_iterations*100)/amount + 1);
		int32_t i;

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			int32_t* data = new int32_t[amount];
			for (size_t j=0; j<amount; j++) {
				data[j] = (int32_t)j;
			}
			delete[] data;
		}
		t.Stop();

		SString str;
		str << "Array Build Static " << amount << " Int";
		WriteResult(TextOutput(), str.String(), t);
	}

	{
		RestartDataSize();
		Timer t((m_iterations*100)/amount + 1);
		int32_t i;

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SVector<int32_t> dataVec;
			dataVec.SetSize(amount);
			int32_t* data = dataVec.EditArray();
			for (size_t j=0; j<amount; j++) {
				data[j] = (int32_t)j;
			}
		}
		t.Stop();

		SString str;
		str << "SVector Build Static " << amount << " Int";
		WriteResult(TextOutput(), str.String(), t);
	}

	{
		RestartDataSize();
		Timer t((m_iterations*100)/amount + 1);
		int32_t i;

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SVector<int32_t> data;
			for (size_t j=0; j<amount; j++) {
				data.AddItem((int32_t)j);
			}
		}
		t.Stop();

		SString str;
		str << "SVector Build Dynamic " << amount << " Int";
		WriteResult(TextOutput(), str.String(), t);
	}

	{
		RestartDataSize();
		Timer t((m_iterations*100)/amount + 1);
		int32_t i;

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SKeyedVector<int32_t, int32_t> data;
			for (size_t j=0; j<amount; j++) {
				data.AddItem((int32_t)j, (int32_t)j);
			}
		}
		t.Stop();

		SString str;
		str << "SKeyedVector Build " << amount << " Int";
		WriteResult(TextOutput(), str.String(), t);
	}

	{
		RestartDataSize();
		Timer t((m_iterations*100)/amount + 1);
		int32_t i;

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SValue data;
			for (size_t j=0; j<amount; j++) {
				data.JoinItem(SSimpleValue<int32_t>((int32_t)j), SSimpleValue<int32_t>((int32_t)j));
			}
		}
		t.Stop();

		SString str;
		str << "SValue Build " << amount << " Int";
		WriteResult(TextOutput(), str.String(), t);
	}

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunValueStringBuildTest(size_t amount)
{
	{
		RestartDataSize();
		Timer t((m_iterations*100)/amount + 1);
		int32_t i;

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SVector<SString> dataVec;
			dataVec.SetSize(amount);
			SString* data = dataVec.EditArray();
			for (size_t j=0; j<amount; j++) {
				data[j] = m_strings[j%MAX_DATA];
			}
		}
		t.Stop();

		SString str;
		str << "SVector Build Static " << amount << " Str";
		WriteResult(TextOutput(), str.String(), t);
	}

	{
		RestartDataSize();
		Timer t((m_iterations*100)/amount + 1);
		int32_t i;

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SVector<SString> dataVec;
			dataVec.SetSize(amount);
			SString* data = dataVec.EditArray();
			for (size_t j=0; j<amount; j++) {
				data[j] = m_strings[j%MAX_DATA].String();
			}
		}
		t.Stop();

		SString str;
		str << "SVector Build Static " << amount << " Copy Str";
		WriteResult(TextOutput(), str.String(), t);
	}

	{
		RestartDataSize();
		Timer t((m_iterations*100)/amount + 1);
		int32_t i;

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SVector<SString> data;
			for (size_t j=0; j<amount; j++) {
				data.AddItem(m_strings[j%MAX_DATA]);
			}
		}
		t.Stop();

		SString str;
		str << "SVector Build Dynamic " << amount << " Str";
		WriteResult(TextOutput(), str.String(), t);
	}

	{
		RestartDataSize();
		Timer t((m_iterations*100)/amount + 1);
		int32_t i;

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SKeyedVector<SString, SString> data;
			for (size_t j=0; j<amount; j++) {
				data.AddItem(m_strings[j%MAX_DATA], m_strings[j%MAX_DATA]);
			}
		}
		t.Stop();

		SString str;
		str << "SKeyedVector Build " << amount << " Str";
		WriteResult(TextOutput(), str.String(), t);
	}

	{
		RestartDataSize();
		Timer t((m_iterations*100)/amount + 1);
		int32_t i;

		t.Start();
		for (int32_t i=0; i<t.N; i++) {
			SValue data;
			for (size_t j=0; j<amount; j++) {
				data.JoinItem(m_values[j%MAX_DATA], m_values[j%MAX_DATA]);
			}
		}
		t.Stop();

		SString str;
		str << "SValue Build " << amount << " Str";
		WriteResult(TextOutput(), str.String(), t);
	}

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunHandlerTest(int32_t num)
{
	RestartDataSize();
	Timer t(m_iterations * 5 * (num == 1 ? 3 : 1));

	handler_test_state state;
	state.iterations = t.N;
	state.priority = m_priority;
	state.dataSize = NextDataSizePreferZero(); // XXX Not correct!

	sptr<HandlerTest> h1 = new HandlerTest;
	sptr<HandlerTest> h2 = new HandlerTest;

	if (num == 1) {
		h1->Init(&state);
	} else if (num == 2) {
		h1->Init(&state, h2);
		h2->Init(&state, h1);
	}

	t.Start();
	h1->RunTest();
	t.Stop();

	WriteResult(TextOutput(), num == 1 ? "Pulse Handler" : "PingPong Handler", t);

	return SValue::Status(B_OK);
}

B_STATIC_STRING_VALUE_40(kTransactionTestName, "org.openbinder.tools.commands.BPerf.TransactionTest", );

SValue BinderPerformance::RunInstantiateTest(bool remote)
{
	sptr<IProcess> proc;
	if (remote) {
		proc = BackgroundProcess();
		if (proc == NULL) {
			TextOutput() << "Remote Instantiate: <processes disabled>" << endl;
			return SValue::Undefined();
		}
	}

	Timer t(m_iterations);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		if (remote) {
			sptr<IBinder> obj = Context().RemoteNew(kTransactionTestName, proc);
		} else {
			sptr<IBinder> obj = Context().New(kTransactionTestName);
		}
	}
	t.Stop();

	WriteResult(TextOutput(), remote ? "Remote Instantiate" : "Local Instantiate", t);

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunTransactionTest(bool remote, size_t sizeFactor)
{
	SValue result(SValue::Status(B_OK));
	
	RestartDataSize();
	sptr<IBinder> target;

	SString str(remote ? "Remote " : "Local ");
	if (sizeFactor == 0) str << "Empty ";
	else if (sizeFactor != 1) {
		str << sizeFactor;
		str << "xSize ";
	}
	str << "Transaction";
	
	sptr<IProcess> proc;
	SValue componentArgs(key_validate, SValue::Bool(m_validating));
	if (remote) {
		proc = BackgroundProcess();
		if (proc == NULL) {
			TextOutput() << str << ": <processes disabled>" << endl;
			return SValue::Undefined();
		}
		target = Context().RemoteNew(kTransactionTestName, proc, componentArgs);
	} else {
		target = Context().New(kTransactionTestName, componentArgs);
	}

	if (target == NULL) {
		TextError() << "Unable to instantiate org.openbinder.tools.commands.BPerf.TransactionTest!" << endl;
		return SValue::Int32(10);
	}

	SParcel send, buffer;
	if (m_dataSize > 0 && sizeFactor > 0) {
		void* data = send.Alloc(m_dataSize*sizeFactor);
		if (data) memset(data, 0xff, m_dataSize*sizeFactor);
		else {
			TextError() << "Unable to create payload in send parcel!" << endl;
			return SValue::Status(B_ENTRY_NOT_FOUND);
		}
	} else if (m_validating && sizeFactor) {
		void* data = buffer.Alloc(((MaxDataSize()*sizeFactor)+3)&~3);
		if (data) {
			uint32_t* p = (uint32_t*)data;
			uint32_t* e = (uint32_t*)(((uint8_t*)data)+MaxDataSize()*sizeFactor);
			//bout << "Filling " << buffer.Length() << ": from " << p << " to " << e << " (" << (e-p) << ")" << endl;
			uint32_t i=0;
			while (p < e) *p++ = i++;
			//bout << "Test data: " << SHexDump(buffer.Data(), buffer.Length()) << endl;
		}
	}

	Timer t((remote ? m_iterations : (m_iterations*100)) / (sizeFactor > 0 ? sizeFactor : 1));
	SParcel *reply = SParcel::GetParcel();
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		if (m_dataSize < 0 && sizeFactor > 0) {
			int32_t size = NextDataSize() * sizeFactor;
			//bout << "Allocating parcel size " << size << endl;
			void* data = send.Alloc(size);
			if (data) {
				if (!m_validating) memset(data, 0xff, size);
				else memcpy(data, buffer.Data(), size);
			}
			else {
				TextError() << "Unable to create payload in send parcel!" << endl;
				return SValue::Status(B_NO_MEMORY);
			}
		}
		//bout << "Sending transaction #" << i << " of " << send.Length() << " bytes" << endl;
		target->Transact(kTestTransaction, send, reply);
		if (m_validating) {
			if (!validate_transaction_data(*reply)) {
				result = SValue::Status(B_BAD_DATA);
				break;
			}
		}
	}
	t.Stop();
	SParcel::PutParcel(reply);

	if (result.AsStatus() == B_OK)
		WriteResult(TextOutput(), str.String(), t);
	else
		TextOutput() << str << ": FAILED" << endl;

	return result;
}

SValue BinderPerformance::RunPingPongTransactionTest()
{
	SValue result(SValue::Status(B_OK));
	
	RestartDataSize();
	sptr<IBinder> target;

	sptr<IProcess> proc;
	proc = BackgroundProcess();
	if (proc == NULL) {
		TextOutput() << "Ping-Pong Transaction: <processes disabled>" << endl;
		return SValue::Undefined();
	}

	SValue componentArgs(key_validate, SValue::Bool(m_validating));
	target = Context().RemoteNew(kTransactionTestName, proc, componentArgs);

	if (target == NULL) {
		TextError() << "Unable to instantiate org.openbinder.tools.commands.BPerf.TransactionTest!" << endl;
		return SValue::Status(B_ENTRY_NOT_FOUND);
	}

	sptr<TransactionTest> server = new TransactionTest(Context(), m_validating, true);

	SParcel *send = SParcel::GetParcel();
	send->WriteBinder(server.ptr());

	Timer t(m_iterations);
	SParcel *reply = SParcel::GetParcel();
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		target->Transact(kPingPongTransaction, *send, reply);
		if (m_validating) {
			sptr<IBinder> binder = reply->ReadBinder();
			if (binder != server->PongDatum()) {
				result = SValue::Status(B_BAD_DATA);
				break;
			}
		}
	}
	t.Stop();
	SParcel::PutParcel(reply);

	if (result.AsStatus() == B_OK)
		WriteResult(TextOutput(), "Ping-Pong Transaction", t);
	else
		TextOutput() << "Ping-Pong Transaction: FAILED (Pong reply did not contain our Binder)" << endl;

	SParcel::PutParcel(send);
	return result;
}

SValue BinderPerformance::RunBinderTransferTest(bool remote, int type)
{
	SValue result(SValue::Status(B_OK));
	
	sptr<IBinder> target;
	sptr<IBinder> object;
	wptr<IBinder> wobject;

	bool doweak = type == kOldWeakBinder || type == kSameWeakBinder || type == kNewWeakBinder;
	
	const char* typeStr;
	if (type == kOldBinder) typeStr = "Old Binder: ";
	else if (type == kOldWeakBinder) typeStr = "Old Weak Binder: ";
	else if (type == kWeakToStrongBinder) typeStr = "Weak To Strong Binder: ";
	else if (type == kSameBinder) typeStr = "Same Binder: ";
	else if (type == kSameWeakBinder) typeStr = "Same Weak Binder: ";
	else if (type == kNewBinder) typeStr = "New Binder: ";
	else if (type == kNewWeakBinder) typeStr = "New Weak Binder: ";
	else typeStr = "??";

	SString str(remote ? "Remote " : "Local ");
	str += typeStr;

	SValue componentArgs(key_validate, SValue::Bool(m_validating));
	sptr<IProcess> proc;
	if (remote) {
		proc = BackgroundProcess();
		if (proc == NULL) {
			TextOutput() << str << "<processes disabled>" << endl;
			return SValue::Undefined();
		}
		target = Context().RemoteNew(kTransactionTestName, proc, componentArgs);
	} else {
		target = Context().New(kTransactionTestName, componentArgs);
	}

	if (target == NULL) {
		TextError() << "Unable to instantiate org.openbinder.tools.commands.BPerf.TransactionTest!" << endl;
		return SValue::Status(B_ENTRY_NOT_FOUND);
	}

	if (type != kNewBinder && type != kNewWeakBinder) {
		object = new TransactionTest(Context(), m_validating);
		wobject = object;
	}

	SParcel *reply = SParcel::GetParcel();
	if (type == kOldBinder) {
		SParcel setup;
		ssize_t err = setup.WriteBinder(object);
		if (err < B_OK) {
			TextError() << "Unable to write setup binder!" << endl;
			return SValue::Status(B_BINDER_BAD_TRANSACT);
		}
		if (target->Transact(kKeepTransaction, setup, reply) != B_OK) {
			TextError() << "Unable to send setup binder!" << endl;
			return SValue::Status(B_BINDER_BAD_TRANSACT);
		}
	} else if (type == kOldWeakBinder || type == kWeakToStrongBinder) {
		SParcel setup;
		ssize_t err = setup.WriteWeakBinder(wobject);
		if (err < B_OK) {
			TextError() << "Unable to write setup binder!" << endl;
			return SValue::Status(B_BINDER_BAD_TRANSACT);
		}
		if (target->Transact(kKeepWeakTransaction, setup, reply) != B_OK) {
			TextError() << "Unable to send setup binder!" << endl;
			return SValue::Status(B_BINDER_BAD_TRANSACT);
		}
	}

	Timer t(remote ? m_iterations : (m_iterations*100));
	SParcel send;
	sptr<IBinder> objects[32];
	wptr<IBinder> wobjects[32];
	if (type == kNewBinder || type == kNewWeakBinder) {
		for (int32_t i=0; i<32; i++) {
			objects[i] = new TransactionTest(Context(), m_validating);
			wobjects[i] = objects[i];
		}
	}
	
	t.Start();
	if (doweak) {
		uint32_t code = type == kOldWeakBinder ? kDropSameWeakTransaction : kDropWeakTransaction;
		for (int32_t i=0; i<t.N; i++) {
			send.Reset();
			if (type == kNewWeakBinder) {
				wobject = wobjects[i&0x1f];// new TransactionTest(Context(), m_validating);
			}
			send.WriteWeakBinder(wobject);
			status_t err = target->Transact(code, send, reply);
			if (m_validating && err != B_OK) {
				bout << str << ": Binder transfer failed!" << endl;
				bout << "I sent: " << send << endl;
				result = SValue::Status(B_BAD_DATA);
				break;
			}
		}
	} else {
		uint32_t code = type == kOldBinder || type == kWeakToStrongBinder
					  ? kDropSameTransaction : kDropTransaction;
		for (int32_t i=0; i<t.N; i++) {
			send.Reset();
			if (type == kNewBinder) {
				object = objects[i&0x1f];// new TransactionTest(Context(), m_validating);
			}
			send.WriteBinder(object);
			status_t err = target->Transact(code, send, reply);
			if (m_validating && err != B_OK) {
				//bout << str << ": Binder transfer failed!" << endl;
				result = SValue::Status(B_BAD_DATA);
				break;
			}
		}
	}
	t.Stop();
	
	SParcel::PutParcel(reply);

	if (result.AsStatus() == B_OK)
		WriteResult(TextOutput(), str.String(), t);
	else
		TextOutput() << str << ": FAILED" << endl;

	return result;
}

SValue
BinderPerformance::RunEffectIPCTest(bool remote)
{
	SContext context = Context();

	sptr<IBinder> bt;
#if TEST_PALMOS_APIS
	sptr<IBinder> bv;
#endif

	sptr<IProcess> proc;
	if (remote) {
		proc = BackgroundProcess();
		if (proc == NULL) {
			TextOutput() << "EffectIPC: <processes disabled>" << endl;
			return SValue::Undefined();
		}
		bt = context.RemoteNew(SValue::String("org.openbinder.tools.commands.BPerf.ImplementsIProcess"), proc);
#if TEST_PALMOS_APIS
		bv = context.RemoteNew(SValue::String("org.openbinder.tools.commands.BPerf.ImplementsIView"), proc);
#endif

	} else {
		bt = context.New(SValue::String("org.openbinder.tools.commands.BPerf.ImplementsIProcess"));
#if TEST_PALMOS_APIS
		bv = context.New(SValue::String("org.openbinder.tools.commands.BPerf.ImplementsIView"));
#endif
	}

	if (bt == NULL) {
		TextError() << "Unable to instantiate org.openbinder.tools.commands.BPerf.ImplementsIProcess!" << endl;
		return SValue::Status(B_ERROR);
	}
#if TEST_PALMOS_APIS
	if (bv == NULL) {
		TextError() << "Unable to instantiate org.openbinder.tools.commands.BPerf.ImplementsIView!" << endl;
		return SValue::Status(B_ERROR);
	}
#endif
	
	sptr<IProcess> t = IProcess::AsInterface(bt);
#if TEST_PALMOS_APIS
	sptr<IView> v = IView::AsInterface(bv);
#endif
	
	const char* prefix = remote ? "Remote " : "Local ";
	SString text;

	Timer timer(m_iterations * (remote ? 1 : 100));
	
	// IProcess tests...
	
	timer.Start();
	for (int32_t i=0; i<timer.N; i++) {
		t->RestartMallocProfiling();
	}
	timer.Stop();
	text = prefix;
	text += "EffectIPC 0 args, no return";
	WriteResult(TextOutput(), text.String(), timer);
	
	timer.Start();
	for (int32_t i=0; i<timer.N; i++) {
		t->AtomMarkLeakReport();
	}
	timer.Stop();
	text = prefix;
	text += "EffectIPC 0 args, int32 return";
	WriteResult(TextOutput(), text.String(), timer);
	
	timer.Start();
	for (int32_t i=0; i<timer.N; i++) {
		t->AtomLeakReport(1,2,3);
	}
	timer.Stop();
	text = prefix;
	text += "EffectIPC 3 int args, no return";
	WriteResult(TextOutput(), text.String(), timer);
	
	SValue componentInfo(SValue::String("some_info_not_much"));
	SString component("lalalalala");
	SValue args(B_0_INT32, SValue::String("an_arg"));
	timer.Start();
	for (int32_t i=0; i<timer.N; i++) {
		t->InstantiateComponent(context.Root(), componentInfo, component, args);
	}
	timer.Stop();
	text = prefix;
	text += "EffectIPC complex args, no return";
	WriteResult(TextOutput(), text.String(), timer);

	
#if TEST_PALMOS_APIS

	// IView tests
	BRect frame;
	timer.Start();
	for (int32_t i=0; i<timer.N; i++) {
		frame = v->DrawingBounds();
	}
	timer.Stop();
	text = prefix;
	text += "EffectIPC DrawingBounds";
	WriteResult(TextOutput(), text.String(), timer);
		
	BLayoutConstraints cn;
	timer.Start();
	for (int32_t i=0; i<timer.N; i++) {
		cn = v->Constraints();
	}
	timer.Stop();
	text = prefix;
	text += "EffectIPC Constraints";
	WriteResult(TextOutput(), text.String(), timer);
	
	sptr<IViewParent> p;
	timer.Start();
	for (int32_t i=0; i<timer.N; i++) {
		p = v->Parent();
	}
	timer.Stop();
	text = prefix;
	text += "EffectIPC Parent";
	WriteResult(TextOutput(), text.String(), timer);
	
	timer.Start();
	for (int32_t i=0; i<timer.N; i++) {
		v->MarkTraversalPath(0);
	}
	timer.Stop();
	text = prefix;
	text += "EffectIPC MarkTraversalPath";
	WriteResult(TextOutput(), text.String(), timer);
#endif

	return SValue::Status(B_OK);
}


class WeakTestBinder : public BBinder, public SPackageSptr
{
public:
	
	WeakTestBinder() { SHOW_OBJS(bout << "WeakTestBinder created" << endl);
	}
	virtual ~WeakTestBinder() { SHOW_OBJS(bout << "WeakTestBinder destroyed" << endl);
	}
	
	wptr<IBinder> m_other;
};

class AttemptIncStrongHandler : public BHandler, public SPackageSptr
{
public:
	bool failed;

	AttemptIncStrongHandler(handler_test_state* state, const wptr<IBinder>& target)
		: m_state(state), m_target(target), failed(false)
	{
		SHOW_OBJS(bout << "AttemptIncStrongHandler " << this << " constructed..." << endl);
	}

	void StartTest()
	{
		SMessage msg(kTestHandler);
		msg.SetPriority(m_state->priority);
		PostMessage(msg);
	}

	void PerformTest()
	{
		while (SysAtomicAdd32(&m_state->current, 1) < m_state->iterations) {
			//SysThreadDelay(B_MS2NS(100), B_RELATIVE_TIMEOUT);
			bool success =  m_target.promote() != NULL;
			if (!success) {
				bout << "Attempt inc strong: promote failed!" << endl;
				failed = true;
			}
		}
		if (SysAtomicAdd32(&m_state->numThreads, -1) == 1)
			m_state->finished.Open();
	}

	virtual	status_t HandleMessage(const SMessage &msg)
	{
		if (msg.What() == kTestHandler) {
			PerformTest();
			return B_OK;
		}

		return BHandler::HandleMessage(msg);
	}

protected:
	virtual ~AttemptIncStrongHandler()
	{
		SHOW_OBJS(bout << "AttemptIncStrongHandler " << this << " destroyed..." << endl);
	}

private:
	handler_test_state*	m_state;
	wptr<IBinder> m_target;
};


SValue BinderPerformance::RunAttemptIncStrongTest(bool remote, int32_t threads)
{
	SValue result(SValue::Status(B_OK));
	
	RestartDataSize();
	Timer t(remote ? m_iterations : (m_iterations*100));
	handler_test_state state;
	state.iterations = t.N;
	state.priority = m_priority;
	state.dataSize = NextDataSizePreferZero(); // XXX Not correct!
	state.numThreads = threads;

	SParcel send, *reply = SParcel::GetParcel();
	sptr<IBinder> starget;
	wptr<IBinder> target;
	//sptr<WeakTestBinder> loc;

	SString str;
	str << (remote ? (threads == 1 ? "Remote " : "Multiple ") : "Local ") << "AttemptIncStrong (pair)";
	
	sptr<IProcess> proc;
	if (remote) {
		proc = BackgroundProcess();
		if (proc == NULL) {
			TextOutput() << str << "<processes disabled>" << endl;
			return SValue::Undefined();
		}
		starget = Context().RemoteNew(kTransactionTestName, proc);
	} else {
		starget = Context().New(kTransactionTestName);
	}

	if (starget == NULL) {
		TextError() << "Unable to run <org.openbinder.tools.commands.BPerf.TransactionTest>!" << endl;
		return SValue::Status(B_ENTRY_NOT_FOUND);
	}
	
	target = starget.ptr();
	starget->Transact(kIncStrongTransaction, send, reply);
#if 0
	loc = new WeakTestBinder;
	loc->m_other = target;
	send.WriteBinder(loc.ptr());
	starget->Transact(kHoldTransaction, send, reply);
#endif
	send.Free();
	starget = NULL;
	//loc = NULL;

	SParcel::PutParcel(reply);
	reply = NULL;
	
	if (remote || threads != 1) {
		t.N /= threads;
		state.iterations  = t.N;
		state.current = 0;
		state.finished.Close();
		sptr<AttemptIncStrongHandler>* handlers = new sptr<AttemptIncStrongHandler>[threads];
		sptr<AttemptIncStrongHandler> dummy; // this might work around an ADS bug.
		int32_t i;
		for (i=0; i<threads; i++) {
			handlers[i] = new AttemptIncStrongHandler(&state, target);
		}
		dummy = handlers[0];
		t.Start();
		for (int32_t i=0; i<threads; i++) {
			handlers[i]->StartTest();
		}
		state.finished.Wait();
		t.Stop();
		for (i=0; i<threads; i++) {
			if (handlers[i] != NULL && handlers[i]->failed) {
				TextOutput() << str << ": FAILED (Unable to acquire strong reference in test)" << endl;
				result = SValue::Status(B_BAD_VALUE);
			}
		}
		delete[] handlers;
	} else {
		// Want the local case comparable to other local operations.
		t.Start();
		for (int32_t i=0; i<state.iterations; i++) {
			sptr<IBinder> starget = target.promote();
			if (m_validating && starget == NULL) {
				result = SValue::Status(B_BAD_VALUE);
				break;
			}
		}
		t.Stop();
	}

	if (result.AsStatus() != B_OK) {
		TextOutput() << str << ": FAILED (Unable to acquire strong reference in test)" << endl;
		return result;
	}
	
	starget = target.promote();
	if (starget != NULL) {
		starget->Transact(kDecStrongTransaction, send, reply);
		if (result.AsStatus() == B_OK)
			WriteResult(TextOutput(), str.String(), t);
		else
			TextOutput() << str << ": FAILED" << endl;
	} else {
		TextOutput() << str << ": FAILED (Unable to acquire final strong reference)" << endl;
		result = SValue::Status(B_BAD_VALUE);
	}
	
	return result;
}

SValue BinderPerformance::RunDmNextTest()
{
#if TEST_PALMOS_APIS
	RestartDataSize();
	Timer t(m_iterations*1);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		DmSearchStateType state;
		DatabaseID db;
		if (DmOpenIteratorByTypeCreator(&state,
										'rsrc',					// only look at resources
										0,						// take any creator
										false,					// all versions
										dmFindExtendedDB) == errNone)
		{
			DmGetNextDatabaseByTypeCreator(&state, &db, NULL);
			DmCloseIteratorByTypeCreator(&state);
		}
	}
	t.Stop();

	WriteResult(TextOutput(), "DmGetNextDatabaseByTypeCreator", t);
#endif
	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunDmInfoTest()
{
#if TEST_PALMOS_APIS
	DatabaseID dbID;
	DmOpenRef ref;

	dbID = DmFindDatabaseByTypeCreator('rsrc', sysFileCUI, dmFindExtendedDB, NULL);
	ref = DmOpenDatabase(dbID, dmModeReadOnly);
	if (ref == NULL) {
		TextOutput() << "DmOpenDatabase: can't open UI resources!" << endl;
		return SValue::Status(DmGetLastErr());
	}

	RestartDataSize();
	Timer t(m_iterations*1);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		char	name[dmDBNameLength] = { 0 };
		uint32_t 	type;
		uint32_t 	creator;
		DmDatabaseInfoType databaseInfo = { 0 };
		databaseInfo.size			= sizeof(databaseInfo);
		databaseInfo.pName			= name;
		databaseInfo.pType			= &type;
		databaseInfo.pCreator		= &creator;

		DmDatabaseInfo(dbID, &databaseInfo);
	}
	t.Stop();

	WriteResult(TextOutput(), "DmDatabaseInfo", t);

	DmCloseDatabase(ref);
#endif
	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunDmOpenTest()
{
#if TEST_PALMOS_APIS
	status_t err;
	DatabaseID db;

	err = DmCreateDatabase("bperf_test_db", 'perf', 'test', false);
	if (err) {
		TextOutput() << "DmCreateDatabase: failed!" << endl;
		return SValue::Status(err);
	}

	db = DmFindDatabase("bperf_test_db", 'perf', dmFindExtendedDB, NULL);
	if (!db) {
		TextOutput() << "DmFindDatabase: couldn't find the test database!" << endl;
		return SValue::Status(dmErrCantFind);
	}

	RestartDataSize();
	Timer t(m_iterations*1);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		DmOpenRef ref = DmOpenDBNoOverlay(db, dmModeReadOnly);
		DmCloseDatabase(ref);
	}
	t.Stop();

	DmDeleteDatabase(db);

	WriteResult(TextOutput(), "DmOpenDatabase (pair)", t);
#endif
	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunDmFindResourceTest()
{
#if TEST_PALMOS_APIS
	DatabaseID dbID;
	DmOpenRef ref;

	dbID = DmFindDatabaseByTypeCreator('rsrc', sysFileCUI, dmFindExtendedDB, NULL);
	ref = DmOpenDatabase(dbID, dmModeReadOnly);
	if (ref == NULL) {
		TextOutput() << "DmOpenDatabase: can't open UI resources!" << endl;
		return SValue::Status(DmGetLastErr());
	}

	RestartDataSize();
	Timer t(m_iterations*1);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		uint16_t index = DmFindResource(ref, formRscType, aboutDialog, NULL);
	}
	t.Stop();

	WriteResult(TextOutput(), "DmFindResource (type/ID)", t);



	MemHandle handle = DmGetResource(ref, formRscType, aboutDialog);

	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		uint16_t index = DmFindResource(ref, 0, 0, handle);
	}
	t.Stop();

	WriteResult(TextOutput(), "DmFindResource (handle)", t);

	DmCloseDatabase(ref);
#endif
	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunDmGetResourceTest()
{
#if TEST_PALMOS_APIS
	DatabaseID dbID;
	DmOpenRef ref;

	dbID = DmFindDatabaseByTypeCreator('rsrc', sysFileCUI, dmFindExtendedDB, NULL);
	ref = DmOpenDatabase(dbID, dmModeReadOnly);
	if (ref == NULL) {
		TextOutput() << "DmOpenDatabase: can't open UI resources!" << endl;
		return SValue::Status(DmGetLastErr());
	}

	RestartDataSize();
	Timer t(m_iterations*1);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		MemHandle handle = DmGetResource(ref, formRscType, aboutDialog);
		DmReleaseResource(handle);
	}
	t.Stop();

	WriteResult(TextOutput(), "DmGetResource (pair)", t);

	DmCloseDatabase(ref);
#endif
	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunDmHandleLockTest()
{
#if TEST_PALMOS_APIS
	DatabaseID dbID;
	DmOpenRef ref;

	dbID = DmFindDatabaseByTypeCreator('rsrc', sysFileCUI, dmFindExtendedDB, NULL);
	ref = DmOpenDatabase(dbID, dmModeReadOnly);
	if (ref == NULL) {
		TextOutput() << "DmOpenDatabase: can't open UI resources!" << endl;
		return SValue::Status(DmGetLastErr());
	}

	MemHandle handle = DmGetResource(ref, formRscType, aboutDialog);
	if (handle == 0) {
		TextOutput() << "DmHandleLock: failed DmGetResource()!" << endl;
		return SValue::Status(DmGetLastErr());
	}

	RestartDataSize();
	Timer t(m_iterations*10);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		MemPtr mem = DmHandleLock(handle);
		DmHandleUnlock(handle);
	}
	t.Stop();

	DmReleaseResource(handle);

	WriteResult(TextOutput(), "DmHandleLock (pair)", t);

	DmCloseDatabase(ref);
#endif
	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunDbCursorOpenTest()
{
#if TEST_PALMOS_APIS
	status_t err;
	DatabaseID dbID;
	DmOpenRef dbRef;
	DbSchemaColumnDefnType colDef = { 1, sizeof(uint32_t), "one", dbUInt32, 0, 0, 0 };
	DbTableDefinitionType tableDef = { "table", 1, &colDef };
	uint32_t rowCount = 500;
	uint32_t rowNum;
	DbSchemaColumnValueType colVal = { &rowNum, sizeof(uint32_t), 1, 0, 0, 0 };
	
	err = DbCreateDatabase("bperf_db_cursor_open_test", 'bprf', 'bprf', 1, &tableDef, &dbID);
	if (err) {
		return SValue::Status(err);
	}

	dbRef = DbOpenDatabase(dbID, dmModeReadWrite, dbShareNone);
	if (!dbRef) {
		DmDeleteDatabase(dbID);
		return SValue::Status(dmErrInvalidDatabaseName);
	}

	for (rowNum = 0; rowNum < rowCount; rowNum++) {
		err = DbInsertRow(dbRef, "table", 1, &colVal, NULL);
		if (err) {
			DbCloseDatabase(dbRef);
			DmDeleteDatabase(dbID);
			return SValue::Status(err);
		}
	}

	uint32_t cursor;
	RestartDataSize();
	Timer t(m_iterations*10);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		DbCursorOpen(dbRef, "table", 0, &cursor);
		DbCursorClose(cursor);
	}
	t.Stop();

	DbCloseDatabase(dbRef);
	DmDeleteDatabase(dbID);

	WriteResult(TextOutput(), "DmCursorOpen (pair)", t);
#endif

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunDbCursorMoveTest()
{
#if TEST_PALMOS_APIS
	status_t err;
	DatabaseID dbID;
	DmOpenRef dbRef;
	DbSchemaColumnDefnType colDef = { 1, sizeof(uint32_t), "one", dbUInt32, 0, 0, 0 };
	DbTableDefinitionType tableDef = { "table", 1, &colDef };
	uint32_t rowCount = 500;
	uint32_t rowNum;
	DbSchemaColumnValueType colVal = { &rowNum, sizeof(uint32_t), 1, 0, 0, 0 };

	err = DbCreateDatabase("bperf_db_cursor_open_test", 'bprf', 'bprf', 1, &tableDef, &dbID);
	if (err) {
		return SValue::Status(err);
	}

	dbRef = DbOpenDatabase(dbID, dmModeReadWrite, dbShareNone);
	if (!dbRef) {
		DmDeleteDatabase(dbID);
		return SValue::Status(dmErrInvalidDatabaseName);
	}

	for (rowNum = 0; rowNum < rowCount; rowNum++) {
		err = DbInsertRow(dbRef, "table", 1, &colVal, NULL);
		if (err) {
			DbCloseDatabase(dbRef);
			DmDeleteDatabase(dbID);
			return SValue::Status(err);
		}
	}

	uint32_t cursor;
	DbCursorOpen(dbRef, "table", 0, &cursor);
	DbCursorMoveNext(cursor);
	RestartDataSize();
	Timer t(m_iterations*10);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		if (i % 2) {
			DbCursorMoveNext(cursor);
		} else {
			DbCursorMovePrev(cursor);
		}
	}
	t.Stop();

	WriteResult(TextOutput(), "DbCursorMoveNext/DbCursorMovePrev", t);


	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		DbCursorSetAbsolutePosition(cursor, myrand(&m_seed) % rowCount);
	}
	t.Stop();

	WriteResult(TextOutput(), "DbCursorSetAbsolutePosition", t);

	DbCursorClose(cursor);
	DbCloseDatabase(dbRef);
	DmDeleteDatabase(dbID);
#endif

	return SValue::Status(B_OK);
}

SValue BinderPerformance::RunDbGetColumnValueTest()
{
#if TEST_PALMOS_APIS
#ifndef LINUX_DEMO_HACK
	status_t err;
	DatabaseID dbID;
	DmOpenRef dbRef;
	DbSchemaColumnDefnType colDef = { 1, sizeof(uint32_t), "one", dbUInt32 };
	DbTableDefinitionType tableDef = { "table", 1, &colDef };
	uint32_t rowID;
	DbSchemaColumnValueType colVal = { &rowID, sizeof(uint32_t), 1 };
	void * val;
	uint32_t size;

	err = DbCreateDatabase("bperf_db_cursor_open_test", 'bprf', 'bprf', 1, &tableDef, &dbID);
	if (err) {
		return SValue::Status(err);
	}

	dbRef = DbOpenDatabase(dbID, dmModeReadWrite, dbShareNone);
	if (!dbRef) {
		DmDeleteDatabase(dbID);
		return SValue::Status(dmErrInvalidDatabaseName);
	}

	rowID = 42;
	err = DbInsertRow(dbRef, "table", 1, &colVal, &rowID);
	if (err) {
		DbCloseDatabase(dbRef);
		DmDeleteDatabase(dbID);
		return SValue::Status(err);
	}

	RestartDataSize();
	Timer t(m_iterations*10);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		DbGetColumnValue(dbRef, rowID, 1, 0, &val, &size);
		DbReleaseStorage(dbRef, val);
	}
	t.Stop();

	WriteResult(TextOutput(), "DbGetColumnValue (pair)", t);

	DbCloseDatabase(dbRef);
	DmDeleteDatabase(dbID);
#endif
#endif

	return SValue::Status(B_OK);
}


#define	STEP_COUNT	(5+32)
#define	LOOP()	if (count & mask) tmp = ((tmp<<1) | (tmp>>31)) + 0x13ef589dL; else tmp = ((tmp<<1) | (tmp>>31)) - 0x3490decbL; \
				if (--count == 0) goto end_loop


uint32_t execute_test(int32_t loop_count, int32_t step, int32_t mask)
{
	int32_t		count;
	uint32_t	tmp;

	tmp = 0x2589ebfaL;
end_loop:
	if (loop_count <= 0) return tmp;
	loop_count -= step;
	count = step;

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	goto end_loop;
}

#ifndef LINUX_DEMO_HACK
SValue BinderPerformance::RunICacheTest()
{
	static int32_t	steps[STEP_COUNT] =
				{ 1, 2, 4, 8, 16,
				32, 64, 96, 128, 160, 192, 224, 256,
				288, 320, 352, 384, 416, 448, 480, 512,
				544, 576, 608, 640, 672, 704, 736, 768,
				800, 832, 864, 896, 928, 960, 992, 1024 };

	int32_t		mask, i_step, step;
	uint32_t		value;
	nsecs_t time;
	int fdc;
	status_t err;

	//for (mask=-1; mask<=1; mask++)

	mask = 0;
	
	{
		for (i_step=0; i_step<STEP_COUNT; i_step++) {
			int64_t now;
			step = steps[i_step];
			// start timer here
			now = KALGetTime(B_TIMEBASE_RUN_TIME);

			value = execute_test(0x800000L, step, mask);

			now = KALGetTime(B_TIMEBASE_RUN_TIME) - now;
			printf("%ld\t0x%08lx\t0x%08lx\t%ld\n", step, mask, value, (int32_t)(B_NS2US(now)));
		}
	}

	return SValue::Status(B_OK);
}
#endif

SValue BinderPerformance::RunDmIsReadyTest()
{
#if !LINUX_DEMO_HACK
	Timer t(m_iterations*10);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		ErrFatalError("BinderPerformance::RunDmIsReadyTest not implemeneted");
		//DmIsReady();
	}
	t.Stop();
	WriteResult(TextOutput(), "DmIsReady", t);
#endif

	return SValue::Status(B_OK);
}


#if TEST_PALMOS_APIS
static SValue 
setup_db( const DmOpenRef& ref, const uint32_t iterations, uint32_t* rowIDs )
{
	status_t err;
	uint32_t* r = rowIDs;
	uint32_t* limit = r + iterations;
	uint32_t data = 1;

	DbSchemaColumnValueType colVal = { &data, sizeof(uint32_t), 1, 0, 0, 0 };

	do {
		err = DbInsertRow( ref, "table", 1, &colVal, r );
		data++;
		r++;
		if (err) {
			return SValue::Status(err);
		}
	} while ( r < limit );
	return SValue::Status(B_OK);
}
#endif

SValue BinderPerformance::RunDmReadTest()
{
#if TEST_PALMOS_APIS
	static const uint32_t maxID = 1600;
	status_t err;
	DatabaseID dbID;
	DmOpenRef dbRef;
	DbSchemaColumnDefnType colDef = { 1, sizeof(uint32_t), "one", dbUInt32, 0, 0, 0 };
	DbTableDefinitionType tableDef = { "table", 1, &colDef };
	uint32_t rowID;
	uint32_t size;
	DbSchemaColumnValueType colVal = { &rowID, sizeof(uint32_t), 1, 0, 0, 0 };
	uint32_t val;
	uint32_t* rowIDs;
	SValue result;

	err = DbCreateDatabase("bperf_db_read_test", 'bprf', 'bprf', 1, &tableDef, &dbID);
	if (err) {
		return SValue::Status(err);
	}

	dbRef = DbOpenDatabase(dbID, dmModeReadWrite, dbShareNone);
	if (!dbRef) {
		DmDeleteDatabase(dbID);
		return SValue::Status(dmErrInvalidDatabaseName);
	}

	rowIDs = new uint32_t[ m_iterations ];
	result = setup_db( dbRef, m_iterations, rowIDs);
	if ( result != SValue::Status(B_OK) ) {
		DbCloseDatabase(dbRef);
		DmDeleteDatabase(dbID);
		delete [] rowIDs;
		return result;
	}

	uint32_t* r = rowIDs;
	Timer t(m_iterations);
	t.Start();
	for (int32_t i=0; i<t.N; i++) {
		DbCopyColumnValue( dbRef, *r, 1, 0, &val, &size);
	}
	t.Stop();

	WriteResult(TextOutput(), "DbReadTest: DbGetColumnValue (pair)", t);

	DbCloseDatabase(dbRef);
	DmDeleteDatabase(dbID);
	delete [] rowIDs;
#endif

	return SValue::Status(B_OK);
}

extern void icache_test_impl();

SValue BinderPerformance::RunICacheTest()
{
	icache_test_impl();
	return SValue::Status(B_OK);
}

sptr<IBinder> InstantiateComponent(const SString& component, const SContext& context, const SValue& args)
{
	sptr<IBinder> obj;

	if (component == "")
	{
		obj = new BinderPerformance(context);
	}
	else if (component == "TransactionTest")
	{
		obj = new TransactionTest(context, args);
	}
	else if (component == "ImplementsIProcess")
	{
		obj = new B_NO_THROW RemoteObjectProcess();
	}
#if TEST_PALMOS_APIS
	else if (component == "ImplementsIView")
	{
		obj = new RemoteObjectView();
	}
#endif

	return obj;
}
