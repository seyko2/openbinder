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

#ifndef	_LOCALEMGR_H__
#define	_LOCALEMGR_H__

#include <LocaleMgrTypes.h>
#include <TextMgr.h>			// CharEncodingType

/***********************************************************************
 * Locale Manager routines
 **********************************************************************/
#ifdef __cplusplus
	extern "C" {
#endif

/* Return the ROM locale in *<oROMLocale>, and the ROM character
encoding as the function result.
*/
CharEncodingType
LmGetROMLocale(		LmLocaleType*	oROMLocale);

/* Return the current system locale in *<oSystemLocale>, and the
system character encoding as the function result.
*/
CharEncodingType
LmGetSystemLocale(	LmLocaleType*	oSystemLocale);

/* Return the locale chosen by the user in the Formats panel in *<oFormatsLocale>.
*/
void
LmGetFormatsLocale(	LmLocaleType*	oFormatsLocale);

/* Set the formats locale to iFormatsLocale, presumably because the user has
identified this as their current locale.  Also update various locale-dependent
preferences items (e.g., number format) so that they match <iFormatsLocale>:
*/
status_t
LmSetFormatsLocale(	const
					LmLocaleType*	iFormatsLocale);

/* Convert <iLanguage> into its (lowercase) 7-bit ASCII ISO 639 two-character
representation, and place the result in <oISONameStr> (which must be at least
3 bytes long).
*/
status_t
LmLanguageToISOName(LmLanguageType	iLanguage,
					char*			oISONameStr);

/* Convert <iCountry> into its (lowercase) 7-bit ASCII ISO 3166 two-character
representation, and place the result in <oISONameStr> (which must be at least
3 bytes long).
*/
status_t
LmCountryToISOName(	LmCountryType	iCountry,
					char*			oISONameStr);

/* Convert the 7-bit ASCII ISO 639 two-character language code representation
<iISONameStr> into *<oLanguage>.
*/
status_t
LmISONameToLanguage(const
					char*			iISONameStr,
					LmLanguageType*	oLanguage);

/* Convert the 7-bit ASCII ISO 3166 two-character country code representation
<iISONameStr> into *<oCountry>.
*/
status_t
LmISONameToCountry(	const
					char*			iISONameStr,
					LmCountryType*	oCountry);

/* Return the number of known locales (maximum locale index + 1).
*/
uint16_t
LmGetNumLocales(void);

/* Convert <iLocale> to <oLocaleIndex> by locating it within the set of known
locales.
*/
status_t
LmLocaleToIndex(	const
					LmLocaleType*	iLocale,
					uint16_t*		oLocaleIndex);

/* Convert <iLocale> to <oLocaleIndex> by locating it within the set of known
locales.  If an exact match cannot be found, the index of the most similar known
locale to <iLocale> will be returned.  This routine is guaranteed to return a
valid locale index.
*/
uint16_t
LmBestLocaleToIndex(const
					LmLocaleType*	iLocale);

/* Return in <oValue> the setting identified by <iChoice> which is appropriate for
the locale identified by <iLocaleIndex>.  Return lmErrSettingDataOverflow if the
data for <iChoice> occupies more than <iValueSize> bytes.  Display a non-fatal
error if <iValueSize> is larger than the data for a fixed-size setting.
*/
status_t
LmGetLocaleSetting(	uint16_t		iLocaleIndex,
					LmLocaleSettingChoice	iChoice,
					void*			oValue,
					uint16_t		iValueSize);

/* Return the <oThousandSeparatorChar> and <oDecimalSeparatorChar> used for
<iNumberFormat>.
*/
#define LocGetNumberSeparators LmGetNumberSeparators
void
LmGetNumberSeparators(NumberFormatType	iNumberFormat, 
					char*			oThousandSeparatorChar,
					char*			oDecimalSeparatorChar);
	

#ifdef __cplusplus
	}
#endif

#endif // _LOCALEMGR_H__
