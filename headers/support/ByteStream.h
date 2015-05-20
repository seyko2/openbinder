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

#ifndef	_SUPPORT_BYTESTREAM_H
#define	_SUPPORT_BYTESTREAM_H

/*!	@file support/ByteStream.h
	@ingroup CoreSupportDataModel
	@brief A BByteStream converts byte stream operations in to
	IStorage operations.
	
	In other words, it provides byte input, output, and seeking interfaces to an
	underlying random-access storage.
*/

#include <support/SupportDefs.h>
#include <support/IByteStream.h>
#include <support/IStorage.h>

#include <sys/uio.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*---------------------------------------------------------------------*/
/*------- Local Streams -----------------------------------------------*/

// These must be here (instead of in IByteStream.h) because many low-level
// parts of the system want to know about the byte stream interfaces, but
// BBinder (which the local implementation depends on) must itself know about
// those low-level pieces.  Circular dependencies are bad!

/*-----------------------------------------------------------------*/

class BnByteInput : public BnInterface<IByteInput>
{
public:
		virtual	status_t		Link(const sptr<IBinder>& to, const SValue &bindings, uint32_t flags = 0);
		virtual	status_t		Unlink(const wptr<IBinder>& from, const SValue &bindings, uint32_t flags = 0);
		virtual	status_t		Effect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out);
		virtual	status_t		Transact(uint32_t code, SParcel& data, SParcel* reply = NULL, uint32_t flags = 0);
		
protected:
		inline					BnByteInput() : BnInterface<IByteInput>() { }
		inline					BnByteInput(const SContext& c) : BnInterface<IByteInput>(c) { }
		inline virtual			~BnByteInput() { }
		
		virtual	status_t		HandleEffect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out);
	
private:
								BnByteInput(const BnByteInput& o);	// no implementation
		BnByteInput&			operator=(const BnByteInput& o);	// no implementation
};

/*-----------------------------------------------------------------*/

class BnByteOutput : public BnInterface<IByteOutput>
{
public:
		virtual	status_t		Link(const sptr<IBinder>& to, const SValue &bindings, uint32_t flags = 0);
		virtual	status_t		Unlink(const wptr<IBinder>& from, const SValue &bindings, uint32_t flags = 0);
		virtual	status_t		Effect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out);
		virtual	status_t		Transact(uint32_t code, SParcel& data, SParcel* reply = NULL, uint32_t flags = 0);
		
protected:
		inline					BnByteOutput() : BnInterface<IByteOutput>() { }
		inline					BnByteOutput(const SContext& c) : BnInterface<IByteOutput>(c) { }
		inline virtual			~BnByteOutput() { }
		
		virtual	status_t		HandleEffect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out);
	
private:
								BnByteOutput(const BnByteOutput& o);	// no implementation
		BnByteOutput&			operator=(const BnByteOutput& o);		// no implementation
};

/*-----------------------------------------------------------------*/

class BnByteSeekable : public BnInterface<IByteSeekable>
{
public:
		virtual	status_t		Link(const sptr<IBinder>& to, const SValue &bindings, uint32_t flags = 0);
		virtual	status_t		Unlink(const wptr<IBinder>& from, const SValue &bindings, uint32_t flags = 0);
		virtual	status_t		Effect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out);
		virtual	status_t		Transact(uint32_t code, SParcel& data, SParcel* reply = NULL, uint32_t flags = 0);
		
protected:
		inline					BnByteSeekable() : BnInterface<IByteSeekable>() { }
		inline					BnByteSeekable(const SContext& c) : BnInterface<IByteSeekable>(c) { }
		inline virtual			~BnByteSeekable() { }
		
		virtual	status_t		HandleEffect(const SValue &in, const SValue &inBindings, const SValue &outBindings, SValue *out);
	
private:
								BnByteSeekable(const BnByteSeekable& o);	// no implementation
		BnByteSeekable&			operator=(const BnByteSeekable& o);			// no implementation
};

/*!	@addtogroup CoreSupportDataModel
	@{
*/

/*---------------------------------------------------------------------*/
/*------- BByteStream Class -------------------------------------------*/

//!	Byte streams on top of an IStorage.
/*!	Implements IByteInput, IByteOutput, and IByteSeekable on top of
	a random access IStorage interface. */
class BByteStream : public BnByteInput, public BnByteOutput, public BnByteSeekable
{
public:

							BByteStream(const sptr<IStorage>& store);
	
	virtual	SValue			Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags = 0);
	
	virtual	ssize_t			ReadV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);
	virtual	ssize_t			WriteV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);
	virtual	status_t		Sync();
	
	virtual off_t			Seek(off_t position, uint32_t seek_mode);
	virtual	off_t			Position() const;

protected:
							BByteStream(IStorage *This);

	virtual					~BByteStream();

private:
							BByteStream(const BByteStream&);

			off_t			m_pos;
			IStorage *		m_store;
};

//!	Read-only byte streams on top of an IStorage.
/*!	Implements IByteInput and IByteSeekable on top of
	a random access IStorage interface. */
class BReadOnlyStream : public BnByteInput, public BnByteSeekable
{
public:

							BReadOnlyStream(const sptr<IStorage>& store);
	
	virtual	SValue			Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags = 0);
	
	virtual	ssize_t			ReadV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);
	
	virtual off_t			Seek(off_t position, uint32_t seek_mode);
	virtual	off_t			Position() const;

protected:
							BReadOnlyStream(IStorage *This);

	virtual					~BReadOnlyStream();

private:
							BReadOnlyStream(const BByteStream&);

			off_t			m_pos;
			IStorage *		m_store;
};

//!	Write-only byte streams on top of an IStorage.
/*!	Implements IByteOutput and IByteSeekable on top of
	a random access IStorage interface. */
class BWriteOnlyStream : public BnByteOutput, public BnByteSeekable
{
public:

							BWriteOnlyStream(const sptr<IStorage>& store);
	
	virtual	SValue			Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags = 0);
	
	virtual	ssize_t			WriteV(const struct iovec *vector, ssize_t count, uint32_t flags = 0);
	virtual	status_t		Sync();
	
	virtual off_t			Seek(off_t position, uint32_t seek_mode);
	virtual	off_t			Position() const;

protected:
							BWriteOnlyStream(IStorage *This);

	virtual					~BWriteOnlyStream();

private:
							BWriteOnlyStream(const BByteStream&);

			off_t			m_pos;
			IStorage *		m_store;
};

/*-------------------------------------------------------------*/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_BYTESTREAM_H */
