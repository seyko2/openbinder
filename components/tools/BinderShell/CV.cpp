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

#include "CV.h"

B_STATIC_STRING_VALUE_8 (key_name,		"name",)
B_STATIC_STRING_VALUE_8 (key_Wait,		"Wait",)
B_STATIC_STRING_VALUE_8 (key_Open,		"Open",)
B_STATIC_STRING_VALUE_8 (key_Close,		"Close",)
B_STATIC_STRING_VALUE_12(key_Broadcast,	"Broadcast",)

ConditionVariable::ConditionVariable(const SValue &args)
	:BBinder(),
	 SPackageSptr(),
	 m_cv(args[key_name].AsString().String()),
	 m_name(args[key_name].AsString())
{
}

ConditionVariable::~ConditionVariable()
{
}

status_t
ConditionVariable::Called(const SValue &func, const SValue &args, SValue *out)
{
	if (func == key_Wait) {
		bout << "Waiting " << m_name << endl;
		m_cv.Wait();
	}
	else if (func == key_Open) {
		bout << "Opening " << m_name << endl;
		m_cv.Open();
	}
	else if (func == key_Close) {
		bout << "Closing " << m_name << endl;
		m_cv.Close();
	}
	else if (func == key_Broadcast) {
		bout << "Broadcasting " << m_name << endl;
		m_cv.Broadcast();
	}
	else {
		return B_BINDER_UNKNOWN_METHOD;
	}
	return B_OK;
}


