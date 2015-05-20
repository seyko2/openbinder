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

#include <pthread.h> // POSIX
                     
                     
/* Open up a shared file to synchrize the mutex across processes. */

pthread_mutex_t * mutexPtr;

static void get_mutexPtr(void)
{
  int fd = open("/tmp/pos-mutex", O_CREAT|O_RDWR, 0666);
  assert(fd != -1);
    
  ftruncate(fd, sizeof(pthread_mutex_t));
  
  mutexPtr = (pthread_mutex_t*)mmap(NULL, sizeof(pthread_mutex_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  assert(mutexPtr);
  
  pthread_mutex_init(mutexPtr, NULL);
}


void __attribute__((constructor)) libpalmroot_get_arm_mutexPtr(void) 
{
  get_mutexPtr(); // Pre-load the mutex ptr _before_ we run any code that needs atomic ops!
}

int32_t SysAtomicInc32(volatile int32_t* location)
{
        int32_t result;

        pthread_mutex_lock(mutexPtr);
        result = *location;
        *location += 1;
        pthread_mutex_unlock(mutexPtr);
        
        return result;
}

int32_t SysAtomicDec32(volatile int32_t* location)
{
        int32_t result;

        pthread_mutex_lock(mutexPtr);
        result = *location;
        *location -= 1;
        pthread_mutex_unlock(mutexPtr);
        
        return result;
}

int32_t SysAtomicAdd32(volatile int32_t* location, int32_t addvalue)
{
        int32_t result;

        pthread_mutex_lock(mutexPtr);
        result = *location;
        *location  += addvalue;
        pthread_mutex_unlock(mutexPtr);
        
        return result;
}

int32_t SysAtomicSub32(volatile int32_t* location, int32_t subvalue)
{
        int32_t result;

        pthread_mutex_lock(mutexPtr);
        result = *location;
        *location  -= subvalue;
        pthread_mutex_unlock(mutexPtr);
        
        return result;
}

uint32_t SysAtomicAnd32(volatile uint32_t* location, uint32_t andvalue)
{
        int32_t result;

        pthread_mutex_lock(mutexPtr);
        result = *location;
        *location  &= andvalue;
        pthread_mutex_unlock(mutexPtr);
        
        return result;
}

uint32_t SysAtomicOr32(volatile uint32_t* location, uint32_t orvalue)
{
        int32_t result;

        pthread_mutex_lock(mutexPtr);
        result = *location;
        *location  |= orvalue;
        pthread_mutex_unlock(mutexPtr);
        
        return result;
}

uint32_t SysAtomicCompareAndSwap32(volatile uint32_t* location, uint32_t oldvalue, uint32_t newvalue)
{
        int32_t result;

        pthread_mutex_lock(mutexPtr);
        if (*location == oldvalue) {
          *location = newvalue;
          result = 0;
        } else {
          result = 1;
        }
        pthread_mutex_unlock(mutexPtr);
        
        return result;
}
