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

#ifndef J_SYMBOLSTACK_H
#define J_SYMBOLSTACK_H
#include "idlc.h"
#include "InterfaceRec.h"

#if _SUPPORTS_NAMESPACE
using namespace palmos::support; 
#endif

/******* symbols ***************************/
class IDLSymbol
{
	public:
		IDLSymbol();
		IDLSymbol(const SString name);
		IDLSymbol(const SString name, const sptr<IDLType>& aType);
		IDLSymbol(const IDLSymbol& aSymbol); // copy constructor
		~IDLSymbol(); // destructor

		IDLSymbol&			operator = (const IDLSymbol &);
		
		status_t			SetType(const sptr<IDLType>& aType);
		sptr<IDLType> 		GetType();
		SString				GetId();
					
	private:
		SString				m_id;
		sptr<IDLType>		m_type;
};


/******* stack nodes **********************/
class StackNode
{
	public:
		
		StackNode(); // empty node
		StackNode(const SString newname, const SString parent); //
		StackNode(const StackNode& node); // copy constructor
		StackNode & operator = (const StackNode& node); // assignment 
		~StackNode(); // destructor

		SString 					getScopeName();
		status_t					setScopeName(const SString newname);
		StackNode*					getParentInfo();
		status_t					setParentInfo(StackNode* parentptr);
		// some way of accessing the children in the future
		StackNode*					getChildInfoAt(int32_t index);
		status_t					setChildInfo(StackNode* childptr);	 
		int32_t						countChildren();

		// look up name in the current node, and if it's valid insert it
		IDLSymbol*					lookup(IDLSymbol sym, bool insert);
	
	private:
		SString 							scopename;
		StackNode* 							parentlink;		
		SVector<StackNode*>					childlink;
		SKeyedVector<SString, IDLSymbol>	scopetable;
};


/******* symbol stack **********************/
class SymbolStack
{	
	public:
		SymbolStack(); // empty stack
		~SymbolStack(); // destroys stack

		void push(const SString scope); // new Table for separate scope
		StackNode & pop();
		bool empty() const;

		//IDLSymbol* lookup(IDLSymbol* symbol, bool doinsert);
		const StackNode* scopelookup(const SString scopename);

		//bool lookupInterface(const SString symbolname);
		//void printstack();
		StackNode* getCurrentScope() const;
		
	private:
		SymbolStack(const SymbolStack& stack); // copy constructor
		//void insertsym(IDLSymbol* symbol);
		StackNode* CurrentScope;
};

static SVector<StackNode> SymTab;
#endif //J_SYMBOLSTACK_H
