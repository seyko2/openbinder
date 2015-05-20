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

#include <SysThread.h>
#include <CmnErrors.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

#define MAX_NAMED_PTHREAD_KEYS 64


struct {
  pthread_key_t key;
  uint32_t name;
  SysTSDDestructorFunc * destructor;
} named_pthread_keys[MAX_NAMED_PTHREAD_KEYS] = {};

namespace palmos {
void cleanupNamedTSDKeys(void) // invoked by thread cleanup machinery
{
    for (int i=0; i<sizeof(named_pthread_keys)/sizeof(named_pthread_keys[0]); i++) {
      if (named_pthread_keys[i].name != 0) {
        void * val;
        if (named_pthread_keys[i].destructor && (val = pthread_getspecific(named_pthread_keys[i].key)))
          named_pthread_keys[i].destructor(val);
      }
    }
}
} // namespace palmos

status_t SysTSDAllocate(SysTSDSlotID * oTSDSlot,
                        SysTSDDestructorFunc *iDestructor,
                        uint32_t iName)
{
  int freeNameSlot = -1;
  assert(sizeof(pthread_key_t) == sizeof(SysTSDSlotID)); // SHOULD BE COMPILE TIME ASSERT

  if (iName != sysTSDAnonymous) {
    int i;
    for (i=0; i<sizeof(named_pthread_keys)/sizeof(named_pthread_keys[0]); i++)
      if (named_pthread_keys[i].name == iName) {
        named_pthread_keys[i].destructor = iDestructor;
        *oTSDSlot = (pthread_key_t)named_pthread_keys[i].key;
        return errNone; // Success
      } else if (named_pthread_keys[i].name == 0)
        freeNameSlot=i;
    
    if (freeNameSlot == -1)
      return sysErrParamErr; // Error, too many named keys
  }
  
  pthread_key_t key;
  int err = pthread_key_create(&key, (freeNameSlot == -1) ? iDestructor : NULL);

  if (err == EAGAIN)
    return sysErrParamErr; //kalErrLimitReached; // Error, too many keys

  if (freeNameSlot != -1) {
    named_pthread_keys[freeNameSlot].name = iName;
    named_pthread_keys[freeNameSlot].key = key;
    named_pthread_keys[freeNameSlot].destructor = iDestructor;
  }

  *oTSDSlot = (SysTSDSlotID)key;
  return errNone; // Success
}

status_t SysTSDFree(SysTSDSlotID tsdslot)
{
  pthread_key_t key = (pthread_key_t)tsdslot;

  /* Checked named TSD list, remove if present */
  int i;
  for (i=0; i<sizeof(named_pthread_keys)/sizeof(named_pthread_keys[0]); i++)
    if (named_pthread_keys[i].key == key) {
      named_pthread_keys[i].key = 0;
      named_pthread_keys[i].name = 0;
      named_pthread_keys[i].destructor = 0;
  }

  int err = pthread_key_delete(key); // pthread_key_delete does not invoke destructors
  
  if (err == EINVAL)
    return sysErrParamErr;
  
  return errNone; // Success
}

void SysTSDSet(SysTSDSlotID tsdslot, void * iValue) 
{
  pthread_setspecific((pthread_key_t)tsdslot, iValue);
}

void * SysTSDGet(SysTSDSlotID tsdslot)
{
  return pthread_getspecific((pthread_key_t)tsdslot);
}

