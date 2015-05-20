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

#ifndef _B_SUPPORT_INSTANTIATECOMPONENT
#define _B_SUPPORT_INSTANTIATECOMPONENT

/*!	@file support/InstantiateComponent.h
	@ingroup CoreSupportBinder
	@brief API to package executables that contain components.
*/

#include <support/IBinder.h>
#include <support/String.h>
#include <support/Context.h>

/*!	@addtogroup CoreSupportBinder
	@{
*/

#if TARGET_HOST == TARGET_HOST_PALMOS
#include <CmnLaunchCodes.h>
#else
#define sysPackageLaunchGetInstantiate			72
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

typedef sptr<IBinder> (*instantiate_component_func)(const SString& component,
													const SContext& context,
													const SValue &args);

struct SysPackageLaunchGetInstantiateType
{
	size_t						size;
	instantiate_component_func	out_instantiate;
};

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

//!	This is a prototype for the factory function you should implement to create components.
extern "C"
BNS(palmos::support::) sptr<BNS(palmos::support::)IBinder>
InstantiateComponent(	const BNS(palmos::support::) SString& component,
						const BNS(palmos::support::) SContext& context,
						const BNS(palmos::support::) SValue& args);


/*!	@} */

#endif // _B_SUPPORT_INSTANTIATECOMPONENT
