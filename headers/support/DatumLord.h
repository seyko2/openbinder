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

#ifndef _SUPPORT_DATUMLORD_H
#define _SUPPORT_DATUMLORD_H

/*!	@file support/DatumLord.h
	@ingroup CoreSupportDataModel
	@brief Generates IDatum objects on demand.
*/

#include <storage/GenericDatum.h>

#include <support/KeyedVector.h>
#include <support/Locker.h>
#include <support/MemoryStore.h>

/*!	@addtogroup CoreSupportDataModel
	@{
*/

/*!	@deprecated Use SDatumGeneratorInt instead. */
class SDatumLord : public virtual SAtom
{
public:
						SDatumLord();
						~SDatumLord();
	virtual void		InitAtom();
	
	status_t	RequestDatum(const SValue &key, sptr<IDatum> *result);

protected:
	virtual status_t	GetValue(const SValue &key, SValue *value) = 0;
	virtual status_t	SetValue(const SValue &key, const SValue &value) = 0;

private:
			SDatumLord(const SDatumLord &);
	
// ADS doesn't even closely follow the C++ standard, so we make this public so it can
// compile.  But don't use it outside this class, when we get a real compiler, this will
// be fixed.
// "d:\source\rome\platform\headers\PDK\support\DatumLord.h", line 30: Error: C3032E: 'SDatumLord::Datum' is a non-public member
public:
	class Datum : public BGenericDatum
	{
	public:
						Datum(const sptr<SDatumLord> &owner, const SValue &key);
		virtual			~Datum();
		virtual void	InitAtom();

		// IDatum
		virtual uint32_t ValueType() const;
		virtual void SetValueType(uint32_t type);
		virtual off_t Size() const;
		virtual void SetSize(off_t size);
		virtual SValue Value() const;
		virtual void SetValue(const SValue& value);
		virtual sptr<IBinder> Open(uint32_t mode, const sptr<IBinder>& editor, uint32_t newType);
		
		void StorageDone();
		
	private:
		sptr<SDatumLord> m_owner;
		const SValue m_key;

		SLocker m_lock;
		wptr<IStorage> m_storage;

		// these are used by the Storage 'cuz it keeps a direct pointer to data...
		SLocker m_storageLock;
		SValue m_storageValue;
	};

private:
	class Storage : public BValueStorage
	{
	public:
				Storage(SValue* value, SLocker* lock, const sptr<Datum>& owner);
		virtual	~Storage();
	private:
		sptr<Datum> m_owner;
	};

	friend class Datum;
	
	SLocker m_lock;
	SKeyedVector<SValue, wptr<IDatum> > m_data;
};

/*!	@} */

#endif // _SUPPORT_DATUMLORD_H
