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

#include <support/Flattenable.h>
#include <support/Parcel.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

ssize_t
SFlattenable::ParcelSize(int32_t form) const
{
	SParcel temp;
	SValue value(AsValue(form));
	value.Archive(temp);
	return temp.Length();
}

ssize_t
SFlattenable::WriteParcel(SParcel& target, int32_t form) const
{
	return AsValue(form).Archive(target);
}

ssize_t
SFlattenable::ReadParcel(type_code type, SParcel& source, ssize_t)
{
	if (type != TypeCode()) return B_BAD_TYPE;

	SValue value = source.ReadValue();
	status_t result = SetFromValue(value);
	return result;
}

// -------------

ssize_t
SFlattenable::FlattenedSize() const
{
	return ParcelSize(B_FLATTEN_FORM_ACTIVE);
}

status_t
SFlattenable::Flatten(void *buffer, ssize_t size) const
{
	SParcel target(buffer, size);
	return WriteParcel(target, B_FLATTEN_FORM_ACTIVE);
}

status_t
SFlattenable::Unflatten(type_code c, const void *buf, ssize_t size)
{
	SParcel source(buf, size);
	return ReadParcel(c, source, size);
}

bool SFlattenable::AllowsTypeCode(type_code c) const
{
	return (c == TypeCode());
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
