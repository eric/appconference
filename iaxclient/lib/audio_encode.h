#ifndef _AUDIO_ENCODE_H
#define _AUDIO_ENCODE_H

#include <stdio.h>
#include "iaxclient_lib.h"
#include "gsm.h"

// Define audio encoding constants
#define ENCODE_GSM 1

extern int iaxc_filters;

int send_encoded_audio(struct iaxc_call *most_recent_answer, void *data, int iEncodeType, int samples);
int decode_audio(struct iaxc_call *p, void *out, void *data, int len, int iEncodeType, int *samples);
int check_encoded_audio_length(struct iax_event *e, int iEncodeType);
void increment_encoded_data_count(int *i, int iEncodeType);
void iaxc_set_speex_filters(void);


#endif

