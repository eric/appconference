#include "audio_encode.h"

float input_level = 0.0;
float output_level = 0.0;
int level_calls = 0;


/* calculate the level of audio in the sample.
 * put the result in the cumulative "level" value,
 * return whether we consider this sample to be silent */
int calculate_level(void *audio, int len, float *level)
{
    /* Bogus calculation, just to see some changes */
    *level = (*level * .8) +  *((short *)audio);
    return 0;
}

static int input_postprocess(void *audio, int len)
{
    int silent;
    silent = calculate_level(audio, len, &input_level);

    if(iaxc_levels_callback)
    {
	level_calls++;
	if(level_calls % 100 == 0) 
	    iaxc_levels_callback(input_level,output_level);
    }
    return silent;
}

static int output_postprocess(void *audio, int len)
{

    if(!iaxc_levels_callback) return 0;

    calculate_level(audio, len, &output_level);
    level_calls++;

    if(level_calls % 100 == 0) iaxc_levels_callback(input_level,output_level);
    return 0;
}

int send_encoded_audio(struct peer *most_recent_answer, void *data, int iEncodeType)
{
	gsm_frame fo;
	int silent;

	/* currently always 20ms */
	silent = input_postprocess(data,160);	

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

/* decode encoded audio; return the number of bytes decoded 
 * negative indicates error 
 * XXX out MUST be 160 bytes */
int decode_audio(struct peer *p, void *out, void *data, int len, int iEncodeType)
{
	int ret;
	int datalen;

	if(len == 0) {
		fprintf(stderr, "Empty voice frame\n");
		return -1;
	}

	switch (iEncodeType) {
	case AST_FORMAT_GSM:
		if(len % 33) {
			fprintf(stderr, "Weird gsm frame, not a multiple of 33.\n");
			return -1;
		}
		if (!p->gsmin)
		    p->gsmin = gsm_create();

		if(gsm_decode(p->gsmin, data, out))
		    return -1;
		output_postprocess(out, 160);	
		return 33;
		break;
	}

	/* unknown type */
	return -1;
}


		 
