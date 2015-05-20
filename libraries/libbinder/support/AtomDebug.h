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

#ifndef _SUPPORT_ATOM_DEBUG_H
#define _SUPPORT_ATOM_DEBUG_H

#include <support/Atom.h>
#include <support/atomic.h>

#define INITIAL_PRIMARY_VALUE (1<<28)

#ifndef SUPPORTS_ATOM_DEBUG
#if BUILD_TYPE == BUILD_TYPE_DEBUG
#define SUPPORTS_ATOM_DEBUG 1
#else
#define SUPPORTS_ATOM_DEBUG 0
#endif
#endif

#if SUPPORTS_ATOM_DEBUG

#include <support/CallStack.h>
#include <support/Locker.h>
#include <support/ITextStream.h>
#include <support/SortedVector.h>
#include <support/String.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {

using namespace palmos::support;
using namespace std;
#endif // _SUPPORTS_NAMESPACE

// These two are implemented in Atom.cpp.
int32_t AtomDebugLevel();
int32_t AtomReportLevel(uint32_t flags);

// The rest are implemented in AtomDebug.cpp.

class SAtomLeakChecker {
public:
	SAtomLeakChecker();
	~SAtomLeakChecker();
	
	void Reset();
	void Shutdown();
	
	void NoteCreate();
	void NoteDestroy();
	void NoteFree();

private:
	int32_t fCreated;
	int32_t fDestroyed;
	int32_t fFreed;
};

struct atom_ref_info {
	const void*		id;
	SCallStack		stack;
	int32_t			count;
	SysHandle		thid;
	nsecs_t			when;
	char*			note;
	atom_ref_info*	next;
	
					atom_ref_info();
					~atom_ref_info();
	
	bool			operator==(const atom_ref_info& o) const;
	void			report(const sptr<ITextOutput>& io, bool longForm=false) const;

	int32_t 		refcount() const;
	atom_ref_info*	find_last_ref();
};

struct atom_debug : public SLocker {
	SAtom*				atom;
	SLightAtom*			lightAtom;
	SString				typenm;
	int32_t				primary;
	int32_t				secondary;
	int32_t				initialized;
	int32_t				mark;
	atom_ref_info*		incStrongs;
	atom_ref_info*		decStrongs;
	atom_ref_info*		incWeaks;
	atom_ref_info*		decWeaks;
	
					atom_debug(SAtom* inAtom);
					atom_debug(SLightAtom* inAtom);
					~atom_debug();
	
	void			report(const sptr<ITextOutput>& io, uint32_t flags) const;
};

class SAtomTracker
{
public:
	SAtomTracker();
	~SAtomTracker();
	
	void Reset();
	void Shutdown();
	
	void AddAtom(atom_debug* info);
	void RemoveAtom(atom_debug* info);
	
	inline int32_t CurrentMark() const { return fCurMark; }
	inline int32_t IncrementMark() { return atomic_add(&fCurMark, 1) + 1; }
	
	void PrintActive(const sptr<ITextOutput>& io, int32_t mark, int32_t last, uint32_t flags) const;
	
	bool HasAtom(SAtom* a, bool primary);
	bool HasLightAtom(SLightAtom* a);
	void GetActiveTypeNames(SSortedVector<SString>* outNames);
	void GetAllWithTypeName(const char* typeName, SVector<wptr<SAtom> >* outAtoms, SVector<sptr<SLightAtom> >* outLightAtoms);

	void StartWatching(const B_SNS(std::)type_info* type);
	void StopWatching(const B_SNS(std::)type_info* type);
	void WatchAction(const SAtom* which, const char* action);
	void WatchAction(const SLightAtom* which, const char* action);
	
private:
	mutable SNestedLocker fAccess;
	bool fGone;
	bool fWatching;
	int32_t fFirstMark;
	int32_t fCurMark;
	
	SSortedVector<atom_debug*> fActiveAtoms;
	SSortedVector<B_SNS(std::)type_info*> fWatchTypes;
};

SAtomLeakChecker* LeakChecker();
SAtomTracker* Tracker();

#if _SUPPORTS_NAMESPACE	
} }	// namespace palmos::osp
#endif // _SUPPORTS_NAMESPACE

#endif /* SUPPORTS_ATOM_DEBUG */

#endif /* _SUPPORT_ATOM_DEBUG_H */
