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

int decode_audio(struct iax_event *e, struct peer *p, void *fr, int *len, int iEncodeType)
{
	int ret;
	int datalen;

#ifdef IAXC_IAX2
#define EVENTLEN (e->datalen)
#define EVENTDATA (e->data)
#else
#define EVENTLEN (e->event.voice.datalen)
#define EVENTDATA (e->event.voice.data)
#endif

	if(EVENTLEN == 0) {
		fprintf(stderr, "Empty voice frame\n");
		return -1;
	}

	switch (iEncodeType) {
	case AST_FORMAT_GSM:
		if(EVENTLEN % 33) {
			fprintf(stderr, "Weird gsm frame, not a multiple of 33.\n");
			return -1;
		}
		if (!p->gsmin)
			p->gsmin = gsm_create();
		ret = gsm_decode(p->gsmin, (char *) EVENTDATA + *len, fr);
		if(!ret)
		      *len += 33;
		return ret;

		break;
	}
	return 0;
}

		 
