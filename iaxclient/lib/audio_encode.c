
#include "iaxclient_lib.h"

double iaxc_silence_threshold = -9e99;

static double input_level = 0, output_level = 0;

static SpeexPreprocessState *st = NULL;
int    iaxc_filters = IAXC_FILTER_AGC|IAXC_FILTER_DENOISE;

static int level_calls = 0;

static double vol_to_db(double vol)
{
    return log10(vol) * 20;
}

static int do_level_callback()
{
    static struct timeval last = {0,0};
    struct timeval now;


    gettimeofday(&now,NULL); 
    if(last.tv_sec != 0 && iaxc_usecdiff(&now,&last) < 100000) return;

    last = now;

    iaxc_do_levels_callback(vol_to_db(input_level), vol_to_db(output_level)); 
}


void iaxc_set_speex_filters() 
{
    int i;

    if(!st) st = speex_preprocess_state_init(160,8000); 

    i = (iaxc_silence_threshold > 0) ? 1 : 0;
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_VAD, &i);
    i = (iaxc_filters & IAXC_FILTER_AGC) ? 1 : 0;
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_AGC, &i);
    i = (iaxc_filters & IAXC_FILTER_DENOISE) ? 1 : 0;
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &i);
}

static void calculate_level(short *audio, int len, double *level) {
    short now = 0;
    double nowd;
    int i;

    for(i=0;i<len;i++)
      if(abs(audio[i]) > now) now = abs(audio[i]); 

    nowd = now/32767; 

    *level += (((double)now/32767) - *level) / 5;
}

static int input_postprocess(void *audio, int len)
{
    unsigned long ilen,olen;
    double volume;
    static double lowest_volume = 1;
    int silent;
    int i;

    if(!st) {
	st = speex_preprocess_state_init(160,8000);
	iaxc_set_speex_filters();
    }

    calculate_level(audio, len, &input_level);

    /* only preprocess if we're interested in VAD, AGC, or DENOISE */
    if((iaxc_filters & (IAXC_FILTER_DENOISE | IAXC_FILTER_AGC)) || iaxc_silence_threshold > 0)
	silent = !speex_preprocess(st, audio, NULL);


    /* this is ugly.  Basically just don't get volume level if speex thought
     * we were silent.  just set it to 0 in that case */
    if(iaxc_silence_threshold > 0 && silent)
	input_level = 0;

    do_level_callback();

    volume = vol_to_db(input_level);

    if(volume < lowest_volume) lowest_volume = volume;

    if(iaxc_silence_threshold > 0)
	return silent;
    else
	return volume < iaxc_silence_threshold;
}

static int output_postprocess(void *audio, int len)
{

    calculate_level(audio, len, &output_level);

    do_level_callback();

    return 0;
}

int send_encoded_audio(struct iaxc_call *most_recent_answer, void *data, int iEncodeType)
{
	gsm_frame fo;
	int silent;

	/* currently always 20ms */
	silent = input_postprocess(data,160);	

	if(silent) return;  /* poof! no encoding! */

	switch (iEncodeType) {
		case AST_FORMAT_GSM:
			if(!most_recent_answer->gsmout)
				most_recent_answer->gsmout = gsm_create();

			// encode the audio from the buffer into GSM format and send
			gsm_encode(most_recent_answer->gsmout, (short *) data, (void *)&fo);
			break;
	}
	if(iax_send_voice(most_recent_answer->session,AST_FORMAT_GSM, 
				(char *)&fo, sizeof(gsm_frame)) == -1) 
	{
	      puts("Failed to send voice!");
	      return -1;
	}
	return 0;
}

/* decode encoded audio; return the number of bytes decoded 
 * negative indicates error 
 * XXX out MUST be 160 bytes */
int decode_audio(struct iaxc_call *call, void *out, void *data, int len, int iEncodeType)
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
		if (!call->gsmin)
		    call->gsmin = gsm_create();

		if(gsm_decode(call->gsmin, data, out))
		    return -1;
		output_postprocess(out, 160);	
		return 33;
		break;
	}

	/* unknown type */
	return -1;
}


		 
