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

#ifndef _URL_H
#define _URL_H

/*!	@file support/URL.h
	@ingroup CoreSupportUtilities
	@brief URL manipulation helper class.
*/

#include <support/String.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

struct URLData;

class SUrl
{
	public:
		SUrl();
		~SUrl();

		/*! \param escape_all Set to \c true if you want to escape any "%"s and "+"s in the
		    \c path, \c query, and strings.  Set to \c false if those strings
		    might already contain %-escapes that you don't want escaped a second time.  Any
		    other characters that are unsafe in BUrls are %-escaped in either case.
		*/

		SUrl(const SUrl&);
		//You should use this, if you know SUrl can understand this scheme
		SUrl(const char *urlString, bool escape_all = false);

		//hierarchal scheme's is followed by "://"
		//for example http://

		//hierarchal : need to be supplied only for custom schemes that is internally not supported
		//If a scheme is supported by SUrl, there is no need to supply hierarchal information
		SUrl(const char *scheme, const char *hostname, int port , bool escape_all = false, bool hierarchal =false );
		SUrl(const SUrl& baseURL, const char *relativePath, bool escape_all = false);
		SUrl(const SValue &value, status_t *status=NULL);

		bool IsValid() const;
		static bool IsSchemeSupported(const char *scheme);



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
		status_t	SetInternetStyleSchemeSupport(char queryBeginChar = '?', 
												char queryAssignmentChar = '=',
												char querySeperatorChar = '&');
		//-------------------------------------------------------------------

		SString AsString() const;
		SValue AsValue() const;
		inline operator SValue() const { return AsValue(); }

		status_t SetTo(const SUrl& url);
		status_t SetTo(const char *urlString, bool escape_all = false);
		status_t SetTo(const SUrl& baseURL, const char *relativePath, bool escape_all = false);
		status_t SetTo(const SValue &value);

		/*! \note Equals() does *not* consider the fragment, username, password, or query method.
		    The resource cache depends on this so it doesn't store multiple
		    copies of the same resource.
		*/
		bool Equals(const char*) const;
		bool Equals(const SUrl& url) const;
		bool operator==(const SUrl& url) const;
		bool operator==(const char *urlString) const;
		bool operator!=(const SUrl& url) const;

		//! This assignment operator is equivalent to SUrl::SetTo(url_string, false).
		SUrl& operator=(const char *url_string);
		SUrl& operator=(const SUrl& url);

		status_t	SetHostName(const char *hostname);
		status_t	SetUserName(const char *user);
		status_t	SetPassword(const char *password);
		status_t	SetFragment(const char *fragment);

		//Any string can be set as action. The scheme to which this Url belongs should
		//understand that action

		status_t	SetAction(const char *action);
		status_t	SetPath(const char *path, bool escape_all=false);
		status_t	SetPort(int port);

		const char* GetHostName() const;
		const char* GetUserName() const;
		const char* GetPassword() const;
		const char* GetFragment() const;
		const char *GetAction() const;
		const char* GetPath() const;
		unsigned short GetPort() const;

		const char* GetScheme() const;

		
		const char* GetExtension() const;
		const char* GetQuery() const;


		//! Set this URL's query to \c query.
		status_t SetQuery(const char *query, bool escape_all = false);
		status_t AddQueryParameter(const char *name, const char *value, bool escape_all = false);
		status_t ReplaceQueryParameter(const char* name, const char* value,
		        bool addIfNotPresent = true, bool escape_all = false);
		status_t RemoveQueryParameter(const char* name);
		status_t GetQueryParameter(const char* name, SString* out_value) const;

		
		/*! GetUnescapedFileName(), GetUnescapedPath(), GetUnescapedQuery():
		    Extract the filename, path, or query, converting any %-escapes
		    and "+"s appropriately.
		    
		    \param out A buffer that you have allocated for the
		    null-terminated result.
		    
		    \param size The size of the buffer that you allocated. Note that
		    this is the full size of the buffer, including the room for the
		    null terminator.
		*/
		void GetUnescapedFileName(char *out, int size) const;
		void GetUnescapedPath(char *out, int size) const;
		void GetUnescapedQuery(char *out, int size) const;


		void Reset();




		/*! EscapePathString() and EscapeQueryString():
		    
		    Convert inString to a legal URL string by %-escaping any characters that
		    need it, and put the null-terminated result in outString.
		    
		    EscapePathString() will escape "&"s.  EscapeQueryString() will not.
		    The RFC implies that escaping "&"s in paths should be harmless,
		    though of dubious utility, but {a certain engineer who used to
			work at a certain unmentionable OS company} thought that doing so was
			important enough to merit a checkin comment, so that's what we do. In query
		    strings, however, "&"s must not be escaped, as they have a special
		    reserved meaning there. (Ref. for all of this: RFC 1738, sec. 2.2.)

		    \param escape_all Set to \c true if you want to escape any "%"s and "+"s
		    in \c inString.  Set to \c false if that string might already contain
			%-escapes that you don't want escaped a second time.  Any other
			characters that are unsafe in URLs are %-escaped in either case.
		    
		    \param outString Pointer to a buffer where you want the resulting
		    escaped string. You must have previously allocated this buffer to
		    be at least GetEscapedLength() + 1 bytes long.
		    
		    \param inString The string that needs escaping.

		    \param inLen The length of inString that you want to escape.  Useful
		    if you want to escape a substring of inString, rather than everything
		    up to the null terminator.

		    \returns A pointer to one past the null terminator of the resulting
		    escaped string in outString.
		*/

		static char *EscapePathString(bool escape_all, char *outString, const char *inString,
		        size_t inLen = 0x7fffffff);
		static char *EscapeQueryString(bool escape_all, char *outString, const char *inString,
		        size_t inLen = 0x7fffffff);

		/*! Tells how long the string in \c unescaped_string will be after it
		    has been %-escaped.
		*/
		static int GetEscapedLength(const char *unescaped_string,size_t inLen = 0x7fffffff );

		void Print(SString *dump=NULL) const;
	private:

		//This will go away
		//SUrl(const char *scheme, const char *hostname, const char *path, int port,bool escape_all = false);

		//---------------------------------------------------------------------
		//Stuff thats probably not needed ..........
		//SUrl(const char *name, const SMessage *message);
		//void AddToMessage(const char *name, SMessage *message) const;
		//void ExtractFromMessage(const char *name, const SMessage *message);
		//uint32_t GenerateHash() const;
		//uint32_t HashString(const char *string, uint32_t hash = 0) const;
		//uint32_t HashStringI(const char *string, uint32_t hash = 0) const;

		
		//---------------------------------------------------------------------


		status_t ParseMailToURL(const char *src, bool escape_all);
		status_t ParseJavaScriptURL(const char *url);
		status_t ParseInternetURL(const char *_src, bool escape_all);


		status_t SetToInternal(const SUrl &base, const char *relativePath, bool escape_all);
		//int LookupScheme(const char *scheme);
		//int LookupScheme(const char *scheme, int len);
		char* NormalizePath();
		//void ParsePathURL(const char *url, bool escape_all);

		static char* EscapeString(bool escaped, char *outString, const char *inString, size_t inLen,
		        const unsigned int *validMap, const unsigned int *escapeMap);

		/*! Extract our internal representation of a query string (escaped, stripped
		    of "?" and "#", and null-terminated) from src into dest.  Typical usage
		    is: dest = ExtractQueryString(dest, &src);
		    
		    \param dest Pointer to the buffer where you want the escaped, null-terminated
		    query string.
		    
		    \param src *src is a pointer to the source query string. It must be
		    unescaped. It may be followed by a "#" (fragment). If it does not
		    begin with "?", then ExtractQueryString() assumes there is no query,
		    and merely puts a null terminator into dest. *src is incremented to
		    one past the end of the source query (so it points to either the "#"
		    or the null terminator).
		    
		    \returns A pointer to one past the null terminator of the query string
		    in dest.
		*/


		status_t SetNthString(int16_t setIndex, const char *newString, int32_t newStringLen = -1);

		status_t CreateUrlDataFromUrl(int32_t extraSpace, const URLData *refUrlData, const char *urlString, bool escape_all=false);
		status_t CreateUrlData(int32_t extraSpace, const char *scheme, const char* urlString, bool escape_all=false);

		status_t SetPathInternal(const char *path, int32_t length, bool escape_all);

		status_t SetQueryInternal(const char *query,int32_t length, bool escape_all);

		status_t GetQueryParameterInternal(const char *query, const char *name, 
										 const char **namePP, int32_t *nameLengthP,
										 const char **valuePP, int32_t *valueLengthP) const;

		status_t ComposeEscapedQuery(const char *name, const char *value, 
								   bool escape_all, SString *outStrP) const ;

		status_t UnEscapedQuery(const char *query, int32_t length, SString *outStrP) const  ;


		//------------------------------------------------------------------
		URLData *fData;
		size_t	fDataLen;		// amount of 'fData' currently in use
		size_t	fExtraSpace;	// unused padding bytes at end of 'fData' buffer
};

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif
