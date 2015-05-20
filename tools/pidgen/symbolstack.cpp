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

#include "symbolstack.h"

/******* stack nodes **********************/
StackNode::StackNode()
{
	scopename=SString();
	parentlink=NULL;
}

StackNode::StackNode(const SString newname, const SString parent)
	: scopename(newname)
{	
	parentlink=NULL;
}	

StackNode::StackNode(const StackNode& node) // copy constructor
	: scopename(node.scopename), parentlink(node.parentlink),
		childlink(node.childlink)
{
	scopetable.SetTo(node.scopetable);
}

StackNode::~StackNode()
{	//delete parentlink;	
	//childlink.MakeEmpty();
	//scopetable.MakeEmpty();
}   // destructor

StackNode &
StackNode::operator = (const StackNode &node)
{
	#ifdef SYMDEBUG
		bout << "<----- symbolstack.cpp -----> stacknode assignment = called by "
			<< this->getScopeName() << endl;
		bout << "<----- symbolstack.cpp ----->		about to be set to -> "
			<< node.scopename << endl;
	#endif SYMDEBUG

	scopetable.SetTo(node.scopetable);
	scopename=node.scopename; 
	parentlink=node.parentlink;
	childlink=node.childlink;

	return *this;
} // assignment 

SString
StackNode::getScopeName()
{	return scopename;}

status_t
StackNode::setScopeName(const SString newname)
{	scopename=newname;
	#ifdef SYMDEBUG
		bout << "<----- symbolstack.cpp -----> attn! scopename just got reset to " << newname << endl;
		bout << "<----- symbolstack.cpp ----->		be careful! scopename should rarely be reset " << endl;
	#endif SYMDEBUG
	
	return B_OK;
}

StackNode*
StackNode::getParentInfo()
{	return parentlink;
}

status_t
StackNode::setParentInfo(StackNode* parentptr)
{
	parentlink=parentptr;
	return B_OK;
	//set parent info
}

StackNode*		
StackNode::getChildInfoAt(int32_t index)
{	
	return childlink.ItemAt(index);	 	
}

status_t
StackNode::setChildInfo(StackNode* childptr)
{
	childlink.AddItem(childptr);
	return B_OK;
	//set children info
}

int32_t
StackNode::countChildren()
{	return childlink.CountItems();
}

IDLSymbol*
StackNode::lookup(IDLSymbol sym, bool insert)
{	bool present=false;
	scopetable.ValueFor(sym.GetId(), &present);

	if (insert)
	{	if (present)
		{ berr << "----- symbolstack.cpp -----> failed to insert " << sym.GetId() << 
			"; symbol already present in" << scopename <<endl;
		}
		else {
			scopetable.AddItem(sym.GetId(), sym);	
			bout << "----- symbolstack.cpp -----> " << sym.GetId() << 
				" inserted into " << scopename <<" table" << endl;
		}
		return (&scopetable.EditValueFor(sym.GetId(), &present));	   
	}

	else
	{	if (present)
		{ berr << "----- symbolstack.cpp -----> " << sym.GetId() << 
			" symbol present in" << scopename <<endl;
			return (&scopetable.EditValueFor(sym.GetId(), &present));	
		}
		else {
			scopetable.AddItem(sym.GetId(), sym);	
			bout << "----- symbolstack.cpp -----> " << sym.GetId() << 
				" not present " << scopename <<" table" << endl;
			return NULL;
		}   
	}
}
/******* symbol stack **********************/

SymbolStack::SymbolStack()
{	CurrentScope=NULL;
} // empty stack 

SymbolStack::~SymbolStack()
{	StackNode next;
	while (! empty())
	{	next=pop(); // delete all StackNodes on stack
	}	
}

void 
SymbolStack::push(const SString scope)
{	
		StackNode*  temp;
		StackNode*  childptr;
		SKeyedVector<SString, IDLSymbol>  table;
	
		#ifdef SYMDEBUG
			if (CurrentScope==NULL)
			{ 	bout << "<----- symbolstack.cpp -----> 1st node is being pushed onto the stack - CurrentScope=NULL" << endl;
			}
			else {
				bout << "<----- symbolstack.cpp -----> node is being pushed onto the stack - CurrentScope= " <<
				CurrentScope->getScopeName() << endl;
			}
		#endif
	

		temp= new StackNode;
	
		temp->setScopeName(scope);
		temp->setParentInfo(CurrentScope);
		// give parent scope link to newborn -> parentlink=NULL if this is the first scope table

		childptr=temp;
		if (CurrentScope==NULL) {
			#ifdef SYMDEBUG
				bout << "<----- symbolstack.cpp -----> we don't have any child info to pass back since " 
					<< scope << " is our first scope table " << endl;
			#endif
		}
		else {	
			CurrentScope->setChildInfo(childptr);
			// give child link to parent
		}

		CurrentScope=temp;
		#ifdef SYMDEBUG
			bout << "<----- symbolstack.cpp -----> another node got pushed onto stack - CurrentScope= " <<
				CurrentScope->getScopeName() << endl;
		#endif
//	}
}

StackNode&
SymbolStack::pop()
{	if (empty())
	{	bout <<"<----- symbolstack.cpp -----> symbol stack is empty" << endl;
//		delete CurrentScope;
//		CurrentScope= NULL;
//
//		bout <<"<----- symbolstack.cpp ----->		CurrentScope deleted" << endl;
//		exit (1);
		return *(StackNode*)NULL;
	}
	else
	{	
		#ifdef SYMDEBUG
			bout << "<----- symbolstack.cpp -----> symbolstack is being popped - we're popping "
				<< CurrentScope->getScopeName() << endl;
			if ((CurrentScope->getParentInfo())!=NULL) {
				bout << "<----- symbolstack.cpp ----->		the next scope that is coming up is "			
				<< (CurrentScope->getParentInfo())->getScopeName() << endl;
			}
		#endif
		
		StackNode*	temp;		
		temp=CurrentScope;


		#ifdef SYMDEBUG
//			bout << "<----- symbolstack.cpp -----> about to move CurrentScope " << endl;
		#endif
			CurrentScope=CurrentScope->getParentInfo();
		#ifdef SYMDEBUG
//			bout << "<----- symbolstack.cpp -----> CurrentScope has been moved to" << CurrentScope->getScopeName() << endl;
		#endif
		
		return *temp;	
	}
}

bool SymbolStack::empty() const
{	return (CurrentScope==NULL);
}


StackNode*
SymbolStack::getCurrentScope() const
{	return (CurrentScope);
}

SymbolStack::SymbolStack(const SymbolStack& stack)
{	
/*	if (stack.CurrentScope==NULL)
	{	CurrentScope=NULL;
	}
	else
	{	StackNodePtr end, temp=stack.CurrentScope;
	
		end=new StackNode;

		 table_t=temp->scopetable;
		end->scopetable=table_t;
		
		#ifdef SYMDEBUG
			bout << "<----- symbolstack.cpp -----> the symbol table with CurrentScope= " 
				<< CurrentScope->getScopeName << " just got copied" << "\n";
		#endif
		
		end->setScopeName(temp->getScopeName());
		CurrentScope=end;
		
		temp=temp->stlink;
		while(temp!=NULL)
		{	end->stlink=new StackNode;
			end=end->stlink;

			 table_t1=temp->scopetable;
			end->scopetable=table_t1;

			end->setScopeName(temp->getScopeName());
			temp=temp->stlink;
		}
		end->stlink=NULL;	
	}	 */
}
