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

#ifndef _SUPPORT_AUTOBINDER_H_
#define _SUPPORT_AUTOBINDER_H_

/*!	@file support/Autobinder.h
	@ingroup CoreSupportBinder
	@brief Binder automated marshalling code.
*/

#include <support/Value.h>
#include <support/Parcel.h>

/*!	@addtogroup CoreSupportBinder
	@{
*/

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {
using namespace palmos::support;
#endif

class BAutobinderDummiClass
{
public:
	virtual void Method();
};

typedef void (BAutobinderDummiClass::*BSomeMethod)();

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::osp
#endif



#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

enum {
	B_IN_PARAM		= 0x00000001,
	B_OUT_PARAM		= 0x00000002
};

struct BAutobinderDef;
struct BEffectMethodDef;
struct PTypeMarshaller;
struct BParameterInfo;
struct PPointerToMemberFunction;

typedef status_t (*autobinder_local_hook_t)(const sptr<IInterface>& target,	SParcel *in, SParcel *out);
typedef void (*untyped_func)(void);

//!	Unmarshal an argument list out of a parcel.
status_t	autobinder_unmarshal_args(const BParameterInfo* pi, const BParameterInfo* pi_end, uint32_t dir, SParcel& inParcel, void** outArgs, uint32_t* outDirs);
//!	Marshal an argument list into a parcel.
status_t	autobinder_marshal_args(const BParameterInfo* pi, const BParameterInfo* pi_end, uint32_t dir, void** inArgs, SParcel& outParcel, uint32_t* outDirs);

//!	Used by pidgen to unmarshal when receiving a call.
/*!	In pidgen's generated code, this function is called to unmarshal arguments
	before performing a local function call. */
status_t	autobinder_from_parcel(const BEffectMethodDef &def, SParcel &parcel, void **args, uint32_t* outDirs);
//!	Used by pidgen to marshal result when receiving a call.
/*!	In pidgen's generated code, this function is called to marshal any results
	after performing a local function call.  'dirs' must be the result of 'outDirs'
	in autobinder_from_parcel(); or if autobinder_from_parcel() was not used (there
	were no incoming parameters) it can by 0. */
status_t	autobinder_to_parcel(const BEffectMethodDef &def, void **args, void *result, SParcel &parcel, uint32_t dirs);

// execute_autobinder requires that the definitions be sorted in SValue order of the keys
status_t	execute_autobinder(uint32_t code, const sptr<IInterface>& target,
								SParcel &data, SParcel *reply,
								const BAutobinderDef **defs, size_t def_count,
								uint32_t flags);

status_t	parameter_from_value(type_code type, const struct PTypeMarshaller* marshaller, const SValue &v, void *result);
status_t	parameter_to_value(type_code type, const struct PTypeMarshaller* marshaller, const void *value, SValue *out);

/*!
	Unmarshall the parameters from the SValue,
	call the function, and put the return value
	from that function into rv

	Object must be the base class where the virtual
	method you're trying to call was originally
	declared.
*/
status_t
InvokeAutobinderFunction(	const BEffectMethodDef	* def,
							void					* object,
							const SValue			& args,
							SValue					* returnValue
							);

/*!
	Marshall the parameters, make the remote
	binder call, and unmarshall the return
	value.
*/
void
PerformRemoteBinderCall(	int						ebp,
							const BEffectMethodDef	*def,
							unsigned char			*returnValue,
							const sptr<IBinder>&	remote,
							SValue					binding,
							uint32_t				which
							);

struct PTypeMarshaller
{
	// Amount of data in structure.
	size_t					structSize;

	// This function is used to marshall data.
	// It writes the var into the given parcel and returns the amount
	// of data or an error.
	// (This is exactly the same as SParcel::MarshallFixedData().)
	ssize_t					(*marshal_parcel)(SParcel& dest, const void* var);

	// This function is used to unmarshall data.
	// It reads the data at the current location in the parcel and
	// stored it into 'var'.  If the parcel does not contain the
	// expected type, an error should be returned and 'value'
	// remain unmodified.  If the parcel contains B_NULL_VALUE, you
	// MUST ALWAYS read that value and return B_BINDER_READ_NULL_VALUE.
	// (This is exactly the same as SParcel::UnmarshallFixedData().)
	status_t				(*unmarshal_parcel)(SParcel& src, void* var);

	// Marshall to an SValue.  'var' is always non-NULL.
	status_t				(*marshal_value)(SValue* dest, const void* var);

	// Unmarshall from an SValue.  'var' is always non-NULL.
	status_t				(*unmarshal_value)(const SValue& src, void* var);
};

// ADS can't handle static structure members in templates!!! ARGH!!!!
#if 0
template<class TYPE>
class SMarshallerForType
{
public:
	static const PTypeMarshaller marshaller;
private:
	TYPE* adsIsLame;
};

template<class TYPE>
const PTypeMarshaller SMarshallerForType<TYPE>::marshaller =
{
	sizeof(PTypeMarshaller),
	&TYPE::PreMarshalParcel,
	&TYPE::MarshalParcel,
	&TYPE::UnmarshalParcel,
	&TYPE::MarshalValue,
	&TYPE::UnmarshalValue
};
#else
template<class TYPE>
class SMarshallerForType
{
public:
	static const size_t marshaller;
	static const size_t f1;
	static const size_t f2;
	static const size_t f3;
	static const size_t f4;
	static const size_t f5;
private:
	TYPE* adsIsLame;
};

template<class TYPE>
const size_t SMarshallerForType<TYPE>::marshaller = sizeof(PTypeMarshaller);
template<class TYPE>
const size_t SMarshallerForType<TYPE>::f1 =	(size_t)&TYPE::PreMarshalParcel;
template<class TYPE>
const size_t SMarshallerForType<TYPE>::f2 =	(size_t)&TYPE::MarshalParcel;
template<class TYPE>
const size_t SMarshallerForType<TYPE>::f3 =	(size_t)&TYPE::UnmarshalParcel;
template<class TYPE>
const size_t SMarshallerForType<TYPE>::f4 =	(size_t)&TYPE::MarshalValue;
template<class TYPE>
const size_t SMarshallerForType<TYPE>::f5 =	(size_t)&TYPE::UnmarshalValue;
#endif

struct BParameterInfo
{
	type_code				type;	// if type == 0 then this is not a primitive type
	uint32_t				direction;
	const PTypeMarshaller*	marshaller;
};

struct PPointerToMemberFunction
{
	short delta;				// offset of this pointer
	short index;				//	index into vtable  or negative if not virtual
	union {
		int32_t func;			// address of nonvirtual function
		short offset;			// offset if vtable pointer
	};
};

struct BEffectMethodDef
{
	type_code							returnType;
	const PTypeMarshaller*				returnMarshaller;
	size_t								paramCount;
	const BParameterInfo				*paramTypes;
	autobinder_local_hook_t				localFunc;
	union {
		BNS(palmos::osp::)BSomeMethod	pointer;
		PPointerToMemberFunction		pmf;
	};
};

struct BAutobinderDef
{
	size_t					index;			// index in array
	const void				* raw_key;
	const BEffectMethodDef	* put;
	const BEffectMethodDef	* get;
	const BEffectMethodDef	* invoke;
	int32_t					classOffset;	// B_FIND_CLASS_OFFSET(LSuck, ISuck)
	
	const SValue&		key() const;
};

inline const SValue& BAutobinderDef::key() const
{
	return *reinterpret_cast<const SValue*>(raw_key);
}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

/*!
	Use this macro to create a static array of func_info structs.
	It's not really necessary, but it does the casting for you.
	
XXX Put example here.
*/
#define B_FUNC_INFO(method)		{ NULL }//(BNS(palmos::osp::)BSomeMethod)&(method)}


/*!
	
*/
#define B_FIND_BINDER_I_CLASS_OFFSET(L) ((((int)static_cast<IInterface*>((L*)10))-10)) // use 10-10 because 0 is special cased for NULL


/*!
	
*/
#define B_IMPLEMENT_MARSHAL_PLAN(def, returnValue, remote, binding, which) \
		{ \
			int ebp;									\
			__asm__ __volatile__ ("	movl %%ebp, %0;" :"=r"(ebp) );		\
			PerformRemoteBinderCall(ebp, (def), (unsigned char *)(returnValue), (remote), (binding), (which));	\
		}

/*!	@} */

#endif // _SUPPORT_AUTOBINDER_H_
