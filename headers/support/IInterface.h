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

#ifndef	_SUPPORT_INTERFACE_INTERFACE_H
#define	_SUPPORT_INTERFACE_INTERFACE_H

/*!	@file support/IInterface.h
	@ingroup CoreSupportBinder
	@brief Common base class for abstract binderized interfaces.
*/

#include <support/SupportDefs.h>
#include <support/StaticValue.h>
#include <support/Atom.h>

#include <support/IBinder.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportBinder
	@{
*/

// Forward declare the core binder API (defined in IBinder.h).
class SValue;
class IBinder;

/**************************************************************************************/

//!	Base class for C++ language Binder interfaces.
class IInterface : virtual public SAtom
{
public:
	
								IInterface();
	
			sptr<IBinder>		AsBinder();
			sptr<const IBinder>	AsBinder() const;

protected:
	virtual						~IInterface();

	virtual	sptr<IBinder>		AsBinderImpl();
	virtual	sptr<const IBinder>	AsBinderImpl() const;

	typedef sptr<IInterface>	(*instantiate_proxy_func)(const sptr<IBinder>& binder);
	
	static	sptr<IInterface>	ExecAsInterface(const sptr<IBinder>& binder,
												const SValue& descriptor,
												instantiate_proxy_func proxy_func,
												status_t* out_error = NULL);

	static	sptr<IInterface>	ExecAsInterfaceNoInspect(const sptr<IBinder>& binder,
														 const SValue& descriptor,
														 instantiate_proxy_func proxy_func,
														 status_t* out_error = NULL);

private:
								IInterface(const IInterface&);
};


/**************************************************************************************/

//!	Convert a generic IBinder to a concrete interface.
template<class IFACE>
sptr<IFACE>
interface_cast(const sptr<IBinder> &b)
{
	return IFACE::AsInterface(b);
}

//!	Convert an SValue containing a binder object to a concrete interface.
template<class IFACE>
sptr<IFACE>
interface_cast(const SValue &v)
{
	return IFACE::AsInterface(v);
}


/**************************************************************************************/

//!	Use this macro inside of your IInterface subclass to define the standard IInterface meta-API.
/*!	You will not normally use this, instead letting pidgen generate
	similar code from an IDL file. */
#define B_DECLARE_META_INTERFACE(iname)																		\
																											\
		static	const BNS(::palmos::support::) SValue&														\
			Descriptor();																					\
		static	BNS(::palmos::support::) sptr<I ## iname>													\
			AsInterface(const BNS(::palmos::support::) sptr< BNS(::palmos::support::) IBinder> &o,			\
						status_t* out_error = NULL);														\
		static	BNS(::palmos::support::) sptr<I ## iname>													\
			AsInterface(const BNS(::palmos::support::) SValue &v,											\
						status_t* out_error = NULL);														\
		static	BNS(::palmos::support::) sptr<I ## iname>													\
			AsInterfaceNoInspect(const BNS(::palmos::support::) sptr< BNS(::palmos::support::) IBinder> &o,	\
								 status_t* out_error = NULL);												\
		static	BNS(::palmos::support::) sptr<I ## iname>													\
			AsInterfaceNoInspect(const BNS(::palmos::support::) SValue &v,									\
								 status_t* out_error = NULL);												\

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_INTERFACE_INTERFACE_H */
