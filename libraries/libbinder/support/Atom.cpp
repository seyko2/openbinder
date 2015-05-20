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

#include "AtomDebug.h"

#include <SysThread.h>
#include <support/Atom.h>
#include <support/atomic.h>
#if !LIBBE_BOOTSTRAP
#include <support/Handler.h>
#endif
#include <support/SupportDefs.h>
#include <support/StdIO.h>
#include <support/TLS.h>
#include <support/TypeConstants.h>
#include <support/Value.h>
#include <support/Vector.h>
#include <support/String.h>
#include <support/Debug.h>
#include <support_p/SupportMisc.h>

#include <stdio.h>

/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

struct pending_destruction
{
	SAtom* atom;
	const void* id;
};
B_IMPLEMENT_BASIC_TYPE_FUNCS(pending_destruction);

#if !LIBBE_BOOTSTRAP
static SLocker g_atomDestructorLock;
#endif
static SVector<pending_destruction>* g_atomDestructorVec = NULL;

#if !LIBBE_BOOTSTRAP
// AsyncDestructor not used in LIBBE_BOOTSTRAP
class SAtom::AsyncDestructor : public SHandler
{
public:
	virtual status_t HandleMessage(const SMessage &msg)
	{
		if (msg.What() == 'kill') {
			g_atomDestructorLock.Lock();
			SVector<pending_destruction>* atoms = g_atomDestructorVec;
			g_atomDestructorVec = NULL;
			g_atomDestructorLock.Unlock();

			if (atoms) {
				const size_t N(atoms->CountItems());
				for (size_t i=0; i<N; i++) {
					const pending_destruction& pd=atoms->ItemAt(i);
					pd.atom->do_async_destructor(pd.id);
				}

				g_atomDestructorLock.Lock();
				if (g_atomDestructorVec == NULL) {
					atoms->MakeEmpty();
					g_atomDestructorVec = atoms;
				}
				g_atomDestructorLock.Unlock();
			}
		}
		return B_OK;
	}
};

static sptr<SHandler> g_atomDestructor;

void SAtom::schedule_async_destructor(const void* id)
{
	SLocker::Autolock _l(g_atomDestructorLock);

	pending_destruction pd;
	pd.atom = this;
	pd.id = id;

	if (g_atomDestructorVec == NULL) {
		g_atomDestructorVec = new SVector<pending_destruction>;
		if (g_atomDestructorVec == NULL) goto badstuff;
	}
	if (g_atomDestructor == NULL) {
		g_atomDestructor = new AsyncDestructor;
		if (g_atomDestructor == NULL) goto badstuff;
	}
	
	if (g_atomDestructorVec->CountItems() == 0) {
		if (g_atomDestructor->PostMessage(SMessage('kill')) != B_OK) {
			goto badstuff;
		}
	}

	if (g_atomDestructorVec->AddItem(pd) >= B_OK) {
		return;
	}


badstuff:
	// Something really bad happened...  this isn't safe (we
	// could be stepping into a deadlock here), but the only
	// other choice is to leak...
	_l.Unlock();
	do_async_destructor(id);
}

#else	// #if !LIBBE_BOOTSTRAP
void SAtom::schedule_async_destructor(const void* id)
{
	// no schedule_async_destructor in LIBBE_BOOTSTRAP
	do_async_destructor(id);
}
#endif	// #if !LIBBE_BOOTSTRAP


void SAtom::do_async_destructor(const void* id)
{
	delete this;
	DecWeak(id);
}

struct SAtom::base_data
{
	void unlink();

	// These are only used when debugging is not enabled.  Else
	// the counts are in debugPtr below.
	mutable	int32_t strongCount;
	mutable	int32_t weakCount;

	// Before the base_data is attached to an atom, 'atom' points
	// to the next base_data in a chain of pending allocations.
	// In this case the low bit is set to 1.
	union {
		SAtom* atom;
		base_data* next;
		uint32_t pending;	// low bit set if in the pending allocation chain.
	};

	// Size of the original allocation.  Before being attached to
	// the atom, this is the size of the allocation.  After being
	// attached it is 0.  After the last strong reference goes away
	// this holds the size of the deallocation.
	size_t size;

#if SUPPORTS_ATOM_DEBUG
	mutable	BNS(::palmos::osp::) atom_debug* debugPtr;
	size_t new_size;
#endif
};

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/

#if !SUPPORTS_ATOM_DEBUG

/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/

#include <typeinfo>

/* No atom debugging -- just run as fast as possible. */

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif
	void SAtom::RenameOwnerID(const void*, const void*) const				{ }
	inline void SAtom::move_pointers(SAtom**, SAtom**, size_t)				{ }
	
	void SAtom::Report(const sptr<ITextOutput>&, uint32_t) const			{ }
	int32_t SAtom::MarkLeakReport()											{ return -1; }
	void SAtom::LeakReport(const sptr<ITextOutput>&, int32_t, int32_t, uint32_t) { }
	void SAtom::LeakReport(int32_t, int32_t, uint32_t)						{ }

	bool SAtom::ExistsAndIncStrong(SAtom*)									{ return false; }
	bool SAtom::ExistsAndIncWeak(SAtom*)									{ return false; }
	void SAtom::GetActiveTypeNames(SSortedVector<SString>*)					{ }
	void SAtom::GetAllWithTypeName(const char*, SVector<wptr<SAtom> >*, SVector<sptr<SLightAtom> >* ) { }

	void SAtom::StartWatching(const B_SNS(std::) type_info*)					{ }
	void SAtom::StopWatching(const B_SNS(std::) type_info*)					{ }

	size_t SAtom::AtomObjectSize() const									{ return 0; }
	
	inline void SAtom::init_atom()											{ }
	inline void SAtom::term_atom()											{ }

	inline void SAtom::lock_atom() const									{ }
	inline void SAtom::unlock_atom() const									{ }
	
	inline int32_t* SAtom::strong_addr() const								{ return &m_base->strongCount; }
	inline int32_t* SAtom::weak_addr() const								{ return &m_base->weakCount; }
	inline int32_t SAtom::strong_count() const								{ return m_base->strongCount; }
	inline int32_t SAtom::weak_count() const								{ return m_base->weakCount; }
	
	inline void SAtom::watch_action(const char*) const						{ }
	inline void SAtom::do_report(const sptr<ITextOutput>&, uint32_t) const	{ }
	
	inline void SAtom::add_incstrong(const void*, int32_t) const			{ }
	inline void SAtom::add_decstrong(const void*) const						{ }
	inline void SAtom::add_incweak(const void*) const						{ }
	inline void SAtom::add_decweak(const void*) const						{ }

	inline void SAtom::transfer_refs(SAtom*)								{ }

	/*-------------------------------------------------*/

	void SLightAtom::RenameOwnerID(const void*, const void*) const				{ }
	inline void SLightAtom::move_pointers(SLightAtom**, SLightAtom**, size_t)	{ }
	
	void SLightAtom::Report(const sptr<ITextOutput>&, uint32_t) const			{ }

	bool SLightAtom::ExistsAndIncStrong(SLightAtom*)							{ return false; }

	inline void SLightAtom::init_atom()											{ }
	inline void SLightAtom::term_atom()											{ }

	inline void SLightAtom::lock_atom() const									{ }
	inline void SLightAtom::unlock_atom() const									{ }
	
	inline int32_t* SLightAtom::strong_addr() const								{ return &m_strongCount; }
	inline int32_t SLightAtom::strong_count() const								{ return m_strongCount; }
	
	inline void SLightAtom::watch_action(const char*) const						{ }
	inline void SLightAtom::do_report(const sptr<ITextOutput>&, uint32_t) const	{ }
	
	inline void SLightAtom::add_incstrong(const void*, int32_t) const			{ }
	inline void SLightAtom::add_decstrong(const void*) const					{ }
#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#define NOTE_CREATE()
#define NOTE_DESTROY()
#define NOTE_FREE()
	
#else

/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/

/* Implement SAtom debugging infrastructure */

#include <support/StringIO.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {
#endif
	static volatile uint32_t gHasDebugLevel = 0;
	static int32_t gDebugLevel = 0;
	
	static inline int32_t FastAtomDebugLevel() {
		if ((gHasDebugLevel&2) != 0) return gDebugLevel;
		return AtomDebugLevel();
	}
	
	int32_t AtomDebugLevel() {
		if ((gHasDebugLevel&2) != 0) return gDebugLevel;
		if (g_threadDirectFuncs.atomicOr32(&gHasDebugLevel, 1) == 0) {

			bool instrumentationAllowed = true;
			const char* env = getenv("ATOM_DEBUG");
			if (instrumentationAllowed && env) {
				gDebugLevel = atoi(env);
				if (gDebugLevel < 0)
					gDebugLevel = 0;
			} else {
				gDebugLevel = 0;
			}
			g_threadDirectFuncs.atomicOr32(&gHasDebugLevel, 2);
			if (gDebugLevel > 0)
				printf("ATOM DEBUGGING ENABLED!  ATOM_DEBUG=%d, ATOM_REPORT=%d\n",
					gDebugLevel, AtomReportLevel(0));
		}
		while ((gHasDebugLevel&2) == 0) SysThreadDelay(B_MILLISECONDS(2), B_RELATIVE_TIMEOUT);
		return gDebugLevel;
	}
	
	static volatile uint32_t gHasReportLevel = 0;
	static int32_t gReportLevel = 0;
	
	int32_t AtomReportLevel(uint32_t flags) {
		switch (flags&B_ATOM_REPORT_FORCE_MASK) {
			case B_ATOM_REPORT_FORCE_LONG:		return 10;
			case B_ATOM_REPORT_FORCE_SHORT:		return 5;
			case B_ATOM_REPORT_FORCE_SUMMARY:	return 0;
			default:						break;
		}
		
		if ((gHasReportLevel&2) != 0) return gReportLevel;
		if (g_threadDirectFuncs.atomicOr32(&gHasReportLevel, 1) == 0) {
		
			bool instrumentationAllowed = true;
			const char* env = getenv("ATOM_REPORT");
			if (instrumentationAllowed && env) {
				gReportLevel = atoi(env);
				if (gReportLevel < 0)
					gReportLevel = 0;
			} else {
				gReportLevel = 0;
			}
			g_threadDirectFuncs.atomicOr32(&gHasReportLevel, 2);
		}
		while ((gHasReportLevel&2) == 0) 
			SysThreadDelay(B_MILLISECONDS(2), B_RELATIVE_TIMEOUT);
		return gReportLevel;
	}
#if _SUPPORTS_NAMESPACE	
}	//namespace osp
using namespace osp;
#endif

#if _SUPPORTS_NAMESPACE
namespace support {
#endif	
	static void rename_owner_id(atom_ref_info* refs, const void* newID, const void* oldID)
	{
		while (refs) {
			if (refs->id == oldID) refs->id = newID;
			refs = refs->next;
		}
	}
	
	/*! This is useful, for example, if you have a piece of memory
		with a pointer to an atom, whose memory address you have used
		for the reference ID.  If you are moving your memory to a new
		location with memcpy(), you can use this function to change
		the ID stored in the atom for your reference.
		
		This function does nothing if atom debugging is not currently
		enabled.

		@see MovePointersBefore(), MovePointersAfter()
	*/
	void SAtom::RenameOwnerID(const void* newID, const void* oldID) const
	{
		if (FastAtomDebugLevel() > 5) {
			rename_owner_id(m_base->debugPtr->incStrongs, newID, oldID);
			rename_owner_id(m_base->debugPtr->incWeaks, newID, oldID);
		}
	}
	
	inline void SAtom::move_pointers(SAtom** newPtr, SAtom** oldPtr, size_t num)
	{
		if (FastAtomDebugLevel() > 5) {
			while (num > 0) {
				if (oldPtr[num] != NULL) oldPtr[num]->RenameOwnerID(oldPtr, newPtr);
				num--;
			}
		}
	}
	
	/*!	The @a flags may be one of B_ATOM_REPORT_FORCE_LONG,
		B_ATOM_REPORT_FORCE_SHORT, or B_ATOM_REPORT_FORCE_SUMMARY,
		as well as B_ATOM_REPORT_REMOVE_HEADER.
	
		This is the API called by the "atom report" command described
		in @ref SAtomDebugging.

		@note Debugging only.  Set the ATOM_DEBUG and ATOM_REPORT
		environment variables to use.
		
		This function only works if you are linking
		with a library that contains debugging code and have set the
		ATOM_DEBUG environment variable.  Choices for ATOM_DEBUG are:
			-	<= 0: Debugging disabled.
			-	<= 5: Simple debugging -- create/delete statistics only.
			-	<= 10: Full SAtom debugging enabled.

		In addition, you can modify the default Report() format with the
		ATOM_REPORT environment variable:
			-	<= 0: Short summary report.
			-	<= 5: One-line call stacks.
			-	<= 10: Call stacks with full symbols.
	*/
	void SAtom::Report(const sptr<ITextOutput>& io, uint32_t flags) const {
		sptr<BStringIO> sio(new BStringIO);
		sptr<ITextOutput> blah(sio.ptr());
		lock_atom();
		do_report(blah, flags);
		unlock_atom();
		io << sio->String();
	}
	
	/*!	This is the API called by the "atom mark" command described
		in @ref SAtomDebugging.

		@note Debugging only.  Set the ATOM_DEBUG and ATOM_REPORT
		environment variables (described in Report()) to use.
		@see Report(), LeakReport()
	*/
	int32_t SAtom::MarkLeakReport() {
		if (FastAtomDebugLevel() > 5) return Tracker()->IncrementMark();
		return -1;
	}
	
	/*!	This function prints information about all of the currently
		active atoms created during the leak context \a mark up to and
		including the context \a last.

		A \a mark context of 0 is the first context; a \a last context
		of -1 means the current context.

		This is the API called by the "atom leaks" command described
		in @ref SAtomDebugging.

		@note Debugging only.  Set the ATOM_DEBUG and ATOM_REPORT
		environment variables (described in Report()) to use.
		@see Report(), MarkLeakReport()
	*/
	void SAtom::LeakReport(const sptr<ITextOutput>& io, int32_t mark, int32_t last, uint32_t flags) {
		if (FastAtomDebugLevel() > 5) {
			if (last < 0)
				io << "Active atoms since mark " << mark << ":" << endl;
			else
				io << "Active atoms from mark " << mark << " to " << last << ":" << endl;
			io->MoveIndent(1);
			Tracker()->PrintActive(io, mark, last, flags);
			io->MoveIndent(-1);
		}
	}
	
	void SAtom::LeakReport(int32_t mark, int32_t last, uint32_t flags) {
		LeakReport(berr, mark, last, flags);
	}
	
	/*!	Check whether the given atom currently exists, and acquire a
		strong pointer if so.  These only work when the
		leak checker is turned on; otherwise, false is always returned.
		@note Debugging only.  Set the ATOM_DEBUG and ATOM_REPORT
		environment variables to use.
		@see ExistsAndIncWeak()
		@see Report()
		@see MarkLeakReport()
	*/
	bool SAtom::ExistsAndIncStrong(SAtom* atom) {
		if (FastAtomDebugLevel() > 5 && atom != NULL)
			return Tracker()->HasAtom(atom, true);
		return false;
	}

	/*!	Check whether the given atom currently exists, and acquire a
		weak pointer if so.  These only work when the
		leak checker is turned on; otherwise, false is always returned.
		@note Debugging only.  Set the ATOM_DEBUG and ATOM_REPORT
		environment variables to use.
		@see ExistsAndIncStrong()
		@see Report()
		@see MarkLeakReport()
	*/
	bool SAtom::ExistsAndIncWeak(SAtom* atom) {
		if (FastAtomDebugLevel() > 5 && atom != NULL)
			return Tracker()->HasAtom(atom, false);
		return false;
	}

	/*! Return the type names for all atom objects that current exist.
	*/
	void SAtom::GetActiveTypeNames(SSortedVector<SString>* outNames) {
		if (FastAtomDebugLevel() > 5)
			Tracker()->GetActiveTypeNames(outNames);
	}

	/*! Retrieve all currently existing atoms that are the given type name.
		These are returned as an array of weak references to them, so as a side
		effect you will be acquiring weak references on these atoms as they
		are found.
	*/
	void SAtom::GetAllWithTypeName(const char* typeName, SVector<wptr<SAtom> >* outAtoms, SVector<sptr<SLightAtom> >* outLightAtoms) {
		if (FastAtomDebugLevel() > 5)
			Tracker()->GetAllWithTypeName(typeName, outAtoms, outLightAtoms);
	}

	/*!	When a watch is in effect, SAtom will print to standard out whenever
		significant operations happen on an object of the watched type.
		Note that they must be @e exactly this
		type -- subclasses are not included.
		Use StopWatching() to remove a watched type.
		@note Debugging only.  Set the ATOM_DEBUG and ATOM_REPORT
		environment variables to use.
		@see Report()
		@see MarkLeakReport()
	*/
	void SAtom::StartWatching(const B_SNS(std::) type_info* type) {
		Tracker()->StartWatching(type);
	}
	
	void SAtom::StopWatching(const B_SNS(std::) type_info* type) {
		Tracker()->StopWatching(type);
	}
	
	inline void SAtom::init_atom() {
		if (FastAtomDebugLevel() > 5) {
			m_base->debugPtr = new(B_SNS(std::) nothrow) atom_debug(this);
			ErrFatalErrorIf(!m_base->debugPtr, "Out of memory!");
			Tracker()->AddAtom(m_base->debugPtr);
		}
	}
	inline void SAtom::term_atom() {
		if (FastAtomDebugLevel() > 5) {
			Tracker()->RemoveAtom(m_base->debugPtr);
			delete m_base->debugPtr;
			m_base->debugPtr = NULL;
		}
	}

	inline void SAtom::lock_atom() const {
		if (FastAtomDebugLevel() > 5) m_base->debugPtr->Lock();
	}
	inline void SAtom::unlock_atom() const {
		if (FastAtomDebugLevel() > 5) m_base->debugPtr->Unlock();
	}
	
	inline int32_t* SAtom::strong_addr() const {
		if (FastAtomDebugLevel() > 5) return &(m_base->debugPtr->primary);
		return &m_base->strongCount;
	}
	inline int32_t* SAtom::weak_addr() const {
		if (FastAtomDebugLevel() > 5) return &(m_base->debugPtr->secondary);
		return &m_base->weakCount;
	}
	
	inline int32_t SAtom::strong_count() const {
		if (FastAtomDebugLevel() > 5) return m_base->debugPtr ? m_base->debugPtr->primary : 0;
		return m_base->strongCount;
	}
	inline int32_t SAtom::weak_count() const {
		if (FastAtomDebugLevel() > 5) return m_base->debugPtr ? m_base->debugPtr->secondary : 0;
		return m_base->weakCount;
	}

	inline void SAtom::watch_action(const char* description) const {
		if (FastAtomDebugLevel() > 5) Tracker()->WatchAction(this, description);
	}
	
	inline void SAtom::do_report(const sptr<ITextOutput>& io, uint32_t flags) const {
		if (FastAtomDebugLevel() > 5) m_base->debugPtr->report(io, flags);
	}
	
	inline void SAtom::add_incstrong(const void* id, int32_t cnt) const {
		if (FastAtomDebugLevel() > 5) add_incstrong_raw(id, cnt);
	}
	
	inline void SAtom::add_decstrong(const void* id) const {
		if (FastAtomDebugLevel() > 5) add_decstrong_raw(id);
	}
	
	inline void SAtom::add_incweak(const void* id) const {
		if (FastAtomDebugLevel() > 5) add_incweak_raw(id);
	}
	
	inline void SAtom::add_decweak(const void* id) const {
		if (FastAtomDebugLevel() > 5) add_decweak_raw(id);
	}
	
	void SAtom::add_incstrong_raw(const void* id, int32_t cnt) const {
		if (cnt == INITIAL_PRIMARY_VALUE) {
			m_base->debugPtr->typenm = B_TYPENAME(*this);
		}
		atom_ref_info *ref = new(B_SNS(std::) nothrow) atom_ref_info;
		if (ref) {
			ref->id = id;
			ref->thid = SysCurrentThread();
			ref->when = SysGetRunTime();
			ref->note = NULL;
			ref->stack.Update(1);
			ref->next = m_base->debugPtr->incStrongs;
			m_base->debugPtr->incStrongs = ref;
		}
	}
	
	void SAtom::add_decstrong_raw(const void* id) const {
		bool found = false;
		if (id) {
			atom_ref_info *p,**ref;
			for (ref = &m_base->debugPtr->incStrongs; *ref;ref = &(*ref)->next) {
				if ((*ref)->id == id) {
					p = *ref;
					*ref = (*ref)->next;
					delete p;
					found = true;
					break;
				}
			}
		}
	
		if (!found) {
			atom_ref_info *ref = new(B_SNS(std::) nothrow) atom_ref_info;
			if (ref) {
				ref->id = id;
				ref->thid = SysCurrentThread();
				ref->when = SysGetRunTime();
				ref->stack.Update(1);
				ref->next = m_base->debugPtr->decStrongs;
				m_base->debugPtr->decStrongs = ref;
			}
			
			if (id) {
				sptr<BStringIO> blah(new BStringIO);
				sptr<ITextOutput> io(blah.ptr());
				io->MoveIndent(1);
				io << "SAtom: No IncStrong() found for DecStrong() id=" << id << endl;
				do_report(io, B_ATOM_REPORT_FORCE_LONG);
				io->MoveIndent(-1);
				ErrFatalError(blah->String());
				
			}
		}
	}
	
	void SAtom::add_incweak_raw(const void* id) const {
		atom_ref_info *ref = new(B_SNS(std::) nothrow) atom_ref_info;
		if (ref) {
			ref->id = id;
			ref->thid = SysCurrentThread();
			ref->when = SysGetRunTime();
			ref->note = NULL;
			ref->stack.Update(1);
			ref->next = m_base->debugPtr->incWeaks;
			m_base->debugPtr->incWeaks = ref;
		}
	}
	
	void SAtom::add_decweak_raw(const void* id) const {
		bool found = false;
		if (id) {
			atom_ref_info *p,**ref;
			for (ref = &m_base->debugPtr->incWeaks; *ref;ref = &(*ref)->next) {
				if ((*ref)->id == id) {
					p = *ref;
					*ref = (*ref)->next;
					delete p;
					found = true;
					break;
				}
			}
		}
	
		if (!found) {
			atom_ref_info *ref = new(B_SNS(std::) nothrow) atom_ref_info;
			if (ref) {
				ref->id = id;
				ref->thid = SysCurrentThread();
				ref->when = SysGetRunTime();
				ref->stack.Update(1);
				ref->next = m_base->debugPtr->decWeaks;
				m_base->debugPtr->decWeaks = ref;
			}

			if (id) {
				sptr<BStringIO> blah(new BStringIO);
				sptr<ITextOutput> io(blah.ptr());
				io->MoveIndent(1);
				io << "SAtom: No IncWeak() found for DecWeak() id=" << id << endl;
				do_report(io, B_ATOM_REPORT_FORCE_LONG);
				io->MoveIndent(-1);
				ErrFatalError(blah->String());
			}
		}
	}

	static void transfer_ref_chain(atom_ref_info** dst, atom_ref_info** src)
	{
		if (*src) {
			atom_ref_info* end = (*src)->find_last_ref();
			end->next = *dst;
			*dst = *src;
			*src = NULL;
		}
	}

	void SAtom::transfer_refs(SAtom* from)
	{
		if (FastAtomDebugLevel() <= 5) return;
		transfer_ref_chain(&m_base->debugPtr->incStrongs, &from->m_base->debugPtr->incStrongs);
		transfer_ref_chain(&m_base->debugPtr->decStrongs, &from->m_base->debugPtr->decStrongs);
		transfer_ref_chain(&m_base->debugPtr->incWeaks, &from->m_base->debugPtr->incWeaks);
		transfer_ref_chain(&m_base->debugPtr->decWeaks, &from->m_base->debugPtr->decWeaks);
	}

	/*-------------------------------------------------*/

	/*! This is useful, for example, if you have a piece of memory
		with a pointer to an atom, whose memory address you have used
		for the reference ID.  If you are moving your memory to a new
		location with memcpy(), you can use this function to change
		the ID stored in the atom for your reference.
		
		This function does nothing if atom debugging is not currently
		enabled.

		@see MovePointersBefore(), MovePointersAfter()
	*/
	void SLightAtom::RenameOwnerID(const void* newID, const void* oldID) const
	{
		if (FastAtomDebugLevel() > 5) {
			rename_owner_id(m_debugPtr->incStrongs, newID, oldID);
		}
	}
	
	inline void SLightAtom::move_pointers(SLightAtom** newPtr, SLightAtom** oldPtr, size_t num)
	{
		if (FastAtomDebugLevel() > 5) {
			while (num > 0) {
				if (oldPtr[num] != NULL) oldPtr[num]->RenameOwnerID(oldPtr, newPtr);
				num--;
			}
		}
	}
	
	/*!	@copydoc SAtom::Report */
	void SLightAtom::Report(const sptr<ITextOutput>& io, uint32_t flags) const {
		sptr<BStringIO> sio(new BStringIO);
		sptr<ITextOutput> blah(sio.ptr());
		lock_atom();
		do_report(blah, flags);
		unlock_atom();
		io << sio->String();
	}
	
	/*!	Check whether the given atom currently exists, and acquire a
		strong pointer if so.  This only works when the
		leak checker is turned on; otherwise, false is always returned.
		@note Debugging only.  Set the ATOM_DEBUG and ATOM_REPORT
		environment variables to use.
		@see Report()
		@see MarkLeakReport()
	*/
	bool SLightAtom::ExistsAndIncStrong(SLightAtom* atom) {
		if (FastAtomDebugLevel() > 5 && atom != NULL)
			return Tracker()->HasLightAtom(atom);
		return false;
	}

	inline void SLightAtom::init_atom() {
		if (FastAtomDebugLevel() > 5) {
			m_debugPtr = new(B_SNS(std::) nothrow) atom_debug(this);
			ErrFatalErrorIf(!m_debugPtr, "Out of memory!");
			Tracker()->AddAtom(m_debugPtr);
		}
	}
	inline void SLightAtom::term_atom() {
		if (FastAtomDebugLevel() > 5) {
			Tracker()->RemoveAtom(m_debugPtr);
			delete m_debugPtr;
			m_debugPtr = NULL;
		}
	}

	inline void SLightAtom::lock_atom() const {
		if (FastAtomDebugLevel() > 5) m_debugPtr->Lock();
	}
	inline void SLightAtom::unlock_atom() const {
		if (FastAtomDebugLevel() > 5) m_debugPtr->Unlock();
	}
	
	inline int32_t* SLightAtom::strong_addr() const {
		if (FastAtomDebugLevel() > 5) return &(m_debugPtr->primary);
		return &m_strongCount;
	}
	
	inline int32_t SLightAtom::strong_count() const {
		if (FastAtomDebugLevel() > 5) return m_debugPtr ? m_debugPtr->primary : 0;
		return m_strongCount;
	}

	inline void SLightAtom::watch_action(const char* description) const {
		if (FastAtomDebugLevel() > 5) Tracker()->WatchAction(this, description);
	}
	
	inline void SLightAtom::do_report(const sptr<ITextOutput>& io, uint32_t flags) const {
		if (FastAtomDebugLevel() > 5) m_debugPtr->report(io, flags);
	}
	
	inline void SLightAtom::add_incstrong(const void* id, int32_t cnt) const {
		if (FastAtomDebugLevel() > 5) add_incstrong_raw(id, cnt);
	}
	
	inline void SLightAtom::add_decstrong(const void* id) const {
		if (FastAtomDebugLevel() > 5) add_decstrong_raw(id);
	}
	
	void SLightAtom::add_incstrong_raw(const void* id, int32_t cnt) const {
		if (cnt == INITIAL_PRIMARY_VALUE) {
			m_debugPtr->typenm = B_TYPENAME(*this);
		}
		atom_ref_info *ref = new(B_SNS(std::) nothrow) atom_ref_info;
		if (ref) {
			ref->id = id;
			ref->thid = SysCurrentThread();
			ref->when = SysGetRunTime();
			ref->note = NULL;
			ref->stack.Update(1);
			ref->next = m_debugPtr->incStrongs;
			m_debugPtr->incStrongs = ref;
		}
	}
	
	void SLightAtom::add_decstrong_raw(const void* id) const {
		bool found = false;
		if (id) {
			atom_ref_info *p,**ref;
			for (ref = &m_debugPtr->incStrongs; *ref;ref = &(*ref)->next) {
				if ((*ref)->id == id) {
					p = *ref;
					*ref = (*ref)->next;
					delete p;
					found = true;
					break;
				}
			}
		}
	
		if (!found) {
			atom_ref_info *ref = new(B_SNS(std::) nothrow) atom_ref_info;
			if (ref) {
				ref->id = id;
				ref->thid = SysCurrentThread();
				ref->when = SysGetRunTime();
				ref->stack.Update(1);
				ref->next = m_debugPtr->decStrongs;
				m_debugPtr->decStrongs = ref;
			}
			
			if (id) {
				sptr<BStringIO> blah(new BStringIO);
				sptr<ITextOutput> io(blah.ptr());
				io->MoveIndent(1);
				io << "SLightAtom: No IncStrong() found for DecStrong() id=" << id << endl;
				do_report(io, B_ATOM_REPORT_FORCE_LONG);
				io->MoveIndent(-1);
				ErrFatalError(blah->String());
			}
		}
	}
#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#define NOTE_CREATE() { if (FastAtomDebugLevel() > 0) LeakChecker()->NoteCreate(); }
#define NOTE_DESTROY() { if (FastAtomDebugLevel() > 0) LeakChecker()->NoteDestroy(); }
#define NOTE_FREE() { if (FastAtomDebugLevel() > 0) LeakChecker()->NoteFree(); }

#endif

/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------*/

/* Common SAtom functionality. */

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

// This is the TLS slot in which we store the base address of
// an atom class when allocating it.  Doing this allows us to
// get that information up to the SAtom constructor.
static SysTSDSlotID gAtomBaseIndex = ~0;

void __initialize_atom()
{
	SysTSDAllocate(&gAtomBaseIndex, NULL, sysTSDAnonymous);
}

void __terminate_atom()
{
}

void SAtom::weak_atom_ptr::Increment(const void* id)
{
#if SUPPORTS_ATOM_DEBUG
	atom->IncWeak(id);
#endif

	if (g_threadDirectFuncs.atomicInc32(&ref_count) == 0) {
		B_INC_WEAK(atom, this);
	}

//	printf("SAtom::weak_atom_ptr::Increment() this=%p id=%p ref_count=%d\n", this, id, ref_count);
}

void SAtom::weak_atom_ptr::Decrement(const void* id)
{
#if SUPPORTS_ATOM_DEBUG
	atom->DecWeak(id);
#endif

//	printf("SAtom::weak_atom_ptr::Decrement() this=%p id=%p ref_count=%d\n", this, id, ref_count);
	if (g_threadDirectFuncs.atomicDec32(&ref_count) == 1) {
		B_DEC_WEAK(atom, this);
#if BUILD_TYPE != BUILD_TYPE_RELEASE
		atom = NULL;
#endif
		free(this);
	}
}

/*!	Upon creation it does NOT hold a reference to the atom --
	you must call SAtom::weak_atom_ptr::Increment() on it first.
*/
SAtom::weak_atom_ptr* SAtom::CreateWeak(const void* cookie) const
{
	weak_atom_ptr* weak = (weak_atom_ptr*)malloc(sizeof(weak_atom_ptr));
	if (weak) {
		weak->ref_count = 0;
		weak->atom = const_cast<SAtom*>(this);
		weak->cookie = const_cast<void*>(cookie);
	}
	return weak;
}

// Remove this base_data from the tls chain.
void SAtom::base_data::unlink()
{
	base_data* pos = (base_data*)g_threadDirectFuncs.tsdGet(gAtomBaseIndex);
	if (pos == this) {
		printf("Unlinking %p from top!\n", this);
		g_threadDirectFuncs.tsdSet(gAtomBaseIndex, (void*)(pos->pending&~1));
		return;
	}

	while (pos->next) {
		base_data* after = (base_data*)(pos->pending&~1);
		if (after == this) {
			printf("Unlinking %p from %p!\n", this, pos);
			pos->next = after->next;
			return;
		}
		pos = pos->next;
	}
}

/*!	SAtom overrides its new and delete operators for its implementation.
	You \em must use these operators and instantiating classes derived
	from SAtom.  Thus a subclass can not implement its own new or delete
	operators, nor can you use inplace-new on a SAtom class.
*/
void* SAtom::operator new(size_t size)
{
	// Allocate extra space before the atom, rounded to 8 bytes.
	base_data* ptr = (base_data*)::operator new(size + sizeof(base_data));
	if (ptr) {
	
		ptr->strongCount = INITIAL_PRIMARY_VALUE;
		ptr->weakCount = 0;

		// Chain with previous allocation.
		base_data* prev = (base_data*)g_threadDirectFuncs.tsdGet(gAtomBaseIndex);
		if (prev) {
			base_data* pos = prev;
#if 0
			printf("Chaining new allocation %p to previous %p\n", ptr, prev);
			while (pos->next) {
				pos = (base_data*)(pos->pending&~1);
				printf("...previous %p\n", pos);
			}
#endif
			ptr->next = (base_data*)(((uint32_t)prev)|1);
		} else {
			ptr->next = NULL;
		}
		ptr->size = size + sizeof(base_data);
	
		PRINT(("Allocate atom base=%p, tls=%ld, off=%p\n",
					ptr, gAtomBaseIndex, ptr+1));
	
#if SUPPORTS_ATOM_DEBUG
		ptr->debugPtr = NULL;
		ptr->new_size = size;
#endif
		g_threadDirectFuncs.tsdSet(gAtomBaseIndex, ptr);
		return ptr+1;
	}

#if _SUPPORTS_EXCEPTIONS
	throw std::bad_alloc();
#else
	return NULL;
#endif
}

void* SAtom::operator new(size_t size, const B_SNS(std::)nothrow_t&) throw()
{
	// Allocate extra space before the atom, rounded to 8 bytes.
	base_data* ptr = (base_data*)::operator new(size + sizeof(base_data), B_SNS(std::)nothrow);
	if (ptr) {
	
		ptr->strongCount = INITIAL_PRIMARY_VALUE;
		ptr->weakCount = 0;

		// Chain with previous allocation.
		base_data* prev = (base_data*)g_threadDirectFuncs.tsdGet(gAtomBaseIndex);
		if (prev) {
			base_data* pos = prev;
#if 0
			printf("Chaining new allocation %p to previous %p\n", ptr, prev);
			while (pos->next) {
				pos = (base_data*)(pos->pending&~1);
				printf("...previous %p\n", pos);
			}
#endif
			ptr->next = (base_data*)(((uint32_t)prev)|1);
		} else {
			ptr->next = NULL;
		}
		ptr->size = size + sizeof(base_data);
	
		PRINT(("Allocate atom base=%p, tls=%ld, off=%p\n",
					ptr, gAtomBaseIndex, ptr+1));
	
#if SUPPORTS_ATOM_DEBUG
		ptr->debugPtr = NULL;
		ptr->new_size = size;
#endif
		g_threadDirectFuncs.tsdSet(gAtomBaseIndex, ptr);
		return ptr+1;
	}

	return NULL;
}

void SAtom::operator delete(void* ptr, size_t size)
{
	if (ptr) {
		// Go back to find the pointer into the atom instance.
		base_data* data = ((base_data*)ptr)-1;
		if (data->atom) {
			data->atom->delete_impl(size);
			return;
		}
		// Atom wasn't constructed, so just free the data.
		data->unlink();
		::operator delete(data);
	}
}

void SAtom::delete_impl(size_t size)
{
	if (weak_count() == 0) {
		// The atom has already been destroyed, goodbye.
		PRINT(("Freeing atom memory %p directly\n", this));
		::operator delete(m_base);
		NOTE_FREE();
		return;
	}
	
	const int32_t c = strong_count();
	if (c != 0 && c != INITIAL_PRIMARY_VALUE) {
		char buf[128];
		sprintf(buf, "SAtom: object [%s] deleted with non-zero (%d) strong references",
				lookup_debug(this, false).String(), c);
		ErrFatalError(buf);
	}

	// Cache object size to later free its memory.
	m_base->size = size;
}

void SAtom::destructor_impl()
{
	if (weak_count()) {
		char buf[128];
		sprintf(buf, "SAtom: object [%s] deleted with non-zero (%d) weak references",
				lookup_debug(this, false).String(), weak_count());
		ErrFatalError(buf);
	}
	term_atom();
}

/*!	The reference counts start out at zero.  You must call
	IncStrong() (or create a sptr<T> to the SAtom) to acquire
	the first reference, at which point InitAtom() will be called. */
SAtom::SAtom()
{
	NOTE_CREATE();
	
	// Look for the memory allocation for this atom.
	base_data* pos = (base_data*)g_threadDirectFuncs.tsdGet(gAtomBaseIndex);
	base_data* prev = NULL;
	while ( pos && (this < ((void*)pos) || this >= ((void*)(((char*)pos)+pos->size))) ) {
		// Not this one...  try the next!
		prev = pos;
		pos = (base_data*)(pos->pending&~1);
	}

	DbgOnlyFatalErrorIf(pos == NULL,
		"Problem found in using SAtom.  When deriving from SAtom don't: override operator new() or delete(), "
		"intantiate on the stack, or mix together multiple classes without virtually inheriting from SAtom.");

#if 0
	if (pos->next || prev) {
		printf("Removing %p from chain prev=%p\n", pos, prev);
	}
#endif

	m_base = pos;
	if (!prev) g_threadDirectFuncs.tsdSet(gAtomBaseIndex, (void*)(pos->pending&~1));
	else prev->next = pos->next;

	pos->atom = this;
	pos->size = 0;
	
	init_atom();
	
	PRINT(("Creating SAtom %p\n", this));
}

/*!	Normally called inside of DecStrong() when the reference count
	goes to zero, but you can change that behavior with FinishAtom(). */
SAtom::~SAtom()
{
	NOTE_DESTROY();
	
	PRINT(("Destroying SAtom %p\n", this));
	if (weak_count() == 0) {
		// Whoops, someone is really deleting us.  Do it all.
		destructor_impl();
	}
}

/*!	After calling this function, both objects share the same
	reference counts and they will both be destroyed once
	that count goes to zero.  The 'target' object must have
	only one strong reference on it -- the attach operation is
	not thread safe for that object (its current references
	will be transfered to the other atom).
	@todo This method is not yet implemented!
*/
status_t SAtom::AttachAtom(SAtom* target)
{
	ErrFatalError("Not implemented yet!");

	DbgOnlyFatalErrorIf(strong_count() != 1, "TieAtoms: 'this' must have one strong reference.");

	// We make no pretense at being thread safe on 'target'.
	// Is it possible to do this in a thread safe way?
	// My brain hurts...

	// Transfer reference counts.
	target->lock_atom();
	g_threadDirectFuncs.atomicAdd32(weak_addr(), target->weak_count());
	g_threadDirectFuncs.atomicAdd32(strong_addr(), target->strong_count());
	*target->weak_addr() = 0;
	*target->strong_addr() = 0;
	transfer_refs(target);
	target->unlock_atom();

	// 'target' now points to 'this' reference counts
	base_data* base = target->m_base;
	target->m_base = m_base;

	return B_OK;

}

/*!	This keeps an atom from actually being freed after its
	last primary reference is removed.  If you are only
	holding a weak reference on the object, you know
	that the memory it points to still exists, but don't
	know what state the object is in.  The optional @a id
	parameter is the memory address of the object holding
	this reference.  It is only used for debugging.
	@see wptr<>
*/
int32_t SAtom::IncWeak(const void *id) const
{
	lock_atom();
	const int32_t r = g_threadDirectFuncs.atomicInc32(weak_addr());
	add_incweak(id);
	unlock_atom();

	return r;
}

/*!	If this is the last weak pointer on it, the object will be
	completely deallocated.  Usually this means simply freeing
	the remaining memory for the SAtom object instance, since
	the destructor was previously called when the last strong
	reference went away.  Some SAtom objects may extend their
	lifetime with FinishAtom(), in which case the object's
	destructor may be called at this point.
	@see IncWeak(), DecStrong(), wptr<>
*/
int32_t SAtom::DecWeak(const void *id) const
{
	lock_atom();
	const int32_t r = g_threadDirectFuncs.atomicDec32(weak_addr());
#if BUILD_TYPE != BUILD_TYPE_RELEASE
	const int32_t sr = *strong_addr();
#endif
	add_decweak(id);
	unlock_atom();

	if (r == 1)	{
		watch_action("DecWeak() [last]");
#if BUILD_TYPE != BUILD_TYPE_RELEASE
		if ((sr != INITIAL_PRIMARY_VALUE) && (sr != 0)) {
			char buf[128];
			sprintf(buf, "SAtom: object [%s] DecWeak() reached zero with %d strongs refs remaining.", lookup_debug(this, false).String(), sr);
			ErrFatalError(buf);
		}
#endif
		if (m_base->size > 0) {
			const_cast<SAtom*>(this)->destructor_impl();
			PRINT(("Freeing atom memory %p after last ref\n", this));
			::operator delete(m_base);
			NOTE_FREE();
		} else {
			if (const_cast<SAtom*>(this)->DeleteAtom(id) == B_OK)
				delete this;
		}
	} else if (r < 1) {
		char buf[128];
		sprintf(buf, "SAtom: object [%s] DecWeak() called more times than IncWeak()", lookup_debug(this, false).String());
		ErrFatalError(buf);
	}
	return r;
}

void SAtom::IncWeakFast() const
{
	g_threadDirectFuncs.atomicInc32(weak_addr());
}

void SAtom::DecWeakFast() const
{
	if (g_threadDirectFuncs.atomicDec32(weak_addr()) != 1) {
		return;
	}

	if (m_base->size > 0) {
		const_cast<SAtom*>(this)->destructor_impl();
		PRINT(("Freeing atom memory %p after last ref\n", this));
		::operator delete(m_base);
		NOTE_FREE();
	} else {
		if (const_cast<SAtom*>(this)->DeleteAtom(NULL) == B_OK)
			delete this;
	}
}

#if 0
static int32_t numIncStrong = 0;
static void count_inc_strong()
{
	if (((g_threadDirectFuncs.atomicInc32(&numIncStrong)+1)%10000) == 0) {
		thread_info thinfo;
		get_thread_info(SysCurrentThread(),&thinfo);
		printf("Have done %ld refs in process %ld\n", numIncStrong, thinfo.team);
	}
}
#endif

/*!	This is the standard reference count -- once it
	transitions to zero, the atom will become invalid.
	An atom starts with a reference count of zero, and
	gets invalidated the first time it transitions from one
	to zero.  The optional \a id parameter is the memory address
	of the object holding this reference.  It is only used
	for debugging.

	The first time you call IncStrong() will result in
	InitAtom() being called.

	@see sptr<>
*/
int32_t SAtom::IncStrong(const void *id) const
{
	//count_inc_strong();

	IncWeak(id);
	
	lock_atom();
	int32_t r = g_threadDirectFuncs.atomicInc32(strong_addr());
	if (r == INITIAL_PRIMARY_VALUE)
		g_threadDirectFuncs.atomicAdd32(strong_addr(), -INITIAL_PRIMARY_VALUE);
	
	if (r == 0) {
		// The incstrong count was at zero, this is an error.
		g_threadDirectFuncs.atomicDec32(strong_addr());
		unlock_atom();
		char buf[128];
		sprintf(buf, "SAtom: object [%s] IncStrong() called after final DecStrong().\nDid you use an atom_ptr inside your constructor?", lookup_debug(this, false).String());
		ErrFatalError(buf);
		DecWeak(id);
		return 0;
	}
	
	add_incstrong(id, r);

	unlock_atom();

	if (r == INITIAL_PRIMARY_VALUE) {
		watch_action("InitAtom()");
		const_cast<SAtom*>(this)->InitAtom();
		r -= INITIAL_PRIMARY_VALUE;
	}
	
	return r;
}

/*!	If this is the last strong pointer on it, FinishAtom() will be called.
*/
int32_t SAtom::DecStrong(const void *id) const
{
	lock_atom();
	const int32_t r = g_threadDirectFuncs.atomicDec32(strong_addr());
	add_decstrong(id);
	unlock_atom();
	
	if (r == 1)	{
		watch_action("FinishAtom()");
		const status_t err = const_cast<SAtom*>(this)->FinishAtom(id);
		if (err == FINISH_ATOM_ASYNC) {
			const_cast<SAtom*>(this)->schedule_async_destructor(id);
			return r;
		}
		if (err == B_OK)
			delete this;
	} else if (r < 1) {
		char buf[128];
		sprintf(buf, "SAtom: object [%s] DecStrong() called more times than IncStrong()", lookup_debug(this, false).String());
		ErrFatalError(buf);
	}
	DecWeak(id);
	return r;
}

void SAtom::IncStrongFast() const
{
	//count_inc_strong();

	IncWeakFast();
	
	if (g_threadDirectFuncs.atomicInc32(strong_addr()) != INITIAL_PRIMARY_VALUE)  {
		return;
	}

	g_threadDirectFuncs.atomicAdd32(strong_addr(), -INITIAL_PRIMARY_VALUE);
	const_cast<SAtom*>(this)->InitAtom();
}

void SAtom::DecStrongFast() const
{
	if (g_threadDirectFuncs.atomicDec32(strong_addr()) == 1) {
		const status_t err = const_cast<SAtom*>(this)->FinishAtom(NULL);
		if (err == FINISH_ATOM_ASYNC) {
			const_cast<SAtom*>(this)->schedule_async_destructor(NULL);
			return;
		}
		if (err == B_OK)
			delete this;
	}
	DecWeakFast();
}

bool SAtom::attempt_inc_strong(const void *id, bool useid) const
{
	if (useid) IncWeak(id);
	else IncWeakFast();
	
	lock_atom();
	
	// Attempt to increment the reference count, without
	// disrupting it if it has already gone to zero.
	int32_t current = strong_count();
	while (current > 0 && current < INITIAL_PRIMARY_VALUE) {
		if (!g_threadDirectFuncs.atomicCompareAndSwap32((volatile uint32_t*)strong_addr(), current, current+1)) {
			// Success!
			break;
		}
		// Someone else changed it before us.
		current = strong_count();
	}
	
	if (current <= 0 || current == INITIAL_PRIMARY_VALUE) {
		unlock_atom();
		// The primary count has gone to zero; if the object hasn't yet
		// been deleted, give it a chance to renew the atom.
		const bool die = m_base->size > 0
						|| (const_cast<SAtom*>(this)->IncStrongAttempted(
							current == INITIAL_PRIMARY_VALUE ? B_ATOM_FIRST_STRONG : 0, id) < B_OK);
		if (die) {
			if (useid) DecWeak(id);
			else DecWeakFast();
			return false;
		}
		
		// IncStrongAttempted() has allowed us to revive the atom, so increment
		// the reference count and continue with a success.
		lock_atom();
		current = g_threadDirectFuncs.atomicInc32(strong_addr());
		// If the primary references count has already been incremented by
		// someone else, the implementor of IncStrongAttempted() is holding
		// an unneeded reference.  So call FinishAtom() here to remove it.
		// (No, this is not pretty.)  Note that we MUST NOT do this if we
		// are in fact acquiring the first reference.
		if (current > 0 && current < INITIAL_PRIMARY_VALUE) {
			unlock_atom();
			if (const_cast<SAtom*>(this)->FinishAtom(id) == B_OK) {
				char buf[128];
				sprintf(buf, "SAtom: object [%s] FinishAtom() must not return B_OK if you implement IncStrongAttempted()", lookup_debug(this, false).String());
				ErrFatalError(buf);
			}
			lock_atom();
		}
	}
	
	if (current == INITIAL_PRIMARY_VALUE)
		g_threadDirectFuncs.atomicAdd32(strong_addr(), -INITIAL_PRIMARY_VALUE);
	
	if (useid) add_incstrong(id, current);

	unlock_atom();
	
	if (current == INITIAL_PRIMARY_VALUE) {
		watch_action("InitAtom()");
		const_cast<SAtom*>(this)->InitAtom();
	}
	return true;
}

/*!	You must already have a weak reference on the atom.
	This function will attempt to add a new strong reference
	to the atom.  It returns true on success, in which case
	a new strong reference has been acquired which you must
	later remove with DecStrong().  Otherwise, the atom is
	left as-is.  Failure only occurs after the atom has already
	had IncStrong() called and then completely released.  That is,
	AttemptIncStrong() will succeeded on a newly created SAtom
	that has never had IncStrong() called on it.
	@see IncStrong()
*/
bool SAtom::AttemptIncStrong(const void *id) const
{
	return attempt_inc_strong(id, true);
}

bool SAtom::AttemptIncStrongFast() const
{
	return attempt_inc_strong(NULL, false);
}

/*!	If this atom has any outstanding strong references, this
	function will remove one of them and return true.  Otherwise
	it leaves the atom as-is and returns false.  Note that successful
	removal of the strong reference may result in the object being
	destroyed.
	
	Trust me, it is useful...  though for very few things.

	@see DecStrong()
*/
bool SAtom::AttemptDecStrong(const void *id) const
{
	lock_atom();

	int32_t r = strong_count();
	while (r > 0) {
		if (!g_threadDirectFuncs.atomicCompareAndSwap32((volatile uint32_t*)strong_addr(), r, r-1)) {
			// Success!
			break;
		}
		// Someone else changed it before us.
		r = strong_count();
	}

	if (r > 0) {
		add_decstrong(id);
		unlock_atom();
		
		if (r == 1)	{
			watch_action("FinishAtom()");
			const status_t err = const_cast<SAtom*>(this)->FinishAtom(id);
			if (err == FINISH_ATOM_ASYNC) {
				const_cast<SAtom*>(this)->schedule_async_destructor(id);
				return true;
			}
			if (err == B_OK)
				delete this;
		}
		DecWeak(id);
		return true;
	}
	unlock_atom();
	
	return false;
}

/*!	This is just like IncStrong(), except that it is not an error to
	call on a SAtom that currently does not have a strong reference.
	If you think you need to use this, think again.
	@see IncStrong<>
*/
int32_t SAtom::ForceIncStrong(const void *id) const
{
	int32_t rw = IncWeak(id);
	ErrFatalErrorIf(rw <= 0, "ForceIncStrong on a deleted atom!\n");
	
	lock_atom();
	int32_t r = g_threadDirectFuncs.atomicInc32(strong_addr());
	if (r == INITIAL_PRIMARY_VALUE)
		g_threadDirectFuncs.atomicAdd32(strong_addr(), -INITIAL_PRIMARY_VALUE);
	
	add_incstrong(id, r);

	unlock_atom();

	if (r == INITIAL_PRIMARY_VALUE || r == 0) {
		watch_action("InitAtom()");
		const_cast<SAtom*>(this)->InitAtom();
		r -= INITIAL_PRIMARY_VALUE;
	}
	
	return r;
}

void SAtom::ForceIncStrongFast() const
{
	IncWeakFast();
	
	int32_t r = g_threadDirectFuncs.atomicInc32(strong_addr());
	if (r == INITIAL_PRIMARY_VALUE) {
		g_threadDirectFuncs.atomicAdd32(strong_addr(), -INITIAL_PRIMARY_VALUE);
		const_cast<SAtom*>(this)->InitAtom();
	} else if (r == 0) {
		const_cast<SAtom*>(this)->InitAtom();
	}
}

/*!	Attempt to acquire a weak reference on the atom.  Unlike
	AttemptIncStrong(), you can \e not call this for the first
	weak reference -- it will fail in that case.  Like ForceIncStrong(),
	if you think you need to use this you should think again.
	@see AttemptIncStrong()
*/
bool SAtom::AttemptIncWeak(const void *id) const
{
	lock_atom();
	
	// Attempt to increment the reference count, without
	// disrupting it if it has already gone to zero.
	int32_t current = weak_count();
	while (current > 0) {
		if (!g_threadDirectFuncs.atomicCompareAndSwap32((volatile uint32_t*)weak_addr(), current, current+1)) {
			// Success!
			break;
		}
		// Someone else changed it before us.
		current = weak_count();
	}
	
	if (current > 0) {
		// Successfully acquired a weak reference.
		add_incweak(id);
	}
	
	unlock_atom();
	
	// Return success/failure based on the previous reference count.
	return current > 0;
}

bool SAtom::AttemptIncWeakFast() const
{
	// Attempt to increment the reference count, without
	// disrupting it if it has already gone to zero.
	int32_t current = weak_count();
	while (current > 0) {
		if (!g_threadDirectFuncs.atomicCompareAndSwap32((volatile uint32_t*)weak_addr(), current, current+1)) {
			// Success!
			break;
		}
		// Someone else changed it before us.
		current = weak_count();
	}
	
	// Return success/failure based on the previous reference count.
	return current > 0;
}

/*!	@note Be careful how you use this information, as it can
	change from "true" to "false" even before you get the result.
*/
bool SAtom::HasStrongPointers() const
{
	return StrongCount() > 0;
}

/*!	@note Be careful how you use this information.  You must already
	hold a strong pointer on the atom, and even then it can change
	from "true" to "false" before you get the result.
*/
bool SAtom::HasManyStrongPointers() const
{
	return StrongCount() > 1;
}

/*!	@note Be careful how you use this information, as it can
	change from "true" to "false" even before you get the result.
*/
bool SAtom::HasWeakPointers() const
{
	return WeakCount() > 0;
}

/*!	@note Be careful how you use this information.  You must already
	hold a weak pointer on the atom, and even then it can change
	from "true" to "false" before you get the result.
*/
bool SAtom::HasManyWeakPointers() const
{
	return WeakCount() > 1;
}

void SAtom::MovePointersBefore(SAtom** newPtr, SAtom** oldPtr, size_t num)
{
	if (num == 1) *newPtr = *oldPtr;
	else memcpy(newPtr, oldPtr, sizeof(SAtom*)*num);
	move_pointers(newPtr, oldPtr, num);
}
			
void SAtom::MovePointersAfter(SAtom** newPtr, SAtom** oldPtr, size_t num)
{
	if (num == 1) *newPtr = *oldPtr;
	else memmove(newPtr, oldPtr, sizeof(SAtom*)*num);
	move_pointers(newPtr, oldPtr, num);
}

/*!	@note Debugging only.  The returned value is no longer valid
	as soon as you receive it.  See HasStrongPointers()
	and HasManyStrongPointers() for safer APIs.
*/
int32_t SAtom::StrongCount() const
{
	lock_atom();
	const int32_t r = strong_count();
	unlock_atom();
	return r < INITIAL_PRIMARY_VALUE ? r : (r-INITIAL_PRIMARY_VALUE);
}

/*!	@note Debugging only.  The returned value is no longer valid
	as soon as you receive it.  See HasStrongPointers()
	and HasManyStrongPointers() for safer APIs.
*/
int32_t SAtom::WeakCount() const
{
	lock_atom();
	const int32_t r = weak_count();
	unlock_atom();
	return r;
}

/*!	You can override it and do any setup you
	need.  Note that you do not need to call the SAtom
	implementation.  (So you can derive from two different
	SAtom implementations and safely call down to both
	of their IncStrong() methods.)
	@see IncStrong()
*/
void SAtom::InitAtom()
{
}

/*!	If you return FINISH_ATOM_ASYNC here, your object's destructor will be called
	asynchronously from the current thread.  This is highly recommend if you must
	acquire a lock in the destructor, to avoid unexpected deadlocks due to things
	like sptr<> going out of scope while a lock is held.

	If you return an error code here, the object's destructor will not be called at
	this point.

	The default implementation will return B_OK, allowing the SAtom destruction to
	proceed as normal.  Don't override this method unless you want some other
	behavior.  Like InitAtom(), you do not need to call the SAtom implementation.
	
	@see DecStrong()
*/
status_t SAtom::FinishAtom(const void* id)
{
	return B_OK;
}

/*!	The \a flags will be B_ATOM_FIRST_STRONG if this is the first
	strong reference ever acquired on the atom.   The default
	implementation returns B_OK if B_ATOM_FIRST_STRONG is set,
	otherwise it returns B_NOT_ALLOWED to make the attempted
	IncStrong() fail.
	
	You can override this to return B_OK when you would like an atom
	to continue allowing primary references.  Note that this also
	requires overriding FinishAtom() to return an error code,
	extending the lifetime of the object.  Like FinishAtom(), you
	do not need to call the SAtom implementation.

	@see AttemptIncStrong(), FinishAtom()
*/
status_t SAtom::IncStrongAttempted(uint32_t flags, const void*)
{
	return (flags&B_ATOM_FIRST_STRONG) != 0 ? B_OK : B_NOT_ALLOWED;
}

/*!	If you override FinishAtom() to not call into SAtom
	(and thus extend the life of your object), then this
	method will be called when its last reference goes
	away.  The default implementation returns B_OK to have
	the object deleted.  You can override this to return
	an error code so that the object is not destroyed.

	This is a very, very unusual feature and requires a lot
	of careful and specific management of objects and
	reference counts to make work.  You probably want to
	leave it as-is.

	@see DecWeak(), FinishAtom()
*/
status_t SAtom::DeleteAtom(const void*)
{
	return B_OK;
}

// This is here because it requires base_data --joeo
#if SUPPORTS_ATOM_DEBUG
size_t SAtom::AtomObjectSize() const {
	return m_base->new_size;
}
#endif

/*--------------------------------------------------------------------------*/

/*!	The reference counts start out at zero.  You must call
	IncStrong() (or create a sptr<T> to the SLightAtom) to acquire
	the first reference, at which point InitAtom() will be called. */
SLightAtom::SLightAtom()
	:	m_strongCount(INITIAL_PRIMARY_VALUE)
#if BUILD_TYPE != BUILD_TYPE_RELEASE
		, m_constCount(0)
#endif
{
	NOTE_CREATE();
	
	init_atom();
	
	PRINT(("Creating SLightAtom %p\n", this));
}

SLightAtom::~SLightAtom()
{
	NOTE_DESTROY();
	
	PRINT(("Destroying SLightAtom %p\n", this));

	term_atom();
}

/*!	This is the standard reference count -- once it
	transitions to zero, the atom will become invalid.
	An atom starts with a reference count of zero, and
	gets invalidated the first time it transitions from one
	to zero.  The optional \a id parameter is the memory address
	of the object holding this reference.  It is only used
	for debugging.

	The first time you call IncStrong() will result in
	InitAtom() being called.

	@see sptr<>
*/
int32_t SLightAtom::IncStrong(const void *id) const
{
#if BUILD_TYPE == BUILD_TYPE_RELEASE
	return const_cast<SLightAtom*>(this)->IncStrong(id);
#else
	lock_atom();
	int32_t r = g_threadDirectFuncs.atomicInc32(strong_addr());
	if (r == INITIAL_PRIMARY_VALUE)
		g_threadDirectFuncs.atomicAdd32(strong_addr(), -INITIAL_PRIMARY_VALUE);
	
	if (r == 0) {
		// The incstrong count was at zero, this is an error.
		g_threadDirectFuncs.atomicDec32(strong_addr());
		unlock_atom();
		char buf[128];
		sprintf(buf, "SLightAtom: object [%s] IncStrong() called after final DecStrong().\nDid you use an atom_ptr inside your constructor?", lookup_debug(this, false).String());
		ErrFatalError(buf);
		return 0;
	}
	
	g_threadDirectFuncs.atomicInc32(&m_constCount);

	add_incstrong(id, r);

	unlock_atom();

	if (r == INITIAL_PRIMARY_VALUE) {
		watch_action("InitAtom()");
		const_cast<SLightAtom*>(this)->InitAtom();
		r -= INITIAL_PRIMARY_VALUE;
	}
	
	return r;
#endif
}

int32_t SLightAtom::IncStrong(const void *id)
{
	lock_atom();
	int32_t r = g_threadDirectFuncs.atomicInc32(strong_addr());
	if (r == INITIAL_PRIMARY_VALUE)
		g_threadDirectFuncs.atomicAdd32(strong_addr(), -INITIAL_PRIMARY_VALUE);
	
	if (r == 0) {
		// The incstrong count was at zero, this is an error.
		g_threadDirectFuncs.atomicDec32(strong_addr());
		unlock_atom();
		char buf[128];
		sprintf(buf, "SLightAtom: object [%s] IncStrong() called after final DecStrong().\nDid you use an atom_ptr inside your constructor?", lookup_debug(this, false).String());
		ErrFatalError(buf);
		return 0;
	}
	
	add_incstrong(id, r);

	unlock_atom();

	if (r == INITIAL_PRIMARY_VALUE) {
		watch_action("InitAtom()");
		const_cast<SLightAtom*>(this)->InitAtom();
		r -= INITIAL_PRIMARY_VALUE;
	}
	
	return r;
}

int32_t SLightAtom::DecStrong(const void *id) const
{
#if BUILD_TYPE == BUILD_TYPE_RELEASE
	return const_cast<SLightAtom*>(this)->DecStrong(id);
#else
	lock_atom();
	const int32_t r = g_threadDirectFuncs.atomicDec32(strong_addr());
	g_threadDirectFuncs.atomicDec32(&m_constCount);
	add_decstrong(id);
	unlock_atom();
	
	if (r == 1)	{
		delete this;
	} else if (r < 1) {
		char buf[128];
		sprintf(buf, "SLightAtom: object [%s] DecStrong() called more times than IncStrong()", lookup_debug(this, false).String());
		ErrFatalError(buf);
	}
	return r;
#endif
}

int32_t SLightAtom::DecStrong(const void *id)
{
	lock_atom();
	const int32_t r = g_threadDirectFuncs.atomicDec32(strong_addr());
	add_decstrong(id);
	unlock_atom();
	
	if (r == 1)	{
		delete this;
	} else if (r < 1) {
		char buf[128];
		sprintf(buf, "SLightAtom: object [%s] DecStrong() called more times than IncStrong()", lookup_debug(this, false).String());
		ErrFatalError(buf);
	}
	return r;
}

void SLightAtom::IncStrongFast() const
{
#if BUILD_TYPE == BUILD_TYPE_RELEASE
	return const_cast<SLightAtom*>(this)->IncStrongFast();
#else
	if (g_threadDirectFuncs.atomicInc32(strong_addr()) == INITIAL_PRIMARY_VALUE) {
		g_threadDirectFuncs.atomicAdd32(strong_addr(), -INITIAL_PRIMARY_VALUE);
		const_cast<SLightAtom*>(this)->InitAtom();
	}
	g_threadDirectFuncs.atomicInc32(&m_constCount);
#endif
}

void SLightAtom::IncStrongFast()
{
	if (g_threadDirectFuncs.atomicInc32(strong_addr()) == INITIAL_PRIMARY_VALUE) {
		g_threadDirectFuncs.atomicAdd32(strong_addr(), -INITIAL_PRIMARY_VALUE);
		const_cast<SLightAtom*>(this)->InitAtom();
	}
}

void SLightAtom::DecStrongFast() const
{
#if BUILD_TYPE == BUILD_TYPE_RELEASE
	return const_cast<SLightAtom*>(this)->DecStrongFast();
#else
	g_threadDirectFuncs.atomicDec32(&m_constCount);
	if (g_threadDirectFuncs.atomicDec32(strong_addr()) == 1) {
		delete this;
	}
#endif
}

void SLightAtom::DecStrongFast()
{
	if (g_threadDirectFuncs.atomicDec32(strong_addr()) == 1) {
		delete this;
	}
}

int32_t SLightAtom::ConstCount() const
{
	return m_constCount;
}

/*!	@note Be careful how you use this information.  You must already
	hold a strong pointer on the atom, and even then it can change
	from "true" to "false" before you get the result.
*/
bool SLightAtom::HasManyStrongPointers() const
{
	return StrongCount() > 1;
}

void SLightAtom::MovePointersBefore(SLightAtom** newPtr, SLightAtom** oldPtr, size_t num)
{
	if (num == 1) *newPtr = *oldPtr;
	else memcpy(newPtr, oldPtr, sizeof(SLightAtom*)*num);
	move_pointers(newPtr, oldPtr, num);
}
			
void SLightAtom::MovePointersAfter(SLightAtom** newPtr, SLightAtom** oldPtr, size_t num)
{
	if (num == 1) *newPtr = *oldPtr;
	else memmove(newPtr, oldPtr, sizeof(SLightAtom*)*num);
	move_pointers(newPtr, oldPtr, num);
}

/*!	@note Debugging only.  The returned value is no longer valid
	as soon as you receive it.
*/
int32_t SLightAtom::StrongCount() const
{
	lock_atom();
	const int32_t r = strong_count();
	unlock_atom();
	return r < INITIAL_PRIMARY_VALUE ? r : (r-INITIAL_PRIMARY_VALUE);
}

/*!	You can override it and do any setup you
	need.  Note that you do not need to call the SLightAtom
	implementation.  (So you can derive from two different
	SLightAtom implementations and safely call down to both
	of their IncStrong() methods.)
	@see IncStrong()
*/
void SLightAtom::InitAtom()
{
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
