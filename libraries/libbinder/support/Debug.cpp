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

#include <DebugMgr.h>
#include <support/Debug.h>
#include <support/KeyedVector.h>
#include <support/String.h>
#include <support/TextStream.h>
#include <support/StdIO.h>
#include <assert.h>

#if TARGET_HOST == TARGET_HOST_BEOS

extern "C" void ErrFatalErrorInContext(const char*, uint32_t, const char* errMsg, uint32_t)
{
	ErrFatalError(errMsg);
}

#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {

using namespace palmos::support;
#endif

class BRangedVector
{
public:
	void		AddItem(uint32_t start, uint32_t end, const SString &value);
	SString		FindItem(uint32_t key, bool *found) const;
	
	struct range
	{
		range();
		range(uint32_t s, uint32_t e, const SString &v);
		range(const range &r);
		uint32_t start;
		uint32_t end;
		SString value;
	};
	SVector<range> m_ranges;
};

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const BRangedVector::range &r);
const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const BRangedVector &v);


inline const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const BRangedVector::range &r)
{
	return io << "range(start: " << r.start << "  end: " << r.end << " value: " << r.value << ")";
}

inline const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const BRangedVector &v)
{
	io << "BRangedVector {"<< endl << indent;
	size_t i, count=v.m_ranges.CountItems();
	for (i=0; i<count; i++) {
		io << v.m_ranges[i] << endl;
	}
	io << dedent << "}";
	return io;
}

BRangedVector::range::range()
{
}

BRangedVector::range::range(uint32_t s, uint32_t e, const SString &v)
	:start(s), end(e), value(v)
{
}

BRangedVector::range::range(const range &r)
	:start(r.start), end(r.end), value(r.value)
{
}


void
BRangedVector::AddItem(uint32_t start, uint32_t end, const SString &value)
{
	if (end < start) return ;
	bool added = false;
	size_t i, count=m_ranges.CountItems();
	for (i=0; i<count && !added; i++) {
		range &r = m_ranges.EditItemAt(i);
		if (start < r.start) {
			// Insert it here
			m_ranges.AddItemAt(range(start, end, value), i);
			added = true;
		}
		else if (start == r.start) {
			if (end >= r.end) {
				// this new one entirely overlaps the old one
				r.end = end;
				r.value = value;
				added = true;
			}
			else {
				// need to split (will lose start position)
				r.start = end+1;
				m_ranges.AddItemAt(range(start, end, value), i);
				added = true;
			}
		}
		else if (start > r.start && start <= r.end) {
			if (end < r.end) {
				// need to split (will split into two, three items total now)
				uint32_t third_end = r.end;
				r.end = start-1;
				m_ranges.AddItemAt(range(start, end, value), i+1);
				m_ranges.AddItemAt(range(end+1, third_end, r.value), i+2);
				added = true;
				i++;
			}
			else {
				// need to split (will lose the end)
				r.end = start-1;
				m_ranges.AddItemAt(range(start, end, value), i+1);
				added = true;
				i++;
			}
		}
	}
	
	if (added) {
		count=m_ranges.CountItems();
		// clean up the rest of them after in the list, if we need to
		while (i<count) {
			range &r = m_ranges.EditItemAt(i);
			if (end >= r.end) {
				// r is entirely within me
				m_ranges.RemoveItemsAt(i, 1);
				count--;
				continue;
			}
			else if (r.start <= end) {
				// end is between r.start and r.end
				r.start = end+1;
			}
			else {
				// r.start > end, so we're done picking up the pieces
				break;
			}
			i++;
		}
	}
	else {
		// add it at the end
		m_ranges.AddItem(range(start, end, value));
	}
}

SString
BRangedVector::FindItem(uint32_t key, bool *found) const
{
	size_t i, count=m_ranges.CountItems();
	for (i=0; i<count; i++) {
		const range &r = m_ranges.ItemAt(i);
		if (key >= r.start && key <= r.end) {
			if (found) *found = true;
			return r.value;
		}
		else if (key < r.start) break;
	}
	if (found) *found = false;
	return SString();
}



#if _SUPPORTS_NAMESPACE
} } // namespace palmos::osp

using namespace palmos::osp;
using namespace palmos::support;

#endif



BRangedVector g_debugData;
size_t g_debugFieldWidth = 12;


void
add_debug_atom(BNS(palmos::support::) SAtom * atom, const char *name)
{
//	add_debug_sized(atom, atom->AtomObjectSize(), name);
}

void
add_debug_atom(const BNS(::palmos::support::)sptr<BNS(palmos::support::) SAtom>& atom, const char *name)
{
//	add_debug_sized(atom.ptr(), atom->AtomObjectSize(), name);
}

void
add_debug_sized(const void *key, size_t size, const char *name)
{
#if 0
static const char debug[]	= "[Debug]                              ";
	SString str(debug, MIN(sizeof(debug)-1, g_debugFieldWidth));
	bout << str << "Adding debug info named \"" << name << "\" from " << key << " to " << (void*)((int)key+size) << endl;
	g_debugData.AddItem((uint32_t)key, ((uint32_t)key)+size-1, SString(name));
#endif
}


void
set_debug_fieldwidth(int width)
{
	g_debugFieldWidth = width;
}


SString
lookup_debug(const BNS(::palmos::support::)sptr<BNS(palmos::support::) SAtom>& atom, bool padding)
{
	SString empty;
	return empty;
//	return lookup_debug((void *)atom.ptr(), padding);
}


SString
lookup_debug(const void *key, bool padding)
{
	SString str;
	bool found;
	str = g_debugData.FindItem((uint32_t)key, &found);
	if (!found) {
		char buf[12];
		sprintf(buf, "%p", key);
		str = buf;
	}
	if (padding) {
		size_t len = str.Length();
		if (len < g_debugFieldWidth) str.Append(' ', g_debugFieldWidth-len);
	}
	return str;
}


extern "C"
{
int _debuggerAssert(const char * file, int line, const char * expr)
{
berr << "ASSERT FAILED: File: " << file << ", Line: " << line << indent << endl
	 << "failed condition: " << expr << dedent << endl;
	 printf("FIX ME! support/Debug.cpp");
	assert(false);
	return 0;
}
}

#if 0

void
test_ranged_vector()
{
	bout << "-------------- beginning ranged vector test ----------------------------------------" << endl << endl;
	BRangedVector v;

	bout << "Empty: " << v << endl;
	
	v.AddItem(13, 12, SString("Wrong!"));
	bout << v << endl << endl;
	
	v.AddItem(100, 200, SString("100-200"));
	bout << v << endl << endl;
	
	v.AddItem(10, 20, SString("10-20"));
	bout << v << endl << endl;
	
	v.AddItem(100, 200, SString("100-200"));
	bout << v << endl << endl;
	
	v.AddItem(50, 60, SString("50-60"));
	bout << v << endl << endl;
	
	v.AddItem(80, 100, SString("80-100"));
	bout << v << endl << endl;
	
	v.AddItem(60, 80, SString("60-80"));
	bout << v << endl << endl;
	
	v.AddItem(110, 120, SString("110-120"));
	bout << v << endl << endl;
	
	v.AddItem(180, 200, SString("180-200"));
	bout << v << endl << endl;
	
	v.AddItem(190, 210, SString("190-210"));
	bout << v << endl << endl;
	
	v.AddItem(500, 600, SString("500-600"));
	bout << v << endl << endl;
	
	v.AddItem(500, 550, SString("500-600"));
	bout << v << endl << endl;
	
	uint32_t n;
	bool found;
	SString s;
	
	n = 15;
	s = v.FindItem(n, &found);
	bout << "finding: " << n << "  " << s << "   found: " << found << endl;
	
	n = 25;
	s = v.FindItem(n, &found);
	bout << "finding: " << n << "  " << s << "   found: " << found << endl;
	
	n = 55;
	s = v.FindItem(n, &found);
	bout << "finding: " << n << "  " << s << "   found: " << found << endl;
	
	n = 80;
	s = v.FindItem(n, &found);
	bout << "finding: " << n << "  " << s << "   found: " << found << endl;
	
	n = 81;
	s = v.FindItem(n, &found);
	bout << "finding: " << n << "  " << s << "   found: " << found << endl;
	
	n = 100;
	s = v.FindItem(n, &found);
	bout << "finding: " << n << "  " << s << "   found: " << found << endl;
	
	n = 550;
	s = v.FindItem(n, &found);
	bout << "finding: " << n << "  " << s << "   found: " << found << endl;
	
	bout << "-------------- finished with ranged vector test ----------------------------------------" << endl << endl;
}

#endif
