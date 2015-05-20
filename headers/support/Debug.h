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

#ifndef _SUPPORT_DEBUG_H
#define _SUPPORT_DEBUG_H

/*!	@file support/Debug.h
	@ingroup CoreSupportUtilities
	@brief Common debugging routines, macros, and definitions.
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <support/atomic.h>
#include <support/SupportDefs.h>
#include <ErrorMgr.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#ifdef __cplusplus
#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif
class SString;
#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
#endif	/* __cplusplus */

/*------------------------------*/
/*----- Private... -------------*/
#ifdef __cplusplus
extern "C" {
#endif
#if TARGET_HOST != TARGET_HOST_PALMOS
	extern int _rtDebugFlag;
#else
	#define _rtDebugFlag true
#endif

	int _debugFlag(void);
	int _setDebugFlag(int);

#if TARGET_HOST == TARGET_HOST_BEOS
	int _debugPrintf(const char *, ...);
	int _sPrintf(const char *, ...);
	int _xdebugPrintf(const char *, ...);
#else
	#define _debugPrintf printf
	#define _sPrintf sprintf
	#define _xdebugPrintf printf
#endif
	
	typedef void (*b_debug_action)(void* data);
	void _exec_debug_action(b_debug_action action, void* data);

#ifdef __cplusplus
}
#endif	// __cplusplus
/*-------- ...to here ----------*/

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/*-------------------------------------------------------------*/
/*!	@name Named Debugging Variables */
//@{

#ifdef __cplusplus
#include <support/Atom.h>
	_IMPEXP_SUPPORT void add_debug_atom(BNS(palmos::support::) SAtom * atom, const char *name); // If it's really a pointer you're giving, don't mess with the ref count, so it's safe to call this from inside a constructor
	_IMPEXP_SUPPORT void add_debug_atom(const BNS(::palmos::support::)sptr<BNS(palmos::support::) SAtom>& atom, const char *name);
	_IMPEXP_SUPPORT void add_debug_sized(const void *key, size_t size, const char *name);
	_IMPEXP_SUPPORT void set_debug_fieldwidth(int width);
	_IMPEXP_SUPPORT BNS(palmos::support::)SString lookup_debug(const BNS(::palmos::support::)sptr<BNS(palmos::support::) SAtom>& atom, bool padding = true);
	_IMPEXP_SUPPORT BNS(palmos::support::)SString lookup_debug(const void *key, bool padding = true);
#endif	// __cplusplus

//@}

/*-------------------------------------------------------------*/
/*!	@name Debugging Macros */
//@{

#if DEBUG
	#define SET_DEBUG_ENABLED(FLAG)	_setDebugFlag(FLAG)
	#define	IS_DEBUG_ENABLED()		_debugFlag()
	
	#define SERIAL_PRINT(ARGS)		_sPrintf ARGS
	#define PRINT(ARGS) 			_debugPrintf ARGS
	#define PRINT_OBJECT(OBJ)		if (_rtDebugFlag) {		\
										PRINT(("%s\t", #OBJ));	\
										(OBJ).PrintToStream(); 	\
										} ((void) 0)
	#define TRACE()					_debugPrintf("File: %s, Line: %d, Thread: %d\n", \
										__FILE__, __LINE__, SysCurrentThread())
		
	#define SERIAL_TRACE()			_sPrintf("File: %s, Line: %d, Thread: %d\n", \
										__FILE__, __LINE__, SysCurrentThread())
	
	#define DEBUGGER(MSG)			if (_rtDebugFlag) ErrFatalError(MSG);
	#if !defined(ASSERT)
		#define ASSERT(E)			(!(E) ? ErrFatalError(#E) : (int)0)
	#endif

	#define ASSERT_WITH_MESSAGE(expr, msg) \
									(!(expr) ? ErrFatalError(msg) : (int)0)	
	
	#define VALIDATE(x, recover)	if (!(x)) { ASSERT(x); recover; }
	
	#define TRESPASS()				if (1) { _debugPrintf("Should not be here at File: %s, Line: %d, Thread: %d\n",__FILE__,__LINE__,SysCurrentThread()); DEBUGGER("DO SOMETHING"); }
	
	#define DEBUG_ONLY(arg)			arg

	#define EXEC_DEBUG_ACTION(action, data) \
									_exec_debug_action(actiom data)
	
#else /* DEBUG == 0 */
	#define SET_DEBUG_ENABLED(FLAG)	(void)0
	#define	IS_DEBUG_ENABLED()		(void)0
	
	#define SERIAL_PRINT(ARGS)		(void)0
	#define PRINT(ARGS)				(void)0
	#define PRINT_OBJECT(OBJ)		(void)0
	#define TRACE()					(void)0
	#define SERIAL_TRACE()			(void)0
	
	#define DEBUGGER(MSG)			(void)0
	#if !defined(ASSERT)
		#define ASSERT(E)				(void)0
	#endif
	#define ASSERT_WITH_MESSAGE(expr, msg) \
									(void)0
	#define VALIDATE(x, recover)	if (!(x)) { recover; }
	#define TRESPASS()				(void)0
	#define DEBUG_ONLY(x)
	#define EXEC_DEBUG_ACTION(action, data) \
									(void)0
#endif


#ifdef __cplusplus

template<bool> struct CompileTimeAssert;
template<> struct CompileTimeAssert<true> {};
#define STATIC_ASSERT(x)		CompileTimeAssert< (x) >()

#else	/* __cplusplus */

#if !defined(__MWERKS__)
	// STATIC_ASSERT is a compile-time check that can be used to
	// verify static expressions such as: STATIC_ASSERT(sizeof(int64_t) == 8);
	#define STATIC_ASSERT(x)								\
		do {												\
            enum { __ASSERT_EXPRESSION__ = 2*(x) - 1 };         \
			struct __staticAssertStruct__ {					\
				char __static_assert_failed__[__ASSERT_EXPRESSION__];	\
			};												\
		} while (false)
#else
	#define STATIC_ASSERT(x) 
	// the STATIC_ASSERT above doesn't work too well with mwcc because
	// of scoping bugs; for now make it do nothing
#endif

#endif /* __cplusplus */

//@}

/*-------------------------------------------------------------*/
/*----- Conditionals (only for C++) ---------------------------*/

#ifdef __cplusplus

	template<const char* ENVNAME, int DEFVAL, int MINVAL, int MAXVAL>
	class BDebugCondition
	{
	public:
		static inline int32_t Get() {
			// The four lines below are courtesy of ADS.  Don't ask.
			(void)ENVNAME;
			(void)DEFVAL;
			(void)MINVAL;
			(void)MAXVAL;
			if (m_hasLevel > 2) return m_level;
			return GetSlow();
		}
		
		static int32_t GetSlow() {
			if (atomic_or(&m_hasLevel, 1) == 0) {
#if TARGET_HOST == TARGET_HOST_PALMOS
				char envBuffer[128];
				const char* env = NULL;
				if (SysIsInstrumentationAllowed()) {
					if (HostGetPreference(ENVNAME, envBuffer)) env = envBuffer;
				}
#else
				const char* env = getenv(ENVNAME);
#endif
				if (env) {
					m_level = atoi(env);
					if (m_level < MINVAL) m_level = MINVAL;
					if (m_level > MAXVAL) m_level = MAXVAL;
				} else {
					m_level = DEFVAL;
				}
				atomic_or(&m_hasLevel, 2);
				if (m_level > 0)
					fprintf(stderr, "%s ENABLED!  %s=%d\n", ENVNAME, ENVNAME, m_level);
			} else {
				while ((m_hasLevel&2) == 0) 
					SysThreadDelay(B_MICROSECONDS(2), B_RELATIVE_TIMEOUT);
			}
			return m_level;
		}
	
	private:
		static int32_t m_hasLevel;
		static int32_t m_level;
	};
	
	template<const char* ENVNAME, int DEFVAL, int MINVAL, int MAXVAL>
	int32_t BDebugCondition<ENVNAME, DEFVAL, MINVAL, MAXVAL>::m_hasLevel = 0;
	template<const char* ENVNAME, int DEFVAL, int MINVAL, int MAXVAL>
	int32_t BDebugCondition<ENVNAME, DEFVAL, MINVAL, MAXVAL>::m_level = 0;

	template<const char* ENVNAME, int DEFVAL, int MINVAL, int MAXVAL>
	class BDebugInteger
	{
	public:
		static inline int32_t Get() {
			// The four lines below are courtesy of ADS.  Don't ask.
			(void)ENVNAME;
			(void)DEFVAL;
			(void)MINVAL;
			(void)MAXVAL;
			if (m_hasLevel > 2) return m_level;
			return GetSlow();
		}
		
		static int32_t GetSlow() {
			if (atomic_or(&m_hasLevel, 1) == 0) {
#if TARGET_HOST == TARGET_HOST_PALMOS
				char envBuffer[128];
				const char* env = NULL;
				if (HostGetPreference(ENVNAME, envBuffer)) env = envBuffer;
#else
				const char* env = getenv(ENVNAME);
#endif
				if (env) {
					m_level = atoi(env);
					if (m_level < MINVAL) m_level = MINVAL;
					if (m_level > MAXVAL) m_level = MAXVAL;
				} else {
					m_level = DEFVAL;
				}
				atomic_or(&m_hasLevel, 2);
				if (env)
					fprintf(stderr, "Adjusting to %s to: %d\n", ENVNAME, m_level);
			} else {
				while ((m_hasLevel&2) == 0) 
					SysThreadDelay(B_MICROSECONDS(2), B_RELATIVE_TIMEOUT);
			}
			return m_level;
		}
	
	private:
		static int32_t m_hasLevel;
		static int32_t m_level;
	};
	
	template<const char* ENVNAME, int DEFVAL, int MINVAL, int MAXVAL>
	int32_t BDebugInteger<ENVNAME, DEFVAL, MINVAL, MAXVAL>::m_hasLevel = 0;
	template<const char* ENVNAME, int DEFVAL, int MINVAL, int MAXVAL>
	int32_t BDebugInteger<ENVNAME, DEFVAL, MINVAL, MAXVAL>::m_level = 0;

	template<class STATE>
	class BDebugState
	{
	public:
		static inline STATE* Get() {
			// The line below is courtesy of ADS.  Don't ask.
			if (m_hasState > 2) return m_state;
			return GetSlow();
		}
		
		static STATE* GetSlow() {
			if (atomic_or(&m_hasState, 1) == 0) {
				m_state = new STATE;
				atomic_or(&m_hasState, 2);
			} else {
				while ((m_hasState&2) == 0)
				    SysThreadDelay(B_MICROSECONDS(2), B_RELATIVE_TIMEOUT);
			}
			return m_state;
		}
	
		~BDebugState()
		{
			if ((atomic_or(&m_hasState, 4)&4) == 0) {
				delete m_state;
			}
		}
		
	private:
		static int32_t m_hasState;
		static STATE* m_state;
	};
	
	template<class STATE> int32_t BDebugState<STATE>::m_hasState = 0;
	template<class STATE> STATE* BDebugState<STATE>::m_state = NULL;

#endif

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

/*!	@} */

#endif /* _SUPPORT_DEBUG_H */
