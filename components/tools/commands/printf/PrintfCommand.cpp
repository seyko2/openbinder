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
#include <support/StdIO.h>
#include <support/Value.h>
#include <support/ByteStream.h>
#include <support/TextStream.h>
#include <support/MemoryStore.h>

#include "PrintfCommand.h"
#include <errno.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::storage;
using namespace palmos::app;
#endif

/* /usr/bin/printf implementation from FreeBSD */

#define	warnx1(a, b, c)
#define	warnx2(a, b, c)
#define	warnx3(a, b, c)

#define PF(f, func) { \
	char *b = NULL; \
	if (fieldwidth) \
		if (precision) \
			(void)asprintf(&b, f, fieldwidth, precision, func); \
		else \
			(void)asprintf(&b, f, fieldwidth, func); \
	else if (precision) \
		(void)asprintf(&b, f, precision, func); \
	else \
		(void)asprintf(&b, f, func); \
	if (b) { \
		out << b; \
		free(b); \
	} \
}

static int	 asciicode __P((void));
static void	 escape __P((char *));
static int	 getchr __P((void));
static double	 getdouble __P((void));
static int	 getint __P((int *));
static int	 getquad __P((quad_t *));
static char	*getstr __P((void));
static char	*mklong __P((char *, int));
static void	 usage __P((void));

static char **gargv;

int
progprintf(int argc, char* argv[], const sptr<ITextOutput>& out)
{
	static char *skip1, *skip2;
	int ch, end, fieldwidth, precision;
	char convch, nextch, *format, *fmt, *start;

#if 0
	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		case '?':
		default:
			out << "usage: printf format [arg ...]\n";
			return (1);
		}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		out << "usage: printf format [arg ...]\n";
		return (1);
	}
#endif
	argv++;
	argc--;

	/*
	 * Basic algorithm is to scan the format string for conversion
	 * specifications -- once one is found, find out if the field
	 * width or precision is a '*'; if it is, gather up value.  Note,
	 * format strings are reused as necessary to use up the provided
	 * arguments, arguments of zero/null string are provided to use
	 * up the format string.
	 */
	skip1 = "#-+ 0";
	skip2 = "0123456789";

	escape(fmt = format = *argv);		/* backslash interpretation */
	gargv = ++argv;
	for (;;) {
		end = 0;
		/* find next format specification */
next:		for (start = fmt;; ++fmt) {
			if (!*fmt) {
				/* avoid infinite loop */
				if (end == 1) {
					warnx1("missing format character",
					    NULL, NULL);
					return (1);
				}
				end = 1;
				if (fmt > start)
				{
//					(void)printf("%s", start);
					out << start;
				}
				if (!*gargv)
					return (0);
				fmt = format;
				goto next;
			}
			/* %% prints a % */
			if (*fmt == '%') {
				if (*++fmt != '%')
					break;
				*fmt++ = '\0';
//				(void)printf("%s", start);
				out << start;
				goto next;
			}
		}

		/* skip to field width */
		for (; strchr(skip1, *fmt); ++fmt);
		if (*fmt == '*') {
			if (getint(&fieldwidth))
				return (1);
			++fmt;
		} else {
			fieldwidth = 0;

			/* skip to possible '.', get following precision */
			for (; strchr(skip2, *fmt); ++fmt);
		}
		if (*fmt == '.') {
			/* precision present? */
			++fmt;
			if (*fmt == '*') {
				if (getint(&precision))
					return (1);
				++fmt;
			} else {
				precision = 0;

				/* skip to conversion char */
				for (; strchr(skip2, *fmt); ++fmt);
			}
		} else
			precision = 0;
		if (!*fmt) {
			warnx1("missing format character", NULL, NULL);
			return (1);
		}

		convch = *fmt;
		nextch = *++fmt;
		*fmt = '\0';
		switch(convch) {
		case 'c': {
			char p;

			p = getchr();
			PF(start, p);
			break;
		}
		case 's': {
			char *p;

			p = getstr();
			PF(start, p);
			break;
		}
		case 'd': case 'i': case 'o': case 'p': case 'u': case 'x': case 'X': {
			quad_t p;
			char *f;

			if ((f = mklong(start, convch)) == NULL)
				return (1);
			if (getquad(&p))
				return (1);
			PF(f, p);
			break;
		}
		case 'e': case 'E': case 'f': case 'g': case 'G': {
			double p;

			p = getdouble();
			PF(start, p);
			break;
		}
		default:
			warnx2("illegal format character %c", convch, NULL);
			return (1);
		}
		*fmt = nextch;
	}
	/* NOTREACHED */
}

static char *
mklong(char* str, int ch)
{
	static char *copy;
	static size_t copy_size;
	size_t len, newlen;
	char *newcopy;

	len = strlen(str) + 2;
	if (len > copy_size) {
		newlen = ((len + 1023) >> 10) << 10;
#ifdef SHELL
		if ((newcopy = ckrealloc(copy, newlen)) == NULL)
#else
		if ((newcopy = (char*) realloc(copy, newlen)) == NULL)
#endif
			return (NULL);
		copy = newcopy;
		copy_size = newlen;
	}

	memmove(copy, str, len - 3);
	copy[len - 3] = 'q';
	copy[len - 2] = ch;
	copy[len - 1] = '\0';
	return (copy);
}

static void
escape(char* fmt)
{
	register char *store;
	register int value, c;

	for (store = fmt; (c = *fmt); ++fmt, ++store) {
		if (c != '\\') {
			*store = c;
			continue;
		}
		switch (*++fmt) {
		case '\0':		/* EOS, user error */
			*store = '\\';
			*++store = '\0';
			return;
		case '\\':		/* backslash */
		case '\'':		/* single quote */
			*store = *fmt;
			break;
		case 'a':		/* bell/alert */
			*store = '\7';
			break;
		case 'b':		/* backspace */
			*store = '\b';
			break;
		case 'f':		/* form-feed */
			*store = '\f';
			break;
		case 'n':		/* newline */
			*store = '\n';
			break;
		case 'r':		/* carriage-return */
			*store = '\r';
			break;
		case 't':		/* horizontal tab */
			*store = '\t';
			break;
		case 'v':		/* vertical tab */
			*store = '\13';
			break;
					/* octal constant */
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			for (c = 3, value = 0;
			    c-- && *fmt >= '0' && *fmt <= '7'; ++fmt) {
				value <<= 3;
				value += *fmt - '0';
			}
			--fmt;
			*store = value;
			break;
		default:
			*store = *fmt;
			break;
		}
	}
	*store = '\0';
}

static int
getchr(void)
{
	if (!*gargv)
		return ('\0');
	return ((int)**gargv++);
}

static char *
getstr(void)
{
	if (!*gargv)
		return ("");
	return (*gargv++);
}

static char *Number = "+-.0123456789";
static int
getint(int* ip)
{
	quad_t val;

	if (getquad(&val))
		return (1);
	if (val < INT_MIN || val > INT_MAX) {
		warnx3("%s: %s", *gargv, strerror(ERANGE));
		return (1);
	}
	*ip = (int)val;
	return (0);
}

static int
getquad(quad_t* lp)
{
	quad_t val;
	char *ep;

	if (!*gargv) {
		*lp = 0;
		return (0);
	}
	if (strchr(Number, **gargv)) {
		errno = 0;
		val = strtoll(*gargv, &ep, 0);
		if (*ep != '\0') {
			warnx2("%s: illegal number", *gargv, NULL);
			return (1);
		}
		if (errno == ERANGE)
			if (val == QUAD_MAX) {
				warnx3("%s: %s", *gargv, strerror(ERANGE));
				return (1);
			}
			if (val == QUAD_MIN) {
				warnx3("%s: %s", *gargv, strerror(ERANGE));
				return (1);
			}

		*lp = val;
		++gargv;
		return (0);
	}
	*lp =  (long)asciicode();
	return (0);
}

static double
getdouble(void)
{
	if (!*gargv)
		return ((double)0);
	if (strchr(Number, **gargv))
		return (atof(*gargv++));
	return ((double)asciicode());
}

static int
asciicode(void)
{
	register int ch;

	ch = **gargv;
	if (ch == '\'' || ch == '"')
		ch = (*gargv)[1];
	++gargv;
	return (ch);
}

/* Now the binder shell command */

BPrintfCommand::BPrintfCommand(const SContext& context)
	:	BCommand(context)
{
}

SValue BPrintfCommand::Run(const SValue& args)
{
	status_t err;
	const sptr<ITextOutput> out(TextOutput());

	// sanity check
	if (!args[1].IsDefined())
	{
		TextError() << Documentation() << endl;
		return SValue::Status(B_BAD_VALUE);
	}

	// build an ordinary char** array for progprintf()
	int argc = 0;
	while (args[argc].IsDefined()) argc++;
	char** argv = new char*[argc+1];

	// fill in the argv array
	for (int i = 0; i < argc; i++)
	{
		SString str = args[i].AsString();
		argv[i] = strdup(str.String());
	}
	argv[argc] = NULL;

	// Now invoke the command
	progprintf(argc, argv, out);

	// clean up and go home
	for (int i = 0; i < argc; i++)
	{
		free(argv[i]);
	}
	delete [] argv;
	return SValue::Status(B_OK);
}

SString BPrintfCommand::Documentation() const
{
	return SString(
		"usage: printf FORMAT [ARG ...]\n"
		"\n"
		"Prints the given FORMAT string to stdout, using\n"
		"C-style formatting commands."
	);
}

sptr<IBinder> InstantiateComponent(const SString& component, const SContext& context, const SValue& args)
{
	(void)args;

	sptr<IBinder> obj = NULL;

	if (component == "")
	{
		obj = new(nothrow) BPrintfCommand(context);
	}
	return obj;
}
