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

#include <support/CallStack.h>

#include <support/SupportDefs.h>
#include <support/Autolock.h>
#include <support/Debug.h>
#include <support/Locker.h>
#include <support/ITextStream.h>
#include <support/String.h>
#include <support/Value.h>

#include <support_p/WindowsCompatibility.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if TARGET_HOST == TARGET_HOST_PALMOS
#include <DebugMgr.h>
#endif

#if defined(SUPPORTS_CALL_STACK) && SUPPORTS_CALL_STACK

#define LOG_OUTPUT_QUANTUM	5000
#define REPORT_TOP_COUNT	5

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif // _SUPPORTS_NAMESPACE

#if _SUPPORTS_NAMESPACE
namespace Priv {
#endif // _SUPPORTS_NAMESPACE

struct symbol 
{
	uint32_t	addr;
	uint32_t	name;
};

const char* lookup_symbol(uint32_t addr, uint32_t *offset, char* name, size_t bufSize);

#if _SUPPORTS_NAMESPACE
} // namespace Priv
#endif // _SUPPORTS_NAMESPACE

#if _SUPPORTS_NAMESPACE
using namespace Priv;
#endif // _SUPPORTS_NAMESPACE

SCallStack::SCallStack()
{
	for (int32_t i=0;i<B_CALLSTACK_DEPTH;i++) m_caller[i] = 0;
};

SCallStack::SCallStack(const SCallStack& from)
{
	for (int32_t i=0;i<B_CALLSTACK_DEPTH;i++) m_caller[i] = from.m_caller[i];
}

SCallStack::SCallStack(const SValue &value)
{
	int32_t i;
	SValue v;
	for (i=0; (i<B_CALLSTACK_DEPTH) && (v=value[SValue::Int32(i)]).IsDefined(); i++) {
		m_caller[i] = v.AsInt32();
	}
	for (;i<B_CALLSTACK_DEPTH;i++) m_caller[i] = 0;
}

SCallStack::~SCallStack()
{
}

SValue
SCallStack::AsValue() const
{
	SValue rv;
	for (int32_t i=0;i<B_CALLSTACK_DEPTH;i++) rv.JoinItem(SValue::Int32(i), SValue::Int32(m_caller[i]));
	return rv;
}


#if TARGET_HOST == TARGET_HOST_BEOS

#if defined(__POWERPC__) && defined(__MWERKS__)
static __asm unsigned long* get_caller_frame();

static __asm unsigned long* get_caller_frame ()
{
	lwz     r3, 0 (r1)
	blr
}
#endif	// __POWERPC__ && __MWERKS__

static inline bool bogus_image_addr(size_t addr)
{
	return ((addr) < 0x80000000 || (addr) >= 0xfc000000);
}

static inline bool bogus_stack_addr(size_t addr)
{
	return ((addr) < 0xfc000000);
}

#endif

#if TARGET_HOST == TARGET_HOST_LINUX
#include <cxxabi.h>
extern "C" int backtrace(void **array, int size);
int32_t linux_gcc_demangler(const char *mangled_name, char *unmangled_name, size_t buffersize)
{
	size_t out_len = 0;
	int status = 0;
	char *demangled = abi::__cxa_demangle(mangled_name, 0, &out_len, &status);
	if (status == 0) {
		// OK
		if (out_len < buffersize) memcpy(unmangled_name, demangled, out_len);
		else out_len = 0;
		free(demangled);
	} else {
		out_len = 0;
	}
	return out_len;
}
#endif

#if TARGET_HOST != TARGET_HOST_PALMOS

intptr_t SCallStack::GetCallerAddress(int32_t level) const
{
#if TARGET_HOST == TARGET_HOST_BEOS

#if defined(__POWERPC__) && defined (__MWERKS__)
	unsigned long *cf = get_caller_frame();
	unsigned long ret = 0;
	
	level += 1;
	
	while (cf && --level > 0) {
		ret = cf[2];
		if (ret < 0x80000000) break;
		cf = (unsigned long *)*cf;
	}

	return ret;
#else
	uint32_t fp = 0, nfp, ret=0;

	level += 2;
	
	fp = (uint32_t)get_stack_frame();
	if (bogus_stack_addr(fp))
		return 0;
	nfp = *(ulong *)fp;
	while (nfp && --level > 0) {
		if (bogus_stack_addr(fp))
			return 0;
		nfp = *(ulong *)fp;
		ret = *(ulong *)(fp + 4);
		if (bogus_image_addr(ret))
			return 0;
		fp = nfp;
	}

	return ret;
#endif

#elif TARGET_HOST == TARGET_HOST_WIN32

	level += 2;
	
	CONTEXT context;
	memset( &context, 0, sizeof(context));
	context.ContextFlags = CONTEXT_FULL;

	if (!GetThreadContext(GetCurrentThread(), &context))
		return NULL;

	STACKFRAME stackframe; // in/out stackframe
	memset(&stackframe, 0, sizeof(stackframe));
	stackframe.AddrPC.Offset = context.Eip;
	stackframe.AddrPC.Mode = AddrModeFlat;
	stackframe.AddrFrame.Offset = context.Ebp;
	stackframe.AddrFrame.Mode = AddrModeFlat;

	bool success;
	do
	{
		success = StackWalk(IMAGE_FILE_MACHINE_I386, 
							GetCurrentProcess(), 
							GetCurrentThread(), 
							&stackframe, 
							&context, 
							NULL, 
							SymFunctionTableAccess, 
							SymGetModuleBase, NULL);
	
		if (!success || stackframe.AddrReturn.Offset == 0)
			break;

	} while (--level > 0);

	//printf("level %d\tcaller address = 0x%x 0x%x\n", level, stackframe.AddrPC.Offset, stackframe.AddrReturn.Offset);

	if (!success || level > 1)
		return NULL;

	return stackframe.AddrPC.Offset;
#else
//	#error - Unknown processor/complier combination -
//	get_caller_frame();
	return 0;
#endif
}

#endif

void SCallStack::Update(int32_t ignoreDepth, int32_t maxDepth)
{
	if (maxDepth > B_CALLSTACK_DEPTH) maxDepth = B_CALLSTACK_DEPTH;
#if TARGET_HOST == TARGET_HOST_PALMOS
	int32_t count = DbgWalkStack(ignoreDepth+1, maxDepth, (void**)&m_caller[0]);
	while (count < B_CALLSTACK_DEPTH) m_caller[count++] = NULL;
#elif TARGET_HOST == TARGET_HOST_LINUX
	maxDepth += ignoreDepth;
	if (maxDepth > B_CALLSTACK_DEPTH) maxDepth = B_CALLSTACK_DEPTH;
	int count = backtrace((void**)&m_caller[0], maxDepth);
	if (ignoreDepth) {
		memmove(&m_caller[0], &m_caller[ignoreDepth], sizeof(m_caller[0])*count);
		count -= ignoreDepth;
	}
	while (count < B_CALLSTACK_DEPTH) m_caller[count++] = 0;
#else
	for (int32_t i = 1; i <= maxDepth; i++) {
		m_caller[i - 1] = GetCallerAddress(i+ignoreDepth);
	}
#endif
}

intptr_t SCallStack::AddressAt(int32_t level) const
{
	if (level < 0 || level >= B_CALLSTACK_DEPTH) return 0;
	return m_caller[level];
}

void SCallStack::SPrint(char *buffer) const
{
	buffer[0] = 0;
	for (int32_t i = 0; i < B_CALLSTACK_DEPTH; i++) {
		if (!m_caller[i]) break;
		sprintf(buffer, " 0x%08x", m_caller[i]);
		buffer += strlen(buffer);
	}
}

void SCallStack::Print(const sptr<ITextOutput>& io) const
{
	for (int32_t i = 0; i < B_CALLSTACK_DEPTH; i++) {
		if (!m_caller[i]) break;
		io << " " << (void*)m_caller[i];
	}
}

void SCallStack::LongPrint(const sptr<ITextOutput>& io, b_demangle_func demangler) const
{
	char namebuf[1024];
	char tmp[256];
	char tmp1[32];
	char tmp2[32];
	uint32_t offs;

#if TARGET_HOST == TARGET_HOST_LINUX
	if (!demangler) demangler = linux_gcc_demangler;
#endif

	for (int32_t i = 0; i < B_CALLSTACK_DEPTH; i++) {
		if (!m_caller[i]) break;
		const char* name = lookup_symbol(m_caller[i], &offs, namebuf, sizeof(namebuf));
		if (!name) continue;
		if (demangler && (*demangler)(name, tmp, 256) != 0) {
			name = tmp;
		}
				
		sprintf(tmp1, "0x%08x: <", m_caller[i]);
		sprintf(tmp2, ">+0x%08x", offs);
		
		io << tmp1 << name << tmp2 << endl;
	}
}

SCallStack &SCallStack::operator=(const SCallStack& from)
{
	for (int32_t i=0;i<B_CALLSTACK_DEPTH;i++) m_caller[i] = from.m_caller[i];
	return *this;
}

bool SCallStack::operator==(const SCallStack& s) const
{
	for (int32_t i=0;i<B_CALLSTACK_DEPTH;i++) if (m_caller[i] != s.m_caller[i]) return false;
	return true;
}

int32_t SCallStack::Compare(const SCallStack& s) const
{
	for (int32_t i=0;i<B_CALLSTACK_DEPTH;i++) {
		if (m_caller[i] != s.m_caller[i]) return m_caller[i] < s.m_caller[i] ? -1 : 1;
		if (m_caller[i] == 0) return 0;
	}
	return 0;
}

SCallTreeNode::SCallTreeNode()
{
}

SCallTreeNode::~SCallTreeNode()
{
	PruneNode();
};

void SCallTreeNode::PruneNode()
{
	for (uint32_t i=0;i<branches.CountItems();i++)
		delete branches.ItemAt(i);
	branches.MakeEmpty();
};

void SCallTreeNode::ShortReport(const sptr<ITextOutput>& io)
{
	char tmp[32];
	
	if (!parent) return;
	parent->ShortReport(io);
	
	if (parent->parent)
		sprintf(tmp, ", %08x", addr);
	else
		sprintf(tmp, "%08x", addr);
		
	io << tmp;
};

void SCallTreeNode::LongReport(const sptr<ITextOutput>& io, b_demangle_func demangler,
							   char *buffer, int32_t bufferSize)
{
	char namebuf[1024];
	char tmp1[32];
	char tmp2[32];
	const char* name;
	
	uint32_t offs;
	if (!parent) return;

#if TARGET_HOST == TARGET_HOST_LINUX
	if (!demangler) demangler = linux_gcc_demangler;
#endif

	parent->LongReport(io, demangler, buffer, bufferSize);
	name = lookup_symbol(addr, &offs, namebuf, sizeof(namebuf));
	if (!name) name = "";
	if (demangler && buffer && (*demangler)(name, buffer, bufferSize) != 0) {
		name = buffer;
	}
	
	sprintf(tmp1, "  0x%08x: <", addr);
	sprintf(tmp2, ">+0x%08x", offs);
	
	io << tmp1 << name << tmp2 << endl;
};

SCallTree::SCallTree(const char* /*name*/)
{
	addr = 0;
	count = 0;
	higher = lower = highest = lowest = parent = NULL;
};

SCallTree::~SCallTree()
{
	Prune();
};

void SCallTree::Prune()
{
	PruneNode();
	highest = lowest = higher = lower = NULL;
	count = 0;
};

void SCallTree::Report(const sptr<ITextOutput>& io, int32_t rcount, bool longReport)
{
	SCallTreeNode *n = highest;
	io << count << " total hits, reporting top " << rcount << endl;
	io << "-------------------------------------------------" << endl;
	while (n && rcount--) {
		if (longReport) {
			io << n->count << " hits-------------------------------" << endl;
			n->LongReport(io);
		} else {
			io << n->count << " hits --> ";
			n->ShortReport(io);
			io << endl;
		};
		n = n->lower;
	};
};

void SCallTree::AddToTree(SCallStack *stack, const sptr<ITextOutput>& io)
{
	SCallTreeNode *n,*next,*replace;
	uint32_t i;
	int32_t index = 0;

	if (!stack->AddressAt(0)) return;

	n = this;
	while (stack->AddressAt(index)) {
		for (i=0;i<branches.CountItems();i++) {
			next = branches.ItemAt(i);
			if (next->addr == stack->AddressAt(index)) goto gotIt;
		};
		next = new SCallTreeNode;
		next->addr = stack->AddressAt(index);
		next->higher = NULL;
		next->lower = NULL;
		next->count = 0;
		next->parent = n;
		branches.AddItem(next);
		gotIt:
		n = next;
		index++;
		if (index == B_CALLSTACK_DEPTH) break;
	};
	if (n->count == 0) {
		n->higher = lowest;
		if (lowest) lowest->lower = n;
		else highest = n;
		lowest = n;
	};
	count++;
	n->count++;
	while (n->higher && (n->count > n->higher->count)) {
		replace = n->higher;
		replace->lower = n->lower;
		if (replace->lower == NULL) lowest = replace;
		else replace->lower->higher = replace;
		n->lower = replace;
		n->higher = replace->higher;
		if (n->higher == NULL) highest = n;
		else n->higher->lower = n;
		replace->higher = n;
	};
	
	if (!(count % LOG_OUTPUT_QUANTUM)) {
		Report(io,REPORT_TOP_COUNT,true);
	};
};

SStackCounter::SStackCounter()
	:m_lock("SStackCounter"),
	 m_data(),
	 m_totalCount(0)
{
}

SStackCounter::~SStackCounter()
{
}

void SStackCounter::Update(int32_t ignoreDepth, int32_t maxDepth)
{
	SCallStack stack;
	stack.Update(ignoreDepth+1, maxDepth);
	
	SAutolock _l(m_lock.Lock());
	
	bool found;
	stack_info &info = m_data.EditValueFor(stack, &found);
	if (found) {
		info.count++;
	} else {
		stack_info i;
		i.count = 1;
		m_data.AddItem(stack, i);
	}
	m_totalCount++;
}

void SStackCounter::Reset()
{
	SAutolock _l(m_lock.Lock());
	
	m_data.MakeEmpty();
	m_totalCount = 0;
}

struct stack_count_info
{
	size_t count;
	SCallStack stack;
};

inline bool operator > (const stack_count_info &l, const stack_count_info &r)
{
	return l.count > r.count;
}

void SStackCounter::Print(const sptr<ITextOutput>& io, size_t maxItems) const
{
	SAutolock _l(m_lock.Lock());


	SVector<stack_count_info> sorted;
	size_t i, j, count = m_data.CountItems();
	for (i=0; i<count; i++) {
		bool added = false;
		stack_count_info info;
		info.count = m_data.ValueAt(i).count;
		info.stack = m_data.KeyAt(i);
		for (j=0; j<i; j++) {
			if (info > sorted[j]) {
				sorted.AddItemAt(info, j);
				added = true;
				break;
			}
		}
		if (!added) {
			sorted.AddItem(info);
		}
	}
	if (count > maxItems) count = maxItems;
	for (i=0; i<count; i++) {
		io << "Count: " << sorted[i].count << " ";
		sorted[i].stack.Print(io);
		io << endl;
	}
}

void SStackCounter::LongPrint(const sptr<ITextOutput>& io, size_t maxItems, b_demangle_func demangler) const
{
	SAutolock _l(m_lock.Lock());

#if TARGET_HOST == TARGET_HOST_LINUX
	if (!demangler) demangler = linux_gcc_demangler;
#endif

	SVector<stack_count_info> sorted;
	size_t i, j, count = m_data.CountItems();
	for (i=0; i<count; i++) {
		bool added = false;
		stack_count_info info;
		info.count = m_data.ValueAt(i).count;
		info.stack = m_data.KeyAt(i);
		for (j=0; j<i; j++) {
			if (info > sorted[j]) {
				sorted.AddItemAt(info, j);
				added = true;
				break;
			}
		}
		if (!added) {
			sorted.AddItem(info);
		}
	}
	if (count > maxItems) count = maxItems;
	for (i=0; i<count; i++) {
		io << "Count: " << sorted[i].count << endl;
		sorted[i].stack.LongPrint(io, demangler);
		io << endl;
	}
}

size_t SStackCounter::TotalCount() const
{
	// if we locked, value might be incorrect immediately, so just reuturn something
	return m_totalCount;
}


#if _SUPPORTS_NAMESPACE
namespace Priv {
#endif

#if TARGET_HOST == TARGET_HOST_PALMOS

const char* lookup_symbol(uint32_t addr, uint32_t *offset, char* name, size_t bufSize)
{
	if (DbgLookupSymbol((const void*)addr, (int32_t)bufSize, name, (void**)offset) == errNone){
		return name;
	}
	return "";
}

#elif TARGET_HOST == TARGET_HOST_LINUX
//#define _GNU_SOURCE
#include <dlfcn.h>
const char *lookup_symbol(uint32_t addr, uint32_t *offset, char* name, size_t bufSize)
{
	Dl_info info;

	if (dladdr((void*)addr, &info)) {
		*offset = (uint32_t)info.dli_saddr;
		return info.dli_sname;
	}
	return 0;
}

#else

static SLocker			symbolAccess;
static char *			symbolNames = NULL;
static int32_t			symbolNamesSize = 0;
static int32_t			symbolNamesPtr = 0;
static symbol*			symbolTable = NULL;
static int32_t			symbolTableSize = 0;
static int32_t			symbolTableCount = 0;
static int32_t			symbolWindowsInit = 0;

static void free_symbol_memory()
{
	if (symbolTable) free(symbolTable);
	symbolTable = NULL;
	symbolNamesSize = symbolNamesPtr = 0;
	if (symbolNames) free(symbolNames);
	symbolNames = NULL;
	symbolTableSize = symbolTableCount = 0;
}

class SymbolTableCleanup
{
public:
	~SymbolTableCleanup() 
	{
		free_symbol_memory();
	#if TARGET_HOST == TARGET_HOST_WIN32
		SymCleanup(GetCurrentProcess());
	#endif
	}
};

static SymbolTableCleanup symbolTableCleanup;

static int symbol_cmp(const void *p1, const void *p2)
{
	const symbol *sym1 = (const symbol*)p1;
	const symbol *sym2 = (const symbol*)p2;
	return (sym1->addr == sym2->addr)?0:
			((sym1->addr > sym2->addr)?1:-1);
}

bool load_symbols()
{
#if TARGET_HOST == TARGET_HOST_BEOS
	if (symbolNames != NULL) return true;

	SAutolock _l(symbolAccess.Lock());
	if (symbolNames != NULL) return true;

	//_sPrintf("*** Loading application symbols...\n");

	image_info info;
	int32_t cookie = 0;
	
	while (get_next_image_info(0, &cookie, &info) == B_OK) {
		const image_id id = info.id;
		int32_t n = 0;
		char name[256];
		int32_t symType = B_SYMBOL_TYPE_ANY;
		int32_t nameLen = sizeof(name);
		void *location;
		while (get_nth_image_symbol(id,n,name,&nameLen,&symType,&location) == B_OK) {
	
			// resize string block, if needed.
			while ((symbolNamesPtr + nameLen) > symbolNamesSize) {
				if (symbolNamesSize > 0) symbolNamesSize *= 2;
				else symbolNamesSize = 1024;
				symbolNames = (char*)realloc(symbolNames,symbolNamesSize);
				if (!symbolNames) {
					free_symbol_memory();
					return false;
				}
			}

			// resize symbol block, if needed.
			if ((sizeof(symbol)*symbolTableCount) >= (size_t)symbolTableSize) {
				if (symbolTableSize > 0) symbolTableSize *= 2;
				else symbolTableSize = 1024;
				symbolTable = (symbol*)realloc(symbolTable, symbolTableSize);
				if (!symbolTable) {
					free_symbol_memory();
					return false;
				}
			}

			// set up symbol.
			symbol& sym = symbolTable[symbolTableCount++];
			sym.addr = (uint32_t)location;
			sym.name = symbolNamesPtr;
			
			// set up name.
			strcpy(symbolNames + symbolNamesPtr, name);
			symbolNamesPtr += nameLen;

			n++;
			symType = B_SYMBOL_TYPE_ANY;
			nameLen = sizeof(name);
		}
	}
	qsort(symbolTable,symbolTableCount,sizeof(symbol),&symbol_cmp);
	return true;

#elif TARGET_HOST == TARGET_HOST_WIN32

	if (symbolWindowsInit) return true;

	SAutolock _l(symbolAccess.Lock());
	if (symbolWindowsInit) return true;

	symbolWindowsInit = 1;

	const int32_t BUFSIZE = 65536;
	char* buf = new char[BUFSIZE];

	SString symSearchPath("");

	if (GetCurrentDirectory(BUFSIZE, buf))
	{
		symSearchPath += buf;
		symSearchPath += ";";
	}
	
	if (GetModuleFileName(0, buf, BUFSIZE))
	{
		char* ptr;

		for (ptr = buf + strlen(buf) - 1 ; ptr >= buf ; --ptr)
		{
			// locate the rightmost path separator
			if (*ptr == '\\' || *ptr == '/' || *ptr == ':' )
				break;
		}
		// if we found one, p is pointing at it; if not, tt only contains
		// an exe name (no path), and p points before its first byte
		if (ptr != buf) // path sep found?
		{
			if (*ptr == ':') // we leave colons in place
				++ptr;
			*ptr = '\0'; // eliminate the exe name and last path sep
			symSearchPath += buf;
			symSearchPath += ";";
		}
	}
	
	delete buf;

	const char* env;

	env = getenv("_NT_SYMBOL_PATH");
	if (env)
	{
		symSearchPath += getenv("_NT_SYMBOL_PATH");
		symSearchPath += ";";
	}
	
	env = getenv("_NT_ALTERNATE_SYMBOL_PATH");
	if (env)
	{
		symSearchPath += getenv("_NT_ALTERNATE_SYMBOL_PATH");
		symSearchPath += ";";
	}

	env = getenv("SYSTEMROOT");
	if (env)
	{
		symSearchPath += getenv("SYSTEMROOT");
		symSearchPath += ";";
	}

	// if we added anything, we have a trailing semicolon
	if (symSearchPath.Length() > 0) 
		symSearchPath.RemoveLast(";");

	//printf( "symbols path: %s\n", symSearchPath.String());
	
	SymInitialize(GetCurrentProcess(), (char*)symSearchPath.String(), false);

	int32_t symOptions = SymGetOptions();
	symOptions |= SYMOPT_LOAD_LINES;
	symOptions &= ~SYMOPT_UNDNAME;
	SymSetOptions(symOptions); 

	
	MODULEENTRY32 me;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());

	bool moreModules = true;
	
	moreModules = Module32First(snapshot, &me);

	do 
	{
		SymLoadModule(GetCurrentProcess(), 0, me.szExePath, me.szModule, (DWORD)me.modBaseAddr, me.modBaseSize);
		
		moreModules = Module32Next(snapshot, &me);
	} while (moreModules);
	
	return true;

#else
	return false;
#endif
}

const char* lookup_symbol(uint32_t addr, uint32_t *offset, char* name, size_t bufSize)
{
	if (!load_symbols()) return "";

#if TARGET_HOST == TARGET_HOST_BEOS
	(void)name;
	int32_t i;
	const int32_t count = symbolTableCount-1;
	for (i=0;i<count;i++) {
		if ((addr >= symbolTable[i].addr) && (addr < symbolTable[i+1].addr)) break;
	};
	if (i >= count) return NULL;
	*offset = addr - symbolTable[i].addr;
	return symbolNames + symbolTable[i].name;

#elif TARGET_HOST == TARGET_HOST_WIN32
	
	if (!name)
		return "";

	const int32_t NAMELEN = bufSize;
	const int32_t SYMBLEN = sizeof(IMAGEHLP_SYMBOL) + NAMELEN;
	
	IMAGEHLP_SYMBOL *sym = (IMAGEHLP_SYMBOL*)malloc(SYMBLEN);

	memset(sym, 0, SYMBLEN);
	sym->SizeOfStruct = SYMBLEN;
	sym->MaxNameLength = NAMELEN;

	if (SymGetSymFromAddr(GetCurrentProcess(), addr, offset, sym))
	{
		UnDecorateSymbolName(sym->Name, name, NAMELEN, UNDNAME_COMPLETE);
	}
	
	free(sym);

	return name;
#else
	return "";
#endif
}

#endif	// else TARGET_HOST == TARGET_HOST_PALMOS

#if _SUPPORTS_NAMESPACE
}	// namespace Priv
#endif

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif // SUPPORTS_CALLSTACK
