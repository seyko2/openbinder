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

#ifndef PSI_PALMOS_LIBPALMROOT_H_
#define PSI_PALMOS_LIBPALMROOT_H_

// Include elementary types
#include <PalmTypes.h>					// Basic types

#include <bsd-string.h> // strlcat, strlcpy

#ifdef __cplusplus
// Pure C++ routines

namespace palmos {

extern void clock_timespec_add_nano(struct timespec * t, nsecs_t adj);
extern void clock_timespec_from_nano(struct timespec * t, nsecs_t desired);

}

#endif


#endif // PSI_PALMOS_LIBPALMROOT_H_

