#include "audio_encode.h"

int send_encoded_audio(struct peer *most_recent_answer, void *data, int iEncodeType)
{
	gsm_frame fo;
	switch (iEncodeType) {
		case AST_FORMAT_GSM:
			if(!most_recent_answer->gsmout)
				most_recent_answer->gsmout = gsm_create();

			// encode the audio from the buffer into GSM format and send
			gsm_encode(most_recent_answer->gsmout, (short *) ((char *) data), (void *)&fo);
			break;
	}
	if(iax_send_voice(most_recent_answer->session,AST_FORMAT_GSM, (char *)&fo, sizeof(gsm_frame)) == -1) {
	      puts("Failed to send voice!");
	      return -1;
	}
	return 0;
}

int decode_audio(struct iax_event *e, struct peer *p, void *fr, int len, int iEncodeType)
{
	switch (iEncodeType) {
	case AST_FORMAT_GSM:

		if (!p->gsmin)
			p->gsmin = gsm_create();

		return gsm_decode(p->gsmin, (char *) e->event.voice.data + len, fr);
		break;
	}
	return 0;
}

int check_encoded_audio_length(struct iax_event *e, int iEncodeType)
{
	switch (iEncodeType) {
		case AST_FORMAT_GSM:
			if(e->event.voice.datalen % 33) {
				fprintf(stderr, "Weird gsm frame, not a multiple of 33.\n");
				return -1;
			}
			break;
	}
	return 0;
}

void increment_encoded_data_count(int *i, int iEncodeType) {
	switch (iEncodeType) {
		case AST_FORMAT_GSM:
			*i += 33;
			break;
	}
}
		 
