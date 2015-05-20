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

#include "TypeBank.h"
#include "idlstruct.h"
#include "InterfaceRec.h"
#include "OutputI.h"
#include "OutputCPP.h"
#include "OutputUtil.h"
#include "WsdlOutput.h"
#include "WSDL.h"

#include <support/atomic.h>

#if TARGET_HOST != TARGET_HOST_WIN32
#include <getopt.h>
#else
#include "getopt.h"
#endif

#include <ctype.h>

#define PRINT_PATHS(x) // x

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::storage; // BFile only
#endif

static SVector<SString> IdlFiles;
static SVector<SString> WsdlFiles;
static SVector<InterfaceRec> ParseTree;
static SKeyedVector<SString, StackNode> SymbolTable;
static SKeyedVector<SString, sptr<IDLType> > TypeBank;
static SKeyedVector<SString, InterfaceRec*> ifnamevector;

const SKeyedVector<SString, sptr<IDLType> >& getTypeBank()
{
	return TypeBank;
}

SVector<SString> cppn;
static InterfaceRec gIBinderInterface(SString("Binder"), SString("org.openbinder.support.IBinder"), cppn, NULL, IMP); // default binder

#define READ_BUFFER_SIZE 32768

extern FILE *yyin; // flex input
bool verbose = false;

// Set to non-zero if there was a parse error.
static int parseError = 0;

int
usage()
{	
	bout << " usage: pidgen [flags] foo.idl " << endl 
			<< " if more than 1 file specified, files will be run in sequence" << endl 
			<< " with the same set of flags" 
			<< endl << endl << " flags: " << endl << endl;
	bout << "-- import path | -I path " << endl << " adds path to the list of import directories " << endl;
	bout << "-- output-dir path | -O path " << endl << " outputdir for generated files; dir for cpp files only when used with -S flag; default is directory of .idl file " << endl;
	bout << "-- output-header-dir path | -S path" << endl << "outputdir for generated header files - when used in conjunction with -O, header files go in output-header directory and cpp files to the output dir; default is directory of .idl file " << endl;
	bout << "-- base-header-dir path | -B path" << endl << "base dir of header files - when used in conjunction with -O, header files go in output-header directory and cpp files to the output dir; default is directory of .idl file " << endl;
	bout << "-- verbose | -v " << endl << " prints some information while running" << endl;
	bout << "-- help | -h " << endl << " displays this text" << endl;
	return 2;
}

status_t
readargs(int argc, char ** argv, SString& hd, SString& bd, SString& od, SVector<SString>& id)
{
	static option long_options[] = {
		{"import", 1, 0, 'I'},
		{"output-dir", 1, 0, 'O'},
		{"output-headerdir", 1, 0, 'S'},
		{"base-headerdir", 1, 0, 'B'},
		{"verbose", 1, 0, 'v'},
		{"help", 0, 0, 'h'},
		{0, 0, 0, 0}
	};

	char getopt_result;
	for (;;) {
		getopt_result=getopt_long(argc, argv, "I:O:S:B:vh", long_options, NULL);

		if (getopt_result==-1) break;
		switch (getopt_result) {
				case 'I' : {
					id.AddItem(SString(optarg));
					//bout << "we have " << id.CountItems() << " importdirs" << endl;
					break;
				}
				case 'O': {
					od=optarg;
					break;
				}
				case 'S': {
					hd=optarg;
					break;
				}
				case 'B': {
					bd=optarg;
					break;
				}
				case 'v': {
					verbose = true;
					break;
				}
				default: {
					return usage();
					break;
				}
		}
	}

	if (optind>=argc) { 
		berr << argv[0] << ": no idl file specified" << endl;	
		return usage(); 
	}
	else {
		for (int ifile=optind; ifile<argc; ifile++) {
			SString fn(argv[ifile]);

			int32_t index = fn.IFindLast(".wsdl");

			if (index >= 0)
				WsdlFiles.AddItem(fn);
			else
				IdlFiles.AddItem(fn);
		}
	}

	// if no import directory was specified then
	// use the current direcotry
	if (id.CountItems() == 0) {
		id.AddItem(od);
	}
	
	return B_OK;
}


//idl file, name of new header, name of new cpp file, specified headerdir, specified outputdir
status_t
setpath(
	SString& filebase, SString& iheader,
	SString& cppfile, SString& header, const SString& basehdir,
	SString& output, SString& prevdir)
{
	SString origbase = filebase;

	int32_t pos = -1;

	filebase = filebase.PathLeaf();
	PRINT_PATHS(bout << "Initial filebase: " << filebase << endl);
	
	// no path is given in association with the idl file
	if (output == "") {
		if (pos>=0) {
			origbase.PathGetParent(&output);
		}
		else {
			output.PathSetTo(".");
		}
	}
	if (filebase.Length()<=0) {
		return usage();
	}
		
	pos=filebase.FindLast('.');
	if (pos>=0) {
		if (0==strcmp(filebase.String()+pos, ".idl")) {
			filebase.Truncate(pos);
		}
	}
	PRINT_PATHS(bout << "truncated filebase=" << filebase << endl);

	status_t err;
	
	// if no header dir specified, default is output dir for .h
	if (header != "") {
		iheader.PathSetTo(header);
	}
	else {
		iheader.PathSetTo(output); 
	}

	iheader.PathAppend(filebase);
	iheader.Append(".h");

	cppfile.PathSetTo(output);
	cppfile.PathAppend(filebase);
	cppfile.Append(".cpp");

	PRINT_PATHS(bout << "Header : " << header << endl);
	PRINT_PATHS(bout << "I Header : " << iheader << endl);
	PRINT_PATHS(bout << "CPP File : " << cppfile << endl);

	if (basehdir == "") {
		// Try to infer the path to the header -- just assume it is
		// one level deep.
		prevdir.PathSetTo(SString(header.PathLeaf()));
	} else {
		// Strip 'basehdir' off the front of the full header path.
		int32_t p = header.FindFirst(basehdir);
		if (p == 0) {
			header.CopyInto(prevdir, basehdir.Length(), header.Length()-basehdir.Length());
			if (prevdir.FindFirst(SString::PathDelimiter()) == 0) {
				prevdir.Remove(0, strlen(SString::PathDelimiter()));
			}
		}
	}

	PRINT_PATHS(bout << "prevdir=" << prevdir << endl);
	prevdir.PathAppend(SString(iheader.PathLeaf()));
	PRINT_PATHS(bout << "printableIHeader=" << prevdir << endl);

	return B_OK;
}

InterfaceRec*
FindInterface(const SString &name)
{	
	if (name=="IBinder" || name=="palmos::support::IBinder") {
		return &gIBinderInterface;
	}
	else {
		bool present=false;
		IDLSymbol iface(name);
		ifnamevector.ValueFor(name, &present);

		if (present) {	
			return(ifnamevector.EditValueFor(name, &present));
		}
		else {
			berr << "pidgen: <---- FindInterface ----> invalid interface - check to see if foward declaration needed for " << name << endl; 
			parseError = 10;
			_exit(10);
			return NULL;
		}
	}
}

sptr<IDLType> 
FindType(const sptr<IDLType>& typeptr)
{	
	bool present=false;
	SString code=typeptr->GetName();
	TypeBank.ValueFor(code, &present);

	if (present) { 
		return TypeBank.EditValueFor(code, &present); 
	}
	else {
		berr << "pidgen: <---- FindType ----> " << code << " could not be found in TypeBank" << endl; 
		parseError = 10;
		return NULL;
	}
}

int
yyerror(char *errmsg)
{	
	fprintf(stderr, "we got an err %s\n", errmsg);
	parseError = 10;
	return 1;
}

int generate_from_idl(
	const SString& _idlFileBase, const SString& _headerdir,
	const SString& basehdir, const SString& _outputdir, SVector<SString>& importdir)
{
	if (verbose)
		bout << "IDL File: " << _idlFileBase << endl;
	// open interface 
	const char* nfn= _idlFileBase.String();
	FILE *input = fopen(nfn, "r");

	sptr<IByteOutput> byteStream;
	sptr<ITextOutput> stream;
	sptr<BFile> file;
	status_t err;
	bool system = false;

	if (!input) {
		berr << "pidgen: --- could not open " << _idlFileBase << " --- " << endl;
		return 10;
	}
	else {
		// if input was valid, create header and cppfile name
		SString idlFileBase(_idlFileBase), headerdir(_headerdir), outputdir(_outputdir);
		SString iheader, cppfile, prevdir;
		setpath(idlFileBase, iheader, cppfile, headerdir, basehdir, outputdir, prevdir);
		PRINT_PATHS(bout << "idlFileBase: " << idlFileBase << ", iheader: " << iheader
			<< ", cppfile: " << cppfile << ", headerdir: " << headerdir
			<< ", outputdir: " << outputdir << ", prevdir: " << prevdir << endl);

		initTypeBank(TypeBank);
		IDLStruct result;
		
		// pass importdir to flex if it is used
		if (importdir.CountItems()!=0) { 
			for (size_t a=0; a<importdir.CountItems(); a++)
			{	result.AddImportDir(importdir.ItemAt(a)); }
		}

		// force flex to discard any previous files
		fflush(yyin);
		yyin=input;
		yyparse((void*)&result);

		// If there was an error, abort!
		if (parseError != 0) return parseError;

		// get things back from bison - everything that is passed back is a hardcopy
		ParseTree=result.Interfaces();
		headerdir=result.Header();
		SymbolTable.SetTo(result.SymbolTable());

		// add user-defined types from bison into the TypeBank
		// First we need to add all "primitive" types, and then
		// we need to add typedefs.  If we don't do this two step
		// approach we can get to a typedef that uses a type we
		// haven't yet added to our TypeBank

		const SKeyedVector<SString, sptr<IDLType> >& UTypes = result.TypeBank();

		// Iterate and add all user defined types
		for (size_t i = 0; i < UTypes.CountItems(); i++) {
			SString userKey = UTypes.KeyAt(i);
			sptr<IDLType> userType = UTypes.ValueAt(i);
			const SString& syn = userType->GetPrimitiveName();
			if (syn.Length() == 0) {
				TypeBank.AddItem(userKey, userType);
			}
		}

		// Now iterate again, but only pick up typedefs
		for (size_t i=0; i<UTypes.CountItems(); i++) {
			// Get the real type and verify that we know about it
			SString userKey = UTypes.KeyAt(i);
			sptr<IDLType> userType = UTypes.ValueAt(i);
			const SString& syn = userType->GetPrimitiveName();
			if (syn.Length() > 0) {	
				bool exists=false;
				sptr<IDLType> baseType = TypeBank.ValueFor(syn, &exists);
				if (exists) {
					// Now add a clone of the existing type with the new name
					// That way, when that name is used, we will still know
					// how to deal with the type.
					sptr<IDLType> newType = new IDLType(baseType); 
					// If the parse put in some code modifier, we need to
					// bring that along also
					if (userType->GetCode() != B_UNDEFINED_TYPE) {
						newType->SetCode(userType->GetCode());
					}
					// For sptr (et al) types, the parse put in an iface,
					// but the base types don't specify the interface.
					// Make sure we don't leave that behind either
					newType->SetIface(userType->GetIface());
					// bout << "baseType code =" << baseType->GetCode() << " baseType name=" << baseType->GetName() << endl;
					// bout << "UTypes.KeyAt = " << userKey << endl;
					TypeBank.AddItem(userKey, newType); }
				else {
					bout << "Error - Can't find user type: " << syn << " for typedef: " << userKey << endl;
					exit(10);
				}
			}
		}

		//checktb(TypeBank);

		SVector<InterfaceRec*> ifvector;
		// store things from the parse tree into digestable formats for the output engine
		for (size_t s=0; s<ParseTree.CountItems(); s++) {
			InterfaceRec* _interface = const_cast<InterfaceRec*>(&ParseTree[s]);

			// Trim out forward declarations if an import or definition has
			// also been found.
			if(_interface->Declaration()==FWD) {
				// bout << "FWD: " << _interface->FullInterfaceName() << endl;
				bool skip = false;
				for (size_t t=0; !skip && t<ifvector.CountItems(); t++) {
					if (ifvector[t]->FullInterfaceName() == _interface->FullInterfaceName()) skip = true;
				}
				if (skip) continue;
			} 
			else {
				// bout << "IMP/DCL: " << _interface->FullInterfaceName() << endl;
				for (size_t t=0; t<ifvector.CountItems(); t++) {
					if (ifvector[t]->FullInterfaceName() == _interface->FullInterfaceName()
							&& ifvector[t]->Declaration()==FWD) {
						// we currently have this interface as a forward declaration, 
						// remove that version so we can add implementation version
						ifvector.RemoveItemsAt(t);
						ifnamevector.RemoveItemFor(_interface->ID());
					}
				}
			}

			ifvector.AddItem(_interface);
			ifnamevector.AddItem(_interface->ID(), _interface);
			// bout << "we just added interface = " << _interface->ID() << " as... " << ((_interface->Declaration()==FWD)?"Forward":"Implementation") << endl;
		}

		// scan through the includes to attach namespaces & check their validity  	
		SVector<IncludeRec> headers;
		
		for (size_t t=0; t<(result.Includes()).CountItems(); t++) {	
			IncludeRec includerec = (result.Includes()).ItemAt(t);
			SString includefile = includerec.File();

			#if TARGET_HOST == TARGET_HOST_WIN32
				includefile.ReplaceAll('.\\', '/');
			#endif
			//bout << "includefile=" << includefile << endl;

			includefile.ReplaceLast("/", ":");
			int s=includefile.FindLast("/");
			if (s>=0) { 
				includefile=includefile.String()+s+1; 
			}

			int32_t pos=includefile.FindLast('.');
			if (pos>=0) {
				if (0==strcmp(includefile.String()+pos, ".idl")) {
					includefile.Truncate(pos);
				}
			}

			includefile.Append(".h");
			includefile.ReplaceAll(":", "/");
			headers.AddItem(IncludeRec(includefile, includerec.Comments()));
		}

		// create interface header
		uint32_t flags =  O_WRONLY | O_CREAT | O_TRUNC;
#if TARGET_HOST == TARGET_HOST_WIN32
		flags |= O_BINARY;
#endif
		if (verbose)
			bout << "Trying to open file '" << iheader << "'" << endl;
		file=new BFile(iheader.String(), flags);
		if (!file->IsWritable()) {
			berr << "pidgen Overwrite failed - could not open " << iheader << " for writing" << endl; 
			return 10;
		}
		byteStream=new BByteStream(sptr<IStorage>(file.ptr()));
		stream=new BTextOutput(byteStream);

		// A word about include files...
		// The current .h file is specified with prevdir, which uses the leaf of the
		// -S directory (so that the result is "widget/IWidget.h" rather than "IWidget.h"
		// For other header files, if found on search paths, it uses the last directory
		// in the path, so that once again, the result is a "widget/IWidget.h" format.
		// This should probably be controlled with a -include_parent_dir to indicate
		// what you want there.  .idl files outside the normal build directory structures
		// might not fit this pattern.
		// Also, there should be a -system_include or -local_include type option which would
		// dictate the #include using <> or "".
		err=WriteIHeader(stream, ifvector, prevdir, headers, result.BeginComments(), result.EndComments(), system);

		stream=NULL;
		byteStream=NULL;
		file=NULL;
		unlink(cppfile.String());
		
		// create cppfile
		file=new BFile(cppfile.String(), flags);
		if (!file->IsWritable()) {
			berr << "pidgen: Overwrite failed - could not open " << cppfile << " for writing" << endl; 
			return 10;
		}
		byteStream=new BByteStream(sptr<IStorage>(file.ptr()));
		stream=new BTextOutput(byteStream);

		err=WriteCPP(stream, ifvector, idlFileBase, prevdir, system);

		stream=NULL;
		byteStream=NULL;
		file=NULL;

		// clean up before we start on the next file
		ParseTree.MakeEmpty();	
		cleanTypeBank(TypeBank);
		SymbolTable.MakeEmpty();
		headers.MakeEmpty();
		ifvector.MakeEmpty();
		ifnamevector.MakeEmpty();
		importdir.MakeEmpty();

		fflush(yyin);
		fclose(yyin);
		yyin = NULL;
	}

	return parseError;
}

int generate_from_wsdl(
	const SString& wsdlFile, const SString& headerdir,
	const SString& basehdir, const SString& outputdir, SVector<SString>& importdir)
{
	if (verbose)
		bout << endl << "WSDL File : " << wsdlFile << endl;

	const char* name = wsdlFile.String();
	int fd = open(name, 0, O_RDONLY);

	sptr<BFile> file;
	sptr<IByteOutput> byteStream;
	sptr<ITextOutput> stream;

	if (!fd) {
		berr << "pidgen: --- could not open " << wsdlFile << " --- " << endl;
		return 10;
	}

	char* buf = (char*)malloc(READ_BUFFER_SIZE);
	size_t howmany = 0;
	SString buffer;
	do {
		memset(buf, 0, READ_BUFFER_SIZE);
		howmany = read(fd, buf, READ_BUFFER_SIZE);
		buffer.Append(buf, howmany);
	} while (howmany == READ_BUFFER_SIZE);

	free(buf);
	
	sptr<BWsdl> wsdl = new BWsdl();
	sptr<BWsdlCreator> creator = new BWsdlCreator();

	creator->Parse(buffer, wsdl);
	if (verbose)
		bout << "--------------------------------------------" << endl;

	// create the WsdlClass.
	sptr<WsdlClass> obj = NULL;
	create_wsdl_class(wsdl, obj);
	
	SString output;
	
	SString typesInterface;
	typesInterface.Append(wsdl->Name());
	typesInterface.Append("Types.idl");
	
	output.PathSetTo(outputdir);
	output.PathAppend(typesInterface);
//	bout << "Creating " << output << endl;
	uint32_t flags =  O_WRONLY | O_CREAT | O_TRUNC;
#if TARGET_HOST == TARGET_HOST_WIN32
		flags |= O_BINARY;
#endif
	file = new BFile(output.String(), flags);
	if (!file->IsWritable()) {
		berr << "pidgen: Overwrite failed - could not open " << output << " for writing" << endl; 
		return 10;
	}
	
	// generate the interface
	byteStream = new BByteStream(sptr<IStorage>(file.ptr()));
	stream = new BTextOutput(byteStream);
	wsdl_create_types_interface(obj, stream);

	SString interfaceName("I");
	interfaceName.Append(wsdl->Name());
	interfaceName.Append(".idl");
	output.PathSetTo(outputdir);
	output.PathAppend(interfaceName);
//	bout << "Creating " << output << endl;
	file = new BFile(output.String(), flags);
	if (!file->IsWritable()) {
		berr << "pidgen: Overwrite failed - could not open " << output << " for writing" << endl; 
		return 10;
	}
	
	// generate the interface
	byteStream = new BByteStream(sptr<IStorage>(file.ptr()));
	stream = new BTextOutput(byteStream);
	wsdl_create_interface(obj, stream, typesInterface);
	
	// create the output file
	SString headername = wsdl->Name();
	headername.Append(".h");
	output.PathSetTo(outputdir);
	output.PathAppend(headername);
//	bout << "Creating " << output << endl;
	file = new BFile(output.String(), flags);
	if (!file->IsWritable()) {
		berr << "pidgen: Overwrite failed - could not open " << output << " for writing" << endl; 
		return 10;
	}
	
	// generate the header
	byteStream = new BByteStream(sptr<IStorage>(file.ptr()));
	stream = new BTextOutput(byteStream);
	wsdl_create_header(obj, stream, headername);

	SString typesname = wsdl->Name();
	typesname.Append("Types.h");
	output.PathSetTo(outputdir);
	output.PathAppend(typesname);
//	bout << "Creating " << output << endl;
	file = new BFile(output.String(), flags);
	if (!file->IsWritable()) {
		berr << "pidgen: Overwrite failed - could not open " << output << " for writing" << endl; 
		return 10;
	}	

	// generate the types header
	byteStream = new BByteStream(sptr<IStorage>(file.ptr()));
	stream = new BTextOutput(byteStream);
	wsdl_create_types_header(obj, stream, typesname);
	
	SString cppname = wsdl->Name();
	cppname.Append(".cpp");
	output.PathSetTo(outputdir);
	output.PathAppend(cppname);
//	bout << "Creating " << output << endl;
	file = new BFile(output.String(), flags);
	if (!file->IsWritable()) {
		berr << "pidgen: Overwrite failed - could not open " << output << " for writing" << endl; 
		return 10;
	}
	
	// generate the cpp
	byteStream = new BByteStream(sptr<IStorage>(file.ptr()));
	stream = new BTextOutput(byteStream);
	wsdl_create_cpp(obj, stream, cppname, headername);


	SString typescppName = wsdl->Name();
	typescppName.Append("Types.cpp");
	output.PathSetTo(outputdir);
	output.PathAppend(typescppName);
//	bout << "Creating " << output << endl;
	file = new BFile(output.String(), flags);
	if (!file->IsWritable()) {
		berr << "pidgen: Overwrite failed - could not open " << output << " for writing" << endl; 
		return 10;
	}
	
	// generate the cpp
	byteStream = new BByteStream(sptr<IStorage>(file.ptr()));
	stream = new BTextOutput(byteStream);
	wsdl_create_types_cpp(obj, stream, typescppName, headername);

	// now process the idl file
	output.PathSetTo(outputdir);
	output.PathAppend(interfaceName);
	if (verbose)
		bout << "--------------------------------------------" << endl;
	return generate_from_idl(output, headerdir, basehdir, outputdir, importdir);
}

int 
main(int argc, char ** argv)
{
	// headerdir stores the header directory specified when we want to separate the .h and .cpp output
	SString outputdir;
	SString basehdir;
	SString headerdir;
	SVector<SString> importdir;
	
	gIBinderInterface.AddCppNamespace(SString("palmos::support"));
	readargs(argc, argv, headerdir, basehdir, outputdir, importdir);
	outputdir.PathNormalize();
	basehdir.PathNormalize();
	headerdir.PathNormalize();

	size_t size = IdlFiles.CountItems();
	for (size_t i = 0; i < size; i++) {
		SString idl = IdlFiles.ItemAt(i);
		idl.PathNormalize();
		int result = generate_from_idl(idl, headerdir, basehdir, outputdir, importdir);
		if (result != 0) return result;
	}

	size = WsdlFiles.CountItems();
	for (size_t i = 0; i < size; i++) {
		SString wsdl = WsdlFiles.ItemAt(i);
		wsdl.PathNormalize();
		int result = generate_from_wsdl(wsdl, headerdir, basehdir, outputdir, importdir);
		if (result != 0) return result;
	}
	
	return 0;
}
