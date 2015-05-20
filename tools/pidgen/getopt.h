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
/* 
 *  getopt.h - interface to my re-implementation of getopt.
 *  Copyright 1997, 2000, 2001, Benjamin Sittler
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#ifndef MY_GETOPT_H_INCLUDED
#define MY_GETOPT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* UNIX-style short-argument parser */
extern int getopt(int, char **, const char *);

extern int optind, opterr, optopt;
extern char* optarg;

struct option {
  const char* name;
  int has_arg;
  int *flag;
  int val;
};

/* human-readable values for has_arg */
#undef no_argument
#define no_argument 0
#undef required_argument
#define required_argument 1
#undef optional_argument
#define optional_argument 2

/* GNU-style long-argument parsers */
extern int getopt_long(int, char*[], const char*, const struct option*, int*);

extern int getopt_long_only(int, char*[], const char*, const struct option*, int*);

extern int _getopt_internal(int, char*[], const char*, const struct option*, int*, int);

#ifdef __cplusplus
}
#endif

#endif /* MY_GETOPT_H_INCLUDED */
