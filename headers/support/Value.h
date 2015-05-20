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

#ifndef _SUPPORT_VALUE_H_
#define _SUPPORT_VALUE_H_

/*!	@file support/Value.h
	@ingroup CoreSupportUtilities
	@brief A general-purpose data container.
*/

#include <support/SupportDefs.h>
#include <support/TypeFuncs.h>
#include <support/StaticValue.h>
#include <support/IByteStream.h>
#include <support/ITextStream.h>
#include <support/IBinder.h>
#include <support/ByteOrder.h>
#include <support/TypeConstants.h>
#include <support/KeyID.h>

#include <string.h>
#include <stdint.h>

BNS(namespace palmos {)
BNS(namespace support {)

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SParcel;
class SSharedBuffer;
class SString;
class BValueMap;
class SValue;

//!	Flags for SValue operations.
enum {
	//! don't restructure to avoid complex keys?
	B_NO_VALUE_FLATTEN			= 0x00000001,
	//!	don't traverse into nested values?
	B_NO_VALUE_RECURSION		= 0x00000002
};

//!	Flags for SValue::LockData().
enum {
	//!	keep existing data as-is.
	B_EDIT_VALUE_DATA		= 0x00000001
};

//!	Additional flags for PrintToStream().
enum {
	B_PRINT_VALUE_TYPES			= 0x00010000,	//!< Print full type information?
	B_PRINT_CATALOG_CONTENTS	= 0x00020000,	//!< Print contents of a context catalog?
	B_PRINT_BINDER_INTERFACES	= 0x00040000,	//!< Print interfaces implemented by a binder?
	B_PRINT_BINDER_CONTENTS		= 0x00080000	//!< Print properties in a binder?
};

/*----- Some useful constants -----------------------------*/

const static_small_value B_UNDEFINED_VALUE	= { B_UNDEFINED_TYPE, "" };
const static_small_value B_WILD_VALUE		= { B_PACK_SMALL_TYPE(B_WILD_TYPE, 0), "" };
const static_small_value B_NULL_VALUE		= { B_PACK_SMALL_TYPE(B_NULL_TYPE, 0), "" };

const static_bool_value	 B_FALSE_BOOL		= { B_PACK_SMALL_TYPE(B_BOOL_TYPE, sizeof(int8_t)), false };
const static_bool_value B_TRUE_BOOL			= { B_PACK_SMALL_TYPE(B_BOOL_TYPE, sizeof(int8_t)), true };

B_CONST_INT32_VALUE(B_0_INT32, 0, );
B_CONST_INT32_VALUE(B_1_INT32, 1, );
B_CONST_INT32_VALUE(B_2_INT32, 2, );
B_CONST_INT32_VALUE(B_3_INT32, 3, );
B_CONST_INT32_VALUE(B_4_INT32, 4, );
B_CONST_INT32_VALUE(B_5_INT32, 5, );
B_CONST_INT32_VALUE(B_6_INT32, 6, );
B_CONST_INT32_VALUE(B_7_INT32, 7, );
B_CONST_INT32_VALUE(B_8_INT32, 8, );
B_CONST_INT32_VALUE(B_9_INT32, 9, );
B_CONST_INT32_VALUE(B_10_INT32, 10, );

B_CONST_FLOAT_VALUE(B_0_0_FLOAT, 0.0f, );
B_CONST_FLOAT_VALUE(B_0_1_FLOAT, 0.1f, );
B_CONST_FLOAT_VALUE(B_0_2_FLOAT, 0.2f, );
B_CONST_FLOAT_VALUE(B_0_25_FLOAT,0.25f, );
B_CONST_FLOAT_VALUE(B_0_3_FLOAT, 0.3f, );
B_CONST_FLOAT_VALUE(B_0_4_FLOAT, 0.4f, );
B_CONST_FLOAT_VALUE(B_0_5_FLOAT, 0.5f, );
B_CONST_FLOAT_VALUE(B_0_6_FLOAT, 0.6f, );
B_CONST_FLOAT_VALUE(B_0_7_FLOAT, 0.7f, );
B_CONST_FLOAT_VALUE(B_0_75_FLOAT,0.75f, );
B_CONST_FLOAT_VALUE(B_0_8_FLOAT, 0.8f, );
B_CONST_FLOAT_VALUE(B_0_9_FLOAT, 0.9f, );
B_CONST_FLOAT_VALUE(B_1_0_FLOAT, 1.0f, );

B_CONST_STRING_VALUE_LARGE(B_VALUE_VALUE, "value", );
B_CONST_STRING_VALUE_SMALL(B_EMPTY_STRING, "", );

// Those are here only for compatibility. Use the B_ constants instead.
#define B_0_INT32		B_0_INT32
#define B_1_INT32		B_1_INT32
#define B_2_INT32		B_2_INT32
#define B_3_INT32		B_3_INT32
#define B_4_INT32		B_4_INT32
#define B_5_INT32		B_5_INT32
#define B_6_INT32		B_6_INT32
#define B_7_INT32		B_7_INT32
#define B_8_INT32		B_8_INT32
#define B_9_INT32		B_9_INT32
#define B_10_INT32		B_10_INT32
#define B_0_0_FLOAT		B_0_0_FLOAT
#define B_0_1_FLOAT		B_0_1_FLOAT
#define B_0_2_FLOAT		B_0_2_FLOAT
#define B_0_25_FLOAT	B_0_25_FLOAT
#define B_0_3_FLOAT		B_0_3_FLOAT
#define B_0_4_FLOAT		B_0_4_FLOAT
#define B_0_5_FLOAT		B_0_5_FLOAT
#define B_0_6_FLOAT		B_0_6_FLOAT
#define B_0_7_FLOAT		B_0_7_FLOAT
#define B_0_75_FLOAT	B_0_75_FLOAT
#define B_0_8_FLOAT		B_0_8_FLOAT
#define B_0_9_FLOAT		B_0_9_FLOAT
#define B_1_0_FLOAT		B_1_0_FLOAT

/*----- Create simple SValue -----------------------------*/

//!	Convenience for creating simple data values.
/*!	This class is more efficient than the corresponding
	SValue method such as SValue::Int32(), but semantically
	they are equivalent.

	SSimpleValue allows you to create integers, booleans,
	and floats.  Use SSimpleStatusValue to create status
	codes.
*/
template<class T>
class SSimpleValue : public static_small_value
{
public:
	inline SSimpleValue(T v) {
		type = get_type(v);
		*reinterpret_cast<T*>(data) = v; 
	}
	inline SSimpleValue(int32_t tc, T v) {
		type = B_PACK_SMALL_TYPE(tc, sizeof(v));
		*reinterpret_cast<T*>(data) = v; 
	}
private:
	int32_t get_type(int8_t)	{ return B_PACK_SMALL_TYPE(B_INT8_TYPE,		sizeof(int8_t)); }
	int32_t get_type(int16_t)	{ return B_PACK_SMALL_TYPE(B_INT16_TYPE,	sizeof(int16_t)); }
	int32_t get_type(int32_t)	{ return B_PACK_SMALL_TYPE(B_INT32_TYPE,	sizeof(int32_t)); }
	int32_t get_type(uint8_t)	{ return B_PACK_SMALL_TYPE(B_INT8_TYPE,		sizeof(int8_t)); }
	int32_t get_type(uint16_t)	{ return B_PACK_SMALL_TYPE(B_INT16_TYPE,	sizeof(int16_t)); }
	int32_t get_type(uint32_t)	{ return B_PACK_SMALL_TYPE(B_INT32_TYPE,	sizeof(int32_t)); }
	int32_t get_type(bool)		{ return B_PACK_SMALL_TYPE(B_BOOL_TYPE,		sizeof(int8_t)); }
	int32_t get_type(float)		{ return B_PACK_SMALL_TYPE(B_FLOAT_TYPE,	sizeof(float)); }
};

//!	Convenience for creating a value containing a status code.
/*! This unfortunately can't be a part of SSimpleValue<> because of
	amiguity with int32_t.
*/
class SSimpleStatusValue : public static_small_value
{
public:
	inline SSimpleStatusValue(status_t v) {
		type = B_PACK_SMALL_TYPE(B_STATUS_TYPE,	sizeof(status_t));
		*reinterpret_cast<status_t*>(data) = v; 
	}
};

// --------------------------------------------------------------------

//!	A general-purpose data container, also known as a variant.
/*!	SValue is formally an abstract set of <tt>{key->value}</tt> mappings, where
	each key and value can be any typed blob of data.  Standard data types
	are defined for strings, integers, IBinder objects, and other common data
	types.  In addition, there are three special values: \c B_UNDEFINED_VALUE is a
	SValue that is not set; \c B_WILD_VALUE is a special value that can be thought of
	as being every possible value all at the same time; \c B_NULL_VALUE is used to
	indicate value that are being supplied but not specified (such as to
	use the default value for an optional parameter).

	Note that the definition of SValue is very recursive -- it is a
	collection of key/value mappings, where both the key and value are
	themselves a SValue.  As such, a SValue containing a single data
	item is conceptually actually the mapping <tt>{WILD->value}</tt>; because of
	the properties of WILD, a WILD key can appear more than one time
	so that a set of the values <tt>{ A, B }</tt> can be constructed in a SValue
	as the mapping <tt>{ {WILD->A}, {WILD->B} }</tt>.  When iterating over and
	thinking of operations on a value, it is always viewed as a map in
	this way.  However, there are many convenience functions for working
	with a value in its common form of a single <tt>{WILD->A}</tt> item.

	@nosubgrouping
*/
class SValue
{
public:
	/*!	@name Constructor and Destructor */
	//@{
			inline 			SValue();
			inline 			SValue(const SValue& o);
							
			//! NOTE THAT THE DESTRUCTOR IS NOT VIRTUAL!
			inline			~SValue();
	//@}

	/*!	@name Constructors for Mappings
		These constructors are used to create values containing
		a single explicit maping. */
	//@{
			inline			SValue(const SValue& key, const SValue& value);
			inline			SValue(const SValue& key, const SValue& value, uint32_t flags);
			inline			SValue(const SValue& key, const SValue& value, uint32_t flags, size_t numMappings);
	//@}

	/*!	@name Constructors for Data
		These constructors are used to create values containing
		a single piece of data of various types, which is implicitly
		the mapping <tt>{WILD->data}</tt>. */
	//@{
			inline			SValue(type_code type, const void* data, size_t len);
							SValue(type_code type, const SSharedBuffer* buffer);
			inline explicit	SValue(int32_t num);
			explicit		SValue(const char* str);
			explicit		SValue(const SString& str);
			inline explicit	SValue(const static_small_string_value& str);
			inline explicit	SValue(const static_large_string_value& str);
							SValue(const sptr<IBinder>& binder);
							SValue(const wptr<IBinder>& binder);
							SValue(const sptr<SKeyID>& binder);
	//@}
							
	
	/*!	@name Creating Special Values
		Convenience functions to access special value types. */
	//@{
	
			//! Not-a-value, synonym for B_UNDEFINED_VALUE.
	static inline const SValue& SValue::Undefined()		{ return B_UNDEFINED_VALUE; }
			//!	The all-encompassing value, synonym for B_WILD_VALUE.
	static inline const SValue& SValue::Wild()			{ return B_WILD_VALUE; }
			//!	The null value, synonym for B_NULL_VALUE.
	static inline const SValue& SValue::Null()			{ return B_NULL_VALUE; }

	//@}

	/*!	@name Creating Data Values
		These static functions allow you to build SValues of various standard
		types.  Note that it is bette to use SSimpleValue for int8_t, int16_t,
		int32_t, bool, and float types, and SSimpleStatusValue for status_t. */
	//@{

	/*
	// Unfortunately, this doesn't work because it can cause more than 2 userdef conversions which
	// is an error.
	static	inline SSimpleValue	<int8_t>	Int8(int8_t v)		{ return SSimpleValue<int8_t>(v); }
	static	inline SSimpleValue	<int16_t>	Int16(int16_t v)	{ return SSimpleValue<int16_t>(v); }
	static	inline SSimpleValue	<int32_t>	Int32(int32_t v)	{ return SSimpleValue<int32_t>(v); }
	static	inline SSimpleValue	<bool>		Bool(bool v)		{ return SSimpleValue<bool>(v); }
	static	inline SSimpleValue	<float>		Float(float v)		{ return SSimpleValue<float>(v); }
	*/

	static	 SValue			Int8(int8_t v);
	static	 SValue			Int16(int16_t v);
	static	inline SValue	Int32(int32_t v)	{ return SValue(v); }
	static	 SValue			Bool(bool v);
	static	 SValue			Float(float v);

	static	inline SValue	Binder(const sptr<IBinder>& value)		{ return SValue(value); }
	static	inline SValue	WeakBinder(const wptr<IBinder>& value)	{ return SValue(value); }
	static	inline SValue	String(const char *value)				{ return SValue(value); }
	static	inline SValue	String(const SString& value)			{ return SValue(value); }
	static	SValue			Int64(int64_t value);
	static	SValue			Time(nsecs_t value);
	static	SValue			Double(double value);
	static	SValue			Atom(const sptr<SAtom>& value);
	static	SValue			WeakAtom(const wptr<SAtom>& value);
	static	SValue			Status(status_t error);
	static	inline SValue	KeyID(const sptr<SKeyID>& value)		{ return SValue(value); }
	
	static inline SValue	SSize(ssize_t value)					{ return SValue(int32_t(value)); }
	static inline SValue	Offset(off_t value)						{ return Int64(value); }

	//@}

	/*!	@name Basic Operations
		Assignment, status, existence, etc. */
	//@{

			//!	Return status of the value.
			/*!	If the value's ErrorCheck() is not B_OK, this is returned.
				If B_STATUS_TYPE, that error code is returned; if value
				is undefined, B_NO_INIT is returned.
				Otherwise, B_OK returned.  (Any valid type is considered
				to also be an "okay" status.)  See also AsStatus(),
				which performs a -conversion- to a status code.
			*/
			status_t		StatusCheck() const;

			//! Retrieve exception-level error.
			/*!	Returns the error or B_OK if it is a valid value
				(including a status value).  You probably want to use
				StatusCheck() or AsStatus() instead. */
			status_t		ErrorCheck() const;
			//!	Set an exception-level error code.
			/*!	Error values are special, and consume any other value
				they are joined with.  They are usually used for
				reporting internal errors (such as out of memory) from
				inside of SValue itself.  Users will normally want
				to create a status value with SValue::Status(). */
			void			SetError(status_t error);
			
			SValue&			Assign(const SValue& o);
			SValue&			Assign(type_code type, const void* data, size_t len);
	inline	SValue&			operator=(const SValue& o)			{ return Assign(o); }

			//! Move value backward in memory (for implementation of BMoveBefore).
	static	void			MoveBefore(SValue* to, SValue* from, size_t count = 1);
			//! Move value forward in memory (for implementation of BMoveAfter).
	static	void			MoveAfter(SValue* to, SValue* from, size_t count = 1);

			//!	Exchange contents of 'this' and 'with'.
			void			Swap(SValue& with);
			
			//!	Set value to B_UNDEFINED_VALUE.
			void			Undefine();
			//!	Check whether the value is not B_UNDEFINED_VALUE.
	inline	bool			IsDefined() const;

			//!	Checking whether this value is B_WILD_VALUE.
	inline	bool			IsWild() const;
			
			//!	Check whether this value is neither B_UNDEFINED_VALUE nor B_WILD_VALUE.
	inline	bool			IsSpecified() const;
			
			//!	Check whether this value is a single item of the form <tt>{WILD->A}</tt>.
	inline	bool			IsSimple() const;
			
			//!	Check whether this value is an object, such as a binder.
			bool			IsObject() const;
			
			status_t		CanByteSwap() const;
			status_t		ByteSwap(swap_action action);
			
	//@}

	/*!	@name Raw Data Operations
		Operations on the value as a raw blob of data with a type code.  Note
		that these are not value for objects containing anything besides
		simple values.  That is, values that are only the single mapping
		<tt>{WILD->data}</tt>. */
	//@{
	
			//!	Return type code of data in this value.
			type_code		Type() const;
			//!	Return raw data in the value.
			const void*		Data() const;
			//!	Return number of bytes of data in the value.
			size_t			Length() const;
			//!	Return raw value data as a SSharedBuffer.
			const SSharedBuffer*	SharedBuffer() const;
			
			//! Make sure value's data is in the SSharedBuffer pool.
			void			Pool();

			//! Return the data if the value is the exact given type and length.
			const void*		Data(type_code type, size_t length) const;

			//!	Retrieve data if value is type and length given.
			/*!	The data if the value is the exact given type and at least
				as large as the requested length.
				@param[in] type The required type of the value.
				@param[in,out] inoutMinLength On call, this is the minimum
					length needed; upon return it is changed to the actual
					number of bytes in the value.
				@return The same as calling Data() if the given conditions
					are met, otherwise NULL.
			*/
			const void*		Data(type_code type, size_t* inoutMinLength) const;

			//!	Change this value's type code.
			/*!	This will fail if either the value currently contains an
				object or mapping, or the new type is an object type, to ensure
				you don't create corrupt values. */
			status_t		SetType(type_code newType);

			//!	Start raw editing of data in value.
			/*!	Only valid on values for which IsSimple() is true and IsObject()
				is false.  Call EndEditBytes() when done.
				@param[in] type The new type code to assign to the value.  Must
					not be a value map or object type.
				@param[in] length Number of bytes needed.
				@param[in] flags Use B_EDIT_VALUE_DATA to make the returned
					buffer have the some contents as the current value,
					otherwise the initial contents are undefined and you
					must set it all yourself.
				@return NULL if there was an error, otherwise a pointer to
					a buffer of 'length' bytes.
			*/
			void*			BeginEditBytes(type_code type, size_t length, uint32_t flags=0);
			//!	Finish raw editing after previously calling BeginEditBytes().
			/*	@param[in] final_length If >= zero, the value's data will be
					resized to this amount, otherwise it is left unchanged.
			*/
			status_t		EndEditBytes(ssize_t final_length=-1);
	
	//@}

	/*!	@name Mapping Operations
		Working with an SValue as structured data. */
	//@{
	
			//!	Combine contents to two values together.
			/*!	Joins all mappings in both values to create the final
				result.  Duplicate terminal values will create sets.
				
				For example:
@code
{ (A->B), (D->E) }.Join( { (A->G), (B->I) } )
@endcode
				
				Results in:
@code
{ (A->(B,G)), (B->I), (D->E) }
@endcode
				
				Note that key A now contains the contents of both mappings,
				resulting in a set.
				@see Overlay(), Inherit().
			*/
			SValue&			Join(const SValue& from, uint32_t flags = 0);
			//!	Non-destructive version of Join().
			const SValue	JoinCopy(const SValue& from, uint32_t flags = 0) const;
			
			//!	Convenience function for overlaying a single map item.
			/*!	\note JoinItem(B_WILD_VALUE, A) is the same as Join(A).
			*/
			SValue&			JoinItem(const SValue& key, const SValue& value, uint32_t flags=0);
			
			//!	Apply mappings on top of this value.
			/*!	Overwrites any existing mappings in the value that are the
				same as the new mappings.
				
				For example:
@code
{ (A->B), (D->E) }.Overlay( { (A->G), (B->I) } )
@endcode
				
				Results in:
@code
{ (A->G), (B->I), (D->E) }
@endcode
				
				Note that key A contains the value from the right-hand side.
				@see Inherit(), Join().
			*/
			SValue&			Overlay(const SValue& from, uint32_t flags = 0);
			//!	Non-destructive version of Overlay().
			const SValue	OverlayCopy(const SValue& from, uint32_t flags = 0) const;
			
			//!	Apply mappings underneath this value.
			/*!	Does not modify any existing mappings in the value.
			
				For example:
@code
{ (A->B), (D->E) }.Inherit( { (A->G), (B->I) } )
@endcode
				
				Results in:
@code
{ (A->B), (B->I), (D->E) }
@endcode
				
				Note that key A contains the value from the left-hand side.
				@see Overlay(), Join().
			*/
			SValue&			Inherit(const SValue& from, uint32_t flags = 0);
			//!	Non-destructive version of Inherit().
			const SValue	InheritCopy(const SValue& from, uint32_t flags = 0) const;
			
			//!	Perform a value remapping operation.
			/*!	This operation is defined as
				<tt>(A->B).MapValues((D->E)) == (A->((D->E)[B]))</tt>.  This is performed for
				every mapping in 'this', the results of which are aggregated
				using Join().
				
				For example:
@code
{ (A->B), (D->E) }.MapValues( { (A->G), (B->I) } )
@endcode
				
				Results in:
@code
{ (A->I) }
@endcode
			*/
			SValue&			MapValues(const SValue& from, uint32_t flags = 0);
			//!	Non-destructive version of MapValues().
			const SValue	MapValuesCopy(const SValue& from, uint32_t flags = 0) const;
	
			//!	Remove mappings from this value.
			/*!	For every mapping in \a from, if that \e mapping appears in
				\a this then it is removed.  To remove an entire key, use
				the mapping <tt>(key->WILD)</tt>.  This is equivalent to a set
				subtraction.
			*/
			SValue&			Remove(const SValue& from, uint32_t flags = 0);
			//!	Non-destructive version of Remove().
			const SValue	RemoveCopy(const SValue& from, uint32_t flags = 0) const;
			//!	Convenience for Remove() of a single item/mapping.
			status_t		RemoveItem(	const SValue& key,
										const SValue& value = B_WILD_VALUE);
			
			//! Keep mappings in this value.
			/*!	For every mapping in \a this, if that mapping \e doesn't appear
				in \a from then it is removed.  This is equivalent to a set
				intersection.
			*/
			SValue&			Retain(const SValue& from, uint32_t flags = 0);
			//!	Non-destructive version of Retain().
			const SValue	RetainCopy(const SValue& from, uint32_t flags = 0) const;
			
			//!	Change the name of a key in the value.
			/*!	@note The old and new keys must be a specified value --
				you can not rename using WILD values.
				@see IsSpecified
			*/
			status_t		RenameItem(	const SValue& old_key,
										const SValue& new_key);
			
			//!	Check whether this value contains the given mapping.
			bool			HasItem(const SValue& key,
									const SValue& value = B_UNDEFINED_VALUE) const;
	
			//!	Given an array @a keys of values, returns their indices in @a this's value mapping.
			/*!	This is used in conjunction with ReplaceValues() to efficiently change
				a set of items in an SValue mapping.  This function is called first, filling
				in @a outIndices with the internal location of each of the corresponding @a keys. */
			void 			GetKeyIndices(const SValue* keys, size_t* outIndices, size_t count);

			//!	Batch replace the values of a set of mappings.
			/*!	After using GetKeyIndices() to find the indices of an array of values in
				this mapping, you can use ReplaceValues() to set the values of each of
				those mappings to the new @a values. */
			const SValue&	ReplaceValues(const SValue* values, const size_t* indices, size_t count);
			
			//!	Look up a key in this value.
			/*!	Looking up a WILD key returns the entire value as-is.
				Looking up with a key that is a map will match all items
				of the map.
				
				For example:
@code
{ (A->(B->(D->E))) }.ValueFor( { (A->(B->D)) } )
@endcode
				
				Results in:
@code
{ E }
@endcode
			*/
			const SValue	ValueFor(const SValue& key, uint32_t flags) const;

			//!	Optimization of ValueFor() with default flags.
			const SValue&			ValueFor(const SValue& key) const;
			//!	Optimization of ValueFor() for raw character strings.
			/*!	@note Doesn't support \a this being WILD. */
			const SValue&			ValueFor(const char* str) const;
			//!	Optimization of ValueFor() for SString.
			/*!	@note Doesn't support \a this being WILD. */
			const SValue&			ValueFor(const SString& str) const;
			//!	Optimization of ValueFor() for integers.
			/*!	@note Doesn't support \a this being WILD. */
			const SValue&			ValueFor(int32_t index) const;
			//!	Optimization of ValueFor() for static values.
			inline const SValue&	ValueFor(const static_small_string_value& str) const { return ValueFor(reinterpret_cast<const SValue&>(str)); }
			//!	Optimization of ValueFor() for static values.
			inline const SValue&	ValueFor(const static_large_string_value& str) const { return ValueFor(reinterpret_cast<const SValue&>(str)); }

	
			//!	Count the number of mappings in the value.
			int32_t			CountItems() const;
			//!	Iterate over the mappings in the value.
			/*!	@param[in,out] cookie State of iteration.  Initialize to NULL before
					first call.
				@param[out] out_key The key of the next item.
				@param[out] out_value The value of the next item.
				@return B_OK if there was a next item, an error if there were no more
					mappings in the SValue.
			*/
			status_t		GetNextItem(void** cookie, SValue* out_key, SValue* out_value) const;
			
			//! Get the keys from this value
			SValue			Keys() const;
			
			//! Direct editing of sub-items in the value.
			/*!	@param key The key of the item you would like to edit.  This must be
					a simple value, not a complex mapping.
				@return A pointer to the value stored inside the mapping, or NULL
					if \a key could not be found.
				@note Only valid for a value that contains full mappings <tt>{A->B}</tt>.
					Be sure to call EndEditItem() when done; you must not make any
					other calls on this object until you have done so.
			*/
			SValue*			BeginEditItem(const SValue &key);
			//!	Finish sub-item editing after previously calling BeginEditItem().
			/*	@param[in] item The item returned by BeginEditItem().
			*/
			void			EndEditItem(SValue* item);
			
			//!	Synonym for ValueFor().
	inline	const SValue&	operator[](const SValue& key) const						{ return ValueFor(key); }
			//!	Synonym for ValueFor().
	inline	const SValue&	operator[](const char* key) const						{ return ValueFor(key); }
			//!	Synonym for ValueFor().
	inline	const SValue&	operator[](const SString& key) const					{ return ValueFor(key); }
			//!	Synonym for ValueFor().
	inline	const SValue&	operator[](int32_t index) const							{ return ValueFor(index); }
			//!	Synonym for ValueFor().
	inline	const SValue&	operator[](const static_small_string_value& key) const	{ return ValueFor(key); }
			//!	Synonym for ValueFor().
	inline	const SValue&	operator[](const static_large_string_value& key) const	{ return ValueFor(key); }
	
			//!	Synonym for JoinCopy().
	inline	const SValue	operator+(const SValue& o) const						{ return JoinCopy(o); }
			//!	Synonym for RemoveCopy().
	inline	const SValue	operator-(const SValue& o) const						{ return RemoveCopy(o); }
			//!	Synonym for MapValuesCopy().
	inline	const SValue	operator*(const SValue& o) const						{ return MapValuesCopy(o); }

			//!	Synonym for Join().
	inline	SValue&			operator+=(const SValue& o)								{ return Join(o); }
			//!	Synonym for Remove().
	inline	SValue&			operator-=(const SValue& o)								{ return Remove(o); }
			//!	Synonym for MapValues().
	inline	SValue&			operator*=(const SValue& o)								{ return MapValues(o); }
	
	//@}

	/*!	@name Archiving
		Reading and writing as flat byte streams. */
	//@{
	
			ssize_t			ArchivedSize() const;
			ssize_t			Archive(SParcel& into) const;
			ssize_t			Archive(const sptr<IByteOutput>& into) const;
			ssize_t			Unarchive(SParcel& from);
			ssize_t			Unarchive(const sptr<IByteInput>& from);
			
	//@}

	/*!	@name Comparison
		The common SValue comparison operation is a quick binary
		compare of its type code and data.  Not only is this fast, but
		the way values are compared is type-independent and thus will never
		change.  The Compare() method and all comparison operators
		use this approach. */
	//@{
	
			int32_t			Compare(const SValue& o) const;
			int32_t			Compare(const char* str) const;
			int32_t			Compare(const SString& str) const;
			int32_t			Compare(type_code type, const void* data, size_t length) const;
			
	inline	bool			operator==(const SValue& o) const						{ return Compare(o) == 0; }
	inline	bool			operator!=(const SValue& o) const						{ return Compare(o) != 0; }
	inline	bool			operator<(const SValue& o) const						{ return Compare(o) < 0; }
	inline	bool			operator<=(const SValue& o) const						{ return Compare(o) <= 0; }
	inline	bool			operator>=(const SValue& o) const						{ return Compare(o) >= 0; }
	inline	bool			operator>(const SValue& o) const						{ return Compare(o) > 0; }
	
	inline	bool			operator==(const char* str) const						{ return Compare(str) == 0; }
	inline	bool			operator!=(const char* str) const						{ return Compare(str) != 0; }
	inline	bool			operator<(const char* str) const						{ return Compare(str) < 0; }
	inline	bool			operator<=(const char* str) const						{ return Compare(str) <= 0; }
	inline	bool			operator>=(const char* str) const						{ return Compare(str) >= 0; }
	inline	bool			operator>(const char* str) const						{ return Compare(str) > 0; }
	
	inline	bool			operator==(const SString& str) const					{ return Compare(str) == 0; }
	inline	bool			operator!=(const SString& str) const					{ return Compare(str) != 0; }
	inline	bool			operator<(const SString& str) const						{ return Compare(str) < 0; }
	inline	bool			operator<=(const SString& str) const					{ return Compare(str) <= 0; }
	inline	bool			operator>=(const SString& str) const					{ return Compare(str) >= 0; }
	inline	bool			operator>(const SString& str) const						{ return Compare(str) > 0; }
	
	inline	bool			operator==(const static_small_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) == 0; }
	inline	bool			operator!=(const static_small_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) != 0; }
	inline	bool			operator<(const static_small_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) < 0; }
	inline	bool			operator<=(const static_small_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) <= 0; }
	inline	bool			operator>=(const static_small_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) >= 0; }
	inline	bool			operator>(const static_small_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) > 0; }

	inline	bool			operator==(const static_large_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) == 0; }
	inline	bool			operator!=(const static_large_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) != 0; }
	inline	bool			operator<(const static_large_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) < 0; }
	inline	bool			operator<=(const static_large_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) <= 0; }
	inline	bool			operator>=(const static_large_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) >= 0; }
	inline	bool			operator>(const static_large_string_value& str) const	{ return Compare(reinterpret_cast<const SValue&>(str)) > 0; }

			//!	Perform a type-dependent comparison between two values.
			/*!	This comparison is smart about types, trying to get standard
				types in their natural order.  Because of this, note that the
				relative ordering of LexicalCompare() WILL CHANGE between operating
				system releases.  You normally will use the standard Compare()
				operation.
			*/
			int32_t			LexicalCompare(const SValue& o) const;
	
	//@}

	/*!	@name Type Conversions
		Retrieve the value as a standard type, potentially performing
		a conversion. */
	//@{
	
			//!	Convert value to a status code.
			/*!	This is the same as AsSSize(), except it does not return positive
				integers -- in that case B_OK is returned and 'result' is set to
				B_BAD_TYPE.  Also see StatusCheck() to retrieve the value status
				without performing any conversions.
			*/
			status_t		AsStatus(status_t* result = NULL) const;
			
			//!	Convert value to a status/size code.
			/*!	Result will be:
					- If the value's ErrorCheck() is not B_OK, this is returned.
					- If B_STATUS_TYPE, that error code is returned.
					- If AsInt32() succeeds, its value is returned.
				Otherwise, B_OK returned and result is set to B_BAD_TYPE.  Use
				AsStatus() instead to only retrieve valid status codes.
			*/
			ssize_t			AsSSize(status_t* result = NULL) const;
			
			//!	Convert value to a strong IBinder pointer.
			/*!	Converts from the following types:
					- B_BINDER_TYPE: Returned as-is.
					- B_BINDER_HANDLE_TYPE: Returns binder proxy.
					- B_WEAK_BINDER_TYPE: Attempts to promote to strong pointer.
					- B_WEAK_BINDER_HANDLE_TYPE: Retrieve proxy and attempt to promote.
					- B_NULL_TYPE: Returns NULL.
				For all other types, returns NULL and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			sptr<IBinder>	AsBinder(status_t* result = NULL) const;
			
			//!	Convert value to a weak IBinder pointer.
			/*!	Converts from the following types:
					- B_BINDER_TYPE: Returned as a weak pointer.
					- B_BINDER_HANDLE_TYPE: Returns binder proxy as a weak pointer.
					- B_WEAK_BINDER_TYPE: Returns as-is.
					- B_WEAK_BINDER_HANDLE_TYPE: Returns binder proxy.
					- B_NULL_TYPE: Returns NULL.
				For all other types, returns NULL and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			wptr<IBinder>	AsWeakBinder(status_t* result = NULL) const;
			
			//!	Convert value to a SString.
			/*!	Converts from the following types:
					- B_STRING_TYPE: Returned as-is.
					- B_DOUBLE_TYPE: Converted to a string using "%g".
					- B_FLOAT_TYPE: Converted to a string using "%g".
					- B_INT64_TYPE, B_BIGTIME_TYPE: Converted using "%Ld".
					- B_INT32_TYPE: Convered to a string using "%ld".
					- B_INT16_TYPE, B_INT8_TYPE: Converted using "%d".
					- B_BOOL_TYPE: Converted to the string "true" or "false".
					- B_NULL_TYPE: Returns "".
				For all other types, returns "" and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			SString			AsString(status_t* result = NULL) const;
			
			//!	Convert value to boolean.
			/*!	Converts from the following types:
					- B_BOOL_TYPE: Returned as-is.
					- B_STRING_TYPE: Converted to true if "true" or numeric > 0.
					- B_DOUBLE_TYPE, B_FLOAT_TYPE, B_INT64_TYPE, B_BIGTIME_TYPE,
						B_INT32_TYPE, B_INT16_TYPE, B_INT8_TYPE:
						Converted to true if value is not zero.
					- B_ATOM_TYPE: Converted to true if address is not NULL.
					- B_ATOM_WEAK_TYPE: Converted to true if address is not NULL.
					- B_BINDER_TYPE, B_BINDER_HANDLE_TYPE,
						B_WEAK_BINDER_TYPE, B_WEAK_BINDER_HANDLE_TYPE:
						Converted to true if address is not NULL.
					- B_NULL_TYPE: Returns false.
				For all other types, returns false and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			bool			AsBool(status_t* result = NULL) const;
			
			//!	Convert value to int32_t.
			/*!	Converts from the following types:
					- B_INT32_TYPE: Returned as-is.
					- B_STRING_TYPE: Converted to a number for the formats
						- "[+-]nnn" -- base 10 conversion.
						- "[+-]0xnnn" -- base 16 conversion.
						- "[+-]0nnn" -- base 8 conversion.
						- "[aaaa]" -- packed ASCII conversion.
						- "'aaaa'" -- packed ASCII conversion.
					- B_DOUBLE_TYPE, B_FLOAT_TYPE, B_INT64_TYPE, B_BIGTIME_TYPE,
						B_INT16_TYPE, B_INT8_TYPE:
						Converted to an int32_t, losing precision as needed.
						No bounds checking is performed.
					- B_BOOL_TYPE: Returned a 1 or 0.
					- B_NULL_TYPE: Returns 0.
				For all other types, returns 0 and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			int32_t			AsInt32(status_t* result = NULL) const;
			
			//!	Convert value to int64_t.
			/*!	Converts from the following types:
					- B_INT64_TYPE: Returned as-is.
					- B_STRING_TYPE: Converted as per AsInt32() above.
					- B_DOUBLE_TYPE, B_FLOAT_TYPE, B_BIGTIME_TYPE,
						B_INT32_TYPE, B_INT16_TYPE, B_INT8_TYPE:
						Converted to an int64_t, losing precision as needed.
						No bounds checking is performed.
					- B_BOOL_TYPE: Returned a 1 or 0.
					- B_NULL_TYPE: Returns 0.
				For all other types, returns 0 and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			int64_t			AsInt64(status_t* result = NULL) const;
			
			//!	Convert value to nsecs_t.
			/*!	Converts from the following types:
					- B_BIGTIME_TYPE: Returned as-is.
					- B_STRING_TYPE: Converted using strtoll().
					- B_DOUBLE_TYPE, B_FLOAT_TYPE, B_INT64_TYPE,
						B_INT32_TYPE, B_INT16_TYPE, B_INT8_TYPE:
						Converted to a nsecs_t, losing precision as needed.
						No bounds checking is performed.
					- B_BOOL_TYPE: Returned a 1 or 0.
					- B_NULL_TYPE: Returns 0.
				For all other types, returns 0 and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			nsecs_t			AsTime(status_t* result = NULL) const;
			
			//!	Convert value to float.
			/*!	Converts from the following types:
					- B_FLOAT_TYPE: Returned as-is.
					- B_STRING_TYPE: Converted using strtod().
					- B_DOUBLE_TYPE, B_INT64_TYPE, B_BIGTIME_TYPE,
						B_INT32_TYPE, B_INT16_TYPE, B_INT8_TYPE:
						Converted to a float, losing precision as needed.
						No bounds checking is performed.
					- B_BOOL_TYPE: Returned a 1 or 0.
					- B_NULL_TYPE: Returns 0.0.
				For all other types, returns 0.0 and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			float			AsFloat(status_t* result = NULL) const;
			
			//!	Convert value to double.
			/*!	Converts from the following types:
					- B_DOUBLE_TYPE: Returned as-is.
					- B_STRING_TYPE: Converted using strtod().
					- B_FLOAT_TYPE, B_INT64_TYPE, B_BIGTIME_TYPE,
						B_INT32_TYPE, B_INT16_TYPE, B_INT8_TYPE:
						Converted to a double, losing precision as needed.
					- B_BOOL_TYPE: Returned a 1 or 0.
					- B_NULL_TYPE: Returns 0.0.
				For all other types, returns 0.0 and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			double			AsDouble(status_t* result = NULL) const;
			
			//!	Convert value to a strong SAtom pointer.
			/*!	Converts from the following types:
					- B_ATOM_TYPE: Returned as-is.
					- B_ATOM_WEAK_TYPE: Attempts to promote to strong pointer.
					- B_NULL_TYPE: Returns NULL.
				For all other types, returns NULL and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			sptr<SAtom>		AsAtom(status_t* result = NULL) const;
			
			//!	Convert value to a weak SAtom pointer.
			/*!	Converts from the following types:
					- B_ATOM_TYPE: Returned as a weak pointer.
					- B_ATOM_WEAK_TYPE: Returns as-is.
					- B_NULL_TYPE: Returns NULL.
				For all other types, returns NULL and 'result' is set
				to the current error/status code or B_BAD_TYPE.
			*/
			wptr<SAtom>		AsWeakAtom(status_t* result = NULL) const;
	
			//!	Convert value to a wrapped PalmOS KeyID, SKeyID.
			/*!	We don't actually do any conversions here.  If the SValue doesn't
				contain a valid SKeyID, we set result = B_BAD_TYPE and return NULL.
			*/
			sptr<SKeyID>	AsKeyID(status_t* result = NULL) const;
			
			//!	Synonym for AsInt64().
	inline	off_t			AsOffset(status_t* result = NULL) const { return (off_t)AsInt64(result); }
			
			/*! \deprecated Use AsInt32() instead. */
	inline	int32_t			AsInteger(status_t* result = NULL) const { return AsInt32(result); }
			
	//@}

	/*!	@name Typed Data Access
		Retrieve data in value as a specific type, not performing conversion.
		(These functions fail if the value is not exactly the requested type.) */
	//@{
	
			status_t		GetBinder(sptr<IBinder> *obj) const;
			status_t		GetWeakBinder(wptr<IBinder> *obj) const;
			status_t		GetString(const char** a_string) const;
			status_t		GetString(SString* a_string) const;
			status_t		GetBool(bool* val) const;
			status_t		GetInt8(int8_t* val) const;
			status_t		GetInt16(int16_t* val) const;
			status_t		GetInt32(int32_t* val) const;
			status_t		GetInt64(int64_t* val) const;
			status_t		GetStatus(status_t* val) const;
			status_t		GetTime(nsecs_t* val) const;
			status_t		GetFloat(float* a_float) const;
			status_t		GetDouble(double* a_double) const;
			status_t		GetAtom(sptr<SAtom>* atom) const;
			status_t		GetWeakAtom(wptr<SAtom>* atom) const;
			status_t		GetKeyID(sptr<SKeyID> *keyid) const;

	//@}

	/*!	@name Printing
		Function for printing values in a nice format. */
	//@{
			//!	Print the value's contents in a pretty format.
			status_t		PrintToStream(const sptr<ITextOutput>& io, uint32_t flags = 0) const;
			
			//!	SValue pretty printer function.
			/*!	\param	io		The stream to print in.
				\param	val		The value to print.
				\param	flags	Additional formatting information.
			*/
	typedef	status_t			(*print_func)(	const sptr<ITextOutput>& io,
												const SValue& val,
												uint32_t flags);
	
			//!	Add a new printer function for a type code.
			/*!	When PrintToStream() is called, values of the given \a type
			 * will be printed by \a func.
			*/
	static	status_t			RegisterPrintFunc(type_code type, print_func func);
			//!	Remove printer function for type code.
	static	status_t			UnregisterPrintFunc(type_code type, print_func func=NULL);

	//@}

	//! This is for internal use, do not use directly.
	static	const SValue&	LookupConstantValue(int32_t index);

private:
	
	friend	class			BValueMap;
	
			// These are technically public because some inline methods call them.
			void			InitAsCopy(const SValue& o);
			void			InitAsMap(const SValue& key, const SValue& value);
			void			InitAsMap(const SValue& key, const SValue& value, uint32_t flags);
			void			InitAsMap(const SValue& key, const SValue& value, uint32_t flags, size_t numMappings);
			void			InitAsRaw(type_code type, const void* data, size_t len);
			void			FreeData();

			void*			alloc_data(type_code type, size_t len);
			void			init_as_shared_buffer(type_code type, const SSharedBuffer* buffer);
			bool			is_defined() const;
			bool			is_wild() const;
			bool			is_null() const;
			bool			is_error() const;
			bool			is_final() const;	// wild or error
			bool			is_simple() const;	// !is_map()
			bool			is_map() const;
			bool			is_object() const;
			bool			is_specified() const;
			int32_t			compare(uint32_t type, const void* data, size_t length) const;
			status_t		remove_item_index(size_t index);
			status_t		find_item_index(size_t index,
											SValue* out_key, SValue* out_value) const;
			void			shrink_map(BValueMap* map);
	static	int32_t			compare_map(const SValue* key1, const SValue* value1,
										const SValue* key2, const SValue* value2);
			void*			edit_data(size_t len);
			status_t		copy_small_data(type_code type, void* buf, size_t len) const;
			status_t		copy_big_data(type_code type, void* buf, size_t len) const;
			ssize_t			unarchive_internal(SParcel& from, size_t amount);
			BValueMap**		edit_map();
			BValueMap**		make_map_without_sets();
			status_t		set_error(ssize_t code);
			status_t		type_conversion_error() const;
			bool			check_integrity() const;
			
			// --- THE STATE ---
			// It is no coincidence that the following structure is exactly
			// the same as a small_flat_data...  and in fact we count on that.

			uint32_t		m_type;

			union {
				// out-of-line storage
				const SSharedBuffer*	buffer;		// B_TYPE_LENGTH_BIG -- block of data
				const BValueMap*		map;		// B_TYPE_LENGTH_MAP -- value mappings

				// variations on inline storage
				uint8_t		local[4];
				char		string[4];
				int32_t		integer;
				uint32_t	uinteger;
				void*		object;
			}				m_data;
};

/*----- Type and STL utilities --------------------------------------*/
inline void		BMoveBefore(SValue* to, SValue* from, size_t count);
inline void		BMoveAfter(SValue* to, SValue* from, size_t count);
inline void		BSwap(SValue& v1, SValue& v2);
inline int32_t	BCompare(const SValue& v1, const SValue& v2);
inline void		swap(SValue& x, SValue& y);

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SValue& value);

// -----------------------------------------------------------------
/*!	@name Type-specific Optimizations and Sequence Marshalling
	Some higher level TypeFuncs that need to know about SValue
	BArrayAsValue & BArrayConstruct 

	These are some implementations of the generic type functions
	to optimize the performance of SVector, SSortedVector,
	SKeyedVector, and other type-specific functions when used
	with these types.

	In addition, a these provide the custom marshalling code
	needed for these types to marshal/unmarshal an SVector
	of the type (called by PerformAsValue and PerformSetFromValue).
	These are needed for IDL sequences; they are specialized
	for plain old data, but to marshal an SVector of a user type,
	you must use one of the implementation macros or write
	your own implementation.

	Options:

	- If you can treat your type as a stream of bits		: B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS()
	- If your type is derived from SFlattenable				: B_IMPLEMENT_SFLATTENABLE_FLATTEN_FUNCS()
	- If your type implements
	   SValue TYPE::AsValue() and TYPE::TYPE(const SValue&)	: B_IMPLEMENT_FLATTEN_FUNCS()
	- If you have an interface								: B_IMPLEMENT_IINTERFACE_FLATTEN_FUNCS()
	- If you have an IBinder								: (implemented explicitly, no action required)

	Please note that sequences do not support SVector< wptr<TYPE> >.
*/
//@{

// -----------------------------------------------------------------
// Specializations for plain old data types
// -----------------------------------------------------------------

extern SSharedBuffer* BArrayAsValueHelper(void*& toPtr, size_t count, size_t elementSize, type_code typeCode);
extern const void* BArrayConstructHelper(const SValue& value, size_t count, size_t elementSize, type_code typeCode);

//!	Optimization for simple types that can be flattened/restored by just a copy
/*!	@note Ignores endianness issues. */
#define B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(TYPE, TYPECODE)									\
	inline SValue BArrayAsValue(const TYPE* from, size_t count)									\
	{																							\
		void* voidPtr = NULL;																	\
		SSharedBuffer* buffer = BArrayAsValueHelper(voidPtr, count, sizeof(TYPE), TYPECODE);	\
		TYPE* toPtr = static_cast<TYPE*>(voidPtr);												\
		BCopy(toPtr, from, count);																\
		SValue value(B_FIXED_ARRAY_TYPE, buffer);												\
		buffer->DecUsers();																		\
		return value;																			\
	}																							\
	inline status_t BArrayConstruct(TYPE* to, const SValue& value, size_t count)							\
	{																										\
		const TYPE* fromPtr = NULL;																			\
		fromPtr = static_cast<const TYPE*>(BArrayConstructHelper(value, count, sizeof(TYPE), TYPECODE));	\
		if (fromPtr != NULL) {																				\
			BCopy(to, fromPtr, count);																		\
		}																									\
		else {																								\
			return B_BAD_DATA;																				\
		}																									\
		return B_OK;																						\
	}																										\

// -----------------------------------------------------------------
// Specializations for types derived from SFlattenable
// -----------------------------------------------------------------

#define B_IMPLEMENT_SFLATTENABLE_FLATTEN_FUNCS(TYPE)								\
	inline SValue BArrayAsValue(const TYPE* from, size_t count)						\
	{																				\
		SValue result;																\
		for (size_t i = 0; i < count; i++) {										\
			result.JoinItem(SSimpleValue<int32_t>(i), from->AsValue());				\
			from++;																	\
		}																			\
		return result;																\
	}																				\
	inline status_t BArrayConstruct(TYPE* to, const SValue& value, size_t count)	\
	{																				\
		BConstruct(to, count);														\
		for (size_t i = 0; i < count; i++) {										\
			to->SetFromValue(value[SSimpleValue<int32_t>(i)]);						\
			to++;																	\
		}																			\
		return B_OK;																\
	}																				\

// -----------------------------------------------------------------
// Specializations for types implementing
// SValue TYPE::AsValue() and TYPE::TYPE(const SValue&)
// -----------------------------------------------------------------

#define B_IMPLEMENT_FLATTEN_FUNCS(TYPE)												\
	inline SValue BArrayAsValue(const TYPE* from, size_t count)						\
	{																				\
		SValue result;																\
		for (size_t i = 0; i < count; i++) {										\
			result.JoinItem(SSimpleValue<int32_t>(i), from->AsValue());				\
			from++;																	\
		}																			\
		return result;																\
	}																				\
	inline status_t BArrayConstruct(TYPE* to, const SValue& value, size_t count)	\
	{																				\
		for (size_t i = 0; i < count; i++) {										\
		*to = TYPE(value[SSimpleValue<int32_t>(i)]);								\
			to++;																	\
		}																			\
		return B_OK;																\
	}																				\

// -----------------------------------------------------------------
// Specializations for sptr<IInterface>
// -----------------------------------------------------------------

#define B_IMPLEMENT_IINTERFACE_FLATTEN_FUNCS(TYPE)											\
	inline SValue BArrayAsValue(const sptr<TYPE>* from, size_t count)						\
	{																						\
		SValue result;																		\
		for (size_t i = 0; i < count; i++) {												\
			result.JoinItem(SSimpleValue<int32_t>(i), SValue::Binder((*from)->AsBinder()));	\
			from++;																			\
		}																					\
		return result;																		\
	}																						\
	inline status_t BArrayConstruct(sptr<TYPE>* to, const SValue& value, size_t count)		\
	{																						\
		BConstruct(to, count);																\
		for (size_t i = 0; i < count; i++) {												\
			/* We know that only objects of TYPE are in sequence */							\
			*to = TYPE::AsInterfaceNoInspect(value[SSimpleValue<int32_t>(i)]);				\
			to++;																			\
		}																					\
		return B_OK;																		\
	}																						\

// -----------------------------------------------------------------
// Specializations for sptr<IBinder>
// -----------------------------------------------------------------

inline SValue BArrayAsValue(const sptr<IBinder>* from, size_t count)
{
	SValue result;
	for (size_t i = 0; i < count; i++) {
		result.JoinItem(SSimpleValue<int32_t>(i), SValue::Binder(*from));
		from++;
	}
	return result;
}

inline status_t BArrayConstruct(sptr<IBinder>* to, const SValue& value, size_t count)
{
	BConstruct(to, count);
	for (size_t i = 0; i < count; i++) {
		*to = value[SSimpleValue<int32_t>(i)].AsBinder();
		to++;
	}
	return B_OK;
}

// -----------------------------------------------------------------
// Specializations for marshalling SVector<SValue>
// -----------------------------------------------------------------

SValue BArrayAsValue(const SValue* from, size_t count);
status_t BArrayConstruct(SValue* to, const SValue& value, size_t count);

//@}

/*!	@} */

/*-------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------*/

B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(bool, B_BOOL_TYPE)
B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(int8_t, B_INT8_TYPE)
B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(uint8_t, B_UINT8_TYPE)
B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(int16_t, B_INT16_TYPE)
B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(uint16_t, B_UINT16_TYPE)
B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(int32_t, B_INT32_TYPE)
B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(uint32_t, B_UINT32_TYPE)
B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(int64_t, B_INT64_TYPE)
// nsecs_t specialization not needed...  it is handled by int64_t 
B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(uint64_t, B_UINT64_TYPE)
B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(float, B_FLOAT_TYPE)
B_IMPLEMENT_SIMPLE_TYPE_FLATTEN_FUNCS(double, B_DOUBLE_TYPE)

// Making this constructor inline is a nice performance optimization,
// but it could potentially have binary compatibility issues -- it means
// that we can never change the fact that an undefined SValue contains
// a first integer with the constant below and second integer of garbage.
inline SValue::SValue()
	: m_type(B_UNDEFINED_TYPE)
{
}

inline SValue::SValue(const SValue& o)
{
	//printf("*** Creating SValue %p from SValue\n", this);
	InitAsCopy(o);
}

inline SValue::SValue(const SValue& key, const SValue& value)
{
	//printf("*** Creating SValue %p from SValue->SValue\n", this);
	InitAsMap(key, value);
}

inline SValue::SValue(const SValue& key, const SValue& value, uint32_t flags)
{
	//printf("*** Creating SValue %p from SValue->SValue\n", this);
	InitAsMap(key, value, flags);
}

inline SValue::SValue(const SValue& key, const SValue& value, uint32_t flags, size_t numMappings)
{
	//printf("*** Creating SValue %p from SValue->SValue\n", this);
	InitAsMap(key, value, flags, numMappings);
}

inline SValue::SValue(type_code type, const void* data, size_t len)
{
	//printf("*** Creating SValue %p from type,data,len\n", this);
	InitAsRaw(type, data, len);
}

// Likewise for this constructor.
inline SValue::SValue(int32_t num)
	: m_type(B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)))
{
	m_data.integer = num;
}

inline SValue::SValue(const static_small_string_value& str)
{
	InitAsCopy(static_cast<const SValue&>(str));
}

inline SValue::SValue(const static_large_string_value& str)
{
	InitAsCopy(static_cast<const SValue&>(str));
}

inline SValue::~SValue()
{
	//printf("*** Destroying SValue %p\n", this);
	FreeData();
}

inline void BMoveBefore(SValue* to, SValue* from, size_t count)
{
	SValue::MoveBefore(to, from, count);
}

inline void BMoveAfter(SValue* to, SValue* from, size_t count)
{
	SValue::MoveAfter(to, from, count);
}

inline void BSwap(SValue& v1, SValue& v2)
{
	v1.Swap(v2);
}

inline int32_t BCompare(const SValue& v1, const SValue& v2)
{
	return v1.Compare(v2);
}

inline void swap(SValue& x, SValue& y)
{
	x.Swap(y);
}

inline bool SValue::IsDefined() const
{
	return m_type != B_UNDEFINED_TYPE; //kUndefinedTypeCode
}

inline bool SValue::IsWild() const
{
	return m_type == B_PACK_SMALL_TYPE(B_WILD_TYPE, 0); //kWildTypeCode;
}

inline bool SValue::IsSimple() const
{
	return B_UNPACK_TYPE_LENGTH(m_type) != B_TYPE_LENGTH_MAP;
}

inline bool SValue::IsSpecified() const
{
	return IsDefined() && !IsWild();
}

BNS(} })	// namespace palmos::support

#endif	/* _SUPPORT_VALUE_H_ */
