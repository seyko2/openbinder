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

#ifndef _SUPPORT_BUILD_H
#define _SUPPORT_BUILD_H

/*!	@file support/SupportBuild.h
	@ingroup CoreSupportUtilities
	@brief Build declarations for support framework.
*/

#if (defined(__CC_ARM) || defined(_MSC_VER))
#define _SUPPORTS_WARNING					0
#define _SUPPORTS_FEATURE_SHORT_PATH_MAX	0
#endif

#include <BuildDefaults.h>

#if TARGET_HOST == TARGET_HOST_WIN32
#	define _SUPPORTS_WINDOWS_FILE_PATH 1
#else
#	define _SUPPORTS_UNIX_FILE_PATH 1
#endif

#ifndef LIBBE_BOOTSTRAP
#define LIBBE_BOOTSTRAP 0
#endif

#ifndef _SUPPORTS_FEATURE_SHORT_PATH_MAX
#define _SUPPORTS_FEATURE_SHORT_PATH_MAX 0
#endif

#ifndef _NO_INLINE_ASM
#define _NO_INLINE_ASM 0
#endif

#if defined(NO_RUNTIME_SHARED_LIBRARIES) && defined(__arm)
	// prevent r9 (sb) from being used in PalmOS so that changes
	// we make to it are maintained for applications
	__global_reg(6)   unsigned long sb;
#endif

#if defined(__GNUC__)
#define _UNUSED(x) x
#define _PACKED	__attribute__((packed))
#endif

#ifdef _MSC_VER
//	This is warning is not particularly useful, and causes lots of spewage when
//	the StaticValue.h macros are used.
#pragma warning (disable:4003)
//	The following warning actually -is- useful, but in many places we do things
//	that instigate it where there isn't really a problem.
#pragma warning (disable:4355)
// disable warning C4800: 'int' : forcing value to bool 'true' or 'false' (performance warning)
#if _MSC_VER <= 1300
#pragma warning (disable:4800)
#endif
#endif

#ifndef _BUILDING_SUPPORT
#define _BUILDING_SUPPORT 0
#endif

#if LIBBE_BOOTSTRAP
#undef _EXPORT
#undef _IMPORT
#define _EXPORT
#define _IMPORT
#endif

#if _BUILDING_SUPPORT
#define	_IMPEXP_SUPPORT		_EXPORT
#else
#define	_IMPEXP_SUPPORT		_IMPORT
#endif

#ifdef __cplusplus
extern "C++" { // the BeOS standard C++ library is indirectly including this file inside an extern "C"

#if _SUPPORTS_NAMESPACE
namespace palmos {
#endif

#if _SUPPORTS_NAMESPACE
namespace package {
#endif

#if TARGET_HOST == TARGET_HOST_WIN32
class _IMPEXP_SUPPORT BPackageManager;
#endif

#if _SUPPORTS_NAMESPACE
} // package
#endif


#if _SUPPORTS_NAMESPACE
namespace storage {
#endif

class _IMPEXP_SUPPORT BFile;

#if _SUPPORTS_NAMESPACE
} // storage
#endif



#if _SUPPORTS_NAMESPACE
namespace support {
#endif

/*	These are always exported because they are really just collections of
	inline functions.  If we don't export them, we get warnings from any
	"real" classes using them, which themselves are being exported
*/
template<class TYPE> class _EXPORT atom_ptr;
template<class TYPE> class _EXPORT atom_ref;
template<class TYPE> class _EXPORT SVector;
template<class TYPE> class _EXPORT SSortedVector;
template<class KEY, class VALUE> class _EXPORT SKeyedVector;
template<class INTERFACE> class _EXPORT BnInterface;

/* support kit */
#if TARGET_HOST == TARGET_HOST_WIN32
class _IMPEXP_SUPPORT SAbstractList;
class _IMPEXP_SUPPORT SAbstractSortedVector;
class _IMPEXP_SUPPORT SAbstractVector;
class _IMPEXP_SUPPORT BArmInputStream;
class _IMPEXP_SUPPORT BArmOutputStream;
class _IMPEXP_SUPPORT SAtom;
class _IMPEXP_SUPPORT BBinder;
class _IMPEXP_SUPPORT SBitfield;
class _IMPEXP_SUPPORT BByteStream;
class _IMPEXP_SUPPORT BBufferedPipe;
class _IMPEXP_SUPPORT SCallStack;
class _IMPEXP_SUPPORT BContainer;
class _IMPEXP_SUPPORT BContext;
class _IMPEXP_SUPPORT BController;
class _IMPEXP_SUPPORT BDictionary;
class _IMPEXP_SUPPORT BFlattenable;
class _IMPEXP_SUPPORT SFloatDump;
// class _IMPEXP_SUPPORT BHandler;
class _IMPEXP_SUPPORT SHasher;
class _IMPEXP_SUPPORT SHexDump;
class _IMPEXP_SUPPORT BSharedObject;
class _IMPEXP_SUPPORT BKernelIStr;
class _IMPEXP_SUPPORT BKernelOStr;
class _IMPEXP_SUPPORT SLocker;
class _IMPEXP_SUPPORT SLooper;
class _IMPEXP_SUPPORT BMallocStore;
class _IMPEXP_SUPPORT BMemoryStore;
class _IMPEXP_SUPPORT BMemory;
class _IMPEXP_SUPPORT BLocalMemory;
class _IMPEXP_SUPPORT BMemoryHeap;
class _IMPEXP_SUPPORT SMessage;
class _IMPEXP_SUPPORT SMessageList;
class _IMPEXP_SUPPORT SNestedLocker;
class _IMPEXP_SUPPORT SParcel;
class _IMPEXP_SUPPORT BPipe;
class _IMPEXP_SUPPORT SPrintf;
class _IMPEXP_SUPPORT SReadWriteLocker;
class _IMPEXP_SUPPORT BRoot;
class _IMPEXP_SUPPORT SSharedBuffer;
class _IMPEXP_SUPPORT BStreamInputPipe;
class _IMPEXP_SUPPORT BStreamOutputPipe;
class _IMPEXP_SUPPORT BStreamPipe;
class _IMPEXP_SUPPORT SString;
class _IMPEXP_SUPPORT BStringBuffer;
class _IMPEXP_SUPPORT BStringIO;
class _IMPEXP_SUPPORT BProcess;
class _IMPEXP_SUPPORT BTextOutput;
class _IMPEXP_SUPPORT STypeCode;
class _IMPEXP_SUPPORT SUrl;
class _IMPEXP_SUPPORT SValue;
class _IMPEXP_SUPPORT SVectorIO;
class _IMPEXP_SUPPORT IBinder;
class _IMPEXP_SUPPORT IByteInput;
class _IMPEXP_SUPPORT IByteOutput;
class _IMPEXP_SUPPORT IByteSeekable;
class _IMPEXP_SUPPORT BnFeatures;
class _IMPEXP_SUPPORT BnVaultManagerServer;
class _IMPEXP_SUPPORT IHandler;
class _IMPEXP_SUPPORT IInterface;
class _IMPEXP_SUPPORT IManifestParser;
class _IMPEXP_SUPPORT IMemory;
class _IMPEXP_SUPPORT IMemoryHeap;
class _IMPEXP_SUPPORT IMemoryDealer;
class _IMPEXP_SUPPORT IStorage;
class _IMPEXP_SUPPORT ITask;
class _IMPEXP_SUPPORT IProcess;
class _IMPEXP_SUPPORT ITextOutput;
class _IMPEXP_SUPPORT IValueInput;
class _IMPEXP_SUPPORT IValueOutput;
class _IMPEXP_SUPPORT IVirtualMachine;
class _IMPEXP_SUPPORT BnByteInput;
class _IMPEXP_SUPPORT BnByteOutput;
class _IMPEXP_SUPPORT BnByteSeekable;
class _IMPEXP_SUPPORT BnHandler;
class _IMPEXP_SUPPORT BnMemory;
class _IMPEXP_SUPPORT BnMemoryHeap;
class _IMPEXP_SUPPORT BnMemoryDealer;
class _IMPEXP_SUPPORT BnStorage;
class _IMPEXP_SUPPORT BnTask;
class _IMPEXP_SUPPORT BnProcess;
class _IMPEXP_SUPPORT BnValueInput;
class _IMPEXP_SUPPORT BnValueOutput;
class _IMPEXP_SUPPORT BnVirtualMachine;
class _IMPEXP_SUPPORT SFlattenable;
class _IMPEXP_SUPPORT BpAtom;

class _IMPEXP_SUPPORT SKeyID;
class _IMPEXP_SUPPORT SCallTreeNode;
class _IMPEXP_SUPPORT SCallTree;
class _IMPEXP_SUPPORT SEventFlag;
class _IMPEXP_SUPPORT SRegExp;
#endif

#if _SUPPORTS_NAMESPACE
} // namespace support
#endif

// services kit

#if _SUPPORTS_NAMESPACE
namespace services {
#endif

#if TARGET_HOST == TARGET_HOST_WIN32
class _IMPEXP_SUPPORT IFeatures;
#endif

#if TARGET_HOST == TARGET_HOST_WIN32
class _IMPEXP_SUPPORT IVaultManagerServer;
#endif

#if _SUPPORTS_NAMESPACE
} // namespace services
#endif


// xml kit

#if _SUPPORTS_NAMESPACE
namespace xml {
#endif

#if TARGET_HOST == TARGET_HOST_WIN32
class _IMPEXP_SUPPORT BCreator;
class _IMPEXP_SUPPORT BOutputStream;
class _IMPEXP_SUPPORT BParser;
class _IMPEXP_SUPPORT BWriter;
class _IMPEXP_SUPPORT BXML2ValueCreator;
class _IMPEXP_SUPPORT BXML2ValueParser;
class _IMPEXP_SUPPORT BXMLBufferSource;
class _IMPEXP_SUPPORT BXMLDataSource;
class _IMPEXP_SUPPORT BXMLIByteInputSource;
class _IMPEXP_SUPPORT BXMLParseContext;
class _IMPEXP_SUPPORT BXMLParser;
class _IMPEXP_SUPPORT CXMLOStr;
class _IMPEXP_SUPPORT IXMLOStr;
#endif

#if _SUPPORTS_NAMESPACE
} // namespace xml
#endif

#if _SUPPORTS_NAMESPACE
} // namespace palmos::
#endif

} // extern "C++"
#endif		/* __cplusplus */

#endif
