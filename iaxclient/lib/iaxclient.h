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
#define AUDIO_EXTERNAL 99

/* payload formats : WARNING: must match libiax values!!! */
/* Data formats for capabilities and frames alike */
#define IAXC_FORMAT_G723_1       (1 << 0)        /* G.723.1 compression */
#define IAXC_FORMAT_GSM          (1 << 1)        /* GSM compression */
#define IAXC_FORMAT_ULAW         (1 << 2)        /* Raw mu-law data (G.711) */
#define IAXC_FORMAT_ALAW         (1 << 3)        /* Raw A-law data (G.711) */
#define IAXC_FORMAT_MP3          (1 << 4)        /* MPEG-2 layer 3 */
#define IAXC_FORMAT_ADPCM        (1 << 5)        /* ADPCM (whose?) */
#define IAXC_FORMAT_SLINEAR      (1 << 6)        /* Raw 16-bit Signed Linear (8000 Hz) PCM */
#define IAXC_FORMAT_LPC10        (1 << 7)        /* LPC10, 180 samples/frame */
#define IAXC_FORMAT_G729A        (1 << 8)        /* G.729a Audio */

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



#define IAXC_EVENT_BUFSIZ	256
struct iaxc_ev_levels {
	float input;
	float output;
};

struct iaxc_ev_text {
	int type;
	char message[IAXC_EVENT_BUFSIZ];
};

struct iaxc_ev_call_state {
	int callNo;
	int state;
	char remote[IAXC_EVENT_BUFSIZ];
	char local[IAXC_EVENT_BUFSIZ];
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
void iaxc_set_encode_format(int fmt);
void iaxc_process_calls();
int iaxc_service_audio();
int iaxc_start_processing_thread();
int iaxc_stop_processing_thread();
void iaxc_call(char *num);
void iaxc_register(char *user, char *pass, char *host);
void iaxc_answer_call(int callNo); 
void iaxc_dump_call(void);
void iaxc_reject_call(void);
void iaxc_send_dtmf(char digit);
int iaxc_was_call_answered();
void iaxc_millisleep(long ms);
void iaxc_set_silence_threshold(double thr);
void iaxc_set_audio_output(int mode);
int iaxc_select_call(int callNo);

#ifdef __cplusplus
}
#endif

#endif
