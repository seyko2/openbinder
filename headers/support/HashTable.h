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

#ifndef _SUPPORT_HASHTABLE_H_
#define _SUPPORT_HASHTABLE_H_

/*!	@file support/HashTable.h
	@ingroup CoreSupportUtilities
	@brief Hash table template class.
*/

#include <support/SupportDefs.h>
#include <support/KeyedVector.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/**************************************************************************************/

class SHasher
{
	public:
	
					SHasher(int32_t bits);
		uint32_t	BaseHash(const void *data, int32_t len) const;

	private:

		int32_t		m_bits;
		uint32_t	m_crcxor[256];
};

template <class KEY, class VALUE>
class SHashTable : public SHasher
{
	public:
	
		SHashTable(int32_t bits);
		~SHashTable();

		uint32_t Hash(const KEY &key) const;

		const VALUE& Lookup(const KEY &key) const
		{
			return m_table[Hash(key)].ValueFor(key);
		}

		bool Lookup(const KEY &key, VALUE &value) const
		{
			bool found;
			value = m_table[Hash(key)].ValueFor(key, &found);
			return found;
		}

		void Insert(const KEY &key, const VALUE &value)
		{
			m_table[Hash(key)].AddItem(key,value);
		}	

		void Remove(const KEY &key)
		{
			m_table[Hash(key)].RemoveItemFor(key);
		}	

	private:

		SKeyedVector<KEY,VALUE> *	m_table;
};

/*!	@} */

template<class KEY, class VALUE>
SHashTable<KEY, VALUE>::SHashTable(int32_t bits) : SHasher(bits)
{
	m_table = new SKeyedVector<KEY,VALUE> [1<<bits];
}

template<class KEY, class VALUE>
SHashTable<KEY, VALUE>::~SHashTable()
{
}

/**************************************************************************************/

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif	/* _SUPPORT_HASHTABLE_H_ */
