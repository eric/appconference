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
	iaxc_millisleep(10);
}

int levels_callback(float input, float output) {
    fprintf(stderr, "IN: %f OUT: %f\n", input, output);
}

void usage()
{ 
    fprintf(stderr, "Usage is XXX\n");
    exit(1);
}

int main(int argc, char **argv)
{
	FILE *f;
	char c;
	int i;
	char *dest = "guest@10.23.1.31/9999";
	int do_levels = 0;
	double silence_threshold = -99;


	f = stdout;

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


	printf("settings: \n");
	printf("\tsilence threshold: %f\n", silence_threshold);
	printf("\tlevel output: %s\n", do_levels ? "on" : "off");

	/* activate the exit handler */
	atexit(killem);
	
	iaxc_initialize(AUDIO_INTERNAL_PA);
	iaxc_set_encode_format(IAXC_FORMAT_GSM);
	iaxc_set_silence_threshold(silence_threshold);

	if(do_levels)
	  iaxc_set_levels_callback(levels_callback); 


	fprintf(f, "\n\
	    TestCall accept some keyboard input while it's running.\n\
	    You must hit 'enter' for your keypresses to be recognized,\n\
	    although you can type more than one key on a line\n\
\n\
	    q: drop the call and hangup.\n\
	    0-9 * or #: dial those DTMF digits.\n");
	fprintf(f, "Calling %s\n", dest);
	
	iaxc_call(dest);

	iaxc_start_processing_thread();
	printf("ready for keyboard input\n");
		
	while(c = getc(stdin)) {
	    switch (tolower(c)) {
	      case 'q':
		printf("Hanging up and exiting\n");
		iaxc_dump_call();
		iaxc_millisleep(1000);
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
