/* iaxcli: A command line interface to the iax client library.
 * Copyright 2004 Sun Microsystems, by Stephen Uhler
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: Redistribution of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * Neither the name of Sun Microsystems, Inc. or the names of
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission. This software
 * is provided "AS IS," without a warranty of any kind.  ALL EXPRESS OR
 * IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN MIDROSYSTEMS, INC.
 * ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED
 * BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING THIS
 * SOFTWARE OR ITS DERIVATIVES. IN NO EVENT WILL SUN OR ITS LICENSORS BE
 * LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA, OR FOR DIRECT, INDIRECT,
 * SPECIAL, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED
 * AND REGARDLESS OF THE THEORY OF LIABILITY, ARISING OUT OF THE USE OF
 * OR INABILITY TO USE THIS SOFTWARE, EVEN IF SUN HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES. You acknowledge that this software is not
 * designed, licensed or intended for use in the design, construction,
 * operation or maintenance of any nuclear facility.
 */

/*
 * This is a "command line" based iax client.  It is intended to be
 * driven by a GUI by reading/writing its stdin/stdout.
 * CLI Commands (terminated by \n on stdin):
	q		quit
	t <n>		send dtmf digit <n>
	a		answer a call
	d <usr:pass@server/nnn>
			dial a number (using iax protocol)
	h		hangup a call
	x		reject an incoming call
	?		return current call status: one or more of:
			active,outgoing,ringing,complete,selected
	r <usr> <pass> <server> 
			register with a server
	g r		return audio record level (% of max)
	g p		return audio play level (% of max)
	s i <number> <name>
			set caller id info
	s m <on|off>	monitor audio levels
	s a <on|off>	monitor command results
	s p <level>	set playback level (% of max)
	s r <level>	set record level (% of max)
	s d <c>		set event delimiter to character <c> (default=' ')
	s t <n>		set silence threshold (not implemented)
	# ??		call transfer (not implemented)
   Status is returned by reading stdin.  tokens in the return value are
   delimited with "set delim X".  X defaults to "\t".
   The status returned is one of:
        1|0		command result (if "set ack on")
	L <input> <output>
			report audio levels (in db).  Enabled|diabled with:
			"set monitor on|off"
	T <type> <message>
			text event.
	S <state_code> <state> <remote> <remote_name> <local> <local_context>
			state change event
			state_code:	hex value of state
			state:		same as "?"
			remote:		caller number
			remote_name:	caller name
			local		local extension called (e.g. s)
			locate_context	asterisk context
	? <request response>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#include "iaxclient.h"

#define DELIM " \n"	/* command token delimiters */
#define ACK   "1"	/* command succeeded */
#define NAK   "0"	/* command failed */

void event_level(double in, double out);
void event_state(int state, char *rem, char *rem_name, char *loc, char *ln);
void event_text(int type, char *text);
void event_unknown(int type);
void report(char *text);
char *map_state(int state);
int set_device(char *name, int out);

extern void tone_dtmf(char tone, int samples, double vol, short *data);

static char delim='\t';		/* output field delimiter */
static int last_state=0;	/* previous state of the channel */
static char states[256];	/* buffer to hold ascii states */
static char tmp[256];		/* report output buffer */
static int show_levels=0;	/* report volume levels in db */
static int show_ack_nak=0;	/* report command success/failure */

void fatal_error(char *err) {
    iaxc_shutdown();
    fprintf(stderr, "FATAL ERROR: %s\n", err);
    exit(1);
}

static char *map[] = {
    "unknown", "active", "outgoing", "ringing", "complete", "selected", NULL
};

char *map_state(int state) {
    int i, j;
    int next=0;
    *states = '\0';

    if (state == 0) {
	return "free";
    }
    for(i=0, j=1; map[i] != NULL; i++, j<<=1) {
	if (state&j) {
	    if (next) strcat(states, ",");
	    strcat(states, map[i]);
	    next = 1;
	}
    }
    return states;
}

/*
 * all iax callbacks come here
 */

int iaxc_callback(iaxc_event e) {
    switch(e.type) {
        case IAXC_EVENT_LEVELS:
	    event_level(e.ev.levels.input, e.ev.levels.output);
	    break;
        case IAXC_EVENT_TEXT:
	    event_text(e.ev.text.type, e.ev.text.message);
	    break;
        case IAXC_EVENT_STATE:
	    event_state(e.ev.call.state, e.ev.call.remote,
		    e.ev.call.remote_name, e.ev.call.local,
		    e.ev.call.local_context);
	    break;
        default:
	    event_unknown(e.type);
	    break;
    }
    return 1;
}

/*
 * volume levels
 */

void event_level(double in, double out) {
    if (show_levels) {
	sprintf(tmp, "L%c%.1f%c%.1f", delim, in, delim, out);
	report(tmp);
    }
}

/*
 * State events
 */

void event_state(int state, char *remote, char *remote_name,
	char *local, char *local_context) {
    last_state=state;
    sprintf(tmp, "S%c0x%x%c%s%c%.50s%c%.50s%c%.50s%c%.50s",
	delim, state, delim, map_state(state), delim, remote, delim,
	remote_name, delim, local, delim, local_context);
    report(tmp);
}

/*
 * text events
 */

void event_text(int type, char *message) {
    sprintf(tmp, "T%c%d%c%.200s", delim, type, delim, message);
    report(tmp);
}

/*
 * unknown events - this is never called
 */

void event_unknown(int type) {
    sprintf(tmp, "U%c%d", delim, type);
    report(tmp);
}

/*
 * report an event to the gui
 */

void report(char *text) {
    printf("%s\n", text);
    fflush(stdout);
}

/*
 * ack/nak a command
 */

void ack() {
    if (show_ack_nak) {
	report(NAK);
    }
}

void nak() {
    if (show_ack_nak) {
	report(ACK);
    }
}

/*
 * report available audio devices, current first
 */

char *
report_devices(int in) {
    struct iaxc_audio_device *devs; /* audio devices */
    int ndevs;	/* audio dedvice count */
    int input, output, ring;	/* audio device id's */
    int current, i;
    int flag = in ? IAXC_AD_INPUT : IAXC_AD_OUTPUT;
    iaxc_audio_devices_get(&devs,&ndevs,&input,&output,&ring);
    current = in ? input : output;
    sprintf(tmp,"?%c%s", delim, devs[current].name);
    for (i=0;i<ndevs; i++) {
	if (devs[i].capabilities & flag && i != current) {
	    sprintf(tmp+strlen(tmp), "%c%s",delim,devs[i].name);
	}
    }
    return tmp;
}

/*
 * Select a device for input or output
 */

int set_device(char *name, int out) {
    struct iaxc_audio_device *devs; /* audio devices */
    int ndevs;	/* audio dedvice count */
    int input, output, ring;	/* audio device id's */
    int i;

    iaxc_audio_devices_get(&devs,&ndevs,&input,&output,&ring);
    for (i=0;i<ndevs; i++) {
	if (devs[i].capabilities & (out ? IAXC_AD_OUTPUT:IAXC_AD_INPUT) &&
	strcmp(name, devs[i].name)==0) {
            if (out) {
                output = devs[i].devID;
            } else {
                input = devs[i].devID;
            }
            fprintf(stderr, "device %s = %s (%d)\n", out?"out":"in", name,
			devs[i].devID);
            iaxc_audio_devices_set(input, output, ring);
            return 1;
        }
    }
    return 0;
}


int main(int argc, char **argv) {
    char line[256];

    atexit(iaxc_shutdown); /* activate the exit handler */
    if (iaxc_initialize(AUDIO_INTERNAL_PA,1)) {
	fatal_error("cannot initialize iaxclient!");
    }

    iaxc_set_encode_format(IAXC_FORMAT_GSM);
    iaxc_set_silence_threshold(-99.0); /* the default */
    iaxc_set_audio_output(0);	/* the default */
    iaxc_set_event_callback(iaxc_callback); 
    iaxc_start_processing_thread();

    report("? Ready");
    while (fgets(line,sizeof(line),stdin)) {
	char *cmd = strtok(line, DELIM);	/* 1st token */
	char *token = strtok(NULL, DELIM);	/* 2nd token */
	char *arg;				/* another token */
	int value;				/* an integer value */
	if (cmd == NULL) {
	    continue;
	}
	switch (*cmd) {
	case 'q':	/* quit */
	    iaxc_dump_call();
	    iaxc_millisleep(1000);
	    iaxc_stop_processing_thread();
	    report("X");
	    exit(0);
	case '#':	/* transfer */
	    iaxc_blind_transfer_call(0, token); /* XXX broken? */
	    ack();
	break;		
	case 't':	/* send tone */
	    if (token != NULL) {
		iaxc_send_dtmf(*token);
		ack();
	    } else {
		nak();
	    }
        break;
	case 'p': {	/* play tone locally (experimental) */
            struct iaxc_sound sound; /* sound to play */
	    short buff[2000]; /* touch tone buffer */
	    sound.data=buff;
	    sound.len = 2000;
	    sound.malloced=0;
	    sound.repeat=0;
            tone_dtmf(*token,1600,100.0, buff);
	    tone_dtmf('X',400,100.0, buff+1600);
	    iaxc_play_sound(&sound, 0);
	   }
	   break;
	case 'a':	/* answer a call */
	  iaxc_answer_call(0);
	  iaxc_select_call(0);
	  ack();
        break;
	case 'd':	/* dial a number */
	    if (token != NULL) {
		iaxc_call(token);
		iaxc_select_call(0);
		ack();
	    } else {
		nak();
	    }
        break;
	case 'h':	/* hangup a call */
	    iaxc_dump_call();
	    ack();
	break;
	case 'x':	/* reject a call */
	    iaxc_select_call(0);
	    iaxc_reject_call();
	    ack();
	case '?':	/* call status */
	    sprintf(tmp, "?%c%s", delim, map_state(last_state));
	    report(tmp);
	break;
	case 'r':	/* register */
	    {
		char *user = token;
		char *pass =  strtok(NULL, DELIM);
		char *host = strtok(NULL, DELIM);
		if (user !=NULL && pass != NULL && host!= NULL) {
		    iaxc_register(user, pass, host);
		    ack();
		} else {
		    nak();
		}
	    }
	break;
	case 'g':	/* get some parameter */
	    if (token == NULL) {
		nak();
		break;
	    }
	    arg = strtok(NULL, DELIM);	/* 3rd token */
	    switch (*token) {
		case 'r':	/* audio input level */
                    sprintf(tmp, "?%c%d", delim,
			    (int)(100*iaxc_input_level_get()+.5));
		    report(tmp);
		break;
		case 'p':	/* audio output level */
                    sprintf(tmp, "?%c%d", delim,
				(int)(100*iaxc_output_level_get()+.5));
		    report(tmp);
		break;
		case 'i':	/* audio input devices */
		case 'o':	/* audio output devices */
		    report(report_devices(*token=='i'));
		break;
		default:
		    nak();
		break;
	    }
	break;
	case 's':	/* set some parameter */
	    if (token == NULL) {
		nak();
		break;
	    }
	    switch (*token) {
		case 'n':	/* caller id */
		    arg = strtok(NULL, DELIM);	/* 3rd token */
		    if (arg != NULL) {
			char *name =  strtok(NULL, "\n");
			if (name==NULL) {
			    name="unknown";
			}
			/* fprintf(stderr, "ID: (%s) <%s>\n", name, arg); */
			iaxc_set_callerid(name, arg);
			ack();
		    } else {
			nak();
		    }
		break;
		case 'm':	/* monitor voice  on/off */
		case 'a':	/* ack/nak  on/off */
		    arg = strtok(NULL, DELIM);	/* 3rd token */
		    if (arg != NULL && strcmp(arg, "on")==0) {
		        if (*token=='m') {
			    fprintf(stderr, "Monitoring is ON\n");
			    show_levels=1;
			} else {
			    show_ack_nak=1;
			}
			ack();
		    } else if (arg != NULL &&  strcmp(arg, "off")==0) {
		        if (*token=='m') {
			    fprintf(stderr, "Monitoring is OFF\n");
			    show_levels=0;
			} else {
			    show_ack_nak=0;
			}
			ack();
		    } else {
			nak();
		    }
		    break;
		case 'p':	/* play volume (%) */
		case 'r':	/* record volume (%) */
		    arg = strtok(NULL, DELIM);	/* 3rd token */
		    if (arg != NULL && (value=atoi(arg))>=0 && value<=100) {
			ack();
			if (*token=='r') {
			    iaxc_input_level_set(((double)value)/100.0);
			} else {
			    iaxc_output_level_set((double)value/100.0);
			}
		    } else {
			nak();
		    }
		break;
		case 'd':	/* set delimiter */
		    arg = strtok(NULL, DELIM);	/* 3rd token */
		    delim = arg==NULL ? ' ':*arg;
		    ack();
		break;
		case 't':	/* silence threshold */
		break;
		case 'i':	/* audio input device */
		case 'o':	/* audio output device */
		    arg = strtok(NULL, "\n");	/* 3rd token */
                    if (set_device(arg, *token=='o'?1:0)) {
                        ack();
		    } else {
                        nak();
                    }
		break;
		default:
		    nak();
		break;
	    }
        break;
	default:
	   nak();
	break;
	}
    }
    return 0;
}
