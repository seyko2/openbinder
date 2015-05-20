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

#ifndef _SUPPORT_ATOMIC_H
#define _SUPPORT_ATOMIC_H

/*!	@file support/atomic.h
	@ingroup CoreSupportUtilities
	@brief Utilities for atomic integer operations.
*/

#include <support/SupportDefs.h>

#include <SysThread.h>

#ifdef __cplusplus

//!	Convenience wrapper for an atomic integer.
/*!	@ingroup CoreSupportUtilities */
struct SAtomicInt32
{
public:
	inline	int32_t				Value() const;
	inline	void				SetTo(int32_t newVal);

	inline	int32_t				Add(int32_t amount);
	inline	bool				Swap(int32_t oldVal, int32_t newVal);

	inline	uint32_t			Or(uint32_t bits);
	inline	uint32_t			And(uint32_t bits);

private:
	volatile int32_t			m_value;
};

extern "C" {

#endif	// __cplusplus

/* not in the SDK, but exported for PDK use */
uint32_t SysAtomicCompareAndSwap64(uint64_t volatile *ioOperandP, uint64_t iOldValue, uint64_t iNewValue);

/*
 * Lightweight locking (implemented in Locker.cpp)
 */
extern void _IMPEXP_SUPPORT SysCriticalSectionEnter(SysCriticalSectionType *iCS);
extern void _IMPEXP_SUPPORT SysCriticalSectionExit(SysCriticalSectionType *iCS);

/*
 * Lightweight condition variables (also in Locker.cpp)
 */
extern void _IMPEXP_SUPPORT SysConditionVariableWait(SysConditionVariableType *iCV, SysCriticalSectionType *iOptionalCS);
extern void _IMPEXP_SUPPORT SysConditionVariableOpen(SysConditionVariableType *iCV);
extern void _IMPEXP_SUPPORT SysConditionVariableClose(SysConditionVariableType *iCV);
extern void _IMPEXP_SUPPORT SysConditionVariableBroadcast(SysConditionVariableType *iCV);

extern int32_t	_IMPEXP_SUPPORT atomic_add(volatile int32_t *value, int32_t addvalue);
extern int32_t	_IMPEXP_SUPPORT atomic_and(volatile int32_t *value, int32_t andvalue);
extern int32_t	_IMPEXP_SUPPORT atomic_or(volatile int32_t *value, int32_t orvalue);
extern int32_t	_IMPEXP_SUPPORT compare_and_swap32(volatile int32_t *location, int32_t oldValue, int32_t newValue);
extern int32_t	_IMPEXP_SUPPORT compare_and_swap64(volatile int64_t *location, int64_t oldValue, int64_t newValue);

#if TARGET_HOST == TARGET_HOST_WIN32


// Synonyms from PalmOS.
inline int32_t SysAtomicAdd32(int32_t volatile* ioOperandP, int32_t iAddend)
	{ return (int32_t)atomic_add((int32_t volatile*)ioOperandP, (int32_t)iAddend); }
inline uint32_t SysAtomicAnd32(uint32_t volatile* ioOperandP, uint32_t iValue)
	{ return (uint32_t)atomic_and((int32_t volatile*)ioOperandP, (int32_t)iValue); }
inline uint32_t SysAtomicOr32(uint32_t volatile* ioOperandP, uint32_t iValue)
	{ return (uint32_t)atomic_or((int32_t volatile*)ioOperandP, (int32_t)iValue); }
inline uint32_t SysAtomicCompareAndSwap32(uint32_t volatile* ioOperandP, uint32_t iOldValue, uint32_t iNewValue)
	{ return (uint32_t)!compare_and_swap32((int32_t volatile*)ioOperandP, (int32_t)iOldValue, (int32_t)iNewValue); }
inline uint32_t SysAtomicCompareAndSwap64(uint64_t volatile* ioOperandP, uint64_t iOldValue, uint64_t iNewValue)
	{ return (uint32_t)!compare_and_swap64((int64_t volatile*)ioOperandP, (int64_t)iOldValue, (int64_t)iNewValue); }

#endif	// #if TARGET_HOST == TARGET_HOST_WIN32

//!	Perform a 32-bit atomic compare and swap
/*!	If the current value of @a *atom is the same as the current value
	of @a *value, atomically sets @a *atom to @a newValue and returns TRUE.
	Otherwise, sets @a *value to the current value of @a *atom and
	returns FALSE.
	@ingroup CoreSupportUtilities */
INLINE_FNC int cmpxchg32(volatile int32_t *atom, int32_t *value, int32_t newValue)
{
	if (SysAtomicCompareAndSwap32((volatile uint32_t*)atom, *value, newValue)) {
		// Failed.
		*value = *atom;
		return FALSE;
	}

	return TRUE;
}

//!	Perform a 64-bit atomic compare and swap
/*!	If the current value of @a *atom is the same as the current value
	of @a *value, atomically sets @a *atom to @a newValue and returns TRUE.
	Otherwise, sets @a *value to the current value of @a *atom and
	returns FALSE.
	@ingroup CoreSupportUtilities */
INLINE_FNC int cmpxchg64(volatile int64_t *atom, int64_t *value, int64_t newValue)
{
	int32_t success = compare_and_swap64(atom, *value, newValue);
	if (!success)
		*value = *atom;

	return success ? TRUE : FALSE;
}

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

#ifdef __cplusplus
}

inline int32_t SAtomicInt32::Value() const
{
	return m_value;
}

inline void SAtomicInt32::SetTo(int32_t newVal)
{
	m_value = newVal;
}

inline int32_t SAtomicInt32::Add(int32_t amount)
{
	return SysAtomicAdd32(&m_value, amount);
}

inline bool SAtomicInt32::Swap(int32_t oldVal, int32_t newVal)
{
	return SysAtomicCompareAndSwap32((volatile uint32_t*)&m_value, oldVal, newVal) == 0;
}

inline uint32_t SAtomicInt32::Or(uint32_t bits)
{
	return SysAtomicOr32((volatile uint32_t*)&m_value, bits);
}

inline uint32_t SAtomicInt32::And(uint32_t bits)
{
	return SysAtomicAnd32((volatile uint32_t*)&m_value, bits);
}

#endif	// __cplusplus

#endif /* _SUPPORT_ATOMIC_H */
