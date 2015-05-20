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

#ifndef _SUPPORT_ATOM_H
#define _SUPPORT_ATOM_H

/*!	@file support/Atom.h
	@ingroup CoreSupportUtilities

	@brief Basic reference-counting classes, and smart pointer templates
	for working with them.
*/

#include <support/SupportDefs.h>
#include <new>
#include <typeinfo>

#if _SUPPORTS_NAMESPACE || _REQUIRES_NAMESPACE
namespace std {
#endif
	struct nothrow_t;
#if _SUPPORTS_NAMESPACE || _REQUIRES_NAMESPACE
}
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {
#endif
	struct atom_debug;
#if _SUPPORTS_NAMESPACE
} }	// palmos::osp
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities Utility Classes and Functions
	@ingroup CoreSupport
	@brief Reference counting, data containers, and other tools.
	@{
*/

// forward reference
class ITextOutput;
class SAtomTracker;
template <class TYPE> class sptr;
template <class TYPE> class wptr;

class SString;
template <class TYPE> class SVector;
template <class TYPE> class SSortedVector;

/**************************************************************************************/

//!	Flags for SAtom debugging reports
enum {
	B_ATOM_REPORT_FORCE_LONG		= 0x0001,	//!< Print full stack crawls.
	B_ATOM_REPORT_FORCE_SHORT		= 0x0002,	//!< Print one-line stack crawls.
	B_ATOM_REPORT_FORCE_SUMMARY		= 0x0003,	//!< Print only holder summary.
	B_ATOM_REPORT_FORCE_MASK		= 0x000f,	//!< Mask for report type.
	
	B_ATOM_REPORT_REMOVE_HEADER		= 0x0010	//!< Don't print atom header.
};

//! Flags passed to SAtom::IncStrongAttempted()
enum {
	B_ATOM_FIRST_STRONG				= 0x0001,	//!< This is the first strong reference ever.
};

class SAtom;
class SLightAtom;

//!	Base class for a reference-counted object.
/*!	SAtom is used with most objects that are reference counted.
	In particular, IBinder and all Binder interfaces include SAtom,
	meaning all Binder objects are intrinsically reference counted
	using the SAtom facilities.

	The reference counting semantics are designed so that you will
	very rarely, if ever, have to directly deal with reference count
	management.  Instead, you use the smart pointer classes 
	@link sptr sptr<T> @endlink and @link wptr wptr<T> @endlink
	(for "strong pointer" and "weak pointer", respectively)
	to create pointers to SAtom objects, which will automatically
	manage the reference count for you.  For example, the following
	function creates, assigns, and returns objects, in all cases
	maintaining proper reference counts:
@code
sptr< MyClass > RefFunction(sptr< MyClass >* inOutClass)
{
	sptr< MyClass > old = *inOutClass;
	*inOutClass = new MyClass;
	return old;
}
@endcode

	When deriving directly from SAtom to make your own reference
	counted object, you should usually use virtual inhertance so
	that your class can be properly mixed in with other
	SAtom-derived classes (ensuring there is only one SAtom object,
	with its single set of reference counts, for the entire final
	class):
@code
class MyClass : public virtual SAtom
{
};
@endcode

	Weak pointers allow you to hold a reference on an SAtom object
	that does not prevent it from being destroyed.  The only thing
	you can do with a weak pointer is compare its address against
	other weak or strong pointers (to check if they are the same
	object), and use the wptr::promote() operation to attempt to
	obtain a strong pointer.  The promotion will fail (by returning
	a sptr<T> containing NULL) if the object has already been destroyed
	(because all of its strong references
	have gone away).  This mechanism is very useful to maintain
	parent/child relationships, where the child holds a weak pointer
	up to its parent:
@code
class MyClass : public virtual SAtom
{
private:
	// NOTE: Not thread safe!
	wptr<MyClass> m_parent;
	sptr<MyClass> m_child;

public:
	void SetChild(const sptr<MyClass>& child)
	{
		m_child = child;
		child->SetParent(this);
	}

	void SetParent(const sptr<MyClass>& parent)
	{
		m_parent = parent;
	}

	sptr<MyClass> Parent() const
	{
		return m_parent.promote();
	}
};
@endcode

	The SAtom class includes a rich debugging facility for
	tracking down reference counting bugs, and especially
	leaks.  See @ref SAtomDebugging for more information on
	it.

	@nosubgrouping
*/
class SAtom
{
public:
	// --------------------------------------------------------------
	/*!	@name Custom New and Delete Operators
		SAtom must use its own new and delete operators to perform
		the bookkeeping necessary to support weak pointers. */
	//@{
			void*			operator new(size_t size);
			void			operator delete(void* ptr, size_t size);
			void*			operator new(size_t size, const B_SNS(std::)nothrow_t&) throw();
	//@}
	
protected:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Object initialization, destruction, and lifetime management. */
	//@{

			//! Construct a new SAtom.
							SAtom();
			//!	Destructor called when it is time to destroy the object.  Never call yourself!
	virtual					~SAtom();
	
			//!	Called the first time a strong reference is acquired.  All significant object initialization should go here.
	virtual	void			InitAtom();
	
			enum {
				FINISH_ATOM_ASYNC	= 0x0001,	//!< Use with FinishAtom() to perform asynchronous destruction.
			};

			//!	Objects can optionally override this function to extend the lifetime of an atom (past the last strong reference).
	virtual	status_t		FinishAtom(const void* id);
	
			//!	Called during IncStrong() after an atom has been released.
	virtual	status_t		IncStrongAttempted(uint32_t flags, const void* id);
	
			//!	Called after last DecRefs() when the life of an atom is extended.
	virtual	status_t		DeleteAtom(const void* id);
		
	//@}

public:
	// --------------------------------------------------------------
	/*!	@name Reference Counting
		Basic APIs for managing the reference count of an SAtom object. */
	//@{

			//!	Increment the atom's strong pointer count.
			int32_t			IncStrong(const void* id) const;
			
			//!	Decrement the atom's strong pointer count.
			int32_t			DecStrong(const void* id) const;
			
			//!	Optimized version of IncStrong() for release builds.
			void			IncStrongFast() const;
			//!	Optimized version of DecStrong() for release builds.
			void			DecStrongFast() const;
			
			//!	Represents a safe weak reference on an SAtom object.
			/*!	This is needed because, when holding a weak reference, you can only
				safely call directly on to the SAtom object.  You can't call to
				a derived class, because with virtual inheritance it will need to
				go through the vtable to find the SAtom object, and that vtable
				may no longer exist due to its code being unloaded.  This structure
				is used by wptr<> and others to only use 4 bytes for a pointer,
				where it needs to have a pointer to both the real (derived) class
				and its base SAtom address. */
			struct weak_atom_ptr
			{
				//!	Add another reference to the weak_atom_ptr.
				void	Increment(const void* id);
				//!	Remove a reference.  If this is the last, the structure is freed.
				void	Decrement(const void* id);

				int32_t	ref_count;	//!< Number of weak references this structure holds.
				SAtom*	atom;		//!< That SAtom object we hold the reference on.
				void*	cookie;		//!< Typically the "real" derived object you are working with.
			};

			//! Create a weak reference to the atom.
			weak_atom_ptr*	CreateWeak(const void* cookie) const;
		
			//!	Increment the atom's weak pointer count.
			int32_t			IncWeak(const void* id) const;
			
			//!	Decrement the atom's weak pointer count.
			int32_t			DecWeak(const void* id) const;
			
			//!	Optimized version of IncWeak() for release builds.
			void			IncWeakFast() const;
			//!	Optimized version of DecWeak() for release builds.
			void			DecWeakFast() const;
		
			//!	Try to acquire a strong pointer on this atom.
			bool			AttemptIncStrong(const void* id) const;
			//!	Optimized version of AttemptIncStrong() for release builds.
			bool			AttemptIncStrongFast() const;
	
			//!	Perform a DecStrong() if there any strong pointers remaining.
			bool			AttemptDecStrong(const void* id) const;
			
			//!	Acquire a strong pointer on the atom, even if it doesn't have one.
			int32_t			ForceIncStrong(const void* id) const;
			//!	Optimized version of ForceIncStrong() for release builds.
			void			ForceIncStrongFast() const;
		
			//!	Try to acquire a weak pointer on this atom.
			bool			AttemptIncWeak(const void* id) const;
			//!	Optimized version of AttemptIncWeak() for release builds.
			bool			AttemptIncWeakFast() const;
			
			//!	Tie the reference counts of two atoms together.
			status_t		AttachAtom(SAtom* target);

			//!	@deprecated Backwards compatibility.  Do not use.
	inline	int32_t			Acquire(const void* id) const { return IncStrong(id); }
			//!	@deprecated Backwards compatibility.  Do not use.
	inline	int32_t			Release(const void* id) const { return DecStrong(id); }
			//!	@deprecated Backwards compatibility.  Do not use.
	inline	int32_t		 	IncRefs(const void* id) const { return IncWeak(id); }
			//!	@deprecated Backwards compatibility.  Do not use.
	inline	int32_t		 	DecRefs(const void* id) const { return DecWeak(id); }
			//!	@deprecated Backwards compatibility.  Do not use.
	inline	bool			AttemptAcquire(const void* id) const { return AttemptIncStrong(id); }
			//!	@deprecated Backwards compatibility.  Do not use.
	inline	bool			AttemptRelease(const void* id) const { return AttemptDecStrong(id); }
			//!	@deprecated Backwards compatibility.  Do not use.
	inline	int32_t			ForceAcquire(const void* id) const { return ForceIncStrong(id); }
		
	//@}

	// --------------------------------------------------------------
	/*!	@name Reference Count Access
		Retrieve the current reference count on the object.  Generally
		only used for debugging. */
	//@{

			//!	Return true if this atom has any strong acquires on it.
			bool			HasStrongPointers() const;
			
			//!	Return true if this atom has more than one strong acquire on it.
			bool			HasManyStrongPointers() const;
			
			//!	Return true if this atom has any weak acquires on it.
			bool			HasWeakPointers() const;
			
			//!	Return true if this atom has more than one weak acquire on it.
			bool			HasManyWeakPointers() const;
	
	//@}

	// --------------------------------------------------------------
	/*!	@name Reference Count Owner
		SAtom provides a rich reference count debugging facility described
		in @ref SAtomDebugging.  For it to work, each reference on an SAtom must
		have a unique object ID (usually a memory address) of the entity that
		"owns" that reference.  You should use these APIs in situations
		where that object ID changes, so that when atom debugging is
		enabled others will be able to properly track their references. */
	//@{

			//!	Change an owner ID associated with this atom.
			void			RenameOwnerID(const void* newID, const void* oldID) const;
			
			//!	Perform a memcpy() of an array of SAtom*, updating owner IDs as needed.
	static	void			MovePointersBefore(SAtom** newPtr, SAtom** oldPtr, size_t num = 1);
			
			//!	Perform a memmove() of an array of SAtom*, updating owner IDs as needed.
	static	void			MovePointersAfter(SAtom** newPtr, SAtom** oldPtr, size_t num = 1);
	
	//@}

	// --------------------------------------------------------------
	/*!	@name Debugging
		These methods allow you to work with the SAtom reference count
		debugging and leak tracking mechanism as described in
		@ref SAtomDebugging.  They are only implemented
		on debug builds when the atom debugging facility is enabled.
		In other cases, they are a no-op. */
	//@{

			//!	Return the number of strong pointers currently on the atom.
			int32_t			StrongCount() const;

			//!	Return the number of weak pointers currently on the atom.
			int32_t			WeakCount() const;
			
			//!	Print information state and references on this atom to @a io.
			void			Report(const sptr<ITextOutput>& io, uint32_t flags=0) const;
			
			//!	Start a new SAtom/SLightAtom leak context and returns its identifier.
	static	int32_t			MarkLeakReport();
	
			//!	Print information about SAtom/SLightAtoms in a leak context to an output stream.
	static	void			LeakReport(	const sptr<ITextOutput>& io, int32_t mark=0, int32_t last=-1,
										uint32_t flags=0);
			//!	Print information about SAtom/SLightAtoms in a leak context to standard output.
	static	void			LeakReport(	int32_t mark=0, int32_t last=-1,
										uint32_t flags=0);
			
			//!	Check for atom existance and acquire string reference.
	static	bool			ExistsAndIncStrong(SAtom* atom);
			//!	Check for atom existance and acquire weak reference.
	static	bool			ExistsAndIncWeak(SAtom* atom);

			//! Return a set of the type names for all SAtom/SLightAtoms that currently exist.
	static	void			GetActiveTypeNames(SSortedVector<SString>* outNames);

			//! Return all existing atoms that are the given type name.
	static	void			GetAllWithTypeName(	const char* typeName,
												SVector<wptr<SAtom> >* outAtoms = NULL,
												SVector<sptr<SLightAtom> >* outLightAtoms = NULL);

			//!	Register a particular class type for watching SAtom/SLightAtom operations.
	static	void			StartWatching(const B_SNS(std::) type_info* type);
			//!	Remove a watching of a class type originally supplied to StartWatching().
	static	void			StopWatching(const B_SNS(std::) type_info* type);
			
			//! Prints the size of the object.  Sometimes useful to know.
			size_t			AtomObjectSize() const;
	
	//@}

private:
			friend class	SAtomTracker;
			friend class	SLightAtom;
			class			AsyncDestructor;
			friend class	AsyncDestructor;
			
							SAtom(const SAtom&);
			
			void			destructor_impl();
			void			delete_impl(size_t size);
			
			bool			attempt_inc_strong(const void* id, bool useid) const;


			


			// ----- private debugging support -----
			
	static	void			move_pointers(SAtom** newPtr, SAtom** oldPtr, size_t num);
			
			void			init_atom();
			void			term_atom();
			
			void			lock_atom() const;
			void			unlock_atom() const;
			
			int32_t*		strong_addr() const;
			int32_t*		weak_addr() const;
			
			int32_t			strong_count() const;
			int32_t			weak_count() const;
			
			void			watch_action(const char* description) const;
			void			do_report(const sptr<class ITextOutput>& io, uint32_t flags) const;
			
			void			add_incstrong(const void* id, int32_t cnt) const;
			void			add_decstrong(const void* id) const;
			void			add_incweak(const void* id) const;
			void			add_decweak(const void* id) const;
	
			void			add_incstrong_raw(const void* id, int32_t cnt) const;
			void			add_decstrong_raw(const void* id) const;
			void			add_incweak_raw(const void* id) const;
			void			add_decweak_raw(const void* id) const;
	
			void			transfer_refs(SAtom* from);

			void			schedule_async_destructor(const void* id);
			void			do_async_destructor(const void* id);

			struct base_data;
			base_data*		m_base;
};

/**************************************************************************************/

//!	A variation of SAtom that only supports strong reference counts.
/*!	This can be useful when you want to avoid the overhead of the
	more complex / richer SAtom class.  This class only requires 8
	bytes of overhead (the reference count + vtable) in addition to
	the normal malloc overhead, as opposed to the 24 bytes required
	by SAtom.  Reference count management is also slightly more
	efficient, since there is only one reference count to maintain.

	You will often @e not use virtual inheritance with this class, since
	it is usually used to manage internal data structures where you
	don't have to worry about someone else wanting to mix together
	two different reference-counted classes.

	@nosubgrouping
*/
class SLightAtom
{
protected:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Object initialization, destruction, and lifetime management. */
	//@{

			//! Construct a new SAtom.
							SLightAtom();
			//!	Destructor called when it is time to destroy the object.  Never call yourself!
	virtual					~SLightAtom();
	
			//!	Called the first time a strong reference is acquired.  All significant object initialization should go here.
	virtual	void			InitAtom();

	//@}

public:
	// --------------------------------------------------------------
	/*!	@name Reference Counting
		Basic APIs for managing the reference count of an SLightAtom object. */
	//@{

			//!	Increment the atom's strong pointer count.
			int32_t			IncStrong(const void* id) const;
			
			//!	Decrement the atom's strong pointer count.
			int32_t			DecStrong(const void* id) const;
			
			//! Optimized version of IncStrong() for release builds.
			void			IncStrongFast() const;
			//! Optimized version of DecStrong() for release builds.
			void			DecStrongFast() const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Const Reference Counting
		These are for debugging.  We use them to identify const vs.
		non-const references, which can be used to do sanity checks. */
	//@{

			int32_t			IncStrong(const void* id);
			int32_t			DecStrong(const void* id);
			void			IncStrongFast();
			void			DecStrongFast();
			int32_t			ConstCount() const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Reference Count Owner
		SAtom provides a rich reference count debugging facility described
		in @ref SAtomDebugging.  For it to work, each reference on an SLightAtom must
		have a unique object ID (usually a memory address) of the entity that
		"owns" that reference.  You should use these APIs in situations
		where that object ID changes, so that when atom debugging is
		enabled others will be able to properly track their references. */
	//@{

			//!	Return true if this atom has more than one strong acquire on it.
			bool			HasManyStrongPointers() const;
			
			//!	Change an owner ID associated with this atom.
			void			RenameOwnerID(const void* newID, const void* oldID) const;
			
			//!	Perform a memcpy() of an array of SLightAtom*, updating owner IDs as needed.
	static	void			MovePointersBefore(SLightAtom** newPtr, SLightAtom** oldPtr, size_t num = 1);
			
			//!	Perform a memmove() of an array of SLightAtom*, updating owner IDs as needed.
	static	void			MovePointersAfter(SLightAtom** newPtr, SLightAtom** oldPtr, size_t num = 1);
	
	//@}

	// --------------------------------------------------------------
	/*!	@name Debugging
		These methods allow you to work with the SLightAtom reference count
		debugging and leak tracking mechanism as described in
		@ref SAtomDebugging.  They are only implemented
		on debug builds when the atom debugging facility is enabled.
		In other cases, they are a no-op.
		
		Also see the SAtom debugging functions for more features (SAtom
		and SLightAtom share the same back-end debugging state). */
	//@{

			//!	Return the number of strong/weak pointers currently on the atom.
			int32_t			StrongCount() const;
			
			//!	Print information state and references on this atom to @a io.
			void			Report(const sptr<ITextOutput>& io, uint32_t flags=0) const;
			
			//!	Check for atom existance and acquire strong reference.
	static	bool			ExistsAndIncStrong(SLightAtom* atom);

	//@}

private:
			friend class	SAtomTracker;
			
							SLightAtom(const SLightAtom&);
			
			// ----- private debugging support -----
			
	static	void			move_pointers(SLightAtom** newPtr, SLightAtom** oldPtr, size_t num);
			
			void			init_atom();
			void			term_atom();
			
			void			lock_atom() const;
			void			unlock_atom() const;
			
			int32_t*		strong_addr() const;
			
			int32_t			strong_count() const;
			
			void			watch_action(const char* description) const;
			void			do_report(const sptr<class ITextOutput>& io, uint32_t flags) const;
			
			void			add_incstrong(const void* id, int32_t cnt) const;
			void			add_decstrong(const void* id) const;
	
			void			add_incstrong_raw(const void* id, int32_t cnt) const;
			void			add_decstrong_raw(const void* id) const;
	
			union {
				mutable	int32_t								m_strongCount;
				mutable	BNS(::palmos::osp::) atom_debug*	m_debugPtr;
			};
			// XXX this should be moved to m_debugPtr, probably.
			mutable int32_t									m_constCount;
};

/**************************************************************************************/

//!	A minimalistic variation of SAtom/SLightAtom that only supports strong reference counts
/*! SLimAtom adds only 4 bytes of overhead and no vtable. SLimAtom doesn't support ATOM_DEBUG
	(not yet at least).

	Usage:
@code
class type_to_atomize : public SLimAtom< type_to_atomize >
{
};
@endcode

	Be sure to call the SLimAtom<type_to_atomize> default constructor in your implementation.	

	@nosubgrouping
*/
template <class T>
class SLimAtom
{
public:
	//!	Constructor -- initialize reference count to zero.
	inline SLimAtom() : m_strongCount(0) { }

	//!	Increment reference count.
	inline void IncStrongFast() const { SysAtomicInc32(&m_strongCount); }

	//!	Decrement reference count, deleting object when it goes back to zero.
	inline void DecStrongFast() const { if (SysAtomicDec32(&m_strongCount) == 1) { delete static_cast<const T*>(this); } }

	//!	Version of IncStrongFast() for compatibility with sptr<T>.
	inline void IncStrong(const void *) const {	IncStrongFast(); }

	//!	Version of DecStrongFast() for compatibility with sptr<T>.
	inline void DecStrong(const void *) const {	DecStrongFast(); }

	typedef T type;	// this is needed to placate ADS!
protected:

	//!	Destroyed when the reference count goes to zero.  Do not manually delete yourself!
	inline ~SLimAtom() { }
	
private:
	mutable volatile int32_t m_strongCount;
};


/**************************************************************************************/

/*!	@name Reference Counting Macros

	These are macros for incremementing and decrementing reference
	counts, which use a special fast path on release builds.  You
	would be much happier ignoring that these exist, and instead
	using the sptr<> and wptr<> smart pointers below. */
//@{
#if BUILD_TYPE == BUILD_TYPE_DEBUG
#define B_INC_STRONG(atom, who) atom->IncStrong(who)
#define B_ATTEMPT_INC_STRONG(atom, who) atom->AttemptIncStrong(who)
#define B_FORCE_INC_STRONG(atom, who) atom->ForceIncStrong(who)
#define B_DEC_STRONG(atom, who) atom->DecStrong(who)
#define B_INC_WEAK(atom, who) atom->IncWeak(who)
#define B_ATTEMPT_INC_WEAK(atom, who) atom->AttemptIncWeak(who)
#define B_DEC_WEAK(atom, who) atom->DecWeak(who)
#else
#define B_INC_STRONG(atom, who) atom->IncStrongFast()
#define B_ATTEMPT_INC_STRONG(atom, who) atom->AttemptIncStrongFast()
#define B_FORCE_INC_STRONG(atom, who) atom->ForceIncStrongFast()
#define B_DEC_STRONG(atom, who) atom->DecStrongFast()
#define B_INC_WEAK(atom, who) atom->IncWeakFast()
#define B_ATTEMPT_INC_WEAK(atom, who) atom->AttemptIncWeakFast()
#define B_DEC_WEAK(atom, who) atom->DecWeakFast()
#endif
//@}

/**************************************************************************************/

//!	Smart-pointer template that maintains a strong reference on a reference counted class.
/*!	Can be used with SAtom, SLightAtom, SLimAtom, and SSharedBuffer.

	For the most part a sptr<T> looks and feels like a raw C++ pointer.  However,
	currently sptr<> does not have a boolean conversion operator, so to check
	whether a sptr is NULL you will need to write code like this:
@code
if (my_sptr == NULL) ...
@endcode
*/
template <class TYPE>
class sptr
{
public:
		//!	Initialize to NULL pointer.
		sptr();
		//!	Initialize from a raw pointer.
		sptr(TYPE* p);

		//!	Assignment from a raw pointer.
		sptr<TYPE>& operator =(TYPE* p);

		//!	Initialize from another sptr.
		sptr(const sptr<TYPE>& p);
		//!	Assignment from another sptr.
		sptr<TYPE>& operator =(const sptr<TYPE>& p);
		//!	Initialization from a strong pointer to another type of SAtom subclass (type conversion).
		template <class NEWTYPE> sptr(const sptr<NEWTYPE>& p);
		//!	Assignment from a strong pointer to another type of SAtom subclass (type conversion).
		template <class NEWTYPE> sptr<TYPE>& operator =(const sptr<NEWTYPE>& p);

		//!	Release strong reference on object.
		~sptr();

		//!	Dereference pointer.
		TYPE& operator *() const;
		//!	Member dereference.
		TYPE* operator ->() const;
		
		//!	Return the raw pointer of this object.
		/*!	Keeps the object and leaves its reference count as-is.  You normally
			don't need to use this, and instead can use the -> and * operators. */
		TYPE* ptr() const;
		//!	Clear (set to NULL) and return the raw pointer of this object.
		/*! You now own its strong reference and must manually call DecStrong(). */
		TYPE* detach();
		//!	Return true if ptr() is NULL.
		/*!	@note It is preferred to just compare against NULL, that is:
			@code
if (my_atom == NULL) ...
			@endcode */
		bool is_null() const;

		//!	Retrieve edit access to the object.
		/*!	Use this with SSharedBuffer objects to request edit access to the
			buffer.  If the buffer needs to be copied, the sptr<> will be updated
			to point to the new buffer.  Returns a raw pointer to the buffer that
			you can modify. */
		TYPE* edit();
		//!	Version of edit() that allows you to change the size of the shared buffer.
		TYPE* edit(size_t size);

		// Give comparison operators access to our pointer.
		#define COMPARE_FRIEND(op)								\
			bool operator op (const TYPE* p2) const;			\
			bool operator op (const sptr<TYPE>& p2) const;		\
			bool operator op (const wptr<TYPE>& p2) const;		\
		
		COMPARE_FRIEND(==)
		COMPARE_FRIEND(!=)
		COMPARE_FRIEND(<=)
		COMPARE_FRIEND(<)
		COMPARE_FRIEND(>)
		COMPARE_FRIEND(>=)
		
		#undef COMPARE_FRIEND

private:
		friend class wptr<TYPE>;
		
		// Special constructor for more efficient wptr::promote().
		sptr(SAtom::weak_atom_ptr* weak, bool);
		
		TYPE *m_ptr;
};

/**************************************************************************************/

//!	Smart-pointer template that maintains a weak reference on an SAtom class.
/*!	You should always use this class when working with weak SAtom pointers,
	because it restricts what you can do with the referenced objects to only
	those things that are safe.  For example, you can't call any methods on
	the object; to call a method, you must first call promote() to get a valid
	sptr<> from which you can then call the method.
*/
template <class TYPE>
class wptr
{
public:
		//!	Initialize to NULL pointer.
		wptr();
		//!	Initialize to given pointer.
		wptr(TYPE* p);

		//!	Assignment from a raw pointer.
		wptr<TYPE>& operator =(TYPE* p);

		//!	Initialize from another wptr.
		wptr(const wptr<TYPE>& p);
		//!	Initialize from a sptr.
		wptr(const sptr<TYPE>& p);
		//!	Assignment from another wptr.
		wptr<TYPE>& operator =(const wptr<TYPE>& p);
		//!	Assignment from a sptr.
		wptr<TYPE>& operator =(const sptr<TYPE>& p);
		//!	Initialization from a weak pointer to another type of SAtom subclass.
		template <class NEWTYPE> wptr(const wptr<NEWTYPE>& p);
		//!	Initialization from a strong pointer to another type of SAtom subclass.
		template <class NEWTYPE> wptr(const sptr<NEWTYPE>& p);
		//!	Assignment from a strong pointer.
		template <class NEWTYPE> wptr<TYPE>& operator =(const sptr<NEWTYPE>& p);
		//! Assigment from another weak pointer.
		template <class NEWTYPE> wptr<TYPE>& operator =(const wptr<NEWTYPE>& p);
		
		//!	Release weak reference on object.
		~wptr();
		
		//!	Retrieve raw pointer.
		/*!	@attention This is ONLY safe if you know that
			the atom is designed to remain valid after all strong
			pointers are removed, or that you will only access the
			safe parts of SAtom. */
		TYPE* unsafe_ptr_access() const;
		
		//!	Attempt to promote this secondary reference to a primary reference.
		/*!	The returned sptr<> will be NULL if it failed. */
		const sptr<TYPE> promote() const;
		
		#define COMPARE_FRIEND(op)								\
			bool operator op (const TYPE* p2) const;			\
			bool operator op (const sptr<TYPE>& p2) const;		\
			bool operator op (const wptr<TYPE>& p2) const;		\
		
		COMPARE_FRIEND(==)
		COMPARE_FRIEND(!=)
		COMPARE_FRIEND(<=)
		COMPARE_FRIEND(<)
		COMPARE_FRIEND(>)
		COMPARE_FRIEND(>=)
		
		#undef COMPARE_FRIEND

		//! Explicitly increment reference count of this atom.
		/*! Should very rarely be used -- normally you want to let
			wptr<> take care of the reference count for you.
			@note This function directly modifies the weak reference
			count of the object by calling SAtom::IncWeak().  It
			does @em not change the reference count of the
			SAtom::weak_atom_ptr being held by the wptr<>. */
		void inc_weak(const void* id) const;
		//! Explicitly decrement reference count of this atom.
		/*! Should very rarely be used -- normally you want to let
			wptr<> take care of the reference count for you.
			@note This function directly modifies the weak reference
			count of the object by calling SAtom::DecWeak().  It
			does @em not change the reference count of the
			SAtom::weak_atom_ptr being held by the wptr<>. */
		void dec_weak(const void* id) const;
	
		//!	Retrieve the internal weak pointer object.
		/*!	Should very rarely be used. */
		SAtom::weak_atom_ptr* get_weak_atom_ptr() const;
		//!	Manually set the internal weak pointer object.
		/*!	Should very rarely be used. */
		void set_weak_atom_ptr(SAtom::weak_atom_ptr* weak);

private:
		friend class sptr<TYPE>;
		SAtom::weak_atom_ptr* m_ptr;
};

/**************************************************************************************/

//!	Compile-time check to see whether a type conversion is valid.
template <class FROM, class TO>
class TypeConversion
{
	static char Test(const TO&);
	static FROM* t;
public:
	static void Assert() { (void)sizeof(Test(*t)); }
};


//!	Convenience to cast between sptr<> objects of different types.
template<class NEWTYPE, class TYPE> inline
sptr<NEWTYPE>&
sptr_reinterpret_cast(const sptr<TYPE>& v)
{
	return *(sptr<NEWTYPE> *)( &v );
}

//!	Convenience to cast between wptr<> objects of different types.
template<class NEWTYPE, class TYPE> inline
wptr<NEWTYPE>&
wptr_reinterpret_cast(const wptr<TYPE>& v)
{
	return *(wptr<NEWTYPE> *)( &v );
}


/**************************************************************************************/

// A zillion kinds of comparison operators.
#define COMPARE(op)																				\
template<class TYPE> inline																		\
bool sptr<TYPE>::operator op (const TYPE* p2) const												\
	{ return ptr() op p2; }																		\
template<class TYPE> inline																		\
bool sptr<TYPE>::operator op (const sptr<TYPE>& p2) const										\
	{ return ptr() op p2.ptr(); }																\
template<class TYPE> inline																		\
bool sptr<TYPE>::operator op (const wptr<TYPE>& p2) const										\
	{ return ptr() op p2.unsafe_ptr_access(); }													\
template<class TYPE> inline																		\
bool wptr<TYPE>::operator op (const TYPE* p2) const												\
	{ return unsafe_ptr_access() op p2; }														\
template<class TYPE> inline																		\
bool wptr<TYPE>::operator op (const sptr<TYPE>& p2) const										\
	{ return unsafe_ptr_access() op p2.ptr(); }													\
template<class TYPE> inline																		\
bool wptr<TYPE>::operator op (const wptr<TYPE>& p2) const										\
	{ return unsafe_ptr_access() op p2.unsafe_ptr_access(); }									\

COMPARE(==)
COMPARE(!=)
COMPARE(<=)
COMPARE(<)
COMPARE(>)
COMPARE(>=)

#undef COMPARE

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

/* ----------------- sptr Implementation ------------------*/

template<class TYPE> inline
sptr<TYPE>::sptr()								{ m_ptr = NULL; }
template<class TYPE> inline
sptr<TYPE>::sptr(TYPE* p)						{ if ((m_ptr=p) != NULL) B_INC_STRONG(p, this); }

template<class TYPE> inline
sptr<TYPE>& sptr<TYPE>::operator =(TYPE* p)
{
	if (p) B_INC_STRONG(p, this);
	if (m_ptr) B_DEC_STRONG(m_ptr, this);
	m_ptr = p;
	return *this;
}

template<class TYPE> inline
sptr<TYPE>::sptr(const sptr<TYPE>& p)			{ if ((m_ptr=p.m_ptr) != NULL) B_INC_STRONG(m_ptr, this); }
template<class TYPE> inline
sptr<TYPE>& sptr<TYPE>::operator =(const sptr<TYPE>& p)
{
	if (p.ptr()) B_INC_STRONG(p, this);
	if (m_ptr) B_DEC_STRONG(m_ptr, this);
	m_ptr = p.ptr();
	return *this;
}
template<class TYPE> template<class NEWTYPE> inline
sptr<TYPE>::sptr(const sptr<NEWTYPE>& p)
{
	TypeConversion<NEWTYPE, TYPE>::Assert();
	if ((m_ptr=p.ptr()) != NULL) B_INC_STRONG(m_ptr, this);
}
template<class TYPE> template<class NEWTYPE> inline
sptr<TYPE>& sptr<TYPE>::operator =(const sptr<NEWTYPE>& p)
{
	TypeConversion<NEWTYPE, TYPE>::Assert();
	if (p.ptr()) B_INC_STRONG(p, this);
	if (m_ptr) B_DEC_STRONG(m_ptr, this);
	m_ptr = p.ptr();
	return *this;
}

template<class TYPE> inline
sptr<TYPE>::~sptr()								{ if (m_ptr) B_DEC_STRONG(m_ptr, this); }

template<class TYPE> inline
sptr<TYPE>::sptr(SAtom::weak_atom_ptr* p, bool)
{
 	m_ptr = (p && B_ATTEMPT_INC_STRONG(p->atom, this)) ? reinterpret_cast<TYPE*>(p->cookie) : NULL;
//	m_ptr = (p && B_ATTEMPT_INC_STRONG(p->atom, this)) ? dynamic_cast<TYPE*>(p->cookie) : NULL;
}

template<class TYPE> inline
TYPE & sptr<TYPE>::operator *() const			{ return *m_ptr; }
template<class TYPE> inline
TYPE * sptr<TYPE>::operator ->() const			{ return m_ptr; }
template<class TYPE> inline
TYPE * sptr<TYPE>::ptr() const					{ return m_ptr; }
template<class TYPE> inline
TYPE * sptr<TYPE>::detach()						{ TYPE* p = m_ptr; m_ptr = NULL; return p; }
template<class TYPE> inline
bool sptr<TYPE>::is_null() const				{ return m_ptr == NULL; }

template<class TYPE> inline
TYPE * sptr<TYPE>::edit()
{
	TYPE* p = m_ptr;
	if (p != NULL) {
		p = p->Edit();
		m_ptr = p;
	}
	return p;
}

template<class TYPE> inline
TYPE * sptr<TYPE>::edit(size_t size)
{
	TYPE* p = m_ptr;
	if (p != NULL) {
		p = p->Edit(size);
		m_ptr = p;
	}
	return p;
}

/* ----------------- wptr Implementation ------------------*/

template<class TYPE> inline wptr<TYPE>::wptr()
{
	m_ptr = NULL;
}

template<class TYPE> inline wptr<TYPE>::wptr(TYPE* p)
{
	if (p) {
		m_ptr = p->CreateWeak(p);
		if (m_ptr != NULL) m_ptr->Increment(this);
	} else m_ptr = NULL;
}

template<class TYPE> inline wptr<TYPE>& wptr<TYPE>::operator =(TYPE *p)
{
	SAtom::weak_atom_ptr* weak = NULL;
	if (p) {
		weak = p->CreateWeak(p);
		if (weak != NULL) weak->Increment(this);
	}
	if (m_ptr) m_ptr->Decrement(this);
	m_ptr = weak;
	return *this;
}

template<class TYPE> inline SAtom::weak_atom_ptr* wptr<TYPE>::get_weak_atom_ptr() const
{
	return m_ptr;
}

template<class TYPE> inline void wptr<TYPE>::set_weak_atom_ptr(SAtom::weak_atom_ptr* weak)
{
	weak->Increment(this);
	if (m_ptr != NULL) m_ptr->Decrement(this);
	m_ptr = weak;
}

template<class TYPE> inline wptr<TYPE>::wptr(const wptr<TYPE>& p)
{
	if ((m_ptr=p.m_ptr) != NULL) m_ptr->Increment(this);
}

template<class TYPE> inline wptr<TYPE>::wptr(const sptr<TYPE>& p)
{
	if (p.ptr()) {
		m_ptr = p->CreateWeak(p.ptr());
		if (m_ptr != NULL) m_ptr->Increment(this);
	} else m_ptr = NULL;
}

template<class TYPE> inline wptr<TYPE>& wptr<TYPE>::operator =(const wptr<TYPE>& p)
{
	if (p.m_ptr) p.m_ptr->Increment(this);
	if (m_ptr) m_ptr->Decrement(this);
	m_ptr = p.m_ptr;
	return *this;
}

template<class TYPE> inline wptr<TYPE>& wptr<TYPE>::operator =(const sptr<TYPE>& p)
{
	SAtom::weak_atom_ptr* weak = NULL;
	if (p.ptr()) {
		weak = p->CreateWeak(p.ptr());
		if (weak != NULL) weak->Increment(this);
	}
	if (m_ptr) m_ptr->Decrement(this);
	m_ptr = weak;
	return *this;
}

template<class TYPE> template<class NEWTYPE> inline
wptr<TYPE>::wptr(const wptr<NEWTYPE>& p)
{
	TypeConversion<NEWTYPE, TYPE>::Assert();
	if (p.m_ptr)
	{
		m_ptr = p->CreateWeak((NEWTYPE*)p.m_ptr);
		if (m_ptr != NULL) m_ptr->Increment(this);
	} else m_ptr = NULL;
}

template<class TYPE> template<class NEWTYPE> inline
wptr<TYPE>::wptr(const sptr<NEWTYPE>& p)
{
	TypeConversion<NEWTYPE, TYPE>::Assert();
	if (p.ptr())
	{
		m_ptr = p->CreateWeak((NEWTYPE*)p.ptr());
		if (m_ptr != NULL) m_ptr->Increment(this);
	} else m_ptr = NULL;
}

template<class TYPE> template<class NEWTYPE> inline
wptr<TYPE>& wptr<TYPE>::operator =(const sptr<NEWTYPE> &p)
{
	TypeConversion<NEWTYPE, TYPE>::Assert();
	SAtom::weak_atom_ptr* weak = NULL;
	if (p.ptr()) {
		weak = p->CreateWeak((NEWTYPE*)p.ptr());
		if (weak != NULL) weak->Increment(this);
	}
	if (m_ptr) m_ptr->Decrement(this);
	m_ptr = weak;
	return *this;
}
template<class TYPE> template<class NEWTYPE> inline
wptr<TYPE>& wptr<TYPE>::operator =(const wptr<NEWTYPE> &p)
{
	TypeConversion<NEWTYPE, TYPE>::Assert();
	SAtom::weak_atom_ptr* weak = NULL;
	if (p.m_ptr) {
		weak = p->CreateWeak((NEWTYPE*)p.m_ptr);
		if (weak != NULL) weak->Increment(this);
	}
	if (m_ptr) m_ptr->Decrement(this);
	m_ptr = weak;
	return *this;
}

template<class TYPE> inline
wptr<TYPE>::~wptr()
{
	if (m_ptr) m_ptr->Decrement(this);
}

template<class TYPE> inline
TYPE * wptr<TYPE>::unsafe_ptr_access() const
{
	return (m_ptr != NULL) ? reinterpret_cast<TYPE*>(m_ptr->cookie) : NULL;
}

template<class TYPE> inline
const sptr<TYPE> wptr<TYPE>::promote() const
{
	return sptr<TYPE>(m_ptr, true);
}

template<class TYPE> inline
void wptr<TYPE>::inc_weak(const void* id) const
{
	if (m_ptr) B_INC_WEAK(m_ptr->atom, id);
}

template<class TYPE> inline
void wptr<TYPE>::dec_weak(const void* id) const
{
	if (m_ptr) B_DEC_WEAK(m_ptr->atom, id);
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif /* _SUPPORT_ATOM_H */
