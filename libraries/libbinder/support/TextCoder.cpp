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

#include <support/Value.h>  // for some reason ADS wants this
#include <support/TextCoder.h>
#include <support/SharedBuffer.h>

#include <TextMgrPrv.h>		// TxtDeviceToUTF32Lengths
#include <ErrorMgr.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

// ----------------------------------------------------------------------

CharEncodingType
STextCoderUtils::gDeviceEncoding = charEncodingUnknown;

CharEncodingType
STextCoderUtils::DeviceEncoding()
{
	if (gDeviceEncoding == charEncodingUnknown)
		gDeviceEncoding = LmGetSystemLocale(NULL);
	return gDeviceEncoding;
}

// ----------------------------------------------------------------------

STextDecoder::STextDecoder()
	: m_text(""), m_consumed(0), m_length(0), m_useInternalBuffer(true)
{
	m_buffer[0] = '\0';
}

STextDecoder::~STextDecoder()
{
}

static const wchar32_t kByteMask = 0x000000BF;
static const wchar32_t kByteMark = 0x00000080; 

// Surrogates aren't valid for UTF-32 characters, so define some
// constants that will let us screen them out.
static const wchar32_t	kUnicodeSurrogateHighStart	= 0x0000D800;
static const wchar32_t	kUnicodeSurrogateHighEnd	= 0x0000DBFF;
static const wchar32_t	kUnicodeSurrogateLowStart	= 0x0000DC00;
static const wchar32_t	kUnicodeSurrogateLowEnd		= 0x0000DFFF;
static const wchar32_t	kUnicodeSurrogateStart		= kUnicodeSurrogateHighStart;
static const wchar32_t	kUnicodeSurrogateEnd		= kUnicodeSurrogateLowEnd;

// Mask used to set appropriate bits in first byte of UTF-8 sequence,
// indexed by number of bytes in the sequence.
static const wchar32_t kFirstByteMark[] = { 0x00000000, 0x00000000, 0x000000C0, 0x000000E0, 0x000000F0 };

status_t
STextDecoder::DeviceToUTF8(char const *text, size_t srcLen,
		char const * substitutionStr, size_t substitutionLen)
{
	m_text = "";
	m_useInternalBuffer = true;

	size_t srcPos = 0, dstPos = 0, dstAvail = kInternalBufferSize;
	char* buf = m_buffer;
	while (srcPos < srcLen)
	{
		// convert from device encoding to UTF32.
		// For high ASCII remapping support on double-byte systems, we
		// rely on TxtDeviceToUTF32Lengths being called here, so if this
		// code is ever fixed to use TxtConvertEncoding then the new
		// high ASCII flag will have to be explicitly passed to TCE().
		wchar32_t codepoints[kInternalBufferSize];
		uint8_t lengths[kInternalBufferSize];
		ssize_t result = TxtDeviceToUTF32Lengths(((const uint8_t *)text)+srcPos, srcLen - srcPos, codepoints, lengths, kInternalBufferSize);
		if (result <= 0) {
			codepoints[0] = 0xfffd;
			lengths[0] = 1;
			result = 1;
		}

		// convert each UTF32 character into UTF8, writing into string.
		ssize_t offset=0;
		do
		{
			wchar32_t srcChar = codepoints[offset];

			size_t bytesToWrite;

			// Figure out how many bytes the result will require.
			// Note that for performance reasons we don't try to
			// identify invalid characters.  We assume that
			/// TxtDeviceToUTF32Lengths() won't be annoying enough
			// to generate such things.
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
				bytesToWrite = 3;
			}
			// Max code point for Unicode is 0x0010FFFF.
			else if (srcChar < 0x00110000)
			{
				bytesToWrite = 4;
			}
			else
			{
				// Invalid UTF-32 character.
				bytesToWrite = 0;
			}
			
			dstPos += bytesToWrite;

			if (dstPos >= dstAvail) {
				// Out of room, need to grow buffer.
				size_t newSize = ((dstAvail*3)/2)&~0x7;
				if (m_useInternalBuffer) {
					m_useInternalBuffer = false;
					char* newBuf = m_text.LockBuffer(newSize);
					memcpy(newBuf, buf, dstAvail);
					buf = newBuf;
					dstAvail = newSize;
				} else {
					m_text.UnlockBuffer(dstAvail);
					dstAvail = ((dstAvail*3)/2)&~0x7;
					buf = m_text.LockBuffer(dstAvail);
					if (buf == NULL) {
						m_consumed = 0;
						m_length = 0;
						m_useInternalBuffer = true;
						return B_NO_MEMORY;
					}
				}
			}

			char* p = buf+dstPos;
			switch (bytesToWrite)
			{	/* note: everything falls through. */
				case 4:	*--p = (uint8_t)((srcChar | kByteMark) & kByteMask); srcChar >>= 6;
				case 3:	*--p = (uint8_t)((srcChar | kByteMark) & kByteMask); srcChar >>= 6;
				case 2:	*--p = (uint8_t)((srcChar | kByteMark) & kByteMask); srcChar >>= 6;
				case 1:	*--p = (uint8_t)(srcChar | kFirstByteMark[bytesToWrite]);
			}

			srcPos += lengths[offset++];
		} while (offset < result);
	}

	m_consumed = srcPos;
	m_length = dstPos;
	if (m_useInternalBuffer)
	{
		buf[dstPos] = 0;
	}
	else
	{
		m_text.UnlockBuffer(dstPos);
	}

	return errNone;
}

status_t
STextDecoder::DeviceToUTF8(wchar32_t srcChar,
		char const * substitutionStr, size_t substitutionLen)
{
	char text[maxCharBytes]; // from TextMgr.h, maximum octets in a wchar
	size_t srcLen = TxtSetNextChar(text, 0, srcChar);
	return DeviceToUTF8(text, srcLen, 
			substitutionStr, substitutionLen);
}

status_t
STextDecoder::EncodingToUTF8(const SValue & text,
		char const * substitutionStr, size_t substitutionLen)
{
	if (text.Type() == B_ENCODED_TEXT_TYPE) {
		CharEncodingType encoding = 
			((STextEncoder::encoding_info_block*)text.Data())->encoding;
		char const * textBuf = 
			(char const *)text.Data() + sizeof(STextEncoder::encoding_info_block);
		int16_t srcLen = text.Length() - sizeof(STextEncoder::encoding_info_block);
		return EncodingToUTF8(textBuf, srcLen, encoding,
			substitutionStr, substitutionLen);
	}
	return B_BAD_VALUE;
}

status_t
STextDecoder::EncodingToUTF8(
		char const *text, size_t srcLen, CharEncodingType fromEncoding,
		char const * substitutionStr, size_t substitutionLen)
{
	m_text = "";
	m_consumed = 0;
	m_length = 0;

	// First pre-flight the conversion.
	// FUTURE - if it was really important to minimize calls to
	// TxtConvertEncoding, then we'd want to (a) bump the default
	// buffer size a bit, to say 60 bytes, and (b) try the conversion
	// first, then re-convert what's remaining.

	size_t srcLength = srcLen;
	size_t dstLength;
	status_t result = 
		TxtConvertEncoding(true,
			NULL,
			text, &srcLength, fromEncoding,
			NULL, &dstLength, charEncodingUTF8,
			substitutionStr,
			substitutionLen);
	if (result != errNone) {
		DbgOnlyFatalError("STextDecoder: conversion pre-flight");
		return result; // bail out
	}


	m_length = dstLength;
	size_t bufferSize = m_length + 1; // Need terminating null byte.
	m_useInternalBuffer = bufferSize <= kInternalBufferSize;
	char * buf;
	if (m_useInternalBuffer) {
		buf = m_buffer;
	} else {
		buf = m_text.LockBuffer(bufferSize);
	}

	if (dstLength < substitutionLen) {
		// TxtConvertEncoding has an assertion that the destination length
		// is at least as large as the substitution length.  I think this
		// is kind-of bogus, but we need to deal with it here.
		// Note that we are lying about how large the destination buffer is,
		// which is horrible, but we know the real size from the pre-flight
		// so this will only cause memory corruption if TxtConvertEncoding
		// is seriously broken.
		dstLength = substitutionLen;
	}

	// Now really do the conversion.
	srcLength = srcLen;
	result = 
		TxtConvertEncoding(true,
			NULL,
			text, &srcLength, fromEncoding,
			buf,  &dstLength, charEncodingUTF8,
			substitutionStr,
			substitutionLen);
	if (result != errNone || 
			(result == txtErrNoCharMapping && substitutionStr == NULL)) 
	{
		DbgOnlyFatalError("STextDecoder: conversion pre-flight was OK, but actual conversion failed");
		return result;
	}

	DbgOnlyFatalErrorIf(dstLength != m_length, "STextDecoder: different length after actual conversion");

	// Make sure the string is NULL terminated, as the asString method requires this.
	buf[m_length] = '\0';

	m_consumed = srcLength;

	if (!m_useInternalBuffer) {
		m_text.UnlockBuffer(bufferSize);
	}

	return errNone;
}

char const *
STextDecoder::Buffer() const
{
	if (m_useInternalBuffer) {
		return m_buffer;
	} else {
		return m_text.String();
	}
}


SString
STextDecoder::AsString() const
{
	if (m_useInternalBuffer) {
		return SString(m_buffer);
	} else {
		return m_text;
	}
}

// ----------------------------------------------------------------------

STextEncoder::STextEncoder()
	: m_sharedBuffer(NULL), m_consumed(0), m_length(0), m_encoding(charEncodingUnknown)
{
	m_buffer[0] = '\0';
}

STextEncoder::~STextEncoder()
{
	if (m_sharedBuffer != NULL) {
		m_sharedBuffer->DecUsers();
	}
}

status_t
STextEncoder::UTF8ToDevice(
		const SString & text, 
		char const * substitutionStr,
		size_t substitutionLen)
{
	return UTF8ToEncoding(text, 
			STextCoderUtils::DeviceEncoding(), 
			substitutionStr, substitutionLen);
}

status_t
STextEncoder::UTF8ToEncoding(
		const SString & _text, 
		CharEncodingType toEncoding, 
		char const * substitutionStr,
		size_t substitutionLen)

{
	// Reset things to a known state.
	m_consumed = 0;
	m_length = 0;
	if (m_sharedBuffer != NULL) {
		m_sharedBuffer->DecUsers();
		m_sharedBuffer = NULL;
	}

	// First pre-flight the conversion.
	// FUTURE - if it was really important to minimize calls to
	// TxtConvertEncoding, then we'd want to (a) bump the default
	// buffer size a bit, to say 60 bytes, and (b) try the conversion
	// first, then re-convert what's remaining.
	char const * text = _text.String();
	size_t srcLength = _text.Length();
	size_t dstLength;
	status_t result = 
		TxtConvertEncoding(true,
			NULL,
			text, &srcLength, charEncodingUTF8,
			NULL, &dstLength, toEncoding,
			substitutionStr,
			substitutionLen);
	if (result != errNone) {
		DbgOnlyFatalError("STextEncoder: conversion pre-flight");
		return result; // bail out
	}

	m_length = (size_t)dstLength;
	// Need to include space for terminating NULL byte.
	size_t bufferSize = m_length + 1;
	char * buf;
	if ((bufferSize + sizeof(encoding_info_block)) > kInternalBufferSize) {
		m_sharedBuffer = SSharedBuffer::Alloc(bufferSize + sizeof(encoding_info_block));
		buf = (char *)m_sharedBuffer->Data();
	} else {
		buf = m_buffer;
	}

	// We store the encoding in an encoding_info_block at the beginning
	// of the SValue data.  This way a B_ENCODED_TEXT_VALUE can be
	// converted to UTF8 at any time without any additional encoding
	// information---it's all stored in-band.

	((encoding_info_block*)buf)->encoding = toEncoding;
	buf += sizeof(encoding_info_block);

	// Now really do the conversion.
	srcLength = _text.Length();
	result = 
		TxtConvertEncoding(true,
			NULL,
			text, &srcLength, charEncodingUTF8,
			buf,  &dstLength, toEncoding,
			substitutionStr,
			substitutionLen);
	if (result != errNone || 
			(result == txtErrNoCharMapping && substitutionStr == NULL)) 
	{
		DbgOnlyFatalError("STextEncoder: conversion pre-flight was OK, but actual conversion failed (or required substitution but none was provided)");
		return result;
	}
	DbgOnlyFatalErrorIf(dstLength != m_length, "STextEncoder: different resulting length");

	// Better null-terminate the string.
	buf[m_length] = '\0';

	m_consumed = srcLength;

	return errNone;
}

char const *
STextEncoder::Buffer() const
{
	char const * ptr;
	if (m_sharedBuffer != NULL) {
		ptr = (char const*)m_sharedBuffer->Data();
	} else {
		ptr = (char const *)m_buffer;
	}
	return (char const *)(ptr + sizeof(encoding_info_block));
}

CharEncodingType
STextEncoder::Encoding() const
{
	encoding_info_block * ptr;
	if (m_sharedBuffer != NULL) {
		ptr = (encoding_info_block*)m_sharedBuffer->Data();
	} else {
		ptr = (encoding_info_block*)m_buffer;
	}
	return ptr->encoding;
}

SValue
STextEncoder::AsValue() const
{
	if (m_sharedBuffer != NULL) {
		return SValue(B_ENCODED_TEXT_TYPE, m_sharedBuffer);
	} else {
		return SValue(B_ENCODED_TEXT_TYPE, m_buffer, m_length);
	}
}

// -fin-
