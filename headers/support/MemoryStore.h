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

#ifndef	_SUPPORT_MEMORYSTORE_H
#define	_SUPPORT_MEMORYSTORE_H

/*!	@file support/MemoryStore.h
	@ingroup CoreSupportDataModel
	@brief IStorage implementations for physical memory.

	Also mixes in BByteStream for convenience (one
	usually wants a streaming interface to memory).

	@note This should be replaced with new implementations
	on top of BStreamDatum.
*/

#include <support/IStorage.h>
#include <support/ByteStream.h>
#include <support/Locker.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

/*-------------------------------------------------------------------*/

//!	Generic implementation of storage and byte streams on a block of memory.
/*!	This class can be used as-is on an existing pieces of memory (which
	is @b very dangerous due to reference counting), or subclasses can
	provide their own memory management semantics (such as BMallocStore).

	@todo Change this to be based off of BStreamDatum.
*/
class BMemoryStore : public BnStorage, public BByteStream
{
public:

						BMemoryStore();
						BMemoryStore(const BMemoryStore &);
						BMemoryStore(void *data, size_t size);
						BMemoryStore(const void *data, size_t size);
	
			BMemoryStore&operator=(const BMemoryStore &);
			
			bool		operator<(const BMemoryStore &) const;
			bool		operator<=(const BMemoryStore &) const;
			bool		operator==(const BMemoryStore &) const;
			bool		operator!=(const BMemoryStore &) const;
			bool		operator>=(const BMemoryStore &) const;
			bool		operator>(const BMemoryStore &) const;
			
			//!	Direct access to buffer size.
			/*!	The same as Size(), but it can be convenient when you know
				you are dealing with a BMemoryStore (and thus don't need
				a full off_t to contain the size). */
			size_t		BufferSize() const;
			//!	Direct access to the buffer.
			const void *Buffer() const;

			status_t	AssertSpace(size_t newSize);
			status_t	Copy(const BMemoryStore &);
			int32_t		Compare(const BMemoryStore &) const;
	
	virtual	SValue		Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags = 0);

	virtual	off_t		Size() const;
	virtual	status_t	SetSize(off_t position);

	virtual	ssize_t		ReadAtV(off_t position, const struct iovec *vector, ssize_t count);
	virtual	ssize_t		WriteAtV(off_t position, const struct iovec *vector, ssize_t count);
	virtual	status_t	Sync();

			void		SetBlockSize(size_t blocksize);
		
protected:
	virtual				~BMemoryStore();
			void		Reset();
			

private:
	virtual	void *		MoreCore(void *oldBuf, size_t newSize);
	virtual	void		FreeCore(void *oldBuf);
			
			size_t		m_length;
			char*		m_data;
			bool		m_readOnly;
};

/*-------------------------------------------------------------------*/

//!	BMemoryStore that performs memory allocations on the heap.
/*!	@todo Change this to be based off of BStreamDatum.
*/
class BMallocStore : public BMemoryStore
{
public:
	
						BMallocStore();
						BMallocStore(const BMemoryStore &);

			void		SetBlockSize(size_t blocksize);

protected:
	virtual				~BMallocStore();
		
private:
						BMallocStore(const BMallocStore& o);

	virtual	void *		MoreCore(void *oldBuf, size_t newSize);
	virtual	void		FreeCore(void *oldBuf);
	
			size_t		m_blockSize;
			size_t		m_mallocSize;
};

/*-------------------------------------------------------------*/

//!	@deprecated Use BValueDatum instead.
/*!	This is an old implementation of byte streams on a value.
	It is flawed in some very significant ways, you should use
	the BValueDatum class instead. */
class BValueStorage : public BMemoryStore
{
public:
	BValueStorage(SValue* value, SLocker* lock, const sptr<SAtom>& object);

	SValue Value() const;
	
	virtual	status_t	SetSize(off_t position);

	virtual	ssize_t		ReadAtV(off_t position, const struct iovec *vector, ssize_t count);
	virtual	ssize_t		WriteAtV(off_t position, const struct iovec *vector, ssize_t count);
	virtual	status_t	Sync();

protected:
	virtual ~BValueStorage();
	
private:
	virtual	void* MoreCore(void *oldBuf, size_t newSize);
	virtual	void FreeCore(void *oldBuf);

	SValue* m_value;
	SLocker* m_lock;
	sptr<SAtom> m_atom;
	bool m_editing;
};

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

inline bool BMemoryStore::operator<(const BMemoryStore &o) const
{
	return Compare(o) < 0;
}

inline bool BMemoryStore::operator<=(const BMemoryStore &o) const
{
	return Compare(o) <= 0;
}

inline bool BMemoryStore::operator==(const BMemoryStore &o) const
{
	return Compare(o) == 0;
}

inline bool BMemoryStore::operator!=(const BMemoryStore &o) const
{
	return Compare(o) != 0;
}

inline bool BMemoryStore::operator>=(const BMemoryStore &o) const
{
	return Compare(o) >= 0;
}

inline bool BMemoryStore::operator>(const BMemoryStore &o) const
{
	return Compare(o) > 0;
}

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_MEMORYSTORE_H */
