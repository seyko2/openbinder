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

#include <DateTime.h>
#include <Preferences.h>

#include "Legacy.h"


struct date_format
{
	uint32_t key;
	const char* value;
};

struct date_format g_dates[] = 
{
	{ dfMDYWithSlashes,		"%m/%d/%y" },	// 12/31/95
	{ dfDMYWithSlashes,		"%d/%m/%y" },	// 31/12/95
	{ dfDMYWithDots,		"%d.%m.%y" },	// 31.12.95
	{ dfDMYWithDashes,		"%d-%m-%y" },	// 31-12-95
	{ dfYMDWithSlashes,		"%y/%m/%d" },	// 95/12/31
	{ dfYMDWithDots,		"%y.%m.%d" },	// 95.12.31
	{ dfYMDWithDashes,		"%y.%m.%d" },	// 95-12-31
	{ dfMDYLongWithComma,	"" }, 			// Dec 31, 1995
	{ dfDMYLong, 			"" }, 			// 31 Dec 1995
	{ dfDMYLongWithDot,		"" }, 			// 31. Dec 1995
	{ dfDMYLongNoDay,		"" }, 			// Dec 1995
	{ dfDMYLongWithComma,	"" }, 			//	31 Dec, 1995
	{ dfYMDLongWithDot,		"" }, 			//	1995.12.31
	{ dfYMDLongWithSpace,	"" }, 			//	1995 Dec 31
	{ dfMYMed,				"" }, 			//	Dec '95
	{ dfMYMedNoPost,		"" }, 			//	Dec 95
	{ dfMDYWithDashes, 		"%m-%d-%y" },	// 12-31-95
	{ 0, 0 }
};

SValue legacy_date_format(const SValue& arg)
{
	SValue result;

	uint32_t value = arg.AsInt32();
	
	// double check to make sure that the struct lines up
	if (value == g_dates[value].key)
	{
		result = SValue::String(g_dates[value].value);	
	}
	
	return result;
}


