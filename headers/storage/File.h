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

#ifndef _STORAGE2_FILE_H
#define _STORAGE2_FILE_H

#include <support/SupportDefs.h>
#include <support/IBinder.h>
#include <support/IStorage.h>

#include <fcntl.h>
#include <sys/stat.h>


#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
using namespace palmos::support;
#endif // _SUPPORTS_NAMESPACE

class BFile;

class BFile : public BnStorage
{
public:

							BFile();
							BFile(const char* path, uint32_t open_mode);
							BFile(const BFile &file);

#if TARGET_HOST == TARGET_HOST_BEOS
							BFile(const entry_ref *ref, uint32_t open_mode);
							BFile(const BEntry *entry, uint32_t open_mode);
							BFile(const char *path, uint32_t open_mode);
							BFile(const BCatalog *dir, const char *path,
								  uint32_t open_mode);
#endif

				status_t	SetTo(const char* path, uint32_t open_mode);

#if TARGET_HOST == TARGET_HOST_BEOS
				status_t	SetTo(const entry_ref *ref, uint32_t open_mode);
				status_t	SetTo(const BEntry *entry, uint32_t open_mode);
				status_t	SetTo(const BCatalog *dir, const char *path,
								uint32_t open_mode);
#endif

				bool		IsReadable() const;
				bool		IsWritable() const;
		
		virtual	off_t		Size() const;
		virtual	status_t	SetSize(off_t size);

		virtual	ssize_t		ReadAtV(off_t position, const struct iovec* vector, ssize_t count);
		virtual	ssize_t		WriteAtV(off_t position, const struct iovec* vector, ssize_t count);
		virtual	status_t	Sync();
	
protected:
		virtual				~BFile();
		
private:				
		int32_t				m_fd;
		uint32_t				m_mode;
};

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif // _SUPPORTS_NAMESPACE

#endif	// _STORAGE2_FILE_H
