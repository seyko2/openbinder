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

#ifndef _PDK__SUPPORT_TEXTCODER__H_
#define _PDK__SUPPORT_TEXTCODER__H_

/*!	@file support/TextCoder.h
	@ingroup CoreSupportUtilities
	@brief Helper classes for converting to and from native
	UTF-8 text encoding.
*/

#include <support/SupportDefs.h>
#include <support/String.h>

#include <LocaleMgr.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class STextCoderUtils {
private:
	static CharEncodingType		gDeviceEncoding;
	static CharEncodingType		DeviceEncoding();

	friend class STextDecoder;
	friend class STextEncoder;
};

class STextDecoder {
public:
								STextDecoder();
								~STextDecoder();

	status_t					DeviceToUTF8(
									char const *text, size_t srcLen,
									char const * substitutionStr = "",
									size_t substitutionLen = 0);
	status_t					DeviceToUTF8(
									wchar32_t srcChar,
									char const * substitutionStr = "",
									size_t substitutionLen = 0);
	status_t					EncodingToUTF8(
									char const *text, 
									size_t srcLen, 
									CharEncodingType fromEncoding,
									char const * substitutionStr = "",
									size_t substitutionLen = 0);
	status_t					EncodingToUTF8(const SValue & text,
									char const * substitutionStr = "",
									size_t substitutionLen = 0);

	SString						AsString() const;
	char const * 				Buffer() const;
	inline size_t 				Size() const
									{ return m_length; }
	inline size_t 				ConsumedBytes() const
									{ return m_consumed; }
private:
								STextDecoder(const STextDecoder&);
	
	static size_t const 		kInternalBufferSize = 60;

 	SString  					m_text;
	size_t						m_consumed;
	size_t						m_length;

	bool						m_useInternalBuffer;
	char						m_buffer[kInternalBufferSize];
};

class STextEncoder {
public:
								STextEncoder();
								~STextEncoder();

	// Note: the default substitution string is not NULL here, so by
	// default TxtConvertEncoding will skip over untranslatable
	// characters and convert the entire string.  To force the
	// conversion to abort on an untranslatable character, pass NULL for
	// substitutionStr.
	status_t					UTF8ToDevice(
									const SString & text, 
									char const * substitutionStr = "",
									size_t substitutionLen = 0);
	status_t					UTF8ToEncoding(
									const SString & text, 
									CharEncodingType toEncoding, 
									char const * substitutionStr = "",
									size_t substitutionLen = 0);

	SValue						AsValue() const;
	char const * 				Buffer() const;
	CharEncodingType			Encoding() const;
	inline size_t 				Size() const
									{ return m_length; }
	inline size_t 				ConsumedBytes() const
									{ return m_consumed; }
private:
								STextEncoder(const STextEncoder&);

	struct encoding_info_block {
		CharEncodingType 		encoding;
		uint16_t				_unused;
	};
	friend class STextDecoder;	// STextEncoder needs to know about 
	                          	// encoding_info_block

	static size_t const 		kInternalBufferSize = 60;

 	SSharedBuffer *				m_sharedBuffer;
	size_t						m_consumed;
	size_t						m_length;
	CharEncodingType			m_encoding;

	char						m_buffer[kInternalBufferSize];
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namepsace palmos::support
#endif

#endif //_PDK__SUPPORT_TEXTCODER__H_
