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
#include <asm/atomic.h>	// Linux
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>

#if 1 /* use kernel cmpxchg helper */

/*  From 2.6 arch/arm/kernel/entry-armv.S

 * Definition and user space usage example:
 *
 *      typedef int (__kernel_cmpxchg_t)(int oldval, int newval, int *ptr);
 *      #define __kernel_cmpxchg (*(__kernel_cmpxchg_t *)0xffff0fc0)
 *
 * Atomically store newval in *ptr if *ptr is equal to oldval for user space.
 * Return zero if *ptr was changed or non-zero if no exchange happened.
 * The C flag is also set if *ptr was changed to allow for assembly
 * optimization in the calling code.
 *
 * For example, a user space atomic_add implementation could look like this:
 *
 * #define atomic_add(ptr, val) \
 *      ({ register unsigned int *__ptr asm("r2") = (ptr); \
 *         register unsigned int __result asm("r1"); \
 *         asm volatile ( \
 *             "1: @ atomic_add\n\t" \
 *             "ldr     r0, [r2]\n\t" \
 *             "mov     r3, #0xffff0fff\n\t" \
 *             "add     lr, pc, #4\n\t" \
 *             "add     r1, r0, %2\n\t" \
 *             "add     pc, r3, #(0xffff0fc0 - 0xffff0fff)\n\t" \
 *             "bcc     1b" \
 *             : "=&r" (__result) \
 *             : "r" (__ptr), "rIL" (val) \
 *             : "r0","r3","ip","lr","cc","memory" ); \
 *         __result; })
 */


typedef int (__kernel_cmpxchg_t)(int oldval, int newval, volatile int *ptr);
#define __kernel_cmpxchg (*(__kernel_cmpxchg_t *)0xffff0fc0)

uint32_t SysAtomicCompareAndSwap32(volatile uint32_t* location, uint32_t oldvalue, uint32_t newvalue)
{
	register uint32_t __oldvalue = oldvalue;
	register uint32_t __newvalue asm("r1") = newvalue;
	register volatile uint32_t *__ptr asm("r2") = (location);
	register uint32_t __result asm("r0");
	do {
		asm volatile (
			"mov     r3, #0xffff0fff;"
			"add     lr, pc, #4;"
			"mov     r0, %2;"
			"add     pc, r3, #(0xffff0fc0 - 0xffff0fff)"
			: "=&r" (__result)
			: "r" (__ptr), "rIL" (__oldvalue), "rIL" (__newvalue)
			: "r3","ip","lr","cc","memory");
	} while(__result && *__ptr == __oldvalue);
	return __result;
}

#if 0
// slightly faster, but does not compile without warnings
uint32_t __attribute__((naked)) SysAtomicCompareAndSwap32(volatile uint32_t* location, uint32_t oldvalue, uint32_t newvalue)
{
	asm volatile (
		"stmdb   sp!, {r4, lr};"
		"mov     r4, r1;"
		"mov     r1, r2;"
		"mov     r2, r0;"
		"1: @ SysAtomicCompareAndSwap32;\n"
		"mov     r3, #0xffff0fff;"
		"add     lr, pc, #4;"
		"mov     r0, r4;"
		"add     pc, r3, #(0xffff0fc0 - 0xffff0fff);"
		"ldmcsia sp!, {r4, pc};"
		"ldr     r0, [r2];"
		"cmp     r0, r1;"
		"beq     1b;"
		"mov     r0, #1;"
		"ldmia   sp!, {r4, pc}" );
}
#endif

int32_t SysAtomicInc32(volatile int32_t* location)
{
	register volatile int32_t *__ptr asm("r2") = (location);
	register int32_t __result asm("r1");
	asm volatile (
		"1: @ SysAtomicInc32\n\t"
		"ldr     r0, [r2]\n\t"
		"mov     r3, #0xffff0fff\n\t"
		"add     lr, pc, #4\n\t"
		"add     r1, r0, #1\n\t"
		"add     pc, r3, #(0xffff0fc0 - 0xffff0fff)\n\t"
		"bcc     1b"
		: "=r" (__result)
		: "r" (__ptr)
		: "r0","r3","ip","lr","cc","memory" );
	return __result - 1;
}

int32_t SysAtomicDec32(volatile int32_t* location)
{
	register volatile int32_t *__ptr asm("r2") = (location);
	register int32_t __result asm("r1");
	asm volatile (
		"1: @ SysAtomicDec32\n\t"
		"ldr     r0, [r2]\n\t"
		"mov     r3, #0xffff0fff\n\t"
		"add     lr, pc, #4\n\t"
		"sub     r1, r0, #1\n\t"
		"add     pc, r3, #(0xffff0fc0 - 0xffff0fff)\n\t"
		"bcc     1b"
		: "=r" (__result)
		: "r" (__ptr)
		: "r0","r3","ip","lr","cc","memory" );
	return __result + 1;
}

int32_t SysAtomicAdd32(volatile int32_t* location, int32_t addvalue)
{
	register volatile int32_t *__ptr asm("r2") = (location);
	register int32_t __result asm("r1");
	register int32_t val asm("r4") = addvalue;
	asm volatile (
		"1: @ SysAtomicAdd32\n\t"
		"ldr     r0, [r2]\n\t"
		"mov     r3, #0xffff0fff\n\t"
		"add     lr, pc, #4\n\t"
		"add     r1, r0, %2\n\t"
		"add     pc, r3, #(0xffff0fc0 - 0xffff0fff)\n\t"
		"bcc     1b"
		: "=r" (__result)
		: "r" (__ptr), "rIL" (val)
		: "r0","r3","ip","lr","cc","memory" );
	return __result - val;
}

int32_t SysAtomicSub32(volatile int32_t* location, int32_t subvalue)
{
	register volatile int32_t *__ptr asm("r2") = (location);
	register int32_t __result asm("r1");
	register int32_t val asm("r4") = subvalue;
	asm volatile (
		"1: @ SysAtomicSub32\n\t"
		"ldr     r0, [r2]\n\t"
		"mov     r3, #0xffff0fff\n\t"
		"add     lr, pc, #4\n\t"
		"sub     r1, r0, %2\n\t"
		"add     pc, r3, #(0xffff0fc0 - 0xffff0fff)\n\t"
		"bcc     1b"
		: "=r" (__result)
		: "r" (__ptr), "rIL" (val)
		: "r0","r3","ip","lr","cc","memory" );
	return __result + val;
}

uint32_t SysAtomicAnd32(volatile uint32_t* location, uint32_t andvalue)
{
  uint32_t oldvalue, newvalue;
  do {
    oldvalue = *location;
    newvalue = oldvalue & andvalue;
  } while(__kernel_cmpxchg(oldvalue, newvalue, location));
  return oldvalue;
}

uint32_t SysAtomicOr32(volatile uint32_t* location, uint32_t orvalue)
{
  uint32_t oldvalue, newvalue;
  do {
    oldvalue = *location;
    newvalue = oldvalue | orvalue;
  } while(__kernel_cmpxchg(oldvalue, newvalue, location));
  return oldvalue;
}

#else
                     
                     
/* Implement a crude mutex operation using the ARM's atomic swap mechanism.
   This has to yield to avoid being a spin-lock. Unfortunately, there is
   nothing to prevent priority inversion. */
   
#define GETMUTEX(locationP) 			\
{						\
  uint32_t tmp1, tmp2;				\
    asm volatile				\
    (						\
        "1:"					\
        "mov %1, $1;"				\
        "swp %0, %1, [%2];"			\
        "teq %0, %1;"				\
        "bne 2f;"				\
        "bl sched_yield;"			\
        "b 1b;"					\
        "2:"					\
        : "=&r"(tmp1), "=&r"(tmp2) 		\
        : "r"(locationP)			\
        : "r0", "r1", "r2", "r3", "r12", "lr" \
    );						\
}

#define PUTMUTEX(locationP)			\
{						\
  assert(*locationP == 1); \
  uint32_t tmp1;				\
    asm volatile				\
    (						\
        "1:"					\
        "mov %0, $0;"				\
        "str %0, [%1];"				\
        : "=&r"(tmp1)				\
        : "r"(locationP)			\
    );						\
}

/* Open up a shared file to synchrize the mutex across processes. */

static inline unsigned int * mutexPtr(void)
{
  static unsigned int * ptr;
  if (!ptr) {
    int fd = open("/tmp/pos-mutex", O_CREAT|O_RDWR, 0666);
    assert(fd != -1);
    
    ftruncate(fd, 4);
    
    ptr = (unsigned int*)mmap(NULL, 4, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    assert(ptr);
  }
  return ptr;
}

void __attribute__((constructor)) libpalmroot_get_arm_mutexPtr(void) 
{
  mutexPtr(); // Pre-load the mutex ptr _before_ we run any code that needs atomic ops!
}

int32_t SysAtomicInc32(volatile int32_t* location)
{
        unsigned int * ptr = mutexPtr();
        int32_t result;

        GETMUTEX(ptr);
        result = *location;
        *location += 1;
        PUTMUTEX(ptr);
        
        return result;
}

int32_t SysAtomicDec32(volatile int32_t* location)
{
        unsigned int * ptr = mutexPtr();
        int32_t result;

        GETMUTEX(ptr);
        result = *location;
        *location -= 1;
        PUTMUTEX(ptr);
        
        return result;
}

int32_t SysAtomicAdd32(volatile int32_t* location, int32_t addvalue)
{
        unsigned int * ptr = mutexPtr();
        int32_t result;

        GETMUTEX(ptr);
        result = *location;
        *location  += addvalue;
        PUTMUTEX(ptr);
        
        return result;
}

int32_t SysAtomicSub32(volatile int32_t* location, int32_t subvalue)
{
        unsigned int * ptr = mutexPtr();
        int32_t result;

        GETMUTEX(ptr);
        result = *location;
        *location  -= subvalue;
        PUTMUTEX(ptr);
        
        return result;
}

uint32_t SysAtomicAnd32(volatile uint32_t* location, uint32_t andvalue)
{
        unsigned int * ptr = mutexPtr();
        int32_t result;

        GETMUTEX(ptr);
        result = *location;
        *location  &= andvalue;
        PUTMUTEX(ptr);
        
        return result;
}

uint32_t SysAtomicOr32(volatile uint32_t* location, uint32_t orvalue)
{
        unsigned int * ptr = mutexPtr();
        int32_t result;

        GETMUTEX(ptr);
        result = *location;
        *location  |= orvalue;
        PUTMUTEX(ptr);
        
        return result;
}

uint32_t SysAtomicCompareAndSwap32(volatile uint32_t* location, uint32_t oldvalue, uint32_t newvalue)
{
        unsigned int * ptr = mutexPtr();
        int32_t result;

        GETMUTEX(ptr);
        if (*location == oldvalue) {
          *location = newvalue;
          result = 0;
        } else {
          result = 1;
        }
        PUTMUTEX(ptr);
        
        return result;
}

#endif
