
#include "iaxclient_lib.h"
#include "codec_gsm.h"
#include "codec_ulaw.h"

double iaxc_silence_threshold = -9e99;

static double input_level = 0, output_level = 0;

static SpeexPreprocessState *st = NULL;
int    iaxc_filters = IAXC_FILTER_AGC|IAXC_FILTER_DENOISE;

/* use to measure time since last audio was processed */
static struct timeval timeLastInput ;
static struct timeval timeLastOutput ;

static double vol_to_db(double vol)
{
    /* avoid calling log10 on zero */
    return log10(vol + 1.0e-99) * 20;
}

static int do_level_callback()
{
    static struct timeval last = {0,0};
    struct timeval now;
	double input_db, output_db ;

    gettimeofday(&now,NULL); 
    if(last.tv_sec != 0 && iaxc_usecdiff(&now,&last) < 100000) return 0;

    last = now;

	/* if input has not been processed in the last second, set to silent */
	input_db = ( iaxc_usecdiff( &now, &timeLastInput ) < 1000000 ) 
		? vol_to_db( input_level ) : -99.9 ;

	/* if output has not been processed in the last second, set to silent */
	output_db = ( iaxc_usecdiff( &now, &timeLastOutput ) < 1000000 ) 
		? vol_to_db( output_level ) : -99.9 ;

    iaxc_do_levels_callback( input_db, output_db ) ;

    return 0;
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
    double volume;
    static double lowest_volume = 1;
    int silent;

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

int send_encoded_audio(struct iaxc_call *call, void *data, int iEncodeType)
{
	char outbuf[1024];
	int outsize = 1024;
	int insize = 160; /* currently always 20ms */
	int silent;

	/* update last input timestamp */
	gettimeofday( &timeLastInput, NULL ) ;

	silent = input_postprocess(data,insize);	

	if(silent) return 0;  /* poof! no encoding! */

	/* destroy encoder if it is incorrect type */
	if(call->encoder && call->encoder->format != iEncodeType)
	{
	    call->encoder->destroy(call->encoder);
	    call->encoder = NULL;
	}

	/* create encoder if necessary */
	if(!call->encoder) {
	    switch (iEncodeType) {
		case AST_FORMAT_GSM:
		  call->encoder = iaxc_audio_codec_gsm_new();
		break;
		case AST_FORMAT_ULAW:
		  call->encoder = iaxc_audio_codec_ulaw_new();
		break;
		default:
		  /* ERROR: codec not supported */
		  fprintf(stderr, "ERROR: Codec not supported: %d\n", iEncodeType);
		  return 0;
	    }
	}

	if(!call->encoder) {
		  /* ERROR: no codec */
		  fprintf(stderr, "ERROR: Codec could not be created: %d\n", iEncodeType);
		  return 0;
	}

	if(call->encoder->encode(call->encoder, &insize, (short *)data, &outsize, outbuf)) {
		  /* ERROR: codec error */
		  fprintf(stderr, "ERROR: encode error: %d\n", iEncodeType);
		  return 0;
	}
	    
	if(iax_send_voice(call->session,iEncodeType, outbuf, 1024-outsize, 160-insize) == -1) 
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
	int insize = len;
	int outsize = 160;
      
	/* update last output timestamp */
	gettimeofday( &timeLastOutput, NULL ) ;

	if(len == 0) {
		fprintf(stderr, "Empty voice frame\n");
		return -1;
	}

	/* destroy decoder if it is incorrect type */
	if(call->decoder && call->decoder->format != iEncodeType)
	{
	    call->decoder->destroy(call->decoder);
	    call->decoder = NULL;
	}

	/* create encoder if necessary */
	if(!call->decoder) {
	    switch (iEncodeType) {
		case AST_FORMAT_GSM:
		  call->decoder = iaxc_audio_codec_gsm_new();
		break;
		case AST_FORMAT_ULAW:
		  call->decoder = iaxc_audio_codec_ulaw_new();
		break;
		default:
		  /* ERROR: codec not supported */
		  fprintf(stderr, "ERROR: Codec not supported: %d\n", iEncodeType);
		  return -1;
	    }
	}

	if(!call->decoder) {
		  /* ERROR: no codec */
		  fprintf(stderr, "ERROR: Codec could not be created: %d\n", iEncodeType);
		  return -1;
	}

	if(call->decoder->decode(call->decoder, &insize, data, &outsize, out)) {
		  /* ERROR: codec error */
		  fprintf(stderr, "ERROR: decode error: %d\n", iEncodeType);
		  return -1;
	}

	output_postprocess(out, 160-outsize);	

	return len-insize;
}


		 
