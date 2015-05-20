%option never-interactive

%{
/* scanner for Pidgen - Palm Interface Definition Generator */
#include "InterfaceRec.h"
#include "idlstruct.h"
#include "yacc.h"
#define YY_DECL	int yylex (IDLStruct* ref)
#define MAX_IMPORT_DEPTH 12
YY_BUFFER_STATE import_stack[MAX_IMPORT_DEPTH];
int import_stack_index = 0;
int totalfiles = 0;
SVector<SString> import_filenames;
SVector<IncludeRec> jincludes;

// brief & detailed comments are collected into a single comment block
// which is associated with the next applicable item during parse
sptr<IDLCommentBlock> gCommentBlock = NULL;

void createtype();
void createtype(const char* name);
bool checkfile(const SString& filename);
void recordfile(const SString& filename);
sptr<IDLCommentBlock> get_comments();
extern FILE *yyin;

%}

Digits                  [0-9]+
Oct_Digit               [0-7]
Hex_Digit               [a-fA-F0-9]
Int_Literal				[1-9][0-9]*
Oct_Literal				0{Oct_Digit}*
Hex_Literal				(0x|0X){Hex_Digit}*
Esc_Sequence1           "\\"[ntvbrfa\\\?\'\"]
Esc_Sequence2           "\\"{Oct_Digit}{1,3}
Esc_Sequence3           "\\"(x|X){Hex_Digit}{1,2}
Esc_Sequence            ({Esc_Sequence1}|{Esc_Sequence2}|{Esc_Sequence3})
Char                    ([^\n\t\"\'\\]|{Esc_Sequence})
Char_Literal            "'"({Char}|\")"'"
String_Literal			\"({Char}|"'")*\"
Float_Literal1			{Digits}"."{Digits}?(e|E)("+"|"-")?{Digits}  
Float_Literal2			{Digits}(e|E)("+"|"-")?{Digits}
Float_Literal3          {Digits}"."{Digits}
Float_Literal4          {Digits}"."
Float_Literal5			"."{Digits} 
Float_Literal6			"."{Digits}(e|E)("+"|"-")?{Digits}  
Fixed_Literal1          {Digits}(d|D)
Fixed_Literal2          {Digits}"."(d|D)
Fixed_Literal3          "."{Digits}(d|D)
Fixed_Literal4          {Digits}"."{Digits}(d|D)

ident					[a-zA-Z_][a-zA-Z0-9_]*
filename				[a-zA-Z_][a-zA-Z0-9_]*"."[a-zA-Z_]* 

%x imp
%x comment

%%
import									BEGIN(imp);
<imp>[ \t]*
<imp>[^ \t\n]+ {

	if (import_stack_index>=MAX_IMPORT_DEPTH) {	
		fprintf(stderr, "imports nested too deeply!!");
		_exit(1);
	}
	
	else {
		SVector<SString> idirs=ref->ImportDir();
		SString ifile(yytext);
		ifile.RemoveAll("<");
		ifile.RemoveAll(">");

		bool found=false;
		size_t dirnum=0;
		SString imported;

		if (idirs.CountItems()<=0) {		
			// bout << "<-------- flex --------> no import directory specified...aborting import " << ifile << endl; 
			_exit(1);
		}
		// we go thru and see which of the import directories this file is in
		while ((!found) && (dirnum<idirs.CountItems())) {	
			imported.PathSetTo(idirs.ItemAt(dirnum));
			// bout << "<-------- flex --------> imported=" << imported << endl;
			imported.PathAppend(ifile);
			// bout << "<-------- flex --------> imported=" << imported << endl;

	
			bool dejavu=checkfile(imported);
	
			if (dejavu) {	
				// bout << "<-------- flex --------> we've seen " << ifile << " before, skipping... " << endl; 
				found=true;
			}
			else {	
				FILE* opened;
				opened=fopen(imported.String(), "r");
				// bout << "<-------- flex --------> " << imported.String() << " needs to be opened " << endl; 		
				if (!opened) {	
					bout << "<-------- flex --------> " << imported << " can't be opened " << endl; 
				}
				else { 
					recordfile(imported);
					if (import_stack_index == 0) {
						// Remember to write out this include file, if we are at
						// the top level.
						jincludes.AddItem(IncludeRec(imported, get_comments()));
					}
					import_stack[import_stack_index++]=YY_CURRENT_BUFFER;	
					ref->SetParserState(true);
					yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE)); 
					found=true;
					yyin=opened;
				}		
			}
			dirnum++;
			if ((dirnum==idirs.CountItems()) && (!found)) {	
				bout << "<-------- flex --------> idirs are now == ";
				for (int s=0; s<idirs.CountItems(); s++) { 
					bout << idirs.ItemAt(s) << endl; 
				}
				bout << "<-------- flex --------> unable to import " << ifile << endl; 
				_exit(1);
			}
		}
	}
	BEGIN(INITIAL);
}


<<EOF>> {
	if (--import_stack_index<0) {	
		ref->AddIncludes(jincludes);
		jincludes.MakeEmpty();
		ref->SetEndComments(get_comments());
		import_filenames.MakeEmpty();
		yyterminate();
	}
	else {
		// As we finish scanning import text, we need to set state for the
		// continuation of importing file.  (We don't want comments at the
		// end of imported file to be associated with next element.)
		gCommentBlock = NULL; 
		yy_delete_buffer(YY_CURRENT_BUFFER);
		yy_switch_to_buffer(import_stack[import_stack_index]);
		if (import_stack_index==0) {	
			ref->SetParserState(false); 
		}
	}
}

"/*"					{
							BEGIN(comment);
							if (gCommentBlock == NULL) {
								gCommentBlock = new IDLCommentBlock();
							}
							gCommentBlock->AddComment(SString(yytext));
						}
<comment>[^*\r\n]*		|
<comment>"*"+[^*/\r\n]*	|
<comment>\r				|
<comment>\n				{
							gCommentBlock->AppendToComment(SString(yytext));
						}				
<comment>"*"+"/"		{
							gCommentBlock->AppendToComment(SString(yytext));
							BEGIN(INITIAL);
						}

{Float_Literal1}	|
{Float_Literal2}	|
{Float_Literal3}	|
{Float_Literal4}	|
{Float_Literal5}	|
{Float_Literal6}	{	
						SString v = SString(yytext);
						IDLNameType* ptr=new IDLNameType();
						#ifdef IDLDEBUG
							ptr->IncStrong(NULL);					 		
						#endif
						ptr->m_id=v;
							
						yylval._symbol=ptr;
						return J_FLOATPTLITERAL;
					}
{Fixed_Literal1}	|
{Fixed_Literal2}	|
{Fixed_Literal3}	|
{Fixed_Literal4}	{	
						SString v = SString(yytext);
						IDLNameType* ptr=new IDLNameType();
						#ifdef IDLDEBUG
							ptr->IncStrong(NULL);					 		
						#endif
						ptr->m_id=v;
						
						yylval._symbol=ptr;
						return J_FIXEDPTLITERAL;
					}
{Int_Literal}		|
{Oct_Literal}		|
{Hex_Literal}		{	
						SString v = SString(yytext);
						IDLNameType* ptr=new IDLNameType();
						#ifdef IDLDEBUG
							ptr->IncStrong(NULL);					 		
						#endif
						ptr->m_id=v;
						
						yylval._symbol=ptr;
						return J_INTLITERAL;
					}

{Char_Literal}		{	
						SString v = SString(yytext);
						IDLNameType* ptr=new IDLNameType();
						#ifdef IDLDEBUG
							ptr->IncStrong(NULL);					 		
						#endif
						ptr->m_id=v;
						
						yylval._symbol=ptr;
						return J_CHARLITERAL;
					}
{String_Literal}	{	
						SString v = SString(yytext);
						IDLNameType* ptr=new IDLNameType();
						#ifdef IDLDEBUG
							ptr->IncStrong(NULL);					 		
						#endif
						ptr->m_id=v;
						
						yylval._symbol=ptr;
						return J_STRINGLITERAL;
					}

[ \t]				;
[\r\n]				;
"//"[^\r\n]*		{
						if (gCommentBlock == NULL) {
							gCommentBlock = new IDLCommentBlock();
						}
						gCommentBlock->AddComment(SString(yytext));
					}

"#"					return J_POUND;
";"					return J_SEMICOLON;		
"{"					return J_LCURLY;
"}"					return J_RCURLY;		
":"					return J_COLON;		
","					return J_COMMA;		
"="					return J_EQ;		
"+"					return J_PLUS;		
"-"					return J_MINUS;		
"("					return J_LPAREN;		
")"					return J_RPAREN;		
"<"					return J_LT;
">"					return J_GT;
"["					return J_LBRACKET;
"]"					return J_RBRACKET;
"'"					return J_QUOTE;
"|"					return J_VERTICALLINE;
"^"					return J_CIRCUMFLEX;
"&"					return J_AMPERSAND;
"*"					return J_ASTERISK;
"/"					return J_SOLIDUS;
"%"					return J_PERCENT;
"~"					return J_TILDE;
"::"				return J_DCOLON;				

abstract			return J_ABSTRACT;
any					return J_ANY;
attribute			return J_ATTRIBUTE;
case				return J_CASE;
headerdir			return J_HEADERDIR;

void				{ createtype(); return J_VOID; }	
SValue				{ createtype(); return J_SVALUE; }
SString				{ createtype(); return J_STRING; }
SMessage			{ createtype(); return J_SMESSAGE; }

nsecs_t				{ createtype(); return J_NSST; }
bigtime_t			{ createtype(); return J_BIGT; }
off_t				{ createtype(); return J_OFFT; }
size_t				{ createtype(); return J_SIZT; }  
ssize_t				{ createtype(); return J_SSZT; }
status_t			{ createtype(); return J_STST; }		

bool				{ createtype(); return J_BOOLEAN; }
char				{ createtype(); return J_CHAR; }
double				{ createtype(); return J_DOUBLE; }
float				{ createtype(); return J_FLOAT; }
long				{ createtype(); return J_LONG; }
short				{ createtype(); return J_SHORT; }
int8_t				{ createtype(); return J_INT8; }
int16_t				{ createtype(); return J_SHORT; }
int32_t				{ createtype(); return J_LONG; }
int64_t				{ createtype(); return J_INT64; }

uint8_t				{ createtype(); return J_UINT8; }
uint16_t			{ createtype(); return J_UINT16; }
uint32_t			{ createtype(); return J_UINT32; }
uint64_t			{ createtype(); return J_UINT64; }
wchar32_t			{ createtype(); return J_WCHAR32; }


const					return J_CONST;
custommarshal			return J_CUSTOMMARSHAL;
optional				return J_OPTIONAL;
custom					return J_CUSTOM;
default					return J_DEFAULT;
exception				return J_EXCEPTION;
enum					{
							// since enums can be un-named, we have to grab
							// the comments when we see enum rather than waiting
							// for the identifier
							IDLNameType* ptr=new IDLNameType(SString("enum"), NULL, get_comments());
							#ifdef IDLDEBUG
								ptr->IncStrong(NULL);					 		
							#endif
							yylval._symbol=ptr;
							return J_ENUM;
						}
factory					return J_FACTORY;
FALSE					return J_FALSE;
fixed					return J_FIXED;
in						return J_IN;
inout					return J_INOUT;
include					return J_INCLUDE;
interface				return J_INTERFACE;
methods					return J_METHODS;
events					return J_EVENTS;
local					return J_LOCAL;
module					return J_MODULE;
native					return J_NATIVE;
Object					return J_OBJECT;
octet					return J_OCTET; 
oneway					return J_ONEWAY;
out						return J_OUT;
private					return J_PRIVATE;
properties				return J_PROPERTIES;
public					return J_PUBLIC;
raises					return J_RAISES;
readonly				return J_READONLY;
reserved				return J_RESERVED;
sequence				return J_SEQUENCE;
struct					return J_STRUCT;
supports				return J_SUPPORTS;
switch					return J_SWITCH;
TRUE					return J_TRUE;
truncatable				return J_TRUNCATABLE;
typedef					return J_TYPEDEF;
type					return J_TYPE;
unsigned				return J_UNSIGNED;
union					return J_UNION;
ValueBase				return J_VALUEBASE;
valuetype				return J_VALUETYPE;
weak					return J_WEAK;
wstring					return J_WSTRING;
namespace				return J_NAMESPACE;
using					return J_USING;
				

{ident}				{	
						SString id = SString(yytext);
						IDLNameType* ptr=new IDLNameType(SString(yytext), NULL, get_comments());
						#ifdef IDLDEBUG
							ptr->IncStrong(NULL);					 		
						#endif
						yylval._symbol=ptr;
						return J_IDENTIFIER;
					}

<*>.|\\n			fprintf(stderr, "Warning, unmatched input during scan: [%s]\n", yytext);

%%

int yywrap(void)
{ 
	return 1;
}

void createtype()
{
	createtype(yytext);
}

void createtype(const char* name)
{
		IDLType* obj=new IDLType(SString(name));
		#ifdef IDLDEBUG
			obj->IncStrong(NULL);					 		
		#endif
		yylval._type=obj;
}

// I looked at augmenting the grammar to support comments, but this required
// many changes to the grammer, and didn't provide much benefit.  Rather than
// doing that, I completely ignore comments in the grammer, and go retrieve
// them at appropriate places.  This is handled by gathering the comments
// when seen during the scan into the gCommentBlock, and then associating
// the comment block with the next identifier

sptr<IDLCommentBlock> 
get_comments()
{
	// adopt the current comment block (if any)
	// setting the global comment block to null will
	// then force a new one to be created at the next
	// comment seen during scan
	
	sptr<IDLCommentBlock> current = gCommentBlock;
	gCommentBlock = NULL;
	return current;
}

void
pushback_comments(sptr<IDLCommentBlock> comments)
{
	gCommentBlock = comments;
}

bool checkfile(const SString& filename)
{
		bool exists=false;

		//bout << "<-------- flex checkfile --------> totalfiles=" << totalfiles << endl;
		for (int i=0; i<totalfiles; i++)
		{	
			//bout << "<-------- flex checkfile --------> import_filenames[" << i << "]=" << import_filenames.ItemAt(i) << endl;
			if (import_filenames.ItemAt(i)==filename)
			{	exists=true; 
			}		
		}		
	
		//bout << "<-------- flex checkfile --------> check for " << filename << " is " << exists << endl;
		return exists;
}

void recordfile(const SString& filename)
{
	import_filenames.AddItem(filename);
	totalfiles++;	
	//bout << "<-------- flex recordfile --------> totalfiles = " << totalfiles << endl;

}

