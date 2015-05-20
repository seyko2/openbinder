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

#include "idlc.h"
#include "idlstruct.h"

#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::storage;
#endif

IDLStruct::IDLStruct()
{
	p_state.parseImport=false;
}

IDLStruct::IDLStruct(const SVector<InterfaceRec>& interfaces,
			const SVector<IncludeRec>& includes, 
			const SKeyedVector<SString, IDLType*>& typebank,
			const SKeyedVector<SString, StackNode>& symtable)
		: p_interfaces(interfaces), p_includes(includes), 
		p_namespaces(SString())
{
	p_state.parseImport=false;

	for (uint32_t index=0; index<typebank.CountItems(); index++) {	
		p_typebank.AddItem(typebank.KeyAt(index), typebank.ValueAt(index));	
	}
	p_symboltable.SetTo(symtable);
}

IDLStruct::~IDLStruct(void)
{	
	p_includes.MakeEmpty();
	p_interfaces.MakeEmpty();
	p_typebank.MakeEmpty();	
	p_symboltable.MakeEmpty();
}

status_t								
IDLStruct::SetHeader(SString headerdir)
{		
	p_header=headerdir;
	return B_OK;
}

status_t 
IDLStruct::AddIncludes(SVector<IncludeRec> includes)
{	
	p_includes=includes;
	return B_OK;
}

status_t 
IDLStruct::SetInterfaces(SVector<InterfaceRec> interfaces)
{
	p_interfaces=interfaces;
	#ifdef STRUCTDEBUG
		bout << "<----- idlstruct.cpp -----> there are now "
			<< p_interfaces.CountItems() << " interfaces in IDLStruct\n";
	
		for (size_t index=0 ; index<interfaces.CountItems() ; index++ ) {	
			bout << "<----- idlstruct.cpp ----->		"
				<<p_interfaces.ItemAt(index).ID() << " added\n"; 
		}
	#endif		
	return B_OK;
}

status_t								
IDLStruct::AddImportDir(SString importdir)
{		
	p_importdir.AddItem(importdir);
	return B_OK;
}

status_t								
IDLStruct::SetParserState(bool isimport)
{		
	p_state.parseImport=isimport;
	return B_OK;
}

status_t
IDLStruct::SetNamesOnStack(SString namespaces)
{	
	p_namespaces=namespaces;
	return B_OK;
}

status_t								
IDLStruct::AddScopeTable(StackNode scopetable)
{
	p_symboltable.AddItem(scopetable.getScopeName(), scopetable);
	#ifdef STRUCTDEBUG
		bout << "<----- idlstruct.cpp -----> there are now "
			<< p_symboltable.CountItems() << " nodes in the Symbol Table\n";
	
		bout << "<----- idlstruct.cpp ----->		"
				<< scopetable.getScopeName() << " added\n"; 
	#endif		
	return B_OK;
}	

status_t
IDLStruct::SetBeginComments(const sptr<IDLCommentBlock>& comments)
{
	if (!p_state.parseImport && p_begincomments == NULL)
		p_begincomments = comments;
	return B_OK;
}

status_t
IDLStruct::SetEndComments(const sptr<IDLCommentBlock>& comments)
{
	p_endcomments = comments;
	return B_OK;
}

status_t 
IDLStruct::AddUserType(const sptr<IDLType>& utype)
{		
	p_typebank.AddItem(utype->GetName(), utype);

	#ifdef STRUCTDEBUG
		bout << "<----- idlstruct.cpp -----> type "
			<< utype->GetName() << " just added" << endl;
	#endif

	return B_OK;
}

const SVector<SString>&
IDLStruct::ImportDir()
{
	return p_importdir;
}

SString								
IDLStruct::Header()
{
	return p_header;
}

const SVector<IncludeRec>&
IDLStruct::Includes()
{
	return p_includes;
}

const SVector<InterfaceRec>&
IDLStruct::Interfaces()
{
	return p_interfaces;
}

pState								
IDLStruct::ParserState()
{
	return p_state;
}


SString
IDLStruct::NamesOnStack()
{
	return p_namespaces;
}


const SKeyedVector<SString, sptr<IDLType> >&
IDLStruct::TypeBank()
{
	return p_typebank;
}

const SKeyedVector<SString, StackNode>&
IDLStruct::SymbolTable()
{
	return p_symboltable;
}

const sptr<IDLCommentBlock>&
IDLStruct::BeginComments()
{
	return p_begincomments;
}

const sptr<IDLCommentBlock>&
IDLStruct::EndComments()
{
	return p_endcomments;
}

/*
IDLSymbol*
IDLStruct::FindSymbol(IDLSymbol sym)
{	
	p_symboltable.CountItems();
	if (p_symboltable.CountItems()<=0){	
		berr << "<----- idlstruct.cpp -----> " << sym.GetId() << " cannot be found : Symbol Table is empty" << endl;
		_exit(1);	
	}
	else { 	
		uint32_t index=0;
		IDLSymbol* result=NULL;

		while ((index<p_symboltable.CountItems()) && (result==NULL)) {	
			StackNode temp=p_symboltable.ValueAt(index);
			IDLSymbol*	result=temp.lookup(sym, false);
			index++;
		}

		if (result==NULL) {	
			berr << "<----- idlstruct.cpp -----> " << sym.GetId() <<" cannot be found" << endl;
			_exit(1);	
		}			
		else { return result;}
	}	
} */


StackNode*	
IDLStruct::FindScope(SString scopename)
{		bool present=false;
		p_symboltable.ValueFor(scopename, &present); 

		if (present) {	
			return (&p_symboltable.EditValueFor(scopename, &present));
		}
		else {
			bout << "<----- idlstruct.cpp ----->  " << scopename << " is not present in the Symbol Table" << endl;
			return NULL;
		}
}

sptr<IDLType> 
IDLStruct::FindIDLType(const sptr<IDLType>& typeptr, tbaction insert)
{
	// callers must be prepared for the potential of a NULL return on LOOK
	bool present=false;
	SString code=typeptr->GetName();
	p_typebank.ValueFor(code, &present);

	if ((insert==ADD) || (insert==EDIT)) {	
		if (present) {
			if (insert==EDIT) {	
				ssize_t r=p_typebank.RemoveItemFor(code);
				if (r>=B_OK) {	
					AddUserType(typeptr);
					return p_typebank.EditValueFor(code, NULL);		
				}
				else {	
					bout << "<----- idlstruct.cpp -----> " << code << " edit failed in TypeBank" << endl;
					_exit(1);
				}
			}
			else {
				bout << "<----- idlstruct.cpp -----> " << code << " is already present in TypeBank" << endl;
				_exit(1);
			}
		}
		else {
			//bout << "<----- idlstruct.cpp -----> " << code << " about to be added to TypeBank" << endl;
			AddUserType(typeptr);
			return p_typebank.EditValueFor(code, NULL);
		}		
	}
	else if ((insert==LOOK)) {	
		if (present) { 
			return p_typebank.EditValueFor(code, &present); 
		}	
		else { 
			// bout << "<----- idlstruct.cpp ----->  " << code << " could not be found in TypeBank" << endl;
			return NULL;
		}
	}
	return NULL;
}
