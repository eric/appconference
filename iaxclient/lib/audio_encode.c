
#include "iaxclient_lib.h"
#include "sox/sox.h"

double iaxc_silence_threshold = -9e99;
static compand_t input_compand = NULL;
static compand_t output_compand = NULL;

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

    double ilevel, olevel;

    gettimeofday(&now,NULL); 
    if(last.tv_sec != 0 && iaxc_usecdiff(&now,&last) < 100000) return;

    last = now;

    if(input_compand)
	ilevel = *input_compand->volume;
    else
	ilevel = 0.0;

    if(output_compand)
	olevel = *output_compand->volume;
    else
	olevel = 0.0;

    iaxc_do_levels_callback(vol_to_db(ilevel), vol_to_db(olevel)); 
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


static int input_postprocess(void *audio, int len, void *out)
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

    /* only preprocess if we're interested in VAD, AGC, or DENOISE */
    if((iaxc_filters & (IAXC_FILTER_DENOISE | IAXC_FILTER_AGC)) || iaxc_silence_threshold > 0)
	silent = !speex_preprocess(st, audio, NULL);

    if(!input_compand) {
	char *argv[5];
	/* just use the compander for the volume meters AGC is handled by speex now * */
	argv[0] = strdup("0.05,0.1"); /* attack, decay */
	argv[1] = strdup("-90,-90,0,0"); /* transfer function */
	st_compand_start(&input_compand, argv, 2);
    }


    ilen=olen=len;

    /* this is ugly.  Basically just don't get volume level if speex thought
     * we were silent.  just set it to 0 in that case */
    if(iaxc_silence_threshold <= 0 || !silent)
	st_compand_flow(input_compand, audio, out, &ilen, &olen);
    else 
	*input_compand->volume = 0;

    /* until the compander fills it's buffer, it might not put out full
     * buffers worth of data.  So, clear it unless it's all valid 
     * (also, this helps shut up valgrind :) */
    if(olen != len)
	memset(out,0,len*2);

    do_level_callback();

    volume = vol_to_db(*input_compand->volume);

    if(volume < lowest_volume) lowest_volume = volume;

    if(iaxc_silence_threshold > 0)
	return silent;
    else
	return volume < iaxc_silence_threshold;
}

static int output_postprocess(void *audio, int len)
{
    unsigned long l = len;

    if(!output_compand) {
	char *argv[2];
	argv[0] = strdup("0.05,0.1"); /* attack, decay */
	argv[1] = strdup("-90,-90,0,0"); /* transfer function */
	st_compand_start(&output_compand, argv, 2);
    }

    st_compand_flow(output_compand, audio, audio, &l, &l);

    do_level_callback();

    return 0;
}

int send_encoded_audio(struct iaxc_call *most_recent_answer, void *data, int iEncodeType)
{
	gsm_frame fo;
	int silent;
	short processed[160];

	/* currently always 20ms */
	silent = input_postprocess(data,160, processed);	

	if(silent) return;  /* poof! no encoding! */

	switch (iEncodeType) {
		case AST_FORMAT_GSM:
			if(!most_recent_answer->gsmout)
				most_recent_answer->gsmout = gsm_create();

			// encode the audio from the buffer into GSM format and send
			gsm_encode(most_recent_answer->gsmout, (short *) ((char *) processed), (void *)&fo);
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


		 
