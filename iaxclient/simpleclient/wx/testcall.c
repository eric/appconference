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

int levels_callback(float input, float output);


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


void usage()
{ 
#if 0
    fprintf(stderr, "Usage is XXX\n");
#endif
    exit(1);
}

int doTestCall(int argc, char **argv)
{
	char c;
	int i;
	char *dest = "guest@ast1";
	int do_levels = 1;
	double silence_threshold = -99;


	for(i=1;i<argc;i++)
	{
	   if(argv[i][0] == '-') 
	   {
	      switch(tolower(argv[i][1]))
	      {
		case 'v':
		  do_levels = 1;
		  break;
		case 's':
		  if(i+1 >= argc) usage();
		  silence_threshold = atof(argv[++i]);
		  break;
		default:
		  usage();
	      }
	    } else {
	      dest=argv[i];
	    }
	}


	/* activate the exit handler */
	atexit(killem);
	
	iaxc_initialize(AUDIO_INTERNAL_PA);
	iaxc_set_encode_format(IAXC_FORMAT_GSM);
	iaxc_set_silence_threshold(silence_threshold);

	if(do_levels)
	  iaxc_set_levels_callback(levels_callback); 

	iaxc_start_processing_thread();

#if 0		

	printf("settings: \n");
	printf("\tsilence threshold: %f\n", silence_threshold);
	printf("\tlevel output: %s\n", do_levels ? "on" : "off");
	fprintf(f, "\n\
	    TestCall accept some keyboard input while it's running.\n\
	    You must hit 'enter' for your keypresses to be recognized,\n\
	    although you can type more than one key on a line\n\
\n\
	    q: drop the call and hangup.\n\
	    0-9 * or #: dial those DTMF digits.\n");
	fprintf(f, "Calling %s\n", dest);
	

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
#endif
	return 0;
}
