#ifndef _AUDIO_ENCODE_H
#define _AUDIO_ENCODE_H

#include <stdio.h>
#include "gsm.h"
#include "iax-client.h"

// Define audio encoding constants
#define ENCODE_GSM 1

struct peer {
	int time;
	gsm gsmin;
	gsm gsmout;

	struct iax_session *session;
	struct peer *next;
};

void send_encoded_audio(struct peer *most_recent_answer, void *data, void *fo,int iEncodeType);
int decode_audio(struct iax_event *e, struct peer *p, void *fr, int len, int iEncodeType);
int check_encoded_audio_length(struct iax_event *e, int iEncodeType);
void increment_encoded_data_count(int *i, int iEncodeType);

#endif