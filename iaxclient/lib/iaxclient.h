#ifndef _iaxclient_h
#define _iaxclient_h

#ifdef __cplusplus
extern "C" {
#endif

/* IAXCLIENT.H external library include 2003 SteveK, 
 * This file is covered by the LGPL */

/* This is the include file which declared all external API functions to
 * IAXCLIENT.  It should include all functions and declarations needed
 * by IAXCLIENT library users, but not include internal structures, or
 * require the inclusion of library internals (or sub-libraries) */

#include <stdio.h>
#ifdef WIN32
#include <windows.h>
#endif

/* Define audio type constants */
#define AUDIO_INTERNAL 0
#define AUDIO_INTERNAL_PA 1
#define AUDIO_INTERNAL_FILE 2
#define AUDIO_EXTERNAL 99

/* payload formats : WARNING: must match libiax values!!! */
/* Data formats for capabilities and frames alike */
#define IAXC_FORMAT_G723_1       (1 << 0)        /* G.723.1 compression */
#define IAXC_FORMAT_GSM          (1 << 1)        /* GSM compression */
#define IAXC_FORMAT_ULAW         (1 << 2)        /* Raw mu-law data (G.711) */
#define IAXC_FORMAT_ALAW         (1 << 3)        /* Raw A-law data (G.711) */
#define IAXC_FORMAT_G726         (1 << 4)        /* ADPCM, 32kbps  */
#define IAXC_FORMAT_ADPCM        (1 << 5)        /* ADPCM IMA */
#define IAXC_FORMAT_SLINEAR      (1 << 6)        /* Raw 16-bit Signed Linear (8000 Hz) PCM */
#define IAXC_FORMAT_LPC10        (1 << 7)        /* LPC10, 180 samples/frame */
#define IAXC_FORMAT_G729A        (1 << 8)        /* G.729a Audio */
#define IAXC_FORMAT_SPEEX        (1 << 9)        /* Speex Audio */
#define IAXC_FORMAT_ILBC         (1 << 10)       /* iLBC Audio */

#define IAXC_FORMAT_MAX_AUDIO (1 << 15)  /* Maximum audio format */
#define IAXC_FORMAT_JPEG         (1 << 16)       /* JPEG Images */
#define IAXC_FORMAT_PNG          (1 << 17)       /* PNG Images */
#define IAXC_FORMAT_H261         (1 << 18)       /* H.261 Video */
#define IAXC_FORMAT_H263         (1 << 19)       /* H.263 Video */



#define IAXC_EVENT_TEXT			1
#define IAXC_EVENT_LEVELS		2
#define IAXC_EVENT_STATE		3

#define IAXC_CALL_STATE_FREE 		0
#define IAXC_CALL_STATE_ACTIVE 		(1<<1)
#define IAXC_CALL_STATE_OUTGOING 	(1<<2)
#define IAXC_CALL_STATE_RINGING 	(1<<3)
#define IAXC_CALL_STATE_COMPLETE 	(1<<4)
#define IAXC_CALL_STATE_SELECTED 	(1<<5)

#define IAXC_TEXT_TYPE_STATUS		1
#define IAXC_TEXT_TYPE_NOTICE		2
#define IAXC_TEXT_TYPE_ERROR		3
/* FATAL ERROR: User Agent should probably display error, then die. */
#define IAXC_TEXT_TYPE_FATALERROR	4
#define IAXC_TEXT_TYPE_IAX		5



#define IAXC_EVENT_BUFSIZ	256
struct iaxc_ev_levels {
	float input;
	float output;
};

struct iaxc_ev_text {
	int type;
	int callNo; /* call number for IAX text */
	char message[IAXC_EVENT_BUFSIZ];
};

struct iaxc_ev_call_state {
	int callNo;
	int state;
	char remote[IAXC_EVENT_BUFSIZ];
	char remote_name[IAXC_EVENT_BUFSIZ];
	char local[IAXC_EVENT_BUFSIZ];
	char local_context[IAXC_EVENT_BUFSIZ];
};

typedef struct iaxc_event_struct {
	int type;
	union {
		struct iaxc_ev_levels 		levels;
		struct iaxc_ev_text 		text;
		struct iaxc_ev_call_state 	call;
	} ev;
} iaxc_event;

typedef int (*iaxc_event_callback_t)(iaxc_event e);
void iaxc_set_event_callback(iaxc_event_callback_t func);

int iaxc_initialize(int audType, int nCalls);
void iaxc_shutdown();
void iaxc_set_formats(int preferred, int allowed);
void iaxc_set_min_outgoing_framesize(int samples);
void iaxc_set_callerid(char *name, char *number);
void iaxc_process_calls();
int iaxc_service_audio();
int iaxc_start_processing_thread();
int iaxc_stop_processing_thread();
void iaxc_call(char *num);
void iaxc_register(char *user, char *pass, char *host);
void iaxc_answer_call(int callNo); 
void iaxc_blind_transfer_call(int callNo, char *number); 
void iaxc_dump_all_calls(void);
void iaxc_dump_call(void);
void iaxc_reject_call(void);
void iaxc_send_dtmf(char digit);
int iaxc_was_call_answered();
void iaxc_millisleep(long ms);
void iaxc_set_silence_threshold(double thr);
void iaxc_set_audio_output(int mode);
int iaxc_select_call(int callNo);
int iaxc_first_free_call();
int iaxc_selected_call();
int iaxc_quelch(int callNo, int MOH);
int iaxc_unquelch(int call);
int iaxc_mic_boost_get( void ) ;
int iaxc_mic_boost_set( int enable ) ;

#define IAXC_AD_INPUT           (1<<0)
#define IAXC_AD_OUTPUT          (1<<1)
#define IAXC_AD_RING            (1<<2)
#define IAXC_AD_INPUT_DEFAULT   (1<<3)
#define IAXC_AD_OUTPUT_DEFAULT  (1<<4)
#define IAXC_AD_RING_DEFAULT    (1<<5)

struct iaxc_audio_device {
	char *name;             /* name of the device */
	long capabilities;      /* flags, defined above */
	int devID;              /* driver-specific ID */
};

/* Get audio device information:
 * 	**devs: a pointer to an array of device structures, as declared above.  function
 * 	will give you a pointer to the proper array, which will be valid as long as iaxc is
 * 	initialized.
 *
 * 	*nDevs: a pointer to an int, to which the count of devices in the array devs will be
 * 	written
 *
 * 	*input, *output, *ring: the currently selected devices for input, output, ring will
 * 	be written to the int pointed to by these pointers.
 */
int iaxc_audio_devices_get(struct iaxc_audio_device **devs, int *nDevs, int *input, int *output, int *ring); 
int iaxc_audio_devices_set(int input, int output, int ring);

double iaxc_input_level_get();
double iaxc_output_level_get();
int iaxc_input_level_set(double level);
int iaxc_output_level_set(double level);


struct iaxc_sound {
	short 	*data;          /* sound data */
	long 	len;            /* length of sample */
	int 	malloced;	/* should the library free() the data after it is played? */
	int	channel;	/* 0 for outputSelected, 1 for ringSelected */
	int 	repeat;      	/* number of times to repeat (-1 = infinite) */
	long	pos;		/* internal use: current play position */
	int 	id;		/* internal use: sound ID */
	struct iaxc_sound *next; /* internal use: next in list */
};

/* play a sound.  sound = an iaxc_sound structure, ring: 0: play through output device; 1: play through "ring" device */
int iaxc_play_sound(struct iaxc_sound *sound, int ring);

/* stop sound with ID "id" */
int iaxc_stop_sound(int id);


#define IAXC_FILTER_DENOISE 	(1<<0)
#define IAXC_FILTER_AGC 	(1<<1)
#define IAXC_FILTER_ECHO 	(1<<2)
#define IAXC_FILTER_AAGC 	(1<<3) /* Analog (mixer-based) AGC */
#define IAXC_FILTER_CN 		(1<<4) /* Send CN frames when silence detected */
int iaxc_get_filters(void);
void iaxc_set_filters(int filters);

/* speex specific codec settings */
void iaxc_set_speex_settings(int decode_enhance, float quality, int bitrate, int vbr, int abr, int complexity);

#ifdef __cplusplus
}
#endif

#endif
