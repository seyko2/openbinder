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

#include "bomb_host.h"
#include <support/Looper.h>
#include <support/Process.h>
#include <support/StdIO.h>
#include <app/ICommand.h>
#include <stdio.h>
#include <unistd.h>

static int
usage()
{
	fprintf(stderr, "usage: bomber HOST BREADTH DEPTH\n");
	fprintf(stderr, "    HOST: Path in the context for the bomb host\n");
	fprintf(stderr, "    BREADTH: How many children each should spawn\n");
	fprintf(stderr, "    DEPTH: How many times we'll do this before stopping\n");
	return 1;
}

int
main(int argc, char **argv)
{
	int32_t breadth;
	int32_t depth;
	SString host_path;

	if (argc != 4) {
		return usage();
	}
	host_path = argv[1];
	breadth = atoi(argv[2]);
	depth = atoi(argv[3]);

	pid_t pid = getpid();
	
	printf("pid=%d host_path=%s breadth=%d depth=%d\n", pid, host_path.String(), breadth, depth);

	if (host_path.Length() == 0 || breadth <= 0 || depth <= 0) {
		return usage();
	}

	// get my object
	printf("getting context\n");
	SContext context = get_default_context();
	printf("got context\n");
	status_t err = context.InitCheck();
	if (err != B_OK) {
		fprintf(stderr, "bomber: error: couldn't get the default context: 0x%08x\n", err);
		return err;
	}
	printf("getting host\n");
	sptr<IBinder> host = context.Lookup(host_path).AsBinder();
	printf("got host\n");
	if (host == NULL) {
		fprintf(stderr, "bomber: error: couldn't get the host \"%s\"\n", host_path.String());
		return err;
	}

	printf("getting object\n");
	sptr<IBinder> object = host->Invoke(key_GiveObject, SValue(B_0_INT32, SValue::Int32(pid))).AsBinder();
	printf("got object\n");

	// reproduce
	if (depth > 1) {
		for (int32_t i=0; i<breadth; i++) {
			pid_t childpid = fork();

			if (childpid == 0)
			{
				char depth_str[10];
				sprintf(depth_str, "%d", depth-1);
				char * args[5];
				args[0] = argv[0];
				args[1] = const_cast<char *>(host_path.String());
				args[2] = argv[2];
				args[3] = depth_str;
				args[4] = NULL;

				int err = execv(argv[0], args);
			}
		}
	}
	printf("pid=%d done\n", pid);

	return 0;
}
