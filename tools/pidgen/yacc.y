%{
//#define YYDEBUG 1 

#include "idlc.h"
#include "idlstruct.h"

#define REF ((IDLStruct *) ref)

#define YYPARSE_PARAM ref
#define YYLEX_PARAM REF

#define YYERROR_VERBOSE 1

extern int yylex(IDLStruct* ref);
extern int yyerror(char *errmsg);
extern FILE *yyin;
extern sptr<IDLCommentBlock> get_comments();
extern void pushback_comments(sptr<IDLCommentBlock> comments);

SymbolStack				jtable;
SString					jheaders;
SVector<InterfaceRec>	jinterface;

SString 
full_scoped_name(IDLTypeScope* scoped_names)
{	
	int32_t params=scoped_names->CountParams();
	SString name;

	for (int32_t i=0; i<params; i++) {	
		sptr<IDLNameType> nameptr=scoped_names->ParamAt(i);
		if (i!=0) {	
			name.Append("::");
		}
		name.Append(nameptr->m_id);
	}	
	return name;
}

IDLMethod*
build_method(IDLNameType* name, IDLType* returnType, IDLTypeScope* paramList)
{
	// name = J_IDENTIFIER
	// returnType = op_type_spec
	// paramList = parameter_dcls

	// Build the IDLMethod, adding all the parameters (and comments)

	IDLMethod* method = new IDLMethod(name->m_id, returnType, name->m_comment);
	// the attributes (if any) associated with the return type 
	// also become associated with the method
	method->SetAttributes(returnType->GetAttributes());
		
	// iterate parameter list and add to the method		
	int32_t count = paramList->CountParams();
	for (int32_t i = 0; i < count; i++) {
		sptr<IDLNameType> param = paramList->ParamAt(i);
		method->AddParam(param->m_id, param->m_type, param->m_comment);
	}
	
	// if the user puts a trailing comment after the last parameter
	// we need to pick that up also...
	method->AddTrailingComment(get_comments());

	return method;
}

void
copy_elements(InterfaceRec* dest, InterfaceRec* source)
{
	// copy all the elements from source to destination
	
	SVector<SString> namespaces = source->CppNamespace();
	size_t namespaceNum = namespaces.CountItems();
	for (size_t i = 0; i < namespaceNum; i++) { 
		dest->AddCppNamespace(namespaces[i]);
	}

	int32_t constructNum = source->CountConstructs();
	for (int32_t i = 0; i < constructNum; i++) {
		dest->AddConstruct(source->ConstructAt(i));
	}

	int32_t propNum = source->CountProperties();
	for (int32_t i = 0; i < propNum ; i++) {
		sptr<IDLNameType> prop = source->PropertyAt(i);
		dest->AddProperty(prop->m_id, prop->m_type, prop->m_comment, prop->m_custom);
	}
	
	int32_t methNum=source->CountMethods();
	for (int32_t i = 0; i < methNum ; i++) {
		dest->AddMethod(source->MethodAt(i));
	}

	int eventNum = source->CountEvents();
	for (int32_t i = 0; i < eventNum ; i++) {
		dest->AddEvent(source->EventAt(i));
	}
	
	int typedefs = source->CountTypedefs();
	for (int32_t i = 0; i < typedefs; i++) {
		sptr<IDLNameType> def = source->TypedefAt(i);
		dest->AddTypedef(def->m_id, def->m_type, def->m_comment);
	}
}

%}

%union {
	void*					_anything;
	IDLNameType*			_symbol;  	 				 	
	IDLType*				_type;
	IDLTypeScope*			_scope;
	IDLMethod*				_method;
	InterfaceRec*		 	_interface;
	SVector<InterfaceRec>*	_vectorif;
};

%token <_anything> J_HEADERDIR
%token <_anything> J_CONST
%token <_anything> J_CUSTOMMARSHAL	// attribute (on type)
%token <_anything> J_OPTIONAL		// attribute
%token <_anything> J_IN				// attribute
%token <_anything> J_OUT			// attribute
%token <_anything> J_INOUT			// attribute 
%token <_anything> J_ONEWAY			// not used
%token <_anything> J_READONLY		// attribute
%token <_anything> J_LOCAL			// attribute (on interface, property or method)
%token <_anything> J_WEAK			// attribute
%token <_anything> J_RESERVED		// attribute
%token <_anything> J_ATTRIBUTE 
%token <_anything> J_EXCEPTION
%token <_anything> J_NATIVE
%token <_anything> J_PERIOD
%token <_anything> J_COLON		
%token <_anything> J_PLUS		
%token <_anything> J_MINUS		
%token <_anything> J_LT
%token <_anything> J_GT
%token <_anything> J_QUOTE
%token <_anything> J_VERTICALLINE
%token <_anything> J_CIRCUMFLEX
%token <_anything> J_AMPERSAND
%token <_anything> J_ASTERISK
%token <_anything> J_SOLIDUS
%token <_anything> J_PERCENT
%token <_anything> J_TILDE
%token <_anything> J_ABSTRACT
%token <_anything> J_TYPE

%token <_anything> J_USING
%token <_anything> J_NAMESPACE
%token <_anything> J_TYPEDEF
%token <_anything> J_INTERFACE
%token <_anything> J_SEMICOLON
%token <_anything> J_LPAREN
%token <_anything> J_RPAREN
%token <_anything> J_LCURLY
%token <_anything> J_RCURLY
%token <_anything> J_LBRACKET
%token <_anything> J_RBRACKET
%token <_anything> J_EQ
%token <_anything> J_COMMA
%token <_anything> J_DCOLON
%token <_anything> J_POUND
%token <_anything> J_INCLUDE
%token <_anything> J_CASE
%token <_anything> J_CUSTOM
%token <_anything> J_DEFAULT

%token <_anything> J_FACTORY
%token <_anything> J_FALSE
%token <_anything> J_FIXED
%token J_RAISES
%token J_SEQUENCE
%token J_PUBLIC
%token J_PRIVATE
%token J_SUPPORTS
%token J_TYPEDEF
%token J_MODULE
%token J_OBJECT
%token J_SWITCH
%token J_TRUE
%token J_TRUNCATABLE
%token J_UNSIGNED
%token J_VALUEBASE
%token J_VALUETYPE
%token J_PROPERTIES
%token J_METHODS
%token J_EVENTS

%token <_symbol> J_FLOATPTLITERAL
%token <_symbol> J_FIXEDPTLITERAL
%token <_symbol> J_INTLITERAL
%token <_symbol> J_CHARLITERAL
%token <_symbol> J_STRINGOPSLITERAL
%token <_symbol> J_STRINGLITERAL
%token <_symbol> J_IDENTIFIER
%token <_symbol> J_INCLUDENAME
%token <_symbol> J_ENUM

// unsupported types
%token <_type> J_ANY
%token <_type> J_WSTRING
%token <_type> J_OCTET
%token <_type> J_STRING

%token <_type> J_STRUCT
%token <_type>  J_UNION



// std types
%token <_type> J_VOID
%token <_type> J_BOOLEAN
%token <_type> J_CHAR
%token <_type> J_DOUBLE
%token <_type> J_FLOAT
%token <_type> J_LONG
%token <_type> J_SHORT
%token <_type> J_INT8
%token <_type> J_INT64
%token <_type> J_UINT8
%token <_type> J_UINT16
%token <_type> J_UINT32
%token <_type> J_UINT64
%token <_type> J_WCHAR32

// err m_code types
%token <_type> J_SIZT
%token <_type> J_OFFT
%token <_type> J_BIGT
%token <_type> J_NSST
%token <_type> J_STST
%token <_type> J_SSZT

// known types
%token <_type> J_SVALUE
%token <_type> J_SSTRING
%token <_type> J_SMESSAGE

%type <_type> bigtimet_type;
%type <_type> nsecst_type;
%type <_type> offt_type;
%type <_type> sizet_type;
%type <_type> ssizet_type;
%type <_type> statust_type;
%type <_type> svalue_type;
%type <_type> sstring_type;
%type <_type> smessage_type;
%type <_type> sequence_type;

%type <_type> integer_type;
%type <_type> unsigned_int;
%type <_type> unsigned_int8
%type <_type> unsigned_short_int;
%type <_type> unsigned_long_int;
%type <_type> unsigned_longlong_int;
%type <_type> signed_int;
%type <_type> signed_int8;
%type <_type> signed_short_int;
%type <_type> signed_long_int;
%type <_type> signed_longlong_int;

%type <_type> floating_pt_type;
%type <_type> param_type_spec;
%type <_type> char_type;
%type <_type> charptr_type;
%type <_type> wide_char_type;
%type <_type> wide_string_type;
%type <_type> boolean_type;
%type <_type> octet_type;
%type <_type> any_type;
%type <_type> value_base_type;
%type <_type> string_type;
%type <_type> const_type;
%type <_type> fixed_pt_const_type;
%type <_type> op_type_spec;
%type <_type> base_type_spec;
%type <_type> template_type_spec;
%type <_type> simple_type_spec;
%type <_type> switch_type_spec;
%type <_type> constr_forward_dcl;
%type <_type> constr_forward_name_dcl
%type <_type> type_spec;
%type <_type> param_attribute;
%type <_type> attributes;
%type <_type> attribute;
%type <_type> property_attribute_list;
%type <_type> property_attributes;
%type <_type> property_attribute;
%type <_type> return_attribute_list;
%type <_type> return_attributes;
%type <_type> return_attribute;

%type <_symbol> op;
%type <_symbol> ops;
%type <_symbol> literal;
%type <_symbol> boolean_literal;
%type <_symbol> interfaceid;
%type <_symbol> simple_declarator;
%type <_symbol> complex_declarator;
%type <_symbol> type_declarator;
%type <_symbol> declarator;

%type <_symbol> enumerator;
%type <_symbol> namespace_dcl;
%type <_symbol> param_dcl;
%type <_symbol> attr_dcl;

%type <_scope> declarators;
%type <_scope> member_list;
%type <_scope> member;

%type <_scope> enumerators;
%type <_scope> enum_type;
%type <_scope> scoped_names;
%type <_scope> param_dcls;
%type <_scope> parameter_dcls;
%type <_scope> interface_inheritance_spec

%type <_method> struct_type;
%type <_method> op_dcl;
%type <_method> type_dcl;
%type <_method> const_dcl; 
%type <_method> except_dcl;

%type <_interface> interface;
%type <_interface> interface_name;
%type <_interface> interface_body;
%type <_interface> interface_dcl;
%type <_interface> forward_dcl;
%type <_interface> declarations;
%type <_interface> declaration;
%type <_interface> export;
%type <_interface> attr_dcls;
%type <_interface> op_dcls;
%type <_interface> evnt_dcls;

%type <_anything> module;

%%
specification
	: definitions 	{
		#ifdef IDLDEBUG
			bout << "rule<<<specification" << endl<< endl; 
		#endif
		
		int pos=jheaders.FindLast("::");
		jheaders.Truncate(pos);
		REF->SetHeader(jheaders);
		REF->SetInterfaces(jinterface);

		jinterface.MakeEmpty();
		jheaders=NULL;
	}
	;

definitions
	: definition {
		#ifdef IDLDEBUG
			bout << "rule<<<definitions - definition" << endl<< endl; 
		#endif
	}
	| definitions definition
	;
	
definition
	: interface	{
		#ifdef IDLDEBUG
			bout << "rule<<<interfaces-1" << endl<< endl; 
		#endif	
		jinterface.AddItem(*($1));
		IDLSymbol jsymbol($1->ID());		
		delete $1;
	}
	| export {
		#ifdef IDLDEBUG
			bout << "rule<<<definition - export" << endl<< endl; 
		#endif
		// Notice how the export interface is completely ignored.
		// Enums, typedefs that are done external to the interface
		// are used but not emitted in the output.  
		// Those items need to be defined in other headers.
	}				
	| module {
		#ifdef IDLDEBUG
			bout << "rule<<<definition - module" << endl<< endl; 
		#endif
	}			
	;

module
	: namespace_dcl definitions J_RCURLY {
		#ifdef IDLDEBUG
			bout << "rule<<<module" << endl<< endl; 
		#endif
		
		
		if (jheaders!=NULL) { jheaders.Append("::"); }
		jheaders.Append($1->m_id);

		REF->AddScopeTable(jtable.pop());			

		SString ns;
		StackNode* familyptr=jtable.getCurrentScope();
		
		while (familyptr!=NULL)
		{			
			ns.Prepend(familyptr->getScopeName());
			familyptr=familyptr->getParentInfo();
			
			if (familyptr!=NULL)
			{ ns.Prepend(":"); }
		}

		REF->SetNamesOnStack(ns);	
		//bout << "Namespace=" << REF->NamesOnStack() << endl;
		
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif		
	}
	;
	
namespace_dcl
	: J_NAMESPACE J_IDENTIFIER J_LCURLY	{
		#ifdef IDLDEBUG
			bout << "rule<<<namespace_dcl" << endl<< endl; 
		#endif

		jtable.push($2->m_id);

		// A little nastyness here.  If there are comments
		// associated with the namespace, assume this is
		// first namespace so we should treat them as just
		// some comments at the beginning of the IDL file.
		if ($2->m_comment != NULL)
		{
			REF->SetBeginComments($2->m_comment);
		}
		
		SString ns;
		StackNode* familyptr=jtable.getCurrentScope();
		
		while (familyptr!=NULL)
		{			
			ns.Prepend(familyptr->getScopeName());
			familyptr=familyptr->getParentInfo();
			
			if (familyptr!=NULL)
			{ ns.Prepend(":"); }
		}

		REF->SetNamesOnStack(ns);	
		//bout << "Namespace=" << REF->NamesOnStack() << endl;


		$$=$2;
	}
	;

interface
	: interface_dcl	{ 
		#ifdef IDLDEBUG
			bout << "rule<<<interface" << endl<< endl; 
		#endif
		$$=$1;			
	}										
	| forward_dcl { 
		#ifdef IDLDEBUG
			bout << "rule<<<interface" << endl<< endl;											
		#endif					
		$$=$1;
	}										
	;

interface_dcl
	: interface_body { 
		SString ns;
		StackNode* familyptr=jtable.getCurrentScope();
		
		while (familyptr!=NULL) {			
			ns.Prepend(familyptr->getScopeName());
			familyptr=familyptr->getParentInfo();
			
			if (familyptr!=NULL)
			{ ns.Prepend("::"); }
		}

		#ifdef IDLDEBUG
			bout << "rule<<<interface_dcl, namespace=" << ns << endl<< endl; 
		#endif

		$$=$1;
		$$->SetNamespace(ns);
		bool d=(REF->ParserState()).parseImport;
		//if indeed parsing in import state
		if (d) {	
			$$->SetDeclaration(IMP);
		}	
		else if (!d) {	
			$$->SetDeclaration(DCL);
		}	
	}								
	;

forward_dcl
	: J_INTERFACE J_IDENTIFIER {
		SString ns;
		StackNode* familyptr=jtable.getCurrentScope();
		
		while (familyptr!=NULL)
		{			
			ns.Prepend(familyptr->getScopeName());
			familyptr=familyptr->getParentInfo();
			
			if (familyptr!=NULL) { 
				ns.Prepend("::"); 
			}
		}

		#ifdef IDLDEBUG
			bout << "rule<<<forward_dcl, namespace=" << ns << endl<< endl; 
		#endif

		SVector<SString> cppn;

		bool d=(REF->ParserState()).parseImport;
		//always start as a forward declaration, resolve
		//later if it is something more
		$$=new InterfaceRec($2->m_id, ns, cppn, $2->m_comment, FWD);

		#ifdef IDLDEBUG
			$2->DecStrong(NULL);		 	
		#endif		
	}
	;

interface_name
	: J_INTERFACE interfaceid {
		#ifdef IDLDEBUG
			bout << "rule<<<interface_name" << endl<< endl; 
	 	#endif
		bool d=(REF->ParserState()).parseImport;
		//if indeed parsing in import state
		if (d) {	
			$$=new InterfaceRec($2->m_id, SString(), SVector<SString>(), $2->m_comment, IMP);
		}	
		else if (!d) {	
			$$=new InterfaceRec($2->m_id, SString(), SVector<SString>(), $2->m_comment, DCL);
		}
	}
	| J_LBRACKET J_LOCAL J_RBRACKET J_INTERFACE interfaceid {
		#ifdef IDLDEBUG
			bout << "rule<<<interface_name (local only)" << endl<< endl; 
	 	#endif
		bool d=(REF->ParserState()).parseImport;
		//if indeed parsing in import state
		if (d)
		{	$$=new InterfaceRec($5->m_id, SString(), SVector<SString>(), $5->m_comment, IMP);
		}	
		else if (!d)			
		{	$$=new InterfaceRec($5->m_id, SString(), SVector<SString>(), $5->m_comment, DCL);
		}
		$$->SetAttribute(kLocal);
	}
	| J_INTERFACE interfaceid J_COLON interface_inheritance_spec {
		#ifdef IDLDEBUG
			bout << "rule<<<interface_name - interface inheritance" << endl<< endl; 
	 	#endif

		bool d=(REF->ParserState()).parseImport;
		//if indeed parsing in import state
		if (d)
		{	$$=new InterfaceRec($2->m_id, SString(), SVector<SString>(), $2->m_comment, IMP);
		}	
		else if (!d)			
		{	$$=new InterfaceRec($2->m_id, SString(), SVector<SString>(), $2->m_comment, DCL);
		}	
		// iterate the parents (stored as params in an IDLTypeScope)
		int32_t count = $4->CountParams();
		for (int32_t i = 0; i < count; i++) {
			sptr<IDLNameType> param = $4->ParamAt(i);
			$$->AddParent(param->m_id);
		}
	}
	| J_LBRACKET J_LOCAL J_RBRACKET J_INTERFACE interfaceid J_COLON interface_inheritance_spec {
		#ifdef IDLDEBUG
			bout << "rule<<<interface_name - interface inheritance (local)" << endl<< endl; 
	 	#endif

		bool d=(REF->ParserState()).parseImport;
		//if indeed parsing in import state
		if (d) {	
			$$=new InterfaceRec($5->m_id, SString(), SVector<SString>(), $5->m_comment, IMP);
		}	
		else if (!d) {	
			$$=new InterfaceRec($5->m_id, SString(), SVector<SString>(), $5->m_comment, DCL);
		}
		// iterate the parents (stored as params in an IDLTypeScope)
		int32_t count = $7->CountParams();
		for (int32_t i = 0; i < count; i++) {
			sptr<IDLNameType> param = $7->ParamAt(i);
			$$->AddParent(param->m_id);
		}
		$$->SetAttribute(kLocal);
	}
	;
	
interface_inheritance_spec
	: scoped_names {
		#ifdef IDLDEBUG
			bout << "rule<<<interface_inheritance_spec" << endl<< endl; 
	 	#endif
	 	$$=new IDLTypeScope();
	 	#ifdef IDLDEBUG
	 		$$->IncStrong(NULL);
	 	#endif
	 	SString scopedName = full_scoped_name($1);
	 	$$->AddParam(scopedName, NULL, $1->m_comment);
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
	}
// ---------------------------------------------------------------------------
/*
	NOTE:
	The following comment block disables multiple inheritance in idl.
	When we went from name lookup to index lookup for autobinder,
	we broke multiple inheritance.  The trouble is that for a non left-most
	base interface, the index of method Foo(), will not match the index
	of method Foo() when implemented in the derived interface.  If we have
	a derived interface, Foo() might be at index 8, rather than at index
	0, which we would use if we have the non left-most base interface.
	In the debug build, we unmarshall the key and look for a match, but
	in the release build we ignore the key.
	
	If we want to support multiple inheritance, we need to do the
	following:
	1. Uncomment the following block to allow multiple interface-inheritance_specs
	2. Pidgen must be modified to create an array of offsets into autobinderdef array
	ie: [0, 5, 8]
	Each offset is the start of an interface's methods in the definition array
	
	3. The lookup in execute_autobinder must do the following:
	
	for (i = 0; i < number_of_interfaces) {
		interface_methods_offset = helper_array[i];
		Look at key at array[index+interface_methods_offset];
		if match then break;
	}
	if no match, then report error...
		
	4. Notice that the order of methods in the autobinderdef array must be
	the order specified in the interface.  They cannot be sorted alphabetically.  
	This is so that the order of methods in the derived interface matches
	the order of methods in the left-most base interface.
	
	Looking at the key doesn't require an unmarshall against an SValue,
	it can just compare against the buffer in the parcel.  (Dianne says:
	We have all the parcel and keys right there, so that wouldn't be too hard to do.)
	
	Notice how the loop above would not find a match of Foo() at offset 0
	or offset 5, but it would find a match at offset 8.  (The offset of th
	methods from the non left-most base interface.)
*/
// ---------------------------------------------------------------------------
//	| interface_inheritance_spec J_COMMA scoped_names {
//		#ifdef IDLDEBUG
//			bout << "rule<<<interface_inheritance_spec (comma names)" << endl<< endl; 
//	 	#endif	
//		$$=$1;
//	 	SString scopedName = full_scoped_name($3);
//		$$->AddParam(scopedName, NULL, $3->m_comment);
//		#ifdef IDLDEBUG
//			$3->DecStrong(NULL);
//		#endif
//	}
	;

interface_body
	: interface_name J_LCURLY declarations J_RCURLY {
		#ifdef IDLDEBUG
			bout << "rule<<<interface_body" << endl<< endl; 
	 	#endif

		$$=$1;
		copy_elements($$, $3);
		delete $3;
    }	
	;

/*11*/
interfaceid
	: J_IDENTIFIER { 
		#ifdef IDLDEBUG
			bout << "rule<<<interfaceid" << endl<< endl;
	 	#endif
		$$=$1; 
	}  										
	;

/*12*/
scoped_names
	: J_IDENTIFIER { 
		#ifdef IDLDEBUG
			bout << "rule<<<scoped_names" << endl<< endl;
	 	#endif
		// scoped_names are always types, and we don't
		// want to associate comments with them to push
		// back the comment for the next identifier we see
		pushback_comments($1->m_comment);
		$$=new IDLTypeScope();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->AddParam($1->m_id, $1->m_type, NULL);
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
	}   												
	| scoped_names J_DCOLON J_IDENTIFIER    { 
		#ifdef IDLDEBUG
			bout << "rule<<<scoped_names" << endl<< endl;
	 	#endif
		// scoped_names are always types, and we don't
		// want to associate comments with them to push
		// back the comment for the next identifier we see
		pushback_comments($3->m_comment);
		$$ = $1;
		$$->AddParam($3->m_id, $3->m_type, NULL);
		#ifdef IDLDEBUG
			$3->DecStrong(NULL);		 	
		#endif	
	}					
	;

declarations
	: declaration  { 
		#ifdef IDLDEBUG
			bout << "rule<<<declarations-1" << endl<< endl;
	 	#endif
		$$=$1;
	}
	| declarations declaration {
		#ifdef IDLDEBUG
			bout << "rule<<<declarations-2" << endl<< endl;
	 	#endif

		$$=$1;
		copy_elements($$, $2);
		delete $2;
	}
	;

export
	: type_dcl J_SEMICOLON  { 
		#ifdef IDLDEBUG
			bout << "rule<<<export-type_dcl" << endl<< endl;
	 	#endif
		$$=new InterfaceRec();	

		sptr<IDLType> rt=$1->ReturnType();

		if ((rt->GetCode()=='ENUM') || (rt->GetCode()=='STRT')) {
			$$->AddConstruct($1);
		}
		else if (rt->GetCode() == 'TDEF') {
			sptr<IDLNameType> p = $1->ParamAt(0);
			$$->AddTypedef(p->m_id, p->m_type, p->m_comment);
		}

		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif			
	}
	| const_dcl J_SEMICOLON {
		#ifdef IDLDEBUG
			bout << "rule<<<export-const_dcl - not supported yet " << endl<< endl; 
	 	#endif
	}
	| except_dcl J_SEMICOLON {
		#ifdef IDLDEBUG
			bout << "rule<<<export-except_dcl - not supported yet " << endl<< endl; 
	 	#endif
	}
	| J_USING J_NAMESPACE scoped_names J_SEMICOLON {
		#ifdef IDLDEBUG
			bout << "rule<<<export-using_dcls" << endl<< endl; 
	 	#endif

		SString sn=full_scoped_name($3);
		
		SVector<SString> cppn;
		cppn.AddItem(sn);

		bool d=(REF->ParserState()).parseImport;
		//if indeed parsing in import state
		if (d) { 
			$$=new InterfaceRec(SString(), SString(), cppn, NULL, IMP); 
		}	
		else if (!d) { 
			$$=new InterfaceRec(SString(), SString(), cppn, NULL, DCL); 
		}
		#ifdef IDLDEBUG
			$3->DecStrong(NULL);		 	
		#endif						
	}
	| constr_forward_dcl J_SEMICOLON {
		#ifdef IDLDEBUG
			bout << "rule<<<type_dcl - constr_forward_dcl " << endl<< endl; 
	 	#endif
	 	
		$1->SetCode(B_WILD_TYPE);
		REF->FindIDLType($1, EDIT);
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	

		sptr<IDLType>	t=new IDLType(SString(), B_WILD_TYPE); 
		#ifdef IDLDEBUG
			t->IncStrong(NULL);
	 	#endif		
	}
	;

declaration
	: type_dcl J_SEMICOLON  { 
		#ifdef IDLDEBUG
			bout << "rule<<<export-type_dcl" << endl<< endl;
	 	#endif
		$$=new InterfaceRec();	

		sptr<IDLType> rt=$1->ReturnType();
		
		if ((rt->GetCode()=='ENUM') || (rt->GetCode()=='STRT')) {
			$$->AddConstruct($1);
		}
		else if (rt->GetCode() == 'TDEF') {
			sptr<IDLNameType> p = $1->ParamAt(0);
			$$->AddTypedef(p->m_id, p->m_type, p->m_comment);
		}
		
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif			
	}
	| const_dcl J_SEMICOLON {
		#ifdef IDLDEBUG
			bout << "rule<<<export-const_dcl - not supported yet " << endl<< endl; 
	 	#endif
	}
	| except_dcl J_SEMICOLON {
		#ifdef IDLDEBUG
			bout << "rule<<<export-except_dcl - not supported yet " << endl<< endl; 
	 	#endif
	}
	| J_PROPERTIES J_COLON attr_dcls { 
		#ifdef IDLDEBUG
			bout << "rule<<<export-attr_dcls" << endl<< endl; 
	 	#endif
		$$=$3;			
	}
	| J_METHODS J_COLON op_dcls { 
		#ifdef IDLDEBUG
			bout << "rule<<<export-op_dcls" << endl<< endl; 
	 	#endif
		$$=$3;
	}
	| J_EVENTS J_COLON evnt_dcls {
		#ifdef IDLDEBUG
			bout << "rule<<<export-evnt_dcls" << endl<< endl; 
	 	#endif
		$$=$3;
	}
	| J_USING J_NAMESPACE scoped_names J_SEMICOLON {
		#ifdef IDLDEBUG
			bout << "rule<<<export-using_dcls" << endl<< endl; 
	 	#endif

		SString sn=full_scoped_name($3);
		
		SVector<SString> cppn;
		cppn.AddItem(sn);

		bool d=(REF->ParserState()).parseImport;
		//if indeed parsing in import state
		if (d) { 
			$$=new InterfaceRec(SString(), SString(), cppn, NULL, IMP);
		}	
		else if (!d) {
			$$=new InterfaceRec(SString(), SString(), cppn, NULL, DCL);
		}
		#ifdef IDLDEBUG
			$3->DecStrong(NULL);		 	
		#endif						
	}
	;

attr_dcls
	: attr_dcl {
		#ifdef IDLDEBUG
			bout << "rule<<<attr_dcls" << endl<< endl; 
	 	#endif

		SVector<SString> cppn;

		bool d=(REF->ParserState()).parseImport;
		//if indeed parsing in import state
		if (d) { 
			$$=new InterfaceRec(SString(), SString(), cppn, NULL, IMP);			
		}	
		else if (!d) {
			$$=new InterfaceRec(SString(), SString(), cppn, NULL, DCL);			
		}	

		$$->AddProperty($1->m_id, $1->m_type, $1->m_comment, false);
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
	}
	| attr_dcls attr_dcl {
		#ifdef IDLDEBUG
			bout << "rule<<<attr_dcls" << endl<< endl; 
	 	#endif
		
		$$=$1;
		$$->AddProperty($2->m_id, $2->m_type, $2->m_comment, false);
		#ifdef IDLDEBUG
			$2->DecStrong(NULL);		 	
		#endif		
	}
	;

op_dcls
	: op_dcl {
		#ifdef IDLDEBUG
			bout << "rule<<<op_dcls" << endl<< endl; 
	 	#endif			

		SVector<SString> cppn;

		bool d=(REF->ParserState()).parseImport;
		//if indeed parsing in import state
		if (d) {
			$$=new InterfaceRec(SString(), SString(), cppn, NULL, IMP);
		}	
		else if (!d) {
			$$=new InterfaceRec(SString(), SString(), cppn, NULL, DCL);
		}		

		$$->AddMethod($1);
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
	}
	| op_dcls op_dcl {
		#ifdef IDLDEBUG
			bout << "rule<<<op_dcls" << endl<< endl; 
	 	#endif			

		$$=$1;
		$$->AddMethod($2);
		#ifdef IDLDEBUG
			$2->DecStrong(NULL);		 	
		#endif		
	}
	;

/*evnt_dcls are just like op_dcls except they use AddEvent rather than AddMethod*/
evnt_dcls
	: op_dcl {
		#ifdef IDLDEBUG
			bout << "rule<<<evnt_dcls" << endl<< endl; 
	 	#endif			

		SVector<SString> cppn;

		bool d=(REF->ParserState()).parseImport;
		//if indeed parsing in import state
		if (d) {
			$$=new InterfaceRec(SString(), SString(), cppn, NULL, IMP);
		}	
		else if (!d) {
			$$=new InterfaceRec(SString(), SString(), cppn, NULL, DCL);
		}		

		$$->AddEvent($1);
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
	}
	| evnt_dcls op_dcl {
		#ifdef IDLDEBUG
			bout << "rule<<<evnt_dcls" << endl<< endl; 
	 	#endif			

		$$=$1;
		$$->AddEvent($2);
		#ifdef IDLDEBUG
			$2->DecStrong(NULL);		 	
		#endif		
	}
	;

type_dcl
	: J_TYPEDEF type_declarator { 
		#ifdef IDLDEBUG
			bout << "rule<<<type_dcl" << endl<< endl;
	 	#endif
		// type_dcl wants an IDLMethod so we have to put
		// all information into that.  
		// Notice how enums, structs, and typedefs
		// all use IDLMethod for their own purposes.

		sptr<IDLType> t = new IDLType(SString("typedef"), 'TDEF'); 
		#ifdef IDLDEBUG
			t->IncStrong(NULL);
	 	#endif		

		SString n("typedef ");
		$$=new IDLMethod(n, t, $2->m_comment);
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif

		// Introduce the typedef with new name into our type bank
		// When we are done parsing, we iterate this type bank
		// and find the type specified in the primitive name and hook
		// this new type to the existing type (see main.cpp:generate_from_idl)
		SString syn=$2->m_type->GetName();
		$2->m_type->SetName($2->m_id);
		$2->m_type->SetPrimitiveName(syn);
		REF->FindIDLType($2->m_type, ADD);

		// Add the actual type as a parameter
		$$->AddParam($2->m_id, $2->m_type, $2->m_comment);			
		#ifdef IDLDEBUG
			$2->DecStrong(NULL);		 	
		#endif
	}
	| struct_type {
		#ifdef IDLDEBUG
			bout << "rule<<<type_dcl - struct_type" << endl<< endl; 
	 	#endif			
		$$=$1;
	}
	| union_type {
		#ifdef IDLDEBUG
			bout << "rule<<<type_dcl - union_type " << endl<< endl; 
	 	#endif			
		//$$=$1;
	}
	| enum_type {
		#ifdef IDLDEBUG
			bout << "rule<<<type_dcl - enum_type " << endl<< endl; 
	 	#endif			
		// enum_type gets converted into a method
		// with enumerators as parameters
		
		sptr<IDLType> rt = new IDLType(SString("enum"), 'ENUM'); 
		#ifdef IDLDEBUG
			rt->IncStrong(NULL);
	 	#endif		

		SString n("enum ");
		n.Append($1->ID());
		sptr<IDLNameType> tempName = new IDLNameType(n, rt, $1->m_comment);
		$$ = build_method(tempName.ptr(), rt.ptr(), $1);
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$1->DecStrong(NULL);		 	
		#endif
	}
	| J_NATIVE simple_declarator {
		#ifdef IDLDEBUG
			bout << "rule<<<type_dcl - simple_declarator - not supported yet " << endl<< endl; 
	 	#endif
	}
	;
	
type_declarator
	: type_spec declarator { 
		#ifdef IDLDEBUG
			bout << "rule<<<type_declarator" << endl<< endl;
	 	#endif

		$$=$2;
		$$->m_type=$1;

		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
	}
	//: type_spec declarators { 

	;

/*44*/
type_spec
	: simple_type_spec { 
		#ifdef IDLDEBUG
			bout << "rule<<<type_spec" << endl<< endl;
	 	#endif
		$$=$1;
	}
	| constr_type_spec {
		#ifdef IDLDEBUG
			bout << "rule<<<type_spec - constr_type_spec - not supported yet " << endl<< endl; 
	 	#endif
	}
	;
	
/*45*/
simple_type_spec
	: base_type_spec { 
		#ifdef IDLDEBUG
			bout << "rule<<<simple_type_spec -base_type_spec" << endl<< endl;
	 	#endif
		$$=$1;
	}
	| template_type_spec { 
		#ifdef IDLDEBUG
			bout << "rule<<<simple_type_spec - template_type_spec" << endl<< endl;
	 	#endif
	 	$$=$1;
	}
	| scoped_names {
		#ifdef IDLDEBUG
			bout << "rule<<<simple_type_spec - scoped_names -- (and we don't want to forget it!)" << endl<< endl;
	 	#endif
	 	SString sn=full_scoped_name($1);
	 	sptr<IDLType> temp = new IDLType(sn);
	 	sptr<IDLType> result = REF->FindIDLType(temp, LOOK);
		if (result!=NULL) {	
			$$=new IDLType(result->GetName());
		}
	 	else {			
			// if we don't find the type, we assume we are
			// talking about an interface here, so set up
			// a pointer to the interface
			// When we go use it, if we don't find it in the
			// interface list, the user will be told
			$$ = new IDLType(SString("sptr"));
			// bout << "Assuming interface for ... " << sn << endl;
			$$->SetIface(sn);
		}
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$1->DecStrong(NULL);
	 	#endif
	}
	;
/*27*/
const_dcl
	: J_CONST const_type J_IDENTIFIER J_EQ const_exp {
		#ifdef IDLDEBUG
			bout << "rule<<<const_dcl" << endl<< endl;
	 	#endif

	}
	;

/*28*/
const_type
	: integer_type
	| char_type
	| wide_char_type
	| boolean_type
	| floating_pt_type
	| string_type
	| wide_string_type
	| fixed_pt_const_type
	| scoped_names { 
		#ifdef IDLDEBUG
			bout << "rule<<<const_type - scoped_names - not supported yet" << endl<< endl; 
	 	#endif		
	}
	| octet_type { 
		#ifdef IDLDEBUG
			bout << "rule<<<const_type - octet_type - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;

/*29*/
const_exp
	: or_expr
	;

/*30*/
or_expr
	: xor_expr
	| or_expr J_VERTICALLINE xor_expr
	;

/*31*/
xor_expr
	: and_expr
	| xor_expr J_CIRCUMFLEX and_expr
	;

/*32*/
and_expr
	: shift_expr
	| and_expr J_AMPERSAND shift_expr
	;

/*33*/
shift_expr
	: add_expr
	| shift_expr ">>" add_expr
	| shift_expr "<<" add_expr
	;

/*34*/
add_expr
	: mult_expr
	| add_expr J_PLUS mult_expr
	| add_expr J_MINUS mult_expr
	;

/*35*/
mult_expr
	: unary_expr
	| mult_expr J_ASTERISK unary_expr
	| mult_expr J_SOLIDUS unary_expr
	| mult_expr J_PERCENT unary_expr
	;

/*36*/
unary_expr
	: unary_operator primary_expr
	| primary_expr
	;

/*37*/
unary_operator
	: J_MINUS { 
		#ifdef IDLDEBUG
			bout << "rule<<<unary_operator - J_MINUS - not supported yet" << endl<< endl; 
	 	#endif		
	}
	| J_PLUS { 
		#ifdef IDLDEBUG
			bout << "rule<<<unary_operator - J_PLUS - not supported yet" << endl<< endl; 
	 	#endif		
	}
	| J_TILDE { 
		#ifdef IDLDEBUG
			bout << "rule<<<unary_operator - J_TILDE - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;	

/*39*/
primary_expr
	: scoped_names { 
		#ifdef IDLDEBUG
			bout << "rule<<<primary_expr - scoped_names - not supported yet" << endl<< endl; 
	 	#endif		
	}
	| literal { 
		#ifdef IDLDEBUG
			bout << "rule<<<primary_expr - literal - not supported yet" << endl<< endl; 
	 	#endif		
	}
	| J_LPAREN const_exp J_RPAREN { 
		#ifdef IDLDEBUG
			bout << "rule<<<primary_expr - const_exp - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;
	
/*40*/
literal
	: J_INTLITERAL { 
		#ifdef IDLDEBUG
			bout << "rule<<<literal - int_literal" << endl<< endl; 
	 	#endif		

		$$=$1;
	}
	| J_STRINGLITERAL { 
		#ifdef IDLDEBUG
			bout << "rule<<<literal - string_literal" << endl<< endl; 
	 	#endif		

		$$=$1;
	}
	| J_STRINGOPSLITERAL { 
		#ifdef IDLDEBUG
			bout << "rule<<<literal - string_literal" << endl<< endl; 
	 	#endif		

		$$=$1;
	}
	| J_CHARLITERAL { 
		#ifdef IDLDEBUG
			bout << "rule<<<literal - char_literal" << endl<< endl; 
	 	#endif		

		$$=$1;
	}
	| J_FIXEDPTLITERAL { 
		#ifdef IDLDEBUG
			bout << "rule<<<literal - fixed_pt_literal" << endl<< endl; 
	 	#endif		

		$$=$1;
	}
	| J_FLOATPTLITERAL { 
		#ifdef IDLDEBUG
			bout << "rule<<<literal - int_literal" << endl<< endl; 
	 	#endif		

		$$=$1;
	}
	| boolean_literal { 
		#ifdef IDLDEBUG
			bout << "rule<<<literal - boolean_literal" << endl<< endl; 
	 	#endif		

		$$=$1;
	}
	;
	
/*40*/
boolean_literal
	: J_TRUE  { 
		#ifdef IDLDEBUG
			bout << "rule<<<boolean_literal - not supported yet" << endl<< endl; 
	 	#endif		
	}	
	| J_FALSE { 
		#ifdef IDLDEBUG
			bout << "rule<<<boolean_literal - not supported yet" << endl<< endl; 
	 	#endif		
	}	
	;

/*41*/
positive_int_const
	: const_exp
	;

/*46*/
base_type_spec
	: floating_pt_type { 
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 	
	 	#endif	
		$$=$1; 
	} 
	| integer_type { 	
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif
		$$=$1; 
	}
	| char_type { 
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec - char_type" << endl<< endl; 
	 	#endif
		$$=$1; 
	}
	| charptr_type { 
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec - charptr_type" << endl<< endl; 
	 	#endif
		$$=$1; 
	}
	| wide_char_type { 
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif
		$$=$1; 
	} 
	| boolean_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif
		$$=$1; 
	}
	| octet_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif
		$$=$1; 
	}
	| any_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif
		$$=$1; 
	}
	| object_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec - !Attn! objects not supported" << endl<< endl; 
	 	#endif	
	}
	| value_base_type { 	
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	} 
	| svalue_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;			
	}
	| sstring_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	}
	| smessage_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	}
	| statust_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	}
	| sizet_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	}
	| ssizet_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	}
 	| offt_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	}
	| nsecst_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	}
	| bigtimet_type {
		#ifdef IDLDEBUG
			bout << "rule<<<base_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	}
	;
		
svalue_type 
	: J_SVALUE { 
		#ifdef IDLDEBUG
			bout << "rule<<<svalue_type" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	;
	
sstring_type 
	: J_SSTRING { 
		#ifdef IDLDEBUG
			bout << "rule<<<sstring_type" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	;
	
smessage_type 
	: J_SMESSAGE { 
		#ifdef IDLDEBUG
			bout << "rule<<<smessage_type" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	;

statust_type 
	: J_STST { 
		#ifdef IDLDEBUG
			bout << "rule<<<statust_type" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	;	

sizet_type 
	: J_SIZT { 
		#ifdef IDLDEBUG
			bout << "rule<<<sizet_type" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	;

ssizet_type 
	: J_SSZT { 
		#ifdef IDLDEBUG
			bout << "rule<<<ssizet_type" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	;
	
offt_type 
	: J_OFFT { 
		#ifdef IDLDEBUG
			bout << "rule<<<offt_type" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	;	

nsecst_type	
	: J_NSST { 
		#ifdef IDLDEBUG
			bout << "rule<<<nsecst_type" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	;

bigtimet_type	
	: J_BIGT { 
		#ifdef IDLDEBUG
			bout << "rule<<<bigtimet_type" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	;	

/*47*/
template_type_spec
	: sequence_type { 
		#ifdef IDLDEBUG
			bout << "rule<<<template_type_spec" << endl<< endl; 
	 	#endif
	 	$$->SetCode(B_VARIABLE_ARRAY_TYPE);
	 	$$=$1;
	}	
	| string_type { 
		#ifdef IDLDEBUG
			bout << "rule<<<template_type_spec - string_type" << endl<< endl; 
	 	#endif
	 	$$=$1;
	}
	| wide_string_type {
		#ifdef IDLDEBUG
			bout << "rule<<<template_type_spec - wide_string_type (not supported)" << endl<< endl; 
	 	#endif		
	}
	| fixed_pt_type { 
		#ifdef IDLDEBUG
			bout << "rule<<<template_type_spec - fixed_pt_type (not supported)" << endl<< endl; 
	 	#endif		
	}
	;

/*48*/
constr_type_spec
	: struct_type { 			
		#ifdef IDLDEBUG
			bout << "rule<<<constr_type_spec - not supported yet" << endl<< endl; 
	 	#endif		
	}
	| union_type
	| enum_type { 
		#ifdef IDLDEBUG
			bout << "rule<<<constr_type_spec - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;
	
/*49*/
declarators
	: declarator { 
		#ifdef IDLDEBUG
			bout << "rule<<<declarators --->" << endl<< endl; 
	 	#endif		
		
		$$=new IDLTypeScope();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->AddParam($1->m_id, $1->m_type, $1->m_comment);
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
	}
	| declarators J_COMMA declarator { 
		#ifdef IDLDEBUG
			bout << "<--- rule declarators --->" << endl<< endl; 
		#endif
		$$=$1;

		$$->AddParam($3->m_id, $3->m_type, $3->m_comment);
		#ifdef IDLDEBUG
			$3->DecStrong(NULL);		 	
		#endif				
	}
	;

/*50*/
declarator
	: simple_declarator { 
		#ifdef IDLDEBUG
			bout << "rule<<<declarator" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	| complex_declarator{ 
		#ifdef IDLDEBUG
			bout << "rule<<<declarator - complex_declarator - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;

/*51*/
simple_declarator
	: J_IDENTIFIER {
		#ifdef IDLDEBUG
			bout << "rule<<<simple_declarator" << endl<< endl; 
	 	#endif		
		$$=$1;
	}
	;

/*52*/
complex_declarator
	: array_declarator { 
		#ifdef IDLDEBUG
			bout << "rule<<<complex_declarator - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;

/*53*/
floating_pt_type
	: J_FLOAT { 
		#ifdef IDLDEBUG
			bout << "rule<<<floating_pt_type" << endl<< endl; 
	 	#endif		
		$$=$1; 
	}
	| J_DOUBLE {
		#ifdef IDLDEBUG
			bout << "rule<<<floating_pt_type" << endl<< endl; 
	 	#endif		
		$$=$1; 
	}
	| J_LONG J_DOUBLE { 
		#ifdef IDLDEBUG
			bout << "rule<<<floating_pt_type" << endl<< endl; 
	 	#endif		
		$$=$2; // for now, later will be both 1&2 combined
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
	}
	;

/*54*/
integer_type
	: signed_int { 
		#ifdef IDLDEBUG
			bout << "rule<<<integer_type" << endl<< endl; 
	 	#endif		
		$$=$1; 
	}
	| unsigned_int {
		#ifdef IDLDEBUG
			bout << "rule<<<integer_type" << endl<< endl; 
	 	#endif
		$$=$1; 
	}
	;

/*55*/
signed_int
	: signed_int8 { 
		#ifdef IDLDEBUG
			bout << "rule<<<signed_int" << endl<< endl; 
	 	#endif
	 	$$=$1; 
	}
	| signed_short_int { 
		#ifdef IDLDEBUG
			bout << "rule<<<signed_int" << endl<< endl; 
	 	#endif
	 	$$=$1; 
	}
	| signed_long_int { 
		#ifdef IDLDEBUG
			bout << "rule<<<signed_int" << endl<< endl; 
	 	#endif			
		$$=$1; 
	}
	| signed_longlong_int { 
		#ifdef IDLDEBUG
			bout << "rule<<<signed_int" << endl<< endl; 
	 	#endif
		$$=$1; 
	}
	;

/*56*/
signed_int8
	: J_INT8 { 
		#ifdef IDLDEBUG
			bout << "rule<<<signed_int8" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	;
signed_short_int
	: J_SHORT { 
		#ifdef IDLDEBUG
			bout << "rule<<<signed_short_int" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	;

/*57*/
signed_long_int
	: J_LONG {
		#ifdef IDLDEBUG
			bout << "rule<<<signed_long_int" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	;
	
/*58*/
signed_longlong_int
	: J_INT64 { 
		#ifdef IDLDEBUG
			bout << "rule<<<signed_longlong_int" << endl<< endl; 
	 	#endif	
		$$=$1;
	}
	;
	
/*59*/
unsigned_int
	: unsigned_int8 { 
		#ifdef IDLDEBUG
			bout << "rule<<<unsigned_int" << endl<< endl; 
	 	#endif
	 	$$=$1; 
	}
	| unsigned_short_int { 
		#ifdef IDLDEBUG
			bout << "rule<<<unsigned_int" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	| unsigned_long_int {
		#ifdef IDLDEBUG
			bout << "rule<<<unsigned_int" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	| unsigned_longlong_int {
		#ifdef IDLDEBUG
			bout << "rule<<<unsigned_int" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	;	
	

unsigned_int8
	: J_UINT8 {
		#ifdef IDLDEBUG
			bout << "rule<<<unsigned_short_int" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	;

/*60*/
unsigned_short_int
	: J_UINT16 {
		#ifdef IDLDEBUG
			bout << "rule<<<unsigned_short_int" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	;

/*61*/
unsigned_long_int
	: J_UINT32 {
		#ifdef IDLDEBUG
			bout << "rule<<<unsigned_long_int" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	;
		
/*62*/
unsigned_longlong_int
	: J_UINT64 {
		#ifdef IDLDEBUG
			bout << "rule<<<unsigned_long_long_int" << endl<< endl; 
	 	#endif	

		$$=$1; 
	}
	;

/*63*/
char_type
	: J_CHAR {			
		#ifdef IDLDEBUG
			bout << "rule<<<char_type" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	;
	
charptr_type
	: J_CHAR J_ASTERISK {			
		#ifdef IDLDEBUG
			bout << "rule<<<char*_type" << endl<< endl; 
	 	#endif	
		$$=$1;
		$$->SetCode(B_CONSTCHAR_TYPE);
		$$->SetName(SString("char*")); 
	}
	;


/*64*/
wide_char_type
	: J_WCHAR32 { 
		#ifdef IDLDEBUG
			bout << "rule<<<wchar32_type" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	;
	
/*65*/
boolean_type
	: J_BOOLEAN { 
		#ifdef IDLDEBUG
			bout << "rule<<<boolean_type" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	;
	
/*66*/
octet_type
	: J_OCTET { 
		#ifdef IDLDEBUG
			bout << "rule<<<octet_type" << endl<< endl; 
	 	#endif	
		$$=$1; 
       }
	;
	
/*67*/
any_type
	: J_ANY {
		#ifdef IDLDEBUG
			bout << "rule<<<any_type" << endl<< endl; 
	 	#endif	
		$$=$1; 
	   }
	;
	
/*68*/
object_type
	: J_OBJECT { 
		#ifdef IDLDEBUG
			bout << "rule<<<object_type - not supported" << endl<< endl; 
	 	#endif	
	}
	;

/*69*/
struct_type
	: J_STRUCT J_IDENTIFIER J_LCURLY member_list J_RCURLY  { 
		#ifdef IDLDEBUG
			bout << "rule<<<struct_type - not supported yet" << endl<< endl; 
	 	#endif		

		sptr<IDLType> rt=new IDLType(SString("struct"), 'STRT');
		#ifdef IDLDEBUG
			rt->IncStrong(NULL);
	 	#endif		

		SString n("struct ");
		n.Append($2->m_id);
		sptr<IDLNameType> tempName = new IDLNameType(n, rt, $2->m_comment);
		$$ = build_method(tempName.ptr(), rt.ptr(), $4);
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$2->DecStrong(NULL);
			$4->DecStrong(NULL);
	 	#endif
	}
	;
	
/*70*/
member_list
	: member {
		#ifdef IDLDEBUG
			bout << "rule<<<member" << endl<< endl; 
	 	#endif
			$$=new IDLTypeScope();
			#ifdef IDLDEBUG
				$$->IncStrong(NULL);
		 	#endif
	
			SString n=$1->ID();
			n.Append(" ");

			for (int32_t i=0; i<$1->CountParams(); i++) {	
				sptr<IDLNameType>	p=$1->ParamAt(i);
				n.Append(p->m_id);
				if (i<$1->CountParams()-1) { 
					n.Append(", "); 
				}
			}
			sptr<IDLType> t=new IDLType($1->ID(), 'STRT'); 
			#ifdef IDLDEBUG
				t->IncStrong(NULL);
		 	#endif		

			$$->AddParam(n, t , NULL);
			#ifdef IDLDEBUG
				$1->DecStrong(NULL);		 	
			#endif	
	}
	| member_list member {
		#ifdef IDLDEBUG
			bout << "rule<<<member" << endl<< endl; 
	 	#endif
			$$=$1;
	
			SString n=$2->ID();
			n.Append(" ");
			for (int32_t i=0; i<$2->CountParams(); i++) {	
				sptr<IDLNameType> p=$2->ParamAt(i);
				n.Append(p->m_id);
				if (i<$2->CountParams()-1) {
					n.Append(", ");
				}
			}
			sptr<IDLType> t=new IDLType($2->ID(), 'STRT'); 
			#ifdef IDLDEBUG
				t->IncStrong(NULL);
		 	#endif		

			$$->AddParam(n, t , NULL);
			#ifdef IDLDEBUG
				$2->DecStrong(NULL);
		 	#endif
	}
	;

/*71*/
member
	: type_spec declarators J_SEMICOLON { 
		#ifdef IDLDEBUG
			bout << "rule<<<member" << endl<< endl; 
	 	#endif

		$$=new IDLTypeScope($1->GetName(), $2->m_comment);	
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif

		for (int32_t i=0; i<$2->CountParams(); i++) {
			sptr<IDLNameType>	p=$2->ParamAt(i);
			$$->AddParam(p->m_id, p->m_type, p->m_comment);
		}

		#ifdef IDLDEBUG
			$2->DecStrong(NULL);
	 	#endif
	}
	;	
	
/*72*/
union_type
	: J_UNION J_IDENTIFIER J_SWITCH J_LPAREN switch_type_spec J_RPAREN J_LCURLY switch_body J_RCURLY  { 
		#ifdef IDLDEBUG
			bout << "rule<<<union_type - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;

/*73*/
switch_type_spec
	: integer_type 
	| char_type 
	| boolean_type 
	| enum_type { 
		#ifdef IDLDEBUG
			bout << "rule<<<switch_type_spec - enum_type - not supported yet" << endl<< endl; 
	 	#endif		
	}
	| scoped_names { 
		#ifdef IDLDEBUG
			bout << "rule<<<switch_type_spec - scoped_names - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;

/*74*/
switch_body
	: cases
	;
cases
	: case
	| cases case
	;

/*75*/
case
	: case_labels element_spec J_SEMICOLON 
	;
	
case_labels
	: case_label
	| case_labels case_label
	;
	
/*76*/
case_label
	: J_CASE const_exp J_COLON  { 
		#ifdef IDLDEBUG
			bout << "rule<<<case_label - not supported yet" << endl<< endl; 
	 	#endif		
	}
	| J_DEFAULT J_COLON  { 
		#ifdef IDLDEBUG
			bout << "rule<<<case_label - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;
	
/*77*/
element_spec
	: J_CHAR { 
		#ifdef IDLDEBUG
			bout << "rule<<<element_spec - not supported yet" << endl<< endl; 
	 	#endif		
	}
/*	: type_spec declarator*/ 
	;	

/*78*/
enum_type
	: J_ENUM J_IDENTIFIER J_LCURLY enumerators J_RCURLY { 
		#ifdef IDLDEBUG
			bout << "<--- rule enum_type --->" << endl<< endl; 
	 	#endif
		
		// notice that the comment comes from J_ENUM in this case
		$$=new IDLTypeScope($2->m_id, $1->m_comment);
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif		

		for (int32_t i=0; i<$4->CountParams(); i++) {	
			sptr<IDLNameType>	p=$4->ParamAt(i);
			$$->AddParam(p->m_id, p->m_type, p->m_comment);			
		}
		#ifdef IDLDEBUG
			$4->DecStrong(NULL);
	 	#endif		
	}
	| J_ENUM J_LCURLY enumerators J_RCURLY { 
		#ifdef IDLDEBUG
			bout << "<--- rule enum_type --->" << endl<< endl; 
	 	#endif
		$$=$3;
	 	// the IDLScope from enumerators has everything we need
	 	// except the comment associated with J_ENUM
	 	$$->m_comment = $1->m_comment;
	}
	;

enumerators 
	: enumerator { 
		#ifdef IDLDEBUG
			bout << "<--- rule enumerator --->" << endl<< endl; 
	 	#endif
		$$=new IDLTypeScope();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif

		$$->AddParam($1->m_id, $1->m_type, $1->m_comment);
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif				
	}										
	| enumerators J_COMMA enumerator { 
		#ifdef IDLDEBUG
			bout << "<--- rule enumerator --->" << endl<< endl; 
	 	#endif
		$$=$1;

		$$->AddParam($3->m_id, $3->m_type, $3->m_comment);
		#ifdef IDLDEBUG
			$3->DecStrong(NULL);		 	
		#endif					
	}
	;	
	
		
op 
	: J_PLUS	{
		$$=new IDLNameType();
		$$->m_id="+";
	}
	| J_MINUS {
		$$=new IDLNameType();
		$$->m_id="-";
	}
	| J_LT {
		$$=new IDLNameType();
		$$->m_id="<";
	}
	| J_GT {
		$$=new IDLNameType();
		$$->m_id=">";
	}
	;

ops
	: op {
		$$=$1;
	}
	| ops op {
		$$=$1;
		$$->m_id.Append($2->m_id);
	}
	;	

	
/*79*/
enumerator
	: J_IDENTIFIER { 
		#ifdef IDLDEBUG
			bout << "<--- rule enumerator --->" << endl<< endl; 
	 	#endif
	 	$$=$1;	
	}	
	| J_IDENTIFIER J_EQ literal {
		#ifdef IDLDEBUG
			bout << "<--- rule enumerator --->" << endl<< endl; 
	 	#endif

		SString v=$1->m_id;
		v.Append("=");
		v.Append($3->m_id);

	 	$$=$1;
		$$->m_id=v;
		#ifdef IDLDEBUG
			$3->DecStrong(NULL);		 	
		#endif		
	}	
	| J_IDENTIFIER J_EQ literal ops literal {
		$$=$1;
		$$->m_id.Append("=");
		$$->m_id.Append($3->m_id);
		$$->m_id.Append($4->m_id);
		$$->m_id.Append($5->m_id);
	}				
	;	
	
	
/*80*/
// In the future we could support this syntax where the user
// can specify a maximum bound on the sequence
// 	| J_SEQUENCE J_LT simple_type_spec J_COMMA positive_int_const J_GT

sequence_type
	: J_SEQUENCE J_LT simple_type_spec J_GT {
		#ifdef IDLDEBUG
			bout << "rule<<<sequence_type" << endl<< endl; 
	 	#endif	
		$$=$3;
	}
	;	

/*81*/
string_type
	: J_STRING J_LT positive_int_const J_GT 
	| J_STRING {
		#ifdef IDLDEBUG
			bout << "rule<<<string_type" << endl<< endl; 
	 	#endif	
		$$=$1;
	}
	;	
	
/*82*/
wide_string_type
	: J_WSTRING J_LT positive_int_const J_GT
	| J_WSTRING
	;

/*83*/
array_declarator
	: J_IDENTIFIER fixed_array_sizes {
		#ifdef IDLDEBUG
			bout << "rule<<<array_declarator - not supported yet " << endl<< endl; 
	 	#endif
	}
	;
	
fixed_array_sizes
	: fixed_array_size
	| fixed_array_sizes fixed_array_size
	;	
	
/*84*/
fixed_array_size
	: J_LBRACKET positive_int_const J_RBRACKET { 
		#ifdef IDLDEBUG
			bout << "rule<<<fixed_array_size - not supported yet" << endl<< endl; 
	 	#endif		
	}	
	;
	
/*85*/
// XXX NEED WORK HERE
attr_dcl
	: param_type_spec simple_declarator J_SEMICOLON {
		#ifdef IDLDEBUG
			bout << "rule<<<attr_dcl" << endl<< endl; 
	 	#endif	
		$$=new IDLNameType($2->m_id, $1, $2->m_comment, false);
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
		#ifdef IDLDEBUG
			$2->DecStrong(NULL);
	 	#endif
	}	
	|  property_attribute_list param_type_spec simple_declarator J_SEMICOLON {
		#ifdef IDLDEBUG
			bout << "rule<<<attr_dcl" << endl<< endl; 
	 	#endif	
		$2->SetAttributes($1->GetAttributes());
		$$=new IDLNameType($3->m_id, $2, $3->m_comment, true);
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$2->DecStrong(NULL);
			$3->DecStrong(NULL);
	 	#endif
	}			
	;
	
property_attribute_list
	: J_LBRACKET property_attributes J_RBRACKET {
		#ifdef IDLDEBUG
			bout << "rule<<<param_attribute" << endl<< endl; 
	 	#endif
	 	$$=$2;
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$2->DecStrong(NULL);
	 	#endif
	}
	;

property_attributes
	: property_attribute {
		#ifdef IDLDEBUG
			bout << "rule<<<property_attributes - attribute" << endl<< endl; 
	 	#endif
		$$ = $1;
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$1->DecStrong(NULL);
	 	#endif
	}
	| property_attributes property_attribute {
		#ifdef IDLDEBUG
			bout << "rule<<<property_attributes - property_attributes property_attribute" << endl<< endl; 
	 	#endif
	 	// at this point, we have two (or more) attributes
	 	// we need to combine them into our result, otherwise we will only get
	 	// the last attribute... To do this, we OR them together.  However,
	 	// we need spedcial handling of the direction attributes.  If they
	 	// have been set in the final attribute, that overrides the previous.
	 	// All that is handled in AddAttributes
	 	$1->AddAttributes($2->GetAttributes());
		$$ = $1;
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$1->DecStrong(NULL);
			$2->DecStrong(NULL);
	 	#endif
	}
	;
	
property_attribute
	: J_READONLY {
		#ifdef IDLDEBUG
			bout << "rule<<<property_attribute - read only " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kReadOnly);
	}
	| J_WEAK {
		#ifdef IDLDEBUG
			bout << "rule<<<property_attribute - weak pointer " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kWeak);
	}
	| J_RESERVED {
		#ifdef IDLDEBUG
			bout << "rule<<<property_attribute - reserved " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kReserved);
	}
	;
	
/*86*/
except_dcl
	: J_EXCEPTION J_IDENTIFIER J_LCURLY members J_RCURLY { 
		#ifdef IDLDEBUG
			bout << "rule<<<except_dcl - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;	

members
	:
	| members member
	;

/*87 - IDLMethod*/
op_dcl
	: op_type_spec J_IDENTIFIER parameter_dcls J_SEMICOLON { 
		#ifdef IDLDEBUG
			bout << "rule<<<op_dcl1" << endl<< endl; 
	 	#endif	
		$$ = build_method($2, $1, $3);
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$1->DecStrong(NULL);		 	
			$2->DecStrong(NULL);
			$3->DecStrong(NULL); 
		#endif	
	}	
	| op_type_spec J_IDENTIFIER parameter_dcls J_CONST J_SEMICOLON { 
		#ifdef IDLDEBUG
			bout << "rule<<<op_dcl1" << endl<< endl; 
	 	#endif
	 	$$ = build_method($2, $1, $3);
	 	$$->SetConst(true);
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$1->DecStrong(NULL);		 	
			$2->DecStrong(NULL);
			$3->DecStrong(NULL); 
		#endif	
	}
	| op_type_spec J_IDENTIFIER parameter_dcls raises_expr J_SEMICOLON { 
		bout << "rule op_dcl" << endl<< endl; 
	}
	;

/*89*/
op_type_spec
	: param_type_spec {
		#ifdef IDLDEBUG
			bout << "rule<<<op_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	}	
	| J_VOID { 
		#ifdef IDLDEBUG
			bout << "rule<<<op_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1;
	}	
	| return_attribute_list param_type_spec {
		#ifdef IDLDEBUG
			bout << "rule<<<op_type_spec" << endl<< endl; 
	 	#endif	
		$$=$2;
		$$->SetAttributes($1->GetAttributes());
	}	
	| return_attribute_list J_VOID { 
		#ifdef IDLDEBUG
			bout << "rule<<<op_type_spec" << endl<< endl; 
	 	#endif	
		$$=$2;
		$$->SetAttributes($1->GetAttributes());
	}	
	;

return_attribute_list
	: J_LBRACKET return_attributes J_RBRACKET {
		#ifdef IDLDEBUG
			bout << "rule<<<return_attribute_list" << endl<< endl; 
	 	#endif
	 	$$=$2;
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$2->DecStrong(NULL);
	 	#endif
	}
	;

return_attributes
	: return_attribute {
		#ifdef IDLDEBUG
			bout << "rule<<<return_attributes - return_attribute" << endl<< endl; 
	 	#endif
		$$ = $1;
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$1->DecStrong(NULL);
	 	#endif
	}
	| return_attributes return_attribute {
		#ifdef IDLDEBUG
			bout << "rule<<<return_attributes - return_attributes return_attribute" << endl<< endl; 
	 	#endif
	 	// at this point, we have two (or more) attributes
	 	// we need to combine them into our result, otherwise we will only get
	 	// the last attribute... To do this, we OR them together.  However,
	 	// we need spedcial handling of the direction attributes.  If they
	 	// have been set in the final attribute, that overrides the previous.
	 	// All that is handled in AddAttributes
	 	$1->AddAttributes($2->GetAttributes());
		$$ = $1;
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$1->DecStrong(NULL);
			$2->DecStrong(NULL);
	 	#endif
	}
	;
	
return_attribute
	: J_LOCAL {
		#ifdef IDLDEBUG
			bout << "rule<<<return_attribute - local " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kLocal);
	}
	| J_WEAK {
		#ifdef IDLDEBUG
			bout << "rule<<<return_attribute - weak pointer " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kWeak);
	}
	| J_RESERVED {
		#ifdef IDLDEBUG
			bout << "rule<<<return_attribute - reserved " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kReserved);
	}
	;

/*90*/	
parameter_dcls
	: J_LPAREN param_dcls J_RPAREN	{ 
		#ifdef IDLDEBUG
			bout << "rule<<<parameter_dcls" << endl<< endl; 
	 	#endif	
		$$=$2;
	}
	| J_LPAREN J_RPAREN				{
		#ifdef IDLDEBUG
			bout << "rule<<<parameter_dcls" << endl<< endl; 
	 	#endif	
		$$=new IDLTypeScope();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif       
	}		
	;
	
param_dcls
	: param_dcl {
		#ifdef IDLDEBUG
			bout << "rule<<<param_dcls" << endl<< endl; 
	 	#endif	
		$$=new IDLTypeScope();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->AddParam($1->m_id, $1->m_type, $1->m_comment);
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
	}
	| param_dcls J_COMMA param_dcl { 
		#ifdef IDLDEBUG
			bout << "rule<<<param_dcls" << endl<< endl; 
	 	#endif	
		$$=$1;
		$$->AddParam($3->m_id, $3->m_type, $3->m_comment);
		#ifdef IDLDEBUG
			$3->DecStrong(NULL);
		#endif
	}
	;
	
/*91*/
param_dcl
	: param_type_spec simple_declarator { 
		#ifdef IDLDEBUG
			bout << "rule<<<param_dcl" << endl<< endl; 
	 	#endif		
		$$=$2;
		$$->m_type=$1;
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
	}
	| param_attribute param_type_spec simple_declarator {
		#ifdef IDLDEBUG
			bout << "rule<<<param_dcl" << endl<< endl; 
	 	#endif		
		$$=$3;
		$2->SetAttributes($1->GetAttributes());
		$$->m_type=$2;
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
			$2->DecStrong(NULL);
		#endif	
	}
	;

/*92*/
param_attribute
	: J_LBRACKET attributes J_RBRACKET {
		#ifdef IDLDEBUG
			bout << "rule<<<param_attribute" << endl<< endl; 
	 	#endif
	 	$$=$2;
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$2->DecStrong(NULL);
	 	#endif
	}
	;

attributes
	: attribute {
		#ifdef IDLDEBUG
			bout << "rule<<<attributes" << endl<< endl; 
	 	#endif
		$$ = $1;
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$1->DecStrong(NULL);
	 	#endif
	}
	| attributes attribute {
		#ifdef IDLDEBUG
			bout << "rule<<<attributes - attributes attribute" << endl<< endl; 
	 	#endif
	 	// at this point, we have two (or more) attributes
	 	// we need to combine them into our result, otherwise we will only get
	 	// the last attribute... To do this, we OR them together.  However,
	 	// we need spedcial handling of the direction attributes.  If they
	 	// have been set in the final attribute, that overrides the previous.
	 	// All that is handled in AddAttributes
	 	$1->AddAttributes($2->GetAttributes());
		$$ = $1;
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$1->DecStrong(NULL);
			$2->DecStrong(NULL);
	 	#endif
	}
	;
	
attribute
	: J_IN {
		#ifdef IDLDEBUG
			bout << "rule<<<attribute - in " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kIn);
	}
	| J_OUT {
		#ifdef IDLDEBUG
			bout << "rule<<<attribute - out " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kOut);
	}
	| J_INOUT {
		#ifdef IDLDEBUG
			bout << "rule<<<attribute - inout " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kInOut);
	}
	| J_OPTIONAL {
		#ifdef IDLDEBUG
			bout << "rule<<<attribute - optional " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kOptional);
	}
	| J_WEAK {
		#ifdef IDLDEBUG
			bout << "rule<<<attribute - weak pointer " << endl<< endl; 
	 	#endif
		$$=new IDLType();
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif
		$$->SetAttribute(kWeak);
	}
	;
	
/*93*/
raises_expr
	: J_RAISES J_LPAREN scoped_names
	;


/*95*/
param_type_spec
	: base_type_spec {
		#ifdef IDLDEBUG
			bout << "rule<<<param_type_spec" << endl<< endl; 
	 	#endif		
		$$=$1; 
	}
	| string_type {
		#ifdef IDLDEBUG
			bout << "rule<<<param_type_spec" << endl<< endl; 
	 	#endif	
		$$=$1; 
	}
	| wide_string_type {
		#ifdef IDLDEBUG
			bout << "rule<<<param_type_spec" << endl<< endl; 
	 	#endif
		$$=$1; 
	}
	| scoped_names { 
		#ifdef IDLDEBUG
			bout << "rule<<<param_type_spec" << endl<< endl;
	 	#endif
	 	
	 	SString sn=full_scoped_name($1);
	 	sptr<IDLType> temp = new IDLType(sn);
	 	sptr<IDLType> result = REF->FindIDLType(temp, LOOK);
		if (result!=NULL) {	
			$$=new IDLType(result->GetName());
		}
	 	else {
			// if we don't find the type, we assume we are
			// talking about an interface here, so set up
			// a pointer to the interface
			// When we go use it, if we don't find it in the
			// interface list, the user will be told
			$$ = new IDLType(SString("sptr"));
			// bout << "Assuming interface for ... " << sn << endl;
			$$->SetIface(sn);
		}

		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
	 	#endif			
		#ifdef IDLDEBUG
			$1->DecStrong(NULL);		 	
		#endif	
		//delete result;
	}
	;	

/*96*/ 
fixed_pt_type
	: J_FIXED J_LT positive_int_const J_COMMA positive_int_const J_GT   { 
		#ifdef IDLDEBUG
			bout << "rule<<<fixed_pt_type - not supported yet" << endl<< endl; 
	 	#endif		
	}
	;
		
/*97*/
fixed_pt_const_type
	: J_FIXED {
		#ifdef IDLDEBUG
			bout << "rule<<<fixed_pt_const_type - not supported yet " << endl<< endl; 
	 	#endif
	}
	;
	
/*98*/	
value_base_type
	: J_VALUEBASE {
		#ifdef IDLDEBUG
			bout << "rule<<<value_base_type - not supported yet " << endl<< endl; 
	 	#endif
	}
	;
		
/*99*/
constr_forward_dcl
	: constr_forward_name_dcl  J_LCURLY declarations J_RCURLY{ 
		#ifdef IDLDEBUG
			bout << "rule<<<constr_forward_dcl" << endl<< endl; 
	 	#endif		

		$$=$1;

		#ifdef IDLDEBUG
			bout << "rule<<<constr_forward_dcl ----- name=" 
					<< $$->GetCode() << endl<< endl; 
	 	#endif	
		
		for (int32_t index=0; index<($3->CountMethods()); index++) {		
			sptr<IDLMethod> m = $3->MethodAt(index);
			sptr<IDLType> rt = m->ReturnType();
			jmember* j=new jmember(m->ID(), rt->GetName());
			#ifdef IDLDEBUG
				j->IncStrong(NULL);
			 #endif
				
			for (int32_t index2=0; index2<(m->CountParams()); index2++) {
				sptr<IDLNameType>	nt=m->ParamAt(index2);
				j->AddParam(nt->m_id, nt->m_type->GetName());
			}
	
			$$->AddMember(j);
		}

		#ifdef IDLDEBUG
		int32_t numberoffunc=$$->CountMembers();
		for (int32_t index=0; index<numberoffunc ; index++) {		
			sptr<jmember> tm=$$->GetMemberAt(index);
			bout << "rule<<<constr_forward_dcl ----- function=" 
				 << tm->ID() << endl<< endl; 
				
			if (tm->CountParams()>0) {  
				bout << "			----- param name=" 
					 << (tm->ParamAt(0))->m_id << endl<< endl; 	
				bout << "			----- param type=" 
					 << (tm->ParamAt(0))->m_type << endl<< endl; 	
			}
		}
	 	#endif
	}
	;

constr_forward_name_dcl
	: J_LBRACKET J_CUSTOMMARSHAL J_RBRACKET J_TYPE scoped_names {
		#ifdef IDLDEBUG
			bout << "rule<<<constr_forward_name_dcl (automarshal)" << endl<< endl; 
	 	#endif		

		SString sn=full_scoped_name($5);

		$$=new IDLType(sn, B_WILD_TYPE);
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$5->DecStrong(NULL);
	 	#endif
		REF->FindIDLType($$, ADD);
		$$->AddAttributes(kAutoMarshal);
		IDLSymbol jsymbol($$->GetName(), $$);
	}
	| J_TYPE scoped_names {
		#ifdef IDLDEBUG
			bout << "rule<<<constr_forward_name_dcl" << endl<< endl; 
	 	#endif		

		SString sn=full_scoped_name($2);

		$$=new IDLType(sn, B_WILD_TYPE);
		#ifdef IDLDEBUG
			$$->IncStrong(NULL);
			$2->DecStrong(NULL);
	 	#endif
		REF->FindIDLType($$, ADD);
		IDLSymbol jsymbol($$->GetName(), $$);
	}
	;
%%

int yywrap()
{  
	return 1;
}
