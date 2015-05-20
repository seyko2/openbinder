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

#include <support/INode.h>
#include <support/IProcess.h>
#include <support/Package.h>

#if TEST_PALMOS_APIS
#include <view/IView.h>
#include <view/IScroller.h>
#include <view/IViewParent.h>
#include <view/ViewUtils.h>
#endif

extern void ALocalFunction(void);
extern void ALocalFunctionWithParam(void* param);

class ALocalClass
{
public:
	virtual void ThisCouldDoSomething();
};

class RemoteObjectProcess : public BnProcess, public SPackageSptr
{
	virtual	sptr<IBinder>			InstantiateComponent(const sptr<INode>& ctx, const SValue& componentInfo,
									const SString& component, const SValue& args, status_t* outError = NULL);
	
	virtual	int32_t					AtomMarkLeakReport();
	virtual	void					AtomLeakReport(int32_t mark, int32_t last, uint32_t flags);
	virtual	void					PrintBinderReferences();
	virtual	void					RestartMallocProfiling();
	virtual	void					SetMallocProfiling(bool enabled, int32_t dumpPeriod, int32_t maxItems, int32_t stackDepth);
	virtual	void					RestartVectorProfiling();
	virtual	void					SetVectorProfiling(bool enabled, int32_t dumpPeriod, int32_t maxItems, int32_t stackDepth);
	virtual	void					RestartMessageIPCProfiling();
	virtual	void					SetMessageIPCProfiling(bool enabled, int32_t dumpPeriod, int32_t maxItems, int32_t stackDepth);
	virtual	void					SetBinderIPCProfiling(bool enabled, int32_t dumpPeriod, int32_t maxItems, int32_t stackDepth);
	virtual void					RestartBinderIPCProfiling();
	virtual	void					PrintBinderIPCProfiling();

};

#if TEST_PALMOS_APIS

class ADerivedClass : public BUISpecialEffects
{
public:
	virtual void Dummy() { };
};

class RemoteObjectView : public BnView, public SPackageSptr
{
public:
	virtual SString					ViewName() const;
	virtual void					SetViewName(const SString &);
	virtual	void					Display(const sptr<IRender>& into, const SRect& dirty, uint32_t);
	virtual	void					Draw(const sptr<IRender>& into, const SRect& dirty);
	virtual void					Invalidate(SUpdate* update, uint32_t = 0);
	virtual SRect					Frame() const;
	virtual	sptr<IViewParent>		Parent() const;
	virtual	bool					SetExternalConstraints(const BLayoutConstraints &c, uint32_t execFlags);
	virtual bool					SetFrame(const SRect& bounds);
	virtual	void					MarkTraversalPath(int32_t flags, uint32_t execFlags = 0);
	virtual	status_t				InvalidateConstraints(uint32_t execFlags = 0);
	virtual	void					RequestTransaction(uint32_t execFlags = 0);
	virtual SRect					Bounds() const;
	virtual	SRegion					Shape() const;
	virtual bool					SetTransactionSize(float width, float height);
	virtual	SRect					TransactionFrame() const;
	virtual	SRect					TransactionBounds() const;
	virtual	SRegion					TransactionShape() const;
	virtual	void					SetParent(const sptr<IViewParent>& parent);
	virtual	BLayoutConstraints		Constraints() const;
	virtual void					SetViewFlags(uint32_t flags, uint32_t mask, uint32_t execFlags = 0);
	virtual uint32_t				ViewFlags() const;
	virtual void					Hide(uint32_t execFlags = 0);
	virtual void					Show(uint32_t execFlags = 0);
	virtual bool					IsHidden() const;
	virtual bool					IsTranslucent() const;
	virtual	SRegion					DrawingShape() const;
	virtual SRect					DrawingBounds() const;
	virtual SRect					VirtualBounds() const;
	virtual SRect					DrawingFrame() const;
	virtual	status_t				RequestFocus(uint32_t flags=0);
	virtual	bool					TakeFocus(uint32_t operation, const SValue& hints);
    virtual bool                    HasFocus() const;
	virtual	status_t				DispatchFocusChange(bool hasFocus, uint32_t flags);
	virtual	int32_t					DispatchKeyEvent(	const SMessage &msg);
	virtual	int32_t					DispatchPointerEvent(	const SMessage &msg,
															const SPoint& where,
															const sptr<IMotion>& history);
	virtual	int32_t					DispatchEvent(	const SMessage &msg);
	virtual	uint32_t				PreTraversal(const SGraphicPlane& plane);
	virtual	void					PostTraversal(BUpdate* outDirty);
	virtual	void					CommitTransaction();
	virtual uint32_t				TransactionFlags() const;
	virtual uint32_t				DrawingFlags() const;
	virtual	status_t				MakeSelected(uint32_t selected, const SValue& hints = B_UNDEFINED_VALUE, uint32_t execFlags = 0);
	virtual void					ScrollTo(double x, double y, uint32_t execFlags);
	virtual void					ScrollBy(double dx, double dy, uint32_t execFlags);
	virtual void					SetXScrollPosition(double x);
	virtual void					SetYScrollPosition(double y);
	virtual double					XScrollPosition() const;
	virtual double					YScrollPosition() const;

	virtual void					SetXScroller(const sptr<IScroller>& scroller);
	virtual void					SetYScroller(const sptr<IScroller>& scroller);
	virtual sptr<IScroller>		XScroller() const;
	virtual sptr<IScroller>		YScroller() const;
};

#endif
