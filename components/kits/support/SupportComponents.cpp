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

#include <support/Package.h>
#include <support/CatalogMirror.h>
#include <support/InstantiateComponent.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

// helpful macro
#define COMPONENT_CLASS_WITH_ARGS(Name, Base) \
	class Name : public Base \
	{ \
	public: \
		Name(const SContext& context, const SValue &attr); \
	private: \
		SPackageSptr blah; \
	}; \
	Name::Name(const SContext& context, const SValue &attr) \
		:Base(context, attr) \
	{ \
	}

#define COMPONENT_CLASS(Name, Base) \
	class Name : public Base \
	{ \
	public: \
		Name(const SContext& context); \
	private: \
		SPackageSptr blah; \
	}; \
	Name::Name(const SContext& context) \
		:Base(context) \
	{ \
	}

COMPONENT_CLASS_WITH_ARGS(CatalogMirrorComponent, BCatalogMirror);

// =========================================================================
// Instantiation
// =========================================================================

sptr<IBinder> InstantiateComponent(	const SString& component,
									const SContext& context,
									const SValue &args)
{
	if (component == "CatalogMirror") {
		return static_cast<BnCatalogPermissions*>(new CatalogMirrorComponent(context, args));
	}

	return NULL;
}
