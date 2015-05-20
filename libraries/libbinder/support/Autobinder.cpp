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

#include <new>
#include <stdlib.h>
#include <stdio.h>
#include <support/Autobinder.h>
#include <support/Binder.h>
#include <support/TypeConstants.h>
#include <support/SupportDefs.h>
#include <support/StdIO.h>
#include <support/String.h>
#include <support/Value.h>
#include <support/Debug.h>
#include <support_p/SupportMisc.h>

#include <support_p/WindowsCompatibility.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

status_t
autobinder_unmarshal_args(const BParameterInfo* pi, const BParameterInfo* pi_end, uint32_t dir, SParcel& parcel, void** args, uint32_t* outDirs)
{
	// Notice that autobinder_from_parcel has a potential side effect
	// of setting entries in args to NULL.  (See test of B_BINDER_READ_NULL_VALUE below).
	// Any time ReadTypedData gets a B_NULL_TYPE rather than an actual
	// type, the corresponding entry in args will be cleared to NULL.
	// This is so that we can duplicate the same calling semantics.
	// If the remote method was called with NULL, we need to call the
	// local method the same way.

	uint32_t dirs = 0;
	ssize_t err;

	// in and inout parameters
	while (pi < pi_end) {
		dirs |= pi->direction;
		if (pi->direction & dir) {
			if (pi->marshaller) {
				err = pi->marshaller->unmarshal_parcel(parcel, *args);
			} else {
				err = parcel.ReadTypedData(pi->type, *args);
				// as we get parameter values from the remote side, we have
				// to worry about getting a B_NULL_TYPE.  In that case, we have
				// an optional parameter that wasn't provided.  We need to duplicate
				// the NULL parameter on this side so that the function is 
				// called with the same semantics.
			}
			if (err == B_BINDER_READ_NULL_VALUE) {
				err = B_OK;
				*args = NULL;
			}
		} else {
			err = parcel.SkipValue();
		}
		if (err < B_OK) {
			return err;
		}
		pi++;
		args++;
	}
	
	*outDirs = dirs;

	return B_OK;
}

status_t
autobinder_from_parcel(const BEffectMethodDef &def, SParcel &parcel, void** args, uint32_t* outDirs)
{
	return autobinder_unmarshal_args(def.paramTypes, def.paramTypes+def.paramCount, B_IN_PARAM, parcel, args, outDirs);
}

status_t
autobinder_marshal_args(const BParameterInfo* pi, const BParameterInfo* pi_end, uint32_t dir, void **args, SParcel& parcel, uint32_t* outDirs)
{
	uint32_t dirs = 0;
	ssize_t amt;

	// inout and out parameters
	while (pi < pi_end) {
		dirs |= pi->direction;
		if (pi->direction & dir) {
			if (pi->marshaller) {
				amt = pi->marshaller->marshal_parcel(parcel, *args);
			} else {
				amt = parcel.WriteTypedData(pi->type, *args);
				//bout << "Wrote param " << STypeCode(def.paramTypes[i].type) << ": " << parcel << endl;
#if BUILD_TYPE == BUILD_TYPE_DEBUG
				DbgOnlyFatalErrorIf(amt != SParcel::TypedDataSize(pi->type, *args),
									"Written size different than expected size");
#endif
			}
		} else {
			amt = parcel.WriteTypedData(B_UNDEFINED_TYPE, NULL);
			//bout << "Wrote empty param: " << parcel << endl;
#if BUILD_TYPE == BUILD_TYPE_DEBUG
			DbgOnlyFatalErrorIf(amt != SParcel::TypedDataSize(B_UNDEFINED_TYPE, NULL),
								"Written size different than expected size");
#endif
		}
		if (amt < 0) return amt;
		pi++;
		args++;
	}

	*outDirs = dirs;

	return B_OK;
}

status_t
autobinder_to_parcel(const BEffectMethodDef &def, void **args, void *result, SParcel &parcel, uint32_t dirs)
{
	ssize_t amt;
	size_t required;

	const bool hasOut = (dirs&B_OUT_PARAM) != 0;

	// if there are no outputs at all, don't write anything.
	if (def.returnType != B_UNDEFINED_TYPE || hasOut) {

		// 2. allocate in the parcel
		//parcel.Reserve(required);
		
		// 3. add the values to the parcel
		
		// return value
		if (def.returnMarshaller) {
			amt = def.returnMarshaller->marshal_parcel(parcel, result);
		} else {
			amt = parcel.WriteTypedData(def.returnType, result);
			//bout << "Wrote return value " << STypeCode(def.returnType) << ": " << parcel << endl;
		}
		if (amt < 0) return amt;
		
		if (!hasOut) return B_OK;

		// inout and out parameters
		return autobinder_marshal_args(def.paramTypes, def.paramTypes+def.paramCount, B_OUT_PARAM, args, parcel, &dirs);
	}
	
	return B_OK;
}


status_t
execute_autobinder(uint32_t code, const sptr<IInterface>& target,
					SParcel &data, SParcel *reply,
					const BAutobinderDef **defs, size_t def_count,
					uint32_t flags)
{
	// Get the position in the array of the action.
	uint32_t index = (uint32_t)data.ReadInt32();
	if (index >= def_count) {
#if BUILD_TYPE == BUILD_TYPE_DEBUG
		char msg[200];
		sprintf(msg, "execute_autobinder: entry index=%d out of range def_count=%d",
				index, def_count);
		bout << msg << endl;
		bout << "execute_autobinder data=" << data.Data() << " ";
		data.PrintToStream(bout);
		bout << endl << "execute_autobinder reply=" << (reply ? reply->Data() : NULL) << " ";
		if (reply) {
			reply->PrintToStream(bout);
		} else {
			bout << "NULL";
		}
		bout << endl;
		DbgOnlyFatalError(msg);
#endif // BUILD_TYPE == BUILD_TYPE_DEBUG
		return B_NAME_NOT_FOUND;
	}

	const BAutobinderDef * const def = defs[index];

#if BUILD_TYPE == BUILD_TYPE_DEBUG
	SValue key = data.ReadValue();
	if (key != def->key()) {
		char msg[200];
		sprintf(msg, "execute_autobinder: entry at index=%d does not match request key",
				index);
		bout << msg << endl;
		bout << "key=" << key << endl;
		bout << "def->key()=" << def->key() << endl;
		bout << "execute_autobinder data=" << data.Data() << " ";
		data.PrintToStream(bout);
		bout << endl << "execute_autobinder reply=" << (reply ? reply->Data() : NULL) << " ";
		if (reply) {
			reply->PrintToStream(bout);
		} else {
			bout << "NULL";
		}
		bout << endl;
		DbgOnlyFatalError(msg);
	}

	if(0) { // verbose
		bout << "key=" << key << endl;
		bout << "def->key()=" << def->key() << endl;
		bout << "execute_autobinder data=" << data.Data() << " ";
		data.PrintToStream(bout);
		bout << endl << "execute_autobinder reply=" << (reply ? reply->Data() : NULL) << " ";
		if (reply) {
			reply->PrintToStream(bout);
		} else {
			bout << "NULL";
		}
		bout << endl;
	}

#else
	// Skip the name of the action -- not needed here.
	data.SkipValue();
#endif

	// don't pass in inappropriate parcels to put and get funcs
	switch (code)
	{
		case B_INVOKE_TRANSACTION:
			return def->invoke->localFunc(target, &data, reply);
		case B_GET_TRANSACTION:
			return def->get->localFunc(target, NULL, reply);
		case B_PUT_TRANSACTION:
			return def->put->localFunc(target, &data, NULL);
		default:
			return B_BINDER_UNKNOWN_TRANSACT;
	}
}


status_t
parameter_to_value(type_code type, const struct PTypeMarshaller* marshaller, const void *value, SValue *out)
{
	// value can be NULL if we are dealing with
	// an optional parameter, in that case
	// write a B_NULL_VALUE into the stream 
	// (so the other side can see this, and correctly supply 
	// a NULL pointer when it calls the function).

	if (value == NULL) {
		*out = B_NULL_VALUE;
	}

	if (marshaller)
	{
		return marshaller->marshal_value(out, value);
	}

	switch (type)
	{
		case B_BOOL_TYPE:
			*out = SValue::Bool(*reinterpret_cast<const bool*>(value));
			
		case B_FLOAT_TYPE:
			*out = SValue::Float(*reinterpret_cast<const float*>(value));
			break;

		case B_DOUBLE_TYPE:
			*out = SValue::Double(*reinterpret_cast<const double*>(value));
			break;

		case B_CHAR_TYPE:
		case B_INT8_TYPE:
		case B_UINT8_TYPE:
			*out = SValue::Int8(*reinterpret_cast<const int8_t*>(value));
			break;
		
		case B_INT16_TYPE:
		case B_UINT16_TYPE:
			*out = SValue::Int16(*reinterpret_cast<const int16_t*>(value));
			break;
		
		case B_INT32_TYPE:
		case B_UINT32_TYPE:
		case B_SIZE_T_TYPE:
		case B_SSIZE_T_TYPE:
		case B_WCHAR_TYPE:
			*out = SValue::Int32(*reinterpret_cast<const int32_t*>(value));
			break;
		
		case B_INT64_TYPE:
		case B_UINT64_TYPE:
		case B_NSECS_TYPE:
		case B_BIGTIME_TYPE:
		case B_OFF_T_TYPE:
			*out = SValue::Int64(*reinterpret_cast<const int64_t*>(value));
			break;
		
		case B_BINDER_TYPE:
			*out = SValue::Binder(*reinterpret_cast<const sptr<IBinder>*>(value));
			break;
		
		case B_BINDER_WEAK_TYPE:
			*out = SValue::WeakBinder(*reinterpret_cast<const wptr<IBinder>*>(value));
			break;

		case B_VALUE_TYPE:
			*out = *reinterpret_cast<const SValue*>(value);
			break;
#if 0		
		case B_CONSTCHAR_TYPE:
			*out = SValue::String(reinterpret_cast<const char*>(value));
			break;
#endif 
		case B_CONSTCHAR_TYPE:
		case B_STRING_TYPE:
			*out = SValue::String(*reinterpret_cast<const SString*>(value));
			break;

		case B_UNDEFINED_TYPE:
			out->Undefine();
			break;
		
		default:
			return B_BAD_TYPE;
	}
	return B_OK;
}


status_t
parameter_from_value(type_code type, const struct PTypeMarshaller* marshaller, const SValue &v, void *result)
{
	// result can be NULL if we are dealing with optional
	// parameters.  In that case, don't store the value.
	if (result == NULL) {
		return B_OK;
	}

	if (marshaller)
	{
		return marshaller->unmarshal_value(v, result);
	}

	switch (type)
	{
		case B_BOOL_TYPE:
			*reinterpret_cast<bool*>(result) = v.AsBool();
			
		case B_FLOAT_TYPE:
			*reinterpret_cast<float*>(result) = v.AsFloat();
			break;
		
		case B_DOUBLE_TYPE:
			*reinterpret_cast<double*>(result) = v.AsDouble();
			break;
		
		case B_CHAR_TYPE:
		case B_INT8_TYPE:
		case B_UINT8_TYPE:
			*reinterpret_cast<int8_t*>(result) = static_cast<int8_t>(v.AsInt32());
			break;

		case B_INT16_TYPE:
		case B_UINT16_TYPE:
			*reinterpret_cast<int16_t*>(result) = static_cast<int8_t>(v.AsInt32());
			break;

		case B_INT32_TYPE:
		case B_UINT32_TYPE:
		case B_SIZE_T_TYPE:
		case B_SSIZE_T_TYPE:
		case B_WCHAR_TYPE:
			*reinterpret_cast<int32_t*>(result) = v.AsInt32();
			break;
			
		case B_INT64_TYPE:
			*reinterpret_cast<int64_t*>(result) = v.AsInt64();
			break;

		case B_BINDER_TYPE:
			*reinterpret_cast<sptr<IBinder>*>(result) = v.AsBinder();
			break;
		
		case B_BINDER_WEAK_TYPE:
			*reinterpret_cast<wptr<IBinder>*>(result) = v.AsWeakBinder();
			break;

		case B_VALUE_TYPE:
			*reinterpret_cast<SValue*>(result) = v;
			break;
#if 0
		case B_CONSTCHAR_TYPE:
			*reinterpret_cast<const char**>(result) = (v.AsString()).String();
			break;
#endif
		case B_CONSTCHAR_TYPE:
		case B_STRING_TYPE:
			*reinterpret_cast<SString*>(result) = v.AsString();
			break;	

		case B_UNDEFINED_TYPE:
			break;
		
		default:
			break;
	}
	return B_OK;
}


#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif
