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

#ifndef ANSIESCAPES_H_
#define ANSIESCAPES_H_

#define ANSI_NORMAL			"\033[00m"
#define ANSI_BLACK			"\033[0;30m"
#define ANSI_BLUE			"\033[0;34m"
#define ANSI_GREEN			"\033[0;32m"
#define ANSI_CYAN			"\033[0;36m"
#define ANSI_RED			"\033[0;31m"
#define ANSI_PURPLE			"\033[0;35m"
#define ANSI_BROWN			"\033[0;33m"
#define ANSI_GRAY			"\033[0;37m"
#define ANSI_DARK_GRAY		"\033[1;30m"
#define ANSI_LIGHT_BLUE		"\033[1;34m"
#define ANSI_LIGHT_GREEN	"\033[1;32m"
#define ANSI_LIGHT_CYAN		"\033[1;36m"
#define ANSI_LIGHT_RED		"\033[1;31m"
#define ANSI_LIGHT_PURPLE	"\033[1;35m"
#define ANSI_YELLOW			"\033[1;33m"
#define ANSI_WHITE			"\033[1;37m"

#define COLOR(_c_, _s_)		ANSI_##_c_ _s_ ANSI_NORMAL

#endif // ANSIESCAPES_H_
