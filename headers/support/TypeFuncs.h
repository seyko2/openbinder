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

#ifndef _SUPPORT_TYPEFUNCS_H
#define _SUPPORT_TYPEFUNCS_H

/*!	@file support/TypeFuncs.h
	@ingroup CoreSupportUtilities
	@brief Templatized functions for various common type operations.
*/

#include <support/SupportDefs.h>

#include <string.h>
#include <new>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/*--------------------------------------------------------*/
/*----- Basic operations on types ------------------------*/

/*!	@name Generic Type Functions */
//@{

template<class TYPE>
inline void BConstruct(TYPE* base)
{
	new(base) TYPE;
}

template<class TYPE>
inline void BDestroy(TYPE* base)
{
	base->~TYPE();
}

template<class TYPE>
inline void BCopy(TYPE* to, const TYPE* from)
{
	new(to) TYPE(*from);
}

template<class TYPE>
inline void BReplicate(TYPE* to, const TYPE* protoElement)
{
	new(to) TYPE(*protoElement);
}

template<class TYPE>
inline void BMoveBefore(TYPE* to, TYPE* from)
{
	new(to) TYPE(*from);
	from->~TYPE();
}

template<class TYPE>
inline void BMoveAfter(TYPE* to, TYPE* from)
{
	new(to) TYPE(*from);
	from->~TYPE();
}

template<class TYPE>
inline void BAssign(TYPE* to, const TYPE* from)
{
	*to = *from;
}



template<class TYPE>
inline void BConstruct(TYPE* base, size_t count)
{
	while (--count != (size_t)-1) {
		new(base) TYPE;
		base++;
	}
}

template<class TYPE>
inline void BDestroy(TYPE* base, size_t count)
{
	while (--count != (size_t)-1) {
		base->~TYPE();
		base++;
	}
}

template<class TYPE>
inline void BCopy(TYPE* to, const TYPE* from, size_t count)
{
	while (--count != (size_t)-1) {
		new(to) TYPE(*from);
		to++;
		from++;
	}
}


template<class TYPE>
inline void BReplicate(TYPE* to, const TYPE* protoElement, size_t count)
{
	while (--count != (size_t)-1) {
		new(to) TYPE(*protoElement);
		to++;
	}
}

template<class TYPE>
inline void BMoveBefore(	TYPE* to, TYPE* from, size_t count)
{
	while (--count != (size_t)-1) {
		new(to) TYPE(*from);
		from->~TYPE();
		to++;
		from++;
	}
}

template<class TYPE>
inline void BMoveAfter(	TYPE* to, TYPE* from, size_t count)
{
	to+=(count-1);
	from+=(count-1);
	while (--count != (size_t)-1) {
		new(to) TYPE(*from);
		from->~TYPE();
		to--;
		from--;
	}
}

template<class TYPE>
inline void BAssign(	TYPE* to, const TYPE* from, size_t count)
{
	while (--count != (size_t)-1) {
		*to = *from;
		to++;
		from++;
	}
}

template<class TYPE>
inline void BSwap(TYPE& v1, TYPE& v2)
{
	TYPE tmp(v1); v1 = v2; v2 = tmp;
}

template<class TYPE>
inline int32_t BCompare(const TYPE& v1, const TYPE& v2)
{
	return (v1 < v2) ? -1 : ( (v2 < v1) ? 1 : 0 );
}

template<class TYPE>
inline bool BLessThan(const TYPE& v1, const TYPE& v2)
{
	return v1 < v2;
}

//@}

/*!	@} */

#if !defined(_MSC_VER)

/*--------------------------------------------------------*/
/*----- Optimizations for all pointer types --------------*/

template<class TYPE>
inline void BConstruct(TYPE** base, size_t count = 1)
	{ (void)base; (void)count; }
template<class TYPE>
inline void BDestroy(TYPE** base, size_t count = 1)
	{ (void)base; (void)count; }
template<class TYPE>
inline void BCopy(TYPE** to, TYPE* const * from, size_t count = 1)
	{ if (count == 1) *to = *from; else memcpy(to, from, sizeof(TYPE*)*count); }
template<class TYPE>
inline void BMoveBefore(TYPE** to, TYPE** from, size_t count = 1)
	{ if (count == 1) *to = *from; else memmove(to, from, sizeof(TYPE*)*count); }
template<class TYPE>
inline void BMoveAfter(TYPE** to, TYPE** from, size_t count = 1)
	{ if (count == 1) *to = *from; else memmove(to, from, sizeof(TYPE*)*count); }
template<class TYPE>
inline void BAssign(TYPE** to, TYPE* const * from, size_t count = 1)
	{ if (count == 1) *to = *from; else memcpy(to, from, sizeof(TYPE*)*count); }

#endif

/*--------------------------------------------------------*/
/*----- Optimizations for basic data types ---------------*/

// Standard optimizations for types that don't contain internal or
// other pointers to themselves.
#define B_IMPLEMENT_SIMPLE_TYPE_FUNCS(TYPE)												\
inline void BMoveBefore(TYPE* to, TYPE* from, size_t count)							\
	{ memmove(to, from, sizeof(TYPE)*count); }											\
inline void BMoveAfter(TYPE* to, TYPE* from, size_t count)								\
	{ memmove(to, from, sizeof(TYPE)*count); }											\

// Extreme optimizations for types whose constructor and destructor
// don't need to be called.
#define B_IMPLEMENT_BASIC_TYPE_FUNCS(TYPE)												\
inline void BConstruct(TYPE* base, size_t count)										\
	{ (void)base; (void)count; }														\
inline void BDestroy(TYPE* base, size_t count)											\
	{ (void)base; (void)count; }														\
inline void BCopy(TYPE* to, const TYPE* from, size_t count)							\
	{ if (count == 1) *to = *from; else memcpy(to, from, sizeof(TYPE)*count); }		\
inline void BReplicate(TYPE* to, const TYPE* protoElement, size_t count)				\
	{ while (--count != (size_t)-1) { *to = *protoElement; to++; } }					\
inline void BMoveBefore(TYPE* to, TYPE* from, size_t count)							\
	{ if (count == 1) *to = *from; else memmove(to, from, sizeof(TYPE)*count); }		\
inline void BMoveAfter(TYPE* to, TYPE* from, size_t count)								\
	{ if (count == 1) *to = *from; else memmove(to, from, sizeof(TYPE)*count); }		\
inline void BAssign(TYPE* to, const TYPE* from, size_t count)							\
	{ if (count == 1) *to = *from; else memcpy(to, from, sizeof(TYPE)*count); }		\

B_IMPLEMENT_BASIC_TYPE_FUNCS(bool)
B_IMPLEMENT_BASIC_TYPE_FUNCS(int8_t)
B_IMPLEMENT_BASIC_TYPE_FUNCS(uint8_t)
B_IMPLEMENT_BASIC_TYPE_FUNCS(int16_t)
B_IMPLEMENT_BASIC_TYPE_FUNCS(uint16_t)
B_IMPLEMENT_BASIC_TYPE_FUNCS(int32_t)
B_IMPLEMENT_BASIC_TYPE_FUNCS(uint32_t)
B_IMPLEMENT_BASIC_TYPE_FUNCS(int64_t)
B_IMPLEMENT_BASIC_TYPE_FUNCS(uint64_t)
B_IMPLEMENT_BASIC_TYPE_FUNCS(float)
B_IMPLEMENT_BASIC_TYPE_FUNCS(double)

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif
