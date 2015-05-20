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

#include <package/PackageKit.h>
#include <support/ByteStream.h>
#include <storage/File.h>
#include <assert.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace package {
using namespace palmos::storage;
using namespace palmos::support;
#endif

B_STATIC_STRING_VALUE_LARGE(key_file,"file",)

sptr<IByteInput>
get_script_data_from_value(const SValue &info, status_t *error)
{
	status_t err;
	SString filename;
	sptr<BFile> file;

	filename = info[key_file].AsString(&err);
	if (err != B_OK) goto ERROR;
	
	file = new BFile();
	err = file->SetTo(filename.String(), O_RDONLY);
	if (err != B_OK) {
		file = NULL;
		goto ERROR;
	}

	if (error) *error = B_OK;
	return new BByteStream(file);

ERROR:
	// err now contains the error code, if there was an error,
	// data might be NULL.
	if (error) *error = err;
	return NULL;
}

#if _SUPPORTS_NAMESPACE
} }
#endif
