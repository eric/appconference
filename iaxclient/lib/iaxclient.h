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




int iaxc_initialize(int audType);
void iaxc_shutdown();
void iaxc_set_encode_format(int fmt);
void iaxc_process_calls();
int iaxc_service_audio();
void iaxc_call(char *num);
void iaxc_answer_call(void); 
void iaxc_dump_call(void);
void iaxc_reject_call(void);
void iaxc_send_dtmf(char digit);
int iaxc_was_call_answered();
void iaxc_millisleep(long ms);

typedef int (*iaxc_levels_callback_t)(float input, float output);
void iaxc_set_levels_callback(iaxc_levels_callback_t func);

typedef void (*iaxc_message_callback_t)(char *);
void iaxc_set_status_callback(iaxc_message_callback_t func);
void iaxc_set_error_callback(iaxc_message_callback_t func);


#ifdef __cplusplus
}
#endif

#endif
