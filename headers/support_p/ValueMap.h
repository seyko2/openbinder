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

#ifndef _SUPPORT_VALUEMAP_H
#define _SUPPORT_VALUEMAP_H

#include <support/Locker.h>
#include <support/IByteStream.h>
#include <support/Value.h>
#include <support/Vector.h>

#include <support_p/ValueMapFormat.h>
#include <support_p/SupportMisc.h>

#if _SUPPORTS_NAMESPACE || _REQUIRES_NAMESPACE
namespace std {
#endif
	struct nothrow_t;
#if _SUPPORTS_NAMESPACE || _REQUIRES_NAMESPACE
}
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

class SParcel;
class BValueMap;

class BValueMapPool
{
public:
	BValueMapPool();
	~BValueMapPool();

	BValueMap* Create(size_t initSize);
	void Delete(BValueMap* map);

	int32_t Hits() const { return m_hits; }
	int32_t Misses() const { return m_misses; }

private:
	SLocker m_lock;
	struct pool {
		int32_t count;
		BValueMap* objects;

		pool() : count(0), objects(NULL) { }
	};
	enum { NUM_POOLS = 3 };
	pool m_pools[NUM_POOLS];
	int32_t m_hits, m_misses;
};

extern BValueMapPool g_valueMapPool;

class BValueMap
{
public:
			struct pair {
				SValue key;
				SValue value;
				
				inline pair() { }
				inline pair(const SValue& k, const SValue& v)	:	key(k), value(v) { }
				inline pair(const pair& o)						:	key(o.key), value(o.value) { }
				inline ~pair() { }
				
				inline pair& operator=(const pair& o) { key = o.key; value = o.value; return *this; }
			};
			
	static	BValueMap*		Create(size_t initSize = 1);
	static	BValueMap*		Create(SParcel& from, size_t avail, size_t count, ssize_t* out_size);
			BValueMap*		Clone() const;

			void			SetFirstMap(const SValue& key, const SValue& value);

			void			IncUsers() const;
			void			DecUsers() const;
			bool			IsShared() const;
	
			ssize_t			ArchivedSize() const;
			ssize_t			Archive(SParcel& into) const;
			ssize_t			IndexFor(	const SValue& key,
										const SValue& value = B_UNDEFINED_VALUE) const;
			ssize_t			IndexFor(	uint32_t type, const void* data, size_t length) const;
			const pair&		MapAt(size_t index) const;

			size_t			CountMaps() const;
			int32_t			Compare(const BValueMap& o) const;
			int32_t			LexicalCompare(const BValueMap& o) const;
			
	static	ssize_t			AddNewMap(BValueMap** This, const SValue& key, const SValue& value);
	static	status_t		RemoveMap(	BValueMap** This,
										const SValue& key,
										const SValue& value = B_UNDEFINED_VALUE);
	static	void			RemoveMapAt(BValueMap** This, size_t index);
	static	status_t		RenameMap(	BValueMap** This,
										const SValue& old_key,
										const SValue& new_key);
			
	static	SValue*			BeginEditMapAt(BValueMap** This, size_t index);
	static	void			EndEditMapAt(BValueMap** This);
			bool			IsEditing() const;

			void			Pool();

			void 			AssertEditing() const;
			
private:
	friend	class			BValueMapPool;

							// These are not implemented.
							BValueMap();
							BValueMap(const BValueMap& o);
							~BValueMap();

			void			Construct(int32_t avail);
			void			Destroy();

			void			Delete();
			
	static	ssize_t			AddMapAt(BValueMap** This, size_t index, const SValue& key, const SValue& value);

			bool			GetIndexOf(	uint32_t type, const void* data, size_t length,
										size_t* index) const;
			bool			GetIndexOf(	const SValue& k, const SValue& v,
										size_t* index) const;
			ssize_t			ComputeArchivedSize() const;

	mutable	int32_t			m_users;
	mutable	ssize_t			m_dataSize;
			ssize_t			m_size;
			ssize_t			m_avail;
			int32_t			m_pad0;
			ssize_t			m_editIndex;

			// Mappings start here.
			pair			m_maps[1];
};

inline void BValueMap::IncUsers() const
{
	AssertEditing();
	
	g_threadDirectFuncs.atomicInc32(&m_users);
}

inline void BValueMap::DecUsers() const
{
	AssertEditing();
	
	if (g_threadDirectFuncs.atomicDec32(&m_users) == 1)
		const_cast<BValueMap*>(this)->Delete();
}

inline bool BValueMap::IsShared() const
{
	return m_users > 1;
}

inline ssize_t BValueMap::ArchivedSize() const
{
	if (m_dataSize >= 0) return m_dataSize;
	return ComputeArchivedSize();
}

inline ssize_t BValueMap::IndexFor(const SValue& key, const SValue& value) const
{
	size_t index;
	return GetIndexOf(key, value, &index) ? index : B_NAME_NOT_FOUND;
}

inline ssize_t BValueMap::IndexFor(uint32_t type, const void* data, size_t length) const
{
	size_t index;
	return GetIndexOf(type, data, length, &index) ? index : B_NAME_NOT_FOUND;
}

inline const BValueMap::pair& BValueMap::MapAt(size_t index) const
{
	return m_maps[index];
}

inline size_t BValueMap::CountMaps() const
{
	return m_size;
}

inline bool	BValueMap::IsEditing() const
{
	return m_editIndex >= 0;
}

inline void BValueMap::AssertEditing() const
{
	ErrFatalErrorIf(IsEditing(), "This operation can not be performed while editing a value");
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
#endif
