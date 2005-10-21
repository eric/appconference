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
	g i		list audio input devices
	g o		list audio output devices
	g f		get current filters (bitmask as defined in iaxclient.h)

	s i <number> <name>
			set caller id info
	s m <on|off>	monitor audio levels
	s a <on|off>	monitor command results
	s h <on|off>	monitor hotkey status (if USE_HOTKEY defined)
	s p <level>	set playback level (% of max)
	s r <level>	set record level (% of max)
	s d <c>		set event delimiter to character <c> (default=' ')
	s t <n>		set silence threshold (not implemented)
	s i <devno>	set current input device to devno
	s o <devno>	set current output device to devno
	s f <filters>	set audio filters (as defined in iaxclient.h)
	s c u|g|s	set preferred codec: (u)law, (g)sm, (s)peex

	# ??		call transfer (not implemented)
   Status is returned by reading stdin.  tokens in the return value are
   delimited with "set delim X".  X defaults to "\t".
   The status returned is one of:
        1|0		command result (if "set ack on")
	L <input> <output>
			report audio levels (in db).  Enabled|diabled with:
			"set monitor on|off"
	H 1|0		Hotkey is pressed (1) not pressed (0)
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

#ifdef USE_HOTKEY
#ifdef WIN32
#else
#ifdef MACOSX
#include <Carbon/Carbon.h>
#else
/* GTK */
#include <gdk/gdk.h>

static GdkWindow *hotkeywindow;
#endif
#endif
#endif

#define DELIM " \n"	/* command token delimiters */
#define ACK   "1"	/* command succeeded */
#define NAK   "0"	/* command failed */

void event_level(double in, double out);
void event_state(int state, char *rem, char *rem_name, char *loc, char *ln);
void event_text(int type, char *text);
void event_netstat(int callno, int rtt, int r_jitter, int r_losspct, int r_losscnt, 
		                        int r_packets, int r_delay, int r_dropped, int r_ooo,
                                        int l_jitter, int l_losspct, int l_losscnt, 
                                        int l_packets, int l_delay, int l_dropped, int l_ooo);

void event_unknown(int type);
void report(char *text);
char *map_state(int state);
int set_device(char *name, int out);

extern void tone_dtmf(char tone, int samples, double vol, short *data);

static char delim='\t';		/* output field delimiter */
static int last_state=0;	/* previous state of the channel */
static char states[256];	/* buffer to hold ascii states */
static char tmp[1024];		/* report output buffer */
static int show_levels=0;	/* report volume levels in db */
static int show_ack_nak=0;	/* report command success/failure */
static int show_hotkey=0;	/* report hotkey status */

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

/* get the state of a "hot key" (presently hardcoded as the control-key" 
 * Can be used by clients to implement "push-to-talk" functionality
 */
int hotkeystate(void) {
    int pressed = 0;
#ifdef USE_HOTKEY
#ifdef MACOSX
    KeyMap theKeys;
    GetKeys(theKeys);
    // that's the Control Key (by experimentation!)
    pressed = theKeys[1] & 0x08;
#else
#ifdef WIN32
    pressed = GetAsyncKeyState(VK_CONTROL)&0x8000;
#else /* GTK */
    int x, y;
    GdkModifierType modifiers;
    gdk_window_get_pointer(hotkeywindow, &x, &y, &modifiers);
    pressed = modifiers & GDK_CONTROL_MASK;
#endif
#endif
#endif
    return pressed;
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
        case IAXC_EVENT_NETSTAT:
	  event_netstat(e.ev.netstats.callNo, e.ev.netstats.rtt,
			e.ev.netstats.local.jitter, e.ev.netstats.local.losspct,
			e.ev.netstats.local.losscnt, e.ev.netstats.local.packets,
			e.ev.netstats.local.delay, e.ev.netstats.local.dropped,
			e.ev.netstats.local.ooo,
			e.ev.netstats.remote.jitter, e.ev.netstats.remote.losspct,
			e.ev.netstats.remote.losscnt, e.ev.netstats.remote.packets,
			e.ev.netstats.remote.delay, e.ev.netstats.remote.dropped,
			e.ev.netstats.remote.ooo);
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
	snprintf(tmp, sizeof(tmp), "L%c%.1f%c%.1f", delim, in, delim, out);
	report(tmp);
    }
    if(show_hotkey) {
	snprintf(tmp, sizeof(tmp), "H%c%c", delim, hotkeystate() ? '1' : '0');
	report(tmp);
    }

}

/*
 * State events
 */

void event_state(int state, char *remote, char *remote_name,
	char *local, char *local_context) {
    last_state=state;
    snprintf(tmp, sizeof(tmp), "S%c0x%x%c%s%c%.50s%c%.50s%c%.50s%c%.50s",
	delim, state, delim, map_state(state), delim, remote, delim,
	remote_name, delim, local, delim, local_context);
    report(tmp);
}

/*
 * Netstat Events
 */

void event_netstat(int callno, int rtt, 
		   int l_jitter, int l_losspct, int l_losscnt, 
		   int l_packets, int l_delay, int l_dropped, int l_ooo,
		   int r_jitter, int r_losspct, int r_losscnt, 
		   int r_packets, int r_delay, int r_dropped, int r_ooo) {

  snprintf(tmp, sizeof(tmp), "N%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d", delim, callno, delim, rtt,
	  delim, l_jitter, delim, l_losspct, delim, l_losscnt,
	  delim, l_packets, delim, l_delay, delim, l_dropped, delim, l_ooo,
	  delim, r_jitter, delim, r_losspct, delim, r_losscnt,
	  delim, r_packets, delim, r_delay, delim, r_dropped, delim, r_ooo);
  report(tmp);
}



/*
 * text events
 */

void event_text(int type, char *message) {
    snprintf(tmp, sizeof(tmp), "T%c%d%c%.200s", delim, type, delim, message);
    report(tmp);
}

/*
 * unknown events - this is never called
 */

void event_unknown(int type) {
    snprintf(tmp, sizeof(tmp), "U%c%d", delim, type);
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
    snprintf(tmp, sizeof(tmp),"?%c%s", delim, devs[current].name);
    for (i=0;i<ndevs; i++) {
	if (devs[i].capabilities & flag && i != current) {
	    snprintf(tmp+strlen(tmp), sizeof(tmp)-strlen(tmp), "%c%s",delim,devs[i].name);
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

#if defined(USE_HOTKEY) && !defined(MACOSX) && !defined(WIN32)
    // window used for getting keyboard state
    GdkWindowAttr attr;
    gdk_init(&argc, &argv);
    hotkeywindow = gdk_window_new(NULL, &attr, 0);
#endif

    atexit(iaxc_shutdown); /* activate the exit handler */
    if (iaxc_initialize(AUDIO_INTERNAL_PA,1)) {
	fatal_error("cannot initialize iaxclient!");
    }

    iaxc_set_silence_threshold(-99.0); /* the default */
    iaxc_set_audio_output(0);	/* the default */
    iaxc_set_event_callback(iaxc_callback); 
    iaxc_start_processing_thread();

    report("? Ready");
    while (fgets(line,sizeof(line),stdin)) {
	/* fprintf(stderr, "GOT: %s\n", line); */
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
	    snprintf(tmp, sizeof(tmp), "?%c%s", delim, map_state(last_state));
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
                    snprintf(tmp, sizeof(tmp), "?%c%d", delim,
			    (int)(100*iaxc_input_level_get()+.5));
		    report(tmp);
		break;
		case 'p':	/* audio output level */
                    snprintf(tmp, sizeof(tmp), "?%c%d", delim,
				(int)(100*iaxc_output_level_get()+.5));
		    report(tmp);
		break;
		case 'i':	/* audio input devices */
		case 'o':	/* audio output devices */
		    report(report_devices(*token=='i'));
		break;
		case 'f':
		    snprintf(tmp, sizeof(tmp), "?%c%d", delim,
				iaxc_get_filters());	
		    report(tmp);
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
		case 'h':	/* show hotkey state */
		    arg = strtok(NULL, DELIM);	/* 3rd token */
		    if (arg != NULL && strcmp(arg, "on")==0) {
		        if (*token=='m') {
			    fprintf(stderr, "Monitoring is ON\n");
			    show_levels=1;
#ifdef USE_HOTKEY
			} else if (*token == 'h') {
			    show_hotkey=1;
#endif
			} else {
			    show_ack_nak=1;
			}
			ack();
		    } else if (arg != NULL &&  strcmp(arg, "off")==0) {
		        if (*token=='m') {
			    fprintf(stderr, "Monitoring is OFF\n");
			    show_levels=0;
			} else if (*token == 'h') {
			    show_hotkey=0;
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
		case 'c':	/* set preferred codec */
		    arg = strtok(NULL, DELIM);	/* 3rd token */
		    if (!arg) {
			break;
		        nak();
		    }
		    if (*arg == 's') {
			value = IAXC_FORMAT_SPEEX;
		    } else if (*arg == 'u') {
			value = IAXC_FORMAT_ULAW;
		    } else {
			value = IAXC_FORMAT_GSM;
		    }
		    iaxc_set_formats(value,
			    IAXC_FORMAT_ULAW|IAXC_FORMAT_GSM|IAXC_FORMAT_SPEEX);
		    ack();
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
		case 'f':
		    arg = strtok(NULL, "\n");	/* 3rd token */
		    if (arg != NULL && (value=atoi(arg))>=0) {
			ack();
			iaxc_set_filters(value);
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
