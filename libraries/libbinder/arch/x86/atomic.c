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


#include <SysThread.h>	// PalmOS

/* First, inlinable macro versions, for eventually breaking out
   into the header */

#define SYSATOMICADD32(locationP,addvalue)	\
({						\
	int32_t res;				\
	asm volatile				\
	(					\
		"1:"				\
		"mov %2, %%ecx;"		\
		"mov (%1), %0;"			\
		"add %0, %%ecx;"		\
		"lock; cmpxchg %%ecx, (%1);"	\
		"jnz 1b"			\
		: "=&a"(res)			\
		: "r"(locationP), "r"(addvalue)	\
		: "%ecx"			\
	);					\
	res;					\
})

#define SYSATOMICSUB32(locationP,subvalue)	\
({						\
	int32_t res;				\
	asm volatile				\
	(					\
		"1:"				\
		"mov %2, %%ecx;"		\
		"mov (%1), %0;"			\
		"sub %0, %%ecx;"		\
		"lock; cmpxchg %%ecx, (%1);"	\
		"jnz 1b"			\
		: "=&a"(res)			\
		: "r"(locationP), "r"(subvalue)	\
		: "%ecx"			\
	);					\
	res;					\
})

#define SYSATOMICOR32(locationP,orvalue)	\
({						\
	uint32_t res;				\
	asm volatile				\
	(					\
		"1:"				\
		"mov %2, %%ecx;"		\
		"mov (%1), %0;"			\
		"or %0, %%ecx;"			\
		"lock; cmpxchg %%ecx, (%1);"	\
		"jnz 1b"			\
		: "=&a"(res)			\
		: "r"(locationP), "r"(orvalue)	\
		: "%ecx"			\
	);					\
	res;					\
})

#define SYSATOMICAND32(locationP,andvalue)	\
({						\
	uint32_t res;				\
	asm volatile				\
	(					\
		"1:"				\
		"mov %2, %%ecx;"		\
		"mov (%1), %0;"			\
		"and %0, %%ecx;"		\
		"lock; cmpxchg %%ecx, (%1);"	\
		"jnz 1b"			\
		: "=&a"(res)			\
		: "r"(locationP), "r"(andvalue)	\
		: "%ecx"			\
	);					\
	res;					\
})

#define SYSATOMICCMPXCHG32(locationP,oldvalue,newvalue)	\
({								\
	uint32_t success;					\
	asm volatile						\
	(							\
	 	"lock; cmpxchg %%ecx, (%%edx);"			\
                " sete %%al;"					\
                " andl $1, %%eax"				\
		: "=a" (success) 				\
		: "a" (oldvalue), "c" (newvalue), "d" (location)\
	);							\
	success;						\
})

#define SYSATOMICINC32(locationP) SYSATOMICADD32(locationP,1)
#define SYSATOMICDEC32(locationP) SYSATOMICADD32(locationP,-1)

/* Next, the actual exports */

int32_t SysAtomicInc32(volatile int32_t* location)
{
	return SYSATOMICINC32(location);
}

int32_t SysAtomicDec32(volatile int32_t* location)
{
	return SYSATOMICDEC32(location);
}

int32_t SysAtomicAdd32(volatile int32_t* location, int32_t addvalue)
{
        return SYSATOMICADD32(location, addvalue);
}

int32_t SysAtomicSub32(volatile int32_t* location, int32_t subvalue)
{
        return SYSATOMICSUB32(location, subvalue);
}

uint32_t SysAtomicAnd32(volatile uint32_t* location, uint32_t andvalue)
{
        return SYSATOMICAND32(location, andvalue);
}

uint32_t SysAtomicOr32(volatile uint32_t* location, uint32_t orvalue)
{
        return SYSATOMICOR32(location, orvalue);
}

uint32_t SysAtomicCompareAndSwap32(volatile uint32_t* location, uint32_t oldvalue, uint32_t newvalue)
{
        return !SYSATOMICCMPXCHG32(location, oldvalue, newvalue);
}
