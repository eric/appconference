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
static char *output_filename = NULL;
int do_levels = 0;

/* routine called at exit to shutdown audio I/O and close nicely.
NOTE: If all this isnt done, the system doesnt not handle this
cleanly and has to be rebooted. What a pile of doo doo!! */
void killem(void)
{
	iaxc_shutdown();
	return;
}

void fatal_error(char *err) {
	killem();
	fprintf(stderr, "FATAL ERROR: %s\n", err);
	exit(1);
}

void mysleep(void)
{
	iaxc_millisleep(10);
}

int levels_callback(float input, float output) {
    if(do_levels) fprintf(stderr, "IN: %f OUT: %f\n", input, output);
}

int iaxc_callback(iaxc_event e)
{
    switch(e.type) {
        case IAXC_EVENT_LEVELS:
            return levels_callback(e.ev.levels.input, e.ev.levels.output);
        case IAXC_EVENT_TEXT:
            return 0; // don't handle
        case IAXC_EVENT_STATE:
            return 0;
        default:
            return 0;  // not handled
    }
}

void list_devices()
{
    struct iaxc_audio_device *devs;
    int nDevs, input, output, ring;
    int i;

    iaxc_audio_devices_get(&devs,&nDevs, &input, &output, &ring);
    for(i=0;i<nDevs;i++) {
	fprintf(stderr, "DEVICE ID=%d NAME=%s CAPS=%x\n", devs[i].devID, devs[i].name, devs[i].capabilities);
    }
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
		case 'f':
		  if(i+1 >= argc) usage();
		  output_filename = argv[++i];
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
	
	if(output_filename) {
	  FILE *outfile;
	  if(iaxc_initialize(AUDIO_INTERNAL_FILE,1))
	    fatal_error("cannot initialize iaxclient!");
	  outfile = fopen(output_filename,"w");
	  file_set_files(NULL, outfile);
	} else {
	  if(iaxc_initialize(AUDIO_INTERNAL_PA,1))
	    fatal_error("cannot initialize iaxclient!");
	}

	iaxc_set_formats(IAXC_FORMAT_SPEEX,IAXC_FORMAT_ULAW|IAXC_FORMAT_GSM|IAXC_FORMAT_SPEEX);
	iaxc_set_silence_threshold(silence_threshold);

	list_devices();

	if(do_levels)
	  iaxc_set_event_callback(iaxc_callback); 


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
	
	if(output_filename) {
	    for(;;)
	      sleep(10);
	}
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
