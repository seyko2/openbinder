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

#ifndef _SYSTHREADCONCEALED_H_
#define _SYSTHREADCONCEALED_H_

#include <SysThread.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Once flag are a mix up between barriers and critical
 * sections. We could do without them except that for
 * ARMs EABI we only have four bytes to play with, hence
 * we need a new primitive type.
 *
 * For the time being these are kept as private since
 * they are only intended for Compiler Writers for
 * EABI support, in a future it may get to the SDK
 * or exported through a pthreads compatibility library.
 * 
 * See extended note in KALTSD.c
 */
typedef uint32_t volatile SysOnceFlagType;
enum {
	sysOnceFlagInitializer= 0,
	sysOnceFlagPending    = sysOnceFlagInitializer,
	sysOnceFlagDone       = 1,
	sysOnceFlagXYZZY      = 0x7FFFFFFF,
};
uint32_t SysOnceFlagTest(SysOnceFlagType *iOF);
uint32_t SysOnceFlagSignal(SysOnceFlagType *iOF, uint32_t result);

/*
 * Some conveniences for getting information about the
 * caller's process.  Should probably be moved to the SDK
 * at some point.
 */
uint32_t SysProcessID(void);
const char* SysProcessName(void);

/*
 * These give a faster way to perform some of the low-
 * level operations, by avoiding the overhead of a
 * shared library call.
 */
typedef struct sysThreadDirectFuncsTag
{
	// Upon calling SysThreadGetDirectFuncs(), this contains the number
	// of funcs available in the structure; upon return it contains the
	// number that were filled in.
	int32_t		numFuncs;

	int32_t		(*atomicInc32)(int32_t volatile *ioOperandP);
	int32_t		(*atomicDec32)(int32_t volatile *ioOperandP);
	int32_t		(*atomicAdd32)(int32_t volatile *ioOperandP, int32_t iAddend);
	uint32_t	(*atomicAnd32)(uint32_t volatile *ioOperandP, uint32_t iValue);
	uint32_t	(*atomicOr32)(uint32_t volatile *ioOperandP, uint32_t iValue);
	uint32_t	(*atomicCompareAndSwap32)(uint32_t volatile *ioOperandP, uint32_t iOldValue, uint32_t iNewValue);

	void*		(*tsdGet)(SysTSDSlotID tsdslot);
	void		(*tsdSet)(SysTSDSlotID tsdslot, void *iValue);

	void		(*criticalSectionEnter)(SysCriticalSectionType *iCS);
	void		(*criticalSectionExit)(SysCriticalSectionType *iCS);

	void		(*conditionVariableWait)(SysConditionVariableType *iCV, SysCriticalSectionType *iOptionalCS);
	void		(*conditionVariableOpen)(SysConditionVariableType *iCV);
	void		(*conditionVariableClose)(SysConditionVariableType *iCV);
	void		(*conditionVariableBroadcast)(SysConditionVariableType *iCV);

} sysThreadDirectFuncs;

// This is the number of functions in the current structure.
enum {
	sysThreadDirectFuncsCount = 14
};

status_t SysThreadGetDirectFuncs(sysThreadDirectFuncs* ioFuncs);

#ifdef __cplusplus
}	// extern "C"
#endif

#endif // _SYSTHREADCONCEALED_H_
