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

#include "bomb_host.h"
#include <support/InstantiateComponent.h>
#include <support/Package.h>
#include <support/Binder.h>
#include <SysThread.h>
#include <stdio.h>

volatile int32_t g_objectCount;

// ================================================
class Object : public BBinder
{
public:
			Object(uint32_t p);
	virtual ~Object();

	uint32_t pid;

private:
	SPackageSptr m_package;
};

Object::Object(uint32_t p)
	:BBinder(),
	 pid(p),
	 m_package()
{
	SysAtomicInc32(&g_objectCount);
	printf("Object created: %d\n", pid);
}

Object::~Object()
{
	SysAtomicDec32(&g_objectCount);
	printf("Object destroyed: %d\n", pid);
}


// ================================================
class BombHost : public BBinder
{
public:
	BombHost();

	virtual	status_t Called(const SValue& func, const SValue& args, SValue* out);

private:
	SPackageSptr m_package;
};

BombHost::BombHost()
	:BBinder(),
	 m_package()
{
}

status_t
BombHost::Called(const SValue& func, const SValue& args, SValue* out)
{
	if (func == key_GiveObject) {
		*out = SValue::Binder(new Object(args[B_0_INT32].AsInt32()));
	}
	else if (func == key_Dump) {
		printf("BombHost object count is %d\n", g_objectCount);
	}

	return B_OK;
}


// ================================================
sptr<IBinder>
InstantiateComponent(const SString& component, const SContext& context, const SValue& args)
{
	if (component == "Host") {
		return new BombHost();
	}

	return NULL;
}

