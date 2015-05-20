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

#ifndef _SUPPORT_SHARED_BUFFER_H_
#define _SUPPORT_SHARED_BUFFER_H_

/*!	@file support/SharedBuffer.h
	@ingroup CoreSupportUtilities
	@brief Standard representation of a block of shared data that
	supports copy-on-write.
*/

#include <support/Atom.h>
#include <support/SupportDefs.h>
#include <support/atomic.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

//!	Special bits in the SSharedBuffer user count and length fields.
/*!	Note that these magic values are the LOW bits.  This is to help code
	generation of THUMB instructions.  It means that retrieving the real
	value requires a shift instead of a mask. */
enum {

	B_BUFFER_USERS_SHIFT	= 4,		//!< Amount by which to shift user count.
	B_BUFFER_LENGTH_SHIFT	= 1,		//!< Amount by which to shift length.

	//!	Flag indicating SSharedBuffer is in the static text section of an executable.
	/*!	This is part of the reference count of the shared memory buffer. */
	B_STATIC_USERS			= 0x00000001,

	//!	Flag indicating SSharedBuffer is in the buffer pool.
	/*!	This is part of the reference count of the shared memory buffer. */
	B_POOLED_USERS			= 0x00000002,

	//!	Flag indicating SSharedBuffer has additional meta-data before the buffer.
	/*!	This is part of the length of the shared memory buffer. */
	B_EXTENDED_BUFFER	= 0x00000001
};

// *************************************************************************
//!	A chunk of memory that supports copy-on-write semantics.
/*! A class for managing raw blocks of memory, which also includes support
	for sharing among multiple users with copy-on-write semantics.
	
	To correctly use the class, you will usually keep a const SSharedBuffer*
	to your buffer, and call Edit() when you would like to modify its data.
	This ensures you follow the correct copy-on-write semantics, not allowing
	you to modify a buffer if it has more than one user.

	\note Because you do not own this memory, you can not place anything
	in it for which you must do some cleanup when freeing its memory.
	(For example, the memory can not contain pointer to binders or any
	other SAtom-derived class, which will need a Release() call when the
	shared buffer it is in goes away.)
*/
class SSharedBuffer
{
public:
			//!	Create a new buffer of the given size.
			/*!	A buffer starts out with a user count of 1; call DecUsers() to free it. */
	static	SSharedBuffer*	Alloc(size_t length);
			
			//!	Return a read-only version of the buffer's data.
	inline	const void*		Data() const;
			//!	Return a read/write version of the buffer's data.
			/*!	\note You MUST call Edit() to ensure you are the sole
				owner of this buffer, before modifying its data.
				\sa Edit()
			*/
	inline	void*			Data();
			//!	Return the number of bytes in this buffer.
	inline	size_t			Length() const;
			
			//!	Inverse of Data().
			/*! Given a data pointer returned by Data(),
				returns the SSharedBuffer that the pointer came from.  This
				exists largely for the use of SString, which likes to store
				just the pointer to its string data.  In general, it's better
				to store a pointer to the SSharedBuffer, and use Data() to
				access its data.
			 */
	static inline	const SSharedBuffer* BufferFromData(const void *data);

			//!	Add a user to the buffer.
			void			IncUsers() const;
			//!	Remove a user from the buffer.
			/*!	If you are the last user, the buffer is deallocated. */
			void			DecUsers() const;
			//!	Return the number of users this buffer has.
			/*!	\note Because of threading issues, you can only safely use
				this value to check if the user count is 1 or greater than 1;
				and if it is greater than one, you can't assume it will stay
				that way. */
	inline	int32_t			Users() const;
			
			//!	Given a read-only buffer, start modifying it.
			/*!	If the buffer currently has multiple users, it will be
				copied and a new one returned to you.
			*/
			SSharedBuffer*	Edit(size_t newLength) const;

			//! Edit() without changing size.
			SSharedBuffer*	Edit() const;

			//!	Place the shared buffer into the process's buffer pool.
			/*!	If a buffer of this data doesn't already exist in the
				pool, this buffer is modified and placed in the pool.
				Otherwise, your reference on this buffer is released,
				a reference on the on in the pool added and that buffer
				returned to you. */
	const	SSharedBuffer*	Pool() const;

			//!	Perform a comparison of the data in two buffers.
			/*!	@result < 0 if \a this is less than \a other; > 0 if
					\a this is greater than \a other; 0 if they are
					identical. */
			int32_t			Compare(const SSharedBuffer* other) const;

			//!	Convert this SSharedBuffer to buffering mode.
			/*!	Buffering mode is more efficient for a serious of
				operations that will increase or decrease the size of
				the buffer each time.  Call EndBuffering() when done. */
			SSharedBuffer*	BeginBuffering();
			//!	Stop buffering mode started with BeginBuffering().
			SSharedBuffer*	EndBuffering();

			//!	Is BeginBuffering() mode active?
			bool			Buffering() const;
			//!	How big is the actual size of the buffer?
			size_t			BufferSize() const;

			//!	Synonym for IncUsers(), for compatibility with sptr<>.
			void			IncStrong(const void* ) const	{ IncUsers(); }
			//!	Synonym for DecUsers(), for compatibility with sptr<>.
			void			DecStrong(const void* ) const	{ DecUsers(); }
			//!	Synonym for IncUsers(), for compatibility with sptr<>.
			void			IncStrongFast() const			{ IncUsers(); }
			//!	Synonym for DecUsers(), for compatibility with sptr<>.
			void			DecStrongFast() const			{ DecUsers(); }

			// These are used for static values.  See StaticValue.h.
	typedef	void			(*inc_ref_func)();
	typedef	void			(*dec_ref_func)();

private:
	inline					SSharedBuffer() { }
	inline					~SSharedBuffer() { }
	
	struct	extended_info;
			extended_info*	get_extended_info() const;
			bool			unpool() const;
			void			do_delete() const;

							SSharedBuffer(const SSharedBuffer& o);

	static	SSharedBuffer*	AllocExtended(extended_info* prototype, size_t length);
			SSharedBuffer*	Extend(size_t length) const;

			void			SetBufferSize(size_t size);
			void			SetLength(size_t len);

	/* *******************************************************
	 * WARNING
	 *
	 * The size or layout of SSharedBuffer cannot be changed.
	 * In order to do some important optimizations, some code
	 * assumes this layout. For eg, see support/StaticValue.h.
	 *
	 * *******************************************************/

	mutable	int32_t			m_users;
			size_t			m_length;
#if defined(_MSC_VER)
			char			m_data[0];
#endif
};

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

inline const void* SSharedBuffer::Data() const
{
	return this+1;
}

inline void* SSharedBuffer::Data()
{
	return this+1;
}

inline const SSharedBuffer* SSharedBuffer::BufferFromData(const void *data)
{
	return ((static_cast<const SSharedBuffer *>(data)) - 1);
}

inline size_t SSharedBuffer::Length() const
{
	return m_length>>B_BUFFER_LENGTH_SHIFT;
}

inline int32_t SSharedBuffer::Users() const
{
	return m_users>>B_BUFFER_USERS_SHIFT;
}

/*-------------------------------------------------------------*/
// We need to specialize SSharedBuffer, because
// raw SSharedBuffer alread have a count of 1

template<> inline
sptr<SSharedBuffer>::sptr(SSharedBuffer* p)
{
	m_ptr = p;
}

template<> inline
sptr<SSharedBuffer>& sptr<SSharedBuffer>::operator = (SSharedBuffer* p)
{
	if (m_ptr) B_DEC_STRONG(m_ptr, this);
	m_ptr = p;
	return *this;
}


#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif	/* _SUPPORT_SHARED_BUFFER_H_ */
