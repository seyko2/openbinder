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

#ifndef _PACKAGE_KIT_H
#define _PACKAGE_KIT_H

#include <support/Value.h>
#include <support/IByteStream.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace package {
using namespace palmos::storage;
using namespace palmos::support;
#endif

// Returns an SValue given a component description (from the manifest)
// using the standard names:
//       Key      Type
//       file     string        Path to the script file
sptr<IByteInput> get_script_data_from_value(const SValue &info, status_t *err = NULL);


#if _SUPPORTS_NAMESPACE
} }
#endif

#endif // _PACKAGE_KIT_H
