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
#include <storage/BDatabaseStore.h>
#include <support/MemoryStore.h>

#include "FortuneCommand.h"

#include <SysUtils.h>

#include <CPMLib.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
using namespace palmos::storage;
using namespace palmos::app;
#endif

BFortune::BFortune(const SContext& context)
	:	BCommand(context)
{
}

float frandom_0_1(status_t * errP) {
	/*
	// Non-CPM lame version:
	return (float)SysRandom(B_NS2MS(KALGetTime(B_TIMEBASE_RUN_TIME)) % sysRandomMax) 
		/ (float)sysRandomMax;
	*/
	uint16_t randval;
	uint32_t buflen = sizeof(randval);
	status_t err = CPMLibOpen(NULL);
	if (err != errNone && err != cpmErrAlreadyOpen) {
		if (errP) *errP = err;
		return 0.0;
	}

	uint64_t now = B_NS2MS(KALGetTime(B_TIMEBASE_RUN_TIME));
	
	err = CPMLibAddRandomSeed((uint8_t*)(&now), sizeof(now));
	err = CPMLibGenerateRandomBytes((uint8_t*)&randval, &buflen);
	if (err != errNone) {
		if (errP) *errP = err;
		return 0.0;
	}


	(void) CPMLibClose();

	*errP = errNone;
	
	return ((float)randval / (float)0xFFFF);
}

SString BFortune::GetFortuneFromFile(SString filename, int32_t selectedFortune)
{
	SString s("");
	sptr<BDatabaseStore> store = new(B_SNS(std::) nothrow) BDatabaseStore('libr', 'fort', 'text', 1000);

	if (store != NULL && store->ErrorCheck() == B_OK) {
		// Note: this is so ugly I had to wash my keyboard when I was
		// done.  Hopefully we can fix this sptr<IFoo>(bar.ptr()) stuff
		// soon.
		sptr<BTextInput> in = new BTextInput(sptr<IByteInput>(store.ptr()));
		ssize_t read;
		SString line("");
		bool incomment = true;
		int32_t numCookies = 0;

		while ( (read = in->AppendLineTo(&line)) != EOF ) {
			// NOTE: Difference from BSD fortune syntax:  Any line
			// starting with %% will be considered a delimiter.
			if (strncmp(line.String(), "%%", 2) == 0) {
				if (!incomment) {
					// We combine adjacent %%-starting lines.  We don't
					// want empty fortunes.
					incomment = true;
				}
			} else {
				if (incomment && line != "\n") {
					numCookies ++;
					incomment = false;
				}
			}
			line.SetTo("");
		}

		//bout << "numCookies: " << numCookies << endl;
		
		if (numCookies > 0) {
			status_t err = errNone;
			if (selectedFortune == BFortune::kRandomFortune) {
				selectedFortune = 
					(int32_t)(
						frandom_0_1(&err) * numCookies
					);
				if (err != errNone) {
					char buf[12];
					sprintf(buf,"0x%08lx", err);
					s 	<< "fortune: error: system too deterministic (CPMLib error " 
						<< buf << ")\n";
					return s;
				}
			} else {
				if (selectedFortune >= numCookies) {
					s << "fortune: error: fortune " << selectedFortune
						<< " out of range (" << numCookies << ")\n";
					return s;
				}
				s << "fortune: " << selectedFortune << " of " 
					<< numCookies << ":\n";
			}

			
			int32_t fortune = -1;
			incomment = true;
			store->Seek(0, SEEK_SET);
			// in = new BTextInput(sptr<IByteInput>(pio.ptr()));
			while ( (read = in->AppendLineTo(&line)) != EOF ) {
				if (strncmp(line.String(), "%%", 2) == 0) {
					if (fortune == selectedFortune) break;
					if (!incomment) {
						// We combine adjacent %%-starting lines.  We don't
						// want empty fortunes.
						incomment = true;
					}
				} else {
					if (incomment && line != "\n") {
						fortune ++;
						incomment = false;
					}	
					if (fortune == selectedFortune)
						s << line;
				}
				line.SetTo("");
			}
			if (s == "") {
				s << "fortune: error: could not parse fortune cookies file"
					<< " (selected "
					<< selectedFortune
					<< " of "
					<< numCookies 
					<< ")\n";
			}
		}
		if (s == "") {
			s << "fortune: error: no fortunes found in cookie file\n";
		}
	}
	
	if (s == "") {
		s << "fortune: error: could not read fortune cookies file\n";
	}
	return s;
}

SValue BFortune::Run(const SValue& args)
{
	const sptr<ITextOutput> out(TextOutput());

	if (args.HasItem(SValue::Int32(1)))
		out << GetFortuneFromFile(SString("fortunes.txt"), args[1].AsInt32());
	else
		out << GetFortuneFromFile(SString("fortunes.txt"));

	

	return SValue::Status(B_OK);
}

SString BFortune::Documentation() const
{
	return SString(
		"usage: fortune [WHICH]\n"
		"\n"
		"Prints a fortune to stdout.  If WHICH is supplied,\n"
		"that fortune (numeric index) will be printed.  Else\n"
		"a random fortune will be printed."
	);
}

sptr<IBinder> InstantiateComponent(const SString& component, const SContext& context, const SValue& args)
{
	(void)args;

	sptr<IBinder> obj = NULL;

	if (component == "")
	{
		obj = new(B_SNS(std::) nothrow) BFortune(context);
	}
	return obj;
}
