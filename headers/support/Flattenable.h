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

#ifndef	_SUPPORT_FLATTENABLE_H
#define	_SUPPORT_FLATTENABLE_H

/*!	@file support/Flattenable.h
	@ingroup CoreSupportUtilities
	@brief Abstract interface for an object that can be flattened into
	a byte buffer.
*/

#include <support/SupportDefs.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

enum {
	// Request a flattened form that may contain references to
	// active objects.
	B_FLATTEN_FORM_ACTIVE = 0,

	// Request a flattened form that can be written to persistent
	// storage.
	B_FLATTEN_FORM_PERSISTENT = 1
};

class SFlattenable
{
public:	
	virtual				~SFlattenable() { };
	virtual	ssize_t		FlattenedSize() const;
	virtual	status_t	Flatten(void *buffer, ssize_t size) const;
	virtual	status_t	Unflatten(type_code c, const void *buf, ssize_t size);

			// Convert this object into a SValue.
	virtual	SValue		AsValue(int32_t form = B_FLATTEN_FORM_ACTIVE) const = 0;

			// Assign this object from a SValue previously created by
			// the above function.
	virtual	status_t	SetFromValue(const SValue& value) = 0;

	virtual	ssize_t		ParcelSize(int32_t form = B_FLATTEN_FORM_ACTIVE) const;

			// Flatten the object into a raw parcel.  This will keep
			// track of the number of bytes written, but NOT the type
			// code (it is up to the caller to store the type code
			// separately).
	virtual	ssize_t		WriteParcel(SParcel& target, int32_t form = B_FLATTEN_FORM_ACTIVE) const;

			// Unflatten the object from a raw parcel.  If it can't
			// accept the given type code, it must fail without reading
			// anything from the parcel.
	virtual	ssize_t		ReadParcel(type_code type, SParcel& source, ssize_t size);


	virtual	bool		IsFixedSize() const = 0;
	virtual	type_code	TypeCode() const = 0;
	virtual	bool		AllowsTypeCode(type_code code) const;
};

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_FLATTENABLE_H */
