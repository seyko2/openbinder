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

#ifndef _SUPPORT_SELECTOR_H
#define _SUPPORT_SELECTOR_H

/*!	@file support/Selector.h
	@ingroup CoreSupportBinder
	@brief Select something with the Binder!
*/

#include <support/ISelector.h>
#include <support/Locker.h>
#include <support/KeyedVector.h>

/*	@addtogroup CoreSupportBinder
	@{
*/

//!	When you set the value, it pushes <tt>\@{"value"->\<<i>the new value</i>\>}</tt>
#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

class BSelector : public BnSelector
{
public:
					BSelector();
					BSelector(const SContext& context);
					BSelector(const SContext& context, const SValue &initialValue);
	virtual			~BSelector();
	
	virtual SValue	Value() const;
	virtual void	SetValue(const SValue &value);

	virtual status_t Register(const SValue &key, const sptr<IBinder> &binder,
								const SValue &property, uint32_t flags = 0);
	virtual status_t Unregister(const SValue &key, const sptr<IBinder> &binder,
								const SValue &property, uint32_t flags = 0);

private:
	struct Registration {
		sptr<IBinder> binder;
		SValue property;
		uint32_t flags;
	};

	static void send_to_binders(const SVector<Registration> &regs, bool value);

	mutable SLocker m_lock;
	SValue m_value;
	SKeyedVector<SValue, SVector<Registration> > m_regs;
};

/*!	@} */
#if _SUPPORTS_NAMESPACE
} } // palmos::support;
#endif

#endif // _SUPPORT_SELECTOR_H
