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

#ifndef _TEXTMGR_H_
#define _TEXTMGR_H_

#include <PalmTypes.h>
#include <Chars.h>

/***********************************************************************
 * Public types & constants
 ***********************************************************************/

typedef uint16_t CharEncodingType;

// Maximum length of any character encoding name.
#define	maxEncodingNameLength	40

/* Various character encodings supported by the PalmOS. Actually these
are a mixture of repetoires (coded character sets) and character encodings
(CES - character encoding standard). Many, however, are some of both (e.g.
CP932 is the Shift-JIS encoding of three JIS character sets + Microsoft's
extensions).

WARNING!!!!!
This character code section must match the analogous section of LocaleModule.rh!
You must also update the encoding flags array inside TxtGetEncodingFlags
	whenever you define new encodings.
*/
#define CHAR_ENCODING_VALUE(value) ((CharEncodingType)value)

// Unknown to this version of PalmOS.
#define	charEncodingUnknown		CHAR_ENCODING_VALUE(0)

// Maximum character encoding _currently_ defined
#define	charEncodingMax			CHAR_ENCODING_VALUE(91)

// Latin Palm OS character encoding, and subsets.
// PalmOS variant of CP1252, with 10 extra Greek characters
#define charEncodingPalmGSM		CHAR_ENCODING_VALUE(78)
// PalmOS version of CP1252
#define	charEncodingPalmLatin	CHAR_ENCODING_VALUE(3)
// Windows variant of 8859-1
#define	charEncodingCP1252		CHAR_ENCODING_VALUE(7)
// ISO 8859 Part 1
#define	charEncodingISO8859_1	CHAR_ENCODING_VALUE(2)
// ISO 646-1991
#define	charEncodingAscii		CHAR_ENCODING_VALUE(1)

// Japanese Palm OS character encoding, and subsets.
// PalmOS version of CP932
#define	charEncodingPalmSJIS	CHAR_ENCODING_VALUE(5)
// Windows variant of ShiftJIS
#define	charEncodingCP932		CHAR_ENCODING_VALUE(8)
// Encoding for JIS 0208-1990 + 1-byte katakana
#define	charEncodingShiftJIS	CHAR_ENCODING_VALUE(4)

// Unicode character encodings
#define charEncodingUCS2		CHAR_ENCODING_VALUE(9)
#define charEncodingUTF8		CHAR_ENCODING_VALUE(6)
#define charEncodingUTF7		CHAR_ENCODING_VALUE(24)
#define charEncodingUTF16		CHAR_ENCODING_VALUE(75)
#define charEncodingUTF16BE		CHAR_ENCODING_VALUE(76)
#define charEncodingUTF16LE		CHAR_ENCODING_VALUE(77)
#define charEncodingUTF32		CHAR_ENCODING_VALUE(84)
#define charEncodingUTF32BE		CHAR_ENCODING_VALUE(85)
#define charEncodingUTF32LE		CHAR_ENCODING_VALUE(86)
#define charEncodingUTF7_IMAP	CHAR_ENCODING_VALUE(87)
#define	charEncodingUCS4		CHAR_ENCODING_VALUE(88)

// Latin character encodings
#define charEncodingCP850		CHAR_ENCODING_VALUE(12)
#define charEncodingCP437		CHAR_ENCODING_VALUE(13)
#define charEncodingCP865		CHAR_ENCODING_VALUE(14)
#define charEncodingCP860		CHAR_ENCODING_VALUE(15)
#define charEncodingCP861		CHAR_ENCODING_VALUE(16)
#define charEncodingCP863		CHAR_ENCODING_VALUE(17)
#define charEncodingCP775		CHAR_ENCODING_VALUE(18)
#define charEncodingMacIslande	CHAR_ENCODING_VALUE(19)
#define charEncodingMacintosh	CHAR_ENCODING_VALUE(20)
#define charEncodingCP1257		CHAR_ENCODING_VALUE(21)
#define charEncodingISO8859_3	CHAR_ENCODING_VALUE(22)
#define charEncodingISO8859_4	CHAR_ENCODING_VALUE(23)

// Extended Latin character encodings
#define charEncodingISO8859_2	CHAR_ENCODING_VALUE(26)
#define charEncodingCP1250		CHAR_ENCODING_VALUE(27)
#define charEncodingCP852		CHAR_ENCODING_VALUE(28)
#define charEncodingXKamenicky	CHAR_ENCODING_VALUE(29)
#define charEncodingMacXCroate	CHAR_ENCODING_VALUE(30)
#define charEncodingMacXLat2	CHAR_ENCODING_VALUE(31)
#define charEncodingMacXRomania	CHAR_ENCODING_VALUE(32)
#define charEncodingGSM			CHAR_ENCODING_VALUE(90)

// Japanese character encodings
#define charEncodingEucJp		CHAR_ENCODING_VALUE(25)
#define charEncodingISO2022Jp	CHAR_ENCODING_VALUE(10)
#define charEncodingXAutoJp		CHAR_ENCODING_VALUE(11)

// Greek character encodings
#define charEncodingISO8859_7	CHAR_ENCODING_VALUE(33)
#define charEncodingCP1253		CHAR_ENCODING_VALUE(34)
#define charEncodingCP869		CHAR_ENCODING_VALUE(35)
#define charEncodingCP737		CHAR_ENCODING_VALUE(36)
#define charEncodingMacXGr		CHAR_ENCODING_VALUE(37)

// Cyrillic character encodings
#define charEncodingCP1251		CHAR_ENCODING_VALUE(38)
#define charEncodingISO8859_5	CHAR_ENCODING_VALUE(39)
#define charEncodingKoi8R		CHAR_ENCODING_VALUE(40)
#define charEncodingKoi8		CHAR_ENCODING_VALUE(41)
#define charEncodingCP855		CHAR_ENCODING_VALUE(42)
#define charEncodingCP866		CHAR_ENCODING_VALUE(43)
#define charEncodingMacCyr		CHAR_ENCODING_VALUE(44)
#define charEncodingMacUkraine	CHAR_ENCODING_VALUE(45)

// Turkish character encodings
#define charEncodingCP1254		CHAR_ENCODING_VALUE(46)
#define charEncodingISO8859_9	CHAR_ENCODING_VALUE(47)
#define charEncodingCP857		CHAR_ENCODING_VALUE(48)
#define charEncodingMacTurc		CHAR_ENCODING_VALUE(49)
#define charEncodingCP853		CHAR_ENCODING_VALUE(50)

// Arabic character encodings
#define charEncodingISO8859_6	CHAR_ENCODING_VALUE(51)
#define charEncodingAsmo708		CHAR_ENCODING_VALUE(52)
#define charEncodingCP1256		CHAR_ENCODING_VALUE(53)
#define charEncodingCP864		CHAR_ENCODING_VALUE(54)
#define charEncodingAsmo708Plus	CHAR_ENCODING_VALUE(55)
#define charEncodingAsmo708Fr	CHAR_ENCODING_VALUE(56)
#define charEncodingMacAra		CHAR_ENCODING_VALUE(57)

// Simplified Chinese character encodings
#define charEncodingGB2312		CHAR_ENCODING_VALUE(58)
#define charEncodingHZ			CHAR_ENCODING_VALUE(59)
#define charEncodingGBK			CHAR_ENCODING_VALUE(82)
#define charEncodingPalmGB		CHAR_ENCODING_VALUE(83)

// Traditional Chinese character encodings
#define charEncodingBig5		CHAR_ENCODING_VALUE(60)
#define charEncodingBig5_HKSCS	CHAR_ENCODING_VALUE(79)
#define charEncodingBig5Plus	CHAR_ENCODING_VALUE(80)
#define charEncodingPalmBig5	CHAR_ENCODING_VALUE(81)
#define	charEncodingISO2022CN	CHAR_ENCODING_VALUE(89)

// Vietnamese character encodings
#define charEncodingViscii		CHAR_ENCODING_VALUE(61)
#define charEncodingViqr		CHAR_ENCODING_VALUE(62)
#define charEncodingVncii		CHAR_ENCODING_VALUE(63)
#define charEncodingVietnet		CHAR_ENCODING_VALUE(65)
#define charEncodingCP1258		CHAR_ENCODING_VALUE(66)

// Korean character encodings
#define charEncodingEucKr		CHAR_ENCODING_VALUE(67)		// Was charEncodingKsc5601
#define charEncodingCP949		CHAR_ENCODING_VALUE(68)
#define charEncodingISO2022Kr	CHAR_ENCODING_VALUE(69)
#define charEncodingPalmKorean	CHAR_ENCODING_VALUE(91)

// Hebrew character encodings
#define charEncodingISO8859_8I	CHAR_ENCODING_VALUE(70)
#define charEncodingISO8859_8	CHAR_ENCODING_VALUE(71)
#define charEncodingCP1255		CHAR_ENCODING_VALUE(72)
#define charEncodingCP1255V		CHAR_ENCODING_VALUE(73)

// Thai character encodings
#define charEncodingTis620		CHAR_ENCODING_VALUE(74)
#define charEncodingCP874		CHAR_ENCODING_VALUE(64)

// Character attribute flags. These replace the old flags defined in
// CharAttr.h, but are bit-compatible.

// WARNING!!!!!
// This character attribute section must match the analogous section of
// LocaleModule.rh!

#define	charAttr_DO		0x00000400 	// display only (never in user data)
#define	charAttr_XA		0x00000200 	// extra alphabetic
#define	charAttr_XS		0x00000100 	// extra space
#define	charAttr_BB		0x00000080 	// BEL, BS, etc.
#define	charAttr_CN		0x00000040 	// CR, FF, HT, NL, VT
#define	charAttr_DI		0x00000020 	// '0'-'9'
#define	charAttr_LO		0x00000010 	// 'a'-'z' and lowercase extended chars.
#define	charAttr_PU		0x00000008 	// punctuation
#define	charAttr_SP		0x00000004 	// space
#define	charAttr_UP		0x00000002 	// 'A'-'Z' and uppercase extended chars.
#define	charAttr_XD		0x00000001 	// '0'-'9', 'A'-'F', 'a'-'f'

// Various byte attribute flags. Note that multiple flags can be
// set, thus a byte could be both a single-byte character, or the first
// byte of a multi-byte character.

// WARNING!!!!!
// This byte attribute section must match the analogous section of
// LocaleModule.rh!

#define	byteAttrFirst				0x80	// First byte of multi-byte char.
#define	byteAttrLast				0x40	// Last byte of multi-byte char.
#define	byteAttrMiddle				0x20	// Middle byte of muli-byte char.
#define	byteAttrSingle				0x01	// Single byte.

// Some double-byte encoding combinations. Every byte in a stream of
// double-byte data must be either a single byte, a single/low byte,
// or a high/low byte.
#define byteAttrSingleLow		(byteAttrSingle | byteAttrLast)
#define byteAttrHighLow			(byteAttrFirst | byteAttrLast)
 
// Transliteration operations for the TxtTransliterate call. We don't use
// an enum, since each character encoding contains its own set of special
// transliteration operations (which begin at translitOpCustomBase).
typedef uint16_t TranslitOpType;

// Standard transliteration operations.
#define	translitOpStandardBase	0			// Beginning of standard operations.

#define	translitOpUpperCase		0
#define	translitOpLowerCase		1
#define	translitOpReserved2		2
#define	translitOpReserved3		3

// Custom transliteration operations (defined in CharXXXX.h encoding-specific
// header files.
#define	translitOpCustomBase		1000		// Beginning of char-encoding specific ops.

#define	translitOpPreprocess		0x8000	// Mask for pre-process option, where
										// no transliteration actually is done.

// Structure used to maintain state across calls to TxtConvertEncoding, for
// proper handling of source or destination encodings with have modes.
#define	kTxtConvertStateSize		32

typedef struct {
	uint8_t		ioSrcState[kTxtConvertStateSize];
	uint8_t		ioDstState[kTxtConvertStateSize];
} TxtConvertStateType;

// Character encoding assumed for substitution text by TxtConvertEncoding
#define textSubstitutionEncoding	charEncodingUTF8

// Default substitution text for use with TxtConvertEncoding
#define	textSubstitutionDefaultStr	"?"
#define textSubstitutionDefaultLen	1

// Flag to OR with the charEncodingType that is passed to TxtConvertEncoding
#define charEncodingDstBestFitFlag	0x8000

// Flags returned by TxtGetEncodingFlags. These are also available to 68K
// apps via the sysFtrNumCharEncodingFlags feature.
#define	charEncodingOnlySingleByte	0x00000001
#define	charEncodingHasDoubleByte	0x00000002
#define	charEncodingHasLigatures	0x00000004
#define	charEncodingRightToLeft		0x00000008

// Flags returned by FtrGet(sysFtrCreator, sysFtrNumTextMgrFlags)
#define textMgrExistsFlag			0x00000001	// Was intlMgrExists
#define	textMgrStrictFlag			0x00000002	// Was intlMgrStrict
#define	textMgrBestFitFlag			0x00000004	// Was intlMgrBestFit
#define textMgrHighASCIIFixupFlag	0x00000008	// New in 6.2 for 68K apps only!

// Various sets of character attribute flags.
#define	charAttrPrint				(charAttr_DI|charAttr_LO|charAttr_PU|charAttr_SP|charAttr_UP|charAttr_XA)
#define	charAttrSpace				(charAttr_CN|charAttr_SP|charAttr_XS)
#define	charAttrAlNum				(charAttr_DI|charAttr_LO|charAttr_UP|charAttr_XA)
#define	charAttrAlpha				(charAttr_LO|charAttr_UP|charAttr_XA)
#define	charAttrCntrl				(charAttr_BB|charAttr_CN)
#define	charAttrGraph				(charAttr_DI|charAttr_LO|charAttr_PU|charAttr_UP|charAttr_XA)
#define	charAttrDelim				(charAttr_SP|charAttr_PU)

// Remember that sizeof(0x0D) == 2 because 0x0D is treated like an int. The
// same is true of sizeof('a'), sizeof('\0'), and sizeof(chrNull). For this
// reason it's safest to use the sizeOf7BitChar macro to document buffer size
// and string length calcs. Note that this can only be used with low-ascii
// characters, as anything else might be the high byte of a double-byte char.
#define	sizeOf7BitChar(c)			1

// Maximum size a single character will occupy in a text string.
#define	maxCharBytes				4

// Text Manager error codes.
#define	txtErrUknownTranslitOp				(txtErrorClass | 1)
#define	txtErrTranslitOverrun				(txtErrorClass | 2)
#define	txtErrTranslitOverflow				(txtErrorClass | 3)
#define	txtErrConvertOverflow				(txtErrorClass | 4)
#define	txtErrConvertUnderflow				(txtErrorClass | 5)
#define	txtErrUnknownEncoding				(txtErrorClass | 6)
#define	txtErrNoCharMapping					(txtErrorClass | 7)
#define	txtErrTranslitUnderflow				(txtErrorClass | 8)
#define	txtErrMalformedText					(txtErrorClass | 9)
#define	txtErrUnknownEncodingFallbackCopy	(txtErrorClass | 10)

/***********************************************************************
 * Public macros
 ***********************************************************************/

#define	TxtCharIsSpace(ch)		((TxtCharAttr(ch) & charAttrSpace) != 0)
#define	TxtCharIsPrint(ch)		((TxtCharAttr(ch) & charAttrPrint) != 0)
#define	TxtCharIsDigit(ch)		((TxtCharAttr(ch) & charAttr_DI) != 0)
#define	TxtCharIsAlNum(ch)		((TxtCharAttr(ch) & charAttrAlNum) != 0)
#define	TxtCharIsAlpha(ch)		((TxtCharAttr(ch) & charAttrAlpha) != 0)
#define	TxtCharIsCntrl(ch)		((TxtCharAttr(ch) & charAttrCntrl) != 0)
#define	TxtCharIsGraph(ch)		((TxtCharAttr(ch) & charAttrGraph) != 0)
#define	TxtCharIsLower(ch)		((TxtCharAttr(ch) & charAttr_LO) != 0)
#define	TxtCharIsPunct(ch)		((TxtCharAttr(ch) & charAttr_PU) != 0)
#define	TxtCharIsUpper(ch)		((TxtCharAttr(ch) & charAttr_UP) != 0)
#define	TxtCharIsHex(ch)		((TxtCharAttr(ch) & charAttr_XD) != 0)
#define	TxtCharIsDelim(ch)		((TxtCharAttr(ch) & charAttrDelim) != 0)

// <c> is a hard key if the event modifier <m> has the command bit set
// and <c> is either in the proper range or is the calculator character.
#define	TxtCharIsHardKey(m, c)	((((m) & commandKeyMask) != 0) && \
								((((c) >= hardKeyMin) && ((c) <= hardKeyMax)) || ((c) == calcChr)))

// <c> is a virtual character if the event modifier <m> has the command
// bit set.
#define	TxtCharIsVirtual(m, c)	(((m) & commandKeyMask) != 0)

#define	TxtPreviousCharSize(iTextP, iOffset)	TxtGetPreviousChar((iTextP), (iOffset), NULL)
#define	TxtNextCharSize(iTextP, iOffset)		TxtGetNextChar((iTextP), (iOffset), NULL)

//***************************************************************************
// Macros for detecting if character is a rocker character or wheel character
//***************************************************************************

// <c> is a rocker key if the event modifier <m> has the command bit set
// and <c> is in the proper range
#define	TxtCharIsRockerKey(m, c)	((((m) & commandKeyMask) != 0) && \
									((((c) >= vchrRockerUp) && ((c) <= vchrRockerCenter))))

// <c> is a wheel key if the event modifier <m> has the command bit set
// and <c> is in the proper range
#define TxtCharIsWheelKey(m, c)	((((m) & commandKeyMask) != 0) && \
								((((c) >= vchrThumbWheelUp) && ((c) <= vchrThumbWheelBack))))

/***********************************************************************
 * Public routines
 ***********************************************************************/

#ifdef __cplusplus
	extern "C" {
#endif

// Return back byte attribute (first, last, single, middle) for <iByte>.
uint8_t TxtByteAttr(uint8_t iByte);
		
// Return back the standard attribute bits for <iChar>.
uint32_t TxtCharAttr(wchar32_t iChar);

// Return back the extended attribute bits for <iChar>.
uint32_t TxtCharXAttr(wchar32_t iChar);

// Return the size (in bytes) of the character <iChar>. This represents
// how many bytes would be required to store the character in a string.
size_t TxtCharSize(wchar32_t iChar);

// Load the character before offset <iOffset> in the <iTextP> text. Return
// back the size of the character.
size_t TxtGetPreviousChar(const char* iTextP, size_t iOffset, wchar32_t* oChar);

// Load the character at offset <iOffset> in the <iTextP> text. Return
// back the size of the character.
size_t TxtGetNextChar(const char* iTextP, size_t iOffset, wchar32_t* oChar);

// Return the character at offset <iOffset> in the <iTextP> text.
wchar32_t TxtGetChar(const char* iTextP, size_t iOffset);

// Set the character at offset <iOffset> in the <iTextP> text, and
// return back the size of the character.
size_t TxtSetNextChar(char* iTextP, size_t iOffset, wchar32_t iChar);

// Replace the substring "^X" (where X is 0..9, as specified by <iParamNum>)
// with the string <inParamStr>. If <iParamStringP> is NULL then don't modify <iStringP>.
// Make sure the resulting string doesn't contain more than <iMaxLen> bytes,
// excluding the terminating null. Return back the number of occurances of
// the substring found in <iStringP>.
uint16_t TxtReplaceStr(char* iStringP, size_t iMaxLen, const char* iParamStringP, uint16_t iParamNum);

// Allocate a handle containing the result of substituting param0...param3
// for ^0...^3 in <inTemplate>, and return the locked result. If a parameter
// is NULL, replace the corresponding substring in the template with "".

char* TxtParamString(const char* inTemplate, const char* param0,
			const char* param1, const char* param2, const char* param3);

// Return the bounds of the character at <iOffset> in the <iTextP>
// text, via the <oCharStart> & <oCharEnd> offsets, and also return the
// actual value of character at or following <iOffset>.
wchar32_t TxtCharBounds(const char* iTextP, size_t iOffset, size_t* oCharStart, size_t* oCharEnd);

// Return the appropriate byte position for truncating <iTextP> such that it is
// at most <iOffset> bytes long.
size_t TxtGetTruncationOffset(const char* iTextP, size_t iOffset);

// Truncate string <iSrcString> to be no more than <iMaxLength> bytes long, copying
// the resulting string to <iDstString>. If truncation is required, add an ellipsis
// if <iAddEllipsis> is true. Return true if the string was truncated. The source
// and destination strings can be the same.
Boolean TxtTruncateString(	char* iDstString,
							const char* iSrcString,
							size_t iMaxLength,
							Boolean iAddEllipsis);

// Convert the characters in <iSrcTextP> into an appropriate form for searching,
// and copy up to <iDstSize> bytes of converted characters into <oDstTextP>. The
// resulting string will be null-terminated. We assume that <iDstSize> includes
// the space required for the null. Return back the number of bytes consumed in
// <iSrcTextP>. Note that this routine returned nothing (void) in versions of
// the OS previous to 3.5 (and didn't exist before Palm OS 3.1), and that it
// used to not take an <iSrcLen> parameter.
size_t TxtPrepFindString(const char* iSrcTextP, size_t iSrcLen, char* oDstTextP, size_t iDstSize);

// Search for <iTargetStringP> in <iSrcStringP>. If found return true and pass back
// the found position (byte offset) in <oFoundPos>, and the length of the matched
// text in <oFoundLen>.
Boolean TxtFindString(const char* iSrcStringP, const char* iTargetStringP,
			size_t* oFoundPos, size_t* oFoundLen);

// Find the bounds of the word that contains the character at <iOffset>.
// Return the offsets in <*oWordStart> and <*oWordEnd>. Return true if the
// word we found was not empty & not a delimiter (attribute of first char
// in word not equal to space or punct).
Boolean TxtWordBounds(const char* iTextP, size_t iLength, size_t iOffset,
			size_t* oWordStart, size_t* oWordEnd);

// Return the offset of the first break position (for text wrapping) that
// occurs at or before <iOffset> in <iTextP>. Note that this routine will
// also add trailing spaces and a trailing linefeed to the break position,
// thus the result could be greater than <iOffset>.
size_t TxtGetWordWrapOffset(const char* iTextP, size_t iOffset);

// Return the minimum (lowest) encoding required for <iChar>. If we
// don't know about the character, return encoding_Unknown.
CharEncodingType TxtCharEncoding(wchar32_t iChar);

// Return the minimum (lowest) encoding required to represent <iStringP>.
// This is the maximum encoding of any character in the string, where
// highest is unknown, and lowest is ascii.
CharEncodingType TxtStrEncoding(const char* iStringP);

// Return the higher (max) encoding of <a> and <b>.
CharEncodingType TxtMaxEncoding(CharEncodingType a, CharEncodingType b);

// Return a pointer to the 'standard' name for <iEncoding>. If the
// encoding is unknown, return a pointer to an empty string.
const char* TxtEncodingName(CharEncodingType iEncoding);

// Map from a character set name <iEncodingName> to a CharEncodingType.
// If the character set name is unknown, return charEncodingUnknown.
CharEncodingType TxtNameToEncoding(const char* iEncodingName);

// Return information about the encoding <iEncoding>, such as whether
// it encodes all characters as single bytes in a string.
uint32_t TxtGetEncodingFlags(CharEncodingType iEncoding);

// Transliterate <iSrcLength> bytes of text found in <iSrcTextP>, based
// on the requested <iTranslitOp> operation. Place the results in <oDstTextP>,
// and set the resulting length in <ioDstLength>. On entry <ioDstLength>
// must contain the maximum size of the <oDstTextP> buffer. If the
// buffer isn't large enough, return an error (note that outDestText
// might have been modified during the operation). Note that if <iTranslitOp>
// has the preprocess bit set, then <oDstTextP> is not modified, and
// <ioDstLength> will contain the total space required in the destination
// buffer in order to perform the operation. 
status_t TxtTransliterate(const char* iSrcTextP, size_t iSrcLength, char* oDstTextP,
			size_t* ioDstLength, TranslitOpType iTranslitOp);

// Convert <*ioSrcBytes> of text from <srcTextP> between the <srcEncoding>
// and <dstEncoding> character encodings. If <dstTextP> is not NULL, write
// the resulting bytes to the buffer, and always return the number of
// resulting bytes in <*ioDstBytes>. Update <*srcBytes> with the number of
// bytes from the beginning of <*srcTextP> that were successfully converted.
// When the routine is called with <srcTextP> pointing to the beginning of
// a string or text buffer, <newConversion> should be true; if the text is
// processed in multiple chunks, either because errors occurred or due to
// source/destination buffer size constraints, then subsequent calls to
// this routine should pass false for <newConversion>. The TxtConvertStateType
// record maintains state information so that if the source or destination
// character encodings have state or modes (e.g. JIS), processing a single
// sequence of text with multiple calls will work correctly.

// When an error occurs due to an unconvertable character, the behavior of
// the routine will depend on the <substitutionStr> parameter. If it is NULL,
// then <*ioSrcBytes> will be set to the offset of the unconvertable character,
// <ioDstBytes> will be set to the number of successfully converted resulting
// bytes, and <dstTextP>, in not NULL, will contain conversion results up to
// the point of the error. The routine will return an appropriate error code,
// and it is up to the caller to either terminate conversion or skip over the
// unconvertable character and continue the conversion process (passing false
// for the <newConversion> parameter in subsequent calls to TxtConvertEncoding).
// If <substitutionStr> is not NULL, then this string is written to the
// destination buffer when an unconvertable character is encountered in the
// source text, and the source character is skipped. Processing continues, though
// the error code will still be returned when the routine terminates. Note that
// if a more serious error occurs during processing (e.g. buffer overflow) then
// that error will be returned even if there was an earlier unconvertable character.
// Note that the substitution string must use the destination character encoding.
status_t TxtConvertEncoding(Boolean newConversion, TxtConvertStateType* ioStateP,
			const char* srcTextP, size_t* ioSrcBytes, CharEncodingType srcEncoding,
			char* dstTextP, size_t* ioDstBytes, CharEncodingType dstEncoding,
			const char* substitutionStr, size_t substitutionLen);

// Return true if <iChar> is a valid (drawable) character. Note that we'll
// return false if it is a virtual character code.
Boolean TxtCharIsValid(wchar32_t iChar);

// Compare the first <s1Len> bytes of <s1> with the first <s2Len> bytes
// of <s2>. Return the results of the comparison: < 0 if <s1> sorts before
// <s2>, > 0 if <s1> sorts after <s2>, and 0 if they are equal. Also return
// the number of bytes that matched in <s1MatchLen> and <s2MatchLen>
// (either one of which can be NULL if the match length is not needed).
// This comparison is "caseless", in the same manner as a find operation,
// thus case, character size, etc. don't matter.
int16_t TxtCaselessCompare(const char* s1, size_t s1Len, size_t* s1MatchLen,
			const char* s2, size_t s2Len, size_t* s2MatchLen);

// Compare the first <s1Len> bytes of <s1> with the first <s2Len> bytes
// of <s2>. Return the results of the comparison: < 0 if <s1> sorts before
// <s2>, > 0 if <s1> sorts after <s2>, and 0 if they are equal. Also return
// the number of bytes that matched in <s1MatchLen> and <s2MatchLen>
// (either one of which can be NULL if the match length is not needed).
int16_t TxtCompare(const char* s1, size_t s1Len, size_t* s1MatchLen,
			const char* s2, size_t s2Len, size_t* s2MatchLen);

#ifdef __cplusplus
	}
#endif

#endif // _TEXTMGR_H_
