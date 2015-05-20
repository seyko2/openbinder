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

#include <support/String.h>
#include <support/Debug.h>
#include <support/Message.h>
#include <support/URL.h>
#include <support/StdIO.h>

//#if SUPPORTS_FILE_SYSTEM
//#include <storage/Entry.h>
//#endif /* SUPPORTS_FILE_SYSTEM */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ErrorMgr.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

//-------------------------------------------------------------------
//Internet style Schemes
//// "scheme://<user>:<password>@<host>:<port>/<url-path>"
//http, https, 
//	IPV6 addr style
//		http://[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:80/index.html


//	ftp, example ftp://myname@host.dom/%2Fetc/motd>

//	gopher, gopher://<host>:<port>/<gopher-path>
//			<gopher-path> could be <gophertype><selector>%09<search>%09<gopher+_string>

// nntp
//	nntp://<host>:<port>/<newsgroup-name>/<article-number>


//telnet, 
//		telnet://<user>:<password>@<host>:<port>/


//wais, 
//	wais://<host>:<port>/<database>
//	wais://<host>:<port>/<database>?<search>
//	wais://<host>:<port>/<database>/<wtype>/<wpath>


//file, 
//	file://vms.host.edu/disk$user/my/notes/note12345.txt>


//prospero, 
//	prospero://<host>:<port>/<hsoname>;<field>=<value>
//	seperator of name & value is ';'


//	nfs
//		nfs://server/d/e/f

// pop: after hostname, custom data
//		pop://<user>;auth=<auth>@<host>:<port>



//Internet style sccheme : but no query, or different style query
//	VEMMI - query begin , and query seperator ';'
//	 vemmi://<host>:<port>/<vemmiservice>;<attribute>=<value>
//	vemmi://mctel.fr/demo;$USERDATA=smith;account=1234




//	LDAP - query seperator ','
//			ldap://host.com:6666/o=University%20of%20Michigan,c=US??sub?(cn=Babs%20Jensen)
//			ldap:///??sub??bindname=cn=Manager%2co=Foo




//	IMAP - same till hostname
//		<imap://minbari.org/gray-council;UIDVALIDITY=385759045/;UID=20>




//Single ':' schemes :mailto, news

//	mailto: may have different syntax after : (rfc2368 )
//		mailto:?to=joe@example.com&cc=bob@example.com&body=hello
//		mailto:joe@example.com?cc=bob@example.com&body=hello
//		mailto:infobot@example.com?body=send%20current-issue

//	news:
//		news:comp.infosystems.www.misc


//data:
//	data:[<mediatype>][;base64],<data>
//	data:,A%20brief%20note
//	data:text/plain;charset=iso-8859-7,%be%fg%be



//cid and mid
//mid:960830.1639@XIson.com/partA.960830.1639@XIson.com
//-------------------------------------------------------------------

enum Scheme {
	kNoScheme, 
	kCustomScheme, 
	kHTTP, 
	kHTTPS, 
	kFile, 
	kFTP, 
	kNNTP,
	kTelnet, 
	kWAIS, 
	kMailTo, 
	kJavaScript,
	kAbout
};
//-------------------------------------------------------------------
const char *kSchemes[] = {
	"",		// No Scheme
	"UnSupported",
	"http",
	"https",
	"file",
	"ftp",
	"nntp",
	"telnet",
	"wais",
	"mailto",
	"javascript",
	"about",
	NULL
};
//-------------------------------------------------------------------
#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif // _SUPPORTS_NAMESPACE

//This structure MUST not have a pointer, which keeps data inside it.
	//Since when creating from SValue and from Another SUrl we just copy whole data from here
	//to the new one.
struct URLData {
	Scheme		fScheme;

	char		fQueryBegin;
	char		fQuerySeperator;
	char		fQueryAssignment;
	char		fQueryEnd;

	uint16_t	fPort;

	struct {
		bool	IsSupported		: 1;
		bool	IsHierarchal	: 1;
		bool	HasUserName		: 1;
		bool	HasPassword		: 1;
		bool	__pad0			: 4;	// must pad out to 8
	} flags;

	char fStrings[1];

};

static const char kQueryBeginChar='?' ;
static const char kQuerySeperatorChar='&' ;
static const char kQueryAssignmentChar='=' ;
static const char kQueryEndChar='\0' ;//none

#if _SUPPORTS_NAMESPACE
} }
//-------------------------------------------------------------------
#endif /* _SUPPORTS_NAMESPACE */
int LookupScheme(const char *scheme);
int LookupScheme(const char *scheme, int len);
char*	NormalizePathString(char *path);
//-------------------------------------------------------------------
enum field_offset {
	kAction,
	kScheme,
	kUser,
	kPassword,
	kHostname,
	kPath,
	kQuery,
	kFragment,
	kLastField
};

//-------------------------------------------------------------------
//
//	The format of fStrings is:
//	[Action] \0 //If set by user
//  [scheme] \0 //If not one of the schemes that we support internally
//	[user] '\0' 
//	[password] '\0' 
//	[hostname] '\0' 
//	[path] '\0' 
//	[query/javascript command] '\0' 
//	[fragment] '\0' 
//
//-------------------------------------------------------------------

//Do we have a built in support for this scheme
bool SUrl::IsSchemeSupported(const char *scheme)
{
	Scheme	tempScheme = (Scheme) LookupScheme(scheme);
	if(tempScheme == kCustomScheme)
		return false;
	else
		return true;
}
//-------------------------------------------------------------------
//If Burl does not have a built in support for a scheme
//	But if a scheme still follows the internet syntax for building Url
//	we can help build the URL.

//We build schemes like this
//"scheme://<user>:<password>@<host>:<port>/<url-path>?<query>&<query>"
//Any segement of URL is optional, except for scheme itself
//	By default 
//	? is used to begin query (queryBeginChar)
//	& is used to seperate two different queries (querySeperatorChar)
//  = is used as assignment char is query is of form name=value (queryAssignmentChar)
//-------------------------------------------------------------------
status_t	SUrl::SetInternetStyleSchemeSupport(char queryBeginChar, 
											char queryAssignmentChar,
											char querySeperatorChar)
{
	status_t status=B_OK;
	bool	urlToCopy = true;


	if(!IsValid() )
	{
		status = CreateUrlData( 0, NULL , NULL);
		urlToCopy = false;
	}

	if(status >= B_OK)
	{
		SString	oldUrlStr(AsString());

		fData->flags.IsSupported = true;
		fData->fQueryBegin = queryBeginChar;
		fData->fQuerySeperator = querySeperatorChar;
		fData->fQueryAssignment = queryAssignmentChar;

		if(urlToCopy)
			SetTo(oldUrlStr.String(), false);
	}


	return status;

}

//-------------------------------------------------------------------
SValue SUrl::AsValue() const
{
	SValue value;

	value.Assign(B_URL_TYPE, fData, fDataLen);
	return value;
}
//-------------------------------------------------------------------
SUrl::SUrl(const SValue &value, status_t *statusP)
	:	fData(0), fDataLen(0), fExtraSpace(0)
{
	status_t intStatus = SetTo(value);

	if (statusP != NULL)
		*statusP = intStatus;
}
//-------------------------------------------------------------------
SUrl::SUrl()
	:	fData(0), fDataLen(0), fExtraSpace(0)
{
}
//-------------------------------------------------------------------
SUrl::SUrl(const SUrl &url)
	:	fData(0), fDataLen(0), fExtraSpace(0)
{
	SetTo(url);
}
//-------------------------------------------------------------------
SUrl::SUrl(const char *urlString, bool escape_all)
	:	fData(0), fDataLen(0), fExtraSpace(0)
{
	if(urlString != NULL)
		SetTo(urlString, escape_all);
}
//-------------------------------------------------------------------
SUrl::SUrl(const SUrl &baseURL, const char *relativePath, bool escape_all)
	:	fData(0), fDataLen(0), fExtraSpace(0)
{
	if(relativePath != NULL)
		SetTo(baseURL, relativePath, escape_all);
	else
		SetTo(baseURL);
}
//-------------------------------------------------------------------
SUrl::SUrl(const char *scheme, const char *hostname, int port, bool escape_all, bool hierarchal )
	:	fData(0), fDataLen(0), fExtraSpace(0)
{
	if(!scheme || !(*scheme))
	{
		Reset();
		return;
	}

	status_t status = CreateUrlData( 0, scheme, NULL);

	if(status >= B_OK)
	{
		fData->fPort = port;
		if(hostname)
			status = SetHostName(hostname);

		if(fData->fScheme == kCustomScheme)
			fData->flags.IsHierarchal = hierarchal;
	}
}
//-------------------------------------------------------------------



//-------------------------------------------------------------------
SUrl::~SUrl()
{
	Reset();
}

//-------------------------------------------------------------------
inline char *append(char *start, const char *string)
{
	char *end = start;
	while (*string)
		*end++ = *string++;

	return end;
}
//-------------------------------------------------------------------
inline const char *get_nth_string(const char *buf, int index)
{
	const char *c = buf;
	while (index-- > 0)
		while (*c++ != '\0')
			;
	
	return c;
}
//-------------------------------------------------------------------
Scheme	ExtractScheme(const char *urlString, const char **schemeEndP)
{
	// Remove leading spaces
	const char *src = urlString;
	while (isspace(*src))
		src++;

	// Parse scheme
	*schemeEndP = strchr(src, ':');

	return (Scheme) LookupScheme(urlString, *schemeEndP - urlString);

}
//-------------------------------------------------------------------
inline char *append_decimal(char *start, int dec)
{
	char buf[11];
	buf[10] = '\0';
	char *c = &buf[10];
	while (dec > 0) {
		*--c = (dec % 10) + '0';
		dec /= 10;
	}

	return append(start, c);
}
//-------------------------------------------------------------------
// end what i may need to take out


inline bool IsHierarchalScheme(Scheme scheme)
{
	switch (scheme) {
	case kNoScheme: 
	case kCustomScheme: 
	case kMailTo: 
	case kJavaScript: 
	case kAbout:
		return false;

	case kWAIS: 
	case kFile:
	case kFTP:
	case kNNTP: 
	case kHTTP:
	case kHTTPS:
	case kTelnet: 
	default:
		return true;
	}
}
//-------------------------------------------------------------------
inline int HexToInt(const char *str)
{
    const int kHexDigits[] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,
        0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    };
    
	return (kHexDigits[static_cast<unsigned char>(str[0])] << 4) |
		kHexDigits[static_cast<unsigned char>(str[1])];
}
//-------------------------------------------------------------------
// "&" is valid in a query string, and should not be escaped
// there (RFC 1738, sec 2.2).  However, jeff seemed to think
// that "&" needed escaping (implicitly in the path part of a URL),
// and I'm wary of changing that.  RFC 1738 also says that it's
// OK to escape a valid character as long as you _don't_ want
// it to have special meaning (note that "&" does have special
// meaning in query strings, so we must leave it alone there), so
// letting "&"s be escaped in paths should not hurt anything.
// The upshot is separate validMaps and escapeMaps for paths
// and queries.  -- Laz, 2001-05-17.
static const unsigned int kValidInPath[] = {
	0x00000000, 0x4dfffff5, 0xffffffe1, 0x7fffffe2,
	0x00000000, 0x00000000, 0x00000000, 0x00000000
};
static const unsigned int kEscapeInPath[] = {
	0xffffffff, 0xbe30000a, 0x0000001e, 0x8000001d,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};

static const unsigned int kValidInQuery[] = {
	0x00000000, 0x4ffffff5, 0xffffffe1, 0x7fffffe2,
	0x00000000, 0x00000000, 0x00000000, 0x00000000
};
static const unsigned int kEscapeInQuery[] = {
	0xffffffff, 0xbe30000a, 0x0000001e, 0x8000001d,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};
//-------------------------------------------------------------------
inline bool IsValidURLChar(char c, const unsigned int *validMap)
{
	return (validMap[static_cast<unsigned char>(c) / 32] &
		(0x80000000 >> (static_cast<unsigned char>(c) % 32))) != 0;
}
//-------------------------------------------------------------------
inline bool NeedsEscape(char c, const unsigned int *escapeMap)
{
	return (escapeMap[static_cast<unsigned char>(c) / 32] &
		(0x80000000 >> (static_cast<unsigned char>(c) % 32))) != 0;
}

//-------------------------------------------------------------------
inline void UnescapeString(char *out, const char *in, size_t len)
{
	if (len > 0) {
		--len;
		while (*in && len-- > 0) {
			switch (*in) {
				case '%': {
					in++;
					char c = HexToInt(in);
					if (c == '\0')
						c = ' ';
					
					*out++ = c;
					in += 2;

					if(len > 2)
						len -= 2;
					else
						len =0;

					break;
				}
					
				case '+':
					*out++ = ' ';
					in++;
					break;
					
				default:
					*out++ = *in++;
			}
		}
		*out = '\0';
	}
}

//-------------------------------------------------------------------



/* static */
int SUrl::GetEscapedLength(const char *c, size_t inLen )
{
	int len = 0;
	while (*c && inLen) {
		// Note: this sometimes overestimates the length by a few
		// characters to be on the safe size.
		if (NeedsEscape(*c, kEscapeInPath))
			len += 3;
		else
			len++;

		c++;
		inLen--;
	}

	return len;
}


/* static */
char *SUrl::EscapePathString(bool escape_all, char *outString, const char *inString, size_t inLen)
{
	return EscapeString(escape_all, outString, inString, inLen, kValidInPath, kEscapeInPath);
}

/* static */
char *SUrl::EscapeQueryString(bool escape_all, char *outString, const char *inString, size_t inLen)
{
	return EscapeString(escape_all, outString, inString, inLen, kValidInQuery, kEscapeInQuery);
}

/* static */
char* SUrl::EscapeString(bool escape_all, char *outString, const char *inString, size_t inLen,
                        const unsigned int *validMap, const unsigned int *escapeMap)
{
	// Avertissement!
	// At one time, this code translated spaces to the '+' character.
	// This is actually wrong, as some servers depend on %20 (like www.terraserver.com).
	// Also, if inString is not escaped yet, escape '+' and '%'.
	const char *kHexDigits = "0123456789abcdef";

	if (escape_all) {
		// Replace invalid URL characters with escapes *and* escape out any valid
		// characters that are delimiters or escape sequences so they won't be confused
		// with the real ones.
		for (const char *c = inString; *c && inLen > 0; c++) {
			if (NeedsEscape(*c, escapeMap)) {
				*outString++ = '%';
				*outString++ = kHexDigits[(static_cast<unsigned char>(*c) >> 4) & 0xf];
				*outString++ = kHexDigits[static_cast<unsigned char>(*c) & 0xf];
			} else
				*outString++ = *c;
	
			inLen--;
		}
	} else {
		// Replace invalid URL characters with escapes.  
		for (const char *c = inString; *c && inLen > 0; c++) {
			if (IsValidURLChar(*c, validMap))
				*outString++ = *c;
			else {
				*outString++ = '%';
				*outString++ = kHexDigits[(static_cast<unsigned char>(*c) >> 4) & 0xf];
				*outString++ = kHexDigits[static_cast<unsigned char>(*c) & 0xf];
			}
	
			inLen--;
		}
	}

	*outString++ = '\0';
	return outString;
}

//-------------------------------------------------------------------
void ResetUrlData(URLData *newUrlData);
// Allocates memory for Url
//Must intialize scheme.
//If given a Url string, it can preallocate memory for UrlString. Takes care to allocate enough memory for escaped length
status_t SUrl::CreateUrlData(int32_t extraSpace, const char *scheme, const char *urlString, bool escape_all)
{
	URLData *newUrlData = NULL;
	SString	SchemeString;
	Scheme	tempScheme=kNoScheme;
	status_t	status=B_OK;
	int32_t		dataLen;

	//allocate space for scheme if needed
	if(scheme != NULL)
	{
		tempScheme = (Scheme) LookupScheme(scheme);
		if(tempScheme == kCustomScheme)
		{
			extraSpace += strlen(scheme);
			SchemeString.SetTo(scheme);
		}
	}
	else if(urlString)
	{
		const char *schemeEnd = NULL;
		tempScheme = ExtractScheme(urlString, &schemeEnd);


		if (schemeEnd == 0 || schemeEnd == urlString || tempScheme == kNoScheme) 
			status = B_INVALID_SCHEME;

		if(tempScheme == kCustomScheme && !escape_all)
			extraSpace += strlen(urlString);
		else
			extraSpace +=  GetEscapedLength(urlString);

		if(tempScheme == kCustomScheme)
		{
			SchemeString.SetTo(urlString, schemeEnd - urlString);
			extraSpace += SchemeString.Length();
		}
	}

	dataLen = ((sizeof(URLData) - 1) + kLastField);		// -1 for URLData::fStrings
	newUrlData  =  (URLData*)malloc(dataLen + extraSpace);

	if(newUrlData == NULL)
		return sysErrNoFreeRAM;

	ResetUrlData(newUrlData);
	fData = newUrlData;
	fDataLen = dataLen;
	fExtraSpace = extraSpace;

#if BUILD_TYPE == BUILD_TYPE_DEBUG
	memset(((uint8_t *)fData + fDataLen), 0xff, fExtraSpace);
#endif

	fData->fScheme = tempScheme;
	if(tempScheme == kCustomScheme)
		status = SetNthString(kScheme, SchemeString.String() );
	else
		fData->flags.IsSupported = true;

	fData->flags.IsHierarchal =  IsHierarchalScheme(fData->fScheme);
	
	return status;

}
//-------------------------------------------------------------------
// Creates UrlData and allocates memory for it.
//	ALl other field except for actual Url string, it can allocate from another UrlData
status_t SUrl::CreateUrlDataFromUrl(int32_t extraSpace, const URLData *refUrlData, const char *urlString, bool escape_all)
{
	status_t	status=B_OK;
	if(refUrlData == NULL)
	{
		DbgOnlyFatalError("Invalid Input");
		return -1;
	}

	if(urlString != NULL)
	{
		status = CreateUrlData(extraSpace, NULL, urlString, escape_all);
	}
	else if(refUrlData->fScheme == kCustomScheme)
		status = CreateUrlData(extraSpace, get_nth_string(refUrlData->fStrings, kScheme), NULL);
	else
		status = CreateUrlData(extraSpace, kSchemes[refUrlData->fScheme], NULL);

	if(status >= B_OK)
	{
		fData->fScheme = refUrlData->fScheme;
		fData->fQueryBegin = refUrlData->fQueryBegin;;
		fData->fQuerySeperator = refUrlData->fQuerySeperator;;
		fData->fQueryAssignment = refUrlData->fQueryAssignment; ;
		fData->fQueryEnd = refUrlData->fQueryEnd;
		fData->fPort = refUrlData->fPort;
		fData->flags.IsSupported = refUrlData->flags.IsSupported;
		fData->flags.IsHierarchal = refUrlData->flags.IsHierarchal;
		fData->flags.HasUserName = false;
		fData->flags.HasPassword = false;
	}

	return status;
}
//-------------------------------------------------------------------
//Reset UrlData to default values
void ResetUrlData(URLData *newUrlData)
{
	newUrlData->fScheme = kNoScheme;
	newUrlData->fQueryBegin = kQueryBeginChar;
	newUrlData->fQuerySeperator = kQuerySeperatorChar;
	newUrlData->fQueryAssignment = kQueryAssignmentChar;
	newUrlData->fQueryEnd = kQueryEndChar;
	newUrlData->fPort = 0;
	newUrlData->flags.IsSupported =  false;
	newUrlData->flags.IsHierarchal = false;
	newUrlData->flags.HasUserName = false;
	newUrlData->flags.HasPassword = false;

	memset(newUrlData->fStrings,0, kLastField);
}


//-------------------------------------------------------------------
//buf : Begining of multi-string-string
//Sets the String at Nth location.
//Each component of Url is saved as String, in the order shown at the top of the file.
status_t SUrl::SetNthString(int16_t setIndex, const char *newString, int32_t newStringLen)
{
	// find start of string to be replaced
	const char *start = get_nth_string(fData->fStrings, setIndex);

	// determine difference in old/new string lengths
	if (newStringLen == -1)
		newStringLen = strlen(newString);

	int oldStringLen = strlen(start);
	int diff = (newStringLen - oldStringLen);

	// adjust buffer size as necessary
	if ((diff < 0 && (fExtraSpace - diff) > (2 * 1024)) ||
		(diff > 0 && ((size_t)diff > fExtraSpace)))
	{
		// resize buffer
		URLData *newData = (URLData *)realloc(fData, (fDataLen + diff));
		if (newData == NULL)
			return sysErrNoFreeRAM;
		fData = newData;
		fExtraSpace = 0;

	} else {
		// we won't resize - just adjust cached padding count
		fExtraSpace -= diff;
	}

	if (diff != 0) {
		// make room for new string
		int stringsLen = (fDataLen - (sizeof(URLData) - 1));
		memmove((void *)(start + newStringLen), (start + oldStringLen),
					(stringsLen - ((start + oldStringLen) - fData->fStrings)));

		// keep books
		fDataLen += diff;
	}

	// put new string into buffer
	memcpy((void *)start, newString, newStringLen);

#if BUILD_TYPE == BUILD_TYPE_DEBUG
	memset(((uint8_t *)fData + fDataLen), 0xff, fExtraSpace);
#endif

	return B_OK;
}

//-------------------------------------------------------------------
void SUrl::Reset()
{
	free(fData);
	fData = 0;
	fDataLen = 0;
	fExtraSpace = 0;
}
//-------------------------------------------------------------------
bool SUrl::IsValid() const
{
	return (fData != 0 && fData->fScheme != kNoScheme);
}
//-------------------------------------------------------------------
SUrl& SUrl::operator=(const SUrl &url)
{
	SetTo(url);

	return *this;
}
//-------------------------------------------------------------------
SUrl& SUrl::operator=(const char *urlString)
{
	SetTo(urlString, false);

	return *this;
}
//-------------------------------------------------------------------
status_t SUrl::SetHostName(const char *hostname)
{
	return SetNthString(kHostname, hostname);
}
//-------------------------------------------------------------------
status_t SUrl::SetPort(int port)
{
	status_t status=B_OK;

	if(fData == NULL)
		status = B_INVALID_URL;

	if(status >= B_OK)
		fData->fPort = port;

	return status;
}
//-------------------------------------------------------------------
status_t SUrl::SetUserName(const char *user)
{
	status_t err;

	if(!IsValid())
		err = B_INVALID_URL;
	else {
		if (user == NULL)
		{
			// eliminate username
			err = SetNthString(kUser, NULL, 0);
			if (err == B_OK)
				fData->flags.HasUserName = false;
		}
		else
		{
			// (re-)set username
			err = SetNthString(kUser, user);
			if (err == B_OK)
				fData->flags.HasUserName = true;
		}
	}

	return err;
}

//-------------------------------------------------------------------

status_t SUrl::SetPassword(const char *password)
{
	status_t err;

	if(!IsValid())
		err = B_INVALID_URL;
	else {
		if (password == NULL)
		{
			// eliminate password
			err = SetNthString(kPassword, NULL, 0);
			if (err == B_OK)
				fData->flags.HasPassword = false;
		}
		else
		{
			// (re-)set password
			err = SetNthString(kPassword, password);
			if (err == B_OK)
				fData->flags.HasPassword = true;
		}
	}

	return err;
}

//-------------------------------------------------------------------

status_t SUrl::SetFragment(const char *fragment)
{
	if(!IsValid())
		return B_INVALID_URL;

	if (fData->fScheme == kJavaScript) return B_OK;

	return SetNthString(kFragment, fragment);
}

//-------------------------------------------------------------------
status_t SUrl::SetPath(const char *path, bool escape_all)
{
	if(!IsValid())
		return B_INVALID_URL;

	return SetPathInternal(path, 0, escape_all);
}
//-------------------------------------------------------------------
//NOTE - what rfc's have to say about / between hostname and path
// RFC 1738 sec 3.1 -------------------------------
//	url-path
//        The rest of the locator consists of data specific to the
//       scheme, and is known as the "url-path". It supplies the
//        details of how the specified resource can be accessed. Note
//        that the "/" between the host (or port) and the url-path is
//       NOT part of the url-path.

//-------------------------------------------------------------------
status_t SUrl::SetPathInternal(const char *path, int32_t length, bool escape_all)
{
	if(!length)
		length = strlen(path);
	int32_t pathLength = GetEscapedLength(path, length) + 1+1; //+1may be for '/'
	char *escPathStr = (char*)malloc(pathLength);
	if (escPathStr == 0) return B_NO_MEMORY;
	char *completePath = escPathStr;

	if(path[0] == '/')
	{
		path++;
		length--;
	}

	EscapePathString(escape_all, escPathStr, path, length);
	NormalizePathString(completePath);

	status_t error = SetNthString(kPath, completePath);
	free(completePath);
	return error;

	
	return B_ERROR;
}
//-------------------------------------------------------------------
//Set the Url to the new String
status_t SUrl::SetTo(const char *urlString, bool escape_all)
{

	status_t status = B_OK;

	if(urlString == NULL)
	{
		Reset();
		return B_OK;
	}
	
	if(fData == NULL)
		status= CreateUrlData(0, NULL, urlString, escape_all);
	else
	{
		URLData *oldUrlData = fData;
		status= CreateUrlDataFromUrl(0, oldUrlData, urlString, escape_all);
		free(oldUrlData);
	}


	if(status < B_OK)
	{
		Reset();
		return status;
	}
	const char *schemeEnd;

	ExtractScheme(urlString, &schemeEnd);
	switch (fData->fScheme) {
	case kMailTo:
		status = ParseMailToURL(schemeEnd + 1, escape_all);
		break;
	case kJavaScript:
	case kAbout:
		status = ParseJavaScriptURL(schemeEnd + 1);
		break;

	case kCustomScheme:
		if (schemeEnd)
		{
			int32_t urlLength = strlen(schemeEnd);
			if(schemeEnd && urlLength > 2 && (
				(schemeEnd[1] == '/' && schemeEnd[2] == '/') ||
				(schemeEnd[1] == '\\' && schemeEnd[2] == '\\')))
				fData->flags.IsHierarchal = true;

			if(fData->flags.IsSupported)
				status = ParseInternetURL(schemeEnd + 1, escape_all);
			else
			{
				if(fData->flags.IsHierarchal)
					schemeEnd += 3;//skip past ://
				else
					schemeEnd += 1; //skip past :
				status = SetNthString(kHostname, schemeEnd);
			}
		}
		else
		{
			status = B_INVALID_URL;
		}
		break;

	default:
		// Treat as a path type URL (http, https, ftp, beos, rtsp, etc...
		status = ParseInternetURL(schemeEnd + 1, escape_all);
		break;
	}
	return status;
}
//-------------------------------------------------------------------
status_t SUrl::SetTo(const SUrl &url)
{
	if (&url == this)
		return B_OK;
		
	// wipe old data
	if (fData != NULL)
		Reset();

	if (!url.IsValid())
		return B_INVALID_URL;

	// duplicate 'url's data
	fData = (URLData *)malloc(url.fDataLen);
	if (fData == NULL)
		return sysErrNoFreeRAM;
	memcpy(fData, url.fData, url.fDataLen);
	fDataLen = url.fDataLen;
	fExtraSpace = 0;

	return B_OK;
}
//-------------------------------------------------------------------
status_t SUrl::SetTo(const SValue &value)
{
	status_t err;

	// wipe old data
	if (fData != NULL)
		Reset();

	if (value.IsDefined() && value.Type() == B_URL_TYPE && value.Length() > 0) 
	{
		ssize_t len = value.Length();

		fData = (URLData*) malloc(len);

		if(fData == NULL)
			err = B_NO_MEMORY;
		else {
			memcpy(fData, value.Data(), len);
			fDataLen = len;
			fExtraSpace = 0;

			if(fData->fScheme == kNoScheme)
				err = B_INVALID_URL;
			else
				err = B_OK;
		}
	}
	else
	{
		SString str = value.AsString(&err);
		if (err == B_OK)
		{
			err = SetTo(str.String());
		}
		else
		{
			err = B_INVALID_URL;
		}
	}

	return err;
}
//-------------------------------------------------------------------

status_t SUrl::SetTo(const SUrl &baseURL, const char *relativePath, bool escape_all)
{
	// First, scan the beginning of the relative path and determine the basic form:
	//	scheme://  		-- Absolute path.  Ignore base url.
	//	scheme:path		-- relative path
	//	//machine/path 	-- It only inherits the scheme
	status_t status = B_OK;
	const char *s = relativePath;
	enum {
		kScanScheme,
		kScanFirstSlash,
		kScanSecondSlash,
		kScanLeadingSlash,
		kScanDone,
	} state = kScanScheme; 

	if (!baseURL.IsValid()) 
	{
		Reset();
		return B_INVALID_URL;
	}
	//We don't do anything with custom URL
	if( relativePath == NULL ||
			(LookupScheme(baseURL.GetScheme()) == kCustomScheme && !baseURL.fData->flags.IsHierarchal))
	{
		status = SetTo( baseURL);
		return status;
	}

	while (state != kScanDone && *s != '\0') {
		switch (state) {
		case kScanScheme:
			if (*s == ':')
				state = kScanFirstSlash;
			else if (*s == '/')
				state = kScanLeadingSlash;
			else if (!isalpha(*s))
				state = kScanDone;
			
			break;			

		case kScanFirstSlash:
			if (*s == '/')
				state = kScanSecondSlash;
			else if (s > relativePath + 1) 
			{
				int scheme = LookupScheme(relativePath, s - relativePath - 2);
				if (baseURL.fData->fScheme == scheme && IsHierarchalScheme((Scheme) scheme)) 
					//If both schemes are same and this scheme is hieriarchal, then we have a relative component
					return SetToInternal(baseURL, s, escape_all);	// This is of the form scheme:path.
				else
					return SetTo(relativePath, escape_all);	// Something like mailto.  Treat as absolute
			} 
			else 
			{
				Reset();
				return B_INVALID_URL;
			}
			
			break;
		
		case kScanSecondSlash:
			if (*s == '/')
				return SetTo(relativePath, escape_all);
					// The "relative" path is probably a fully qualified URL.
					// SetTo that.
			
			// This looks like http:/relative path. Strip off scheme
			return SetToInternal(baseURL, s - 1, escape_all);
		
		case kScanLeadingSlash:
			if (*s == '/') {
				// Special case.  The url is of the form //machine/stuff
				// The scheme is missing, so use the scheme from the base URL
				SString tmp;
				tmp << baseURL.GetScheme() << ":" << relativePath;
//				printf("%s\n", tmp.String());
				return SetTo(tmp.String(), escape_all);
			}

			state = kScanDone;
			break;
		
		default:
			;
		}

		s++;
	}

	return SetToInternal(baseURL, relativePath, escape_all);
}
//-------------------------------------------------------------------
const char *ExtractQueryString(const char *src, int *query_length, char qChar);
//-------------------------------------------------------------------
status_t SUrl::SetToInternal(const SUrl &baseURL, const char *relativePath, bool escape_all)
{
	status_t status=B_OK;

	Reset();
	status = CreateUrlDataFromUrl(0, baseURL.fData, NULL);

	SetUserName(baseURL.GetUserName());
	SetPassword(baseURL.GetPassword());
	SetHostName(baseURL.GetHostName());

	// Figure out the unescaped length of the relative path
	int relPathLength = 0;
	char qbegin;
	if(baseURL.fData)
		qbegin = baseURL.fData->fQueryBegin;
	else
		qbegin = kQueryBeginChar; // '?'

	for (const char *tmp = relativePath; *tmp != '\0' &&  *tmp != qbegin && *tmp != '#'; tmp++)
		relPathLength++;


	SString	path(baseURL.GetPath());//Start with path of base URL.
		
	if (relPathLength > 0) 
	{
		if (relativePath[0] == '/') 
		{
			// If the relative path begins with '/', treat it as beginning from root.
			// Just append it onto hostname.
			path.SetTo(relativePath, relPathLength);
		} 
		else 
		{
			// Relative path. 
			int32_t lastSlashOffset = path.FindLast('/');
			if(lastSlashOffset == -1)
				path.SetTo('/', 1);// looks redundant, but handles case where base path has no '/'
			else
				path.Truncate(lastSlashOffset+1);

			path.Append(relativePath, relPathLength);
		}
	} 


	SetPath(path, escape_all);
	// Copy the query.
	const char *src = relativePath + relPathLength;
	int	queryLength;
	const char *query = ExtractQueryString(src, &queryLength, qbegin);
	if(query != NULL && queryLength)
	{
		status = SetQueryInternal(query, queryLength, escape_all);
		src = query+queryLength;
	}

	//Get the fragment if any
	if (src && *src == '#')
	{
		src++;
		status = SetFragment(src);
	}

	return status;
}

//-------------------------------------------------------------------


SString SUrl::AsString() const
{
	SString str;
	const char *dataPtr;

	if (!IsValid())
	{
		return B_EMPTY_STRING;
	}

	str.BeginBuffering();

	str << GetScheme() << ':';

	if (fData->flags.IsHierarchal)
		str << "//";

	if(fData->fScheme == kCustomScheme && !fData->flags.IsSupported)
	{
		str << get_nth_string(fData->fStrings, kHostname);
		str.EndBuffering();
		return str;
	}

	// Javascript
	if (fData->fScheme == kJavaScript || fData->fScheme == kAbout) {
		str << get_nth_string(fData->fStrings, kQuery);
		str.EndBuffering();
		return str;
	}

	if (fData->flags.HasUserName || fData->flags.HasPassword) {
		// insert empty username as necessary, per RFC1738, section 3.1
		const char *username = get_nth_string(fData->fStrings, kUser);
		if (*username)
			str.Append(username);

		if (fData->flags.HasPassword) {
			str += ':';

			const char *password = get_nth_string(fData->fStrings, kPassword);
			if (*password)
				str.Append(password);
		}

		dataPtr = get_nth_string(fData->fStrings, kHostname);
		if( (dataPtr && *dataPtr)|| fData->fPort)//hostname OR port set
			str += '@';
	}

	// Hostname
	dataPtr = get_nth_string(fData->fStrings, kHostname);
	if(	dataPtr && *dataPtr)
		str.Append(dataPtr);

	if (fData->fPort != 0)
		str << ':' << (unsigned long) fData->fPort;

	if (fData->flags.IsHierarchal)
		str <<'/' ; //seperate hostname and path

	//If there is a path append it
	dataPtr = get_nth_string(fData->fStrings, kPath);
	if(dataPtr && *dataPtr)
		str.Append(dataPtr);
	
	
	dataPtr = get_nth_string(fData->fStrings, kQuery);
	if(dataPtr && *dataPtr)
	{//There is an query
		str += fData->fQueryBegin ;
		str.Append(dataPtr);
	}

	dataPtr = get_nth_string(fData->fStrings, kFragment);
	if(dataPtr && *dataPtr)
	{//There is an Fragment
		str += '#' ;
		str.Append(dataPtr);
	}
	
	str.EndBuffering();

	return str;
}

//-------------------------------------------------------------------

status_t SUrl::SetAction(const char *action)
{
	return SetNthString(kAction, action);

}

const char *SUrl::GetAction() const
{
	return get_nth_string(fData->fStrings, kAction);

}

//-------------------------------------------------------------------
const char* SUrl::GetScheme() const
{
	if (fData == 0)
		return "";

	if(fData->fScheme != kCustomScheme)
		return kSchemes[fData->fScheme];
	else
	{
		return get_nth_string(fData->fStrings, kScheme);
	}

}
//-------------------------------------------------------------------
const char* SUrl::GetUserName() const
{
	if (fData == NULL || fData->flags.HasUserName == false)
		return NULL;

	return get_nth_string(fData->fStrings, kUser);
}
//-------------------------------------------------------------------
const char* SUrl::GetPassword() const
{
	if (fData == NULL || fData->flags.HasPassword == false)
		return NULL;

	return get_nth_string(fData->fStrings, kPassword);
}
//-------------------------------------------------------------------
const char* SUrl::GetHostName() const
{
	if (fData == 0 )
		return "";

	return get_nth_string(fData->fStrings, kHostname);
}
//-------------------------------------------------------------------
unsigned short SUrl::GetPort() const
{
	return fData ? fData->fPort : 0;
}
//-------------------------------------------------------------------
const char* SUrl::GetPath() const
{
	if ( fData == 0 || 
			(strcasecmp(GetScheme(), "mailto") == 0))
		return "";

	return get_nth_string(fData->fStrings, kPath);
}
//-------------------------------------------------------------------
const char* SUrl::GetExtension() const
{
	if(fData->fScheme == kCustomScheme)
		return "";

	const char *extension = 0;
	for (const char *path = GetPath(); *path; path++) {
		switch (*path) {
		case '/':
			extension = 0;
			break;
		case '.':
			extension = path + 1;
			break;
		}
	}

	return extension ? extension : "";
}
//-------------------------------------------------------------------
void SUrl::GetUnescapedPath(char *out, int size) const
{
	UnescapeString(out, GetPath(), size);
}
//-------------------------------------------------------------------
void SUrl::GetUnescapedQuery(char *out, int size) const
{
	UnescapeString(out, GetQuery(), size);
}
//-------------------------------------------------------------------
const char* SUrl::GetQuery() const
{
	if (fData == 0)
		return "";

	return get_nth_string(fData->fStrings, kQuery);
}
//-------------------------------------------------------------------
const char* SUrl::GetFragment() const
{
	if (fData == 0)
		return "";

	return get_nth_string(fData->fStrings, kFragment);
}
//-------------------------------------------------------------------
status_t SUrl::SetQuery(const char *query, bool escape_all)
{
	if(!IsValid())
		return B_INVALID_URL;

	return SetQueryInternal(query, 0, escape_all);
}
//-------------------------------------------------------------------
status_t SUrl::SetQueryInternal(const char *query, int32_t length, bool escape_all )
{
	status_t	error;
	SString escQueryString;

	if(length)
	{
		SString OrigQuery(query, length);
		error = ComposeEscapedQuery(OrigQuery, NULL, escape_all, &escQueryString);
	}
	else
		error= ComposeEscapedQuery(query, NULL, escape_all, &escQueryString);

	if(error >= B_OK)
		error = SetNthString(kQuery, escQueryString);
	
	return error;
}
//-------------------------------------------------------------------
status_t SUrl::ComposeEscapedQuery(const char *name, const char *value, 
								   bool escape_all, SString *outStrP) const
{
	if(outStrP == NULL || !name)
		return B_ERROR;

	if (fData->fScheme == kJavaScript || 
		fData->fScheme == kAbout || 
		fData->fScheme == kMailTo ||
		fData->fScheme == kCustomScheme)
	{

		outStrP->SetTo(name);

		if(value && strlen(value))
		{
			outStrP->Append(fData->fQueryAssignment,1);
			outStrP->Append(value);
		}
	}
	else
	{
		SString	escQueryName;
		SString	escQueryValue;

		int esc_nameLen = GetEscapedLength(name) + 1;
		char *esc_buf = escQueryName.LockBuffer(esc_nameLen);
		EscapeQueryString(escape_all, esc_buf, name);
		escQueryName.UnlockBuffer();

		outStrP->SetTo(escQueryName);

		int esc_valLen = 0;
		if(value && strlen(value))
		{
			esc_valLen = GetEscapedLength(value) + 1;
			esc_buf = escQueryValue.LockBuffer(esc_valLen);
			EscapeQueryString(escape_all, esc_buf, value);
			escQueryValue.UnlockBuffer();

			outStrP->Append(fData->fQueryAssignment,1);
			outStrP->Append(escQueryValue);
		}
	}

	return B_OK;
}
//-------------------------------------------------------------------
status_t SUrl::UnEscapedQuery(const char *query, int32_t length, SString *outStrP) const 
{
	if(length == 0)
		length = strlen(query);

	if (fData->fScheme == kJavaScript || 
		fData->fScheme == kAbout || 
		fData->fScheme == kMailTo ||
		fData->fScheme == kCustomScheme)
	{
		outStrP->SetTo(query, length);
	}
	else
	{
		//Unescape Value and Return in SString
		char *unEscBuf = outStrP->LockBuffer(length+1);
		UnescapeString(unEscBuf,query,length+1);
		outStrP->UnlockBuffer();
	}

	return B_OK;
}

//-------------------------------------------------------------------

status_t SUrl::AddQueryParameter(const char *name, const char *value, bool escape_all)
{
	status_t	error = B_OK;

	if (!IsValid())
		return B_INVALID_URL;

	if(!name)
		return B_ERROR;


	SString	queryString;
	ComposeEscapedQuery(name, value, escape_all, &queryString);

	// Get the length of the current query portion -- if there
	// is already something here, then we need to also add an '&'
	// to separate our new parameter.
	int intialQueryLen = strlen(GetQuery());

	SString newQuery;
	if(intialQueryLen)
	{
		newQuery.SetTo(GetQuery());
		newQuery += fData->fQuerySeperator;//'&'

	}

	newQuery.Append(queryString);

	//Now set the complete Query 
	error = SetNthString(kQuery, newQuery);

	return B_OK;
}

//--------------------------------------------------------------------------
status_t SUrl::GetQueryParameter(const char* name, SString* out_value) const
{
	int32_t	nameLength;
	int32_t	valueLength;
	
	const char *queryStrStart =GetQuery(); 
	const char *nameQ = NULL;
	const char *valueQ = NULL;
	status_t	error = GetQueryParameterInternal(queryStrStart, name, &nameQ, &nameLength, &valueQ, &valueLength);

	out_value->SetTo("");

	if(error != B_OK)
		return error;

	//Unescape Value and Return in SString
	UnEscapedQuery(valueQ, valueLength, out_value);

	return error;
}


//--------------------------------------------------------------------------
status_t SUrl::GetQueryParameterInternal(const char *query, const char *name, 
										 const char **namePP, int32_t *nameLengthP,
										 const char **valuePP, int32_t *valueLengthP) const
{

	status_t	status = B_ERROR;

	if(!query || !name || !namePP || !nameLengthP || !valuePP || !valueLengthP)
		return status;

	SString buffer;
	const char *param_start = query;
	const char *param_name_end = NULL;
	const char *value_end = NULL;
	const char *value_start = NULL;

	char qSep = fData->fQuerySeperator;
	char qAssign = fData->fQueryAssignment;

	while(*query)
	{
		param_start = query;


		// Search for end of parameter name
		for(; *query && *query!= qSep && *query != qAssign; query++ ) {}

		param_name_end = query;

		{//did we find a match
			// Unescape the parameter name for comparison
			//DOLATER OPTIM : We could escape the input, that way we don't have to unescapoe all the length
				//CATCH : we don't know if we escaped the original name or not
			UnEscapedQuery(param_start, param_name_end-param_start,	&buffer);
		}

		if(buffer.ICompare(name) == 0)
		{
			status = B_OK; //found
			*namePP = param_start;
			*nameLengthP = param_name_end-param_start;
		}

		if(*query == '\0' || *query == qSep)//if *query == '&' :There is no Value, next query starts
		{	//There is no value
			value_start = NULL;
			value_end = NULL;
		}
		else //if(*query == '=')//get the value
		{
			// Search for end of value
			value_start = query+1;
			for(query++;*query && *query!=qSep; query++) {}
			value_end = query;
		}

		if(status == B_OK)
			break;//we found what we were looking for

		if(*query)
			query++;//Move on to next query
	}


	if(status == B_OK)
	{
		*valuePP = value_start;
		*valueLengthP = value_end - value_start;
	}
	else
	{
		*nameLengthP = 0;
		*valueLengthP = 0;
		*namePP = NULL;
		*valuePP = NULL;
	}

	return status;
}
//--------------------------------------------------------------------------
status_t SUrl::RemoveQueryParameter(const char *name)
{
	int32_t	nameLength;
	int32_t	valueLength;
	
	const char *queryStrStart =GetQuery(); 
	const char *nameQ = NULL;
	const char *valueQ = NULL;
	status_t	error = GetQueryParameterInternal(queryStrStart, name, &nameQ, &nameLength, &valueQ, &valueLength);

	if(error != B_OK )//query not found. Nothing todo
		return B_OK;

	//Create new complete query string, removing the query to be removed.
	SString newQStr;
	if(nameQ)
	{
		bool	lastQuery=false;
		bool	firstQuery=false;
		const char	*part2Str=NULL;


		//Is there another query behind
		if(valueLength && valueQ)
		{
			if( *(valueQ+valueLength) == 0)
				lastQuery = true;
		}
		else
		{
			valueQ = nameQ+nameLength;//end of query
			valueLength= 0;

			if( *(nameQ+nameLength) == 0)
				lastQuery = true;
		}

		if(queryStrStart == nameQ)
		{
			firstQuery = true;
		}


		//Is 1st Query being removed
		if(firstQuery && lastQuery)//There was only one query
			newQStr.SetTo("");
		else
		{
			if(!firstQuery)
			{
				//Copy all queries before the one being removed: Part I
				newQStr.SetTo(queryStrStart, nameQ-queryStrStart-1); //don't copy &
				part2Str = valueQ+valueLength;//get & from 2nd path
			}
			else
				part2Str = valueQ+valueLength+1; //skip past '&'

			if(!lastQuery)
			{	//Part II : Copy all queries after the one being removed
				
				int32_t	part2Len = strlen(queryStrStart)-(valueQ-queryStrStart+valueLength);
				newQStr.Append(part2Str, part2Len);
			}
		}
		
		return SetNthString(kQuery, newQStr);
	}

	return B_ERROR;
}
//--------------------------------------------------------------------------
status_t SUrl::ReplaceQueryParameter(const char *name, const char *value, bool addIfNotPresent, bool escape_all)
{
	int32_t	nameLength;
	int32_t	valueLength;
	
	const char *queryStrStart =GetQuery(); 
	const char *nameQ = NULL;
	const char *valueQ = NULL;
	status_t	error = GetQueryParameterInternal(queryStrStart, name, &nameQ, &nameLength, &valueQ, &valueLength);

	if(error != B_OK)
	{//ADd the new query
		if(addIfNotPresent)
		{
			AddQueryParameter(name, value, escape_all);
			return B_OK;
		}
		else
			return error;
	}

	//Query name is already in. We only need to replace Query value

	if(nameQ)
	{
		SString	newQStr;
		bool	lastQuery=false;
		const char	*part2Str=NULL;

		//Is there another query behind
		if(valueLength && valueQ)
		{
			if( *(valueQ+valueLength) == 0)
				lastQuery = true;
		}
		else
		{
			valueQ = nameQ+nameLength;//end of query
			valueLength= 0;

			if( *(nameQ+nameLength) == 0)
				lastQuery = true;
		}

		//Part I
		//Copy all queries before the one getting replaced including the name of one getting replaced.
		newQStr.SetTo(queryStrStart, nameQ-queryStrStart+nameLength);

		if(value && strlen(value))
		{
			SString escQueryValue;
			newQStr += fData->fQueryAssignment;

			//esc_valLen = GetEscapedLength(value) + 1;
			//char * esc_buf = escQueryValue.LockBuffer(esc_valLen);
			//EscapeQueryString(escape_all, esc_buf, value);
			//escQueryValue.UnlockBuffer();
			ComposeEscapedQuery(value, NULL, escape_all, &escQueryValue);
			newQStr.Append(escQueryValue);
		}
		

		//Part II
		if(!lastQuery)
		{// Copy all queries after the one being replaced
			part2Str = valueQ+valueLength;//get & from 2nd path
			int32_t	part2Len = strlen(queryStrStart)-(valueQ-queryStrStart+valueLength);
			newQStr.Append(part2Str, part2Len);
		}

		error =  SetNthString(kQuery, newQStr);
	}
	
	return error;
}


//-------------------------------------------------------------------
int LookupScheme(const char *scheme)
{
	if(scheme == NULL || scheme[0] == 0)
		return kNoScheme;

	for (int i = 0; kSchemes[i] != 0; i++)
		if (strcasecmp(kSchemes[i], scheme) == 0)
			return i;

	return kCustomScheme;
}
//-------------------------------------------------------------------
int LookupScheme(const char *scheme, int len)
{
	if(scheme == NULL || scheme[0] == 0 || len == 0)
		return kNoScheme;

	for (int i = 0; kSchemes[i] != 0; i++)
		if (strncasecmp(kSchemes[i], scheme, len) == 0)
			return i;

	return kCustomScheme;
}


//-------------------------------------------------------------------

void SUrl::Print(SString *OutStr) const
{
	//SString tmp(AsString());
	SString dump;

	dump.BeginBuffering();
	dump.Append("Url as String :");
	dump.Append(AsString());
	dump += '\n';

	dump << "Scheme :" << GetScheme();
	dump <<"\nHostname :" << GetHostName();
	dump <<"\nPath :" << GetPath();
	dump <<"\nQuery :" << GetQuery() ;
	dump <<"\nUser :" << GetUserName() ;
	dump <<"\nPassword :" << GetPassword() ;
	dump <<"\nPort :" << GetPort();
	dump <<"\nFragment :" << GetFragment() <<"\n";
	dump.EndBuffering();

	bout << dump << endl;

	if(OutStr != NULL)
		OutStr->SetTo(dump);
}
//-------------------------------------------------------------------

//-------------------------------------------------------------------
// As in GenerateHash, this ignores fragment, user, password, and query method
// The path and query are case sensitive.  The scheme and hostname are not.
bool SUrl::Equals(const SUrl &url) const
{
	if (!IsValid() || !url.IsValid())
		return false;

	return fData->fScheme == url.fData->fScheme
		&& fData->fPort == url.fData->fPort
		&& strcasecmp(GetHostName(), url.GetHostName()) == 0
		&& strcmp(GetPath(), url.GetPath()) == 0
		&& strcmp(GetQuery(), url.GetQuery()) == 0;
}

bool SUrl::Equals(const char *str) const
{
	SUrl tmp(str);
	return Equals(tmp);
}

bool SUrl::operator==(const char *urlString) const
{
	return Equals(urlString);	
}

bool SUrl::operator==(const SUrl &url) const
{
	return Equals(url);
}

bool SUrl::operator!=(const SUrl &url) const
{
	return !Equals(url);
}

//-------------------------------------------------------------------
//Internet scheme URL as defined in RFC1738 sec 3.1
//Inter scheme has this pattern
// "scheme://<user>:<password>@<host>:<port>/<url-path>"
//User and password are optional


status_t SUrl::ParseInternetURL(const char *_src, bool escape_all)
{
	status_t status=B_OK;
	const char *src = _src;

	if(fData->flags.IsHierarchal)
	{
		if (src[0] != '/' || src[1] != '/') 
		{
			Reset();
			return B_ERROR;
		}
		src += 2;
	}

	const char *atSign = strchr(src, '@');
	const char *firstSlash = strchr(src, '/');
	const char *begin;

	if (atSign != 0 && (firstSlash == 0 || firstSlash > atSign))
	{//we have user name
		begin = src;
		while(*src && *src != ':' && *src != '@') src++;

		SetNthString(kUser, begin, src - begin);
		fData->flags.HasUserName = true;

		if(*src && *src == ':')
		{//We have password
			src++;
			begin = src;
			while(*src && *src != '@')	src++;
			
			SetNthString(kPassword, begin, src - begin );
			fData->flags.HasPassword = true;
		}
		src++;//skip past @
	}//done with user name and password

	//get hostname
	begin = src;

	char qBegin = fData->fQueryBegin;

	if(*src == '[')
	{//we have an IPV6 addr, look for closing ']'
		while(*src != '\0' && *src != ']') src++;
		//skip past ]
		src++;
	}
	else
        while (*src != ':' && *src != '/' && *src != '\0' && *src != qBegin) src++;

	if (src > begin)
	{
		SetNthString(kHostname, begin, src - begin );
	}

	// port
	if(*src == ':')
	{
		src++;
		SetPort(atoi(src));
		while (*src != '/' && *src != '\0' && *src != qBegin) src++;
	}

	//get to end of path
	begin = src;
	while(*src != '#' && *src != '\0'  && *src != qBegin) 	src++;

	//path
	if(src > begin)
	{//we have a path
		status = SetPathInternal(begin, src - begin, escape_all);
	}


	//query
	if(*src == qBegin)
	{
		src++; //skip past ?
		begin = src;
		while(*src != '#' && *src != '\0') src++;

		status = SetQueryInternal(begin, src-begin, escape_all );
	}

	//fragment
	if(*src == '#')
	{//we have fragment
		src++; //skip past #
		status =  SetNthString(kFragment, src);
	}


	return status;
}


//-------------------------------------------------------------------
//Format 
//username@host?query
status_t SUrl::ParseMailToURL(const char *src, bool escape_all)
{
	status_t status=B_OK;
	const char *begin = src;
	const char *end = src;

	char qBegin = fData->fQueryBegin;

	// Parse username
	while (*end && *end != '@')
			end++;
	
	status = SetNthString(kUser, begin, end- begin);
	fData->flags.HasUserName = true;
	
	
	// Parse hostname
	if (*end == '@'&& status >= B_OK ) 
	{
		end++;
		begin = end;

		while ((*end != '\0') && (*end != qBegin))
				end++;

		status = SetNthString(kHostname, begin, end- begin );
	}	
	
	// Should the query part of a mailto: URL be escaped?  Other
	// queries are, but I don't know.  -- Laz, 2001-05-25
	if (*end == qBegin && status >= B_OK ) 
	{
		end++;
		begin = end;
		while (*end != '\0')
				end++;

		status = SetNthString(kQuery, begin, end- begin);
	}

	return status;
}
//-------------------------------------------------------------------
//Javascript has only query
status_t SUrl::ParseJavaScriptURL(const char *url)
{
	return  SetNthString(kQuery, url);
}
//-------------------------------------------------------------------

const char *ExtractQueryString(const char *src, int *query_length, char qChar)
{
	const char *queryBegin=NULL;
	*query_length = 0;

	if(src && *src == qChar)
	{//We have a query
		src++;//skip past "?'
		queryBegin = src;

		const char *src_query_end = src;
		while (*src_query_end != '#' && *src_query_end != '\0') 
		{
			src_query_end++;
			query_length++;
		}

	}

	return queryBegin;
}
//-------------------------------------------------------------------

//
//	Normalize the path, removing '.' and '..' entries.  Notice that this is done
//	in place, as the length of the path will not increase.
//
char*	NormalizePathString(char *path)
{
	const char *pathStart = path;	
	const char *in = pathStart;
	char *out = (char*) pathStart;

	while (*in) {
		if (in[0] == '/' && in[1] == '.' && in[2] == '/')
			in += 2;	// remove /./
		else if (in[0] == '/' && in[1] == '.' && in[2] == '.'
			&& (in[3] == '/' || in[3] == '\0')) {
			// remove /..
			if (out > pathStart) {
				// Walk back a path component
				out--;
				while (*out != '/' && out > pathStart)
					out--;
			}

			in += 3;
			if (*out == '.' && out[1] == '\0')
				out++;	// special case to preserve last slash in '/..'
		} else
			*out++ = *in++;
	}

	// If this was an empty path, set it to '/'
	//if (out == pathStart)
	//	*out++ = '/';

	*out++ = '\0';
	return out;
}

//-----------------------------------------------------
//-------------------------------------------------------------------
void SUrl::GetUnescapedFileName(char *out, int size) const
{
	const char *fname = 0;
	for (const char *path = GetPath(); *path; path++) 
	{
		if (path && *path == '/')
			fname = path + 1;
		else if(path)
			fname = path;
	}

	if(fname && size > 0)
        UnescapeString(out, fname, size);
	else if(out)
		*out = '\0';
}


//-------------------------------------------------------------------

#ifdef NEVER
SUrl::SUrl(const char *scheme, const char *hostname, const char *path, int port, bool escape_all)
	:	fData(0)
{
	if(!scheme || !(*scheme))
	{
		Reset();
		return;
	}


	int32_t	extraSpace = strlen(hostname)+GetEscapedLength(path);



	if(	CreateUrlData(extraSpace,scheme, NULL ) < B_OK)
	{
		Reset();
		return;
	}

	fData->fPort = port;
	SetHostName(hostname);
	SetPathInternal(path, 0, escape_all);
}
//-------------------------------------------------------------------
SUrl::SUrl(const char *name, const SMessage *message)
	:	fData(0)
{
	ExtractFromMessage(name,message); 
}
//-------------------------------------------------------------------
uint32_t SUrl::HashString(const char *string, uint32_t hash) const
{
	while (*string)
		hash = (hash << 7) ^ (hash >> 24) ^ *string++;

	return hash;
}
//-------------------------------------------------------------------
#if _SUPPORTS_WARNING
#warning "I may need to take these out"
#endif

// Case insensitive version of HashString.
uint32_t SUrl::HashStringI(const char *string, uint32_t hash) const
{
	while (*string) {
		char c = *string++;
		if (isascii(c))
			c = tolower(c);

		hash = (hash << 7) ^ (hash >> 24) ^ c;
	}
	
	return hash;
}
//-------------------------------------------------------------------


//	Important: Do not consider fragment, user, or password
//	in hash.  The do not change the resource pointed to, and
//	returning different hash values can result in multiple
//	cached copies of the same resource.  The scheme and port are not
//	the considered because they are generally always the same (http:80).
uint32_t SUrl::GenerateHash() const
{
	uint32_t hash = 0;	

	// Hash hostname
	// Most of the URLs are going to be www.XXX.com.  Strip off the beginning
	// and end to generate a more unique hash.  Note also the tolower().  As the
	// host is case insensitive, hash the lowercase version of the letters to
	// guarantee different case version generate the same hash.  
	const char *in = GetHostName();
	while (*in)
		if (*in++ == '.')
			break;
		
	while (*in && *in != '.') {
		char c = *in++;
		if (isascii(c))
			c = tolower(c);
			
		hash = (hash << 7) ^ (hash >> 24) ^ c;
	}
	
	// HashString is case sensitive, because the path and query are.
	hash = HashString(GetPath(), hash);
	hash = HashString(GetQuery(), hash);
	return hash;
}
//-------------------------------------------------------------------
void SUrl::AddToMessage(const char *name, SMessage *message) const
{
	message->Data().JoinItem(SValue::String(name), *this);
}

void SUrl::ExtractFromMessage(const char *name, const SMessage *message)
{
	SValue value = message->Data().ValueFor(SValue::String(name));

	if (value.IsDefined() && value.Length() > 0) {
		ssize_t len = value.Length();

		free(fData);
		fData = (URLData*)malloc(len);
		if (fData == 0) return;
		//realloc(fData, len);
		memcpy(fData, value.Data(), len);
	} else {
		// This was a bad URL
		Reset();
	}	
}

//-------------------------------------------------------------------
inline void bindump(char *data, int size)
{
	int lineoffs = 0;
	while (size > 0) {
		printf("\n%04x  ", lineoffs);
		for (int offs = 0; offs < 16; offs++) {
			if (offs < size)
				printf("%02x ", (uint8_t)data[offs]);
			else
				printf("   ");
		}
			
		printf("     ");
		for (int offs2 = 0; offs2 < MIN(size, 16); offs2++)
			printf("%c", (data[offs2] > 31 && (uint8_t) data[offs2] < 128) ? data[offs2] : '.');

		data += 16;
		size -= 16;
		lineoffs += 16;
	}

	printf("\n");
}

#endif
