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

#ifndef _SUPPORT_CALLSTACK_H
#define _SUPPORT_CALLSTACK_H

/*!	@file support/CallStack.h
	@ingroup CoreSupportUtilities
	@brief Debugging tools for retrieving stack crawls.
*/

#include <support/ITextStream.h>
#include <support/SupportDefs.h>
#include <support/Vector.h>
#include <support/KeyedVector.h>
#include <support/Locker.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SCallTree;

typedef int32_t (*b_demangle_func)(	const char *mangled_name,
									char *unmangled_name,
									size_t buffersize);

enum {
	B_CALLSTACK_DEPTH		= 32
};

//!	Create a stack crawl from the current PC.
class SCallStack {
public:
							SCallStack();
							SCallStack(const SCallStack& o);
							SCallStack(const SValue &value);
	virtual					~SCallStack();
							
			SValue			AsValue() const;
							
			SCallStack		&operator=(const SCallStack& o);
			
			void			Update(int32_t ignoreDepth=0, int32_t maxDepth=B_CALLSTACK_DEPTH);
			
			intptr_t		AddressAt(int32_t level) const;
			void			SPrint(char *buffer) const;
			void			Print(const sptr<ITextOutput>& io) const;
			void			LongPrint(const sptr<ITextOutput>& io, b_demangle_func demangler=NULL) const;
		
			bool			operator==(const SCallStack& o) const;
	inline	bool			operator!=(const SCallStack& o) const	{ return !(*this == o); }
	
			int32_t			Compare(const SCallStack& o) const;
	inline	bool			operator<(const SCallStack& o) const	{ return Compare(o) < 0; }
	inline	bool			operator<=(const SCallStack& o) const	{ return Compare(o) <= 0; }
	inline	bool			operator>=(const SCallStack& o) const	{ return Compare(o) >= 0; }
	inline	bool			operator>(const SCallStack& o) const	{ return Compare(o) > 0; }
	
private:
			intptr_t		GetCallerAddress(int32_t level) const;

			intptr_t		m_caller[B_CALLSTACK_DEPTH];
			int32_t			_reserved[2];
};

inline const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SCallStack& stack)
{
	stack.Print(io);
	return io;
}

class SCallTreeNode {
public:
							SCallTreeNode();
	virtual					~SCallTreeNode();
	
			void			PruneNode();
			void			ShortReport(const sptr<ITextOutput>& io);
			void			LongReport(const sptr<ITextOutput>& io, b_demangle_func demangler=NULL,
									   char *buffer=NULL, int32_t bufferSize=0);
	
private:
							SCallTreeNode(const SCallTreeNode& o);
			SCallTreeNode&	operator=(const SCallTreeNode& o);
	
	friend	class					SCallTree;
			
			intptr_t				addr;
			int32_t					count;
			SCallTreeNode *			higher;
			SCallTreeNode *			lower;
			SCallTreeNode *			parent;
			SVector<SCallTreeNode*>	branches;
};

class SCallTree : public SCallTreeNode {
public:
							SCallTree(const char *name);
	virtual					~SCallTree();
	
			void			Prune();
			void			AddToTree(SCallStack *stack, const sptr<ITextOutput>& io);
			void			Report(const sptr<ITextOutput>& io, int32_t count, bool longReport=false);

private:
							SCallTree(const SCallTree& o);
			SCallTree&		operator=(const SCallTree& o);
			
			SCallTreeNode*	highest;
			SCallTreeNode*	lowest;
};

// DON'T USE THIS CLASS IN LOCK DEBUGGING!
class SStackCounter
{
public:
							SStackCounter();
	virtual					~SStackCounter();

			void			Update(int32_t ignoreDepth=0, int32_t maxDepth=B_CALLSTACK_DEPTH);

			void			Reset();
			void			Print(const sptr<ITextOutput>& io, size_t maxItems=-1) const;
			void			LongPrint(const sptr<ITextOutput>& io, size_t maxItems=-1, b_demangle_func demangler=NULL) const;
			
			size_t			TotalCount() const;
			
private:
	struct stack_info {
		size_t count;
	};
	mutable SLocker							m_lock;
	SKeyedVector<SCallStack, stack_info>	m_data;
	size_t									m_totalCount;
};


/*!	@} */

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif /* _SUPPORT_CALLSTACK_H */
