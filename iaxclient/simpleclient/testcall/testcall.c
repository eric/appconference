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
	iaxc_shutdown();
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

int levels_callback(float input, float output) {
    fprintf(stderr, "IN: %f OUT: %f\n", input, output);
}


int main(int argc, char *argv[])
{
	FILE *f;
	char *dest;
	char c;


	f = stdout;

	if(argc > 1)
		dest=argv[1];
	else
		dest="guest@10.23.1.31/9999";

	/* activate the exit handler */
	atexit(killem);
	
	iaxc_initialize(AUDIO_INTERNAL_PA, f);
	iaxc_set_encode_format(IAXC_FORMAT_GSM);
	/* iaxc_set_levels_callback(levels_callback); */

	fprintf(f, "\n\
	    TestCall accept some keyboard input while it's running.\n\
	    You must hit 'enter' for your keypresses to be recognized,\n\
	    although you can type more than one key on a line\n\
\n\
	    q: drop the call and hangup.\n\
	    0-9 * or #: dial those DTMF digits.\n");
	fprintf(f, "Calling %s\n", dest);
	
	iaxc_call(f,dest);

	iaxc_start_processing_thread();
	printf("ready for keyboard input\n");
		
	while(c = getc(stdin)) {
	    switch (tolower(c)) {
	      case 'q':
		printf("Hanging up and exiting\n");
		iaxc_dump_call();
		sleep(1);
		iaxc_stop_processing_thread();
		exit(0);
	      break;		

	      case '1': case '2': case '3': case '4': case '5':
	      case '6': case '7': case '8': case '9': case '0':
	      case '#': case '*':
		printf ("sending %c\n", c);
		iaxc_send_dtmf(c);
	      break;
	    }
	}

	return 0;
}
