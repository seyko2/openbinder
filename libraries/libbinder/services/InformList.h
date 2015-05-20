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

#ifndef INFORMLIST_H
#define INFORMLIST_H

#include <support/KeyedVector.h>
#include <support/ITextStream.h>
#include <support/INode.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

class CallbackInfo
{
public:
	CallbackInfo();
	CallbackInfo(const sptr<IBinder>& target, const SValue &method, uint32_t flags, const SValue &cookie);
	CallbackInfo(const sptr<IBinder>& target, const SValue &method, uint32_t flags);
	CallbackInfo(const CallbackInfo &);
	~CallbackInfo();

	sptr<IBinder>	Target() const;
	SValue			Method() const;
	uint32_t			Flags() const;
	SValue			Cookie() const;

private:
	IBinder	* m_target;
	SValue	m_method;
	uint32_t	m_flags;
	SValue	m_cookie;
};

class CreationInfo
{
public:
	CreationInfo();
	CreationInfo(const sptr<INode>& context, const sptr<IProcess>& team, const SString &component, const SValue &interface,
					const SValue &method, uint32_t flags,  const SValue &cookie);
	CreationInfo(const sptr<INode>& context, const sptr<IProcess>& team, const SString &component, const SValue &interface,
					const SValue &method, uint32_t flags);
	CreationInfo(const CreationInfo &);
	~CreationInfo();
	
	sptr<INode>		Context() const;
	sptr<IProcess>	Process() const;
	SString			Component() const;
	SValue			Interface() const;
	SValue			Method() const;
	uint32_t		Flags() const;
	SValue			Cookie() const;

private:
	sptr<INode>		m_context;
	IProcess* 		m_team;
	SString			m_component;
	SValue			m_interface;
	SValue			m_method;
	uint32_t		m_flags;
	SValue			m_cookie;
};

template <class T>
class InformList
{
public:
				InformList();
				~InformList();

	status_t	Add(const SValue &key, const T &info);
	status_t	Remove(const SValue &key, const T &info);
	status_t	Find(const SValue &key, SSortedVector<T> *info);
	
	SKeyedVector<SValue, SSortedVector<T>* > m_list;
};


// ----------------------------------------------------------------------
inline
CallbackInfo::CallbackInfo()
	:m_target(NULL),
	 m_method(),
	 m_flags(0),
	 m_cookie()
{
}

inline
CallbackInfo::CallbackInfo(const sptr<IBinder>& target, const SValue &method, uint32_t flags, const SValue &cookie)
	:m_target(target.ptr()),
	 m_method(method),
	 m_flags(flags),
	 m_cookie(cookie)
{
	if (m_target != NULL) {
		if (m_flags & B_WEAK_BINDER_LINK) {
			m_target->IncWeak(m_target);
		} else {
			m_target->IncStrong(m_target);
		}
	}
}

inline
CallbackInfo::CallbackInfo(const sptr<IBinder>& target, const SValue &method, uint32_t flags)
	:m_target(target.ptr()),
	 m_method(method),
	 m_flags(flags),
	 m_cookie()
{
	if (m_target != NULL) {
		if (m_flags & B_WEAK_BINDER_LINK) {
			m_target->IncWeak(m_target);
		} else {
			m_target->IncStrong(m_target);
		}
	}
}

inline
CallbackInfo::CallbackInfo(const CallbackInfo &o)
	:m_target(o.m_target),
	 m_method(o.m_method),
	 m_flags(o.m_flags),
	 m_cookie(o.m_cookie)
{
	if (m_target != NULL) {
		if (m_flags & B_WEAK_BINDER_LINK) {
			m_target->IncWeak(m_target);
		} else {
			m_target->IncStrong(m_target);
		}
	}

}

inline
CallbackInfo::~CallbackInfo()
{
	if (m_target != NULL) {
		if (m_flags & B_WEAK_BINDER_LINK) {
			m_target->DecWeak(m_target);
		} else {
			m_target->DecStrong(m_target);
		}
	}

}

inline
sptr<IBinder>
CallbackInfo::Target() const
{
	if (m_flags & B_WEAK_BINDER_LINK) {
		wptr<IBinder> wptr(m_target);
		return wptr.promote();
	} else {
		return m_target;
	}
}

inline
SValue
CallbackInfo::Method() const
{
	return m_method;
}

inline
uint32_t
CallbackInfo::Flags() const
{
	return m_flags;
}

inline
SValue
CallbackInfo::Cookie() const
{
	return m_cookie;
}

inline
int32_t
BCompare(const CallbackInfo& l, const CallbackInfo& r)
{
	if (l.Flags() > r.Flags()) return 1;
	if (l.Flags() < r.Flags()) return -1;
	// flags are equal
	if (l.Target().ptr() > r.Target().ptr()) return 1;
	if (l.Target().ptr() < r.Target().ptr()) return -1;
	// targets are equal
	if (l.Method() > r.Method()) return 1;
	if (l.Method() < r.Method()) return -1;
	// targets are equal
	return 0;
};

inline
bool
BLessThan(const CallbackInfo& l, const CallbackInfo& r)
{
	return BCompare(l, r) < 0;
};

inline
const sptr<ITextOutput>&
operator << (const sptr<ITextOutput>& os, const CallbackInfo &info)
{
	os << "target: " << info.Target();
	os << " method: \"" << info.Method().AsString();
	os << "\" flags: " << (void*)info.Flags();
	os << " cookie: " << info.Cookie();
	return os;
}


// ----------------------------------------------------------------------
inline
CreationInfo::CreationInfo()
	:m_context(),
	 m_team(NULL),
	 m_component(),
	 m_interface(),
	 m_method(),
	 m_flags(0),
	 m_cookie()
{
}

inline
CreationInfo::CreationInfo(const sptr<INode>& context, const sptr<IProcess>& team, const SString &component, const SValue &interface,
					const SValue &method, uint32_t flags, const SValue &cookie)
	:m_context(context),
	 m_team(team.ptr()),
	 m_component(component),
	 m_interface(interface),
	 m_method(method),
	 m_flags(flags),
	 m_cookie(cookie)
{
	if (m_team != NULL) {
		if (m_flags & B_WEAK_BINDER_LINK) {
			m_team->IncWeak(m_team);
		} else {
			m_team->IncStrong(m_team);
		}
	}
}

inline
CreationInfo::CreationInfo(const sptr<INode>& context, const sptr<IProcess>& team, const SString &component, const SValue &interface,
					const SValue &method, uint32_t flags)
	:m_context(context),
	 m_team(team.ptr()),
	 m_component(component),
	 m_interface(interface),
	 m_method(method),
	 m_flags(flags),
	 m_cookie()
{
	if (m_team != NULL) {
		if (m_flags & B_WEAK_BINDER_LINK) {
			m_team->IncWeak(m_team);
		} else {
			m_team->IncStrong(m_team);
		}
	}
}

inline
CreationInfo::CreationInfo(const CreationInfo &o)
	:m_context(o.m_context),
	 m_team(o.m_team),
	 m_component(o.m_component),
	 m_interface(o.m_interface),
	 m_method(o.m_method),
	 m_flags(o.m_flags),
	 m_cookie(o.m_cookie)
{
	if (m_team != NULL) {
		if (m_flags & B_WEAK_BINDER_LINK) {
			m_team->IncWeak(m_team);
		} else {
			m_team->IncStrong(m_team);
		}
	}
}

inline
CreationInfo::~CreationInfo()
{
	if (m_team != NULL) {
		if (m_flags & B_WEAK_BINDER_LINK) {
			m_team->DecWeak(m_team);
		} else {
			m_team->DecStrong(m_team);
		}
	}
}

inline
sptr<INode>
CreationInfo::Context() const
{
	return m_context;
}

inline
sptr<IProcess>
CreationInfo::Process() const
{
	if (m_flags & B_WEAK_BINDER_LINK) {
		wptr<IProcess> wptr(m_team);
		return wptr.promote();
	} else {
		return m_team;
	}
}

inline
SString
CreationInfo::Component() const
{
	return m_component;
}

inline
SValue
CreationInfo::Interface() const
{
	return m_interface;
}

inline
SValue
CreationInfo::Method() const
{
	return m_method;
}

inline
uint32_t
CreationInfo::Flags() const
{
	return m_flags;
}

inline
SValue
CreationInfo::Cookie() const
{
	return m_cookie;
}

inline
int32_t
BCompare(const CreationInfo& l, const CreationInfo& r)
{
	if (l.Flags() > r.Flags()) return 1;
	if (l.Flags() < r.Flags()) return -1;
	// flags are equal
	if (l.Component() > r.Component()) return 1;
	if (l.Component() < r.Component()) return -1;
	// components are equal
	if (l.Process()->AsBinder().ptr() > r.Process()->AsBinder().ptr()) return 1;
	if (l.Process()->AsBinder().ptr() < r.Process()->AsBinder().ptr()) return -1;
	// teams are equal
	if (l.Context()->AsBinder().ptr() > r.Context()->AsBinder().ptr()) return 1;
	if (l.Context()->AsBinder().ptr() < r.Context()->AsBinder().ptr()) return -1;
	// contexts are equal
	if (l.Interface() > r.Interface()) return 1;
	if (l.Interface() < r.Interface()) return -1;
	// interfaces are equal
	if (l.Method() > r.Method()) return 1;
	if (l.Method() < r.Method()) return -1;
	// interfaces are equal
	return 0;
};

inline
bool
BLessThan(const CreationInfo& l, const CreationInfo& r)
{
	return BCompare(l, r) < 0;
};

inline
const sptr<ITextOutput>&
operator << (const sptr<ITextOutput>& os, const CreationInfo &info)
{
	os << "context: " << info.Context()->AsBinder();
	os << " proc: " << info.Process()->AsBinder();
	os << " component: " << info.Component();
	os << " interface: " << info.Interface();
	os << " method: " << info.Method();
	os << " flags: " << (void*)info.Flags();
	os << " cookie: " << info.Cookie();
	return os;
}



// ----------------------------------------------------------------------
template <class T> inline
InformList<T>::InformList()
	:m_list()
{
}

template <class T> inline
InformList<T>::~InformList()
{
	size_t count = m_list.CountItems();
	for (size_t i=0; i<count; i++) {
		delete m_list.EditValueAt(i);
	}
}

template <class T> inline
status_t
InformList<T>::Add(const SValue &key, const T &info)
{
	size_t keyIndex;
	size_t infoIndex;
	bool foundKey;
	bool foundInfo;
	bool added;
	
	foundKey = m_list.GetIndexOf(key, &keyIndex);
	if (foundKey) {
		SSortedVector<T> *registered = m_list.EditValueAt(keyIndex);
		foundInfo = registered->GetIndexOf(info, &infoIndex);
		if (foundInfo) return B_NAME_IN_USE;
		registered->AddItem(info, &added);
		if (!added) return B_NO_MEMORY;
		return B_OK;
	} else {
		SSortedVector<T> *registration = new SSortedVector<T>;
		registration->AddItem(info, &added);
		if (!added) goto err;
		added = 0 <= m_list.AddItem(key, registration);
		if (!added) goto err;
		return B_OK;
err:
		delete registration;
		return B_NO_MEMORY;
	}
}

template <class T> inline
status_t
InformList<T>::Remove(const SValue &key, const T &info)
{
	ssize_t keyIndex;
	ssize_t infoIndex;
	
	keyIndex = m_list.IndexOf(key);
	if (keyIndex < 0) return B_NAME_NOT_FOUND;
	SSortedVector<T> * infoList = m_list.EditValueAt(keyIndex);
	infoIndex = infoList->RemoveItemFor(info);
	if (infoList->CountItems() == 0) {
		m_list.RemoveItemsAt(keyIndex, 1);
		delete infoList;
	}
	return infoIndex >= 0 ? B_OK : infoIndex;
}

template <class T> inline
status_t
InformList<T>::Find(const SValue &key, SSortedVector<T> *info)
{
	bool found;
	SSortedVector<T> *items = m_list.ValueFor(key, &found);
	if (items) *info = *items;
	return found ? B_OK : B_NAME_NOT_FOUND;
}

template <class T> inline
const sptr<ITextOutput>&
operator << (const sptr<ITextOutput>& os, const SSortedVector<T> &list)
{
	size_t i, count = list.CountItems();
	for (i=0; i<count; i++) {
		os << "[" << i << "] " << list[i] << endl;
	}
	return os;
}

template <class T> inline
const sptr<ITextOutput>&
operator << (const sptr<ITextOutput>& os, const InformList<T> &list)
{
	size_t i, count = list.m_list.CountItems();
	for (i=0; i<count; i++) {
		os << "[" << list.m_list.KeyAt(i) << "] {" << indent << endl;
		if (list.m_list.ValueAt(i)) os << *list.m_list.ValueAt(i);
		else os << "NULL";
		os << dedent << "}" << endl;
	}
	return os;
}

#endif // INFORMLIST_H
