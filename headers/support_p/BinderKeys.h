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

#ifndef _SUPPORT_BINDERKEYS_H_
#define _SUPPORT_BINDERKEYS_H_

#include <support/Value.h>
#include <support/StaticValue.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {
#endif

#if _SUPPORTS_NAMESPACE
using namespace support;
#endif

extern const SValue g_keyBranch;

// Standard binder keys.
extern value_csml g_keyRead;
extern value_csml g_keyWrite;
extern value_csml g_keyEnd;
extern value_csml g_keySync;
extern value_clrg g_keyPosition;
extern value_csml g_keySeek;
extern value_csml g_keyMode;
extern value_csml g_keyNode;
extern value_csml g_keyBinding;
extern value_csml g_keyPost;
extern value_clrg g_keyRedirectMessagesTo;
extern value_csml g_keyPrint;
extern value_clrg g_keyBumpIndentLevel;
extern value_csml g_keyStatus;
extern value_csml g_keyValue;
extern value_csml g_keyWhich;
extern value_csml g_keyFlags;

// IBinderVector<> keys
extern const SValue g_keyAddChild;
extern const SValue g_keyAddChildAt;
extern const SValue g_keyOverlayAttributes;
extern const SValue g_keyRemoveChild;
extern const SValue g_keyChildAt;
extern const SValue g_keyChildAt_child;
extern const SValue g_keyChildAt_attr;
extern const SValue g_keyChildFor;
extern const SValue g_keyIndexOf;
extern const SValue g_keyNameOf;
extern const SValue g_keyCount;
extern const SValue g_keyReorder;

//**************************************************************************************
// InterfaceKit binder keys
//**************************************************************************************

// IViewParent keys
extern const SValue g_keyInvalidateChild;
extern const SValue g_keyMarkTraversalPath;
extern const SValue g_keyConstraintChild;

// ISurfaceManager keys
extern const SValue g_keyRootSurface;

//**************************************************************************************

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::osp
#endif

#endif	/* _SUPPORT_BINDERKEYS_H_ */
