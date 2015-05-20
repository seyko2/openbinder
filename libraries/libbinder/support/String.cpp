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

//  String class.
//  Has a very low footprint of just 1 pointer on the stack, same
// 	as char *.
//  Supports assigning, appending, truncating, string formating using <<
//  searching, replacing, removing
//
//	No ref counting yet
//	Not thread safe
//	some UTF8 support

#include <support/String.h>

#include <support/Debug.h>
#include <support/SharedBuffer.h>
#include <support/Value.h>
#include <support/StdIO.h>

#include <support_p/WindowsCompatibility.h>

#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

#ifndef _SUPPORTS_UNIX_FILE_PATH
#define _SUPPORTS_UNIX_FILE_PATH (TARGET_HOST!=TARGET_HOST_WIN32)
#endif
#ifndef _SUPPORTS_WINDOWS_FILE_PATH
#define _SUPPORTS_WINDOWS_FILE_PATH (TARGET_HOST==TARGET_HOST_WIN32)
#endif

#ifndef SUPPORTS_SELFQC_DEBUG

// Lock debugging is not specified.  Turn it on if this is a debug build.
#if BUILD_TYPE == BUILD_TYPE_DEBUG
#define SUPPORTS_SELFQC_DEBUG 1
#else
#define SUPPORTS_SELFQC_DEBUG 0
#endif

#endif



#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

status_t SString::SelfQC()
{
#if SUPPORTS_SELFQC_DEBUG
	SString str(B_UTF8_BULLET " foo:bar!");
	SString newStr(str);
	newStr.ReplaceSet(":!", '.');
	if (newStr != B_UTF8_BULLET " foo.bar.") {
		bout << "Replace ':!' string: " << newStr << endl;
		goto fail;
	}
	newStr = str;
	newStr.RemoveSet("oa");
	if (newStr != B_UTF8_BULLET " f:br!") {
		bout << "Remove 'oa' string: " << newStr << endl;
		goto fail;
	}
	newStr = str;
	newStr.RemoveSet(B_UTF8_BULLET ":!");
	if (newStr != " foobar") {
		bout << "Remove bullet+':!' string: " << newStr << endl;
		goto fail;
	}
	goto success;

fail:
	ErrFatalError("Test failed.");
	return B_ERROR;
#endif // SUPPORTS_SELFQC_DEBUG

success:
	return B_OK;
}

static SSharedBuffer* g_emptyStringBuffer = NULL;
static const char* g_emptyString = NULL;

void __initialize_string()
{
	SSharedBuffer* sb = SSharedBuffer::Alloc(1);
	if (sb) {
		g_emptyStringBuffer = sb;
		memset(sb->Data(), 0, 1);
		g_emptyString = (const char*)sb->Data();
	}
}

void __terminate_string()
{
	SSharedBuffer* sb = g_emptyStringBuffer;
	if (sb) {
		g_emptyStringBuffer = NULL;
		g_emptyString = NULL;
		sb->DecUsers();
	}
}

#if LIBBE_BOOTSTRAP
// For LIBBE_BOOTSTRAP a static SString can be used before __initialize_string/__terminate_string
// have been called, so we need to initialize the strings at first usage.
#endif

// inlines for dealing with UTF8
inline char
UTF8SafeToLower(char ch)
{
	return (((uint8_t)ch) < 0x80) ? tolower(ch) : ch;
}

inline char
UTF8SafeToUpper(char ch)
{
	return (((uint8_t)ch) < 0x80) ? toupper(ch) : ch;
}

inline bool
UTF8SafeIsAlpha(char ch)
{
	return (((uint8_t)ch) < 0x80) ? isalpha(ch) != 0 : false;
}

inline int32_t
UTF8CharLen(uint8_t ch)
{
	return ((0xe5000000 >> ((ch >> 3) & 0x1e)) & 3) + 1;
}

inline uint32_t
UTF8CharToUint32(const uint8_t *src, uint32_t length)
{
	uint32_t result = 0;

	for (uint32_t index = 0; index < length; index++)
		result |= src[index] << (24 - index * 8);

	return result;
}

// Write out the source character to <dstP>, and return the
// number of bytes it occupies in the string.

size_t
WChar32ToUTF8(uint8_t* dstP, wchar32_t srcChar)
{
	const wchar32_t kByteMask = 0x000000BF;
	const wchar32_t kByteMark = 0x00000080;

	// Surrogates aren't valid for UTF-32 characters, so define some
	// constants that will let us screen them out.
	const wchar32_t	kUnicodeSurrogateHighStart	= 0x0000D800;
	const wchar32_t	kUnicodeSurrogateHighEnd	= 0x0000DBFF;
	const wchar32_t	kUnicodeSurrogateLowStart	= 0x0000DC00;
	const wchar32_t	kUnicodeSurrogateLowEnd		= 0x0000DFFF;
	const wchar32_t	kUnicodeSurrogateStart		= kUnicodeSurrogateHighStart;
	const wchar32_t	kUnicodeSurrogateEnd		= kUnicodeSurrogateLowEnd;

	// Mask used to set appropriate bits in first byte of UTF-8 sequence,
	// indexed by number of bytes in the sequence.
	const wchar32_t kFirstByteMark[] = { 0x00000000, 0x00000000, 0x000000C0, 0x000000E0, 0x000000F0 };

	size_t bytesToWrite;

	// Figure out how many bytes the result will require.
	if (srcChar < 0x00000080)
	{
		bytesToWrite = 1;
	}
	else if (srcChar < 0x00000800)
	{
		bytesToWrite = 2;
	}
	else if (srcChar < 0x00010000)
	{
		if ((srcChar < kUnicodeSurrogateStart)
		 || (srcChar > kUnicodeSurrogateEnd))
		{
			bytesToWrite = 3;
		}
		else
		{
			// Surrogates are invalid UTF-32 characters.
			return 0;
		}
	}
	// Max code point for Unicode is 0x0010FFFF.
	else if (srcChar < 0x00110000)
	{
		bytesToWrite = 4;
	}
	else
	{
		// Invalid UTF-32 character.
		return 0;
	}

	dstP += bytesToWrite;
	switch (bytesToWrite)
	{	/* note: everything falls through. */
		case 4:	*--dstP = (uint8_t)((srcChar | kByteMark) & kByteMask); srcChar >>= 6;
		case 3:	*--dstP = (uint8_t)((srcChar | kByteMark) & kByteMask); srcChar >>= 6;
		case 2:	*--dstP = (uint8_t)((srcChar | kByteMark) & kByteMask); srcChar >>= 6;
		case 1:	*--dstP = (uint8_t)(srcChar | kFirstByteMark[bytesToWrite]);
	}

	return bytesToWrite;
}

const int32_t kAllocaBufferThreshold = 64;

template <class ElementType>
class _AllocaBuffer {
public:
	// if small, use stack, if large, new

	_AllocaBuffer(int32_t size)
		:	secondary(size > kAllocaBufferThreshold ? new ElementType [size] : 0)
		{ }
	~_AllocaBuffer()
		{
			delete [] secondary;
		}

	ElementType & EditItemAt(int32_t index)
		{
			if (secondary)
				return secondary[index];

			return primary[index];
		}

	operator ElementType *()
		{
			if (secondary)
				return secondary;

			return primary;
		}

	ElementType operator[](size_t index) const
		{
			if (secondary)
				return secondary[index];

			return primary[index];
		}

	ElementType & operator[](int32_t index)
		{
			if (secondary)
				return secondary[index];

			return primary[index];
		}

private:
	ElementType primary[kAllocaBufferThreshold];
	ElementType *secondary;
};

// Common representation of a set of Unicode characters.
class CharSet
{
public:
	enum {
		MAX_CHAR=128,
		NUM_INTS=(128/32)
	};

	CharSet(const char* setOfChars, bool handleUTF8 = true);

	inline bool HasUTF8() const
	{
		return m_hasUTF8;
	}

	// 'c' MUST be <= 127.
	inline uint32_t Has(const unsigned char c) const
	{
		return m_lowerMap[c/32] & 1<<(c&31);
	}

	// Should this be inline?
	uint32_t Has(const char* start) const
	{
		const unsigned char c(*start);
		return (c&0x80) ? 0 : Has(c);
	}

private:
	uint32_t m_lowerMap[NUM_INTS];
	bool m_hasUTF8;
};

// Don't want this to be inline.
CharSet::CharSet(const char* setOfChars, bool handleUTF8)
{
	memset(m_lowerMap, 0, sizeof(m_lowerMap));
	m_hasUTF8 = false;
	for (const unsigned char *tmp = (const unsigned char*)setOfChars; *tmp; ) {
		const unsigned char c(*tmp++);
		if (c & 0x80) {
			m_hasUTF8 = true;
			tmp += UTF8CharLen(c)-1;
		} else
			m_lowerMap[c/32] |= 1<<(c&31);
	}
	DbgOnlyFatalErrorIf(m_hasUTF8 && handleUTF8, "UTF8 is not yet supported for this operation");
}

// Checks only when DEBUG is turned on.  Otherwise, make these inlined
// empty functions.

#if DEBUG

#error This is not working due to SSharedBuffer changes!

void
SString::_SetUsingAsCString(bool on)
{
	if (on)
		*((uint32_t *)_privateData - 1) |= 0x80000000;
	else
		*((uint32_t *)_privateData - 1) &= ~0x80000000;
}

void
SString::_AssertNotUsingAsCString() const
{
	ASSERT((*((uint32_t *)_privateData - 1) & 0x80000000) == 0);
}

#else

inline void
SString::_SetUsingAsCString(bool )
{
}

inline void
SString::_AssertNotUsingAsCString() const
{
}

#endif

// SString occupies just one word, <_privateData> on the stack.
// When non-empty, _privateData points to the second word of
// a malloced block, which is where the null terminated C-string
// starts. This makes String() trivial and fast.
//
// The length word is stored in the first word of the malloced block.
// To asscess it we go one word before where _privateData points.
// Ref-counting would be implemented by saving the ref count even
// before the length word in the malloced block.
// For this reason it is important that no inlines in the header
// file have any knwledge about these details so that ref-counting
// can be implemented in a compatible way just by rewriting the
// respective low-level calls in this file.
//
// The only thing the header file can rely on is the layout of the
// length word and the fact that _privateData points to the C-string.
//
// The most significant bit is reserved for future use; It doesn't
// matter in this file, but it needs to be properly stripped in
// the Length() inline in the String.h as the inline will be compiled
// into client binaries
//
// For now use the most significant bit for a debugging bit

const SSharedBuffer *
SString::SharedBuffer() const
{
	return SSharedBuffer::BufferFromData(_privateData);
}

void
SString::Pool()
{
	const SSharedBuffer* buf = SharedBuffer();
	if (buf) {
		_privateData = static_cast<const char *>(buf->Pool()->Data());
	}
}

inline SSharedBuffer *
SString::_Edit(size_t newStrLen)
{
	return SSharedBuffer::BufferFromData(_privateData)->Edit(newStrLen + 1);
}

SString::SString()
{
#if !LIBBE_BOOTSTRAP
	SSharedBuffer* sb = g_emptyStringBuffer; // No reference increment
#else
	// Without PalmOS static init help when calling into
	// a library, can't depend on order of static initialization
	SSharedBuffer* sb = SSharedBuffer::Alloc(1); // Performs reference increment.
	memset(sb->Data(), 0, 1);
#endif
	_privateData = (const char*)sb->Data();
// Increment only if needed.
#if !LIBBE_BOOTSTRAP
	sb->IncUsers();
#endif
}

SString::SString(const char *str)
{
	uint32_t length = str ? strlen(str) : 0;
	_Init(str, length);
}

SString::SString(int32_t v)
{
	char str[16];
	sprintf(str, "%d", int(v));
	uint32_t length = strlen(str);
	_Init(str , length);
}

SString::SString(const char *str, int32_t maxLength)
{
	uint32_t length = str ? strnlen(str, maxLength) : 0;
	_Init(str, length);
}

SString::SString(const SString &string)
{
	_privateData = string._privateData;
	SSharedBuffer::BufferFromData(_privateData)->IncUsers();
}

SString::SString(const SSharedBuffer *buf)
{
	_privateData = EmptyString()._privateData;
	if (buf) {
		int32_t buf_length = buf->Length();
		if (buf_length != 0) {
			buf->IncUsers();
			const char *const_data = static_cast<const char *>(buf->Data());
			if (const_data[buf_length - 1] == '\0') {
				_privateData = const_data;
				return;
			} else {
				SSharedBuffer *new_buf = buf->Edit(buf_length + 1);
				if (new_buf) {
					char *data = static_cast<char *>(new_buf->Data());
					data[buf_length] = '\0';
					_privateData = data;
					return;
				} else {
					buf->DecUsers();
				}
			}
		}
	}

	// Error!  Initialize to empty string.
#if !LIBBE_BOOTSTRAP
	SSharedBuffer* sb = g_emptyStringBuffer; // No reference increment
#else
	// Without PalmOS static init help when calling into
	// a library, can't depend on order of static initialization
	SSharedBuffer* sb = SSharedBuffer::Alloc(1); // Performs reference increment.
	memset(sb->Data(), 0, 1);
#endif
	_privateData = (const char*)sb->Data();
// Increment only if needed.
#if !LIBBE_BOOTSTRAP
	sb->IncUsers();
#endif
}

void
SString::MakeEmpty()
{
#if !LIBBE_BOOTSTRAP
	operator = ((const SString&)g_emptyString);
#else
	// can't depend on g_emptyString
	operator = (B_EMPTY_STRING);
#endif
}

const SString&
SString::EmptyString()
{
#if !LIBBE_BOOTSTRAP
	return (const SString&)g_emptyString;
#else
	// can't depend on g_emptyString
	return B_EMPTY_STRING;
#endif
}

SString::~SString()
{
	SSharedBuffer::BufferFromData(_privateData)->DecUsers();
}

void
SString::_Init(const char *str, int32_t length)
{
	_privateData = EmptyString()._privateData;

	ASSERT((!str && !length) || strlen(str) >= (uint32_t)length);
	SSharedBuffer *buf = 0;

	if (length)
		buf = SSharedBuffer::Alloc(length + 1);

	if (buf) {
		char *data = static_cast<char *>(buf->Data());
		if (data) {
			memcpy(data, str, length);
			data[length] = '\0';
			_privateData = data;
			return;
		}
	}

	// Initialize to empty string.
#if !LIBBE_BOOTSTRAP
	buf = g_emptyStringBuffer; // No reference increment
#else
	// Without PalmOS static init help when calling into
	// a library, can't depend on order of static initialization
	buf = SSharedBuffer::Alloc(1); // Performs reference increment.
	memset(buf->Data(), 0, 1);
#endif
	_privateData = (const char*)buf->Data();
// Increment only if needed.
#if !LIBBE_BOOTSTRAP
	buf->IncUsers();
#endif
}

/*
	Here are some notable properties about UTF-8, which I extrapolated from
	http://cui.unige.ch/eao/www/prod/utf-8.7.html  --joeo

	The first byte of a single byte character is always in
	the range of 0x00 to 0x7f (0000 0000 to 0111 1111)

	The first byte of a multiple byte character is always
	in the range of 0xc0 to 0xfd (1100 0000 to 1111 1101)

	The other bytes of a multiple byte character are always
	in the range of 0x80 to 0xbf (1000 0000 to 1011 1111)

	The values 0xfe (1111 1110) and 0xff (1111 1111) are not
	valid utf-8 values.

	Quoted from the above source:

	The following byte sequences are used to represent a character.
	The sequence to be used depends on the UCS code number of the
	character:

	0x00000000 - 0x0000007F:
	0xxx xxxx

	0x00000080 - 0x000007FF:
	110x xxxx 10xxxxxx

	0x00000800 - 0x0000FFFF:
	1110 xxxx 10xxxxxx 10xxxxxx

	0x00010000 - 0x001FFFFF:
	1111 0xxx 10xxxxxx 10xxxxxx 10xxxxxx

	The xxx bit positions are filled with the bits of the character
	code number in binary representation. Only the shortest possible
	multibyte sequence which can represent the code number of the
	character can be used.
*/

int32_t
SString::CountChars() const
{
	int32_t numChars = 0;
	int32_t len = Length();

	for (int32_t i = 0; i < len; i += UTF8CharLen(_privateData[i]))
		numChars++;

	return numChars;
}

int32_t
SString::FirstForChar(int32_t index) const
{
	if (index < 0 || index >= Length()) return -1;

	// this is a single-byte character
	if (!(_privateData[index] & 0x80)) return index;

	// this is multi-byte character
	int32_t stop = index - 7;
	if (stop < -1) stop = -1;
	do {
		if ((_privateData[index] & 0xc0) != 0x80) return index;
		index--;
	} while (index > stop);
	return -1; // stop is one past a valid value.
}

int32_t
SString::NextCharFor(int32_t index) const
{
	int32_t length = Length();
	if (index < 0 || index >= length) return -1;

	// this is a single-byte character
	if (!(_privateData[index] & 0x80)) return index+1;

	// this is a multi-byte character
	int32_t stop = index + 7;
	index++;
	do {
		uint8_t byte = _privateData[index];
		if (byte == '\0') return length;
		if ((byte & 0xc0) != 0x80) return index;
		index++;
	} while (index < stop);
	return -1;
}

int32_t
SString::NextChar(int32_t index) const
{
	int32_t length = Length();
	if (index < 0 || index >= length) return -1;
	int32_t result = index + UTF8CharLen(_privateData[index]);
	if (result > length) return -1;
	return result;
}

int32_t
SString::CharLength(int32_t index) const
{
	int32_t length = Length();
	if (index < 0 || index >= length) return -1;
	uint8_t c = _privateData[index];
	if ((c & 0xc0) == 0x80) return 0;
	return UTF8CharLen(c);
}

#define UTF8_SHIFT_AND_MASK(unicode, byte)  (unicode)<<=6; (unicode) |= (0x3f & (byte));

wchar32_t
SString::CharAt(int32_t index) const
{
	int32_t length = Length();
	if (index < 0 || index > length) return 0xffff;
	if (index == 0 && length == 0) return 0;
	uint8_t c = _privateData[index];
	if (c == 0) return 0;
	if ((c & 0xc0) == 0x80) return 0xffff;
	int32_t chars = UTF8CharLen(c);
	if (index+chars > length) return 0xffff;

	uint32_t unicode;

	switch (chars)
	{
		case 1:
			return c;
		case 2:
			unicode = c & 0x1f;
			UTF8_SHIFT_AND_MASK(unicode, _privateData[index+1])
			return unicode;
		case 3:
			unicode = c & 0x0f;
			UTF8_SHIFT_AND_MASK(unicode, _privateData[index+1])
			UTF8_SHIFT_AND_MASK(unicode, _privateData[index+2])
			return unicode;
		case 4:
			unicode = c & 0x07;
			UTF8_SHIFT_AND_MASK(unicode, _privateData[index+1])
			UTF8_SHIFT_AND_MASK(unicode, _privateData[index+2])
			UTF8_SHIFT_AND_MASK(unicode, _privateData[index+3])
			return unicode;
		default:
			return 0xffff;
	}
}

#define GET_UTF8_LAST_BYTE(unicode)		(char)((0x80 | ((unicode) & 0x3f)))
#define GET_UTF8_BYTE(unicode, shift)	(char)((0x80 | (((unicode) >> (shift)) & 0x3f)))

SString &
SString::AppendChar(wchar32_t unicode)
{
  	char s[7];
  	int32_t length;
	if (unicode == 0) {
		return *this;
	}
  	if (unicode <= 0x0000007F) {
  		s[0] = (char)(unicode);
  		s[1] = 0;
  		length = 1;
  	}
  	else if (unicode <= 0x000007FF) {
  		s[0] = (char)(0xc0 | (unicode >> 6));
  		s[1] = GET_UTF8_LAST_BYTE(unicode);
  		s[2] = 0;
  		length = 2;
  	}
  	else if (unicode <= 0x0000FFFF) {
  		s[0] = (char)(0xe0 | (unicode >> 12));
  		s[1] = GET_UTF8_BYTE(unicode, 6);
  		s[2] = GET_UTF8_LAST_BYTE(unicode);
  		s[3] = 0;
  		length = 3;
  	}
  	else if (unicode <= 0x001FFFFF) {
  		s[0] = (char)(0xf0 | (unicode >> 18));
  		s[1] = GET_UTF8_BYTE(unicode, 12);
  		s[2] = GET_UTF8_BYTE(unicode, 6);
  		s[3] = GET_UTF8_LAST_BYTE(unicode);
  		s[4] = 0;
  		length = 4;
  	}
  	else {
  		// these aren't valid characters, don't append them
  		return *this;
  	}
  	_DoAppend(s, length);
	return *this;
}


// memory management calls

void
SString::_DoAppend(const SString& s)
{
	const char * const str = s.String();
	const int32_t length = s.Length();
	_DoAppend(str, length);
}

void
SString::_DoAppend(const char *str, int32_t length)
{
	_AssertNotUsingAsCString();
	ASSERT((!str && !length) || length <= (int32_t)strlen(str));

	if (!str || !length)
		return;

	int32_t oldLength = Length();
	int32_t newLength = length + oldLength;

	SSharedBuffer *buf = _Edit(newLength);
	if (buf) {
		char *data = static_cast<char *>(buf->Data());
		if (length == 1) {
			// this case happens very often.
			data[oldLength] = *str;
		} else {
			memcpy(data + oldLength, str, length);
		}
		data[newLength] = '\0';
		_privateData = data;
	}
}

char *
SString::_GrowBy(int32_t length)
{
	// caller needs to fill the resulting space with something
	_AssertNotUsingAsCString();
	ASSERT(length);
	int32_t oldLength = Length();
	int32_t newLength = length + oldLength;

	SSharedBuffer *buf = _Edit(newLength);
	if (buf) {
		char *data = static_cast<char *>(buf->Data());
		data[newLength] = '\0';
		_privateData = data;
		return data + oldLength;
	}
	return 0;
}

void
SString::_DoPrepend(const char *str, int32_t length)
{
	_AssertNotUsingAsCString();
	ASSERT((!str && !length) || length <= (int32_t)strlen(str));

	if (!str || !length)
		return;

	int32_t oldLength = Length();
	int32_t newLength = oldLength + length;

	SSharedBuffer *newBuf = SSharedBuffer::Alloc(newLength + 1);
	if (newBuf) {
		char *data = static_cast<char *>(newBuf->Data());
		memcpy(data, str, length);
		memcpy(data + length, _privateData, oldLength);
		SSharedBuffer::BufferFromData(_privateData)->DecUsers();
		data[newLength] = '\0';
		_privateData = data;
	}
}

char *
SString::_OpenAtBy(int32_t offset, int32_t length)
{
	_AssertNotUsingAsCString();
	ASSERT(length > 0);
	int32_t oldLength = Length();
	ASSERT(offset <= oldLength);
	int32_t newLength = length + oldLength;

	// if we knew that realloc will result in a memory move, we could
	// use malloc and memcopy instead to avoid memcopying to the wrong
	// place first and then moving it to the right one
	//
	// could use a heuristic here, doing the above if <length> is "a lot"

	SSharedBuffer *buf = _Edit(newLength);
	if (buf) {
		char *data = static_cast<char *>(buf->Data());
		data[newLength] = '\0';
		memmove(data + offset + length, data + offset, oldLength - offset);
		_privateData = data;
		return data + offset;
	}
	return 0;
}

void
SString::_DoSetTo(const char *str, int32_t newLength)
{
	_AssertNotUsingAsCString();
	ASSERT((!str && !newLength) || newLength <= (int32_t)strlen(str));

	if (newLength) {
		int32_t length = Length();

		if (str >= _privateData && str <= _privateData + length) {
			// handle self assignment case
			SSharedBuffer *buf = SSharedBuffer::Alloc(newLength + 1);
			if (buf) {
				char *data = static_cast<char *>(buf->Data());
				memcpy(data, str, newLength);
				data[newLength] = '\0';
				SSharedBuffer::BufferFromData(_privateData)->DecUsers();
				_privateData = data;
			}
		}
		else {
			SSharedBuffer *buf = _Edit(newLength);
			if (buf) {
				char *data = static_cast<char *>(buf->Data());
				memcpy(data, str, newLength);
				data[newLength] = '\0';
				_privateData = data;
			}
		}
	}
	else {
		MakeEmpty();
	}
}

char *
SString::_ShrinkAtBy(int32_t offset, int32_t length)
{
	int32_t oldLength = Length();
	ASSERT(length && offset + length <= oldLength);
	int32_t fromOffset = offset + length;
	int32_t newLength = oldLength - length;

	if (newLength > 0) {
		SSharedBuffer *buf = _Edit(oldLength);
		if (buf) {
			char *data = static_cast<char *>(buf->Data());
			if (fromOffset < oldLength)
				memmove(data + offset, data + fromOffset, oldLength - fromOffset);
			data[newLength] = '\0';
			buf = buf->Edit(newLength + 1);
			data = static_cast<char *>(buf->Data());
			_privateData = data;
			return data;
		}
	} else {
		MakeEmpty();
	}

	return 0;
}

SString &
SString::SetTo(const char *str, int32_t maxLength)
{
	_AssertNotUsingAsCString();

	int32_t newLength = str ? strnlen(str, maxLength) : 0;
	_DoSetTo(str, newLength);

	return *this;
}

SString &
SString::SetTo(char ch, int32_t length)
{
	_AssertNotUsingAsCString();
	if (length) {
		SSharedBuffer *buf = _Edit(length);
		char *data = static_cast<char *>(buf->Data());
		memset(data, ch, length);
		data[length] = '\0';
		_privateData = data;
	}
	else {
		MakeEmpty();
	}
	return *this;
}

SString &
SString::Printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	size_t size = 256;
	int nchars = 0;
	char* buffer = LockBuffer(size);
	if (buffer) {
		do {
			nchars = vsnprintf(buffer, size, fmt, ap);
			if (nchars < 0) nchars *= 2;
			else if ((size_t)nchars >= size) nchars += 1;
			else break;
			UnlockBuffer(size);
			size = nchars;
			buffer = LockBuffer(size);
		} while (buffer);
	}
	if (buffer) {
		UnlockBuffer(nchars);
	}
	va_end(ap);
	return *this;
}

void
SString::Swap(SString& with)
{
	const char* tmp = _privateData;
	_privateData = with._privateData;
	with._privateData = tmp;
}

SString &
SString::operator=(const SString &string)
{
	// guard against self asignment
	if (string._privateData != _privateData) {
		SSharedBuffer::BufferFromData(_privateData)->DecUsers();
		_privateData = string._privateData;
		SSharedBuffer::BufferFromData(_privateData)->IncUsers();
	}
	return *this;
}

SString &
SString::operator=(const char *str)
{
	if (str != _privateData) {
		int32_t newLength = str ? strlen(str) : 0;
		_DoSetTo(str, newLength);
	}

	return *this;
}

SString &
SString::SetTo(const SString &string)
{
	return operator = (string);
}

SString &
SString::SetTo(const SString &string, int32_t length)
{
	int32_t fromLength = string.Length();
	if (fromLength < length)
		length = fromLength;

	if (length == fromLength)
		SetTo(string);
	else
		_DoSetTo(string._privateData, length);

	return *this;
}

SString &
SString::CopyInto(SString &string, int32_t fromOffset, int32_t length) const
{
	if (length >= 0) {
		if (fromOffset+length > Length())
			length = Length()-fromOffset;
		if (length < 0)
			return string;
	}

	return string.SetTo(String() + fromOffset, length);
}

void
SString::CopyInto(char *into, int32_t fromOffset, int32_t length) const
{
	if (!into)
		return;

	if (length >= 0) {
		int32_t oldLength = (Length() + 1);
		if (length > oldLength - fromOffset)
			length = oldLength - fromOffset;
		if (length <= 0)
			return;
	}

	memcpy(into, String() + fromOffset, length);
}

SString &
SString::operator+=(const char *str)
{
	int32_t length = str ? strlen(str) : 0;
	_DoAppend(str, length);
	return *this;
}

SString &
SString::operator+=(char ch)
{
	const char tmp[2] = {ch, '\0'};
	_DoAppend(tmp, 1);
	return *this;
}

SString &
SString::Append(const SString &string, int32_t length)
{
	int32_t otherLength = string.Length();
	if (length > otherLength)
		length = otherLength;

	if (&string != this) {
		_DoAppend(string.String(), length);
	}
	else {
		int32_t oldLength = Length();
		SSharedBuffer *buf = _Edit(oldLength + length);
		char *data = static_cast<char *>(buf->Data());
		memcpy(data + oldLength, data, length);
		data[oldLength + length] = '\0';
		_privateData = data;
	}
	return *this;
}

SString &
SString::Append(const char *str, int32_t length)
{
	int32_t tmpLength = str ? strnlen(str, length) : 0;
	_DoAppend(str, tmpLength);
	return *this;
}

SString &
SString::Append(char ch, int32_t count)
{
	char *fillStart = _GrowBy(count);
	if (fillStart)
		memset(fillStart, ch, count);

	return *this;
}

SString &
SString::Prepend(const char *str)
{
	int32_t tmpLength = str ? strlen(str) : 0;

	_DoPrepend(str, tmpLength);
	return *this;
}

SString &
SString::Prepend(const SString &string)
{
	if (&string != this)
		_DoPrepend(string.String(), string.Length());

	return *this;
}

SString &
SString::Prepend(const char *str, int32_t length)
{
	int32_t tmpLength = str ? strnlen(str, length) : 0;
	_DoPrepend(str, tmpLength);
	return *this;
}

SString &
SString::Prepend(const SString &string, int32_t length)
{
	if (&string != this) {
		int32_t otherLength = string.Length();
		if (length > otherLength)
			length = otherLength;

		_DoPrepend(string.String(), length);
	}
	return *this;
}

SString &
SString::Prepend(char ch, int32_t count)
{
	char *fillStart = _OpenAtBy(0, count);
	if (fillStart)
		memset(fillStart, ch, count);

	return *this;
}

SString &
SString::Insert(const char *str, int32_t pos)
{
	if (str) {
		int32_t length = strlen(str);

		char *fillStart = _OpenAtBy(pos, length);
		if (fillStart)
			memcpy(fillStart, str, length);
	}
	return *this;
}

SString &
SString::Insert(const char *str, int32_t length, int32_t pos)
{
	ASSERT((!str && !length) || length <= (int32_t)strlen(str));

	if (!str)
		length = 0;

	if (length) {
		int32_t tmpLength = strnlen(str, length);
		char *fillStart = _OpenAtBy(pos, tmpLength);
		if (fillStart)
			memcpy(fillStart, str, tmpLength);
	}
	return *this;
}

SString &
SString::Insert(const char *str, int32_t fromOffset, int32_t length, int32_t pos)
{
	ASSERT((!str && !length) || length <= (int32_t)strlen(str));

	if (str)
		str += fromOffset;
	else
		length = 0;

	if (length) {
		int32_t tmpLength = strnlen(str, length);
		char *fillStart = _OpenAtBy(pos, tmpLength);
		if (fillStart)
			memcpy(fillStart, str, tmpLength);
	}
	return *this;
}

SString &
SString::Insert(const SString &string, int32_t pos)
{
	int32_t length = string.Length();
	if (&string != this && length) {
		char *fillStart = _OpenAtBy(pos, length);
		if (fillStart)
			memcpy(fillStart, string._privateData, length);
	}
	return *this;
}

SString &
SString::Insert(const SString &string, int32_t length, int32_t pos)
{
	if (&string != this) {
		int32_t stringLength = string.Length();
		if (length > stringLength)
			length = stringLength;

		if (length) {
			char *fillStart = _OpenAtBy(pos, length);
			if (fillStart)
				memcpy(fillStart, string._privateData, length);
		}
	}
	return *this;
}

SString &
SString::Insert(const SString &string, int32_t fromOffset, int32_t length, int32_t pos)
{
	if (&string != this) {
		int32_t stringLength = string.Length();
		if (length > stringLength - fromOffset)
			length = stringLength - fromOffset;

		if (length) {
			char *fillStart = _OpenAtBy(pos, length);
			if (fillStart)
				memcpy(fillStart, string._privateData + fromOffset, length);
		}
	}
	return *this;
}

SString &
SString::Insert(char ch, int32_t count, int32_t pos)
{
	if (count) {
		char *fillStart = _OpenAtBy(pos, count);
		if (fillStart)
			memset(fillStart, ch, count);
	}
	return *this;
}

SString &
SString::Truncate(int32_t newLength)
{
	_AssertNotUsingAsCString();
	if (newLength < Length()) {
		if (newLength <= 0) {
			MakeEmpty();
		}
		else {
			SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(newLength + 1);
			if (buf) {
				char *data = static_cast<char *>(buf->Data());
				data[newLength] = '\0';
				_privateData = data;
			}
		}
	}

	return *this;
}

SString &
SString::Remove(int32_t from, int32_t length)
{
	if (length > 0) {
		int32_t oldLength = Length();
		if (from < oldLength) {
			if (length > oldLength - from)
				length = oldLength - from;
			_ShrinkAtBy(from, length);
		}
	}

	return *this;
}

SString &
SString::RemoveFirst(const SString &string)
{
	int32_t length = string.Length();
	if (length) {
		int32_t index = FindFirst(string);
		if (index >= 0)
			_ShrinkAtBy(index, length);
	}
	return *this;
}

SString &
SString::RemoveLast(const SString &string)
{
	int32_t length = string.Length();
	if (length) {
		int32_t index = FindLast(string);
		if (index >= 0)
			_ShrinkAtBy(index, length);
	}
	return *this;
}

SString &
SString::RemoveAll(const SString &pattern)
{
	int32_t length = pattern.Length();
	if (length) {
		for (int32_t index = 0;;) {
			index = FindFirst(pattern, index);
			if (index < 0)
				break;
			_ShrinkAtBy(index, length);
		}
	}
	return *this;
}

SString &
SString::RemoveFirst(const char *str)
{
	int32_t length = str ? strlen(str) : 0;
	if (length) {
		int32_t index = FindFirst(str);
		if (index >= 0)
			_ShrinkAtBy(index, length);
	}
	return *this;
}

SString &
SString::RemoveLast(const char *str)
{
	int32_t length = str ? strlen(str) : 0;
	if (length) {
		int32_t index = FindLast(str);
		if (index >= 0)
			_ShrinkAtBy(index, length);
	}
	return *this;
}

SString &
SString::RemoveAll(const char *str)
{
	int32_t length = str ? strlen(str) : 0;
	if (length) {
		for (int32_t index = 0;;) {
			index = FindFirst(str, index);
			if (index < 0)
				break;
			_ShrinkAtBy(index, length);
		}
	}
	return *this;
}

SString &
SString::RemoveSet(const char *setOfCharsToRemove)
{
	const CharSet chars(setOfCharsToRemove, false);

	char *data = NULL;
	int32_t length = Length();
	if (chars.HasUTF8()) {
		for (int32_t index = 0; index < length;) {
			const unsigned char c = (unsigned char)(_privateData[index]);
			if (c <= 127) {
				if (chars.Has(c)) {
					int32_t fromOffset = index + 1;
					if (!data) {
						SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
						if (!buf) {
							return *this;
						}
						data = static_cast<char *>(buf->Data());
						_privateData = data;
					}
					memcpy(data + index, data + fromOffset,	length - fromOffset);
					length -= 1;
				} else
					index++;
			} else {
				const int32_t utflen = UTF8CharLen(c);
				const uint32_t utf = UTF8CharToUint32((const uint8_t*)(_privateData + index), utflen);
				bool found = false;
				for (const char *tmp = setOfCharsToRemove; *tmp && !found; ) {
					if (*tmp & 0x80) {
						const int32_t len = UTF8CharLen(*tmp);
						if (utf == UTF8CharToUint32((const uint8_t*)tmp, len)) {
							const int32_t fromOffset = index + len;
							if (!data) {
								SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
								if (!buf) {
									return *this;
								}
								data = static_cast<char *>(buf->Data());
								_privateData = data;
							}
							memcpy(data + index, data + fromOffset, length - fromOffset);
							length -= len;
							found = true;
						} else {
							tmp += len;
						}
					} else {
						tmp++;
					}
				}
				if (!found) index += utflen;
			}
		}
	} else {
		for (int32_t index = 0; index < length;)
			if (chars.Has(_privateData+index)) {
				int32_t fromOffset = index + 1;
				if (!data) {
					SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
					if (!buf) {
						return *this;
					}
					data = static_cast<char *>(buf->Data());
					_privateData = data;
				}
				memcpy(data + index, data + fromOffset, length - fromOffset);
				length -= 1;
			} else
				index++;
	}
	if (length < Length())
		Truncate(length);

	return *this;
}

SString &
SString::Compact(const char *setOfCharsToCompact)
{
	_DoCompact(setOfCharsToCompact, NULL);
	return *this;
}

SString &
SString::Compact(const char *setOfCharsToCompact, char* replacementChar)
{
	_DoCompact(setOfCharsToCompact, replacementChar);
	return *this;
}

/*
	I think there might be a bug in _DoCompact(), but I'm not sure, because
	I'm not sure what it's _supposed_ to do (it's undocumented).  It appears
	that what it's supposed to do is find runs of compactable characters
	(given by setOfCharsToCompact), and replace them by the first such
	character in the run.  If you give it a non-NULL "replace" pointer, it
	replaces the run by the character *replace instead.

	The possible bug is that the behavior is different if the string starts
	or ends with compactable characters.  In that case, the compactable
	characters are simply removed.  This is probably what you want if you're,
	say, compacting white space, but it seems a bit odd for a generalized
	function.
		-- Laz, 2001-10-17
*/

void
SString::_DoCompact(const char* setOfCharsToCompact, const char* replace)
{
	const CharSet chars(setOfCharsToCompact);

	char *data = NULL;
	int32_t length = Length();
	int32_t from = 0;
	int32_t to = 0;
	bool lastSpace = true;

	while (from < length) {
		if (chars.Has(_privateData+from)) {
			if (!lastSpace) {
				if (replace) {
					if (!data) {
						SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
						if (!buf) {
							return;
						}
						data = static_cast<char *>(buf->Data());
						_privateData = data;
					}
					data[to] = *replace;
				}
				++to;
				lastSpace = true;
			}
		}
		else {
			if (to < from) {
				if (!data) {
					SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
					if (!buf) {
						return;
					}
					data = static_cast<char *>(buf->Data());
					_privateData = data;
				}
				data[to] = data[from];
			}
			lastSpace = false;
			++to;
		}
		++from;
	}
	if (to > 0 && lastSpace)
		--to;

	if (to < length)
		Truncate(to);
}


SString &
SString::MoveInto(SString &into, int32_t from, int32_t length)
{
	if (length) {
		CopyInto(into, from, length);
		Remove(from, length);
	}
	return into;
}

void
SString::MoveInto(char *into, int32_t from, int32_t length)
{
	if (length) {
		CopyInto(into, from, length);
		Remove(from, length);
	}
}

bool
SString::operator<(const char *str) const
{
	_AssertNotUsingAsCString();
	// can't pass zero pointers to strcmp
	if (!Length())
		return true;

	if (!str)
		return false;

	return strcmp(str, _privateData) > 0;
}

bool
SString::operator<=(const char *str) const
{
	_AssertNotUsingAsCString();
	// can't pass zero pointers to strcmp
	if (!Length())
		return true;

	if (!str)
		return false;

	return strcmp(str, _privateData) >= 0;
}

bool
SString::operator==(const char *str) const
{
	_AssertNotUsingAsCString();
	// can't pass zero pointers to strcmp
	if (!Length() || !str)
		return (!str || *str == 0) == (Length() == 0);

	return strcmp(str, _privateData) == 0;
}

bool
SString::operator>=(const char *str) const
{
	_AssertNotUsingAsCString();
	// can't pass zero pointers to strcmp
	if (!str)
		return true;

	if (!Length())
		return false;

	return strcmp(str, _privateData) <= 0;
}

bool
SString::operator>(const char *str) const
{
	_AssertNotUsingAsCString();
	// can't pass zero pointers to strcmp
	if (!str)
		return true;

	if (!Length())
		return false;

	return strcmp(str, _privateData) < 0;
}

int
SString::Compare(const SString &string) const
{
	return strcmp(String(), string.String());
}

int
SString::Compare(const char *str) const
{
	if (!str)
		return Length() ? 1 : 0;
	return strcmp(String(), str);
}

int
SString::Compare(const SString &string, int32_t n) const
{
	return strncmp(String(), string.String(), n);
}

int
SString::Compare(const char *str, int32_t n) const
{
	if (!str)
		return Length() ? 1 : 0;
	return strncmp(String(), str, n);
}

int
SString::ICompare(const SString &string) const
{
	return strcasecmp(String(), string.String());
}

int
SString::ICompare(const char *str) const
{
	if (!str)
		return Length() ? 1 : 0;
	return strcasecmp(String(), str);
}

int
SString::ICompare(const SString &string, int32_t n) const
{
	return strncasecmp(String(), string.String(), n);
}

int
SString::ICompare(const char *str, int32_t n) const
{
	if (!str)
		return Length() ? 1 : 0;
	return strncasecmp(String(), str, n);
}

const int32_t kShortFindAfterTreshold = 100*1024;

inline int32_t
SString::_ShortFindAfter(const char *str, int32_t fromOffset) const
{
	const char *s1 = _privateData + fromOffset;
	const char *p1 = str;
	char firstc, c1;

	if ((firstc = *p1++) == 0)
		return 0;

	while((c1 = *s1++) != 0)
		if (c1 == firstc)
		{
			const char *s2 = s1;
			const char *p2 = p1;
			char c2;

			while ((c1 = *s2++) == (c2 = *p2++) && c1)
				;

			if (!c2)
				return (s1 - 1) - _privateData;
		}

	return -1;
}


int32_t
SString::_FindAfter(const char *str, int32_t length, int32_t fromOffset) const
{
	int32_t stringLength = Length();
	if (length > stringLength)
		return -1;

	ASSERT(str && length && strlen(str) >= (uint32_t)length);
	ASSERT(stringLength - fromOffset >= length);

	// KMP search
	_AllocaBuffer<int32_t> next(length + 1);

	int32_t i = 0;
	int32_t j = -1;
	next.EditItemAt(0) = -1;
	do {
		if (j == -1 || str[i] == str[j]) {
			i++;
			j++;
			next[i] = (str[j] == str[i]) ? next[j] : j;
		} else
			j = next[j];
	} while (i < length);

	int32_t result = -1;
	for (int32_t ii = fromOffset, jj = 0; ii < stringLength; ) {
		if (jj == -1 || str[jj] == _privateData[ii]) {
			ii++;
			jj++;
			if (jj >= length) {
				result = ii - jj;
				break;
			}
		} else
			jj = next[jj];
	}

#if DEBUG
	// double-check our results using a slow but simple approach
	char *debugPattern = strdup(str);
	debugPattern[length] = '\0';
	char *tmpCheck = strstr(_privateData + fromOffset, debugPattern);
	ASSERT((!tmpCheck && result == -1) || (tmpCheck - _privateData == result));
	free (debugPattern);
#endif

	return result;
}

int32_t
SString::_IFindAfter(const char *str, int32_t length, int32_t fromOffset) const
{
	_AllocaBuffer<char> pattern(length + 1);
	for (int32_t index = 0; index < length; index++)
		pattern[index] = UTF8SafeToLower(str[index]);
	pattern[length] = '\0';

	const char *s1 = _privateData + fromOffset;
	const char *p1 = pattern;
	char firstc, c1;

// printf(">>>>>>pattern>%s<\n", p1);
	if ((firstc = *p1++) == 0)
		return 0;

	while((c1 = *s1++) != 0)
		if (UTF8SafeToLower(c1) == firstc)
		{
			const char *s2 = s1;
			const char *p2 = p1;
			char c2;

			while (UTF8SafeToLower(c1 = *s2++) == (c2 = *p2++) && c1)
				;

			if (!c2)
				return s1 - 1 - _privateData;
		}

	return -1;

#if 0
	int32_t stringLength = Length();
	if (length > stringLength)
		return -1;

	ASSERT(str && length && strlen(str) >= length);
	ASSERT(stringLength - fromOffset >= length);

	_AllocaBuffer<int32_t> pattern(length + 1);
	for (int32_t index = 0; index < length; index++)
		pattern[index] = UTF8SafeToLower(str[index]);

	// KMP search
	_AllocaBuffer<int32_t> next(length + 1);

	int32_t i = 0;
	int32_t j = -1;
	next[0] = -1;
	do {
		if (j == -1 || pattern[i] == pattern[j]) {
			i++;
			j++;
			next[i] = (pattern[j] == pattern[i]) ? next[j] : j;
		} else
			j = next[j];
	} while (i < length);

	int32_t result = -1;
	for (int32_t i = fromOffset, j = 0; i < stringLength; ) {
		if (j == -1 || pattern[j] == UTF8SafeToLower(_privateData[i])) {
			i++;
			j++;
			if (j >= length) {
				result = i - j;
				break;
			}
		} else
			j = next[j];
	}

#if DEBUG
	// double-check our results using a slow dumb search
	char *debugPattern = strdup(str);

	for (int32_t index = 0; index < length; index++)
		debugPattern[index] = UTF8SafeToLower(str[index]);

	debugPattern[length] = '\0';

	char *tmpCheck = strstr(_privateData + fromOffset, debugPattern);
	ASSERT((!tmpCheck && result == -1) || (tmpCheck - _privateData == result));
	free (debugPattern);
#endif

	return result;
#endif
}

int32_t
SString::FindFirst(const SString &pattern) const
{
	int32_t length = Length();
	int32_t patternLength = pattern.Length();
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (patternLength > length)
		return -1;

	if (length < kShortFindAfterTreshold)
		return _ShortFindAfter(pattern._privateData, 0);

	return _FindAfter(pattern._privateData, patternLength, 0);
}

int32_t
SString::FindFirst(const char *str) const
{
	if (!Length())
		return -1;

	return _ShortFindAfter(str, 0);
}

int32_t
SString::FindFirst(const char *str, int32_t fromOffset) const
{
	if (Length() - fromOffset <= 0)
		return -1;

	return _ShortFindAfter(str, fromOffset);
}


int32_t
SString::FindFirst(const SString &pattern, int32_t fromOffset) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	int32_t patternLength = pattern.Length();
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (patternLength > length - fromOffset)
		return -1;

	if (length < kShortFindAfterTreshold)
		return _ShortFindAfter(pattern._privateData, fromOffset);

	return _FindAfter(pattern._privateData, patternLength, fromOffset);
}

int32_t
SString::IFindFirst(const SString &pattern) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	int32_t patternLength = pattern.Length();
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (patternLength > length)
		return -1;

	return _IFindAfter(pattern._privateData, patternLength, 0);
}

int32_t
SString::IFindFirst(const char *str) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	int32_t patternLength = str ? strlen(str) : 0;
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (patternLength > length)
		return -1;

	return _IFindAfter(str, patternLength, 0);
}

int32_t
SString::IFindFirst(const SString &pattern, int32_t fromOffset) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	int32_t patternLength = pattern.Length();
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (patternLength > length - fromOffset)
		return -1;

	return _IFindAfter(pattern._privateData, patternLength, fromOffset);
}

int32_t
SString::IFindFirst(const char *str, int32_t fromOffset) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	int32_t patternLength = str ? strlen(str) : 0;
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (patternLength > length - fromOffset)
		return -1;

	return _IFindAfter(str, patternLength, fromOffset);
}

int32_t
SString::_FindBefore(const char *str, int32_t length, int32_t beforeOffset) const
{
	ASSERT(Length() >= beforeOffset);
	ASSERT(Length() >= length);

	const char *s1 = _privateData + beforeOffset - length;
	const char *to = _privateData;

	for (;;) {
		const char *s2 = s1--;
		if (s2 < to)
			break;

		const char *pat = str;
		char c1, c2;
		while ((c1 = *s2++) == (c2 = *pat++) && c2)
			;

		if (!c2)
			return  s1 + 1 - _privateData;
	}
	return -1;
}

int32_t
SString::_IFindBefore(const char *str, int32_t length, int32_t beforeOffset) const
{
	ASSERT(Length() >= beforeOffset);
	ASSERT(Length() >= length);

	_AllocaBuffer<char> pattern(length + 1);
	for (int32_t index = 0; index < length; index++)
		pattern[index] = UTF8SafeToLower(str[index]);
	pattern[length] = '\0';

	const char *s1 = _privateData + beforeOffset - length;
	const char *to = _privateData;

	for (;;) {
		const char *s2 = s1--;
		if (s2 < to)
			break;

		const char *pat = pattern;
		char c1, c2;
		while (UTF8SafeToLower(c1 = *s2++) == (c2 = *pat++) && c2)
			;

		if (!c2)
			return s1 + 1 - _privateData ;
	}
	return -1;
}

int32_t
SString::FindLast(const SString &pattern) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	int32_t patternLength = pattern.Length();
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (patternLength > length)
		return -1;

	return _FindBefore(pattern.String(), patternLength, length);
}

int32_t
SString::FindLast(const char *str) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	if (!str)
		// everything matches null pattern
		return 0;

	int32_t patternLength = strlen(str);
	if (patternLength > length)
		return -1;

	return _FindBefore(str, patternLength, length);
}

int32_t
SString::FindLast(const SString &pattern, int32_t beforeOffset) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	int32_t patternLength = pattern.Length();
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (beforeOffset > length)
		beforeOffset = length;

	if (patternLength > beforeOffset)
		return -1;

	return _FindBefore(pattern.String(), patternLength, beforeOffset);
}

int32_t
SString::FindLast(const char *str, int32_t beforeOffset) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	if (!str)
		return 0;

	int32_t patternLength = strlen(str);
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (beforeOffset > length)
		beforeOffset = length;

	if (patternLength > beforeOffset)
		return -1;

	return _FindBefore(str, patternLength, beforeOffset);
}

int32_t
SString::IFindLast(const SString &pattern) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	int32_t patternLength = pattern.Length();
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (patternLength > length)
		return -1;

	return _IFindBefore(pattern.String(), patternLength, length);
}

int32_t
SString::IFindLast(const char *str) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	if (!str)
		// everything matches null pattern
		return 0;

	int32_t patternLength = strlen(str);
	if (patternLength > length)
		return -1;

	return _IFindBefore(str, patternLength, length);
}

int32_t
SString::IFindLast(const SString &pattern, int32_t beforeOffset) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	int32_t patternLength = pattern.Length();
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (beforeOffset > length)
		beforeOffset = length;

	if (patternLength > beforeOffset)
		return -1;

	return _IFindBefore(pattern.String(), patternLength, beforeOffset);
}

int32_t
SString::IFindLast(const char *str, int32_t beforeOffset) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	if (!str)
		return 0;

	int32_t patternLength = strlen(str);
	if (!patternLength)
		// everything matches null pattern
		return 0;

	if (beforeOffset > length)
		beforeOffset = length;

	if (patternLength > beforeOffset)
		return -1;

	return _IFindBefore(str, patternLength, beforeOffset);
}

int32_t
SString::FindFirst(char ch) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	for (int32_t index = 0; index < length; index++)
		if (_privateData[index] == ch)
			return index;

	return -1;
}

int32_t
SString::FindFirst(char ch, int32_t fromOffset) const
{
	int32_t length = Length();
	if (length <= fromOffset)
		return -1;

	if (fromOffset < 0)
		fromOffset = 0;
	for (int32_t index = fromOffset; index < length; index++)
		if (_privateData[index] == ch)
			return index;

	return -1;
}

int32_t
SString::FindLast(char ch) const
{
	int32_t length = Length();
	if (!length)
		return -1;

	for (int32_t index = length - 1; index >= 0; index--)
		if (_privateData[index] == ch)
			return index;

	return -1;
}

int32_t
SString::FindLast(char ch, int32_t beforeOffset) const
{
	int32_t length = Length();
	if (beforeOffset < 0 || !length)
		return -1;

	if (beforeOffset >= length)
		beforeOffset = length - 1;

	for (int32_t index = beforeOffset; index >= 0; index--)
		if (_privateData[index] == ch)
			return index;

	return -1;
}

int32_t
SString::FindSetFirst(const char *setOfChars)
{
	return FindSetFirst(setOfChars, 0);
}

int32_t
SString::FindSetFirst(const char *setOfChars, int32_t fromOffset)
{
	int32_t length = Length();
	if (length <= fromOffset)
		return -1;

	const CharSet chars(setOfChars);

	if (fromOffset < 0)
		fromOffset = 0;
	for (int32_t index = fromOffset; index < length; index++)
		if (chars.Has(_privateData+index))
			return index;

	return -1;
}

int32_t
SString::FindSetLast(const char *setOfChars)
{
	return FindSetLast(setOfChars, 0x7fffffff);
}

int32_t
SString::FindSetLast(const char *setOfChars, int32_t beforeOffset)
{
	int32_t length = Length();
	if (beforeOffset < 0 || !length)
		return -1;

	const CharSet chars(setOfChars);

	if (beforeOffset >= length)
		beforeOffset = length - 1;

	for (int32_t index = beforeOffset; index >= 0; index--)
		if (chars.Has(_privateData+index))
			return index;

	return -1;
}

SString &
SString::ReplaceFirst(char ch1, char ch2)
{
	int32_t length = Length();
	for (int32_t index = 0; index < length; index++)
		if (_privateData[index] == ch1) {
			SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
			if (buf) {
				char *data = static_cast<char *>(buf->Data());
				data[index] = ch2;
				_privateData = data;
			}
			break;
		}

	return *this;
}

SString &
SString::ReplaceLast(char ch1, char ch2)
{
	int32_t length = Length();
	for (int32_t index = length - 1; index >= 0; index--)
		if (_privateData[index] == ch1) {
			SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
			if (buf) {
				char *data = static_cast<char *>(buf->Data());
				data[index] = ch2;
				_privateData = data;
			}
			break;
		}

	return *this;
}

SString &
SString::ReplaceAll(char ch1, char ch2, int32_t fromOffset)
{
	char *data = NULL;
	int32_t length = Length();
	for (int32_t index = fromOffset; index < length; index++)
		if (_privateData[index] == ch1) {
			if (!data) {
				SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
				if (buf) {
					data = static_cast<char *>(buf->Data());
					_privateData = data;
				}
				else
					break;
			}
			data[index] = ch2;
		}

	return *this;
}

SString &
SString::Replace(char ch1, char ch2, int32_t maxReplaceCount, int32_t fromOffset)
{
	char *data = NULL;
	int32_t length = Length();
	for (int32_t count = 0, index = fromOffset;
		index < length && count < maxReplaceCount;
		index++)
		if (_privateData[index] == ch1) {
			if (!data) {
				SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
				if (buf) {
					data = static_cast<char *>(buf->Data());
					_privateData = data;
				}
				else
					break;
			}
			data[index] = ch2;
			++count;
		}
	return *this;
}

void
SString::_DoReplaceAt(int32_t offset, int32_t sourceLength, int32_t replaceLength,
	const char *withThis)
{
	ASSERT(replaceLength <= strlen(withThis));

	char *dest = 0;
	if (sourceLength < replaceLength) {
		dest = _OpenAtBy(offset, replaceLength - sourceLength);
		if (!dest)
			return;
	}
	else if (sourceLength > replaceLength) {
		dest = _ShrinkAtBy(offset, sourceLength - replaceLength);
		if (dest)
			dest += offset;
		else
			return;
	}
	else if (replaceLength != 0) {
		SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(Length() + 1);
		if (buf) {
			char *data = static_cast<char *>(buf->Data());
			dest = data + offset;
			_privateData = data;
		}
		else
			return;
	}

	if (replaceLength != 0) {
		ASSERT(dest);
		memcpy(dest, withThis, replaceLength);
	}
}

/*
	XXX: Looks like ReplaceFirst() can fail to work properly if withThis
	points to somewhere within the _privateData buffer.  That is, there's
	no check for self-replacement.
*/

SString &
SString::ReplaceFirst(const char *replaceThis, const char *withThis)
{
	if (replaceThis) {
		int32_t offset = FindFirst(replaceThis);
		if (offset >= 0) {
			_DoReplaceAt(offset, strlen(replaceThis),
				withThis ? strlen(withThis) : 0, withThis);
		}
	}
	return *this;
}

SString &
SString::ReplaceLast(const char *replaceThis, const char *withThis)
{
	if (replaceThis) {
		int32_t offset = FindLast(replaceThis);
		if (offset >= 0) {
			_DoReplaceAt(offset, strlen(replaceThis),
				withThis ? strlen(withThis) : 0, withThis);
		}
	}
	return *this;
}

SString &
SString::ReplaceAll(const char *replaceThis, const char *withThis, int32_t fromOffset)
{
	if (replaceThis) {
		int32_t length = Length();
		int32_t sourceLength = strlen(replaceThis);
		int32_t replaceLength = 0;
		if (withThis)
			replaceLength = strlen(withThis);

		for (int32_t offset = fromOffset; offset < length; offset += replaceLength) {
			offset = FindFirst(replaceThis, offset);
			if (offset < 0)
				break;

			_DoReplaceAt(offset, sourceLength, replaceLength, withThis);

			length += (replaceLength - sourceLength);
		}
	}
	return *this;
}

SString &
SString::Replace(const char *replaceThis, const char *withThis,
	int32_t maxReplaceCount, int32_t fromOffset)
{
	if (replaceThis) {
		int32_t length = Length();
		int32_t sourceLength = strlen(replaceThis);
		int32_t replaceLength = 0;
		if (withThis)
			replaceLength = strlen(withThis);

		for (int32_t count = 0, offset = fromOffset;
			offset < length && count < maxReplaceCount;
			offset += replaceLength, count++) {

			offset = FindFirst(replaceThis, offset);
			if (offset < 0)
				break;

			_DoReplaceAt(offset, sourceLength, replaceLength, withThis);

			length += (replaceLength - sourceLength);
		}
	}
	return *this;
}

SString &
SString::IReplaceFirst(char ch1, char ch2)
{
	int32_t length = Length();
	ch1 = UTF8SafeToLower(ch1);
	for (int32_t index = 0; index < length; index++)
		if (UTF8SafeToLower(_privateData[index]) == ch1) {
			SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
			if (buf) {
				char *data = static_cast<char *>(buf->Data());
				data[index] = ch2;
				_privateData = data;
			}
			break;
		}

	return *this;
}

SString &
SString::IReplaceLast(char ch1, char ch2)
{
	ch1 = UTF8SafeToLower(ch1);
	int32_t length = Length();
	for (int32_t index = length - 1; index >= 0; index--)
		if (UTF8SafeToLower(_privateData[index]) == ch1) {
			SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
			if (buf) {
				char *data = static_cast<char *>(buf->Data());
				data[index] = ch2;
				_privateData = data;
			}
			break;
		}

	return *this;
}

SString &
SString::IReplaceAll(char ch1, char ch2, int32_t fromOffset)
{
	ch1 = UTF8SafeToLower(ch1);
	char *data = NULL;
	int32_t length = Length();
	for (int32_t index = fromOffset; index < length; index++)
		if (UTF8SafeToLower(_privateData[index]) == ch1) {
			if (!data) {
				SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
				if (buf) {
					data = static_cast<char *>(buf->Data());
					_privateData = data;
				}
				else
					break;
			}
			data[index] = ch2;
		}

	return *this;
}

SString &
SString::IReplace(char ch1, char ch2 , int32_t maxReplaceCount,
	int32_t fromOffset)
{
	ch1 = UTF8SafeToLower(ch1);
	char *data = NULL;
	int32_t length = Length();
	for (int32_t count = 0, index = fromOffset;
		index < length && count < maxReplaceCount;
		index++)
		if (UTF8SafeToLower(_privateData[index]) == ch1) {
			if (!data) {
				SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
				if (buf) {
					data = static_cast<char *>(buf->Data());
					_privateData = data;
				}
				else
					break;
			}
			data[index] = ch2;
			++count;
		}
	return *this;
}

SString &
SString::IReplaceFirst(const char *replaceThis, const char *withThis)
{
	if (replaceThis) {
		int32_t offset = IFindFirst(replaceThis);
		if (offset >= 0) {
			_DoReplaceAt(offset, strlen(replaceThis),
				withThis ? strlen(withThis) : 0, withThis);
		}
	}
	return *this;
}

SString &
SString::IReplaceLast(const char *replaceThis, const char *withThis)
{
	if (replaceThis) {
		int32_t offset = IFindLast(replaceThis);
		if (offset >= 0) {
			_DoReplaceAt(offset, strlen(replaceThis),
				withThis ? strlen(withThis) : 0, withThis);
		}
	}
	return *this;
}

SString &
SString::IReplaceAll(const char *replaceThis, const char *withThis, int32_t fromOffset)
{
	if (replaceThis) {
		int32_t length = Length();
		int32_t sourceLength = strlen(replaceThis);
		int32_t replaceLength = 0;
		if (withThis)
			replaceLength = strlen(withThis);

		for (int32_t offset = fromOffset; offset < length; offset += replaceLength) {
			offset = IFindFirst(replaceThis, offset);
			if (offset < 0)
				break;

			_DoReplaceAt(offset, sourceLength, replaceLength, withThis);

			length += (replaceLength - sourceLength);
		}
	}
	return *this;
}

SString &
SString::IReplace(const char *replaceThis, const char *withThis,
	int32_t maxReplaceCount, int32_t fromOffset)
{
	if (replaceThis) {
		int32_t length = Length();
		int32_t sourceLength = strlen(replaceThis);
		int32_t replaceLength = 0;
		if (withThis)
			replaceLength = strlen(withThis);

		for (int32_t count = 0, offset = fromOffset;
			offset < length && count < maxReplaceCount;
			offset += replaceLength, count++) {

			offset = IFindFirst(replaceThis, offset);
			if (offset < 0)
				break;

			_DoReplaceAt(offset, sourceLength, replaceLength, withThis);

			length += (replaceLength - sourceLength);
		}
	}
	return *this;
}

SString &
SString::ReplaceSet(const char *setOfChars, char with)
{
	const CharSet chars(setOfChars);

	char *data = NULL;
	int32_t length = Length();
	for (int32_t index = 0; index < length; index++) {
		if (chars.Has(_privateData+index)) {
			if (!data) {
				SSharedBuffer *buf = SSharedBuffer::BufferFromData(_privateData)->Edit(length + 1);
				if (buf) {
					data = static_cast<char *>(buf->Data());
					_privateData = data;
				}
				else
					break;
			}
			data[index] = with;
		}
	}

	return *this;
}

SString &
SString::ReplaceSet(const char *setOfCharsToRemove, const char *with)
{
	// deal with trivial cases
	if (!with)
		return *this;

	int32_t patternLength = strlen(with);
	if (patternLength == 1)
		return ReplaceSet(setOfCharsToRemove, *with);
	else if (patternLength == 0)
		return RemoveSet(setOfCharsToRemove);

	const CharSet chars(setOfCharsToRemove);

	int32_t length = Length();
	int32_t count = 0;
	for (int32_t index = 0; index < length; index ++)
		if (chars.Has(_privateData+index))
			count++;
	if (count) {
		int32_t moveBy = count * (patternLength - 1);
		char *data = _GrowBy(moveBy);
		if (data) {
			data -= length;
			int32_t chunkEnd = length;
			for (int32_t index = length - 1; index >= 0; index--) {
				if (chars.Has(data+index)) {
					char *dstPtr = data + index + 1 + moveBy;
					memmove(dstPtr, data + index + 1, chunkEnd - (index + 1));
					memcpy(dstPtr - patternLength, with, patternLength);

					// printf("done sofar: %s \n", dstPtr - patternLength);
					chunkEnd = index;
					moveBy -= (patternLength - 1);
					length += (patternLength - 1);
					if (moveBy <= 0)
						break;
				}
			}
		}
	}

	return *this;
}

char *
SString::LockBuffer(int32_t maxLength)
{
	int32_t oldLength = Length();
	SSharedBuffer *buf = _Edit(maxLength);
	if (buf) {
		char *data = static_cast<char *>(buf->Data());
		if (oldLength == 0 && maxLength != 0)
			data[0] = '\0';
		_privateData = data;
		_SetUsingAsCString(true);
		return data;
	}
	return 0;
}

SString &
SString::UnlockBuffer(int32_t length)
{
	ASSERT(!_privateData || (*((uint32_t *)_privateData - 1) & 0x80000000) != 0);
	_SetUsingAsCString(false);

	if (length >= 0) {
		if (length) {
			int32_t lockedLength = Length();
			if (length > lockedLength)
				length = lockedLength;
		}
	} else {
		length = _privateData ? strlen(_privateData) : 0;
	}

	if (length) {
		SSharedBuffer *buf = _Edit(length);
		if (buf) {
			char *data = static_cast<char *>(buf->Data());
			data[length] = '\0';
			_privateData = data;
		}
	}
	else {
		MakeEmpty();
	}

	return *this;
}

SString &
SString::ToLower(int32_t first, int32_t length)
{
	int32_t len = Length();
	int32_t stop = first + length;
	if (stop > len) stop = len;
	SSharedBuffer *buf = _Edit(Length());
	if (buf) {
		char *data = static_cast<char *>(buf->Data());
		for (int32_t index = first; index < stop; index++)
			data[index] = UTF8SafeToLower(data[index]);
		_privateData = data;
	}

	return *this;
}

SString &
SString::ToUpper(int32_t first, int32_t length)
{
	int32_t len = Length();
	int32_t stop = first + length;
	if (stop > len) stop = len;
	SSharedBuffer *buf = _Edit(Length());
	if (buf) {
		char *data = static_cast<char *>(buf->Data());
		for (int32_t index = first; index < stop; index++)
			data[index] = UTF8SafeToUpper(data[index]);
		_privateData = data;
	}

	return *this;
}


SString &
SString::ToLower()
{
	return ToLower(0, Length());
}

SString &
SString::ToUpper()
{
	return ToUpper(0, Length());
}
SString &
SString::Capitalize()
{
	int32_t length = Length();
	if (length == 0)
		return *this;

	SSharedBuffer *buf = _Edit(length);
	if (buf) {
		char *data = static_cast<char *>(buf->Data());
		data[0] = UTF8SafeToUpper(data[0]);
		for (int32_t index = 1; index < length; index++)
			data[index] = UTF8SafeToLower(data[index]);
		_privateData = data;
	}

	return *this;
}

SString &
SString::CapitalizeEachWord()
{
	bool sawAlpha = false;
	int32_t length = Length();
	SSharedBuffer *buf = _Edit(length);
	if (buf) {
		char *data = static_cast<char *>(buf->Data());
		for (int32_t index = 0; index < length; index++)
			if (UTF8SafeIsAlpha(data[index])) {
				data[index] = sawAlpha ?
					UTF8SafeToLower(data[index]) : UTF8SafeToUpper(data[index]);
				sawAlpha = true;
			} else
				sawAlpha = false;
		_privateData = data;
	}
	return *this;
}

inline bool
_CharInSet(char ch, const char *set, int32_t setSize)
{
	for (int32_t index = 0; index < setSize; index++)
		if (ch == set[index])
			return true;
	return false;
}


SString &
SString::CharacterEscape(const char *original, const char *setOfCharsToEscape, char escapeWith)
{
	*this = "";

	int32_t setSize = 0;
	if (setOfCharsToEscape)
		setSize = strlen(setOfCharsToEscape);

	if (!setSize) {
		// trivial case
		*this = original;
		return *this;
	}

	const char *from = original;
	for (;;) {
		int32_t count = 0;
		while(from[count] && !_CharInSet(from[count], setOfCharsToEscape, setSize))
			count++;
		Append(from, count);
		if (!from[count])
			break;
		char tmp[3];
		tmp[0] = escapeWith;
		tmp[1] = from[count];
		tmp[2] = '\0';
		(*this) += tmp;
		from += count + 1;
	}
	return *this;
}

SString &
SString::CharacterEscape(const char *setOfCharsToEscape, char escapeWith)
{
	int32_t setSize = 0;
	if (setOfCharsToEscape)
		setSize = strlen(setOfCharsToEscape);

	if (!setSize)
		// trivial case
		return *this;

	bool hitEscapable = false;

	// find out if we even need to modify the string in the first place
	const char *string = String();
	int32_t count = Length();
	for (int32_t index = 0; index < count; index++)
		if (_CharInSet(string[index], setOfCharsToEscape, setSize)) {
			hitEscapable = true;
			break;
		}

	if (!hitEscapable)
		// nothing to escape, bail
		return *this;

	// ToDo:
	// optimize this to do an in-place escape
	SString tmp(*this);
	MakeEmpty();
	return CharacterEscape(tmp.String(), setOfCharsToEscape, escapeWith);
}

SString &
SString::CharacterDeescape(const char *original, char escapeChar)
{
	(*this) = "";

	// ToDo:
	// optimize this to not grow string for every char

	for (;*original; original++)
		if (*original != escapeChar)
			(*this) += *original;
		else {
			if (!*++original)
				break;
			(*this) += *original;
		}

	return *this;
}

SString &
SString::CharacterDeescape(char escapeChar)
{
	bool hitEscapeChar = false;
	const char *string = String();
	int32_t count = Length();
	for (int32_t index = 0; index < count; index++)
		if (string[index] == escapeChar) {
			hitEscapeChar = true;
			break;
		}

	if (!hitEscapeChar)
		return *this;

	SString tmp(*this);
	MakeEmpty();
	return CharacterDeescape(tmp.String(), escapeChar);
}


SString &
SString::Trim()
{
	if (Length() == 0)
		return *this;
	size_t len = Length();
	char * buf = LockBuffer(len);
	char * end = buf + len - 1;
	char * pos = buf;

	// Find the last non-space character
	while (end >= buf && isspace(*end)) end--;
	end++;
	if (end == buf) {
		// If the string is all spaces
		*buf = '\0';
		UnlockBuffer(0);
		return *this;
	}

	// Go forward, looking for the first non-space
	while (*pos && isspace(*pos)) pos++;

	len = end-pos;
	// Copy in the old string
	memmove(buf, pos, len);
	*(buf+len) = '\0';

	UnlockBuffer(len);
	return *this;
}


SString &
SString::Mush()
{
	if (Length() == 0)
		return *this;
	char* buf = LockBuffer(Length());
	char* begin = buf;
	char* pos = buf;
	bool lastSpace = false;
	while (*buf) {
		if (pos == begin && isspace(*buf)) {
			buf++;
			continue;
		}
		if (pos < buf) *pos = *buf;
		if (*pos == '\n' || *pos == '\r' || *pos == '\t')
			*pos = ' ';
		if (isspace(*buf)) {
			if (!lastSpace) {
				pos++;
				lastSpace = true;
			}
		} else {
			lastSpace = false;
			pos++;
		}
		buf++;
	}
	if (pos > begin && isspace(*(pos-1)))
		--pos;

	*pos = '\0';
	UnlockBuffer(pos-begin);
	return *this;
}

SString &
SString::StripWhitespace()
{
	char* buf = LockBuffer(Length());
	char* begin = buf;
	char* pos = buf;
	while (*buf) {
		if (isspace(*buf)) {
			buf++;
			continue;
		}
		*pos++ = *buf++;
	}
	*pos = '\0';
	UnlockBuffer(pos-begin);
	return *this;
}

bool
SString::IsOnlyWhitespace() const
{
	int32_t i, count = Length();
	for (i=0; i<count; i++) {
		if (!isspace(_privateData[i]))
			return false;
	}
	return true;
}

SString &
SString::operator<<(const char *str)
{
	*this += str;

	return *this;
}

SString &
SString::operator<<(const SString &string)
{
	*this += string;
	return *this;
}

SString &
SString::operator<<(char ch)
{
	*this += ch;

	return *this;
}

SString &
SString::operator<<(int num)
{
	int32_t length = Length();
	char *buffer = (LockBuffer(length + 64) + length);
	sprintf(buffer, "%d", num);
	UnlockBuffer(length + strlen(buffer));

	return *this;
}

SString &
SString::operator<<(unsigned int num)
{
	int32_t length = Length();
	char *buffer = (LockBuffer(Length() + 64) + length);
	sprintf(buffer, "%u", num);
	UnlockBuffer(length + strlen(buffer));

	return *this;
}

SString &
SString::operator<<(long num)
{
	int32_t length = Length();
	char *buffer = (LockBuffer(Length() + 64) + length);
	sprintf(buffer, "%ld", num);
	UnlockBuffer(length + strlen(buffer));

	return *this;
}

SString &
SString::operator<<(unsigned long num)
{
	int32_t length = Length();
	char *buffer = (LockBuffer(Length() + 64) + length);
	sprintf(buffer, "%lu", num);
	UnlockBuffer(length + strlen(buffer));

	return *this;
}

SString &
SString::operator<<(int64_t num)
{
	int32_t length = Length();
	char *buffer = (LockBuffer(Length() + 64) + length);
	sprintf(buffer, "%Ld", num);
	UnlockBuffer(length + strlen(buffer));

	return *this;
}

SString &
SString::operator<<(uint64_t num)
{
	int32_t length = Length();
	char *buffer = (LockBuffer(Length() + 64) + length);
	sprintf(buffer, "%Lu", num);
	UnlockBuffer(length + strlen(buffer));

	return *this;
}

SString &
SString::operator<<(float num)
{
	int32_t length = Length();
	char *buffer = (LockBuffer(Length() + 64) + length);
#if TARGET_HOST == TARGET_HOST_PALMOS
	sprintf(buffer, "%f", num);
#else
	sprintf(buffer, "%.2f", num);
#endif
	UnlockBuffer(length + strlen(buffer));

	return *this;
}

int
Compare(const SString &a, const SString &b)
{
	return a.Compare(b);
}

int
ICompare(const SString &a, const SString &b)
{
	return a.ICompare(b);
}

int
Compare(const SString *a, const SString *b)
{
	return a->Compare(*b);
}

int
ICompare(const SString *a, const SString *b)
{
	return a->ICompare(*b);
}

void
BMoveBefore(SString* to, SString* from, size_t count)
{
	memmove(to, from, sizeof(SString)*count);
}

void
BMoveAfter(SString* to, SString* from, size_t count)
{
	memmove(to, from, sizeof(SString)*count);
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SString& string)
{
#if SUPPORTS_TEXT_STREAM
	const int32_t N = string.Length();
	if (N > 0) io->Print(string.String(),N);
#else
	(void)string;
#endif
	return io;
}

/* Path manipulation */

inline bool path_equals_delimiter(char delimiter)
{
#if _SUPPORTS_UNIX_FILE_PATH
	return (delimiter == '/');
#elif _SUPPORTS_WINDOWS_FILE_PATH
	return (delimiter == '\\');
#else
	(void) delimiter;
	return false;
#endif
}

inline const char* path_delimiter()
{
#if _SUPPORTS_UNIX_FILE_PATH
	return "/";
#elif _SUPPORTS_WINDOWS_FILE_PATH
	return "\\";
#else
	return "";
#endif
}

inline const char* path_dot_delimeter()
{
#if _SUPPORTS_UNIX_FILE_PATH
	return "./";
#elif _SUPPORTS_WINDOWS_FILE_PATH
	return ".\\";
#else
	return "";
#endif
}

inline const char* path_dot_dot_delimeter()
{
#if _SUPPORTS_UNIX_FILE_PATH
	return "../";
#elif _SUPPORTS_WINDOWS_FILE_PATH
	return "..\\";
#else
	return "";
#endif
}

inline int path_delimiter_size()
{
#if _SUPPORTS_UNIX_FILE_PATH
	return 1;
#elif _SUPPORTS_WINDOWS_FILE_PATH
	return 1;
#else
	return 0;
#endif
}

inline bool path_valid_beginning(const char* path)
{
#if _SUPPORTS_UNIX_FILE_PATH
	return (path[0] == '/');
#elif _SUPPORTS_WINDOWS_FILE_PATH

	bool driveLetter =	(toupper(path[0]) >= 'A')		&&
						(toupper(path[0]) <= 'Z')		&&
						(path[1] && (path[1] == ':'))	&&
						(path[2] && (path[2] == '\\'));

	bool slash = (path[0] == '\\');

	return driveLetter || slash;
#else
	(void) path;
	return false;
#endif
}

inline int32_t path_root_length(const char* path)
{
#if _SUPPORTS_UNIX_FILE_PATH

	if (path_valid_beginning(path))
	{
		return 1;
	}

#elif _SUPPORTS_WINDOWS_FILE_PATH

	if (path_valid_beginning(path))
	{
		if ((toupper(path[0]) >= 'A') && (toupper(path[0]) <= 'Z'))
			return 3;
		else if ((path[0] == '\\') && (path[0] == '\\'))
			return 2;
		else
			return 1;
	}

#else
	(void) path;
#endif

	return 0;
}

inline int32_t path_root_size()
{
#if _SUPPORTS_UNIX_FILE_PATH
	return 1;
#elif _SUPPORTS_WINDOWS_FILE_PATH
	return 3;
#else
	return false;
#endif
}

int32_t path_normalize_root(SString& normalme)
{
#if _SUPPORTS_UNIX_FILE_PATH || _SUPPORTS_WINDOWS_FILE_PATH
	const char* p = normalme.String();
#else
	(void) normalme;
#endif

#if _SUPPORTS_UNIX_FILE_PATH

	if (p)
	{
		return 0;
	}

#elif _SUPPORTS_WINDOWS_FILE_PATH

	if (p)
	{
		if ((toupper(p[0]) >= 'A') && (toupper(p[0]) <= 'Z'))
		{
			if ((p[1] == ':'))
			{
				if ((p[2] == '\\'))
					return 2;
				else
					return 0;
			}
			else
			{
				return 0;
			}
		}
		else if (p[0] == '\\' && (p[1] && (p[1] == '\\')))
		{
			return 1;
		}
		else if (p[0] == '\\')
		{
			return 0;
		}
	}
#endif

	return B_ERROR;
}

status_t path_normalize(SString& normalme)
{
	//bout << "Normalizing: " << normalme << endl;
	
	int32_t base = path_normalize_root(normalme);

	if (base < 0) {
		return base;
	}

	size_t i = base;
	bool slash = false;

	while (normalme[i])
	{
		const char c0=normalme[i];
		if ((i == 0) || path_equals_delimiter(c0))
		{
			const char c1=normalme[i+1];
			if (c1)
			{
				if (c1 == '.')
				{
					const char c2=normalme[i+2];
					if (c2)
					{
						if (c2 == '.')
						{
							const char c3=normalme[i+3];
							if (!c3 || path_equals_delimiter(c3))
							{
								//bout << "Going back from " << i << ": " << normalme << endl;
								int32_t end=i;
								while (i > (size_t)base) {
									if(path_equals_delimiter(normalme[i-1])) break;
									--i;
								}
								normalme.Remove(i, end-i+(c3 ? 4:3));
								//bout << "Removed at " << i << ": " << normalme << endl;
								slash = false;
								if (i > (size_t)base) i--;
								continue;
							}
						}
						else if (path_equals_delimiter(c2))
						{
							normalme.Remove(i+1, 2);
						}
					}
					else
					{
						normalme.Remove(i+1, 1);
					}
				}
			}
			else
			{
				if (i != 0)
				{
					normalme.Remove(i, 1);
				}
			}
		}

		if (path_equals_delimiter(normalme[i]))
		{
			if (slash)
			{
				normalme.Remove(i, 1);
				continue;
			}
			slash = true;
		}
		else
			slash = false;

		i++;
	}
	
	// Trim trailing delimiter
	i = normalme.Length()-1;
	if (i > size_t(base) && path_equals_delimiter(normalme[i])) {
		normalme.Remove(i, 1);
	}
	
	//bout << "Result: " << normalme << endl;
	return 0;
}

const char *
SString::PathDelimiter()
{
	return path_delimiter();
}

status_t
SString::PathSetTo(const SString &dir, const SString &leaf, bool normalize)
{
	return _PathSetTo(dir.String(), dir.Length(),
		leaf.String(), leaf.Length(), normalize);
}

status_t
SString::PathSetTo(const char *dir, const char *leaf, bool normalize)
{
	int32_t dirLength;
	int32_t leafLength;

	dirLength = dir ? strlen(dir) : 0;
	leafLength = leaf ? strlen(leaf) : 0;
	return _PathSetTo(dir, dirLength, leaf, leafLength, normalize);
}


status_t
SString::_PathSetTo(const char *dir, int32_t dirLength,
	const char *leaf, int32_t leafLength, bool normalize)
{
	status_t	err = B_ERROR;
	char		*p;
	size_t		slashLength;
	size_t combinedLength = 0;
	SSharedBuffer *buf = NULL;

	if (leaf && path_equals_delimiter(leaf[0])) {
		// A leading '/' means to ignore the current path and
		// start at the root.
		dir = "";
		dirLength = 0;
	}

	if (!dir) {
		err = B_BAD_VALUE;
		goto error1;
	}

	slashLength = 0;
	if (dirLength != 0 && leafLength != 0) {
		if (!path_equals_delimiter((dir + (dirLength - 1))[0]))
			slashLength = path_delimiter_size();
	}

	/*
	 * Don't do an _Edit() here. this allows to pass our own _privateData
	 * as dir or leaf to PathSetTo().
	*/
	buf = SSharedBuffer::Alloc(dirLength + leafLength + slashLength + 1);
	if (!buf) {
		err = B_NO_MEMORY;
		goto error1;
	}
	p = static_cast<char *>(buf->Data());

	if (dirLength != 0) {
		memcpy(&p[0], dir, dirLength);
		memcpy(&p[dirLength], path_delimiter(), slashLength);
		combinedLength = dirLength + slashLength;
	}
	if (leafLength != 0) {
		memcpy(&p[combinedLength], leaf, leafLength);
		combinedLength += leafLength;
	}
	p[combinedLength] = '\0';

	SSharedBuffer::BufferFromData(_privateData)->DecUsers();
	_privateData = p;

	/*
	if the paths look ok, we're fine. otherwise, we have to
	'normalize' them.
	*/

	if (normalize) {
		err = path_normalize(*this);

		if (err != B_OK)
			return err;
	}

	return B_NO_ERROR;

error1:
//+ PRINT(("PATH.SETTO(%s, %s) FAILED\n", dir ? dir : NULL, leaf ? leaf : "NULL"));
	Truncate(0);
	return err;
}

status_t SString::PathNormalize()
{
	return path_normalize(*this);
}

const char *SString::PathLeaf(void) const
{
	int32_t offset = FindLast(path_delimiter());
	if (offset >= 0) {
		return _privateData + offset + 1;
	}
	else {
		// PathLeaf("foo.cpp") == "foo.cpp"
		return String();
	}
}

status_t
SString::PathGetParent(SString *parentPath) const
{
	status_t	err;
	int32_t		offset;

	if (!parentPath) {
		err = B_BAD_VALUE;
		goto error1;
	}

	offset = FindLast(path_delimiter());

	if (offset < 0) {
		err = B_ENTRY_NOT_FOUND;
		goto error1;
	}

	if ((path_root_length(String()) - offset - 1) == 0) {
		if (Length() <= path_root_size()) {
			err = B_ENTRY_NOT_FOUND;
			goto error1;
		}
		parentPath->SetTo(String(), path_root_length(String()));
		return B_NO_ERROR;
	}

	parentPath->SetTo(*this, offset);
	return B_NO_ERROR;

error1:
	parentPath->Truncate(0);
	return err;
}

status_t SString::PathAppend(const SString &leaf, bool normalize)
{
	return _PathSetTo(String(), Length(), leaf._privateData, leaf.Length(), normalize);
}

status_t SString::PathAppend(const char *leaf, bool normalize)
{
	int32_t leafLength = leaf ? strlen(leaf) : 0;
	return _PathSetTo(String(), Length(), leaf, leafLength, normalize);
}

static void get_branch_pos(const SString& str, int32_t* outStart, int32_t* outEnd)
{
	int32_t start = path_root_length(str.String());

	// Skip over leading '/' in path.
	int32_t p;
	while (true)
	{
		p = str.FindFirst(path_delimiter(), start);
		if (p != start) break;
		start = p+1;
	}

	*outStart = start;
	*outEnd = p < 0 ? str.Length() : p;
}

status_t SString::PathGetRoot(SString* branch) const
{
	if (branch == NULL) return B_BAD_VALUE;

	int32_t start, end;
	get_branch_pos(*this, &start, &end);

	branch->SetTo(String() + start, end - start);
	return B_NO_ERROR;
}

status_t SString::PathRemoveRoot(SString* branch)
{
	if (branch == NULL) return B_BAD_VALUE;

	int32_t start, end;
	get_branch_pos(*this, &start, &end);

	branch->SetTo(String() + start, end - start);

	// Remove the branch part.  The +1 is to get rid of
	// the '/' separator.
	Remove(0, end < Length() ? (end+1) : end);

	return B_NO_ERROR;
}

bool SString::IsNumber(int32_t base)  const
{
	const char *s = String();
	unsigned long acc;
	char c;
	unsigned long cutoff;
	int neg, any, cutlim;

	do
	{
		c = *s++;
	} while (isspace((unsigned char)c));

	if (c == '-')
	{
		neg = 1;
		c = *s++;
	}
	else
	{
		neg = 0;
		if (c == '+')
			c = *s++;
	}

	if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X'))
	{
		c = s[1];
		s += 2;
		base = 16;
	}

	if (base == 0)
		base = c == '0' ? 8 : 10;
	acc = any = 0;

	if (base < 2 || base > 36)
		return false;

	cutoff = neg ? (unsigned long)-(LONG_MIN + LONG_MAX) + LONG_MAX : LONG_MAX;
	cutlim = cutoff % base;
	cutoff /= base;
	for ( ; ; c = *s++)
	{
		if (c == 0)
			break;
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'A' && c <= 'Z')
			c -= 'A' - 10;
		else if (c >= 'a' && c <= 'z')
			c -= 'a' - 10;
		else if (isspace(c))
			continue;
		else {
			any = 0;
			break;
		}
		if (c >= base) {
			any = 0;
			break;
		}
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else
		{
			any = 1;
			acc *= base;
			acc += c;
		}
	}

	if (!any)
		return false;

	return true;
}

const int32_t B_BUFFERING_ALLOC_SIZE = 32;

void SString::BeginBuffering()
{
	SSharedBuffer* buf;
	buf = const_cast<SSharedBuffer*>(SSharedBuffer::BufferFromData(_privateData));
	buf = buf->BeginBuffering();
	if (buf) {
		char* data = static_cast<char*>(buf->Data());
		_privateData = data;
	}
}

void SString::EndBuffering()
{
	if (Length() > 0) {
		SSharedBuffer* buf = const_cast<SSharedBuffer*>(SSharedBuffer::BufferFromData(_privateData));
		buf = buf->EndBuffering();
		char* data = static_cast<char*>(buf->Data());
		_privateData = data;
	} else {
		MakeEmpty();
	}
}

SValue 
BArrayAsValue(const SString* from, size_t count)
{
	SValue result;
	for (size_t i = 0; i < count; i++) {
		result.JoinItem(SSimpleValue<int32_t>(i), SValue::String(*from));
		from++;
	}
	return result;
}

status_t 
BArrayConstruct(SString* to, const SValue& value, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		*to = value[SSimpleValue<int32_t>(i)].AsString();
		to++;
	}
	return B_OK;
}


#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
