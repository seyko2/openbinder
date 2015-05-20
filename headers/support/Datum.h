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

#ifndef _SUPPORT_DATUM_H
#define _SUPPORT_DATUM_H

/*!	@file support/Datum.h
	@ingroup CoreSupportDataModel
	@brief Convenience for calling the IDatum interface.
*/

#include <support/IDatum.h>
#include <support/IByteStream.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

//!	Convenience class for using an IDatum.
/*!	This class makes it easier to work with the dual
	nature of IDatum (SValue vs. Open()) by providing
	new APIs for retrieving the datum value that are
	more robust for the client.

	@nosubgrouping
*/
class SDatum
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, copying, comparing, etc. */
	//@{
			//!	Create a new, empty datum.
						SDatum();
			//!	Retrieve a datum from a generic SValue.
			/*!	If the SValue contains an IDatum object, that will be used
				directly.  Otherwise, the SValue will be used
				as a plain blob of data, as if it has been retrieved
				from IDatum::Value(). */
						SDatum(const SValue& value);
			//!	Retrieve a datum from an IBinder, casting to an IDatum interface.
						SDatum(const sptr<IBinder>& binder);
			//!	Initialize directly from an IDatum.
						SDatum(const sptr<IDatum>& datum);
			//!	Copy from another SDatum.
						SDatum(const SDatum& datum);
			//!	Release reference on IDatum.
						~SDatum();

			//!	Replace this SDatum with @a o.
	SDatum&				operator=(const SDatum& o);

	bool				operator<(const SDatum& o) const;
	bool				operator<=(const SDatum& o) const;
	bool				operator==(const SDatum& o) const;
	bool				operator!=(const SDatum& o) const;
	bool				operator>=(const SDatum& o) const;
	bool				operator>(const SDatum& o) const;

			//!	Returns B_OK if we hold a value IDatum or defined SValue.
	status_t			StatusCheck() const;

			//!	Retrieve the IDatum object being used.
	sptr<IDatum>		Datum() const;

	//@}


	// --------------------------------------------------------------
	/*!	@name Datum Operations
		Convenience functions for working with IDatum. */
	//@{

			//!	Retrieve the entire data associated with this value.
			/*!	@param[in] maxSize Maximum number of bytes to read from
					the datum if using IDatum::Open(); if it is larger,
					the fetch will fail with B_OUT_OF_RANGE.
				@param[out] outError Optional error code output in the
					case when B_UNDEFINED_VALUE is returned.
				@result A value SValue if the data was successfully
					fetched, else B_UNDEFINED_VALUE.

				If this is a raw blob of data in an SValue, that is returned
				directly.  Otherwise, if IDatum::Value() succeeds, that
				value is returned.  Otherwise, we try to read the data
				by opening the datum and streaming in the bytes.
				
				If, in case of reading the data, IDatum::Size() is larger than maxSize,
				B_UNDEFINED_VALUE is returned and @a outError is set to B_OUT_OF_RANGE.
				Otherwise the data is read in.  The function may return an
				undefined value if some other error occurs while reading, such
				as running out of memory, in which case @a outError is set
				to the appropriate error code.
			*/
	SValue				FetchValue(size_t maxSize, status_t* outError = NULL) const;

			//!	Retrieve data, possibly truncated, associated with this value
			/*!	@param[in] maxSize Maximum number of bytes to read from
					the datum if using IDatum::Open(); if it is larger,
					only that number of bytes is read and @a outError is
					set to B_DATA_TRUNCATED.
				@param[out] outError Optional error code output in the
					case when B_UNDEFINED_VALUE is returned or data is
					truncated.
				@result A value SValue if the data was successfully
					fetched, else B_UNDEFINED_VALUE.  If the data is truncated,
					this will contain only the truncated data and
					@a outError will be set to B_DATA_TRUNCATED.

				This is exactly like FetchValue(), except in the case where the
				data to be read is larger than @a maxSize.  In that case here,
				we will still read the data, but only @a maxSize bytes of it,
				and @a outError will be set to B_DATA_TRUNCATED.
			*/
	SValue				FetchTruncatedValue(size_t maxSize, status_t* outError = NULL) const;

			//!	Open datum for reading.
			/*!	@param[in] mode Desired IDatum open modes.
				@param[out] outError Status code from IDatum::Open(), B_OK on success.
				@result The input stream ready for you to read from.  If NULL,
					@a outError will be set to an error code.
				
				This is a wrapper for calling IDatum::Open() to read, specifically
				returning an IByteInput stream to you.  You can use interface_cast<T>
				to also retrieve the IByteSeekable interface.

				@note This only succeeds if working with an IDatum object -- if this
				is a simple SValue, the open will always fail.
			*/
	sptr<IByteInput>	OpenInput(	uint32_t mode = IDatum::READ_ONLY,
									status_t* outError = NULL) const;

			//!	Open datum for simple writing.
			/*!	@param[in] mode Desired IDatum open modes.
				@param[out] outError Status code from IDatum::Open(), B_OK on success.
				@result The output stream ready for you to write to.  If NULL,
					@a outError will be set to an error code.
				
				This is a wrapper for calling IDatum::Open() to write, specifically
				returning an IByteOutput stream to you.  You can use interface_cast<T>
				to also retrieve the IByteSeekable or IByteInput interfaces.

				@note This only succeeds if working with an IDatum object -- if this
				is a simple SValue, the open will always fail.
			*/
	sptr<IByteOutput>	OpenOutput(	uint32_t mode = IDatum::READ_WRITE | IDatum::ERASE_DATUM,
									status_t* outError = NULL);

			//!	Open datum for writing, allowing editor objects and/or type changes.
			/*!	@param[in] mode Desired IDatum open modes.
				@param[in] editor Editor object for change notifications as per
					IDatum::Open().
				@param[in] newType New type code for this datum as per IDatum::Open().
					Use 0 to leave the existing type code as-is.
				@param[out] outError Status code from IDatum::Open(), B_OK on success.
				@result The output stream ready for you to write to.  If NULL,
					@a outError will be set to an error code.
				
				This is a wrapper for calling IDatum::Open() to write, specifically
				returning an IByteOutput stream to you.  You can use interface_cast<T>
				to also retrieve the IByteSeekable or IByteInput interfaces.

				@note This only succeeds if working with an IDatum object -- if this
				is a simple SValue, the open will always fail.
			*/
	sptr<IByteOutput>	OpenOutput(	uint32_t mode,
									const sptr<IBinder>& editor,
									uint32_t newType,
									status_t* outError = NULL);

	//@}

private:
	SValue				do_fetch_value(size_t maxSize, status_t* outError, bool truncate) const;

	SValue				m_value;
	sptr<IDatum>		m_datum;
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif // _SUPPORT_DATUM_H
