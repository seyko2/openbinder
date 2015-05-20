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

#include <support/StdIO.h>
#include <support/Looper.h>
#include <SysThreadConcealed.h>

#include <syslog.h>

int main(int argc, char* argv[])
{
	//openlog("binderproc", LOG_PERROR | LOG_NDELAY, LOG_USER);
	bout << "START: binderproc #" << SysProcessID() << " " << (argc > 1 ? argv[1] : "(unnamed)") << endl;
	SLooper* loop = SLooper::This();
	sptr<BProcess> proc(loop->Process());
	loop->SendRootObject(proc->AsBinder());
	loop->Loop(true);
	bout << "EXIT: binderproc #" << SysProcessID() << " " << (argc > 1 ? argv[1] : "(unnamed)") << endl;
	return 0;
}
