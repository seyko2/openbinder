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

#ifndef	_SUPPORT_PARCEL_H
#define	_SUPPORT_PARCEL_H

/*!	@file support/Parcel.h
	@ingroup CoreSupportBinder
	@brief Container for a raw block of data that can be transfered through IPC.
*/

#include <support/SupportDefs.h>
#include <support/IBinder.h>
#include <support/IByteStream.h>
#include <support/ITextStream.h>
#include <support/Vector.h>

#if TARGET_HOST == TARGET_HOST_PALMOS
#include <Kernel.h>
#include <support/KeyID.h>
#endif

BNS(namespace palmos {)
BNS(namespace support {)

/*!	@addtogroup CoreSupportBinder
	@{
*/

class IBinder;

struct binder_ipc_info;
struct flat_binder_object;

struct small_flat_data;
struct large_flat_header;

/*-------------------------------------------------------------*/

//!	Container for a raw block of data that can be transfered through IPC.
class SParcel
{
	public:
		static	SParcel*		GetParcel(void);
		static	void			PutParcel(SParcel *);
	
		typedef	void			(*free_func)(	const void* data,
												ssize_t len,
												void* context);
		typedef	status_t		(*reply_func)(	const SParcel& buffer,
												void* context);
	
								SParcel(ssize_t bufferSize = -1);
								SParcel(sptr<IByteOutput> output,
										sptr<IByteInput> input,
										sptr<IByteSeekable> seek,
										ssize_t bufferSize = -1);
								SParcel(reply_func replyFunc,
										void* replyContext = NULL);
								SParcel(const void* data, ssize_t len,
										free_func freeFunc = NULL,
										void* freeContext = NULL,
										reply_func replyFunc = NULL,
										void* replyContext = NULL);
		virtual					~SParcel();
	
				//!	Return a pointer to the data in this parcel.
				const void*		Data() const;
				
				//!	Return an editable pointer to the data in this parcel.
				void*			EditData();
				
				//!	Return the number of bytes in this parcel, or an error code.
				ssize_t			Length() const;
				
				//!	Return the bytes available for use in this parcel, or an error code.
				ssize_t			Avail() const;
				
				//!	Return the number of Binders that will still fit in the parcel, or an error code.
				/*!	Note that this number is somewhat fuzzy, as the
					real number depends on the kind of binder being written.  We guarantee
					there is enough room to write the returned number of binders of
					any type. */
				ssize_t			AvailBinders() const;
				
				//!	Return the current operating status of the parcel.
				status_t		ErrorCheck() const;
				
				//!	Will PutParcel() be able to return this parcel to the cache?
				bool			IsCacheable() const;

				//!	Set the parcel to reference an external block of data.
				void			Reference(	const void* data, ssize_t len,
											free_func freeFunc = NULL,
											void* context = NULL);
				
				//!	Allocate data in the parcel, copying from somewhere else.
				status_t		Copy(const void* data, ssize_t len);
				
				//!	Allocate data in the parcel, copying from another parcel.
				status_t		Copy(const SParcel& src);
				
				//!	Create space in the parcel for data.
				status_t		Reserve(ssize_t len);
				
				//!	Allocate data in the parcel that you can edit.
				void*			Alloc(ssize_t len);
				
				//!	Modify data in the parcel.
				void*			ReAlloc(ssize_t len);

				//! Release all binders and forget the contents, but don't free the data buffer.
				void			Reset();
				
				//!	Allocate data in the parcel as an array of flattened values.
				status_t		SetValues(const SValue* value1, ...);
				
				//!	Return the number of values in the parcel.
				int32_t			CountValues() const;
				
				//!	Retrieve flattened values from the parcel.
				int32_t			GetValues(int32_t maxCount, SValue* outValues) const;
				
				//!	Transfer ownership of the data from the given SParcel to this one, leaving \param src empty.
				void			Transfer(SParcel* src);
				
				//! Sending replies.   XXX REMOVE.
				//@{
				bool			ReplyRequested() const;
				status_t		Reply();
				//@}

				//!	Deallocating data currently in the parcel.
				void			Free();
				
				off_t			Position() const;
				void			SetPosition(off_t pos);
				status_t		SetLength(ssize_t len);
				
				/*!	@name Typed Data Handling
					These functions read and write typed data in the parcel.
					This data is formatted the same way as SValue's archived
					representation, so it is valid to write a single chunk of data
					through these APIs, which is read back as a generic SValue
					(or vice-versa). */
				//@{

				//!	Write only the header part of a typed data.
				/*!	If the 'amount' is enough to fit into a small data representation,
					only the packed int32 type code is written (with the length encoded
					inside of it).  Otherwise the type code and length is written in
					8 bytes. */
				ssize_t			WriteTypeHeader(type_code type, size_t amount);
				//!	Write header and data.
				/*!	This uses WriteTypeHeader() to write the header, and then writes the
					given data immediately after. */
				ssize_t			WriteTypeHeaderAndData(type_code type, const void *data, size_t amount);
				
				//!	Write fixed-sized marshalled data to parcel.
				/*!	This is the same as WriteTypeHeaderAndData(), except if 'data' is NULL then
					it writes a NULL value. */
				ssize_t			MarshalFixedData(type_code type, const void *data, size_t amount);
				//!	Read fixed-sized marshalled data from parcel.
				/*!	If the data in the parcel matches the given type and size, it will be
					written to 'data'.  Otherwise an error is returned and the parcel must
					be considered invalid. */
				status_t		UnmarshalFixedData(type_code type, void *data, size_t amount);

				//!	Core API for writing a small type header and its data.
				ssize_t			WriteSmallData(uint32_t packedType, uint32_t packedData);
				//!	Core API for writing small flat data structure.
				ssize_t			WriteSmallData(const small_flat_data& flat);
				//! Core API for writing a large type header and its data.
				/*!	If 'data' is NULL, only the header will be written. */
				ssize_t			WriteLargeData(uint32_t packedType, size_t length, const void* data);
				//!	Core API for writing a binder type.
				ssize_t			WriteBinder(const flat_binder_object& val);

				//! Core API for reading the next type header from the parcel.
				ssize_t			ReadSmallData(small_flat_data* out);

				//! Like ReadSmallData(), but will also read object references.
				/*!	If the data being read is an object, this function will
					automatically acquire the appropriate reference on it that
					is owned by 'who'. */
				ssize_t			ReadSmallDataOrObject(small_flat_data* out, const void* who);

				//!	Core API for reading a driver binder type.
				ssize_t			ReadFlatBinderObject(flat_binder_object* out);
				
				//!	Write data by type code.
				/*!	Write the requested typed data into the parcel.  The amount
					written is determined based on the type code -- 'data' must
					have that amount of data. */
				ssize_t			WriteTypedData(type_code type, const void *data);
				//!	Read data by type code.
				/*!	If the given type doesn't match what is currently in the parcel,
					nothing will be read; the object will remain unchanged and a
					type mismatch error will be returned.  If this function fails
					due to a mismatch between the size stored in the type header and
					the amount read, consider the parcel invalid.
					Notice that B_NULL_TYPE can match any type (because a NULL pointer 
					was passed on other side). In this case, the special error code
					B_BINDER_READ_NULL_VALUE will be returned so that clients can react
					if needed. */
				status_t		ReadTypedData(type_code type, void *data);

				//!	If you were to use WriteTypedData() to write this data, how much would it write?
		static	ssize_t			TypedDataSize(type_code type, const void *data);
				
				//! Write an SValue.
				/*!	This is a synomym for val.Archive(parcel). */
				ssize_t			WriteValue(const SValue& val);
				//!	Read an SValue.
				/*!	This is a synonym for SValue::Unarchive(parcel). */
				SValue			ReadValue();

				//!	Skip over a typed value in the parcel, without reading it.
				/*!	Returns B_OK if the value was skipped.  If the data at the current
					location is not a valid typed data, an error is returned in the
					parcel should be considered corrupt. */
				status_t		SkipValue();

				//@}

				/*!	@name Reading and Writing Objects
					This is the high-level object API.  Objects are written as
					structured types, so you can you can mix between reading/writing
					with these functions and with SValue. */
				//@{

		static	ssize_t			BinderSize(const sptr<IBinder>& val);
		static	ssize_t			WeakBinderSize(const wptr<IBinder>& val);

				ssize_t			WriteBinder(const sptr<IBinder>& val);
				ssize_t			WriteWeakBinder(const wptr<IBinder>& val);
				ssize_t			WriteBinder(const small_flat_data& val);
#if TARGET_HOST == TARGET_HOST_PALMOS
				ssize_t			WriteKeyID(const sptr<SKeyID>& val);
#endif

				
				sptr<IBinder>	ReadBinder();
				wptr<IBinder>	ReadWeakBinder();
#if TARGET_HOST == TARGET_HOST_PALMOS
				sptr<SKeyID>	ReadKeyID();
#endif

				//@}

				/*!	@name Raw Data Functions
					These functions do not read/write a type header.  Consider them 'low level'. */
				//@{

				ssize_t			Write(const void* buffer, size_t amount);
				void *			WriteInPlace(size_t amount); //!< Extend the data stream by amount and return a pointer to the begining of the new chunk
				ssize_t			WritePadded(const void* buffer, size_t amount);	//!< Write data, padded appropriated (by 8 bytes).
				ssize_t			WritePadding();	//!< Add padding to ensure next write is at 8-byte boundary
				ssize_t			WriteBool(bool val);
				ssize_t			WriteInt8(int8_t val);
				ssize_t			WriteInt16(int16_t val);
				ssize_t			WriteInt32(int32_t val);
				ssize_t			WriteInt64(int64_t val);
				ssize_t			WriteUInt8(uint8_t val) {return WriteInt8((int8_t)val);}
				ssize_t			WriteUInt16(uint16_t val) {return WriteInt16((int16_t)val);}
				ssize_t			WriteUInt32(uint32_t val) {return WriteInt32((int32_t)val);}
				ssize_t			WriteUInt64(uint64_t val) {return WriteInt64((int64_t)val);}
				ssize_t			WriteFloat(float val);
				ssize_t			WriteDouble(double val);
				ssize_t			WriteString(const SString& val);
				ssize_t			WriteString(const char* val);

				ssize_t			Read(void* buffer, size_t amount);
				const void*		ReadInPlace(size_t amount);
				ssize_t			ReadPadded(void* buffer, size_t amount);
				ssize_t			Drain(size_t amount);
				ssize_t			DrainPadding();	//!< Skip past padding to next 8-byte boundary
				bool			ReadBool();
				int8_t			ReadInt8();
				int16_t			ReadInt16();
				int32_t			ReadInt32();
				int64_t			ReadInt64();
				uint8_t			ReadUInt8() {return (uint8_t)ReadInt8();}
				uint16_t		ReadUInt16() {return (uint16_t)ReadInt16();}
				uint32_t		ReadUInt32() {return (uint32_t)ReadInt32();}
				uint64_t		ReadUInt64() {return (uint64_t)ReadInt64();}
				float			ReadFloat();
				double			ReadDouble();
				SString			ReadString();
				
				ssize_t			Flush();
				ssize_t			Sync();
				
				//@}

				/*!	@name Binder Management
					Retrieving and restoring binder information in the parcel. */
				//@{
				const void*		BinderOffsetsData() const;
				size_t			BinderOffsetsLength() const;
#if TARGET_HOST == TARGET_HOST_PALMOS
				status_t		SetBinderOffsets(	binder_ipc_info const * const offsets,
													size_t count,
													bool takeRefs = true);
#else
				status_t		SetBinderOffsets(	const void* offsets,
													size_t length,
													bool takeRefs = true);
#endif
				//@}
				
				void			PrintToStream(const sptr<ITextOutput>& io, uint32_t flags = 0) const;
	
	protected:
	
		virtual	ssize_t			WriteBuffer(const void* buffer, size_t len) const;
		virtual	ssize_t			ReadBuffer(void* buffer, size_t len);
												
	private:
								SParcel(const SParcel& o);
				SParcel&		operator=(const SParcel& o);
				
				void			do_free();
				void			acquire_binders();
				void			release_binders();
				ssize_t			finish_write(ssize_t amt);
				
				uint8_t*			m_data;
				ssize_t				m_length;
				ssize_t				m_avail;
				
				free_func			m_free;
				void*				m_freeContext;
				
				reply_func			m_reply;
				void*				m_replyContext;
				
				off_t				m_base;
				ssize_t				m_pos;
				sptr<IByteOutput>	m_out;
				sptr<IByteInput>	m_in;
				sptr<IByteSeekable>	m_seek;
				
				uint32_t				m_dirty : 1;
				uint32_t				m_ownsBinders : 1;
				uint32_t				m_reserved: 30;
				
				SVector<size_t>		m_binders;
				
				uint8_t				m_inline[32];
};

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SParcel& value);

/*!	@} */

/*-------------------------------------------------------------*/

BNS(} }) // namespace palmos::support

#endif	// _SUPPORT_PARCEL_H
