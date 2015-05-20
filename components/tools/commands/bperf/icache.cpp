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

#include <PalmTypes.h>
#include <stdio.h>
#include <SysThread.h>

#ifdef __CC_ARM
// ADS cannot handle this file with optimization!!!
#pragma O0
#endif

#define	STEP_COUNT	(5+32)
#define	LOOP()	if (count & mask) tmp = ((tmp<<1) | (tmp>>31)) + 0x13ef589dL; else tmp = ((tmp<<1) | (tmp>>31)) - 0x3490decbL; \
				if (--count == 0) goto end_loop


static uint32_t execute_test(int32_t loop_count, int32_t step, int32_t mask)
{
	int32_t		count;
	uint32_t	tmp;

	tmp = 0x2589ebfaL;
end_loop:
	if (loop_count <= 0) return tmp;
	loop_count -= step;
	count = step;

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();

	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP(); LOOP();
	goto end_loop;
}

void icache_test_impl()
{
	static int32_t	steps[STEP_COUNT] =
				{ 1, 2, 4, 8, 16,
				32, 64, 96, 128, 160, 192, 224, 256,
				288, 320, 352, 384, 416, 448, 480, 512,
				544, 576, 608, 640, 672, 704, 736, 768,
				800, 832, 864, 896, 928, 960, 992, 1024 };

	int32_t		mask, i_step, step;
	uint32_t		value;
	nsecs_t time;
	int fdc;
	status_t err;

	//for (mask=-1; mask<=1; mask++)

	mask = 0;
	
	{
		for (i_step=0; i_step<STEP_COUNT; i_step++) {
			int64_t now;
			step = steps[i_step];
			// start timer here
			now = SysGetRunTime();

			value = execute_test(0x800000L, step, mask);

			now = SysGetRunTime() - now;
			printf("%ld\t0x%08lx\t0x%08lx\t%ld\n", step, mask, value, (int32_t)(B_NS2US(now)));
		}
	}
}

