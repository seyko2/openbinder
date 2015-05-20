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

#ifndef _STORAGE_VALUEDATUM_H
#define _STORAGE_VALUEDATUM_H

/*!	@file storage/ValueDatum.h
	@ingroup CoreSupportDataModel
	@brief A concrete implementation of BStreamData holding a generic SValue.
*/

#include <storage/StreamDatum.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
#endif

/*!	@addtogroup CoreSupportDataModel
	@{
*/

// ==========================================================================
// ==========================================================================

//!	A concrete implementation of BStreamData holding a generic SValue.
/*!	This class is a complete implementation of IDatum on top of an
	SValue.  It uses the memory model of BStreamData to perform read
	and write operations on the SValue it contains.

	@todo  Need to consider what to do with B_STRING_TYPE in terms
	of NUL-termination.

	@todo Should probably introduce a BAbstractValueDatum class
	that this one derives from, and/or add facilities to allow clients
	to more easily control how the value can be modifed (restrict its
	possible types etc).

	@nosubgrouping
*/
class BValueDatum : public BStreamDatum
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
									BValueDatum(uint32_t mode = IDatum::READ_WRITE);
									BValueDatum(const SContext& context, uint32_t mode = IDatum::READ_WRITE);
									BValueDatum(const SValue& value, uint32_t mode = IDatum::READ_WRITE);
									BValueDatum(const SContext& context, const SValue& value, uint32_t mode = IDatum::READ_WRITE);
protected:
	virtual							~BValueDatum();
public:

	//@}

	// --------------------------------------------------------------
	/*!	@name BStreamDatum Implementation
		Provide BStreamDatum implementation to create a concrete class.  This
		implements both the stream and memory models, for best performance. */
	//@{

			//!	Returns the current type code of the value.
	virtual	uint32_t				ValueTypeLocked() const;
			//!	Changes the type code of the value.
			/*!	@note The implementation currently doesn't do anything special
				with B_STRING_TYPE, meaning it allows clients to create
				invalid string values (ones that don't include a final NUL). */
	virtual	status_t				StoreValueTypeLocked(uint32_t type);
			//!	Returns the current number of bytes in the value.
	virtual	off_t					SizeLocked() const;
			//!	Changes the size of the value.
	virtual	status_t				StoreSizeLocked(off_t size);
			//!	Returns the current value.
	virtual	SValue					ValueLocked() const;
			//!	Changes the entire value.
	virtual	status_t				StoreValueLocked(const SValue& value);

			//!	Return bytes in the value for reading.
	virtual	const void*				StartReadingLocked(	const sptr<Stream>& stream, off_t position,
														ssize_t* inoutSize, uint32_t flags) const;
			//!	Clean up from reading (currently a noop).
	virtual	void					FinishReadingLocked(const sptr<Stream>& stream, const void* data) const;
			//!	Edit bytes inside the value, returning the requested starting address.
	virtual	void*					StartWritingLocked(	const sptr<Stream>& stream, off_t position,
														ssize_t* inoutSize, uint32_t flags);
			//!	Finish editing the value.
	virtual	void					FinishWritingLocked(const sptr<Stream>& stream, void* data);

private:
									BValueDatum(const BValueDatum&);
			BValueDatum&			operator=(const BValueDatum&);

			SValue					m_value;
			size_t					m_writeLen;
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif // _STORAGE_VALUEDATUM_H
