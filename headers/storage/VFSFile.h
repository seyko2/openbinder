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

#ifndef	_STORAGE_VFS_FILE_H
#define	_STORAGE_VFS_FILE_H

#include <support/IStorage.h>
#include <support/URL.h>

#include <VFSMgr.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace storage {
using namespace palmos::support;
#endif

class BVFSFile : public BnStorage
{
	public:
		enum flags
		{
			DELETE_IF_EMPTY		= 1
		};
		
		
							BVFSFile();
							BVFSFile(const char* path, int flags, int mode=0, status_t * outResult=0);
							BVFSFile(const SUrl & url, int flags, int mode=0, status_t * outResult=0);
				
				status_t	InitCheck() const;
				
				// Set the object to access the given file.  Opens the file.
				status_t	SetTo(const char* path, int flags, int mode=0);
				status_t	SetTo(const SUrl & url, int flags, int mode=0);

				bool		IsReadable() const;
				bool		IsWritable() const;
	
				uint16_t	VolRefNum() const;	

				status_t	Delete();
		
		virtual	off_t		Size() const;			// query for total file size
		virtual	status_t	SetSize(off_t size);	// resize file
		
		virtual	ssize_t		ReadAtV(off_t position, const struct iovec *vector, ssize_t count);
		virtual	ssize_t		WriteAtV(off_t position, const struct iovec *vector, ssize_t count);
		
		virtual	status_t	Sync();

	protected:
		virtual				~BVFSFile();
		
private:
	status_t		m_initStatus;
	int				m_fd;
	int				m_flags;
	off_t			m_pos;
};



#if _SUPPORTS_NAMESPACE
} } // namespace palmos::storage
#endif

#endif /* _STORAGE_VFS_FILE_H */
