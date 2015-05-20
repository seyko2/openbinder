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

#ifndef _SUPPORT_STRING_H
#define _SUPPORT_STRING_H

/*!	@file support/String.h
	@ingroup CoreSupportUtilities
	@brief Unicode (UTF-8) string class.
*/

#include <support/SupportDefs.h>
#include <support/ITextStream.h>
#include <support/Value.h>

#include <string.h>
#include <PalmTypes.h>				// wchar32_t

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

class SSharedBuffer;

// These are #defines so they can be concatenated with raw strings.
#define B_UTF8_ELLIPSIS				"\xE2\x80\xA6"
#define B_UTF8_BULLET				"\xE2\x80\xA2"
#define B_UTF8_SINGLE_OPEN_QUOTE	"\xE2\x80\x98"
#define B_UTF8_SINGLE_CLOSE_QUOTE	"\xE2\x80\x99"
#define B_UTF8_DOUBLE_OPEN_QUOTE	"\xE2\x80\x9C"
#define B_UTF8_DOUBLE_CLOSE_QUOTE	"\xE2\x80\x9D"
#define B_UTF8_COPYRIGHT			"\xC2\xA9"
#define B_UTF8_REGISTERED			"\xC2\xAE"
#define B_UTF8_TRADEMARK			"\xE2\x84\xA2"
#define B_UTF8_SMILING_FACE_WHITE	"\xE2\x98\xBA"
#define B_UTF8_SMILING_FACE_BLACK	"\xE2\x98\xBB"
#define B_UTF8_HIROSHI				"\xE5\xBC\x98"

enum {
		B_UTF32_ELLIPSIS			= 0x2026,
		B_UTF32_BULLET				= 0x2022,
		B_UTF32_SINGLE_OPEN_QUOTE	= 0x2018,
		B_UTF32_SINGLE_CLOSE_QUOTE	= 0x2019,
		B_UTF32_DOUBLE_OPEN_QUOTE	= 0x201C,
		B_UTF32_DOUBLE_CLOSE_QUOTE	= 0x201D,
		B_UTF32_COPYRIGHT			= 0x00A9,
		B_UTF32_REGISTERED			= 0x00AE,
		B_UTF32_TRADEMARK			= 0x2122,
		B_UTF32_SMILING_FACE_WHITE	= 0x263A,
		B_UTF32_SMILING_FACE_BLACK	= 0x263B
};

//!	UTF8 string container.
/*!	This class represents a UTF-8 string.  It includes a variety
	of common string operations as methods (in fact as way too
	many methods); additional less common operations are available
	in the StringUtils.h header.

	SString uses copy-on-write for its internal data buffer, so
	it is efficient to copy (and usually to convert back and
	forth with SValue, which shares the same copy-on-write
	mechanism).

	@nosubgrouping
*/
class SString
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction. */
	//@{
						SString();
	explicit			SString(const char *);
	explicit			SString(int32_t);
						SString(const SString &);
						SString(const char *, int32_t maxLength);
						SString(const SSharedBuffer *buf);
					
						~SString();

			//!	Run internal QC tests.  Only available on debug builds.
	static	status_t	SelfQC();

	//@}

	// --------------------------------------------------------------
	/*!	@name Access
		Retrieving the string and information about it. */
	//@{

			//!	Return null-terminated C string.
	const char 			*String() const;

			//! Automagic cast to a C string.
			/*!	@todo This should be removed! */
						operator const char *() const { return String(); }

			//!	Retrieve string length (does not require a strlen()).
	int32_t 			Length() const;

			//! Remove all data from string.
	void				MakeEmpty();

			//!	Retrieve empty SString constant.
			/*!	Normally you would want to use the default 
				SString constructor, but this is handy for
				things like default parameters, or when a
				const SString& is desired, and you don't
				have one to provide. */
	static	const SString&	EmptyString();

			//!	Unchecked character access.
	char 				operator[](size_t index) const;

			//!	Checked (safely bounded) character access.
	char 				ByteAt(int32_t index) const;

			//! Checks the string for a valid representation of a number in the base.
	bool				IsNumber(int32_t base = 10) const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Assignment
		Replacing the string with something entirely different. */
	//@{

	SString 			&operator=(const SString &);
	SString 			&operator=(const char *);
	
	SString				&SetTo(const char *);
	SString 			&SetTo(const char *, int32_t length);

	SString				&SetTo(const SString &from);
						
	SString 			&SetTo(const SString &, int32_t length);
		
	SString 			&SetTo(char, int32_t count);

			//!	Set string using a printf-style format.
	SString				&Printf(const char *fmt, ...);

	void				Swap(SString& with);

	//@}

	// --------------------------------------------------------------
	/*!	@name Substring Copying
		Copying a portion of the string to somewhere else. */
	//@{

			//!	Returns @a into ref as it's result.
			/*!	Doesn't do anything if @a into is @a this. */
	SString 			&CopyInto(	SString &into, int32_t fromOffset,
									int32_t length) const;

			//!	Caller guarantees that @a into is large enough.
	void				CopyInto(	char *into, int32_t fromOffset,
									int32_t length) const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Appending
		Adding to the end of the string. */
	//@{

	SString 			&operator+=(const SString &);
	SString 			&operator+=(const char *);
	SString 			&operator+=(char);
	
	SString 			&Append(const SString &);
	SString 			&Append(const char *);
	SString 			&Append(const SString &, int32_t length);
	SString 			&Append(const char *, int32_t length);
	SString 			&Append(char, int32_t count);

	//@}

	// --------------------------------------------------------------
	/*!	@name Prepending
		Adding to the front of the string. */
	//@{

	SString 			&Prepend(const char *);
	SString 			&Prepend(const SString &);
	SString 			&Prepend(const char *, int32_t);
	SString 			&Prepend(const SString &, int32_t);
	SString 			&Prepend(char, int32_t count);

	//@}

	// --------------------------------------------------------------
	/*!	@name Inserting
		Adding to the middle of the string. */
	//@{

	SString 			&Insert(const char *, int32_t pos);
	SString 			&Insert(const char *, int32_t length, int32_t pos);
	SString 			&Insert(const char *, int32_t fromOffset,
								int32_t length, int32_t pos);

	SString 			&Insert(const SString &, int32_t pos);
	SString 			&Insert(const SString &, int32_t length, int32_t pos);
	SString 			&Insert(const SString &, int32_t fromOffset,
								int32_t length, int32_t pos);
	SString 			&Insert(char, int32_t count, int32_t pos);

	//@}

	// --------------------------------------------------------------
	/*!	@name Removing
		Deleting characters from the string. */
	//@{

	SString 			&Truncate(int32_t newLength);
						
	SString 			&Remove(int32_t from, int32_t length);

	SString 			&RemoveFirst(const SString &);
	SString 			&RemoveLast(const SString &);
	SString 			&RemoveAll(const SString &);

	SString 			&RemoveFirst(const char *);
	SString 			&RemoveLast(const char *);
	SString 			&RemoveAll(const char *);
	
	SString 			&RemoveSet(const char *setOfCharsToRemove);
	
	SString				&Compact(const char *setOfCharsToCompact);
	SString				&Compact(const char *setOfCharsToCompact,
								 char* replacementChar);
	
	SString 			&MoveInto(SString &into, int32_t from, int32_t length);
			//! Caller must guarantee that @a into is large enough.
	void				MoveInto(char *into, int32_t from, int32_t length);

	//@}

	// --------------------------------------------------------------
	/*!	@name Comparison
		Standard C++ comparion operators. */
	//@{

	bool 				operator<(const SString &) const;
	bool 				operator<=(const SString &) const;
	bool 				operator==(const SString &) const;
	bool 				operator>=(const SString &) const;
	bool 				operator>(const SString &) const;
	bool 				operator!=(const SString &) const;
	
	bool 				operator<(const char *) const;
	bool 				operator<=(const char *) const;
	bool 				operator==(const char *) const;
	bool 				operator>=(const char *) const;
	bool 				operator>(const char *) const;
	bool 				operator!=(const char *) const;

	//@}

	// --------------------------------------------------------------
	/*!	@name strcmp()-Style Comparison
		Comparison methods with strcmp()-like results. */
	//@{

	int 				Compare(const SString &) const;
	int 				Compare(const char *) const;
	int 				Compare(const SString &, int32_t n) const;
	int 				Compare(const char *, int32_t n) const;
	int 				ICompare(const SString &) const;
	int 				ICompare(const char *) const;
	int 				ICompare(const SString &, int32_t n) const;
	int 				ICompare(const char *, int32_t n) const;

	//@}

	// --------------------------------------------------------------
	/*!	@name Searching
		Finding characters inside the string. */
	//@{

	int32_t 			FindFirst(const SString &) const;
	int32_t 			FindFirst(const char *) const;
	int32_t 			FindFirst(const SString &, int32_t fromOffset) const;
	int32_t 			FindFirst(const char *, int32_t fromOffset) const;
	int32_t				FindFirst(char) const;
	int32_t				FindFirst(char, int32_t fromOffset) const;

	int32_t 			FindLast(const SString &) const;
	int32_t 			FindLast(const char *) const;
	int32_t 			FindLast(const SString &, int32_t beforeOffset) const;
	int32_t 			FindLast(const char *, int32_t beforeOffset) const;
	int32_t				FindLast(char) const;
	int32_t				FindLast(char, int32_t fromOffset) const;

	int32_t 			IFindFirst(const SString &) const;
	int32_t 			IFindFirst(const char *) const;
	int32_t 			IFindFirst(const SString &, int32_t fromOffset) const;
	int32_t 			IFindFirst(const char *, int32_t fromOffset) const;

	int32_t 			IFindLast(const SString &) const;
	int32_t 			IFindLast(const char *) const;
	int32_t 			IFindLast(const SString &, int32_t beforeOffset) const;
	int32_t 			IFindLast(const char *, int32_t beforeOffset) const;

	int32_t				FindSetFirst(const char *setOfChars);
	int32_t				FindSetFirst(const char *setOfChars, int32_t fromOffset);

	int32_t				FindSetLast(const char *setOfChars);
	int32_t				FindSetLast(const char *setOfChars, int32_t beforeOffset);

	//@}

	// --------------------------------------------------------------
	/*!	@name Replacing
		Changing characters inside the string. */
	//@{

	SString 			&ReplaceFirst(char replaceThis, char withThis);
	SString 			&ReplaceLast(char replaceThis, char withThis);
	SString 			&ReplaceAll(char replaceThis, char withThis,
							int32_t fromOffset = 0);
	SString 			&Replace(char replaceThis, char withThis,
							int32_t maxReplaceCount, int32_t fromOffset = 0);
	SString 			&ReplaceFirst(const char *replaceThis,
							const char *withThis);
	SString 			&ReplaceLast(const char *replaceThis,
							const char *withThis);
	SString 			&ReplaceAll(const char *replaceThis,
							const char *withThis, int32_t fromOffset = 0);
	SString 			&Replace(const char *replaceThis, const char *withThis,
							int32_t maxReplaceCount, int32_t fromOffset = 0);

	SString 			&IReplaceFirst(char replaceThis, char withThis);
	SString 			&IReplaceLast(char replaceThis, char withThis);
	SString 			&IReplaceAll(char replaceThis, char withThis,
							int32_t fromOffset = 0);
	SString 			&IReplace(char replaceThis, char withThis,
							int32_t maxReplaceCount, int32_t fromOffset = 0);
	SString 			&IReplaceFirst(const char *replaceThis,
							const char *withThis);
	SString 			&IReplaceLast(const char *replaceThis,
							const char *withThis);
	SString 			&IReplaceAll(const char *replaceThis,
							const char *withThis, int32_t fromOffset = 0);
	SString 			&IReplace(const char *replaceThis, const char *withThis,
							int32_t maxReplaceCount, int32_t fromOffset = 0);
	
	SString				&ReplaceSet(const char *setOfChars, char with);
	SString				&ReplaceSet(const char *setOfChars, const char *with);

	//@}

	// --------------------------------------------------------------
	/*!	@name Fast Low-level Manipulation
		Direct access to the string buffer for efficient implementation
		of more custom string algorithms. */
	//@{

			//!	Retrieve raw string buffer for editing.
			/*!	Make room for characters to be added by C-string like manipulation.
				Returns the equivalent of String() as an editable buffer.
				@a maxLength does not need to include space for the trailing zero --
				the space allocated will be @a maxLength+1 to make room for it.
				It is illegal to call other SString routines that rely on data/length
				consistency until UnlockBuffer() sets things up again. */
	char 				*LockBuffer(int32_t maxLength);

			//!	Finish using SString as C-string, adjusting length.
			/*!	If no length passed in, strlen of internal data is used to
				determine it.  SString is in consistent state after this.  If you
				supply a new length, it should not include the trailing zero --
				this function will make room for it, and ensure it is zero. */
	SString 			&UnlockBuffer(int32_t length = -1);
	
	
			//!	Get the SSharedBuffer that the SString is using.
			/*!	Returns NULL if the string is empty. */
	const	SSharedBuffer*	SharedBuffer() const;
		
			//! Make sure string is in the SSharedBuffer pool.
	void				Pool();

			//!	Start buffering mode on the string data.
			/*!	This provides more efficient growth and reduction of the
				string size, since it doesn't have to reallocate its
				memory every time.  Be sure to call EndBuffering() before
				giving the string to anyone else. */
	void				BeginBuffering();
			//!	Stop BeginBuffering() mode.
	void				EndBuffering();

	//@}

	// --------------------------------------------------------------
	/*!	@name UTF-8 Utility Functions
		Conveniences for working with the string UTF-8 data as
		distinct Unicode characters. */
	//@{

			//!	Returns number of utf-8 characters in string.
			/*!	Keep in mind that the number of characters in
				the string is not necessarily the number of
				glyphs that will appear if you draw this string. */
	int32_t				CountChars() const;

			//!	Returns the index of the first byte of the utf-8 character that includes the byte at the index given.
			/*!	Returns a negative value if it can not be
				determined (which means the string isn't valid
				utf-8), or the index is greater than the length of
				the string, or if the index is the index of the
				NULL terminator. */
	int32_t				FirstForChar(int32_t index) const;

			//!	Returns the index of the first byte of the utf-8 character after the one that includes the byte at the index given.
			/*!	The returned index will count
				up to Length() (when called on the last character
				in the string) and then become negative. */
	int32_t				NextCharFor(int32_t index) const;
		
			//!	Returns the index of the first byte of the utf-8 character after the one that starts at the index given.
			/*!	This is faster than NextCharAfter, but
				index must be the index of the first byte of the character. */
	int32_t				NextChar(int32_t index) const;

			//!	Returns the length of the utf-8 character with its first byte at the given index.
			/*!	If index is
				greater than the length or less than 0, this
				function returns -1.  If index is not the first
				byte of a character, this function returns 0. */
	int32_t				CharLength(int32_t index) const;
	
			//!	Returns the unicode value for the character that starts at index.
			/*!	Returns the invalid unicode
				character 0xffff if there isn't a whole character
				starting at index (note this is 0xffff not 0xffffffff,
				it is not sign-extended). */
	wchar32_t			CharAt(int32_t index) const;

			//! Appends the unicode character (unicode scalar value) to the string.
	SString				&AppendChar(wchar32_t unicode);

	//@}

	// --------------------------------------------------------------
	/*!	@name Case Conversions
		Lowercase to uppercase, or uppercase to lowercase. */
	//@{

			//!	Convert the whole string.
	SString				&ToLower();
			//!	Convert the whole string.
	SString 			&ToUpper();

			//!	Convert the supplied range.
	SString				&ToLower(int32_t first, int32_t length);
			//!	Convert the supplied range.
	SString 			&ToUpper(int32_t first, int32_t length);

			//! Converts first character to upper-case, rest to lower-case
	SString 			&Capitalize();

			//! Converts first character in each white-space-separated word to upper-case, rest to lower-case
	SString 			&CapitalizeEachWord();

	//@}

	// --------------------------------------------------------------
	/*!	@name Escaping and Descaping
		Adding and removing escape sequences to protect special characters. */
	//@{

			/*!	Copies @a original into @a this, escaping characters
				specified in @a setOfCharsToEscape by prepending
				them with @a escapeWith */
	SString				&CharacterEscape(const char *original,
							const char *setOfCharsToEscape, char escapeWith);
			/*!	Escapes characters specified in @a setOfCharsToEscape
				by prepending them with @a escapeWith */
	SString				&CharacterEscape(const char *setOfCharsToEscape,
							char escapeWith);

			/*!	Copy @a original into the string removing the escaping
				characters @a escapeChar */
	SString				&CharacterDeescape(const char *original, char escapeChar);
			/*!	Remove the escaping characters @a escapeChar from 
				the string */
	SString				&CharacterDeescape(char escapeChar);

	//@}

	// --------------------------------------------------------------
	/*!	@name Handling Whitespace
		Commmon whitespace-related operations. */
	//@{

			//!	Remove all leading and trailing whitespace.
	SString				&Trim();
	
			//!	Get rid of redundant whitespace.
			/*!	Removes all leading and trailing whitespace, and
				converts all other whitepace to exactly one
				space character  */
	SString				&Mush();
	
			//!	Removes all whitespace.
	SString				&StripWhitespace();

			//!	Anything but whitespace in the string?
			/*!	Returns true if the string contains one or more
				non-whitespace characters, or the length is
				zero.  Returns false otherwise. */
	bool				IsOnlyWhitespace() const;
	
	//@}

	// --------------------------------------------------------------
	/*!	@name SPrintf Replacements
		Slower than sprintf but type and overflow safe. */
	//@{

	SString				&operator<<(const char *);
	SString				&operator<<(const SString &);
	SString				&operator<<(char);

	SString 			&operator<<(int);
	SString 			&operator<<(unsigned int);
	SString 			&operator<<(unsigned long);
	SString 			&operator<<(long);
	SString 			&operator<<(uint64_t);
	SString 			&operator<<(int64_t);
			//!	Float output hardcodes %.2f style formatting.
	SString				&operator<<(float);

	//@}

	// --------------------------------------------------------------
	/*!	@name Path Handling
		Manipulating string as a filename path. */
	//@{

			//!	Set string to a path.
			/*!	The @a dir and @a leaf are concatenated to
				form a full valid path.  If @a normalize is true,
				the path segments such as ".." and "." will be removed. */
	status_t			PathSetTo(const SString &dir, const SString &leaf = B_EMPTY_STRING,
							bool normalize = false);
			//!	Set string to a path.
			/*!	The @a dir and @a leaf are concatenated to
				form a full valid path.  If @a normalize is true,
				the path segments such as ".." and "." will be removed. */
	status_t			PathSetTo(const char *dir, const char *leaf = NULL,
							bool normalize = false);
			//!	Return the end of the string representing the leaf node in the path.
	status_t			PathNormalize();
	const char *		PathLeaf(void) const;
			//!	Return the parent portion of the path.
			/*!	This is all of the path up to but not including PathLeaf(). */
	status_t			PathGetParent(SString *parentPath) const;
			//!	Append a path to the current path string.
			/*!	If @a leaf is an absolute path, it will replace the current
				path.  @a normalize is as per PathSetTo(). */
	status_t 			PathAppend(const SString &leaf, bool normalize = false);
			//!	Append a path to the current path string.
			/*!	If @a leaf is an absolute path, it will replace the current
				path.  @a normalize is as per PathSetTo(). */
	status_t 			PathAppend(const char *leaf, bool normalize = false);
			//!	Retrieve the root part of the path and place it in @a root.
	status_t			PathGetRoot(SString* root) const;
			//!	Remove root part of path and place it in @a root.
			/*!	This is often used for walking a path -- @a root contains
				the name of the entry at the current level, and @a this
				is the remaining part of the path that must be processed. */
	status_t			PathRemoveRoot(SString* root);

			//!	The character that is used to separate path segments.
	static const char *	PathDelimiter();

	//@}

/*----- Private or reserved ------------------------------------------------*/
private:
	void 				_Init(const char *, int32_t);
	void 				_DoAssign(const char *, int32_t);
	void 				_DoSetTo(const char *, int32_t);
	status_t			_PathSetTo(const char *dir, int32_t dirLength,
						const char *leaf, int32_t leafLength, bool normalize);
	void 				_DoAppend(const char *, int32_t);
	void 				_DoAppend(const SString& s);
	char 				*_GrowBy(int32_t);
	char 				*_OpenAtBy(int32_t, int32_t);
	char 				*_ShrinkAtBy(int32_t, int32_t);
	void 				_DoPrepend(const char *, int32_t);
	void				_DoCompact(const char* set, const char* replace);
	void				_DoReplaceAt(int32_t offset, int32_t sourceLength, int32_t replaceLength, const char *withThis);
	int32_t 			_FindAfter(const char *, int32_t, int32_t) const;
	int32_t 			_IFindAfter(const char *, int32_t, int32_t) const;
	int32_t 			_ShortFindAfter(const char *, int32_t) const;
	int32_t 			_FindBefore(const char *, int32_t, int32_t) const;
	int32_t 			_IFindBefore(const char *, int32_t, int32_t) const;
	
	void				_SetUsingAsCString(bool);
	void 				_AssertNotUsingAsCString() const;

	SSharedBuffer*		_Edit(size_t newStrLen);

protected:
	const char *		_privateData;
};


/*----- Type and STL utilities --------------------------------------*/
_IMPEXP_SUPPORT void	BMoveBefore(SString* to, SString* from, size_t count);
_IMPEXP_SUPPORT void	BMoveAfter(SString* to, SString* from, size_t count);
inline void				BSwap(SString& v1, SString& v2);
inline int32_t			BCompare(const SString& v1, const SString& v2);
inline void				swap(SString& x, SString& y);

/*----- Comutative compare operators --------------------------------------*/
bool 					operator<(const char *, const SString &);
bool 					operator<=(const char *, const SString &);
bool 					operator==(const char *, const SString &);
bool 					operator>(const char *, const SString &);
bool 					operator>=(const char *, const SString &);
bool 					operator!=(const char *, const SString &);

/*----- Non-member compare for sorting, etc. ------------------------------*/
int 					Compare(const SString &, const SString &);
int 					ICompare(const SString &, const SString &);
int 					Compare(const SString *, const SString *);
int 					ICompare(const SString *, const SString *);

/*----- Streaming into ITextOutput --------------------------------------------*/

_IMPEXP_SUPPORT const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SString& string);

/*!	@} */

/*-------------------------------------------------------------------------*/
/*---- No user serviceable parts after this -------------------------------*/

inline int32_t 
SString::Length() const
{
	// This is the length as contained in SSharedBuffer.  We are doing it
	// this way just to avoid having to include the header.
	return (*((int32_t *)_privateData - 1) >> 1) - 1;
}

inline const char *
SString::String() const
{
	return _privateData;
}

inline SString &
SString::SetTo(const char *str)
{
	return operator=(str);
}

inline char 
SString::operator[](size_t index) const
{
	return _privateData[index];
}

inline char 
SString::ByteAt(int32_t index) const
{
	if (index < 0 || index > Length())
		return 0;
	return _privateData[index];
}

inline SString &
SString::operator+=(const SString &string)
{
	_DoAppend(string);
	return *this;
}

inline SString &
SString::Append(const SString &string)
{
	_DoAppend(string);
	return *this;
}

inline SString &
SString::Append(const char *str)
{
	return operator+=(str);
}

inline bool 
SString::operator==(const SString &string) const
{
	return strcmp(String(), string.String()) == 0;
}

inline bool 
SString::operator<(const SString &string) const
{
	return strcmp(String(), string.String()) < 0;
}

inline bool 
SString::operator<=(const SString &string) const
{
	return strcmp(String(), string.String()) <= 0;
}

inline bool 
SString::operator>=(const SString &string) const
{
	return strcmp(String(), string.String()) >= 0;
}

inline bool 
SString::operator>(const SString &string) const
{
	return strcmp(String(), string.String()) > 0;
}

inline bool 
SString::operator!=(const SString &string) const
{
	return strcmp(String(), string.String()) != 0;
}

inline bool 
SString::operator!=(const char *str) const
{
	return !operator==(str);
}

inline void
BSwap(SString& v1, SString& v2)
{
	v1.Swap(v2);
}

inline int32_t
BCompare(const SString& v1, const SString& v2)
{
	return strcmp(v1.String(), v2.String());
}

inline void
swap(SString& x, SString& y)
{
	x.Swap(y);
}

inline bool 
operator<(const char *str, const SString &string)
{
	return string > str;
}

inline bool 
operator<=(const char *str, const SString &string)
{
	return string >= str;
}

inline bool 
operator==(const char *str, const SString &string)
{
	return string == str;
}

inline bool 
operator>(const char *str, const SString &string)
{
	return string < str;
}

inline bool 
operator>=(const char *str, const SString &string)
{
	return string <= str;
}

inline bool 
operator!=(const char *str, const SString &string)
{
	return string != str;
}

SValue BArrayAsValue(const SString* from, size_t count);
status_t BArrayConstruct(SString* to, const SValue& value, size_t count);

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif /* _SUPPORT_STRING_H */
