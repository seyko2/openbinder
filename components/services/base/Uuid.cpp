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

#include "Uuid.h"

#include <support/StringIO.h>
#include <support/StdIO.h>

#include <Encrypt.h>

#include <stdlib.h>
#include <time.h>

B_CONST_STRING_VALUE_LARGE(BV_NODE,		"node", );
B_CONST_STRING_VALUE_LARGE(BV_STATE,	"state", );
B_CONST_STRING_VALUE_LARGE(BV_SETTINGS,	"/settings/system/uuid", );

#define UUIDS_PER_TICK 1024

// system dependent call to get the current system time. Returned as
// 100ns ticks since UUID epoch, but resolution may be less than 100ns.
uint64_t get_SysGetRunTime()
{
	/* Offset between UUID formatted times and Unix formatted times.
	  UUID UTC base time is October 15, 1582.
	  Unix base time is January 1, 1970.*/
	return (SysGetRunTime() * 100) + 0x01B21DD213814000LL;
}

/* get-current_time -- get time as 60-bit 100ns ticks since UUID epoch.
  Compensate for the fact that real clock resolution is
  less than 100ns. */
uint64_t get_current_time()
{
	static int inited = 0;
	static uint64_t time_last;
	static uint16_t  uuids_this_tick;
	uint64_t time_now;

	if (!inited)
	{
		time_now = get_SysGetRunTime();
		uuids_this_tick = UUIDS_PER_TICK;
		inited = 1;
	}

	for ( ; ; )
	{
		time_now = get_SysGetRunTime();

		/* if clock reading changed since last UUID generated, */
		if (time_last != time_now)
		{
			/* reset count of uuids gen'd with this clock reading */
			uuids_this_tick = 0;
			time_last = time_now;
			break;
		}
		
		if (uuids_this_tick < UUIDS_PER_TICK)
		{
		   uuids_this_tick++;
		   break;
		}
	   /* going too fast for our clock; spin */
	}
	/* add the count of uuids to low order bits of the clock reading */
	return time_now + uuids_this_tick;
}

void get_random_info(uint8_t seed[16])
{
	sptr<BStringIO> str = new BStringIO();
	sptr<ITextOutput> io = str.ptr();
	io << SysGetRunTime() << "_"; 
	io << getenv("PALMSOURCE_OS_VERSION_LONG") << "_";
	srand(time(NULL));
	io << rand();
	EncDigestMD5((uint8_t*)str->String(), str->StringLength(), seed);	
}

uint16_t true_random()
{
   static int inited = 0;
   uint64_t time_now;

   if (!inited)
   {
	   time_now = get_SysGetRunTime();
	   time_now = time_now / UUIDS_PER_TICK;
	   srand((unsigned int)(((time_now >> 32) ^ time_now) & 0xffffffff));
	   inited = 1;
   }

   return rand();
}

BUuid::BUuid(const SContext& context)
	:	BnUuid(context)
{
}

void BUuid::InitAtom()
{
	m_nextSave = get_current_time();

	SValue state = Context().Lookup(BV_SETTINGS);
	if (state == B_UNDEFINED_VALUE)
	{
		uint8_t seed[16];
		get_random_info(seed);
		seed[0] |= 0x01;
		memcpy(&m_state.node, seed, sizeof(m_state.node));
		m_state.clockseq = 0;
		m_state.timestamp = 0;
		Context().Publish(BV_SETTINGS, SValue(B_RAW_TYPE, &m_state, sizeof(m_state)));
	}
	else
	{
		memcpy(&m_state, state.Data(), sizeof(m_state));	
	}
}

SValue BUuid::Create()
{
	SLocker::Autolock lock(m_lock);
	
	uint64_t timestamp = get_current_time();
	
	/* if no NV state, or if clock went backwards, or node ID changed (e.g., new network card) change clockseq */
	if (m_state.clockseq == 0)
		m_state.clockseq = true_random();
	else if (timestamp < m_state.timestamp)
		m_state.clockseq++;

	m_state.timestamp = timestamp;

	if (timestamp >= m_nextSave)
	{
		Context().Publish(BV_SETTINGS, SValue(B_RAW_TYPE, &m_state, sizeof(m_state)));
		/* schedule next save for 10 seconds from now */
		m_nextSave = timestamp + (10 * 10 * 1000 * 1000);
	}

	uuid_t uuid;
	uuid.time_low = (unsigned long)(m_state.timestamp & 0xFFFFFFFF);
	uuid.time_mid = (unsigned short)((m_state.timestamp >> 32) & 0xFFFF);
	uuid.time_hi_and_version = (unsigned short)((m_state.timestamp >> 48) & 0x0FFF);
	uuid.time_hi_and_version |= (1 << 12);
	uuid.clock_seq_low = m_state.clockseq & 0xFF;
	uuid.clock_seq_hi_and_reserved = (m_state.clockseq & 0x3F00) >> 8;
	uuid.clock_seq_hi_and_reserved |= 0x80;
	memcpy(&uuid.node, &m_state.node, sizeof(uuid.node));

	return SValue(B_UUID_TYPE, &uuid, sizeof(uuid));
}

