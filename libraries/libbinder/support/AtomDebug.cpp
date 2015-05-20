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

#include <support/Atom.h>

#include <support/atomic.h>
#include <support/SupportDefs.h>
#include <support/StdIO.h>
#include <support/TypeConstants.h>
#include <support/Debug.h>
#include <support/SortedVector.h>
#include <support/Value.h>
#include <support_p/SupportMisc.h>

#include "AtomDebug.h"

#include <stdio.h>


#if SUPPORTS_ATOM_DEBUG

/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/

/* Implement SAtom debugging infrastructure */

#include <support/Autolock.h>
#include <support/Locker.h>
#include <support/String.h>
#include <support/StringIO.h>

#include <typeinfo>

#define AtomIO berr

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {
#endif // _SUPPORTS_NAMESPACE

SAtomLeakChecker::SAtomLeakChecker() : fCreated(0), fDestroyed(0), fFreed(0)
{
}
SAtomLeakChecker::~SAtomLeakChecker()
{
}

void SAtomLeakChecker::Reset()
{
	fCreated = fDestroyed = fFreed = 0;
}

void SAtomLeakChecker::Shutdown()
{
	if (AtomDebugLevel() > 0 && (fCreated || fDestroyed || fFreed)) {
		AtomIO << "SAtom: "
			 << (fCreated-fFreed) << " leaked. ("
			 << fCreated << " created, "
			 << fDestroyed << " destroyed, "
			 << fFreed << " freed)" << endl;
	}
}

void SAtomLeakChecker::NoteCreate()
{
	atomic_add(&fCreated, 1);
}
void SAtomLeakChecker::NoteDestroy()
{
	atomic_add(&fDestroyed, 1);
}
void SAtomLeakChecker::NoteFree()
{
	atomic_add(&fFreed, 1);
}

// --------------------------------------------------------------------

atom_ref_info::atom_ref_info()
	: id(NULL), count(0), thid(0), when(0), note(NULL), next(NULL)
{
}
atom_ref_info::~atom_ref_info()
{
	if (note) free(note);
}

bool atom_ref_info::operator==(const atom_ref_info& o) const
{
	if (id == o.id && stack == o.stack) return true;
	return false;
}

void atom_ref_info::report(const sptr<ITextOutput>& io, bool longForm) const
{
	const atom_ref_info* ref = this;
	while (ref) {
		if (longForm) {
			io	<< "ID: " << (void*)(ref->id)
				<< ", Thread: " << ref->thid
				<< ", When:" << ref->when << endl;
			io->MoveIndent(1);
			ref->stack.LongPrint(io, NULL);
			io->MoveIndent(-1);
		} else {
			io	<< (void*)(ref->id) << " " << ref->thid
				<< " " << ref->when << "us -- ";
			ref->stack.Print(io);
			io << endl;
		}
		ref = ref->next;
	}
}

int32_t atom_ref_info::refcount() const
{
	int32_t num = 0;
	const atom_ref_info* ref = this;
	while (ref) {
		ref = ref->next;
		num++;
	}
	return num;
}

atom_ref_info* atom_ref_info::find_last_ref()
{
	atom_ref_info* ref = this;
	while (ref->next) {
		ref = ref->next;
	}
	return ref;
}

// --------------------------------------------------------------------

atom_debug::atom_debug(SAtom* inAtom)
	:	SLocker("SAtom atom_debug state"),
		atom(inAtom), lightAtom(NULL),
		primary(INITIAL_PRIMARY_VALUE), secondary(0), initialized(0),
		mark(0),
		incStrongs(NULL), decStrongs(NULL), incWeaks(NULL), decWeaks(NULL)
{
}

atom_debug::atom_debug(SLightAtom* inAtom)
	:	SLocker("SAtom atom_debug state"),
		atom(NULL), lightAtom(inAtom),
		primary(INITIAL_PRIMARY_VALUE), secondary(0), initialized(0),
		mark(0),
		incStrongs(NULL), decStrongs(NULL), incWeaks(NULL), decWeaks(NULL)
{
}

atom_debug::~atom_debug()
{
	atom_ref_info *ref;
	while ((ref=incStrongs) != NULL) {
		incStrongs = ref->next;
		delete ref;
	}
	while ((ref=decStrongs) != NULL) {
		decStrongs = ref->next;
		delete ref;
	}
	while ((ref=incWeaks) != NULL) {
		incWeaks = ref->next;
		delete ref;
	}
	while ((ref=decWeaks) != NULL) {
		decWeaks = ref->next;
		delete ref;
	}
	atom = NULL;
	lightAtom = NULL;
}

void atom_debug::report(const sptr<ITextOutput>& io, uint32_t flags) const
{
	if (!(flags&B_ATOM_REPORT_REMOVE_HEADER)) {
		if (atom) io << "Report for atom " << atom;
		else io << "Report for light atom " << lightAtom;
		io	<< " ("
			<< (typenm != ""
					? typenm.String()
					: (primary < INITIAL_PRIMARY_VALUE
						? (atom ? B_TYPENAME(*atom) : B_TYPENAME(*lightAtom))
						: "<not initialized>"
						)
					)
			<< ") at mark #"
			<< mark << ":" << endl;
	}
	const int32_t level = AtomReportLevel(flags);
	if (level > 0) {
		const bool longForm = level > 5;
		io << primary << " IncStrong Remain:" << endl;
		io->MoveIndent(1);
		incStrongs->report(io, longForm);
		io->MoveIndent(-1);
		io << secondary << " IncWeak Remain:" << endl;
		io->MoveIndent(1);
		incWeaks->report(io, longForm);
		io->MoveIndent(-1);
		io << "DecStrong:" << endl;
		io->MoveIndent(1);
		decStrongs->report(io, longForm);
		io->MoveIndent(-1);
		io << "DecWeak:" << endl;
		io->MoveIndent(1);
		decWeaks->report(io, longForm);
		io->MoveIndent(-1);
	} else {
		io << primary << " IncStrong Remain, "
			<< secondary << " IncWeak Remain. ("
			<< incStrongs->refcount() << " IncStrong, "
			<< incWeaks->refcount() << " IncWeak, "
			<< decWeaks->refcount() << " DecWeak, "
			<< decStrongs->refcount() << " DecStrong)" << endl;
	}
}

// --------------------------------------------------------------------

SAtomTracker::SAtomTracker()
	:	fAccess("SAtomTracker"),
		fGone(false), fWatching(false),
		fFirstMark(0), fCurMark(0)
{
}

SAtomTracker::~SAtomTracker()
{
}

void SAtomTracker::Reset()
{
	SAutolock _l(fAccess.Lock());
	fFirstMark = IncrementMark();
}

void SAtomTracker::Shutdown()
{
	if (AtomDebugLevel() > 5) {
		SAutolock _l(fAccess.Lock());
		if (fActiveAtoms.CountItems() > 0) {
			AtomIO << "Leaked atoms:" << endl;
			AtomIO->MoveIndent(1);
			PrintActive(AtomIO, fFirstMark, -1, 0);
			AtomIO->MoveIndent(-1);
		}
		fActiveAtoms.MakeEmpty();
		fWatchTypes.MakeEmpty();
	}
	fGone = true;
}

void SAtomTracker::AddAtom(atom_debug* info)
{
	SAutolock _l(fAccess.Lock());
	if (fGone) return;
	info->mark = fCurMark;
	fActiveAtoms.AddItem(info);
}

void SAtomTracker::RemoveAtom(atom_debug* info)
{
	SAutolock _l(fAccess.Lock());
	if (fGone) return;
	fActiveAtoms.RemoveItemFor(info);
}

void SAtomTracker::PrintActive(const sptr<ITextOutput>& io, int32_t mark, int32_t last, uint32_t flags) const
{
	sptr<BStringIO> sio(new BStringIO);
	sptr<ITextOutput> blah(sio.ptr());

	SAutolock _l(fAccess.Lock());
	if (fActiveAtoms.CountItems() > 1) {
		for (uint32_t index = 0 ; index < fActiveAtoms.CountItems() ; index++) {
			atom_debug* atom = fActiveAtoms.ItemAt(index);
			if (atom->atom == sio.ptr()) continue;
			atom->Lock();
			if (atom->mark >= mark && (last < 0 || atom->mark <= last)) {
				atom->report(blah, flags);
				atom->Unlock();
				sio->PrintAndReset(io);
			} else {
				atom->Unlock();
			}
		}
	} else {
		io << "No active atoms." << endl;
	}
}

bool SAtomTracker::HasAtom(SAtom* a, bool primary)
{
	SAutolock _l(fAccess.Lock());
	
	// Note that there are race conditions here between the last
	// reference going away, the destructor being called, and the
	// atom being unregistered.  But this is just for debugging,
	// so I don't really care that much.
	size_t N = fActiveAtoms.CountItems();
	for (size_t index = 0 ; index < N ; index++) {
		atom_debug* ad = fActiveAtoms.ItemAt(index);
		if (ad->atom == a && a->WeakCount() > 0) {
			if (primary) a->IncStrong(a);
			else a->IncWeak(a);
			return true;
		}
	}
	return false;
}

bool SAtomTracker::HasLightAtom(SLightAtom* a)
{
	SAutolock _l(fAccess.Lock());
	
	// Note that there are race conditions here between the last
	// reference going away, the destructor being called, and the
	// atom being unregistered.  But this is just for debugging,
	// so I don't really care that much.
	size_t N = fActiveAtoms.CountItems();
	for (size_t index = 0 ; index < N ; index++) {
		atom_debug* ad = fActiveAtoms.ItemAt(index);
		if (ad->lightAtom == a && a->StrongCount() > 0) {
			a->IncStrong(a);
			return true;
		}
	}

	return false;
}

void SAtomTracker::GetActiveTypeNames(SSortedVector<SString>* outNames)
{
	SAutolock _l(fAccess.Lock());
	
	const size_t N = fActiveAtoms.CountItems();
	for (size_t i=0; i<N; i++) {
		outNames->AddItem(fActiveAtoms[i]->typenm);
	}
}

void SAtomTracker::GetAllWithTypeName(const char* typeName, SVector<wptr<SAtom> >* outAtoms, SVector<sptr<SLightAtom> >* outLightAtoms)
{
	SAutolock _l(fAccess.Lock());
	
	// Same race conditions as above.  Whatever.
	const size_t N = fActiveAtoms.CountItems();
	for (size_t i=0; i<N; i++) {
		atom_debug* a = fActiveAtoms[i];
		if (a->typenm == typeName) {
			if (a->atom != NULL && a->atom->WeakCount() > 0) {
				outAtoms->AddItem(a->atom);
			} else if (a->lightAtom != NULL && a->lightAtom->StrongCount() > 0) {
				outLightAtoms->AddItem(a->lightAtom);
			}
		}
	}
}

void SAtomTracker::StartWatching(const B_SNS(std::)type_info* type)
{
	SAutolock _l(fAccess.Lock());
	fWatchTypes.AddItem((B_SNS(std::)type_info*)type);
	fWatching = true;
}

void SAtomTracker::StopWatching(const B_SNS(std::)type_info* type)
{
	SAutolock _l(fAccess.Lock());
	fWatchTypes.RemoveItemFor((B_SNS(std::)type_info*)type);
	
	if (fWatchTypes.CountItems() <= 0) 
		fWatching = false;
}

void SAtomTracker::WatchAction(const SAtom* which, const char* action)
{
	SAutolock _l(fAccess.Lock());
	if (fGone || !fWatching) {
		return;
	}
	SAtom* constSucker = (SAtom*)which;
	if (fWatchTypes.IndexOf((B_SNS(std::)type_info*)B_TYPEPTR(*constSucker))) {
		sptr<BStringIO> blah(new BStringIO);
		sptr<ITextOutput> sio(blah.ptr());
		sio << "Action " << which << " "
			<< B_TYPENAME(*which) << "::" << action;
		SCallStack stack;
		stack.Update(2);
		sio << endl;
		sio->MoveIndent(1);
		stack.LongPrint(sio, NULL);
		sio->MoveIndent(-1);
		which->Report(sio);
		AtomIO << blah->String();
	}
}

void SAtomTracker::WatchAction(const SLightAtom* which, const char* action)
{
	SAutolock _l(fAccess.Lock());
	if (fGone || !fWatching) {
		return;
	}
	SLightAtom* constSucker = (SLightAtom*)which;
	if (fWatchTypes.IndexOf((B_SNS(std::)type_info*)B_TYPEPTR(*constSucker))) {
		sptr<BStringIO> blah(new BStringIO);
		sptr<ITextOutput> sio(blah.ptr());
		sio << "Action " << which << " "
			<< B_TYPENAME(*which) << "::" << action;
		SCallStack stack;
		stack.Update(2);
		sio << endl;
		sio->MoveIndent(1);
		stack.LongPrint(sio, NULL);
		sio->MoveIndent(-1);
		which->Report(sio);
		AtomIO << blah->String();
	}
}

// Globals
static int32_t gHasLeakChecker = 0;
static SAtomLeakChecker* gLeakChecker = NULL;
static int32_t gHasTracker = 0;
static SAtomTracker* gTracker = NULL;

SAtomLeakChecker* LeakChecker() 
{
	if ((gHasLeakChecker&2) != 0) return gLeakChecker;
	if (atomic_or(&gHasLeakChecker, 1) == 0) {
		gLeakChecker = new SAtomLeakChecker;
		atomic_or(&gHasLeakChecker, 2);
	} else {
		while ((gHasLeakChecker&2) == 0)
		  SysThreadDelay(B_MILLISECONDS(2), B_RELATIVE_TIMEOUT);
	}
	return gLeakChecker;
}

SAtomTracker* Tracker() 
{
	if ((gHasTracker&2) != 0) return gTracker;
	if (atomic_or(&gHasTracker, 1) == 0) {
		gTracker = new SAtomTracker;
		atomic_or(&gHasTracker, 2);
	} else {
		while ((gHasTracker&2) == 0) 
		    SysThreadDelay(B_MILLISECONDS(2), B_RELATIVE_TIMEOUT);
	}
	return gTracker;
}

struct atom_cleanup
{
	atom_cleanup()
	{
		// Gross hack: forget about any atoms that were created
		// before now, since we will never be able to clean them up.
		if (gLeakChecker) gLeakChecker->Reset();
		if (gTracker) gTracker->Reset();
	}
	
	~atom_cleanup()
	{
		if (gTracker) gTracker->Shutdown();
		if (gLeakChecker) gLeakChecker->Shutdown();
	}
};
static atom_cleanup gCleanup;

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::osp
#endif // _SUPPORTS_NAMESPACE

#endif
