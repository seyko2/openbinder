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

#ifndef J_IDLSTRUCT_H
#define J_IDLSTRUCT_H
// stores carrier struct for bison + lookup utilities for types/symbols/interface names

#include "idlc.h"
#include "symbolstack.h"

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

// in anticipation of other states for the parser/scanner
struct pState {
	bool	parseImport;
};

enum tbaction {
	ADD, LOOK, EDIT
};

class IDLStruct
{
	public:
		IDLStruct(); // empty constructor
		IDLStruct(const SVector<InterfaceRec>& interfaces,
					const SVector<IncludeRec>& includes,
					const SKeyedVector<SString, IDLType*>& typebank,
					const SKeyedVector<SString, StackNode>& symtable);
		IDLStruct(IDLStruct& aStruct); // copy constructor
		~IDLStruct(); // destructor

		//InterfaceRec*					FindInterface(const SString &name, SKeyedVector<SString, InterfaceRec*> ifbank);
		
		// returns NULL ptr if not present
		StackNode*							FindScope(SString scopename);
		sptr<IDLType>						FindIDLType(const sptr<IDLType>& typeptr, tbaction insert);		
		// findsymbols has no meaning for idlstruct now -> symbol insertion only valid on the stack, and no need for lookup util yet
		// IDLSymbol*						FindSymbol(IDLSymbol sym);
		status_t							SetInterfaces(SVector<InterfaceRec> interfaces);
		status_t							AddImportDir(SString importdir);
		status_t							SetHeader(SString headerdir);
		status_t							SetParserState(bool isimport);
		status_t							SetNamesOnStack(SString namespaces);
		status_t 							AddIncludes(SVector<IncludeRec> includes);
		status_t							AddScopeTable(StackNode scopetable);
		status_t							SetBeginComments(const sptr<IDLCommentBlock>& comments);
		status_t							SetEndComments(const sptr<IDLCommentBlock>& comments);

		SString								Header();
		const SVector<SString>&				ImportDir();
		const SVector<IncludeRec>& 			Includes();
		const SVector<InterfaceRec>&		Interfaces();
		pState								ParserState();
		SString								NamesOnStack();
		const SKeyedVector<SString, sptr<IDLType> >& TypeBank();
		const SKeyedVector<SString, StackNode>& SymbolTable();
		const sptr<IDLCommentBlock>&		BeginComments();
		const sptr<IDLCommentBlock>&		EndComments();

	private:
		status_t								AddUserType(const sptr<IDLType>& utype);

		SString									p_header;
		SVector<SString>						p_importdir;
		SVector<IncludeRec> 					p_includes;
		SVector<InterfaceRec>					p_interfaces;
		pState									p_state;
		SKeyedVector<SString, sptr<IDLType> >	p_typebank;
		SKeyedVector<SString, StackNode>		p_symboltable;
		SString									p_namespaces;
		sptr<IDLCommentBlock>					p_begincomments;
		sptr<IDLCommentBlock>					p_endcomments;

};

#endif //J_IDLSTRUCT_H
