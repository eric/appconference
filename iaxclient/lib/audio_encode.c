
#include "iaxclient_lib.h"
#include "sox/sox.h"

double iaxc_silence_threshold = -9e99;
static compand_t input_compand = NULL;
static compand_t output_compand = NULL;

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

    if(!iaxc_levels_callback) return;

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

    iaxc_levels_callback(vol_to_db(ilevel), vol_to_db(olevel)); 
}

static int input_postprocess(void *audio, int len, void *out)
{
    unsigned long ilen,olen;
    double volume;
    static double lowest_volume = 1;

    if(!input_compand) {
	char *argv[5];
	argv[0] = strdup("0.04,0.3"); /* attack, decay */
	argv[1] = strdup("-90,-90,-50,-60,-30,-10,0,-5"); /* transfer function */
	argv[2] = strdup("0"); /* gain */
	argv[3] = strdup("0"); /* init volume */
	argv[4] = strdup("0.04"); /* delay */

	st_compand_start(&input_compand, argv, 5);
    }


    ilen=olen=len;
    st_compand_flow(input_compand, audio, out, &ilen, &olen);

    /* until the compander fills it's buffer, it might not put out full
     * buffers worth of data.  So, clear it unless it's all valid 
     * (also, this helps shut up valgrind :) */
    if(olen != len)
	memset(out,0,len*2);

    do_level_callback();

    volume = vol_to_db(*input_compand->volume);

    if(volume < lowest_volume) lowest_volume = volume;

    if(iaxc_silence_threshold > 0)
	return volume < lowest_volume + 5;
    else
	return volume < iaxc_silence_threshold;
}

static int output_postprocess(void *audio, int len)
{
    int l = len;

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

int send_encoded_audio(struct peer *most_recent_answer, void *data, int iEncodeType)
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


		 
