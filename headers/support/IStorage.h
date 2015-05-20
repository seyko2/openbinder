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

#ifndef	_SUPPORT_ISTORAGE_INTERFACE_H
#define	_SUPPORT_ISTORAGE_INTERFACE_H

/*!	@file support/IStorage.h
	@ingroup CoreSupportDataModel
	@brief Random-access interface to a block of raw data.
*/

#include <support/SupportDefs.h>
#include <support/IInterface.h>
#include <support/Binder.h>
#include <sys/uio.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

/*-------------------------------------------------------------*/
/*------- IStorage Class --------------------------------------*/

//! Raw storage read and write transaction codes.
enum {
	B_WRITE_AT_TRANSACTION		= 'WRAT',
	B_READ_AT_TRANSACTION		= 'RDAT'
};

//! This is the contents of a B_READ_AT_TRANSACTION parcel.
/*!	The reply parcel contains a ssize_t followed by the raw data. */
struct read_at_transaction
{
	off_t	position;
	size_t	size;
};

//!	This is the front of a B_WRITE_AT_TRANSACTION parcel; it
//!	is immediately followed by the data to be written.
/*!	The reply parcel contains a single ssize_t indicating the
	number of bytes actually written, or an error code.
*/
struct write_at_transaction
{
	off_t	position;
};

//!	Random-access interface to a block of raw data.
class IStorage : public IInterface
{
	public:

		B_DECLARE_META_INTERFACE(Storage)

				//!	Return the total number of bytes in the store.
		virtual	off_t		Size() const = 0;
				//!	Set the total number of bytes in the store.
		virtual	status_t	SetSize(off_t size) = 0;

				//!	Read the bytes described by \a iovec from location \a position in the storage.
				/*!	Returns the number of bytes actually read, or a
					negative error code. */
		virtual	ssize_t		ReadAtV(off_t position, const struct iovec *vector, ssize_t count) = 0;
		
				//!	Convenience for reading a vector of one buffer.
				ssize_t		ReadAt(off_t position, void* buffer, size_t size);
				
				//!	Write the bytes described by \a iovec at location \a position in the storage.
				/*!	Returns the number of bytes actually written,
					or a negative error code. */
		virtual	ssize_t		WriteAtV(off_t position, const struct iovec *vector, ssize_t count) = 0;
		
				//!	Convenience for reading a vector of one buffer.
				ssize_t		WriteAt(off_t position, const void* buffer, size_t size);
		
				//!	Make sure all data in the storage is written to its physical device.
				/*!	Returns B_OK if the data is safely stored away, else
					an error code. */
		virtual	status_t	Sync() = 0;
};

/*-----------------------------------------------------------------*/

/*!	@} */

class BnStorage : public BnInterface<IStorage>
{
public:
		virtual	status_t		Transact(uint32_t code, SParcel& data, SParcel* reply = NULL, uint32_t flags = 0);
		
protected:
		inline					BnStorage() : BnInterface<IStorage>() { }
		inline					BnStorage(const SContext& c) : BnInterface<IStorage>(c) { }
		inline virtual			~BnStorage() { }
		
		virtual	status_t		HandleEffect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out);
	
private:
								BnStorage(const BnStorage& o);	// no implementation
		BnStorage&				operator=(const BnStorage& o);	// no implementation
};

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

inline ssize_t IStorage::ReadAt(off_t position, void *buffer, size_t size)
{
	iovec v;
	v.iov_base = buffer;
	v.iov_len = size;
	return ReadAtV(position, &v,1);
}

inline ssize_t IStorage::WriteAt(off_t position, const void *buffer, size_t size)
{
	iovec v;
	v.iov_base = const_cast<void*>(buffer);
	v.iov_len = size;
	return WriteAtV(position, &v,1);
}

/*--------------------------------------------------------------*/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif	// _SUPPORT_ISTORAGE_INTERFACE_H
