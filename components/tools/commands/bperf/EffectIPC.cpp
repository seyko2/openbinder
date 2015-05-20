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

#include "EffectIPC.h"

volatile int32_t g_externInt = 0; // see main.cpp

void ALocalFunction(void)
{
}

void ALocalFunctionWithParam(void* /*param*/)
{
}

void ALocalClass::ThisCouldDoSomething()
{
}

sptr<IBinder>
RemoteObjectProcess::InstantiateComponent(const sptr<INode>& object, const SValue&, const SString&, const SValue&, status_t*)
{
	return object->AsBinder();
}

int32_t				
RemoteObjectProcess::AtomMarkLeakReport()
{
	return 0;
}

void
RemoteObjectProcess::AtomLeakReport(int32_t, int32_t, uint32_t)
{
}

void
RemoteObjectProcess::PrintBinderReferences()
{
}

void
RemoteObjectProcess::RestartMallocProfiling()
{
}

void
RemoteObjectProcess::SetMallocProfiling(bool, int32_t, int32_t, int32_t)
{
}

void
RemoteObjectProcess::RestartVectorProfiling()
{
}

void
RemoteObjectProcess::SetVectorProfiling(bool, int32_t, int32_t, int32_t)
{
}

void
RemoteObjectProcess::RestartMessageIPCProfiling()
{
}

void
RemoteObjectProcess::SetMessageIPCProfiling(bool, int32_t, int32_t, int32_t)
{
}

void
RemoteObjectProcess::SetBinderIPCProfiling(bool, int32_t, int32_t, int32_t)
{
}

void
RemoteObjectProcess::PrintBinderIPCProfiling()
{
}

void
RemoteObjectProcess::RestartBinderIPCProfiling()
{
}

#if TEST_PALMOS_APIS

SString RemoteObjectView::ViewName() const
{
	SString s;
	return s;
}

void RemoteObjectView::SetViewName(const SString &value)
{
}
	
void
RemoteObjectView::Display(const sptr<IRender>& into, const SRect& dirty, uint32_t)
{
}

void
RemoteObjectView::Draw(const sptr<IRender>& into, const SRect& dirty)
{
}

void
RemoteObjectView::Invalidate(SUpdate* update, uint32_t)
{
}

SRect
RemoteObjectView::Frame() const
{
	SRect r;
	return r;
}

sptr<IViewParent>
RemoteObjectView::Parent() const
{
	return 0;
}

bool
RemoteObjectView::SetExternalConstraints(const BLayoutConstraints &c, uint32_t execFlags)
{
	return 0;
}

bool
RemoteObjectView::SetFrame(const SRect& bounds)
{
	return 0;
}

void
RemoteObjectView::MarkTraversalPath(int32_t flags, uint32_t execFlags)
{
}

status_t
RemoteObjectView::InvalidateConstraints(uint32_t execFlags)
{
	return 0;
}

void
RemoteObjectView::RequestTransaction(uint32_t execFlags)
{
}

SRect
RemoteObjectView::Bounds() const
{
	SRect r;
	return r;
}

SRegion
RemoteObjectView::Shape() const
{
	SRegion p;
	return p;
}

bool
RemoteObjectView::SetTransactionSize(float width, float height)
{
	return 0;
}

SRect
RemoteObjectView::TransactionFrame() const
{
	SRect r;
	return r;
}

SRect
RemoteObjectView::TransactionBounds() const
{
	SRect r;
	return r;
}

SRegion
RemoteObjectView::TransactionShape() const
{
	SRegion p;
	return p;
}

void
RemoteObjectView::SetParent(const sptr<IViewParent>& parent)
{
}

BLayoutConstraints
RemoteObjectView::Constraints() const
{
	BLayoutConstraints cn;
	return cn;
}

void
RemoteObjectView::SetViewFlags(uint32_t flags, uint32_t mask, uint32_t execFlags)
{
}

uint32_t
RemoteObjectView::ViewFlags() const
{
	return 0;
}

void
RemoteObjectView::Hide(uint32_t execFlags)
{
}

void
RemoteObjectView::Show(uint32_t execFlags)
{
}

bool
RemoteObjectView::IsHidden() const
{
	return 0;
}

bool
RemoteObjectView::IsTranslucent() const
{
	return 0;
}

SRegion
RemoteObjectView::DrawingShape() const
{
	SRegion p;
	return p;
}

SRect
RemoteObjectView::DrawingBounds() const
{
	SRect r;
	return r;
}

SRect
RemoteObjectView::VirtualBounds() const
{
	SRect r;
	return r;
}

SRect
RemoteObjectView::DrawingFrame() const
{
	SRect r;
	return r;
}

status_t
RemoteObjectView::RequestFocus(uint32_t flags)
{
	return B_OK;
}

bool
RemoteObjectView::TakeFocus(uint32_t operation, const SValue& hints)
{
	return false;
}

bool
RemoteObjectView::HasFocus() const 
{
    return false;
}

status_t
RemoteObjectView::DispatchFocusChange(bool hasFocus, uint32_t flags)
{
	return B_OK;
}

int32_t
RemoteObjectView::DispatchKeyEvent(const SMessage &msg)
{
	return 0;
}

int32_t
RemoteObjectView::DispatchEvent(const SMessage &msg)
{
	return 0;
}

int32_t
RemoteObjectView::DispatchPointerEvent(const SMessage &msg, const SPoint& where, const sptr<IMotion>& history)
{
	return 0;
}

uint32_t
RemoteObjectView::PreTraversal(const SGraphicPlane&)
{
	return 0;
}

void
RemoteObjectView::PostTraversal(BUpdate* outDirty)
{
}

void
RemoteObjectView::CommitTransaction()
{
}

uint32_t
RemoteObjectView::TransactionFlags() const
{
	return 0;
}

uint32_t
RemoteObjectView::DrawingFlags() const
{
	return 0;
}

status_t
RemoteObjectView::MakeSelected(uint32_t,const SValue&, uint32_t)
{
	return 0;
}

void RemoteObjectView::ScrollTo(double x, double y, uint32_t execFlags)
{
}

void RemoteObjectView::ScrollBy(double dx, double dy, uint32_t execFlags)
{
}

void RemoteObjectView::SetXScrollPosition(double x)
{
}

void RemoteObjectView::SetYScrollPosition(double y)
{
}

double RemoteObjectView::XScrollPosition() const
{
	return 0;
}

double RemoteObjectView::YScrollPosition() const
{
	return 0;
}

void RemoteObjectView::SetXScroller(const sptr<IScroller>& )
{
}

void RemoteObjectView::SetYScroller(const sptr<IScroller>& )
{
}

sptr<IScroller> RemoteObjectView::XScroller() const
{
	return 0;
}

sptr<IScroller> RemoteObjectView::YScroller() const
{
	return 0;
}

#endif
