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

#ifndef	_SUPPORT_TEXTSTREAM_INTERFACE_H
#define	_SUPPORT_TEXTSTREAM_INTERFACE_H

/*!	@file support/ITextStream.h
	@ingroup CoreSupportUtilities
	@brief Abstract interface for a formatted text output
	stream, with C++ iostream-like operators.
*/

#include <support/IInterface.h>

#include <sys/uio.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

class SValue;

/*-----------------------------------------------------------------*/

/*!	@addtogroup CoreSupportUtilities
	@{
*/

//!	Abstract interface for streaming out formatted text.
/*!	@todo This should probably be changed to not derive from
	IInterface, so it doesn't depend on the Binder.  It has
	never been clear how to marshal these across processes,
	anyway.
*/
class ITextOutput : public IInterface
{
public:

		// Note: binder remote interface not yet implemented.

		//!	Attributes that can be included in a LogV() call.
		struct log_info {
			int32_t		tag;
			int32_t		team;
			int32_t		thread;
			nsecs_t		time;
			int32_t		indent;
			int32_t		front : 1;
			int32_t		reserved : 30;
			SValue*		extra;
		};
		
		//!	Print the given text to the stream.  The current indentation
		//!	level, and possibly other formatting, will be applied to each line.
		/*!	If \a len is negative, all text up to the first NUL character will
			be printed.  Otherwise, up to \a len characters will be printed. */
		virtual	status_t				Print(	const char *debugText,
												ssize_t len = -1) = 0;
		
		//!	Adjust the current indentation level by \a delta amount.
		virtual void					MoveIndent(	int32_t delta) = 0;
		
		//!	Write a log line.
		/*!	Metadata about the text is supplied in \a info.  The text itself
			is supplied in the iovec, which is handled as one atomic unit.
			You can supply B_WRITE_END for \a flags to indicate the item
			being logged has ended. */
		virtual	status_t				LogV(	const log_info& info,
												const iovec *vector,
												ssize_t count,
												uint32_t flags = 0) = 0;

		//!	Request that any pending data be written.
		/*!	This does NOT block until the data has been written; rather, it
			just ensures that the data will be written at some point in
			the future.
		*/
		virtual	void					Flush() = 0;

		//!	Make sure all data in the stream is written to its
		//!	physical device.
		/*!	Returns B_OK if the data is safely stored away, else an error code.
		*/
		virtual	status_t				Sync() = 0;
};

/*-----------------------------------------------------------------*/

//!	Abstract interface for streaming in formatted text.
/*!	@todo This should probably be changed to not derive from
	IInterface, so it doesn't depend on the Binder.  It has
	never been clear how to marshal these across processes,
	anyway.
*/
class ITextInput : public IInterface
{
public:

		// Note: binder remote interface not yet implemented.

		virtual	ssize_t	ReadChar() = 0;
		virtual	ssize_t	ReadLineBuffer(char* buffer, size_t size) = 0;
		// AppendLineTo: reads a single line from the stream and appends
		//   it to *outString.
		virtual	ssize_t	AppendLineTo(SString* outString) = 0;
};

/*-----------------------------------------------------------------*/

/*!	@} */

/*!	@name ITextOutput Formatters and Operators
	@ingroup CoreSupportUtilities */
//@{

//!	Generic manipulator function for the output stream.
typedef const sptr<ITextOutput>& (*ITextOutputManipFunc)(const sptr<ITextOutput>&);

_IMPEXP_SUPPORT const sptr<ITextOutput>& endl(const sptr<ITextOutput>& io);
_IMPEXP_SUPPORT const sptr<ITextOutput>& indent(const sptr<ITextOutput>& io);
_IMPEXP_SUPPORT const sptr<ITextOutput>& dedent(const sptr<ITextOutput>& io);

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const char* str);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, char);		//!< writes raw character
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, bool);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, int);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, unsigned int);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, unsigned long);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, long);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, uint64_t);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, int64_t);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, float);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, double);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, ITextOutputManipFunc func);
_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const void*);

template <class TYPE>
inline const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const sptr<TYPE>& a)
{
	return io << a.ptr();
}

template <class TYPE>
inline const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const wptr<TYPE>& a)
{
	return io << a.unsafe_ptr_access();
}

/*-----------------------------------------------------------------*/

inline const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, ITextOutputManipFunc func)
{
	return (*func)(io);
}

/*-----------------------------------------------------------------*/

//!	Type code container for formatting to an ITextOutput.
class STypeCode 
{
public:
	STypeCode(type_code type);
	~STypeCode();

	type_code TypeCode() const;
	
private:
	type_code fType;
	int32_t _reserved;
};

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const STypeCode& type);

/*-----------------------------------------------------------------*/

//!	Time duration container for formatting to an ITextOutput.
class SDuration
{
public:
	SDuration(nsecs_t d);
	~SDuration();

	nsecs_t Duration() const;

private:
	nsecs_t m_duration;
};

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SDuration& type);

/*-----------------------------------------------------------------*/

//!	Bytes size_t container for formatting to an ITextOutput.
class SSize
{
public:
	SSize(size_t s);
	~SSize();

	size_t Size() const;

private:
	size_t m_size;
};

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SSize& type);

/*-----------------------------------------------------------------*/
/*------- Formatting status_t codes -------------------------------*/

class SStatus
{
public:
	SStatus(status_t s);
	~SStatus();

	status_t Status() const;
	SString AsString() const;

private:
	status_t m_status;
};

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SStatus& type);

/*-----------------------------------------------------------------*/

//!	Utility to perform hex data dump to an ITextOutput.
class SHexDump
{
public:
	SHexDump(const void *buf, size_t length, size_t bytesPerLine=16);
	~SHexDump();
	
	SHexDump& SetBytesPerLine(size_t bytesPerLine);
	SHexDump& SetSingleLineCutoff(int32_t bytes);
	SHexDump& SetAlignment(size_t alignment);
	SHexDump& SetCArrayStyle(bool enabled);
	
	const void* Buffer() const;
	size_t Length() const;
	size_t BytesPerLine() const;
	int32_t SingleLineCutoff() const;
	size_t Alignment() const;
	bool CArrayStyle() const;

private:
	const void* fBuffer;
	size_t fLength;
	size_t fBytesPerLine;
	int32_t fSingleLineCutoff;
	size_t fAlignment;
	bool fCArrayStyle;
};

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SHexDump& buffer);

/*-----------------------------------------------------------------*/

//!	Utility to perform printf()-like formatting to an ITextOutput.
class SPrintf
{
public:
	SPrintf(const char *fmt, ...);
	~SPrintf();
	const char* Buffer() const;
private:
	char* m_buffer;
	uint32_t _reserved[3];
};

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SPrintf& data);

/*-----------------------------------------------------------------*/

//!	Float container for formatting as raw bytes to an ITextOutput.
class SFloatDump
{
public:
	SFloatDump(float f);
	const float& Float() const { return f_; }
private:
	float f_;
};

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SFloatDump& buffer);

/*-----------------------------------------------------------------*/
/*------- Formatting multibyte character constants ----------------*/
class SCharCode
{
public:
			SCharCode(uint32_t c);
	uint32_t	val;
};

inline SCharCode::SCharCode(uint32_t c)
	:val(c)
{
}

inline const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SCharCode &v)
{
	const char *n = (const char *)(&(v.val));
	char str[5];
#if defined(B_HOST_IS_LENDIAN) && B_HOST_IS_LENDIAN
	str[0] = n[3];
	str[1] = n[2];
	str[2] = n[1];
	str[3] = n[0];
#else
	str[0] = n[0];
	str[1] = n[1];
	str[2] = n[2];
	str[3] = n[3];
#endif
	str[4] = 0;
	return io << str; 
}

/*-----------------------------------------------------------------*/

//@}

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_TEXTSTREAM_INTERFACE_H */
