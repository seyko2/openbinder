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

#ifndef	_LOCALEMGRTYPES_H_
#define	_LOCALEMGRTYPES_H_

#include <PalmTypes.h>

/***********************************************************************
 * Locale Manager errors
 **********************************************************************/

/* Locale not known for call to LmLocaleToIndex, or invalid
country or language code.
*/
#define	lmErrUnknownLocale				(lmErrorClass | 1)

/* Locale index >= LmGetNumLocales().
*/
#define	lmErrBadLocaleIndex				(lmErrorClass | 2)

/* LmLocaleSettingChoice out of bounds.
*/
#define	lmErrBadLocaleSettingChoice		(lmErrorClass | 3)

/* Data for locale setting too big for destination.
*/
#define	lmErrSettingDataOverflow		(lmErrorClass | 4)


/***********************************************************************
 * Locale Manager settings (pass to LmGetLocaleSetting)
 **********************************************************************/

typedef uint16_t LmLocaleSettingChoice;

/* LmLocaleType
*/
#define lmChoiceLocale					((LmLocaleSettingChoice)1)

/* char[kMaxCountryNameLen+1] - Name of the country (localized)
*/
#define lmChoiceCountryName				((LmLocaleSettingChoice)5)

/* DateFormatType
*/
#define lmChoiceDateFormat				((LmLocaleSettingChoice)6)

/* DateFormatType
*/
#define lmChoiceLongDateFormat			((LmLocaleSettingChoice)7)

/* TimeFormatType
*/
#define lmChoiceTimeFormat				((LmLocaleSettingChoice)8)

/* uint16_t - Weekday for calendar column 1 (sunday=0, monday=1, etc.)
*/
#define lmChoiceWeekStartDay			((LmLocaleSettingChoice)9)

/* int16_t - Default GMT offset minutes, + for east of GMT, - for west
*/
#define lmChoiceTimeZone				((LmLocaleSettingChoice)10)

/* NumberFormatType - Specifies decimal and thousands separator characters
*/
#define lmChoiceNumberFormat			((LmLocaleSettingChoice)11)

/* char[kMaxCurrencyNameLen+1] - Name of local currency (e.g., "US Dollar")
*/
#define lmChoiceCurrencyName			((LmLocaleSettingChoice)12)

/* char[kMaxCurrencySymbolLen+1] - Currency symbol (e.g., "$")
*/
#define lmChoiceCurrencySymbol			((LmLocaleSettingChoice)13)

/* char[kMaxCurrencySymbolLen+1] - Unique currency symbol (e.g., "US$")
*/
#define lmChoiceUniqueCurrencySymbol	((LmLocaleSettingChoice)14)

/* uint16_t - Number of decimals for currency (e.g., 2 for $10.12)
*/
#define lmChoiceCurrencyDecimalPlaces	((LmLocaleSettingChoice)15)

/* MeasurementSystemType - Metric, English, etc.
*/
#define lmChoiceMeasurementSystem		((LmLocaleSettingChoice)16)

/* Boolean - Does locale support Chinese Lunar Calendar?
*/
#define lmChoiceSupportsLunarCalendar	((LmLocaleSettingChoice)17)

/* CharEncodingType - First attempt at SMS encoding
*/
#define lmChoicePrimarySMSEncoding		((LmLocaleSettingChoice)18)

/* CharEncodingType - Second attempt at SMS encoding
*/
#define lmChoiceSecondarySMSEncoding	((LmLocaleSettingChoice)19)

/* CharEncodingType - First attempt at Email encoding
*/
#define lmChoicePrimaryEmailEncoding	((LmLocaleSettingChoice)20)

/* CharEncodingType - Second attempt at Email encoding
*/
#define lmChoiceSecondaryEmailEncoding	((LmLocaleSettingChoice)21)

/* CharEncodingType - Outbound encoding for vObjects
*/
#define lmChoiceOutboundVObjectEncoding	((LmLocaleSettingChoice)22)

/* CharEncodingType - Inbound encoding for vObjects with no CHARSET property
*/
#define lmChoiceInboundDefaultVObjectEncoding	((LmLocaleSettingChoice)23)

/***********************************************************************
 * Locale Manager constants
 **********************************************************************/

#define kMaxCountryNameLen		31
#define kMaxCurrencyNameLen		31
#define kMaxCurrencySymbolLen	10

/***********************************************************************
 * Locale constants
 **********************************************************************/

#define lmAnyLanguage		((LmLanguageType)'\?\?')
#define lmAnyCountry		((LmCountryType)'\?\?')

/* Language codes (ISO 639).
*/
#define lAfar				((LmLanguageType)'aa')
#define lAbkhazian			((LmLanguageType)'ab')
#define lAfrikaans			((LmLanguageType)'af')
#define lAmharic			((LmLanguageType)'am')
#define lArabic				((LmLanguageType)'ar')
#define lAssamese			((LmLanguageType)'as')
#define lAymara				((LmLanguageType)'ay')
#define lAzerbaijani		((LmLanguageType)'az')
#define lBashkir			((LmLanguageType)'ba')
#define lByelorussian		((LmLanguageType)'be')
#define lBulgarian			((LmLanguageType)'bg')
#define lBihari				((LmLanguageType)'bh')
#define lBislama			((LmLanguageType)'bi')
#define lBengali			((LmLanguageType)'bn')	// Bangla
#define lTibetan			((LmLanguageType)'bo')
#define lBreton				((LmLanguageType)'br')
#define lCatalan			((LmLanguageType)'ca')
#define lCorsican			((LmLanguageType)'co')
#define lCzech				((LmLanguageType)'cs')
#define lWelsh				((LmLanguageType)'cy')
#define lDanish				((LmLanguageType)'da')
#define lGerman				((LmLanguageType)'de')
#define lBhutani			((LmLanguageType)'dz')
#define lGreek				((LmLanguageType)'el')
#define lEnglish			((LmLanguageType)'en')
#define lEsperanto			((LmLanguageType)'eo')
#define lSpanish			((LmLanguageType)'es')
#define lEstonian			((LmLanguageType)'et')
#define lBasque				((LmLanguageType)'eu')
#define lPersian			((LmLanguageType)'fa')	// Farsi
#define lFinnish			((LmLanguageType)'fi')
#define lFiji				((LmLanguageType)'fj')
#define lFaroese			((LmLanguageType)'fo')
#define lFrench				((LmLanguageType)'fr')
#define lFrisian			((LmLanguageType)'fy')
#define lIrish				((LmLanguageType)'ga')
#define lScotsGaelic		((LmLanguageType)'gd')
#define lGalician			((LmLanguageType)'gl')
#define lGuarani			((LmLanguageType)'gn')
#define lGujarati			((LmLanguageType)'gu')
#define lHausa				((LmLanguageType)'ha')
#define lHindi				((LmLanguageType)'hi')
#define lCroatian			((LmLanguageType)'hr')
#define lHungarian			((LmLanguageType)'hu')
#define lArmenian			((LmLanguageType)'hy')
#define lInterlingua		((LmLanguageType)'ia')
#define lInterlingue		((LmLanguageType)'ie')
#define lInupiak			((LmLanguageType)'ik')
#define lIndonesian			((LmLanguageType)'in')
#define lIcelandic			((LmLanguageType)'is')
#define lItalian			((LmLanguageType)'it')
#define lHebrew				((LmLanguageType)'iw')
#define lYiddish			((LmLanguageType)'ji')
#define lJapanese			((LmLanguageType)'ja')	// Palm calls uses 'jp' in DB names
#define lJavanese			((LmLanguageType)'jw')
#define lGeorgian			((LmLanguageType)'ka')
#define lKazakh				((LmLanguageType)'kk')
#define lGreenlandic		((LmLanguageType)'kl')
#define lCambodian			((LmLanguageType)'km')
#define lKannada			((LmLanguageType)'kn')
#define lKorean				((LmLanguageType)'ko')
#define lKashmiri			((LmLanguageType)'ks')
#define lKurdish			((LmLanguageType)'ku')
#define lKirghiz			((LmLanguageType)'ky')
#define lLatin				((LmLanguageType)'la')
#define lLingala			((LmLanguageType)'ln')
#define lLaothian			((LmLanguageType)'lo')
#define lLithuanian			((LmLanguageType)'lt')
#define lLatvian			((LmLanguageType)'lv')	// Lettish
#define lMalagasy			((LmLanguageType)'ng')
#define lMaori				((LmLanguageType)'mi')
#define lMacedonian			((LmLanguageType)'mk')
#define lMalayalam			((LmLanguageType)'ml')
#define lMongolian			((LmLanguageType)'mn')
#define lMoldavian			((LmLanguageType)'mo')
#define lMarathi			((LmLanguageType)'mr')
#define lMalay				((LmLanguageType)'ms')
#define lMaltese			((LmLanguageType)'mt')
#define lBurmese			((LmLanguageType)'my')
#define lNauru				((LmLanguageType)'na')
#define lNepali				((LmLanguageType)'ne')
#define lDutch				((LmLanguageType)'nl')
#define lNorwegian			((LmLanguageType)'no')
#define lOccitan			((LmLanguageType)'oc')
#define lAfan				((LmLanguageType)'om')	// Oromo
#define lOriya				((LmLanguageType)'or')
#define lPunjabi			((LmLanguageType)'pa')
#define lPolish				((LmLanguageType)'pl')
#define lPashto				((LmLanguageType)'ps')	// Pushto
#define lPortuguese			((LmLanguageType)'pt')
#define lQuechua			((LmLanguageType)'qu')
#define lRhaetoRomance		((LmLanguageType)'rm')
#define lKurundi			((LmLanguageType)'rn')
#define lRomanian			((LmLanguageType)'ro')
#define lRussian			((LmLanguageType)'ru')
#define lKinyarwanda		((LmLanguageType)'rw')
#define lSanskrit			((LmLanguageType)'sa')
#define lSindhi				((LmLanguageType)'sd')
#define lSangho				((LmLanguageType)'sg')
#define lSerboCroatian		((LmLanguageType)'sh')
#define lSinghalese			((LmLanguageType)'si')
#define lSlovak				((LmLanguageType)'sk')
#define lSlovenian			((LmLanguageType)'sl')
#define lSamoan				((LmLanguageType)'sm')
#define lShona				((LmLanguageType)'sn')
#define lSomali				((LmLanguageType)'so')
#define lAlbanian			((LmLanguageType)'sq')
#define lSerbian			((LmLanguageType)'sr')
#define lSiswati			((LmLanguageType)'ss')
#define lSesotho			((LmLanguageType)'st')
#define lSudanese			((LmLanguageType)'su')
#define lSwedish			((LmLanguageType)'sv')
#define lSwahili			((LmLanguageType)'sw')
#define lTamil				((LmLanguageType)'ta')
#define lTelugu				((LmLanguageType)'te')
#define lTajik				((LmLanguageType)'tg')
#define lThai				((LmLanguageType)'th')
#define lTigrinya			((LmLanguageType)'ti')
#define lTurkmen			((LmLanguageType)'tk')
#define lTagalog			((LmLanguageType)'tl')
#define lSetswana			((LmLanguageType)'tn')
#define lTonga				((LmLanguageType)'to')
#define lTurkish			((LmLanguageType)'tr')
#define lTsonga				((LmLanguageType)'ts')
#define lTatar				((LmLanguageType)'tt')
#define lTwi				((LmLanguageType)'tw')
#define lUkrainian			((LmLanguageType)'uk')
#define lUrdu				((LmLanguageType)'ur')
#define lUzbek				((LmLanguageType)'uz')
#define lVietnamese			((LmLanguageType)'vi')
#define lVolapuk			((LmLanguageType)'vo')
#define lWolof				((LmLanguageType)'wo')
#define lXhosa				((LmLanguageType)'xh')
#define lYoruba				((LmLanguageType)'yo')
#define lChinese			((LmLanguageType)'zh')
#define lZulu				((LmLanguageType)'zu')

/* Country codes (ISO 3166).
*/
#define cAndorra					((LmCountryType)'AD')
#define cUnitedArabEmirates			((LmCountryType)'AE')
#define cAfghanistan				((LmCountryType)'AF')
#define cAntiguaAndBarbuda			((LmCountryType)'AG')
#define cAnguilla					((LmCountryType)'AI')
#define cAlbania					((LmCountryType)'AL')
#define cArmenia					((LmCountryType)'AM')
#define cNetherlandsAntilles		((LmCountryType)'AN')
#define cAngola						((LmCountryType)'AO')
#define cAntarctica					((LmCountryType)'AQ')
#define cArgentina					((LmCountryType)'AR')
#define cAmericanSamoa				((LmCountryType)'AS')
#define cAustria					((LmCountryType)'AT')
#define cAustralia					((LmCountryType)'AU')
#define cAruba						((LmCountryType)'AW')
#define cAzerbaijan					((LmCountryType)'AZ')
#define cBosniaAndHerzegovina		((LmCountryType)'BA')
#define cBarbados					((LmCountryType)'BB')
#define cBangladesh					((LmCountryType)'BD')
#define cBelgium					((LmCountryType)'BE')
#define cBurkinaFaso				((LmCountryType)'BF')
#define cBulgaria					((LmCountryType)'BG')
#define cBahrain					((LmCountryType)'BH')
#define cBurundi					((LmCountryType)'BI')
#define cBenin						((LmCountryType)'BJ')
#define cBermuda					((LmCountryType)'BM')
#define cBruneiDarussalam			((LmCountryType)'BN')
#define cBolivia					((LmCountryType)'BO')
#define cBrazil						((LmCountryType)'BR')
#define cBahamas					((LmCountryType)'BS')
#define cBhutan						((LmCountryType)'BT')
#define cBouvetIsland				((LmCountryType)'BV')
#define cBotswana					((LmCountryType)'BW')
#define cBelarus					((LmCountryType)'BY')
#define cBelize						((LmCountryType)'BZ')
#define cCanada						((LmCountryType)'CA')
#define cCocosIslands				((LmCountryType)'CC')
#define cDemocraticRepublicOfTheCongo	((LmCountryType)'CD')
#define cCentralAfricanRepublic		((LmCountryType)'CF')
#define cCongo						((LmCountryType)'CG')
#define cSwitzerland				((LmCountryType)'CH')
#define cIvoryCoast					((LmCountryType)'CI')
#define cCookIslands				((LmCountryType)'CK')
#define cChile						((LmCountryType)'CL')
#define cCameroon					((LmCountryType)'CM')
#define cChina						((LmCountryType)'CN')
#define cColumbia					((LmCountryType)'CO')
#define cCostaRica					((LmCountryType)'CR')
#define cCuba						((LmCountryType)'CU')
#define cCapeVerde					((LmCountryType)'CV')
#define cChristmasIsland			((LmCountryType)'CX')
#define cCyprus						((LmCountryType)'CY')
#define cCzechRepublic				((LmCountryType)'CZ')
#define cGermany					((LmCountryType)'DE')
#define cDjibouti					((LmCountryType)'DJ')
#define cDenmark					((LmCountryType)'DK')
#define cDominica					((LmCountryType)'DM')
#define cDominicanRepublic			((LmCountryType)'DO')
#define cAlgeria					((LmCountryType)'DZ')
#define cEcuador					((LmCountryType)'EC')
#define cEstonia					((LmCountryType)'EE')
#define cEgypt						((LmCountryType)'EG')
#define cWesternSahara				((LmCountryType)'EH')
#define cEritrea					((LmCountryType)'ER')
#define cSpain						((LmCountryType)'ES')
#define cEthiopia					((LmCountryType)'ET')
#define cFinland					((LmCountryType)'FI')
#define cFiji						((LmCountryType)'FJ')
#define cFalklandIslands			((LmCountryType)'FK')
#define cMicronesia					((LmCountryType)'FM')
#define cFaeroeIslands				((LmCountryType)'FO')
#define cFrance						((LmCountryType)'FR')
#define cMetropolitanFrance			((LmCountryType)'FX')
#define cGabon						((LmCountryType)'GA')
#define cUnitedKingdom				((LmCountryType)'GB')	// UK
#define cGrenada					((LmCountryType)'GD')
#define cGeorgia					((LmCountryType)'GE')
#define cFrenchGuiana				((LmCountryType)'GF')
#define cGhana						((LmCountryType)'GH')
#define cGibraltar					((LmCountryType)'GI')
#define cGreenland					((LmCountryType)'GL')
#define cGambia						((LmCountryType)'GM')
#define cGuinea						((LmCountryType)'GN')
#define cGuadeloupe					((LmCountryType)'GP')
#define cEquatorialGuinea			((LmCountryType)'GQ')
#define cGreece						((LmCountryType)'GR')
#define cSouthGeorgiaAndTheSouthSandwichIslands	((LmCountryType)'GS')
#define cGuatemala					((LmCountryType)'GT')
#define cGuam						((LmCountryType)'GU')
#define cGuineaBisseu				((LmCountryType)'GW')
#define cGuyana						((LmCountryType)'GY')
#define cHongKong					((LmCountryType)'HK')
#define cHeardAndMcDonaldIslands	((LmCountryType)'HM')
#define cHonduras					((LmCountryType)'HN')
#define cCroatia					((LmCountryType)'HR')
#define cHaiti						((LmCountryType)'HT')
#define cHungary					((LmCountryType)'HU')
#define cIndonesia					((LmCountryType)'ID')
#define cIreland					((LmCountryType)'IE')
#define cIsrael						((LmCountryType)'IL')
#define cIndia						((LmCountryType)'IN')
#define cBritishIndianOceanTerritory	((LmCountryType)'IO')
#define cIraq						((LmCountryType)'IQ')
#define cIran						((LmCountryType)'IR')
#define cIceland					((LmCountryType)'IS')
#define cItaly						((LmCountryType)'IT')
#define cJamaica					((LmCountryType)'JM')
#define cJordan						((LmCountryType)'JO')
#define cJapan						((LmCountryType)'JP')
#define cKenya						((LmCountryType)'KE')
#define cKyrgyzstan					((LmCountryType)'KG')	// Kirgistan
#define cCambodia					((LmCountryType)'KH')
#define cKiribati					((LmCountryType)'KI')
#define cComoros					((LmCountryType)'KM')
#define cStKittsAndNevis			((LmCountryType)'KN')
#define cDemocraticPeoplesRepublicOfKorea	((LmCountryType)'KP')
#define cRepublicOfKorea			((LmCountryType)'KR')
#define cKuwait						((LmCountryType)'KW')
#define cCaymanIslands				((LmCountryType)'KY')
#define cKazakhstan					((LmCountryType)'KK')
#define cLaos						((LmCountryType)'LA')
#define cLebanon					((LmCountryType)'LB')
#define cStLucia					((LmCountryType)'LC')
#define cLiechtenstein				((LmCountryType)'LI')
#define cSriLanka					((LmCountryType)'LK')
#define cLiberia					((LmCountryType)'LR')
#define cLesotho					((LmCountryType)'LS')
#define cLithuania					((LmCountryType)'LT')
#define cLuxembourg					((LmCountryType)'LU')
#define cLatvia						((LmCountryType)'LV')
#define cLibya						((LmCountryType)'LY')
#define cMorocco					((LmCountryType)'MA')
#define cMonaco						((LmCountryType)'MC')
#define cMoldova					((LmCountryType)'MD')
#define cMadagascar					((LmCountryType)'MG')
#define cMarshallIslands			((LmCountryType)'MH')
#define cMacedonia					((LmCountryType)'MK')
#define cMali						((LmCountryType)'ML')
#define cMyanmar					((LmCountryType)'MM')
#define cMongolia					((LmCountryType)'MN')
#define cMacau						((LmCountryType)'MO')
#define cNorthernMarianaIslands		((LmCountryType)'MP')
#define cMartinique					((LmCountryType)'MQ')
#define cMauritania					((LmCountryType)'MR')
#define cMontserrat					((LmCountryType)'MS')
#define cMalta						((LmCountryType)'MT')
#define cMauritius					((LmCountryType)'MU')
#define cMaldives					((LmCountryType)'MV')
#define cMalawi						((LmCountryType)'MW')
#define cMexico						((LmCountryType)'MX')
#define cMalaysia					((LmCountryType)'MY')
#define cMozambique					((LmCountryType)'MZ')
#define cNamibia					((LmCountryType)'NA')
#define cNewCaledonia				((LmCountryType)'NC')
#define cNiger						((LmCountryType)'NE')
#define cNorfolkIsland				((LmCountryType)'NF')
#define cNigeria					((LmCountryType)'NG')
#define cNicaragua					((LmCountryType)'NI')
#define cNetherlands				((LmCountryType)'NL')
#define cNorway						((LmCountryType)'NO')
#define cNepal						((LmCountryType)'NP')
#define cNauru						((LmCountryType)'NR')
#define cNiue						((LmCountryType)'NU')
#define cNewZealand					((LmCountryType)'NZ')
#define cOman						((LmCountryType)'OM')
#define cPanama						((LmCountryType)'PA')
#define cPeru						((LmCountryType)'PE')
#define cFrenchPolynesia			((LmCountryType)'PF')
#define cPapuaNewGuinea				((LmCountryType)'PG')
#define cPhilippines				((LmCountryType)'PH')
#define cPakistan					((LmCountryType)'PK')
#define cPoland						((LmCountryType)'PL')
#define cStPierreAndMiquelon		((LmCountryType)'PM')
#define cPitcairn					((LmCountryType)'PN')
#define cPuertoRico					((LmCountryType)'PR')
#define cPortugal					((LmCountryType)'PT')
#define cPalau						((LmCountryType)'PW')
#define cParaguay					((LmCountryType)'PY')
#define cQatar						((LmCountryType)'QA')
#define cReunion					((LmCountryType)'RE')
#define cRomania					((LmCountryType)'RO')
#define cRussianFederation			((LmCountryType)'RU')
#define cRwanda						((LmCountryType)'RW')
#define cSaudiArabia				((LmCountryType)'SA')
#define cSolomonIslands				((LmCountryType)'SB')
#define cSeychelles					((LmCountryType)'SC')
#define cSudan						((LmCountryType)'SD')
#define cSweden						((LmCountryType)'SE')
#define cSingapore					((LmCountryType)'SG')
#define cStHelena					((LmCountryType)'SH')
#define cSlovenia					((LmCountryType)'SI')
#define cSvalbardAndJanMayenIslands	((LmCountryType)'SJ')
#define cSlovakia					((LmCountryType)'SK')
#define cSierraLeone				((LmCountryType)'SL')
#define cSanMarino					((LmCountryType)'SM')
#define cSenegal					((LmCountryType)'SN')
#define cSomalia					((LmCountryType)'SO')
#define cSuriname					((LmCountryType)'SR')
#define cSaoTomeAndPrincipe			((LmCountryType)'ST')
#define cElSalvador					((LmCountryType)'SV')
#define cSyrianArabRepublic			((LmCountryType)'SY')
#define cSwaziland					((LmCountryType)'SZ')
#define cTurksAndCaicosIslands		((LmCountryType)'TC')
#define cChad						((LmCountryType)'TD')
#define cFrenchSouthernTerritories	((LmCountryType)'TF')
#define cTogo						((LmCountryType)'TG')
#define cThailand					((LmCountryType)'TH')
#define cTajikistan					((LmCountryType)'TJ')
#define cTokelau					((LmCountryType)'TK')
#define cTurkmenistan				((LmCountryType)'TM')
#define cTunisia					((LmCountryType)'TN')
#define cTonga						((LmCountryType)'TO')
#define cEastTimor					((LmCountryType)'TP')
#define cTurkey						((LmCountryType)'TR')
#define cTrinidadAndTobago			((LmCountryType)'TT')
#define cTuvalu						((LmCountryType)'TV')
#define cTaiwan						((LmCountryType)'TW')
#define cTanzania					((LmCountryType)'TZ')
#define cUkraine					((LmCountryType)'UA')
#define cUganda						((LmCountryType)'UG')
#define cUnitedStatesMinorOutlyingIslands	((LmCountryType)'UM')
#define cUnitedStates				((LmCountryType)'US')
#define cUruguay					((LmCountryType)'UY')
#define cUzbekistan					((LmCountryType)'UZ')
#define cHolySee					((LmCountryType)'VA')
#define cStVincentAndTheGrenadines	((LmCountryType)'VC')
#define cVenezuela					((LmCountryType)'VE')
#define cBritishVirginIslands		((LmCountryType)'VG')
#define cUSVirginIslands			((LmCountryType)'VI')
#define cVietNam					((LmCountryType)'VN')
#define cVanuatu					((LmCountryType)'VU')
#define cWallisAndFutunaIslands		((LmCountryType)'WF')
#define cSamoa						((LmCountryType)'WS')
#define cYemen						((LmCountryType)'YE')
#define cMayotte					((LmCountryType)'YT')
#define cYugoslavia					((LmCountryType)'YU')
#define cSouthAfrica				((LmCountryType)'ZA')
#define cZambia						((LmCountryType)'ZM')
#define cZimbabwe					((LmCountryType)'ZW')

/***********************************************************************
 * Locale Manager types
 **********************************************************************/

// The number format (thousands separator and decimal point).  This defines
// how numbers are formatted and not neccessarily currency numbers (i.e. Switzerland).
typedef Enum8 NumberFormatType ;

typedef uint8_t LanguageType;
typedef uint8_t CountryType;

typedef uint16_t LmLanguageType;
typedef uint16_t LmCountryType;

typedef struct _LmLocaleType LmLocaleType;
struct _LmLocaleType
{
	LmLanguageType	language;		// Language spoken in locale
	LmCountryType	country;		// Country of locale.
};

#endif // _LOCALEMGRTYPES_H_
