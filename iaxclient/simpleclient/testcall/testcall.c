/*
 * testcall: make a single test call with IAXCLIENT
 *
 * IAX Support for talking to Asterisk and other Gnophone clients
 *
 * Copyright (C) 1999, Linux Support Services, Inc.
 *
 * Mark Spencer <markster@linux-support.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU Lesser (Library) General Public License
 */

/* #define	PRINTCHUCK /* enable this to indicate chucked incomming packets */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#include "iaxclient.h"

static int answered_call;

/* routine called at exit to shutdown audio I/O and close nicely.
NOTE: If all this isnt done, the system doesnt not handle this
cleanly and has to be rebooted. What a pile of doo doo!! */
void killem(void)
{
	iax_shutdown();

	return;
}


void mysleep(void)
{
#ifdef POSIXSLEEP
	struct timespec req;
	
	req.tv_nsec=10*1000*1000;  /* 10 ms */
	req.tv_sec=0;

	/* yes, it can return early.  We don't care */
	nanosleep(&req,NULL);
#else
	Sleep(10); /* WinAPI sleep */
#endif

}

int main(int argc, char *argv[])
{
	FILE *f;
	char *dest;
	unsigned long lastouttick = 0;


	f = stdout;

	if(argc > 1)
		dest=argv[1];
	else
		dest="guest@10.23.1.31/9999";

	/* activate the exit handler */
	atexit(killem);
	
	iaxc_initialize(AUDIO_INTERNAL_PA, f);
	iaxc_set_encode_format(IAXC_FORMAT_GSM);

	fprintf(f, "TestCall \n\n");
	fprintf(f, "Calling %s\nHit ^C to quit\n", dest);
	
	iaxc_call(f,dest);
		

	/* main tight loop */
	while(1) {
		iaxc_process_calls();
		answered_call = iaxc_was_call_answered();
		mysleep();
	}

	return 0;
}
