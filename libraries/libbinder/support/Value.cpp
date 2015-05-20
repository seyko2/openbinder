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

#include <support/Value.h>
#include <support/KeyedVector.h>
#include <support/Atom.h>
#include <support/Debug.h>
#include <support/ITextStream.h>
#include <support/String.h>
#include <support/TypeConstants.h>
#include <support/Binder.h>
#include <support/Parcel.h>
#include <support/Looper.h>
#include <support/SharedBuffer.h>
#include <support/StdIO.h>
#include <support/StringIO.h>

#include <support_p/SupportMisc.h>
#include <support_p/ValueMap.h>
#include <support_p/WindowsCompatibility.h>
#include <support_p/binder_module.h>

#include <math.h>
#include <ctype.h>
#include <float.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ValueInternal.h"

#ifndef VALIDATES_VALUE
#define VALIDATES_VALUE 0
#endif

#ifndef DISABLE_COW
#define DISABLE_COW 0
#endif

#if VALIDATES_VALUE
#define DB_INTEGRITY_CHECKS 1
#define DB_CORRECTNESS_CHECKS 1
#else
#if BUILD_TYPE == BUILD_TYPE_DEBUG
#define DB_INTEGRITY_CHECKS 1
#else
#define DB_INTEGRITY_CHECKS 0
#endif
#define DB_CORRECTNESS_CHECKS 0
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#if _SUPPORTS_NAMESPACE
using namespace palmos::osp;
#endif

#if DB_INTEGRITY_CHECKS
#define CHECK_INTEGRITY(v) (v).check_integrity()
#else
#define CHECK_INTEGRITY(v)
#endif

#if DB_CORRECTNESS_CHECKS
static void check_correctness(const SValue& mine, const SValue& correct,
	const SValue& opLeft, const char* opName, const SValue& opRight)
{
	if (mine != correct) {
		sptr<BStringIO> sio(new BStringIO);
		sptr<ITextOutput> io(sio.ptr());
		io << "Correctness check failed!\nWhile computing:\n"
			<< indent << opLeft << dedent << endl
			<< opName << endl
			<< indent << opRight << dedent << endl
			<< "Result:\n"
			<< indent << (mine) << dedent << endl
			<< "Should have been:\n"
			<< indent << correct << dedent << endl;
		ErrFatalError(sio->String());
	}
}
#define INIT_CORRECTNESS(ops) ops
#define CHECK_CORRECTNESS(mine, correct, left, op, right) check_correctness((mine), (correct), (left), (op), (right))
#else
#define INIT_CORRECTNESS(ops)
#define CHECK_CORRECTNESS(mine, correct, left, op, right)
#endif

#if SUPPORTS_TEXT_STREAM
struct printer_registry {
	SLocker lock;
	SKeyedVector<type_code, SValue::print_func> funcs;
	
#if !(__CC_ARM)
	printer_registry()	: lock("SValue printer registry"), funcs((SValue::print_func)NULL) { }
#else
	printer_registry()	: lock("SValue printer registry"), funcs() { }
#endif
};

static int32_t g_havePrinters = 0;
static printer_registry* g_printers = NULL;

static printer_registry* Printers()
{
	if (g_havePrinters&2) return g_printers;
	if (atomic_or(&g_havePrinters, 1) == 0) {
		g_printers = new B_NO_THROW printer_registry;
		atomic_or(&g_havePrinters, 2);
	} else {
		if ((g_havePrinters&2) == 0) 
		   SysThreadDelay(B_MILLISECONDS(10), B_RELATIVE_TIMEOUT);
	}
	return g_printers;
}
#endif

#if SUPPORTS_TEXT_STREAM
static const char* UIntToHex(uint64_t val, char* out)
{
	sprintf(out, "0x%" B_FORMAT_INT64 "x", val);
	return out;
}
#endif

status_t SValue::RegisterPrintFunc(type_code type, print_func func)
{
#if SUPPORTS_TEXT_STREAM
	printer_registry* reg = Printers();
	if (reg && reg->lock.Lock() == B_OK) {
		reg->funcs.AddItem(type, func);
		reg->lock.Unlock();
	}
	return B_OK;
#else
	(void)type;
	(void)func;
	return B_UNSUPPORTED;
#endif
}

status_t SValue::UnregisterPrintFunc(type_code type, print_func func)
{
#if SUPPORTS_TEXT_STREAM
	printer_registry* reg = Printers();
	if (reg && reg->lock.Lock() == B_OK) {
		bool found;
		print_func f = reg->funcs.ValueFor(type, &found);
		if (found && (f == func || func == NULL))
			reg->funcs.RemoveItemFor(type);
		reg->lock.Unlock();
	}
	return B_OK;
#else
	(void)type;
	(void)func;
	return B_UNSUPPORTED;
#endif
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

void SValue::FreeData()
{
	const ssize_t s = B_UNPACK_TYPE_LENGTH(m_type);
	if ((s < sizeof(void*)) || ((s == sizeof(void*)) && !is_object()))
		return;

	switch (s) {
		case B_TYPE_LENGTH_LARGE:
			m_data.buffer->DecUsers();
#if BUILD_TYPE == BUILD_TYPE_DEBUG
			m_type = 0xdeadf00d;
			m_data.uinteger = 0xdeadf00d;
#endif
			return;
		case B_TYPE_LENGTH_MAP:
			m_data.map->DecUsers();
#if BUILD_TYPE == BUILD_TYPE_DEBUG
			m_type = 0xdeadf00d;
			m_data.uinteger = 0xdeadf00d;
#endif
			return;
		case sizeof(void*):
			release_object(*(small_flat_data*)this, this);
#if BUILD_TYPE == BUILD_TYPE_DEBUG
			m_type = 0xdeadf00d;
			m_data.uinteger = 0xdeadf00d;
#endif
			return;
	}
}

void* SValue::alloc_data(type_code type, size_t len)
{
	DbgOnlyFatalErrorIf(type == B_UNDEFINED_TYPE, "B_UNDEFINED_TYPE not valid here.");

	if (len <= B_TYPE_LENGTH_MAX) {
		m_type = B_PACK_SMALL_TYPE(type, len);
		return m_data.local;
	}
	
	if ((m_data.buffer=SSharedBuffer::Alloc(len)) != NULL) {
		m_type = B_PACK_LARGE_TYPE(type);
		return const_cast<void*>(m_data.buffer->Data());
	}

	m_type = kErrorTypeCode;
	m_data.integer = B_NO_MEMORY;
	return NULL;
}

void SValue::shrink_map(BValueMap* map)
{
	switch (map->CountMaps()) {
		case 0: {
			Undefine();
		} break;
		case 1: {
			const BValueMap::pair& p = map->MapAt(0);
			if (p.key.is_wild()) {
				map->IncUsers();
				*this = p.value;
				map->DecUsers();
			}
		} break;
	}
}

// -----------------------------------------------------------------------

SValue::SValue(type_code type, const SSharedBuffer* buffer)
{
	//printf("*** Creating SValue %p from type,SSharedBuffer\n", this);
	VALIDATE_TYPE(type);
	if (buffer) {
		init_as_shared_buffer(type, buffer);
	} else {
		DbgOnlyFatalErrorIf(type == B_UNDEFINED_TYPE, "B_UNDEFINED_TYPE not valid here.");
		m_type = B_PACK_SMALL_TYPE(type, 0);
	}
}

SValue::SValue(const char* str)
{
	//printf("*** Creating SValue %p from string\n", this);
	if (str)
		InitAsRaw(B_STRING_TYPE, str, strlen(str)+1);
	else {
		m_type = B_PACK_SMALL_TYPE(B_STRING_TYPE, 1);
		m_data.local[0] = 0;
	}
}

SValue::SValue(const SString& str)
{
	//printf("*** Creating SValue %p from SString\n", this);
	const SSharedBuffer* sb = str.SharedBuffer();
	if (sb) {
		init_as_shared_buffer(B_STRING_TYPE, sb);
	} else {
		m_type = B_PACK_SMALL_TYPE(B_STRING_TYPE, 1);
		m_data.integer = 0;
	}
}

SValue::SValue(const sptr<IBinder>& binder)
{
	//printf("*** Creating SValue %p from sptr<IBinder>\n", this);
	flatten_binder(binder, (small_flat_data*)this);
	acquire_object(*(small_flat_data*)this, this);
}

SValue::SValue(const sptr<SKeyID>& keyid)
{
	m_type = B_PACK_SMALL_TYPE(B_KEY_ID_TYPE, sizeof(void*));
	m_data.object = reinterpret_cast<void*>(&(*keyid));
	acquire_object(*(small_flat_data*)this, this);
}

SValue::SValue(const wptr<IBinder>& binder)
{
	//printf("*** Creating SValue %p from wptr<IBinder>\n", this);
	flatten_binder(binder, (small_flat_data*)this);
	acquire_object(*(small_flat_data*)this, this);
}

// ---------------------------------------------------------------------

SValue SValue::Int64(int64_t value)
{
	return SValue(B_INT64_TYPE, &value, sizeof(value));
}

SValue SValue::Time(nsecs_t value)
{
	return SValue(B_NSECS_TYPE, &value, sizeof(value));
}

SValue SValue::Double(double value)
{
	return SValue(B_DOUBLE_TYPE, &value, sizeof(value));
}

SValue SValue::Atom(const sptr<SAtom>& value)
{
	return SValue(B_ATOM_TYPE, &value, sizeof(value));
}

SValue SValue::WeakAtom(const wptr<SAtom>& value)
{
	return SValue(B_ATOM_WEAK_TYPE, &value, sizeof(value));
}

SValue SValue::Int8(int8_t v)		{ return SValue( SSimpleValue<int8_t>(v) ); }
SValue SValue::Int16(int16_t v)		{ return SValue( SSimpleValue<int16_t>(v) ); }
SValue SValue::Bool(bool v)			{ return SValue( SSimpleValue<bool>(v) ); }
SValue SValue::Float(float v)		{ return SValue( SSimpleValue<float>(v) ); }

#if 0
static int32_t read_num(const char** p)
{
	int32_t num=0;
	while(**p && isdigit(**p)) {
		num = (num*10) + (**p - '0');
		(*p)++;
	}
	return num;
}
#endif

SValue SValue::Status(status_t error)
{
	return SValue(B_STATUS_TYPE, &error, sizeof(error));
}

#if 0
SValue SValue::SSize(ssize_t val)
{
	SValue result(val);
	if (error < B_OK) result.set_error(error);
	return result;
}
#endif

// ---------------------------------------------------------------------

status_t SValue::StatusCheck() const
{
	status_t val = ErrorCheck();
	
	if (val == B_OK) {
		switch (m_type) {
			case B_PACK_SMALL_TYPE(B_STATUS_TYPE, sizeof(status_t)):
				return m_data.integer;
			case kUndefinedTypeCode:
				return B_NO_INIT;
		}
	}

	return val;
}

status_t SValue::ErrorCheck() const
{
	return is_error() ? (status_t)m_data.integer : B_OK;
}

void SValue::SetError(status_t error)
{
	set_error(error);
}

SValue& SValue::Assign(const SValue& o)
{
	if (this != &o) {
		FreeData();
		InitAsCopy(o);
	}
	
	return *this;
}

SValue& SValue::Assign(type_code type, const void* data, size_t len)
{
	VALIDATE_TYPE(type);
	FreeData();
	InitAsRaw(type, data, len);
	
	return *this;
}

void SValue::InitAsCopy(const SValue& o)
{
	m_data = o.m_data;
	m_type = o.m_type;
	switch (B_UNPACK_TYPE_LENGTH(m_type)) {
		case B_TYPE_LENGTH_LARGE:
			m_data.buffer->IncUsers();
#if DISABLE_COW
			m_data.buffer = o.m_data.buffer->Edit();
#endif
			CHECK_INTEGRITY(*this);
			return;
		case B_TYPE_LENGTH_MAP:
			m_data.map->IncUsers();
			CHECK_INTEGRITY(*this);
			return;
		case sizeof(void*):
			if (is_object()) acquire_object(*(small_flat_data*)this, this);
			CHECK_INTEGRITY(*this);
			return;
	}
	
	CHECK_INTEGRITY(*this);
}

void SValue::InitAsRaw(type_code type, const void* data, size_t len)
{
	DbgOnlyFatalErrorIf(type == B_UNDEFINED_TYPE, "B_UNDEFINED_TYPE not valid here.");

	VALIDATE_TYPE(type);

	if (len <= B_TYPE_LENGTH_MAX) {
		memcpy(m_data.local, data, len);
		m_type = B_PACK_SMALL_TYPE(type, len);
		if (CHECK_IS_SMALL_OBJECT(m_type)) {
			acquire_object(*(small_flat_data*)this, this);
		}
		CHECK_INTEGRITY(*this);
		return;
	}

	if ((m_data.buffer=SSharedBuffer::Alloc(len)) != NULL) {
		m_type = B_PACK_LARGE_TYPE(type);
		memcpy(const_cast<void*>(m_data.buffer->Data()), data, len);
		CHECK_INTEGRITY(*this);
		return;
	}

	m_type = kErrorTypeCode;
	m_data.integer = B_NO_MEMORY;
	CHECK_INTEGRITY(*this);
	return;
}

void SValue::InitAsMap(const SValue& key, const SValue& value)
{
	InitAsMap(key, value, 0, 1);
}

void SValue::InitAsMap(const SValue& key, const SValue& value, uint32_t flags)
{
	InitAsMap(key, value, flags, 1);
}

void SValue::InitAsMap(const SValue& key, const SValue& value, uint32_t flags, size_t numMappings)
{
	if (!key.is_error() && !value.is_error()) {
		if (key.is_defined() && value.is_defined()) {
			if (!key.is_wild()) {
				if (!key.is_map() || (flags&B_NO_VALUE_FLATTEN)) {
					m_type = kMapTypeCode;
					BValueMap* map = BValueMap::Create(numMappings);
					m_data.map = map;
					if (map) {
						map->SetFirstMap(key, value);
					} else {
						set_error(B_NO_MEMORY);
					}
				} else {
					m_type = kUndefinedTypeCode;
					size_t i=0;
					SValue k, v;
					while (key.GetNextItem(reinterpret_cast<void**>(&i), &k, &v) >= B_OK) {
						JoinItem(k, SValue(v, value));
					}
				}
			} else {
				InitAsCopy(value);
			}
		} else {
			m_type = kUndefinedTypeCode;
		}
	} else if (key.is_error()) {
		InitAsCopy(key);
	} else {
		InitAsCopy(value);
	}
	
	CHECK_INTEGRITY(*this);
}

void SValue::init_as_shared_buffer(type_code type, const SSharedBuffer* buffer)
{
	DbgOnlyFatalErrorIf(type == B_UNDEFINED_TYPE, "B_UNDEFINED_TYPE not valid here.");

	if (buffer->Length() <= B_TYPE_LENGTH_MAX) {
		m_type = B_PACK_SMALL_TYPE(type, buffer->Length());
		m_data.integer = *(int32_t*)buffer->Data();
	} else {
		m_type = B_PACK_LARGE_TYPE(type);
		buffer->IncUsers();
		m_data.buffer = buffer;
#if DISABLE_COW
		m_data.buffer = buffer->Edit();
#endif
	}
}

// ---------------------------------------------------------------------

void SValue::MoveBefore(SValue* to, SValue* from, size_t count)
{
	memmove(to, from, sizeof(SValue)*count);
#if SUPPORTS_ATOM_DEBUG
	for (size_t i=0; i<count; i++) {
		if (to[i].is_object()) {
			rename_object(*(small_flat_data*)(to+i), to+i, from+i);
		}
	}
#endif
}

void SValue::MoveAfter(SValue* to, SValue* from, size_t count)
{
	memmove(to, from, sizeof(SValue)*count);
#if SUPPORTS_ATOM_DEBUG
	while (count > 0) {
		count--;
		if (to[count].is_object()) {
			rename_object(*(small_flat_data*)(to+count), to+count, from+count);
		}
	}
#endif
}

typedef uint8_t dummy_value[sizeof(SValue)];

void SValue::Swap(SValue& with)
{
	dummy_value buffer;

	memcpy(&buffer, &with, sizeof(SValue));
	memcpy(&with, this, sizeof(SValue));
	memcpy(this, &buffer, sizeof(SValue));
}

void SValue::Undefine()
{
	FreeData();
	m_type = kUndefinedTypeCode;
	CHECK_INTEGRITY(*this);
}

bool SValue::IsObject() const
{
	return CHECK_IS_SMALL_OBJECT(m_type);
}

status_t SValue::CanByteSwap() const
{
	return is_type_swapped(B_UNPACK_TYPE_CODE(m_type));
}

status_t SValue::ByteSwap(swap_action action)
{
#if B_HOST_IS_LENDIAN
	if ((action == B_SWAP_HOST_TO_LENDIAN) || (action == B_SWAP_LENDIAN_TO_HOST))
		return B_OK;
#endif
#if B_HOST_IS_BENDIAN
	if ((action == B_SWAP_HOST_TO_BENDIAN) || (action == B_SWAP_BENDIAN_TO_HOST))
		return B_OK;
#endif
	
	const size_t len = Length();

	void* data = edit_data(len);
	CHECK_INTEGRITY(*this);
	if (!data) return B_BAD_VALUE;
	return swap_data(B_UNPACK_TYPE_CODE(m_type), data, len, action);
}

SValue& SValue::Join(const SValue& from, uint32_t flags)
{
	// If 'from' is wild or an error, propagate it through.
	if (from.is_final()) {
		*this = from;
	
	// If we currently have a value, and that value is not simple or is not
	// the same as 'from', then we need to build a mapping.
	} else if (is_specified() && (is_map() || this->Compare(from) != 0)) {
		size_t i=0;
		SValue key, value;
		SValue *cur;
		BValueMap **map = NULL;
		
		while (from.GetNextItem(reinterpret_cast<void**>(&i), &key, &value) >= B_OK) {
		
			if (!key.is_wild() && (cur=BeginEditItem(key)) != NULL) {
				// Combine with an existing key.
				if (!(flags&B_NO_VALUE_RECURSION))
					cur->Join(value, flags);
				else
					cur->Assign(value);
				EndEditItem(cur);
				map = NULL;
			} else if (map != NULL || (map=edit_map()) != NULL) {
				// Add a new key.
				const ssize_t result = BValueMap::AddNewMap(map, key, value);
				if (result < B_OK && result != B_BAD_VALUE && result != B_NAME_IN_USE) {
					set_error(result);
				}
			}
		
		}
	
	// If we currently aren't defined, then we're now just 'from'.
	} else if (!is_defined()) {
		*this = from;
	
	}
	
	CHECK_INTEGRITY(*this);
	return *this;
}

SValue & SValue::JoinItem(const SValue& key, const SValue& value, uint32_t flags)
{
	SValue *cur;
	BValueMap **map;
	
	if (!is_final() && key.is_defined() && value.is_defined()) {
		// Most of these checks are to ensure that the value stays
		// normalized, by not creating a BValueMap unless really needed.
		const bool wildkey = key.is_wild();
		if (wildkey && (!is_defined() || value.is_final())) {
			// Handle (wild -> A) special cases.
			Assign(value);
		} else if (wildkey && value.is_map()) {
			// Handle (wild -> (B -> ...)) special case.
			Join(value, flags);
		} else if (value.is_null()) {
			// Handle (* -> null) special case.
			Join(key, flags);
		} else if (key.is_map() || key.is_error() || value.is_error()) {
			// Need to normalize keys or propagate error.
			Join(SValue(key, value, flags), flags);
		} else if (!wildkey && (cur=BeginEditItem(key)) != NULL) {
			// Combine with an existing key.
			if (!(flags&B_NO_VALUE_RECURSION))
				cur->Join(value, flags);
			else
				cur->Assign(value);
			EndEditItem(cur);
		} else if ((map=edit_map()) != NULL) {
			// Add a new key.
			const ssize_t result = BValueMap::AddNewMap(map, key, value);
			if (result < B_OK && result != B_BAD_VALUE && result != B_NAME_IN_USE) {
				set_error(result);
			}
		}
	}
	
	CHECK_INTEGRITY(*this);
	return *this;
}

const SValue SValue::JoinCopy(const SValue& from, uint32_t flags) const
{
	// XXX this could be optimized.
	SValue out(*this);
	out.Join(from, flags);
	return out;
}

SValue& SValue::Overlay(const SValue& from, uint32_t flags)
{
	// If 'from' is wild or an error, propagate it through.
	if (from.is_final()) {
		*this = from;
	
	// If we currently have a value, and that value is not simple or the
	// from value is not simple, then we need to build a mapping.
	} else if (is_specified() && (is_map() || from.is_map())) {
		size_t i=0;
		SValue key, value;
		SValue *cur;
		BValueMap **map = NULL;
		bool setCleared = false;
		
		while (from.GetNextItem(reinterpret_cast<void**>(&i), &key, &value) >= B_OK) {
		
			if (!key.is_wild() && (cur=BeginEditItem(key)) != NULL) {
				// Combine with an existing key.
				if (!(flags&B_NO_VALUE_RECURSION))
					cur->Overlay(value, flags);
				else
					cur->Assign(value);
				EndEditItem(cur);
				map = NULL;
			} else {
				// Add a new key.
				if (!setCleared && key.is_wild()) {
					// A single item replaces all other set items.
					setCleared = true;
					map = make_map_without_sets();
				}
				if (map != NULL || (map=edit_map()) != NULL) {
					const ssize_t result = BValueMap::AddNewMap(map, key, value);
					if (result < B_OK && result != B_BAD_VALUE && result != B_NAME_IN_USE) {
						set_error(result);
					}
				}
			}
		
		}
	
		if (setCleared) shrink_map(*map);

	// If we aren't an error, overwrite with new value.
	} else if (!is_error() && from.is_defined()) {
		*this = from;
	
	}
	
	CHECK_INTEGRITY(*this);
	return *this;
}

const SValue SValue::OverlayCopy(const SValue& from, uint32_t flags) const
{
	// XXX this could be optimized.
	SValue out(*this);
	out.Overlay(from, flags);
	return out;
}

SValue& SValue::Inherit(const SValue& from, uint32_t flags)
{
	INIT_CORRECTNESS(SValue correct(from.OverlayCopy(*this)); SValue orig(*this));
	
	// If 'from' is an error, propagate it through.
	if (from.is_error()) {
		*this = from;
	
	// If we currently have a value, then we need to see if there are any
	// new values to add.
	} else if (is_specified()) {
		size_t i1=0, i2=0;
		SValue k1, v1, k2, v2;
		BValueMap** map = NULL;
		bool first = true, finish = false;
		bool terminals = false;
		bool termcheck = false;
		int32_t cmp;
		while (from.GetNextItem(reinterpret_cast<void**>(&i2), &k2, &v2) >= B_OK) {
			cmp = -1;
			while (!finish && (first || (cmp=compare_map(&k1,&v1,&k2,&v2)) < 0)) {
				if (GetNextItem(reinterpret_cast<void**>(&i1), &k1, &v1) < B_OK)
					finish = termcheck = true;
				else if (!terminals && !termcheck && k1.IsWild())
					terminals = true;
				first = false;
			}
			
			if (cmp != 0) {
				if (k2.is_wild()) {
					// Don't add in new terminal(s) if the current value
					// already has a terminal.
					if (!terminals && !termcheck) {
						size_t it = i1;
						SValue kt, vt;
						while (GetNextItem(reinterpret_cast<void**>(&it), &kt, &vt) >= B_OK
								&& !terminals) {
							if (kt.is_wild()) {
								terminals = true;
							}
						}
						termcheck = true;
					}
					if (!terminals) JoinItem(k2, v2, flags|B_NO_VALUE_FLATTEN);
				} else {
					JoinItem(k2, v2, flags|B_NO_VALUE_FLATTEN);
				}
				map = NULL;
			} else if (cmp == 0 && !(flags&B_NO_VALUE_RECURSION)) {
				if (!k2.is_wild()) {
					if (map != NULL || (map=edit_map()) != NULL) {
						SValue* cur = BValueMap::BeginEditMapAt(map, i1-1);
						if (cur) {
							cur->Inherit(v2, flags);
							BValueMap::EndEditMapAt(map);
						} else {
							set_error(B_NO_MEMORY);
						}
					}
				}
			}
		}
	} else if (!is_defined()) {
		*this = from;
	}
	
	CHECK_INTEGRITY(*this);
	CHECK_CORRECTNESS(*this, correct, orig, "Inherit", from);
	
	return *this;
}

const SValue SValue::InheritCopy(const SValue& from, uint32_t flags) const
{
	// XXX this could be optimized.
	SValue out(*this);
	out.Inherit(from, flags);
	return out;
}

SValue& SValue::MapValues(const SValue& from, uint32_t flags)
{
	// XXX this could be optimized.
	Assign(MapValuesCopy(from, flags));
	return *this;
}

const SValue SValue::MapValuesCopy(const SValue& from, uint32_t flags) const
{
	SValue out;
	
	#if DB_CORRECTNESS_CHECKS
		// This is the formal definition of MapValues().
		SValue correct;
		void* i=NULL;
		SValue key, value;
		while (GetNextItem(&i, &key, &value) >= B_OK) {
			correct.JoinItem(key, from.ValueFor(value, flags));
		}
	#endif
	
	if (!is_error()) {
		if (!from.is_final()) {
			void* i=NULL;
			SValue key1, value1, value2;
			while (GetNextItem(&i, &key1, &value1) >= B_OK) {
				value2 = from.ValueFor(value1, flags);
				if (value2.is_defined()) out.JoinItem(key1, value2);
			}
		} else if (from.is_wild() || is_final()) {
			out = *this;
		} else {
			out = from;
		}
	} else {
		out = *this;
	}
	
	CHECK_INTEGRITY(*this);
	CHECK_CORRECTNESS(out, correct, *this, "MapValues", from);
	
	return out;
}

//#if DB_CORRECTNESS_CHECKS
// This is the formal definition of Remove().
static SValue correct_remove(const SValue& lhs, const SValue& rhs, uint32_t flags)
{
	if (rhs.ErrorCheck() < B_OK) {
		return lhs.ErrorCheck() < B_OK ? lhs : rhs;
	} else if (lhs.IsSimple()) {
		// Terminal cases.
		if (rhs.IsSimple()) {
			return (!rhs.IsWild() && lhs != rhs) ? lhs : SValue::Undefined();
		} else {
			return !rhs.HasItem(SValue::Wild(), lhs) ? lhs : SValue::Undefined();
		}
	} else {
		void* i=NULL;
		SValue key, value;
		SValue result;
		if (!(flags&B_NO_VALUE_RECURSION)) {
			while (lhs.GetNextItem(&i, &key, &value) >= B_OK) {
				result.JoinItem(
					key,
					correct_remove(value, rhs.ValueFor(key), flags),
					flags|B_NO_VALUE_FLATTEN);
			}
		} else {
			while (lhs.GetNextItem(&i, &key, &value) >= B_OK) {
				if (!rhs.ValueFor(key).IsDefined()) {
					result.JoinItem(key, value, flags|B_NO_VALUE_FLATTEN);
				}
			}
		}
		return result;
	}
}
//#endif

SValue& SValue::Remove(const SValue& from, uint32_t flags)
{
	// XXX this could be optimized.
	Assign(RemoveCopy(from, flags));
	return *this;
}

const SValue SValue::RemoveCopy(const SValue& from, uint32_t flags) const
{
	#if DB_CORRECTNESS_CHECKS
		SValue correct(correct_remove(*this, from, flags));
	#endif
	
	// Some day this might be optimized.
	const SValue result(correct_remove(*this, from, flags));
	
	CHECK_INTEGRITY(result);
	CHECK_CORRECTNESS(result, correct, *this, "Remove", from);
	
	return result;
}

status_t SValue::RemoveItem(const SValue& key, const SValue& value)
{
	status_t result;
	
	if (!is_error() && key.is_defined()) {
		if (key.is_wild() &&
				(*this == value || value.is_wild())) {
			Undefine();
			result = B_OK;
		} else if (is_map()) {
			// Removing a simple value, or all values under a mapping?
			if (key.is_wild() || value.is_wild()) {
				BValueMap** map = edit_map();
				if (map) {
					result = BValueMap::RemoveMap(map, key, value);
					if (result >= B_OK) shrink_map(*map);
				} else {
					result = B_NAME_NOT_FOUND;
				}
			} else {
				SValue* cur = BeginEditItem(key);
				result = ErrorCheck();
				if (cur) {
					cur->Remove(value);
					EndEditItem(cur);
					result = ErrorCheck();
				} else if (result >= B_OK) {
					result = B_NAME_NOT_FOUND;
				}
			}
		} else {
			result = B_NAME_NOT_FOUND;
		}
	} else {
		result = ErrorCheck();
	}
	
	CHECK_INTEGRITY(*this);
	return result;
}

//#if DB_CORRECTNESS_CHECKS
// This is the formal definition of Retain().
static SValue correct_retain(const SValue& lhs, const SValue& rhs, uint32_t flags)
{
	if (rhs.ErrorCheck() < B_OK) {
		return lhs.ErrorCheck() < B_OK ? lhs : rhs;
	} else if (lhs.IsSimple()) {
		// Terminal cases.
		if (rhs.IsSimple()) {
			return (rhs.IsWild() || lhs == rhs)
					? lhs
					: (lhs.IsWild() ? rhs : SValue::Undefined());
		} else {
			return rhs.HasItem(SValue::Wild(), lhs) ? lhs : SValue::Undefined();
		}
	} else {
		void* i=NULL;
		SValue key, value;
		SValue result;
		if (!(flags&B_NO_VALUE_RECURSION)) {
			while (lhs.GetNextItem(&i, &key, &value) >= B_OK) {
				result.JoinItem(
					key,
					correct_retain(value, rhs.ValueFor(key), flags),
					flags|B_NO_VALUE_FLATTEN);
			}
		} else {
			while (lhs.GetNextItem(&i, &key, &value) >= B_OK) {
				if (rhs.ValueFor(key).IsDefined()) {
					result.JoinItem(key, value, flags|B_NO_VALUE_FLATTEN);
				}
			}
		}
		return result;
	}
}
//#endif

SValue& SValue::Retain(const SValue& from, uint32_t flags)
{
	// XXX this could be optimized.
	Assign(RetainCopy(from, flags));
	return *this;
}

const SValue SValue::RetainCopy(const SValue& from, uint32_t flags) const
{
	#if DB_CORRECTNESS_CHECKS
		SValue correct(correct_retain(*this, from, flags));
	#endif
	
	// Some day this might be optimized.
	const SValue result(correct_retain(*this, from, flags));
	
	CHECK_INTEGRITY(result);
	CHECK_CORRECTNESS(result, correct, *this, "Retain", from);
	
	return result;
}

status_t SValue::RenameItem(const SValue& old_key,
							const SValue& new_key)
{
	status_t result;
	
	if (is_map()) {
		BValueMap** map = edit_map();
		if (map) {
			result = BValueMap::RenameMap(map, old_key, new_key);
			if (result != B_OK && result != B_NAME_IN_USE
					&& result != B_NAME_NOT_FOUND && result != B_BAD_VALUE) {
				set_error(result);
			}
		} else if (is_error()) {
			result = ErrorCheck();
		} else {
			result = B_NAME_NOT_FOUND;
		}
	} else if (!is_error()) {
		result = B_NAME_NOT_FOUND;
	} else {
		result = ErrorCheck();
	}
	
	CHECK_INTEGRITY(*this);
	return result;
}

bool SValue::HasItem(const SValue& key, const SValue& value) const
{
	if (!is_error()) {
		if (is_wild()) {
			return true;
		} else if (key.is_wild()) {
			if (value.is_wild()) {
				return is_defined();
			} else if (is_map()) {
				return m_data.map->IndexFor(key, value) >= 0;
			} else {
				return *this == value;
			}
		} else {
			if (is_map()) {
				ssize_t index = m_data.map->IndexFor(key, value);
				if (index >= 0) return true;

				// Look in a set
				return m_data.map->IndexFor(B_WILD_VALUE,key) >= 0;
			} else if (key == *this)
				return true;
		}
	}
	return false;
}

const SValue SValue::ValueFor(const SValue& key, uint32_t flags) const
{
	if (!key.is_map() || (flags&B_NO_VALUE_RECURSION)) return ValueFor(key);

	if (is_error()) {
		return *this;
	} else if (is_wild()) {
		return key;
	} else {
 		// Index by entire hierarchy, for each map in the key.
		SValue val;
 		void* i=NULL;
 		SValue key1, value1, value2;
 		while (key.GetNextItem(&i, &key1, &value1) >= B_OK) {
 			value2 = ValueFor(key1, flags | B_NO_VALUE_RECURSION);
 			value2 = value2.ValueFor(value1, flags);
 			if (value2.is_defined()) val.Join(value2, flags);
 		}
		return val;
	}
	return SValue::Undefined();
}

const SValue& SValue::ValueFor(const SValue& key) const
{
	if (key.is_wild()) {
		return *this;
	} else if (is_map()) {
		ssize_t index = m_data.map->IndexFor(key);
//		bout << "SValue::ValueFor: " << index << endl;
		if (index >= B_OK) return m_data.map->MapAt(index).value;

		// Look in a set
		index = m_data.map->IndexFor(B_WILD_VALUE,key);
		if (index >= B_OK) return B_NULL_VALUE;
	} else if (is_wild()) {
		return key;
	} else if (key == *this) {
		return B_NULL_VALUE;
	} else if (is_error()) {
		return *this;
	}
	return SValue::Undefined();
}
	
const SValue& SValue::ValueFor(const char* key) const
{
	if (is_map()) {
		const size_t len = strlen(key)+1;
		const uint32_t type = len <= B_TYPE_LENGTH_MAX
			? B_PACK_SMALL_TYPE(B_STRING_TYPE, len) : B_PACK_LARGE_TYPE(B_STRING_TYPE);
		ssize_t index = m_data.map->IndexFor(type, key, len);
		if (index >= B_OK) return m_data.map->MapAt(index).value;

		// Look in a set
		index = m_data.map->IndexFor(B_WILD_VALUE,SValue::String(key));
		if (index >= B_OK) return B_NULL_VALUE;
	} else if (is_wild()) {
		DbgOnlyFatalError("ValueFor(const char*) from wild not supported!");
		return SValue::Undefined();
		//val = SValue(key);
	} else if (*this == key)
		return B_NULL_VALUE;

	return SValue::Undefined();
}

const SValue& SValue::ValueFor(const SString& key) const
{
	if (is_map()) {
		const size_t len = key.Length()+1;
		const uint32_t type = len <= B_TYPE_LENGTH_MAX
			? B_PACK_SMALL_TYPE(B_STRING_TYPE, len) : B_PACK_LARGE_TYPE(B_STRING_TYPE);
		ssize_t index = m_data.map->IndexFor(type, key.String(), len);
		if (index >= B_OK) return m_data.map->MapAt(index).value;

		// Look in a set
		index = m_data.map->IndexFor(B_WILD_VALUE,SValue::String(key));
		if (index >= B_OK) return B_NULL_VALUE;
	} else if (is_wild()) {
		DbgOnlyFatalError("ValueFor(SString) from wild not supported!");
		return SValue::Undefined();
		//val = SValue(key);
	} else if (*this == key)
		return B_NULL_VALUE;
	return SValue::Undefined();
}

const SValue& SValue::ValueFor(int32_t index) const
{
	// Optimize?
	return ValueFor(SSimpleValue<int32_t>(index));
}

int32_t SValue::CountItems() const
{
	return is_map() ? m_data.map->CountMaps() : ( is_defined() ? 1 : 0 );
}

status_t SValue::GetNextItem(void** cookie, SValue* out_key, SValue* out_value) const
{
	status_t result = find_item_index(*reinterpret_cast<size_t*>(cookie), out_key, out_value);
	if (result == B_OK) {
		*cookie = reinterpret_cast<void*>((*reinterpret_cast<size_t*>(cookie)) + 1);
	} else if (result == B_BAD_INDEX) {
		result = B_ENTRY_NOT_FOUND;
	}
	return result;
}

SValue SValue::Keys() const
{
	SValue val;

	if (!is_error()) {
		if (is_map()) {
			for (size_t index=0; index< m_data.map->CountMaps(); index++) {
				const BValueMap::pair& p = m_data.map->MapAt(index);
				if (!p.key.is_wild()) { // This entry is a set
					val.Join(p.key);
				}
			}
		} else if (is_defined()) {
			val = SValue::Undefined();
		} else {
			val = SValue::Undefined();
		}
	}
	
	CHECK_INTEGRITY(*this);
	return val;
}

SValue *SValue::BeginEditItem(const SValue& key)
{		
	SValue *val;
	if (key.is_wild()) {
		val = NULL;
	} else if (is_map()) {
		ssize_t index = m_data.map->IndexFor(key);
		if (index >= B_OK) {
			BValueMap** map = edit_map();
			val = map ? BValueMap::BeginEditMapAt(map, index) : NULL;
		} else {
			val = NULL;
		}
	} else {
		val = NULL;
	}
	
	return val;
}

void SValue::EndEditItem(SValue* cur)
{
	if (is_map()) {
		BValueMap** map = edit_map();
		if (map) {
			status_t err = cur->ErrorCheck();
			BValueMap::EndEditMapAt(map);
			if (err < B_OK) set_error(err);
			else shrink_map(*map);
		}
	} else {
		DbgOnlyFatalError("BeginEditItem has not been called");
	}
}

status_t SValue::remove_item_index(size_t index)
{
	status_t result;
	
	if (!is_error()) {
		if (is_map()) {
			const size_t N = m_data.map->CountMaps();
			if (index == 0 && N <= 1) {
				Undefine();
				result = B_OK;
			
			} else if (index < m_data.map->CountMaps()) {
				BValueMap** map = edit_map();
				if (map) {
					BValueMap::RemoveMapAt(map, index);
					shrink_map(*map);
					result = B_OK;
				} else {
					result = B_NO_MEMORY;
				}
			} else {
				result = B_BAD_INDEX;
			}
		} else if (index == 0) {
			Undefine();
			result = B_OK;
		} else {
			result = B_BAD_INDEX;
		}
	} else {
		result = ErrorCheck();
	}
	
	CHECK_INTEGRITY(*this);
	return result;
}

status_t SValue::find_item_index(size_t index, SValue* out_key, SValue* out_value) const
{
	status_t result;
	
	if (!is_error()) {
		if (is_map()) {
			if (index < m_data.map->CountMaps()) {
				const BValueMap::pair& p = m_data.map->MapAt(index);
				if (out_key) *out_key = p.key;
				if (out_value) *out_value = p.value;
				result = B_OK;
			} else {
				result = B_BAD_INDEX;
			}
		} else if (index == 0 && is_defined()) {
			if (out_key) *out_key = SValue::Wild();
			if (out_value) *out_value = *this;
			result = B_OK;
		} else {
			result = B_BAD_INDEX;
		}
	} else {
		result = ErrorCheck();
	}
	
	CHECK_INTEGRITY(*this);
	return result;
}

// ------------------------------------------------------------------------

#if VALIDATES_VALUE
#define FINISH_ARCHIVE(func)																\
	ssize_t total = func;																	\
	if (total != ArchivedSize()) {															\
		bout << "Written size: " << total << ", expected: " << ArchivedSize() << endl;		\
		bout << "Myself: " << *this << endl;												\
		bout << "The data so far: " << into << endl;										\
		ErrFatalError("Archived size wrong!");													\
	}																						\
	return total;
#else
#define FINISH_ARCHIVE(func) return func
#endif

ssize_t SValue::ArchivedSize() const
{
	const uint32_t len = B_UNPACK_TYPE_LENGTH(m_type);

	// Small data value.
	if (len <= B_TYPE_LENGTH_MAX) {
		if (!is_error())
			return is_object() ? sizeof (flat_binder_object) : sizeof(small_flat_data);
		return ErrorCheck();
	}

	// Large data value.
	if (len == B_TYPE_LENGTH_LARGE)
		return sizeof(large_flat_header) + value_data_align(m_data.buffer->Length());

	// Else -- it's a map.
	DbgOnlyFatalErrorIf(len != B_TYPE_LENGTH_MAP, "Archiving with unexpected length!");
	return sizeof(value_map_header) + m_data.map->ArchivedSize();
}

ssize_t SValue::Archive(SParcel& into) const
{
	const uint32_t len = B_UNPACK_TYPE_LENGTH(m_type);

	// Small data value.
	if (len <= B_TYPE_LENGTH_MAX) {
		if (!is_error()) {
			if (!is_object()) {
				// This is a small piece of data, and SValue happens to
				// store that in its flattened form.  How convenient!
				FINISH_ARCHIVE(into.WriteSmallData(*reinterpret_cast<const small_flat_data*>(this)));
			}

			// Objects require special handling because reference counts
			// must be maintained.  However, we store them internally
			// in the standard flattened structure, so we just need to
			// tell SParcel to keep track of the data we write.
#if BUILD_TYPE == BUILD_TYPE_DEBUG
			if (m_type == B_PACK_SMALL_TYPE(B_ATOM_TYPE, sizeof(void*))
					|| m_type == B_PACK_SMALL_TYPE(B_ATOM_WEAK_TYPE, sizeof(void*))) {
				ErrFatalError("Can't flatten SValue containing SAtom pointers (maybe you should have added binder objects instead).");
				return B_BAD_DATA;
			}
#endif
			// make a small_flat_data from the SValue: type, this, atom
			FINISH_ARCHIVE(into.WriteBinder(*reinterpret_cast<const small_flat_data*>(this)));
		}
		return ErrorCheck();
	}

	// Large data value.
	if (len == B_TYPE_LENGTH_LARGE) {
		FINISH_ARCHIVE(into.WriteLargeData(m_type, m_data.buffer->Length(), m_data.buffer->Data()));
	}

	// Else -- it's a map.

	DbgOnlyFatalErrorIf(len != B_TYPE_LENGTH_MAP, "Archiving with unexpected length!");

	value_map_header header;
	header.header.type = kMapTypeCode;
	header.header.length = sizeof(value_map_info) + value_data_align(m_data.map->ArchivedSize());
	header.info.count = m_data.map->CountMaps();
	header.info.order = 1;

	ssize_t err;
	if ((err=into.Write(&header, sizeof(header))) < 0) return err;

	err = m_data.map->Archive(into);
	if (err >= B_OK) {
		FINISH_ARCHIVE((sizeof(header)+err));
	}

	return err;
}

#undef FINISH_ARCHIVE

ssize_t SValue::Archive(const sptr<IByteOutput>& into) const
{
	SParcel p(into, NULL, NULL);
	const ssize_t s1 = Archive(p);
	if (s1 >= B_OK) {
		const ssize_t s2 = p.Flush();
		return s1 >= B_OK ? (s1+s2) : s2;
	}
	return s1;
}

ssize_t SValue::Unarchive(SParcel& from)
{
	if (is_defined()) Undefine();
	return unarchive_internal(from, 0x7fffffff);
}

ssize_t SValue::unarchive_internal(SParcel& from, size_t avail)
{
	ssize_t err = B_OK;

	if (avail < sizeof(small_flat_data)) goto error;

	small_flat_data head;
	if ((err=from.ReadSmallDataOrObject(&head, this)) >= sizeof(head)) {

		// Optimization -- avoid aliasing of 'head'
		const uint32_t htype = head.type;
		const uint32_t hdata = head.data.uinteger;

		const uint32_t len = B_UNPACK_TYPE_LENGTH(htype);
		if (len <= B_TYPE_LENGTH_MAX) {
			// Do the easy case first -- a small block of data.
			m_type = htype;
			m_data.uinteger = hdata;
			CHECK_INTEGRITY(*this);
			return err;
		}

		if (len == B_TYPE_LENGTH_LARGE) {
			//  Next case -- a large block of data.
			const size_t readSize = value_data_align(hdata);
			void* buf = alloc_data(htype, hdata);
			if (buf) {
				if ((err=from.Read(buf, readSize)) == readSize) {
					CHECK_INTEGRITY(*this);
					return sizeof(head) + readSize;
				}
				goto error;
			}
			// retrieve error from alloc_data().
			return ErrorCheck();
		}

		// Else -- it's a map.

		DbgOnlyFatalErrorIf(len != B_TYPE_LENGTH_MAP, "Unarchiving with unexpected length!");

		// Need to read the map info struct.
		value_map_info info;
		DbgOnlyFatalErrorIf(sizeof(value_map_info) != sizeof(small_flat_data), "Ooops!");
		if ((err=from.ReadSmallData((small_flat_data*)&info)) == sizeof(info)) {
			ssize_t retErr;	// don't use err so ADS doesn't alias it.
			BValueMap* obj = BValueMap::Create(from, avail-sizeof(value_map_header), info.count, &retErr);
			err = retErr;
			if (err >= 0) {
				// Plug in this value.
				m_type = kMapTypeCode;
				m_data.map = obj;
				CHECK_INTEGRITY(*this);
				return sizeof(value_map_header) + err;
			}
		}
	}

error:
	m_type = kUndefinedTypeCode;
	return set_error(err < B_OK ? err : B_BAD_DATA);
}

ssize_t SValue::Unarchive(const sptr<IByteInput>& from)
{
	// XXX LIMIT BUFFER READS TO NOT GO PAST VALUE DATA!!
	SParcel p(NULL, from, NULL);
	return Unarchive(p);
}

// ------------------------------------------------------------------------

// This is a comparison between values with two nice properties:
// * It is significantly faster than the standard SValue comparison,
//   as it doesn't try to put things in a "nice" order.  In practice,
//   the speedup is about 2x.
// * It has a clearly defined order, where-as the normal ordering of
//   BValues will change whenever we add special cases for other types.
//   This allows us to proclaim the following algorithm as the offical
//   order for items in flattened data.
int32_t SValue::Compare(const SValue& o) const
{
	if (m_type != o.m_type)
		return m_type < o.m_type ? -1 : 1;
	// Special case compare to put integers in a nice order.
	size_t len = B_UNPACK_TYPE_LENGTH(m_type);
	switch (len) {
		case 0:
			return 0;
		case 1:
		case 2:
		case 3:
			return memcmp(m_data.local, o.m_data.local, len);
		case 4:
			return (m_data.integer != o.m_data.integer)
				? ( (m_data.integer < o.m_data.integer) ? -1 : 1 )
				: 0;
		case B_TYPE_LENGTH_LARGE:
			len = m_data.buffer->Length();
			if (len != o.m_data.buffer->Length())
				return len < o.m_data.buffer->Length() ? -1 : 1;
			return memcmp(m_data.buffer->Data(), o.m_data.buffer->Data(), len);
		case B_TYPE_LENGTH_MAP:
			return m_data.map->Compare(*o.m_data.map);
	}
	return 0;
}

int32_t SValue::Compare(const char* str) const
{
	return Compare(B_STRING_TYPE, str, strlen(str)+1);
}

int32_t SValue::Compare(const SString& str) const
{
	return Compare(B_STRING_TYPE, str.String(), str.Length()+1);
}

int32_t SValue::Compare(type_code type, const void* data, size_t length) const
{
	return compare(length <= B_TYPE_LENGTH_MAX
		? B_PACK_SMALL_TYPE(type, length) : B_PACK_LARGE_TYPE(type),
		data, length);
}

// This version assumes 'type' has already been packed.
int32_t SValue::compare(uint32_t type, const void* data, size_t length) const
{
	if (m_type != type)
		return m_type < type ? -1 : 1;
	// Special case compare to put integers in a nice order.
	size_t len = B_UNPACK_TYPE_LENGTH(m_type);
	switch (len) {
		case 0:
			return 0;
		case 1:
		case 2:
		case 3:
			return memcmp(m_data.local, data, len);
		case 4:
			return (m_data.integer != *(int32_t*)data)
				? ( (m_data.integer < *(int32_t*)data) ? -1 : 1 )
				: 0;
		case B_TYPE_LENGTH_LARGE:
			len = m_data.buffer->Length();
			if (len != length)
				return len < length ? -1 : 1;
			return memcmp(m_data.buffer->Data(), data, len);
	}
	return 0;
}

#define TYPED_DATA_COMPARE(d1, d2, type)												\
	( (*reinterpret_cast<const type*>(d1)) < (*reinterpret_cast<const type*>(d2))		\
		? -1																			\
		: (	(*reinterpret_cast<const type*>(d1)) > (*reinterpret_cast<const type*>(d2))	\
			? 1 : 0 )																	\
	)

int32_t SValue::LexicalCompare(const SValue& o) const
{
	if (is_simple() && o.is_simple()) {
		if (B_UNPACK_TYPE_CODE(m_type) != B_UNPACK_TYPE_CODE(o.m_type))
			return m_type < o.m_type ? -1 : 1;
		if (B_UNPACK_TYPE_LENGTH(m_type) == B_UNPACK_TYPE_LENGTH(o.m_type)) {
			switch (m_type) {
				case kUndefinedTypeCode:
				case kWildTypeCode:
					return 0;
				case B_PACK_SMALL_TYPE(B_BOOL_TYPE, sizeof(int8_t)):
				case B_PACK_SMALL_TYPE(B_INT8_TYPE, sizeof(int8_t)):
					return TYPED_DATA_COMPARE(m_data.local, o.m_data.local, int8_t);
				case B_PACK_SMALL_TYPE(B_UINT8_TYPE, sizeof(uint8_t)):
					return TYPED_DATA_COMPARE(m_data.local, o.m_data.local, uint8_t);
				case B_PACK_SMALL_TYPE(B_INT16_TYPE, sizeof(int16_t)):
					return TYPED_DATA_COMPARE(m_data.local, o.m_data.local, int16_t);
				case B_PACK_SMALL_TYPE(B_UINT16_TYPE, sizeof(uint16_t)):
					return TYPED_DATA_COMPARE(m_data.local, o.m_data.local, uint16_t);
				case B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)):
					return TYPED_DATA_COMPARE(m_data.local, o.m_data.local, int32_t);
				case B_PACK_SMALL_TYPE(B_UINT32_TYPE, sizeof(uint32_t)):
					return TYPED_DATA_COMPARE(m_data.local, o.m_data.local, uint32_t);
				case B_PACK_SMALL_TYPE(B_FLOAT_TYPE, sizeof(float)):
					return TYPED_DATA_COMPARE(m_data.local, o.m_data.local, float);
				case B_INT64_TYPE:
					if (m_data.buffer->Length() == sizeof(int64_t)
						&& o.m_data.buffer->Length() == sizeof(int64_t)) {
						return TYPED_DATA_COMPARE(m_data.buffer->Data(), o.m_data.buffer->Data(), int64_t);
					}
					break;
				case B_UINT64_TYPE:
					if (m_data.buffer->Length() == sizeof(uint64_t)
						&& o.m_data.buffer->Length() == sizeof(uint64_t)) {
						return TYPED_DATA_COMPARE(m_data.buffer->Data(), o.m_data.buffer->Data(), uint64_t);
					}
					break;
				case B_DOUBLE_TYPE:
					if (m_data.buffer->Length() == sizeof(double)
						&& o.m_data.buffer->Length() == sizeof(double)) {
						return TYPED_DATA_COMPARE(m_data.buffer->Data(), o.m_data.buffer->Data(), double);
					}
					break;
			}
		}

		const size_t len = Length();
		const size_t olen = o.Length();

		int32_t result = memcmp(Data(), o.Data(), len < olen ? len : olen);
		if (result == 0 && len != olen) {
			return len < olen ? -1 : 1;
		}
		return 0;
	}
	if (m_type != o.m_type)
		return m_type < o.m_type ? -1 : 1;
	return m_data.map->LexicalCompare(*o.m_data.map);
}

#undef TYPED_DATA_COMPARE

int32_t SValue::compare_map(const SValue* key1, const SValue* value1,
							const SValue* key2, const SValue* value2)
{
	const SValue* cmp1;
	const SValue* cmp2;

	// Fix ordering of wild keys -- we want to order the value along
	// with similar non-wild keys.
	const bool key1Wild = key1->is_wild();
	const bool key2Wild = key2->is_wild();
	cmp1 = key1Wild ? value1 : key1;
	cmp2 = key2Wild ? value2 : key2;
	
	// First, quick check if types are different.  However,
	// DON'T do this case if key1 is wild, meaning both keys
	// are wild and we need to use the "simple" case below.
	if (cmp1->m_type != cmp2->m_type && !key1Wild) {
		return cmp1->m_type < cmp2->m_type ? -1 : 1;
	
	// Types aren't different, so they are both either a map
	// or simple.  First the simple case.
	} else if (cmp1->is_simple()) {
		const int32_t c = cmp1->Compare(*cmp2);
		// If they look the same, take into account whether one or
		// the other has a wild key.
		if (c == 0 && key1Wild != key2Wild) {
			return key1Wild ? -1 : 1;
		}
		return c;
	}
	
	// Types aren't different and both are maps.  Don't need to
	// worry about the wild key case here, because the construct
	// (wild -> (a -> b)) is not a normalized form.
	return key1->Compare(*key2);
}

// ------------------------------------------------------------------------

status_t SValue::GetBinder(sptr<IBinder> *obj) const
{
	if (is_object()) {
		return unflatten_binder(*(small_flat_data*)this, obj);
	}
	return B_BAD_TYPE;
}

status_t SValue::GetWeakBinder(wptr<IBinder> *obj) const
{
	if (is_object()) {
		return unflatten_binder(*(small_flat_data*)this, obj);
	}
	return B_BAD_TYPE;
}

status_t SValue::GetString(const char** a_string) const
{
	if (B_UNPACK_TYPE_CODE(m_type) != B_STRING_TYPE) return B_BAD_TYPE;
	*a_string = static_cast<const char*>(Data());
	return B_OK;
}

status_t SValue::GetString(SString* a_string) const
{
	if (B_UNPACK_TYPE_CODE(m_type) != B_STRING_TYPE) return B_BAD_TYPE;
	
	switch (B_UNPACK_TYPE_LENGTH(m_type)) {
		case B_TYPE_LENGTH_LARGE:
			*a_string = SString(m_data.buffer);
			return B_OK;
		case 0:
			*a_string = "";
			return B_OK;
	}

	const SSharedBuffer *buf = SharedBuffer();
	if (!buf)
		return B_NO_MEMORY;

	SString out(buf);
	*a_string = out;
	buf->DecUsers();

	return B_OK;
}

status_t SValue::GetKeyID(sptr<SKeyID> *val) const
{
	if (m_type == B_PACK_SMALL_TYPE(B_KEY_ID_TYPE, sizeof(void*))) {
		*val = reinterpret_cast<SKeyID*>(m_data.object);
		return B_OK;
	}
	return B_BAD_TYPE;
}

status_t SValue::GetBool(bool* val) const
{
	int8_t v;
	status_t err = copy_small_data(B_PACK_SMALL_TYPE(B_BOOL_TYPE, sizeof(int8_t)), &v, sizeof(int8_t));
	*val = v ? true : false;
	return err;
}

status_t SValue::GetInt8(int8_t* val) const
{
	return copy_small_data(B_PACK_SMALL_TYPE(B_INT8_TYPE, sizeof(*val)), val, sizeof(*val));
}

status_t SValue::GetInt16(int16_t* val) const
{
	return copy_small_data(B_PACK_SMALL_TYPE(B_INT16_TYPE, sizeof(*val)), val, sizeof(*val));
}

status_t SValue::GetInt32(int32_t* val) const
{
	return copy_small_data(B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(*val)), val, sizeof(*val));
}

status_t SValue::GetStatus(status_t* val) const
{
	return copy_small_data(B_PACK_SMALL_TYPE(B_STATUS_TYPE, sizeof(*val)), val, sizeof(*val));
}

status_t SValue::GetInt64(int64_t* val) const
{
	return copy_big_data(B_PACK_LARGE_TYPE(B_INT64_TYPE), val, sizeof(*val));
}

status_t SValue::GetTime(nsecs_t* val) const
{
	return copy_big_data(B_PACK_LARGE_TYPE(B_NSECS_TYPE), val, sizeof(*val));
}

status_t SValue::GetFloat(float* val) const
{
	return copy_small_data(B_PACK_SMALL_TYPE(B_FLOAT_TYPE, sizeof(*val)), val, sizeof(*val));
}

status_t SValue::GetDouble(double* val) const
{
	return copy_big_data(B_PACK_LARGE_TYPE(B_DOUBLE_TYPE), val, sizeof(*val));
}

status_t SValue::GetAtom(sptr<SAtom>* atom) const
{
	SAtom* a;
	status_t err = copy_small_data(B_PACK_SMALL_TYPE(B_ATOM_TYPE, sizeof(a)), &a, sizeof(a));
	if (err >= B_OK) *atom = a;
	return err;
}

status_t SValue::GetWeakAtom(wptr<SAtom>* atom) const
{
	SAtom::weak_atom_ptr* wp;
	status_t err = copy_small_data(B_PACK_SMALL_TYPE(B_ATOM_WEAK_TYPE, sizeof(wp)), &wp, sizeof(wp));
	if (err < B_OK) err = copy_small_data(B_PACK_SMALL_TYPE(B_ATOM_WEAK_TYPE, sizeof(wp)), &wp, sizeof(wp));
	if (err >= B_OK) *atom = wp->atom;
	return err;
}

status_t
SValue::type_conversion_error() const
{
	status_t err = StatusCheck();
	return err < B_OK ? err : B_BAD_TYPE;
}

status_t SValue::AsStatus(status_t* result) const
{
	status_t val = AsSSize(result);
	if (val <= B_OK) return val;

	if (result) *result = B_BAD_TYPE;
	return B_OK;
}

ssize_t SValue::AsSSize(status_t* result) const
{
	status_t val = ErrorCheck();
	status_t r = B_OK;
	
	if (val == B_OK) {
		if (m_type == B_PACK_SMALL_TYPE(B_STATUS_TYPE, sizeof(status_t))) {
			val = m_data.integer;
		} else {
			val = AsInt32(&r);
		}
	}
	
	if (result) *result = r;
	return val;
}

sptr<IBinder>
SValue::AsBinder(status_t *result) const
{
	if (is_object()) {
		sptr<IBinder> val;
		status_t err = unflatten_binder(*(small_flat_data*)this, &val);
		if (err != B_OK) {
			wptr<IBinder> wval;
			err = unflatten_binder(*(small_flat_data*)this, &wval);
			if (err == B_OK) {
				val = wval.promote();
			} else if (m_type == B_PACK_SMALL_TYPE(B_NULL_TYPE, 0)) {
				err = B_OK;
			}
		}
		if (result) *result = err;
		return val;
	}
		
	if (result) *result = type_conversion_error();
	return sptr<IBinder>();
}

wptr<IBinder>
SValue::AsWeakBinder(status_t *result) const
{
	if (is_object()) {
		wptr<IBinder> val;
		status_t err = unflatten_binder(*(small_flat_data*)this, &val);
		if (err != B_OK && m_type == B_PACK_SMALL_TYPE(B_NULL_TYPE, 0)) {
			err = B_OK;
		}
		if (result) *result = err;
		return val;
	}
		
	if (result) *result = type_conversion_error();
	return wptr<IBinder>();
}

#if 0
// Some code generation tests...
static int32_t testa_int32(volatile int32_t& v, int32_t* out)
{
	if ((v&0x80000000) == 0) {
		*out = v&0x7fffffff;
		return 1;
	}

	if ((v&0x80000000) == 0x80000000) {
		*out = v&0x80000000;
		return 2;
	}

	if ((v&0x00000001) == 0x00000000) {
		*out = v>>1;
		return 3;
	}

	if ((v&0x00000001) == 0x00000001) {
		*out = v&0x00000001;
		return 4;
	}

	if ((v&0x40000000) == 0) {
		*out = v&~0x40000000;
		return 21;
	}

	if ((v&0x40000000) == 0x40000000) {
		*out = v&0x40000000;
		return 22;
	}

	if ((v&0x00000002) == 0x00000000) {
		*out = v&~0x00000002;
		return 23;
	}

	if ((v&0x00000002) == 0x00000002) {
		*out = v&0x00000002;
		return 24;
	}

	if ((v&0x80000001) == 0x00000000) {
		*out = v&0x7ffffffe;
		return 5;
	}

	if ((v&0x80000001) == 0x80000001) {
		*out = v&0x80000001;
		return 6;
	}

	if ((v&0x80000001) == 0x00000001) {
		*out = v&0xfffffffe;
		return 7;
	}

	if ((v&0x80000001) == 0x80000001) {
		*out = v&0xfffffffe;
		return 8;
	}

	if ((v&0xc0000000) == 0x00000000) {
		*out = v&~0xc0000000;
		return 9;
	}

	if ((v&0xc0000000) == 0x40000000) {
		*out = v&0xc0000000;
		return 10;
	}

	if ((v&0xc0000000) == 0x80000000) {
		*out = v&~0xc0000000;
		return 11;
	}

	if ((v&0x00000003) == 0x00000000) {
		*out = v&~0x00000003;
		return 12;
	}

	if ((v&0x00000003) == 0x00000001) {
		*out = v&0x00000003;
		return 13;
	}

	if ((v&0x00000003) == 0x00000002) {
		*out = v&0x00000003;
		return 14;
	}

	if ((v&0x000f0000) == 0x00000000) {
		*out = v&0x000f0000;
		return 15;
	}

	if ((v&0x000f0000) == 0x000f0000)  {
		*out = v&~0x000f0000;
		return 16;
	}

	if ((v&0x0000f000) == 0x00000000) {
		*out = v&0x0000f000;
		return 17;
	}

	if ((v&0x0000f000) == 0x0000f000) {
		*out = v&~0x0000f000;
		return 18;
	}

	return 0;
}

static int32_t testb_int32(volatile int32_t& v, int32_t* out)
{
	int32_t result = 0;

	if ((v&0x80000000) == 0) {
		*out = v&0x7fffffff;
		result = 1;
	}

	else if ((v&0x80000000) == 0x80000000) {
		*out = v&0x80000000;
		result = 2;
	}

	else if ((v&0x00000001) == 0x00000000) {
		*out = v>>1;
		result = 3;
	}

	else if ((v&0x00000001) == 0x00000001) {
		*out = v&0x00000001;
		result = 4;
	}

	else if ((v&0x40000000) == 0) {
		*out = v&~0x40000000;
		result = 21;
	}

	else if ((v&0x40000000) == 0x40000000) {
		*out = v&0x40000000;
		result = 22;
	}

	else if ((v&0x00000002) == 0x00000000) {
		*out = v&~0x00000002;
		result = 23;
	}

	else if ((v&0x00000002) == 0x00000002) {
		*out = v&0x00000002;
		result = 24;
	}

	else if ((v&0x80000001) == 0x00000000) {
		*out = v&0x7ffffffe;
		result = 5;
	}

	else if ((v&0x80000001) == 0x80000001) {
		*out = v&0x80000001;
		result = 6;
	}

	else if ((v&0x80000001) == 0x00000001) {
		*out = v&0xfffffffe;
		result = 7;
	}

	else if ((v&0x80000001) == 0x80000001) {
		*out = v&0xfffffffe;
		result = 8;
	}

	else if ((v&0xc0000000) == 0x00000000) {
		*out = v&~0xc0000000;
		result = 9;
	}

	else if ((v&0xc0000000) == 0x40000000) {
		*out = v&0xc0000000;
		result = 10;
	}

	else if ((v&0xc0000000) == 0x80000000) {
		*out = v&~0xc0000000;
		result = 11;
	}

	else if ((v&0x00000003) == 0x00000000) {
		*out = v&~0x00000003;
		result = 12;
	}

	else if ((v&0x00000003) == 0x00000001) {
		*out = v&0x00000003;
		result = 13;
	}

	else if ((v&0x00000003) == 0x00000002) {
		*out = v&0x00000003;
		result = 14;
	}

	else if ((v&0x000f0000) == 0x00000000) {
		*out = v&0x000f0000;
		result = 15;
	}

	else if ((v&0x000f0000) == 0x000f0000)  {
		*out = v&~0x000f0000;
		result = 16;
	}

	else if ((v&0x0000f000) == 0x00000000) {
		*out = v&0x0000f000;
		result = 17;
	}

	else if ((v&0x0000f000) == 0x0000f000) {
		*out = v&~0x0000f000;
		result = 18;
	}

	return result;
}

static int32_t testa_uint32(volatile uint32_t& v, int32_t* out)
{
	if ((v&0x80000000) == 0) {
		*out = v&0x7fffffff;
		return 1;
	}

	if ((v&0x80000000) == 0x80000000) {
		*out = v&0x80000000;
		return 2;
	}

	if ((v&0x00000001) == 0x00000000) {
		*out = v>>1;
		return 3;
	}

	if ((v&0x00000001) == 0x00000001) {
		*out = v&0x00000001;
		return 4;
	}

	if ((v&0x40000000) == 0) {
		*out = v&~0x40000000;
		return 21;
	}

	if ((v&0x40000000) == 0x40000000) {
		*out = v&0x40000000;
		return 22;
	}

	if ((v&0x00000002) == 0x00000000) {
		*out = v&~0x00000002;
		return 23;
	}

	if ((v&0x00000002) == 0x00000002) {
		*out = v&0x00000002;
		return 24;
	}

	if ((v&0x80000001) == 0x00000000) {
		*out = v&0x7ffffffe;
		return 5;
	}

	if ((v&0x80000001) == 0x80000001) {
		*out = v&0x80000001;
		return 6;
	}

	if ((v&0x80000001) == 0x00000001) {
		*out = v&0xfffffffe;
		return 7;
	}

	if ((v&0x80000001) == 0x80000001) {
		*out = v&0xfffffffe;
		return 8;
	}

	if ((v&0xc0000000) == 0x00000000) {
		*out = v&~0xc0000000;
		return 9;
	}

	if ((v&0xc0000000) == 0x40000000) {
		*out = v&0xc0000000;
		return 10;
	}

	if ((v&0xc0000000) == 0x80000000) {
		*out = v&~0xc0000000;
		return 11;
	}

	if ((v&0x00000003) == 0x00000000) {
		*out = v&~0x00000003;
		return 12;
	}

	if ((v&0x00000003) == 0x00000001) {
		*out = v&0x00000003;
		return 13;
	}

	if ((v&0x00000003) == 0x00000002) {
		*out = v&0x00000003;
		return 14;
	}

	if ((v&0x000f0000) == 0x00000000) {
		*out = v&0x000f0000;
		return 15;
	}

	if ((v&0x000f0000) == 0x000f0000)  {
		*out = v&~0x000f0000;
		return 16;
	}

	if ((v&0x0000f000) == 0x00000000) {
		*out = v&0x0000f000;
		return 17;
	}

	if ((v&0x0000f000) == 0x0000f000) {
		*out = v&~0x0000f000;
		return 18;
	}

	return 0;
}

static int32_t testb_uint32(volatile uint32_t& v, int32_t* out)
{
	int32_t result = 0;

	if ((v&0x80000000) == 0) {
		*out = v&0x7fffffff;
		result = 1;
	}

	else if ((v&0x80000000) == 0x80000000) {
		*out = v&0x80000000;
		result = 2;
	}

	else if ((v&0x00000001) == 0x00000000) {
		*out = v>>1;
		result = 3;
	}

	else if ((v&0x00000001) == 0x00000001) {
		*out = v&0x00000001;
		result = 4;
	}

	else if ((v&0x40000000) == 0) {
		*out = v&~0x40000000;
		result = 21;
	}

	else if ((v&0x40000000) == 0x40000000) {
		*out = v&0x40000000;
		result = 22;
	}

	else if ((v&0x00000002) == 0x00000000) {
		*out = v&~0x00000002;
		result = 23;
	}

	else if ((v&0x00000002) == 0x00000002) {
		*out = v&0x00000002;
		result = 24;
	}

	else if ((v&0x80000001) == 0x00000000) {
		*out = v&0x7ffffffe;
		result = 5;
	}

	else if ((v&0x80000001) == 0x80000001) {
		*out = v&0x80000001;
		result = 6;
	}

	else if ((v&0x80000001) == 0x00000001) {
		*out = v&0xfffffffe;
		result = 7;
	}

	else if ((v&0x80000001) == 0x80000001) {
		*out = v&0xfffffffe;
		result = 8;
	}

	else if ((v&0xc0000000) == 0x00000000) {
		*out = v&~0xc0000000;
		result = 9;
	}

	else if ((v&0xc0000000) == 0x40000000) {
		*out = v&0xc0000000;
		result = 10;
	}

	else if ((v&0xc0000000) == 0x80000000) {
		*out = v&~0xc0000000;
		result = 11;
	}

	else if ((v&0x00000003) == 0x00000000) {
		*out = v&~0x00000003;
		result = 12;
	}

	else if ((v&0x00000003) == 0x00000001) {
		*out = v&0x00000003;
		result = 13;
	}

	else if ((v&0x00000003) == 0x00000002) {
		*out = v&0x00000003;
		result = 14;
	}

	else if ((v&0x000f0000) == 0x00000000) {
		*out = v&0x000f0000;
		result = 15;
	}

	else if ((v&0x000f0000) == 0x000f0000)  {
		*out = v&~0x000f0000;
		result = 16;
	}

	else if ((v&0x0000f000) == 0x00000000) {
		*out = v&0x0000f000;
		result = 17;
	}

	else if ((v&0x0000f000) == 0x0000f000) {
		*out = v&~0x0000f000;
		result = 18;
	}

	return result;
}
#endif

SString SValue::AsString(status_t* result) const
{
	// First check the fast and easy cases.
	switch (m_type) {
		case B_PACK_LARGE_TYPE(B_STRING_TYPE):
			if (result) *result = B_OK;
			return SString(m_data.buffer);
		case B_PACK_SMALL_TYPE(B_STRING_TYPE, 0):
		case B_PACK_SMALL_TYPE(B_NULL_TYPE, 0):
			if (result) *result = B_OK;
			return SString();
	}

	SString val;
	status_t r;
	switch (B_UNPACK_TYPE_CODE(m_type)) {
		case B_STRING_TYPE: {
			r = GetString(&val);
		} break;
		case B_DOUBLE_TYPE: {
			double ex;
			if ((r=GetDouble(&ex)) == B_OK) {
				char buffer[64];
				sprintf(buffer, "%g", ex);
				if( !strchr(buffer, '.') && !strchr(buffer, 'e') &&
					!strchr(buffer, 'E') ) {
					strncat(buffer, ".0", sizeof(buffer)-1);
				}
				val = buffer;
			}
		} break;
		case B_FLOAT_TYPE: {
			float ex;
			if ((r=GetFloat(&ex)) == B_OK) {
				char buffer[64];
				sprintf(buffer, "%g", (double)ex);
				if( !strchr(buffer, '.') && !strchr(buffer, 'e') &&
					!strchr(buffer, 'E') ) {
					strncat(buffer, ".0", sizeof(buffer)-1);
				}
				val = buffer;
			}
		} break;
		case B_INT64_TYPE: {
			int64_t ex;
			if ((r=GetInt64(&ex)) == B_OK) {
				char buffer[64];
				sprintf(buffer, "%" B_FORMAT_INT64 "d", ex);
				val = buffer;
			}
		} break;
		case B_NSECS_TYPE: {
			nsecs_t ex;
			if ((r=GetTime(&ex)) == B_OK) {
				char buffer[64];
				sprintf(buffer, "%" B_FORMAT_INT64 "d", ex);
				val = buffer;
			}
		} break;
		case B_INT32_TYPE: {
			int32_t ex;
			if ((r=GetInt32(&ex)) == B_OK) {
				char buffer[64];
				sprintf(buffer, "%ld", ex);
				val = buffer;
			}
		} break;
		case B_INT16_TYPE: {
			int16_t ex;
			if ((r=GetInt16(&ex)) == B_OK) {
				char buffer[64];
				sprintf(buffer, "%d", ex);
				val = buffer;
			}
		} break;
		case B_INT8_TYPE: {
			int8_t ex;
			if ((r=GetInt8(&ex)) == B_OK) {
				char buffer[64];
				sprintf(buffer, "%d", ex);
				val = buffer;
			}
		} break;
		case B_BOOL_TYPE: {
			bool ex;
			if ((r=GetBool(&ex)) == B_OK) {
				val = ex ? "true" : "false";
			}
		} break;
		case B_UUID_TYPE: {
			if (Length() == sizeof(uuid_t)) {
				uuid_t* u = (uuid_t*)Data();
				char buffer[64];
				sprintf(buffer, "%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
						u->time_low,
						u->time_mid,
						u->time_hi_and_version,
						u->clock_seq_hi_and_reserved,
						u->clock_seq_low,
						u->node[0], u->node[1], u->node[2], u->node[3], u->node[4], u->node[5]);
				val = buffer;
				r = B_OK;
			} else {
				r = type_conversion_error();
			}
		} break;
		default:
			r = type_conversion_error();
	}
#if 0
	volatile int32_t ini = r;
	volatile uint32_t inu = r;
	int32_t out = 0;
	testa_int32(ini, &out);
	testb_int32(ini, &out);
	testa_uint32(inu, &out);
	testb_uint32(inu, &out);
#endif
	if (result) *result = r;
	return val;
}

bool SValue::AsBool(status_t* result) const
{
	// First check the fast and easy cases.
	switch (m_type) {
		case B_PACK_SMALL_TYPE(B_BOOL_TYPE, sizeof(int8_t)):
			if (result) *result = B_OK;
			return (*reinterpret_cast<const int8_t*>(m_data.local)) ? true : false;
		case B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)):
			if (result) *result = B_OK;
			return m_data.integer ? true : false;
		case B_PACK_SMALL_TYPE(B_NULL_TYPE, 0):
			if (result) *result = B_OK;
			return false;
	}

	bool val = false;
	status_t r;
	switch (B_UNPACK_TYPE_CODE(m_type)) {
		case B_STRING_TYPE: {
			const char* ex;
			if ((r=GetString(&ex)) == B_OK) {
				if (strcasecmp(ex, "true") == 0) val = true;
				else val = strtol(ex, const_cast<char**>(&ex), 10) != 0 ? true : false;
			}
		} break;
		case B_DOUBLE_TYPE: {
			double ex;
			if ((r=GetDouble(&ex)) == B_OK) {
				val = (ex > DBL_EPSILON || ex < (-DBL_EPSILON)) ? true : false;
			}
		} break;
		case B_FLOAT_TYPE: {
			float ex;
			if ((r=GetFloat(&ex)) == B_OK) {
				val = (ex > FLT_EPSILON || ex < (-FLT_EPSILON)) ? true : false;
			}
		} break;
		case B_INT64_TYPE: {
			int64_t ex;
			if ((r=GetInt64(&ex)) == B_OK) {
				val = ex != 0 ? true : false;
			}
		} break;
		case B_NSECS_TYPE: {
			nsecs_t ex;
			if ((r=GetTime(&ex)) == B_OK) {
				val = ex != 0 ? true : false;
			}
		} break;
		case B_INT16_TYPE: {
			int16_t ex;
			if ((r=GetInt16(&ex)) == B_OK) {
				val = ex != 0 ? true : false;
			}
		} break;
		case B_INT8_TYPE: {
			int8_t ex;
			if ((r=GetInt8(&ex)) == B_OK) {
				val = ex != 0 ? true : false;
			}
		} break;
		case B_ATOM_TYPE: {
			sptr<SAtom> ex;
			if ((r=GetAtom(&ex)) == B_OK) {
				val = ex != NULL ? true : false;
			}
		} break;
		case B_ATOM_WEAK_TYPE: {
			wptr<SAtom> ex;
			if ((r=GetWeakAtom(&ex)) == B_OK) {
				val = ex != NULL ? true : false;
			}
		} break;
		default: {
			wptr<IBinder> binder = AsWeakBinder(&r);
			if (r == B_OK) {
				val = binder != NULL ? true : false;
			} else {
				r = type_conversion_error();
			}
		}
	}
	if (result) *result = r;
	return val;
}

sptr<SKeyID>
SValue::AsKeyID(status_t* result) const
{
	sptr<SKeyID> val;
	status_t r;
	switch (B_UNPACK_TYPE_CODE(m_type)) {
		case B_KEY_ID_TYPE:
			r = GetKeyID(&val);
			break;
		case B_NULL_TYPE:
			r = B_OK;
			break;
		default:
			r = type_conversion_error();
	}
	if (result) *result = r;
	return val;
}

static int64_t string_to_number(const char* str, status_t* outResult)
{
	int64_t val = 0;
	bool neg = false;

	while (isspace(*str)) str++;
	if (str[0] == '+') str++;
	if (str[0] == '-') {
		neg = true;
		str++;
	}
	if (str[0] >= '0' && str[0] <= '9') {
		// It's a real, honest-to-ghod number...
		int32_t base = 10;
		if (str[0] == '0' && str[1] == 'x') {
			str += 2;
			base = 16;
		}
		val = strtoll(str, const_cast<char**>(&str), base);
		*outResult = B_OK;
	} else if (str[0] == '[' || str[0] == '\'') {
		const char end = (str[0] == '[') ? ']' : '\'';
		val = 0;
		str++;
		while (str[0] != 0 && str[0] != end) {
			val = (val<<8) | str[0];
			str++;
		}
		if (str[0] == end) {
			*outResult = B_OK;
		} else {
			*outResult = B_BAD_TYPE;
		}
	} else {
		*outResult = B_BAD_TYPE;
	}

	if (neg) val = -val;
	return val;
}

int32_t SValue::AsInt32(status_t* result) const
{
	// First check the fast and easy cases.
	switch (m_type) {
		case B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)):
			if (result) *result = B_OK;
			return m_data.integer;
		case B_PACK_SMALL_TYPE(B_NULL_TYPE, 0):
			if (result) *result = B_OK;
			return 0;
	}

	int32_t val = 0;
	status_t r;
	switch (B_UNPACK_TYPE_CODE(m_type)) {
		case B_STRING_TYPE: {
			const char* ex;
			if ((r=GetString(&ex)) == B_OK) {
				val = static_cast<int32_t>(string_to_number(ex, &r));
			}
		} break;
		case B_DOUBLE_TYPE: {
			double ex;
			if ((r=GetDouble(&ex)) == B_OK) {
				val = static_cast<int32_t>((ex > 0.0 ? (ex+.5) : (ex-.5)));
			}
		} break;
		case B_FLOAT_TYPE: {
			float ex;
			if ((r=GetFloat(&ex)) == B_OK) {
				val = static_cast<int32_t>((ex > 0.0f ? (ex+.5f) : (ex-.5f)));
			}
		} break;
		case B_INT64_TYPE: {
			int64_t ex;
			if ((r=GetInt64(&ex)) == B_OK) {
				val = static_cast<int32_t>(ex);
			}
		} break;
		case B_NSECS_TYPE: {
			nsecs_t ex;
			if ((r=GetTime(&ex)) == B_OK) {
				val = static_cast<int32_t>(ex);
			}
		} break;
		case B_INT16_TYPE: {
			int16_t ex;
			if ((r=GetInt16(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_INT8_TYPE: {
			int8_t ex;
			if ((r=GetInt8(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_BOOL_TYPE: {
			bool ex;
			if ((r=GetBool(&ex)) == B_OK) {
				val = ex ? 1 : 0;
			}
		} break;
		default:
			r = type_conversion_error();
	}
	if (result) *result = r;
	return val;
}

int64_t SValue::AsInt64(status_t* result) const
{
	int64_t val = 0;
	status_t r;
	switch (B_UNPACK_TYPE_CODE(m_type)) {
		case B_STRING_TYPE: {
			const char* ex;
			if ((r=GetString(&ex)) == B_OK) {
				val = string_to_number(ex, &r);
			}
		} break;
		case B_DOUBLE_TYPE: {
			double ex;
			if ((r=GetDouble(&ex)) == B_OK) {
				val = static_cast<int64_t>((ex > 0.0 ? (ex+.5) : (ex-.5)));
			}
		} break;
		case B_FLOAT_TYPE: {
			float ex;
			if ((r=GetFloat(&ex)) == B_OK) {
				val = static_cast<int64_t>((ex > 0.0f ? (ex+.5f) : (ex-.5f)));
			}
		} break;
		case B_INT32_TYPE: {
			int32_t ex;
			if ((r=GetInt32(&ex)) == B_OK) {
				val = static_cast<int64_t>(ex);
			}
		} break;
		case B_NSECS_TYPE: {
			nsecs_t ex;
			if ((r=GetTime(&ex)) == B_OK) {
				val = static_cast<int64_t>(ex);
			}
		} break;
		case B_INT64_TYPE: {
			r = GetInt64(&val);
		} break;
		case B_INT16_TYPE: {
			int16_t ex;
			if ((r=GetInt16(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_INT8_TYPE: {
			int8_t ex;
			if ((r=GetInt8(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_BOOL_TYPE: {
			bool ex;
			if ((r=GetBool(&ex)) == B_OK) {
				val = ex ? 1 : 0;
			}
		} break;
		case B_NULL_TYPE: {
			r = B_OK;
			val = 0;
		} break;
		default:
			r = type_conversion_error();
	}
	if (result) *result = r;
	return val;
}

static bool decode_time(const char** str, nsecs_t* inoutTime)
{
	nsecs_t factor = 0;
	// First check for a suffix.
	const char* num = *str;
	while (*num && isspace(*num)) num++;
	const char* unit = num;
	while (*unit && *unit >= '0' && *unit <= '9') unit++;
	if ((num-unit) == 0) return false;
	while (*unit && isspace(*unit)) unit++;
	const char* end = unit;
	while (*end && !isspace(*end) && (*end < '0' || *end > '9')) end++;
	if ((end-unit) == 0) {
		factor = 1;
	} else if ((end-unit) == 1) {
		if (*unit == 'd') factor = B_ONE_SECOND*60*60*24;
		else if (*unit == 'h') factor = B_ONE_SECOND*60*60;
		else if (*unit == 'm') factor = B_ONE_SECOND*60;
		else if (*unit == 's') factor = B_ONE_SECOND;
	} else if ((end-unit) == 2) {
		if (strncmp(unit, "ms", 2) == 0) factor = B_ONE_MILLISECOND;
		else if (strncmp(unit, "us", 2) == 0) factor = B_ONE_MICROSECOND;
		else if (strncmp(unit, "ns", 2) == 0) factor = 1;
	} else if ((end-unit) == 4) {
		if (strncmp(unit, "day", 3) == 0) factor = B_ONE_SECOND*60*60*24;
		else if (strncmp(unit, "min", 3) == 0) factor = B_ONE_SECOND*60;
		else if (strncmp(unit, "sec", 3) == 0) factor = B_ONE_SECOND;
	} else if ((end-unit) == 4) {
		if (strncmp(unit, "hour", 4) == 0) factor = B_ONE_MILLISECOND*60*60;
		else if (strncmp(unit, "msec", 4) == 0) factor = B_ONE_MILLISECOND;
		else if (strncmp(unit, "usec", 4) == 0) factor = B_ONE_MICROSECOND;
		else if (strncmp(unit, "nsec", 4) == 0) factor = 1;
	}
	if (factor == 0) return false;
	*inoutTime += strtoll(num, const_cast<char**>(&num), 10) * factor;
	*str = end;
	return true;
}

nsecs_t SValue::AsTime(status_t* result) const
{
	nsecs_t val = 0;
	status_t r;
	switch (B_UNPACK_TYPE_CODE(m_type)) {
		case B_STRING_TYPE: {
			const char* ex;
			if ((r=GetString(&ex)) == B_OK) {
				bool gotSomething = false;
				while (decode_time(&ex, &val)) gotSomething = true;
				if (!gotSomething) r = B_BAD_TYPE;
			}
		} break;
		case B_DOUBLE_TYPE: {
			double ex;
			if ((r=GetDouble(&ex)) == B_OK) {
				val = static_cast<nsecs_t>((ex > 0.0 ? (ex+.5) : (ex-.5)));
			}
		} break;
		case B_FLOAT_TYPE: {
			float ex;
			if ((r=GetFloat(&ex)) == B_OK) {
				val = static_cast<nsecs_t>((ex > 0.0f ? (ex+.5f) : (ex-.5f)));
			}
		} break;
		case B_INT64_TYPE: {
			int64_t ex;
			if ((r=GetInt64(&ex)) == B_OK) {
				val = static_cast<nsecs_t>(ex);
			}
		} break;
		case B_NSECS_TYPE: {
			r = GetTime(&val);
		} break;
		case B_INT32_TYPE: {
			int32_t ex;
			if ((r=GetInt32(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_INT16_TYPE: {
			int16_t ex;
			if ((r=GetInt16(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_INT8_TYPE: {
			int8_t ex;
			if ((r=GetInt8(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_BOOL_TYPE: {
			bool ex;
			if ((r=GetBool(&ex)) == B_OK) {
				val = ex ? 1 : 0;
			}
		} break;
		case B_NULL_TYPE: {
			r = B_OK;
			val = 0;
		} break;
		default:
			r = type_conversion_error();
	}
	if (result) *result = r;
	return val;
}

float SValue::AsFloat(status_t* result) const
{
	// First check the fast and easy cases.
	switch (m_type) {
		case B_PACK_SMALL_TYPE(B_FLOAT_TYPE, sizeof(float)):
			if (result) *result = B_OK;
			return *reinterpret_cast<const float*>(m_data.local);
		case B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)):
			if (result) *result = B_OK;
			return static_cast<float>(m_data.integer);
		case B_PACK_SMALL_TYPE(B_NULL_TYPE, 0):
			if (result) *result = B_OK;
			return 0.0f;
	}

	float val = 0;
	status_t r;
	switch (B_UNPACK_TYPE_CODE(m_type)) {
		case B_STRING_TYPE: {
			const char* ex;
			if ((r=GetString(&ex)) == B_OK) {
				val = static_cast<float>(strtod(ex, const_cast<char**>(&ex)));
			}
		} break;
		case B_DOUBLE_TYPE: {
			double ex;
			if ((r=GetDouble(&ex)) == B_OK) {
				val = static_cast<float>(ex);
			}
		} break;
		case B_INT64_TYPE: {
			int64_t ex;
			if ((r=GetInt64(&ex)) == B_OK) {
				val = static_cast<float>(ex);
			}
		} break;
		case B_NSECS_TYPE: {
			nsecs_t ex;
			if ((r=GetTime(&ex)) == B_OK) {
				val = static_cast<float>(ex);
			}
		} break;
		case B_INT16_TYPE: {
			int16_t ex;
			if ((r=GetInt16(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_INT8_TYPE: {
			int8_t ex;
			if ((r=GetInt8(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_BOOL_TYPE: {
			bool ex;
			if ((r=GetBool(&ex)) == B_OK) {
				val = ex ? 1.0f : 0.0f;
			}
		} break;
		default:
			r = type_conversion_error();
	}
	if (result) *result = r;
	return val;
}

double SValue::AsDouble(status_t* result) const
{
	double val = 0;
	status_t r;
	switch (B_UNPACK_TYPE_CODE(m_type)) {
		case B_STRING_TYPE: {
			const char* ex;
			if ((r=GetString(&ex)) == B_OK) {
				val = strtod(ex, const_cast<char**>(&ex));
			}
		} break;
		case B_DOUBLE_TYPE: {
			r = GetDouble(&val);
		} break;
		case B_FLOAT_TYPE: {
			float ex;
			if ((r=GetFloat(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_INT64_TYPE: {
			int64_t ex;
			if ((r=GetInt64(&ex)) == B_OK) {
				val = static_cast<double>(ex);
			}
		} break;
		case B_NSECS_TYPE: {
			nsecs_t ex;
			if ((r=GetTime(&ex)) == B_OK) {
				val = static_cast<double>(ex);
			}
		} break;
		case B_INT32_TYPE: {
			int32_t ex;
			if ((r=GetInt32(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_INT16_TYPE: {
			int16_t ex;
			if ((r=GetInt16(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_INT8_TYPE: {
			int8_t ex;
			if ((r=GetInt8(&ex)) == B_OK) {
				val = ex;
			}
		} break;
		case B_BOOL_TYPE: {
			bool ex;
			if ((r=GetBool(&ex)) == B_OK) {
				val = ex ? 1 : 0;
			}
		} break;
		case B_NULL_TYPE: {
			r = B_OK;
			val = 0.0;
		} break;
		default:
			r = type_conversion_error();
	}
	if (result) *result = r;
	return val;
}

sptr<SAtom> SValue::AsAtom(status_t* result) const
{
	sptr<SAtom> val;
	status_t r = GetAtom(&val);
	if (r < B_OK) {
		wptr<SAtom> ex;
		if ((r=GetWeakAtom(&ex)) == B_OK) {
			val = ex.promote();
		} else if (m_type == B_PACK_SMALL_TYPE(B_NULL_TYPE, 0)) {
			r = B_OK;
		} else {
			r = type_conversion_error();
		}
	}
	if (result) *result = r;
	return val;
}

wptr<SAtom> SValue::AsWeakAtom(status_t* result) const
{
	wptr<SAtom> val;
	status_t r = GetWeakAtom(&val);
	if (r != B_OK && m_type == B_PACK_SMALL_TYPE(B_NULL_TYPE, 0)) {
		r = B_OK;
	}
	if (result) *result = r;
	return val;
}

type_code SValue::Type() const
{
	return B_UNPACK_TYPE_CODE(m_type);
}

size_t SValue::Length() const
{
	const uint32_t len = B_UNPACK_TYPE_LENGTH(m_type);
	if (len <= B_TYPE_LENGTH_MAX) return len;
	if (len == B_TYPE_LENGTH_LARGE) return m_data.buffer->Length();
	return 0;
}

const void* SValue::Data() const
{
	if (B_UNPACK_TYPE_LENGTH(m_type) <= B_TYPE_LENGTH_MAX) return m_data.local;
	else if (B_UNPACK_TYPE_LENGTH(m_type) == B_TYPE_LENGTH_LARGE) return m_data.buffer->Data();
	return NULL;
}

const void* SValue::Data(type_code type, size_t length) const
{
	if (length <= B_TYPE_LENGTH_MAX) {
		if (m_type == B_PACK_SMALL_TYPE(type, length)) return m_data.local;
		return NULL;
	}
	if (B_PACK_LARGE_TYPE(type) == m_type
			&& m_data.buffer->Length() == length) {
		return m_data.buffer->Data();
	}
	return NULL;
}

const void* SValue::Data(type_code type, size_t* inoutMinLength) const
{
	if (*inoutMinLength > B_TYPE_LENGTH_MAX) {
		if (m_type == B_PACK_LARGE_TYPE(type) && m_data.buffer->Length() >= *inoutMinLength) {
			*inoutMinLength = m_data.buffer->Length();
			return m_data.buffer->Data();
		}
		return NULL;
	}
	if (B_UNPACK_TYPE_CODE(m_type) == type) {
		size_t len = B_UNPACK_TYPE_LENGTH(m_type);
		if (len <= B_TYPE_LENGTH_MAX && len >= *inoutMinLength) {
			*inoutMinLength = len;
			return m_data.local;
		}
		if (len == B_TYPE_LENGTH_LARGE) {
			*inoutMinLength = m_data.buffer->Length();
			return m_data.buffer->Data();
		}
	}
	return NULL;
}

const SSharedBuffer* SValue::SharedBuffer() const
{
	if (B_UNPACK_TYPE_LENGTH(m_type) == B_TYPE_LENGTH_LARGE) {
		m_data.buffer->IncUsers();
		return m_data.buffer;
	} else if (B_UNPACK_TYPE_LENGTH(m_type) <= B_TYPE_LENGTH_MAX) {
		SSharedBuffer* buf = SSharedBuffer::Alloc(B_UNPACK_TYPE_LENGTH(m_type));
		if (buf) *(int32_t*)buf->Data() = m_data.integer;
		return buf;
	}
	return NULL;
}

void SValue::Pool()
{
	if (B_UNPACK_TYPE_LENGTH(m_type) == B_TYPE_LENGTH_LARGE) {
		m_data.buffer = m_data.buffer->Pool();
	} else if (is_map()) {
		//const BValueMap* oldMap = m_data.map;
		BValueMap** map = edit_map();
		if (map) {
			(*map)->Pool();
			//if (oldMap != *map) bout << "BValue pool had to copy: " << *this << endl;
		}
	}
}

status_t SValue::SetType(type_code newType)
{
	if (is_simple() && !CHECK_IS_SMALL_OBJECT(m_type) && !CHECK_IS_SMALL_OBJECT(newType)) {
		size_t length = Length();
		m_type = length <= B_TYPE_LENGTH_MAX
			? B_PACK_SMALL_TYPE(newType, length) : B_PACK_LARGE_TYPE(newType);
		return B_OK;
	}

	return B_BAD_TYPE;
}

void* SValue::BeginEditBytes(type_code type, size_t length, uint32_t flags)
{
	void* data;
	if (is_simple() && !CHECK_IS_SMALL_OBJECT(m_type) && !CHECK_IS_SMALL_OBJECT(type)) {
		if (!(flags&B_EDIT_VALUE_DATA)) {
			if (!is_error()) {
				FreeData();
				data = alloc_data(type, length);
			} else data = NULL;
		} else {
			data = edit_data(length);
			if (data) {
				m_type = length <= B_TYPE_LENGTH_MAX
					? B_PACK_SMALL_TYPE(type, length) : B_PACK_LARGE_TYPE(type);
			}
		}
	} else {
		data = NULL;
	}
	return data;
}

status_t SValue::EndEditBytes(ssize_t final_length)
{
	if (!is_error()) {
		if (final_length < 0) return B_OK;
		const size_t len = Length();
		if (final_length == len) return B_OK;
		if (final_length < (ssize_t)len) {
			// Need to convert to inline storage.
			if (edit_data(final_length)) return B_OK;
			return ErrorCheck();
		}
		return B_BAD_VALUE;
	}
	return ErrorCheck();
}

#if SUPPORTS_TEXT_STREAM
static bool LooksLikeFourCharCode(uint32_t t)
{
	for (int i = 3; i >= 0; i--)
	{
		uint8_t c = (uint8_t)((t >> (8 * i)) & 0x00FF);

		if ((c < ' ') || (c >= 0x7F))
			return false;
	}
	
	return true;
}
#endif

status_t SValue::PrintToStream(const sptr<ITextOutput>& io, uint32_t flags) const
{
#if SUPPORTS_TEXT_STREAM
	if (flags&B_PRINT_STREAM_HEADER) io << "SValue(";
	
	if (IsSimple()) {

		sptr<IBinder> binder;
		bool handled = true;
		char buf[128];
		
		switch (m_type) {
			case kUndefinedTypeCode:
				io << "undefined";
				break;
			case kWildTypeCode:
				io << "wild";
				break;
			case B_PACK_SMALL_TYPE(B_NULL_TYPE, 0):
				io << "null";
				break;
			case B_PACK_SMALL_TYPE(B_CHAR_TYPE, sizeof(char)):
				io << "char(" << (int32_t)(char)m_data.local[0]
					<< " or " << UIntToHex(m_data.local[0], buf);
				if (isprint(m_data.local[0])) io << " or '" << (char)m_data.local[0] << "'";
				io << ")";
				break;
			case B_PACK_SMALL_TYPE(B_INT8_TYPE, sizeof(int8_t)):
				io << "int8_t(" << (int32_t)(int8_t)m_data.local[0]
					<< " or " << UIntToHex(m_data.local[0], buf);
				if (isprint(m_data.local[0])) io << " or '" << (char)m_data.local[0] << "'";
				io << ")";
				break;
			case B_PACK_SMALL_TYPE(B_INT16_TYPE, sizeof(int16_t)):
				io << "int16_t(" << *reinterpret_cast<const int16_t*>(m_data.local)
					<< " or " << UIntToHex(*(uint16_t*)m_data.local, buf) << ")";
				break;
			case B_PACK_SMALL_TYPE(B_INT32_TYPE, sizeof(int32_t)):
				io << "int32_t(" << *reinterpret_cast<const int32_t*>(m_data.local)
					<< " or " << UIntToHex(*(uint32_t*)m_data.local, buf);
				if (LooksLikeFourCharCode(*reinterpret_cast<const uint32_t*>(m_data.local)))
					io << " or " << STypeCode(*(uint32_t*)m_data.local);
				io << ")";
				break;
			case B_PACK_LARGE_TYPE(B_INT64_TYPE):
				if (m_data.buffer->Length() == sizeof(int64_t)) {
					io << "int64_t(" << *reinterpret_cast<const int64_t*>(m_data.buffer->Data())
						<< " or " << UIntToHex(*(uint64_t*)m_data.buffer->Data(), buf) << ")";
				} else {
					handled = false;
				}
				break;
			case B_PACK_SMALL_TYPE(B_UINT8_TYPE, sizeof(uint8_t)):
				io << "uint8_t(" << (uint32_t)(uint8_t)m_data.local[0]
					<< " or " << UIntToHex(m_data.local[0], buf);
				if (isprint(m_data.local[0])) io << " or '" << (char)m_data.local[0] << "'";
				io << ")";
				break;
			case B_PACK_SMALL_TYPE(B_UINT16_TYPE, sizeof(uint16_t)):
				io << "uint16_t(" << (uint32_t)*reinterpret_cast<const uint16_t*>(m_data.local)
					<< " or " << UIntToHex(*(uint16_t*)m_data.local, buf) << ")";
				break;
			case B_PACK_SMALL_TYPE(B_UINT32_TYPE, sizeof(uint32_t)):
				io << "uint32_t(" << *reinterpret_cast<const uint32_t*>(m_data.local)
					<< " or " << UIntToHex(*(uint32_t*)m_data.local, buf) << ")";
				break;
			case B_PACK_LARGE_TYPE(B_UINT64_TYPE):
				if (m_data.buffer->Length() == sizeof(uint64_t)) {
					io << "uint64_t(" << *reinterpret_cast<const uint64_t*>(m_data.buffer->Data())
						<< " or " << UIntToHex(*(uint64_t*)m_data.buffer->Data(), buf) << ")";
				} else {
					handled = false;
				}
				break;
			case B_PACK_SMALL_TYPE(B_ERROR_TYPE, sizeof(status_t)): {
				// Internal error
				io << "ERR: " << SStatus(*reinterpret_cast<const status_t*>(m_data.local));
				#if 0
				STextDecoder dec;
				dec.DeviceToUTF8(str, strlen(str));
				io << dec.Buffer();
				#endif
			} break;
			case B_PACK_SMALL_TYPE(B_STATUS_TYPE, sizeof(status_t)): {
				io << SStatus(*reinterpret_cast<const status_t*>(m_data.local));
				#if 0
				STextDecoder dec;
				dec.DeviceToUTF8(str, strlen(str));
				io << dec.Buffer();
				#endif
			} break;
			case B_PACK_SMALL_TYPE(B_FLOAT_TYPE, sizeof(float)):
				io << "float(" << *reinterpret_cast<const float*>(m_data.local) << ")";
				break;
			case B_PACK_LARGE_TYPE(B_DOUBLE_TYPE):
				if (m_data.buffer->Length() == sizeof(double)) {
					io << "double(" << *reinterpret_cast<const double*>(m_data.buffer->Data()) << ")";
				} else {
					handled = false;
				}
				break;
			case B_PACK_SMALL_TYPE(B_BOOL_TYPE, sizeof(int8_t)):
				if (flags&B_PRINT_VALUE_TYPES) io << "bool(";
				io << ((*reinterpret_cast<const int8_t*>(m_data.local)) ? "true" : "false");
				if (flags&B_PRINT_VALUE_TYPES) io << ")";
				break;
			case B_PACK_LARGE_TYPE(B_NSECS_TYPE):
				if (m_data.buffer->Length() == sizeof(nsecs_t)) {
					const nsecs_t val = *reinterpret_cast<const nsecs_t*>(m_data.buffer->Data());
					io << "nsecs_t(" << SDuration(val) << " or " << UIntToHex(val, buf) << ")";
				} else {
					handled = false;
				}
				break;
			case B_PACK_LARGE_TYPE(B_OFF_T_TYPE):
				if (m_data.buffer->Length() == sizeof(off_t)) {
					io << "off_t(" << *reinterpret_cast<const off_t*>(m_data.buffer->Data())
						<< " or " << UIntToHex(*(off_t*)m_data.buffer->Data(), buf) << ")";
				} else {
					handled = false;
				}
				break;
			case B_PACK_SMALL_TYPE(B_SIZE_T_TYPE, sizeof(size_t)):
				io << "size_t(" << *reinterpret_cast<const size_t*>(m_data.local)
					<< " or " << UIntToHex(*(size_t*)m_data.local, buf) << ")";
				break;
			case B_PACK_SMALL_TYPE(B_SSIZE_T_TYPE, sizeof(ssize_t)):
				io << "ssize_t(" << *reinterpret_cast<const ssize_t*>(m_data.local)
					<< " or " << UIntToHex(*(size_t*)m_data.local, buf) << ")";
				break;
			case kPackedSmallAtomType: {
				const SAtom* a = *reinterpret_cast<SAtom* const *>(m_data.local);
				io << "sptr<SAtom>(" << a;
#if _SUPPORTS_RTTI
				if (a) io << " " << typeid(*a).name();
#endif
				io << ")";
			} break;
			case kPackedSmallAtomWeakType: {
				const SAtom::weak_atom_ptr* wp = *reinterpret_cast<SAtom::weak_atom_ptr* const *>(m_data.local);
				io << "wptr<SAtom>(" << (wp ? wp->atom : NULL);
#if _SUPPORTS_RTTI
				if (wp && wp->atom) io << " " << typeid(*wp->atom).name();
#endif
				io << ")";
			} break;
			case kPackedSmallBinderType: {
				const IBinder* b = *reinterpret_cast<IBinder* const *>(m_data.local);
				io << "sptr<IBinder>(" << b;
#if _SUPPORTS_RTTI
				if (b) io << " " << typeid(*b).name();
#endif
				io << ")";
				if (flags&(B_PRINT_BINDER_INTERFACES|B_PRINT_BINDER_CONTENTS)) {
					binder = const_cast<IBinder*>(b);
				}
			} break;
			case kPackedSmallBinderWeakType: {
				const SAtom::weak_atom_ptr* wp = *reinterpret_cast<SAtom::weak_atom_ptr* const *>(m_data.local);
				const IBinder* real = (wp && wp->atom->AttemptIncStrong(this) ? ((const IBinder*)wp->cookie) : NULL);
				io << "wptr<IBinder>(" << (wp ? wp->cookie : NULL);
#if _SUPPORTS_RTTI
				if (real) io << " " << typeid(*real).name();
#endif
				io << ")";
				if (flags&(B_PRINT_BINDER_INTERFACES|B_PRINT_BINDER_CONTENTS)) {
					binder = const_cast<IBinder*>(real);
				}
				if (real) real->DecStrong(this);
			} break;
			case kPackedSmallBinderHandleType:
				io << "IBinder::shnd(" << UIntToHex(*reinterpret_cast<const int32_t*>(m_data.local), buf) << ")";
				if (flags&(B_PRINT_BINDER_INTERFACES|B_PRINT_BINDER_CONTENTS)) {
					binder = SValue(*this).AsBinder();
				}
				break;
			case kPackedSmallBinderWeakHandleType:
				io << "IBinder::whnd(" << UIntToHex(*reinterpret_cast<const int32_t*>(m_data.local), buf) << ")";
				if (flags&(B_PRINT_BINDER_INTERFACES|B_PRINT_BINDER_CONTENTS)) {
					binder = SValue(*this).AsBinder();
				}
				break;
			default:
				handled = false;
				break;
		}
		
		if (!handled) {
			const type_code type = Type();
			const size_t length = Length();
			const void* data = Data();
			if (type == B_STRING_TYPE || type == B_SYSTEM_TYPE) {
				bool valid = true;
				if (length > 0) {
					if (static_cast<const char*>(data)[length-1] != 0) valid = false;
					for (size_t i=0; valid && i<length-1; i++) {
						if (static_cast<const char*>(data)[i] == 0) valid = false;
					}
				} else {
					valid = false;
				}
				if (valid) {
					if (type == B_STRING_TYPE) {
						if (flags&B_PRINT_VALUE_TYPES) io << "string(\"";
						else io << "\"";
						io << static_cast<const char*>(data);
						if (flags&B_PRINT_VALUE_TYPES) io << "\", " << length << " bytes)";
						else io << "\"";
					} else {
						io << "system(\"" << static_cast<const char*>(data) << "\")";
					}
					handled = true;
				} else if (type == B_STRING_TYPE) {
					io	<< indent << "string " << length << " bytes: "
			  			<< (SHexDump(data, length).SetSingleLineCutoff(16)) << dedent;
					handled = true;
				}
			}
			else if (type == B_UUID_TYPE && Length() == sizeof(uuid_t))
			{
				uuid_t* u = (uuid_t*)data;
				io << "UUID(\"";
				io << SPrintf("%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
							u->time_low,
							u->time_mid,
							u->time_hi_and_version,
							u->clock_seq_hi_and_reserved,
							u->clock_seq_low,
							u->node[0], u->node[1], u->node[2], u->node[3], u->node[4], u->node[5]);
				io << "\")";
				handled = true;
			}

			if (!handled) {
				printer_registry* reg = Printers();
				if (reg && reg->lock.Lock() == B_OK) {
					print_func f = reg->funcs.ValueFor(type);
					reg->lock.Unlock();
					if (f) handled = ( (*f)(io, *this, flags|B_PRINT_STREAM_HEADER) == B_OK );
				}
			}

			if (!handled) {
				io	<< indent << STypeCode(type) << " " << length << " bytes:"
					<< (SHexDump(data, length).SetSingleLineCutoff(16)) << dedent;
			}
		}

		if (binder != NULL) {
			bool didInterfaces = false;
			if ((flags&(B_PRINT_BINDER_INTERFACES|0x10000000)) == B_PRINT_BINDER_INTERFACES) {
				SValue val(binder->Inspect(binder, SValue::Wild()));
				if (val.IsDefined()) {
					io << " ";
					val.PrintToStream(io, flags&(~B_PRINT_STREAM_HEADER)|0x10000000);
					didInterfaces = true;
				}
			}
			if (!didInterfaces) {
				if ((flags&B_PRINT_CATALOG_CONTENTS) != 0) {
					// We need a real catalog interface!!!!!!!
					if (!binder->Inspect(binder, SValue::Wild()).IsDefined()) {
						SValue val(binder->Get(SValue::Wild()));
						io << " ";
						val.PrintToStream(io, flags&(~(B_PRINT_STREAM_HEADER|0x10000000)));
					}
				} else if ((flags&B_PRINT_BINDER_CONTENTS) != 0) {
					SValue val(binder->Get(SValue::Wild()));
					io << " ";
					val.PrintToStream(io, flags&(~(B_PRINT_STREAM_HEADER|0x10000000)));
				}
			}
		}
				
	} else {

		SValue key, value;
		const size_t N = CountItems();
		if (N > 1) {
			io << "{" << endl << indent;
		}
		for (size_t i=0; i<N; i++) {
			find_item_index(i, &key, &value);
			if (!key.IsWild()) {
				const bool need_bracket = !key.IsSimple() && key.CountItems() <= 1;
				if (need_bracket) io << "{ ";
				key.PrintToStream(io, flags&(~B_PRINT_STREAM_HEADER));
				if (need_bracket) io << " }";
				io << " -> ";
			}
			value.PrintToStream(io, flags&(~B_PRINT_STREAM_HEADER));
			if (N > 1) {
				if (i < (N-1)) io << "," << endl;
				else io << endl;
			}
		}
		if (N > 1) {
			io << dedent << "}";
		}
	}
	
	if (flags&B_PRINT_STREAM_HEADER) io << ")";
	return B_OK;
#else
	(void)io;
	(void)flags;
	return B_UNSUPPORTED;
#endif
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SValue& value)
{
#if SUPPORTS_TEXT_STREAM
	value.PrintToStream(io, B_PRINT_STREAM_HEADER);
#else
	(void)value;
#endif
	return io;
}

void* SValue::edit_data(size_t len)
{
	if (!is_error()) {

		// Are we currently storing data inline?
		if (B_UNPACK_TYPE_LENGTH(m_type) <= B_TYPE_LENGTH_MAX) {
			if (len <= B_TYPE_LENGTH_MAX) {
				m_type = (m_type&~B_TYPE_LENGTH_MASK) | len;
				return m_data.local;
			}

			// Need to create a new buffer for the data.
			SSharedBuffer* buf = SSharedBuffer::Alloc(len);
			if (buf) {
				*(int32_t*)buf->Data() = m_data.integer;
				m_type = (m_type&~B_TYPE_LENGTH_MASK) | B_TYPE_LENGTH_LARGE;
				m_data.buffer = buf;
				return buf->Data();
			}
			
			set_error(B_NO_MEMORY);
			return NULL;

		// Are we currently storing data in a shared buffer?
		} else if (B_UNPACK_TYPE_LENGTH(m_type) == B_TYPE_LENGTH_LARGE) {

			// Shrinking to inline storage?
			if (len <= B_TYPE_LENGTH_MAX) {
				const SSharedBuffer* buf = m_data.buffer;
				m_data.integer = *(const int32_t*)buf->Data();
				m_type = (m_type&~B_TYPE_LENGTH_MASK) | len;
				buf->DecUsers();
				return m_data.local;
			}

			// Need to resize an existing buffer.
			SSharedBuffer* buf = m_data.buffer->Edit(len);
			if (buf) {
				m_data.buffer = buf;
				return buf->Data();
			}
			
			set_error(B_NO_MEMORY);
			return NULL;

		}

	}

	return NULL;
}

status_t SValue::copy_small_data(type_code type, void* buf, size_t len) const
{
	DbgOnlyFatalErrorIf(type == B_UNDEFINED_TYPE, "B_UNDEFINED_TYPE not valid here.");

	if (m_type == type) {
		memcpy(buf, m_data.local, len);
		return B_OK;
	}

	if (is_error()) return ErrorCheck();
	if (B_UNPACK_TYPE_CODE(m_type) != B_UNPACK_TYPE_CODE(type)) return B_BAD_TYPE;
	return B_MISMATCHED_VALUES;
}

status_t SValue::copy_big_data(type_code type, void* buf, size_t len) const
{
	DbgOnlyFatalErrorIf(type == B_UNDEFINED_TYPE, "B_UNDEFINED_TYPE not valid here.");

	if (m_type == type && m_data.buffer->Length() == len) {
		memcpy(buf, m_data.buffer->Data(), len);
		return B_OK;
	}

	if (is_error()) return ErrorCheck();
	if (B_UNPACK_TYPE_CODE(m_type) != B_UNPACK_TYPE_CODE(type)) return B_BAD_TYPE;
	return B_MISMATCHED_VALUES;
}

BValueMap** SValue::edit_map()
{
	BValueMap** map = (BValueMap**)(&m_data.map);
	BValueMap* other;
	if (is_map()) {
		if ((*map)->IsShared()) {
			other = (*map)->Clone();
			if (other) {
				(*map)->DecUsers();
				*map = other;
			} else {
				map = NULL;
				set_error(B_NO_MEMORY);
			}
		}
	} else if (!is_error()) {
		other = BValueMap::Create(1);
		if (other) {
			if (is_specified()) other->SetFirstMap(SValue::Wild(), *this);
			FreeData();
			m_type = kMapTypeCode;
			*map = other;
		} else {
			set_error(B_NO_MEMORY);
			map = NULL;
		}
	} else {
		map = NULL;
	}
	return map;
}

BValueMap** SValue::make_map_without_sets()
{
	BValueMap **map = NULL;
	
	if (!is_map()) Undefine();
	else {
		size_t N = m_data.map->CountMaps();
		size_t i = 0;
		while (i < N) {
			if (m_data.map->MapAt(i).key.IsWild()) {
				if (!map) map = edit_map();
				if (!map) break;
				BValueMap::RemoveMapAt(map, i);
				N--;
			} else {
				i++;
			}
		}
	}
	
	return map;
}

status_t SValue::set_error(ssize_t code)
{
	FreeData();
	m_type = kErrorTypeCode;
	m_data.integer = code < B_OK ? code : B_ERROR;
	CHECK_INTEGRITY(*this);
	return m_data.integer;
}

#if DB_INTEGRITY_CHECKS
static int32_t gMallocDebugLevel = -1;

static void ReadMallocDebugLevel();
static inline int32_t MallocDebugLevel()
{
	if (gMallocDebugLevel < 0) ReadMallocDebugLevel();
	return gMallocDebugLevel;
}

static void ReadMallocDebugLevel()
{
	char envBuffer[128];
	const char* env = getenv("MALLOC_DEBUG");
	if (env) {
		gMallocDebugLevel = atoi(env);
		if (gMallocDebugLevel < 0)
			gMallocDebugLevel = 0;
	} else {
		gMallocDebugLevel = 0;
	}
}
#endif

bool SValue::check_integrity() const
{
	bool passed = true;
	
#if DB_INTEGRITY_CHECKS
#if !VALIDATES_VALUE
	if (MallocDebugLevel() <= 0) return passed;
#endif

	if ((m_type&(B_TYPE_BYTEORDER_MASK)) != B_TYPE_BYTEORDER_NORMAL && m_type != kUndefinedTypeCode) {
		ErrFatalError("SValue: byte order bits are incorrect");
		passed = false;
	}

	if ((m_type&~(B_TYPE_BYTEORDER_MASK|B_TYPE_LENGTH_MASK|B_TYPE_CODE_MASK)) != 0) {
		ErrFatalError("SValue: unnused type bits are non-zero");
		passed = false;
	}

	switch (B_UNPACK_TYPE_CODE(m_type)) {
		case B_UNDEFINED_TYPE:
		case B_WILD_TYPE:
		case B_NULL_TYPE:
			if (B_UNPACK_TYPE_LENGTH(m_type) != 0) {
				ErrFatalError("SValue: undefined, wild, or null value with non-zero length");
				passed = false;
			}
			break;

		case B_VALUE_TYPE:
			if (B_UNPACK_TYPE_LENGTH(m_type) != B_TYPE_LENGTH_MAP) {
				ErrFatalError("SValue: value map type not using B_TYPE_LENGTH_MAP");
				passed = false;
			}
			break;

		case B_ERROR_TYPE:
			if (B_UNPACK_TYPE_LENGTH(m_type) != sizeof(status_t)) {
				ErrFatalError("SValue: error value with wrong length");
				passed = false;
			}
			break;
	}

	switch (B_UNPACK_TYPE_LENGTH(m_type)) {
		case B_TYPE_LENGTH_LARGE:
		{
			if (m_data.buffer == NULL) {
				ErrFatalError("SValue: length is B_TYPE_LENGTH_LARGE, but m_data.buffer is NULL");
				passed = false;
			}
			else if (m_data.buffer->Length() <= B_TYPE_LENGTH_MAX) {
				ErrFatalError("SValue: length is B_TYPE_LENGTH_LARGE, but buffer is <= B_TYPE_LENGTH_MAX");
				passed = false;
			} else if (m_data.buffer->Users() < 0) {
				ErrFatalError("SValue: shared buffer has negative users; maybe it was deallocated?");
				passed = false;
			}
		} break;

		case B_TYPE_LENGTH_MAP:
		{
			if (B_UNPACK_TYPE_CODE(m_type) != B_VALUE_TYPE) {
				ErrFatalError("SValue: B_TYPE_LENGTH_MAP used with wrong type code");
				passed = false;
			} else if (m_data.map == NULL) {
				ErrFatalError("SValue: contains a map, but m_data.map is NULL");
				passed = false;
			} else {
				if (m_data.map->CountMaps() <= 0) {
					ErrFatalError("SValue: contains a BValueMap of no items");
					passed = false;
				} else if (m_data.map->CountMaps() == 1) {
					const BValueMap::pair& p = m_data.map->MapAt(0);
					if (p.key == SValue::Wild()) {
						ErrFatalError("SValue: contains a redundant BValueMap");
						passed = false;
					}
				} else if (m_data.map->CountMaps() > 0x7fffffff) {
					ErrFatalError("SValue: contains a huge BValueMap; maybe it was deallocated?");
					passed = false;
				}
			}
		} break;
	}
#endif

	return passed;
}

const SValue& SValue::ReplaceValues(const SValue* newValues, const size_t* indices, size_t count)
{
	if (!is_error() && is_map())
	{
		BValueMap **map=edit_map();
		if (map != NULL) {
			size_t i, mapCount = (*map)->CountMaps();
			for (i=0; i<count; i++) {
				const size_t idx(indices[i]);
				if (idx < mapCount) {
					// this can't use BeginEditMap / EndEditMap since that normalizes the undefined
					// values, and doing that will screw up our indices.
					BValueMap::pair& p = (BValueMap::pair&)(*map)->MapAt(idx);
					p.value = newValues[i];
					DbgOnlyFatalErrorIf(p.value.IsDefined() == false, "ReplaceValues cannot be used with undefined values");
				}
			}
		}
	}
	
	CHECK_INTEGRITY(*this);
	return *this;
}

void SValue::GetKeyIndices(const SValue* keys, size_t* outIndices, size_t count)
{
	if (!is_error() && is_map())
	{
		for (size_t i = 0 ; i < count ; i++)
		{
			outIndices[i] = m_data.map->IndexFor(keys[i]);
		}
	}
	else
	{
		for (size_t i = 0 ; i < count ; i++)
		{
			outIndices[i] = ~0; 
		}
	}
}

// -----------------------------------------------------------------
// Specializations for marshalling SVector<SValue>
// -----------------------------------------------------------------

SValue BArrayAsValue(const SValue* from, size_t count)
{
	SValue result;
	for (size_t i = 0; i < count; i++) {
		result.JoinItem(SSimpleValue<int32_t>(i), *from);	
		from++;
	}
	return result;
}

status_t BArrayConstruct(SValue* to, const SValue& value, size_t count)
{
	for (size_t i = 0; i < count; i++) {
	*to = value[SSimpleValue<int32_t>(i)];
		to++;
	}
	return B_OK;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
