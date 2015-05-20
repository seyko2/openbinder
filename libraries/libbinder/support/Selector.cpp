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

#include <support/Selector.h>
#include <support/Autolock.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

#define DBG(_) // _

B_STATIC_STRING_VALUE_LARGE(key_value, "value", )

BSelector::BSelector()
	:BnSelector(),
	 m_lock("BSelector lock"),
	 m_value()
{
}

BSelector::BSelector(const SContext& context)
	:BnSelector(context),
	 m_lock("BSelector lock"),
	 m_value()
{
}

BSelector::BSelector(const SContext& context, const SValue &initial)
	:BnSelector(context),
	 m_lock("BSelector lock"),
	 m_value(initial)
{
}

BSelector::~BSelector()
{
}

SValue
BSelector::Value() const
{
	SAutolock l(m_lock.Lock());
	return m_value;
}

void
BSelector::SetValue(const SValue &value)
{
	m_lock.Lock();
	SValue old = m_value;
	m_value = value;

	DBG(bout << "BSelector SetValue value=" << value << endl;)

	if (old != value) {

		bool found_no, found_yes;
		const SVector<Registration> &reg_no = m_regs.ValueFor(old, &found_no);
		const SVector<Registration> &reg_yes = m_regs.ValueFor(value, &found_yes);
		
		m_lock.Unlock();

		Push(SValue(key_value, value));
		if (found_no) {
			DBG(bout << "sending no reg count=" << reg_no.CountItems() << endl;)
			send_to_binders(reg_no, false);
		}
		if (found_yes) {
			DBG(bout << "sending yes reg count=" << reg_yes.CountItems() << endl;)
			send_to_binders(reg_yes, true);
		}

	} else {
		m_lock.Unlock();
	}
}

status_t
BSelector::Register(const SValue &key, const sptr<IBinder> &binder, const SValue &property, uint32_t flags)
{
	status_t err;
	bool found;

	Registration r;
		r.binder = binder;
		r.property = property;
		r.flags = flags;

	m_lock.Lock();
	SVector<Registration> &reg = m_regs.EditValueFor(key, &found);
	if (found) {
		err = reg.AddItem(r);
		if (err != B_OK) {
			m_lock.Unlock();
			return err;
		}
	} else {
		SVector<Registration> vec;
		vec.AddItem(r);
		err = m_regs.AddItem(key, vec);
		if (err != B_OK) {
			m_lock.Unlock();
			return err;
		}
	}
	SValue current = m_value;
	m_lock.Unlock();

	// let them know of their state
	binder->Put(SValue(property, SValue::Bool(current == key)));

	return B_OK;
}

status_t
BSelector::Unregister(const SValue &key, const sptr<IBinder> &binder, const SValue &property, uint32_t flags)
{
	m_lock.Lock();
	ssize_t index = m_regs.IndexOf(key);
	if (index >= 0) {
		SVector<Registration> &reg = m_regs.EditValueAt(index);
		size_t count = reg.CountItems();
		for (size_t i=count; i>0;) {
			i--;
			const Registration &r = reg[i];
			if (r.binder == binder && r.property == property && r.flags == flags) {
				reg.RemoveItemsAt(i, 1);
				break;
			}
		}
		if (reg.CountItems() == 0) {
			m_regs.RemoveItemsAt(index, 1);
		}
	}
	m_lock.Unlock();

	return B_OK;
}

void
BSelector::send_to_binders(const SVector<Registration> &regs, bool value)
{
	size_t i, count = regs.CountItems();
	for (i=0; i<count; i++) {
		const Registration &r = regs[i];
		if (r.flags & B_NO_TRANSLATE_LINK) {
			if (value) {
				SValue dummiResult;
				r.binder->Effect(r.property, B_WILD_VALUE, B_UNDEFINED_VALUE, &dummiResult);
				DBG(bout << "send_to_binders did Effect: value=" << r.property)
				DBG(	<< " binder = " << r.binder << endl;)
			}
		} else {
			r.binder->Put(SValue(r.property, SValue::Bool(value)));
			DBG(bout << "send_to_binders did put: property=" << SValue(r.property, SValue::Bool(value)))
			DBG(	<< " binder = " << r.binder << endl;)
		}
	}
}
