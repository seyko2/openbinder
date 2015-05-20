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

#include <support/TextStream.h>
#include <support/Autolock.h>
#include <support/Locker.h>
#include <support/Process.h>
#include <support/Debug.h>
#include <support/KeyedVector.h>
#include <support/Package.h>
#include <support/String.h>

#include <support_p/WindowsCompatibility.h>
#include <support_p/SupportMisc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

//#include <os_p/priv_syscalls.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

#if !SUPPORTS_TEXT_STREAM
#if _SUPPORTS_WARNING
#warning Compiling without BTextOutput support
#endif
#endif

/* ---------------------------------------------------------------- */

BTextOutput::style_state::style_state()
	:	tag(0), indent(0), front(1),
		buffering(false), buffer(NULL),
		bufferLen(0), bufferAvail(0)
{
}

BTextOutput::style_state::~style_state()
{
	if (buffer) free(buffer);
}

/* ---------------------------------------------------------------- */

#if SUPPORTS_TEXT_STREAM

struct BTextOutput::thread_styles
{
	SLocker								lock;
	SKeyedVector<int32_t, style_state*>	styles;
	
	thread_styles()
		: lock("BTextOutput thread_styles")
	{
	}
};

#endif

/* ---------------------------------------------------------------- */

BTextOutput::BTextOutput(const sptr<IByteOutput>& stream, uint32_t flags)
	:	m_stream(stream.ptr()), m_flags(flags), m_threadStyles(NULL), m_nextTag(0)
{
#if SUPPORTS_TEXT_STREAM
	InitStyles();
#endif
	m_stream->Acquire(this);
}

BTextOutput::BTextOutput(IByteOutput *This, uint32_t flags)
	:	m_stream(This), m_flags(flags), m_threadStyles(NULL), m_nextTag(0)
{
#if SUPPORTS_TEXT_STREAM
	InitStyles();
#endif
}

sptr<IBinder> BTextOutput::AsBinderImpl()
{
	return m_stream->AsBinder();
}

sptr<const IBinder> BTextOutput::AsBinderImpl() const
{
	return ((const IByteOutput*)m_stream)->AsBinder();
}

/* ---------------------------------------------------------------- */

#if SUPPORTS_TEXT_STREAM
static const uint32_t tagFlags	= B_TEXT_OUTPUT_COLORED
								| B_TEXT_OUTPUT_TAG_TEAM
								| B_TEXT_OUTPUT_TAG_THREAD
								| B_TEXT_OUTPUT_TAG_TIME;

void BTextOutput::InitStyles()
{
	size_t i;
	
	m_numColors = 7;
	m_colors[0] = 0;
	for (i=1; i<m_numColors; i++) m_colors[i] = 30+i;
					
	if ((m_flags&B_TEXT_OUTPUT_FROM_ENV) != 0) {
		const char* env = getenv("TEXT_OUTPUT_FORMAT");
		bool enable = true;
		uint32_t defs = B_TEXT_OUTPUT_THREADED;
		while (env && *env) {
			switch (*env) {
				case 'm':
					if (enable) m_flags |= B_TEXT_OUTPUT_THREADED;
					else {
						m_flags &= ~B_TEXT_OUTPUT_THREADED;
						defs &= ~B_TEXT_OUTPUT_THREADED;
					}
					enable = true;
					break;
				case 'c':
					if (enable) m_flags |= B_TEXT_OUTPUT_COLORED|defs;
					else m_flags &= ~B_TEXT_OUTPUT_COLORED;
					enable = true;
					if (env[1] >= '0' && env[1] <= '9') {
						m_numColors = 0;
						m_colors[0] = 0;
						do {
							env++;
							if (*env >= '0' && *env <= '9') {
								m_colors[m_numColors]	= (m_colors[m_numColors]*10)
														+ ((*env) - '0');
							} else {
								m_numColors++;
								m_colors[m_numColors] = 0;
							}
						} while ((env[1] >= '0' && env[1] <= '9') ||
									(env[1] == ',' && m_numColors < (MAX_COLORS-1)));
						m_numColors++;
					}
					break;
				case 'p':
					if (enable) m_flags |= B_TEXT_OUTPUT_TAG_TEAM|defs;
					else m_flags &= ~B_TEXT_OUTPUT_TAG_TEAM;
					enable = true;
					break;
				case 't':
					if (enable) m_flags |= B_TEXT_OUTPUT_TAG_THREAD|defs;
					else m_flags &= ~B_TEXT_OUTPUT_TAG_THREAD;
					enable = true;
					break;
				case 'w':
					if (enable) m_flags |= B_TEXT_OUTPUT_TAG_TIME|defs;
					else m_flags &= ~B_TEXT_OUTPUT_TAG_TIME;
					enable = true;
					break;
				case '!':
					enable = false;
					break;
				default:
					enable = true;
					break;
			}
			env++;
		}
	}

	if ((m_flags&B_TEXT_OUTPUT_THREADED) != 0) 
	{
		m_threadStyles = new thread_styles;
	}
	else if ((m_flags & B_TEXT_OUTPUT_COLORED) != 0)
	{
		m_globalStyle.tag = (m_flags & B_TEXT_OUTPUT_COLORED_MASK) >> 4;
	}
}

BTextOutput::style_state* BTextOutput::Style()
{
	if (m_threadStyles == NULL) return &m_globalStyle;
	
	SAutolock _l(m_threadStyles->lock.Lock());
	style_state* style = m_threadStyles->styles.ValueFor(SysCurrentThread());
	if (!style) 
	{
		SysThreadExitCallbackID idontcare;
		status_t err = SysThreadInstallExitCallback(DeleteStyle, this, &idontcare);
		if (err == B_OK) 
		{
			style = new style_state;
			if ((m_flags & B_TEXT_OUTPUT_COLORED_MASK) != 0)
			{
				style->tag = (m_flags & B_TEXT_OUTPUT_COLORED_MASK) >> 4;
			}
			else
			{
				style->tag = g_threadDirectFuncs.atomicInc32(&m_nextTag);
			}
			style->buffering = true;
			m_threadStyles->styles.AddItem(SysCurrentThread(), style);
			IncStrong(this);
		}
	}
	return style;
}

void BTextOutput::DeleteStyle(void *_o)
{
	BTextOutput* me = static_cast<BTextOutput*>(_o);
	if (me) {
		SAutolock _l(me->m_threadStyles->lock.Lock());
		style_state* style = me->m_threadStyles->styles.ValueFor(SysCurrentThread());
		me->m_threadStyles->styles.RemoveItemFor(SysCurrentThread());
		delete style;
		me->DecStrong(me);
	}
}
#endif

status_t BTextOutput::Print(const char *debugText, ssize_t len)
{
#if SUPPORTS_TEXT_STREAM
	iovec vec[2];
	status_t result;
	style_state* style = Style();

	if (len < 0) len = strlen(debugText);

	if (style) {
#if TEXTOUTPUT_SMALL_STACK
		log_info& log = style->tmp_log;
#else
		log_info log;
#endif

		if (style->bufferLen <= 0) style->startIndent = style->indent;

		log.tag = style->tag;
		log.team = SysProcessID();
		log.thread = SysCurrentThread();
		log.time = SysGetRunTime();
		log.indent = style->startIndent;
			
		if (style && style->buffering) {
			const ssize_t totalLen = len;
			int32_t count = 0;
			
			while (len >= 1 && debugText[len-1] != '\n') len--;
			
			if (len >= 1 && debugText[len-1] == '\n') {
				if ((vec[count].iov_len=style->bufferLen) > 0) {
					vec[count++].iov_base = style->buffer;
				}
				vec[count].iov_base = const_cast<char*>(debugText);
				vec[count++].iov_len = len;
				
				log.front = true;
				if ((result=LogV(log, vec, count)) >= B_OK) result = B_OK;
				
				style->bufferLen = 0;
			} else {
				result = B_OK;
			}
			
			const ssize_t extra = totalLen-len;
			if (extra > 0) {
				if (style->bufferAvail < style->bufferLen+extra) {
					style->buffer = static_cast<char*>(
						realloc(style->buffer, style->bufferLen+extra));
					style->bufferAvail = style->bufferLen+extra;
				}
				if (style->buffer != NULL) {
					memcpy(style->buffer+style->bufferLen, debugText+len, extra);
					style->bufferLen += extra;
				} else {
					style->bufferLen = style->bufferAvail = 0;
				}
			}
		
		} else {
			vec[0].iov_base = const_cast<char*>(debugText);
			vec[0].iov_len = len;
			log.front = style->front ? true : false;
			if ((result=LogV(log, vec, 1)) >= B_OK) {
				style->front = (result != 0);
				result = B_OK;
			}
		}
	
	} else {
		vec[0].iov_base = const_cast<char*>(debugText);
		vec[0].iov_len = len;
		if ((result=m_stream->WriteV(vec, 1)) >= B_OK) {
			result = B_OK;
		}
	}

	return result;
#else
	(void)debugText;
	(void)len;
	return B_UNSUPPORTED;
#endif
}

status_t BTextOutput::LogV(	const log_info& info,
	const iovec *vector, ssize_t count, uint32_t /*flags*/)
{
#if SUPPORTS_TEXT_STREAM
#if TEXTOUTPUT_SMALL_STACK
	SVectorIO& io = Style()->tmp_vecio;
	char* prefix = Style()->tmp_prefix;
#else
	SVectorIO io;
	char prefix[64];
#endif

	size_t plen = 0;
	int32_t indentLen = info.indent;
	const char* indentText = NULL;
	const char* resetText = NULL;
	const char *start,*end;
	int32_t len;
	int front = info.front;
	bool tagged = false;
	
	for (ssize_t i = 0; i<count; i++) {
		start = end = static_cast<char*>(vector[i].iov_base);
		len = vector[i].iov_len;
		
		// Loop while either the next current is not '0' -or- we have
		// been given an exact number of bytes to write.
		while (len && *start) {
			// Look for end of text or next newline.
			while (len && *end && (*end != '\n')) { len--; end++; };
			
			// If we are going to write the start of a line, first
			// insert an indent.
			if (front) {
				front = false;
				
				if (plen == 0 && (m_flags&tagFlags) != 0) {
					prefix[0] = 0;
					if ((m_flags&B_TEXT_OUTPUT_COLORED) != 0) {
						sprintf(prefix+plen, "\033[%dm", m_colors[info.tag%m_numColors]);
						plen += strlen(prefix+plen);
						resetText = "\033[0m";
					}
					if ((m_flags&B_TEXT_OUTPUT_TAG_TEAM) != 0) {
						sprintf(prefix+plen, "%sProc %9lu", tagged ? ", " : "", info.team);
						plen += strlen(prefix+plen);
						tagged = true;
					}
					if ((m_flags&B_TEXT_OUTPUT_TAG_THREAD) != 0) {
						sprintf(prefix+plen, "%sThr %10lu", tagged ? ", " : "", info.thread);
						plen += strlen(prefix+plen);
						tagged = true;
					}
					if ((m_flags&B_TEXT_OUTPUT_TAG_TIME) != 0) {
						sprintf(prefix+plen, "%s%7Lu.%05Lus",
									tagged ? ", " : "", info.time/1000000, (info.time%1000000)/10);
						plen += strlen(prefix+plen);
						tagged = true;
					}
					if (tagged) {
						strcpy(prefix+plen, ": ");
						plen += strlen(prefix+plen);
					}
				}
				
				if (plen) io.AddVector(prefix, plen);
				if (indentLen > 0) {
					if (indentText == NULL) indentText = MakeIndent(&indentLen);
					io.AddVector(const_cast<char*>(indentText), indentLen);
				}
			}
			
			// Skip ahead to include all newlines in this section.
			while (len && *end == '\n') {
				len--;
				end++;
				front = true;
			}
			
			// Write this line of text and get ready to process the next.
			//m_stream->Write(start, end-start);
			io.AddVector(const_cast<char*>(start), end-start);
			start = end;
		}
	
	}
	
	if (front && resetText) io.AddVector(const_cast<char*>(resetText), strlen(resetText));
	
	const int32_t N = io.CountVectors();
	status_t err = (N > 0) ? m_stream->WriteV(io, N) : B_OK;
	if (err < B_OK && N > 1) {
		// Destination device may not support writev.
		for (int32_t i=0, err=B_OK; i<N && err >= B_OK; i++)
			err = m_stream->WriteV(io.Vectors()+i, 1);
	}

	io.MakeEmpty();

	return err >= B_OK ? (front ? 1 : 0) : err;
#else
	(void)info;
	(void)vector;
	(void)count;
	return B_UNSUPPORTED;
#endif
}

void BTextOutput::Flush()
{
	Sync();
}

status_t BTextOutput::Sync()
{
#if SUPPORTS_TEXT_STREAM
	style_state* style = Style();

	if (style->buffering && style->bufferLen > 0) {
		iovec vec;
		status_t result = B_OK;
#if TEXTOUTPUT_SMALL_STACK
		log_info& log = style->tmp_log;
#else
		log_info log;
#endif

		log.tag = style->tag;
		log.team = this_team();
		log.thread = SysCurrentThread();
		log.time = SysGetRunTime();
		log.indent = style->startIndent;
		log.front = style->front;
		
		vec.iov_len = style->bufferLen;
		vec.iov_base = style->buffer;
		style->bufferLen = 0;
		if ((result=LogV(log, &vec, 1)) < B_OK) return result;
		style->front = (result != 0);
	}

	return m_stream->Sync();
#else
	return B_UNSUPPORTED;
#endif
}

#if SUPPORTS_TEXT_STREAM
const char* BTextOutput::MakeIndent(style_state *style, int32_t* out_indent)
{
	int32_t indent = style->indent;
	const char* txt = MakeIndent(&indent);
	if (out_indent) *out_indent = indent;
	return txt;
}

const char* BTextOutput::MakeIndent(int32_t* inout_indent)
{
	static const char space[] =
	"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
	int32_t off = sizeof(space) - *inout_indent - 1;
	if( off < 0 ) {
		off = 0;
		*inout_indent = sizeof(space)-1;
	}
	return space+off;
}
#endif

void BTextOutput::MoveIndent(int32_t delta)
{
#if SUPPORTS_TEXT_STREAM
	style_state* style = Style();
	
	g_threadDirectFuncs.atomicAdd32(&style->indent, delta);
	if (style->indent < 0) style->indent = 0;
#else
	(void)delta;
#endif
}

BTextOutput::~BTextOutput()
{
	m_stream->AttemptRelease(this);
	//if (m_tlsSlot >= 0) tls_free(m_tlsSlot);
}

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */

BTextInput::BTextInput(const sptr<IByteInput>& stream, uint32_t flags)
	:	m_stream(stream.ptr()), m_flags(flags), m_lock("BTestInput"),
		m_pos(0), m_avail(0), m_inCR(false)
{
	m_stream->Acquire(this);
}

BTextInput::BTextInput(IByteInput *This, uint32_t flags)
	:	m_stream(This), m_flags(flags), m_lock("BTextInput"),
		m_pos(0), m_avail(0), m_inCR(false)
{
}

BTextInput::~BTextInput()
{
	m_stream->AttemptRelease(this);
}

sptr<IBinder> BTextInput::AsBinderImpl()
{
	return m_stream->AsBinder();
}

sptr<const IBinder> BTextInput::AsBinderImpl() const
{
	return ((const IByteInput*)m_stream)->AsBinder();
}

ssize_t BTextInput::ReadChar()
{
	ssize_t retval;

	m_lock.Lock();

	while (true) {
		if (m_pos < m_avail) {
			char c = m_buffer[m_pos++];
			if (c == '\r') {
				// this is the beginning of a CR sequence
				c = '\n';
				m_inCR = true;

			} else if (c == '\n' && m_inCR) {
				// this is the end of a CR sequence - we've already
				// returned a '\n' for this sequence, so just drop this
				// one
				continue;

			} else {
				// normal character - accept as-is
				m_inCR = false;
			}

			// we've got our character
			m_lock.Unlock();
			return c;
		}

		if (m_avail >= 0) {
			// XXX Shouldn't hold lock while doing this...
			m_avail = m_stream->Read(m_buffer, sizeof(m_buffer));
			m_pos = 0;
		}

		if (m_avail <= 0) {
			break;
		}
	}

	if (m_avail == 0)
		retval = EOF;
	else
		retval = m_avail;	// error value

	m_lock.Unlock();
	return retval;
}

ssize_t BTextInput::ReadLineBuffer(char* buffer, size_t size)
{
	size_t outPos = 0;
	char last_c = 32;

	m_lock.Lock();

	while ((outPos+1) < size && last_c != '\r' && last_c != '\n' && last_c != 0) {
		if (m_pos < m_avail) {
			last_c = m_buffer[m_pos++];
			if (last_c == '\b') {
				// Deal with backspace.  Probably shouldn't be here, or at
				// least only as an option.
				if (outPos > 0) outPos--;
				continue;
			} else if (last_c == '\n' && m_inCR) {
				// Eat a '\n' if it immediately follows a '\r'.
				last_c = 1;
				continue;
			}
			buffer[outPos++] = last_c;
			m_inCR = false;
			continue;
		}

		if (m_avail >= 0) {
			// XXX Shouldn't hold lock while doing this...
			m_avail = m_stream->Read(m_buffer, sizeof(m_buffer));
			m_pos = 0;
		}

		if (m_avail <= 0) {
			break;
		}
	}

	// If we terminated on a '\r', turn it into a '\n' and
	// remember this state to skip any following '\n' in the
	// stream.
	if (outPos > 0 && buffer[outPos-1] == '\r') {
		buffer[outPos-1] = '\n';
		m_inCR = true;
	}

	m_lock.Unlock();

	// Append string termination.
	buffer[outPos < size ? outPos : (outPos-1)] = 0;

	if (outPos > 0) return outPos;
	return m_avail >= 0 ? 0 : m_avail;
}


int32_t const kAppendLineBufferSize = 128;

ssize_t BTextInput::AppendLineTo(SString* outString)
{
	// outString may be large, so resizing it will be expensive.
	// So, instead of appending individual chunks to outString, we
	// append them to a temporary SString (which is itself appended to
	// outString later).  outString is resized no more than once.
	SString temp;
	ssize_t total_read = 0;
	ssize_t amount_read;
	for(;;) {
		char * buf = temp.LockBuffer(total_read + kAppendLineBufferSize );
		buf[total_read] = '\0';
		amount_read = ReadLineBuffer(&(buf[total_read]), kAppendLineBufferSize );
		temp.UnlockBuffer();
		if (amount_read > 0) {
			total_read += amount_read;
			if (temp[size_t(total_read-1)] == '\n')
				// If the last character in the buffer is a newline,
				// ReadLineBuffer had enough buffer to finish the line.
				// If the file did not end in a newline, we'll catch the
				// EOF on the next loop.
				break;
		} else {
			// amount_read == 0, or amount_read == EOF
			break;
		}
	}
	
	if (total_read > 0) {
		(*outString) << temp;
		return total_read;
	}

	return EOF;
}

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */

#if SUPPORTS_TEXT_STREAM
static inline int isident(int c)
{
	return isalnum(c) || c == '_';
}

static inline bool isasciitype(char c)
{
	if( c >= ' ' && c < 127 && c != '\'' && c != '\\' ) return true;
	return false;
}

static inline char makehexdigit(uint32_t val)
{
	return "0123456789abcdef"[val&0xF];
}

static char* appendhexnum(uint32_t val, char* out)
{
	for( int32_t i=28; i>=0; i-=4 ) {
		*out++ = makehexdigit( val>>i );
	}
	*out = 0;
	return out;
}

static inline char makeupperhexdigit(uint32_t val)
{
	return "0123456789ABCDEF"[val&0xF];
}

static char* appendupperhexnum(uint32_t val, char* out)
{
	for( int32_t i=28; i>=0; i-=4 ) {
		*out++ = makeupperhexdigit( val>>i );
	}
	*out = 0;
	return out;
}

static char* appendcharornum(char c, char* out, bool skipzero = true)
{
	if (skipzero && c == 0) return out;

	if (isasciitype(c)) {
		*out++ = c;
		return out;
	}

	*out++ = '\\';
	*out++ = 'x';
	*out++ = makehexdigit(c>>4);
	*out++ = makehexdigit(c);
	return out;
}

static char* TypeToString(type_code type, char* out,
						  bool fullContext = true,
						  bool strict = false)
{
	char* pos = out;
	char c[4];
	c[0] = (char)((type>>24)&0xFF);
	c[1] = (char)((type>>16)&0xFF);
	c[2] = (char)((type>>8)&0xFF);
	c[3] = (char)(type&0xFF);
	bool valid;
	if( !strict ) {
		// now even less strict!
		// valid = isasciitype(c[3]);
		valid = true;
		int32_t i = 0;
		bool zero = true;
		while (valid && i<3) {
			if (c[i] == 0) {
				if (!zero) valid = false;
			} else {
				zero = false;
				//if (!isasciitype(c[i])) valid = false;
			}
			i++;
		}
		// if all zeros, not a valid type code.
		if (zero) valid = false;
	} else {
		valid = isident(c[3]) ? true : false;
		int32_t i = 0;
		bool zero = true;
		while (valid && i<3) {
			if (c[i] == 0) {
				if (!zero) valid = false;
			} else {
				zero = false;
				if (!isident(c[i])) valid = false;
			}
			i++;
		}
	}
	if( valid && (!fullContext || c[0] != '0' || c[1] != 'x') ) {
		if( fullContext ) *pos++ = '\'';
		pos = appendcharornum(c[0], pos);
		pos = appendcharornum(c[1], pos);
		pos = appendcharornum(c[2], pos);
		pos = appendcharornum(c[3], pos);
		if( fullContext ) *pos++ = '\'';
		*pos = 0;
		return pos;
	}
	
	if( fullContext ) {
		*pos++ = '0';
		*pos++ = 'x';
	}
	return appendhexnum(type, pos);
}
#endif
	
/* ---------------------------------------------------------------- */

#if SUPPORTS_TEXT_STREAM
static void append_float(const sptr<ITextOutput>& io, double value)
{
	char buffer[64];
	sprintf(buffer, "%g", value);
	if( !strchr(buffer, '.') && !strchr(buffer, 'e') &&
		!strchr(buffer, 'E') ) {
		strncat(buffer, ".0", sizeof(buffer)-1);
	}
	if (&io) io->Print(buffer, strlen(buffer));

}
#endif

/* ---------------------------------------------------------------- */

const sptr<ITextOutput>& endl(const sptr<ITextOutput>& io)
{
#if SUPPORTS_TEXT_STREAM
#if TARGET_HOST == TARGET_HOST_WIN32
	if (io != NULL) io->Print("\r\n");
#else
	if (io != NULL) io->Print("\n");
#endif
#endif
	return io;
}

const sptr<ITextOutput>& indent(const sptr<ITextOutput>& io)
{
#if SUPPORTS_TEXT_STREAM
	if (io != NULL) io->MoveIndent(1);
#endif
	return io;
}

const sptr<ITextOutput>& dedent(const sptr<ITextOutput>& io)
{
#if SUPPORTS_TEXT_STREAM
	if (io != NULL) io->MoveIndent(-1);
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const char* str)
{
#if SUPPORTS_TEXT_STREAM
	if (io != NULL)  {
		if (str) io->Print(str, strlen(str));
		else io->Print("(nil)", 5);
	}
#else
	(void)str;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, char c)
{
#if SUPPORTS_TEXT_STREAM
	if (io != NULL) io->Print(&c, sizeof(c));
#else
	(void)c;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, bool b)
{
#if SUPPORTS_TEXT_STREAM
	if (io != NULL) {
		if (b) io->Print("true", 4);
		else io->Print("false", 5);
	}
#else
	(void)b;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, int num)
{
#if SUPPORTS_TEXT_STREAM
	char buffer[64];
	sprintf(buffer, "%d", num);
	if (io != NULL) io->Print(buffer, strlen(buffer));
#else
	(void)num;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, unsigned int num)
{
#if SUPPORTS_TEXT_STREAM
	char buffer[64];
	sprintf(buffer, "%u", num);
	if (io != NULL) io->Print(buffer, strlen(buffer));
#else
	(void)num;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, long num)
{
#if SUPPORTS_TEXT_STREAM
	char buffer[64];
	sprintf(buffer, "%ld", num);
	if (io != NULL) io->Print(buffer, strlen(buffer));
#else
	(void)num;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, unsigned long num)
{
#if SUPPORTS_TEXT_STREAM
	char buffer[64];
	sprintf(buffer, "%lu", num);
	if (io != NULL) io->Print(buffer, strlen(buffer));
#else
	(void)num;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, int64_t num)
{
#if SUPPORTS_TEXT_STREAM
	char buffer[64];
	sprintf(buffer, "%" B_FORMAT_INT64 "d", num);
	if (io != NULL) io->Print(buffer, strlen(buffer));
#else
	(void)num;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, uint64_t num)
{
#if SUPPORTS_TEXT_STREAM
	char buffer[64];
	sprintf(buffer, "%" B_FORMAT_INT64 "u", num);
	if (io != NULL) io->Print(buffer, strlen(buffer));
#else
	(void)num;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, float num)
{
#if SUPPORTS_TEXT_STREAM
	append_float(io, num);
#else
	(void)num;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, double num)
{
#if SUPPORTS_TEXT_STREAM
	append_float(io, num);
#else
	(void)num;
#endif
	return io;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const void* ptr)
{
#if SUPPORTS_TEXT_STREAM
	char buffer[64];
	sprintf(buffer, "%p", ptr);
	if (io != NULL) io->Print(buffer, strlen(buffer));
#else
	(void)ptr;
#endif
	return io;
}

/* ---------------------------------------------------------------- */

STypeCode::STypeCode(type_code type)
	:	fType(type)
{
}

STypeCode::~STypeCode()
{
}

type_code STypeCode::TypeCode() const
{
	return fType;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const STypeCode& type)
{
#if SUPPORTS_TEXT_STREAM
	char buffer[32];
	const int32_t len = TypeToString(type.TypeCode(), buffer) - buffer;
	buffer[len] = 0;
	io->Print(buffer);
#else
	(void)type;
#endif
	return io;
}

/* ---------------------------------------------------------------- */

SDuration::SDuration(nsecs_t d)
	: m_duration(d)
{
}

SDuration::~SDuration()
{
}

nsecs_t SDuration::Duration() const
{
	return m_duration;
}

static nsecs_t print_unit(const sptr<ITextOutput>& io, nsecs_t amount, const char* suffix, nsecs_t remain, bool* started)
{
	nsecs_t unit = remain/amount;
	if (unit != 0 || (amount == 1 && !(*started)) /*|| *started*/) {
		if (*started) io << " ";
		*started = true;
		io << unit << suffix;
	}
	return remain-(amount*unit);
}

struct print_unit_info
{
	nsecs_t		amount;
	const char*	suffix;
};

static const struct print_unit_info units[] = {
	{ B_S2NS(60*60*24), "d" },
	{ B_S2NS(60*60), "h" },
	{ B_S2NS(60), "m" },
	{ B_S2NS(1), "s" },
	{ B_MS2NS(1), "ms" },
	{ B_US2NS(1), "us" },
	{ 1, "ns" },
	{ 0, NULL }
};

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SDuration& d)
{
	nsecs_t remain = d.Duration();
	bool started = false;
	const print_unit_info* u = units;
	while (u->suffix) {
		remain = print_unit(io, u->amount, u->suffix, remain, &started);
		u++;
	}
	return io;
}

/* ---------------------------------------------------------------- */

SSize::SSize(size_t s)
	: m_size(s)
{
}

SSize::~SSize()
{
}

size_t SSize::Size() const
{
	return m_size;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SSize& s)
{
	if (s.Size() > (1024*1024*10)) {
		io << (float(s.Size())/(1024*1024)) << "Mb";
	} else if (s.Size() > (1024*10)) {
		io << (float(s.Size())/(1024)) << "Kb";
	} else {
		io << s.Size() << "b";
	}
	return io;
}

/* ---------------------------------------------------------------- */

SStatus::SStatus(status_t s)
	: m_status(s)
{
}

SStatus::~SStatus()
{
}

status_t SStatus::Status() const
{
	return m_status;
}

SString SStatus::AsString() const
{
	SString result;
	if ((m_status&0x40000000) != 0) {
		char buf[128];
		const char* str = strerror_r(-m_status, buf, sizeof(buf));
		// gag
		if (strncmp(str, "Unknown error ", sizeof("Unknown error ")-1) != 0) {
			result = str;
		}
#if !LIBBE_BOOTSTRAP
	} else {
		// See if there is a localized string resource
		// for this error code.
		SPackage package(SString("org.openbinder.kits.support"));
		if (package.StatusCheck() == B_OK) {
			char buf[16];
			appendupperhexnum(m_status, buf);
			result = package.LoadString(SString(buf));
		}
#endif
	}
	if (m_status != 0) {
		if (result == "") result = "Unknown error";
		result += " (";
		char buffer[64];
		sprintf(buffer, "%p", (void*)m_status);
		result += buffer;
		result += ")";
		return result;
	}
	
	return SString("No error");
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SStatus& s)
{
#if SUPPORTS_TEXT_STREAM
	io << s.AsString();
#endif
	return io;
}

/* ---------------------------------------------------------------- */

SHexDump::SHexDump(const void *buf, size_t length, size_t bytesPerLine)
	: fBuffer(buf), fLength(length),
	  fBytesPerLine(bytesPerLine), fSingleLineCutoff(bytesPerLine),
	  fAlignment(4), fCArrayStyle(false)
{
	if (bytesPerLine >= 16) fAlignment = 4;
	else if (bytesPerLine >= 8) fAlignment = 2;
	else fAlignment = 1;
}

SHexDump::~SHexDump()
{
}

SHexDump& SHexDump::SetBytesPerLine(size_t bytesPerLine)
{
	fBytesPerLine = bytesPerLine;
	return *this;
}

SHexDump& SHexDump::SetSingleLineCutoff(int32_t bytes)
{
	fSingleLineCutoff = bytes;
	return *this;
}

SHexDump& SHexDump::SetAlignment(size_t alignment)
{
	fAlignment = alignment;
	return *this;
}

SHexDump& SHexDump::SetCArrayStyle(bool enabled)
{
	fCArrayStyle = enabled;
	return *this;
}

const void* SHexDump::Buffer() const		{ return fBuffer; }
size_t SHexDump::Length() const				{ return fLength; }
size_t SHexDump::BytesPerLine() const		{ return fBytesPerLine; }
int32_t SHexDump::SingleLineCutoff() const	{ return fSingleLineCutoff; }
size_t SHexDump::Alignment() const			{ return fAlignment; }
bool SHexDump::CArrayStyle() const			{ return fCArrayStyle; }

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SHexDump& data)
{
#if SUPPORTS_TEXT_STREAM
	size_t offset;
	
	size_t length = data.Length();
	unsigned char *pos = (unsigned char *)data.Buffer();
	size_t bytesPerLine = data.BytesPerLine();
	const size_t alignment = data.Alignment();
	const bool cStyle = data.CArrayStyle();
	
	if (pos == NULL) {
		if (data.SingleLineCutoff() < 0) io << endl;
		io << "(NULL)";
		return io;
	}
	
	if (length == 0) {
		if (data.SingleLineCutoff() < 0) io << endl;
		io << "(empty)";
		return io;
	}
	
	if ((int32_t)length < 0) {
		if (data.SingleLineCutoff() < 0) io << endl;
		io << "(bad length: " << (int32_t)length << ")";
		return io;
	}
	
	char buffer[256];
	static const size_t maxBytesPerLine = (sizeof(buffer)-1-11-4)/(3+1);
	
	if (bytesPerLine > maxBytesPerLine) bytesPerLine = maxBytesPerLine;
	
	const bool oneLine = (int32_t)length <= data.SingleLineCutoff();
	if (cStyle) io << "{" << endl << indent;
	else if (!oneLine) io << endl;
	
	for (offset = 0; ; offset += bytesPerLine, pos += bytesPerLine) {
		long remain = length;

		char* c = buffer;
		if (!oneLine && !cStyle) {
			sprintf(c, "0x%08x: ", (int)offset);
			c += 12;
		}

		size_t index;
		size_t word;
		
		for (word = 0; word < bytesPerLine; ) {

#if B_HOST_IS_LENDIAN
			const size_t startIndex = word+(alignment-(alignment?1:0));
			const ssize_t dir = -1;
#else
			const size_t startIndex = word;
			const ssize_t dir = 1;
#endif

			for (index = 0; index < alignment || (alignment == 0 && index < bytesPerLine); index++) {
			
				if (!cStyle) {
					if (index == 0 && word > 0 && alignment > 0) {
						*c++ = ' ';
					}
				
					if (remain-- > 0) {
						const unsigned char val = *(pos+startIndex+(index*dir));
						*c++ = makehexdigit(val>>4);
						*c++ = makehexdigit(val);
					} else if (!oneLine) {
						*c++ = ' ';
						*c++ = ' ';
					}
				} else {
					if (remain > 0) {
						if (index == 0 && word > 0) {
							*c++ = ',';
							*c++ = ' ';
						}
						if (index == 0) {
							*c++ = '0';
							*c++ = 'x';
						}
						const unsigned char val = *(pos+startIndex+(index*dir));
						*c++ = makehexdigit(val>>4);
						*c++ = makehexdigit(val);
						remain--;
					}
				}
			}
			
			word += index;
		}

		if (!cStyle) {
			remain = length;
			*c++ = ' ';
			*c++ = '\'';
			for (index = 0; index < bytesPerLine; index++) {

				if (remain-- > 0) {
					const unsigned char val = pos[index];
					*c++ = (val >= ' ' && val < 127) ? val : '.';
				} else if (!oneLine) {
					*c++ = ' ';
				}
			}
			
			*c++ = '\'';
			if (length > bytesPerLine) *c++ = '\n';
		} else {
			if (remain > 0) *c++ = ',';
			*c++ = '\n';
		}

//		if (!oneLine) io << indent;
		io->Print(buffer, (size_t)(c-buffer));
		
		if (length <= bytesPerLine) break;
		length -= bytesPerLine;
	}

	if (cStyle) {
		io << dedent << "};" << endl;
	}
#else
	(void)data;
#endif

	return io;
}

/* ---------------------------------------------------------------- */

SPrintf::SPrintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	size_t size = 256;
	m_buffer = (char *)malloc(size);
	if (m_buffer) {
		int nchars;
		do {
			nchars = vsnprintf(m_buffer, size, fmt, ap);
			if (nchars < 0) size *= 2;
			else if ((size_t)nchars >= size) size = nchars+1;
			else break;
			m_buffer = (char *)realloc(m_buffer, size);
		} while (m_buffer);
	}
	va_end(ap);
}

SPrintf::~SPrintf()
{
	free(m_buffer);
}

const char* SPrintf::Buffer() const
{
	return m_buffer;
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SPrintf& data)
{
#if SUPPORTS_TEXT_STREAM
	if (data.Buffer()) {
		io << data.Buffer();
	}
#else
	(void)data;
#endif

	return io;
}

/* ---------------------------------------------------------------- */

SFloatDump::SFloatDump(float f)
	: f_(f)
{
}

const sptr<ITextOutput>& operator<<(const sptr<ITextOutput>& io, const SFloatDump& buffer)
{
	char temp[12];
	sprintf(temp, "0x%08lx", *((const uint32_t *)&buffer.Float()));
	io << temp;
	return io;
}


#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

// ----------------------------------------------------------------- //
// ----------------------------------------------------------------- //
// ----------------------------------------------------------------- //
