/*
 * iaxclient: a portable telephony toolkit
 *
 * Copyright (C) 2003-2004, Horizon Wimba, Inc.
 *
 * Steve Kann <stevek@stevek.com>
 *
 * This program is free software, distributed under the terms of
 * the GNU Lesser (Library) General Public License
 */

#include "codec_speex.h"
#include "iaxclient_lib.h"
#include "speex/speex.h"

/* defining SPEEX_TERMINATOR, and using frame sizes > 160 will result in encoded output with a
 * terminator after each 20ms sub-frame, in a format which is generally incompatible with asterisk. 
 * so, unless you're doing something special, DON'T define this! */

struct state {
      void *state;
      SpeexBits bits;
};


static void destroy ( struct iaxc_audio_codec *c) {
    struct state * encstate = (struct state *) c->encstate;
    struct state * decstate = (struct state *) c->decstate;

    speex_bits_destroy(&encstate->bits);
    speex_bits_destroy(&decstate->bits);
    speex_encoder_destroy(encstate->state);
    speex_decoder_destroy(decstate->state);

    free(c->encstate);
    free(c->decstate);

    free(c);
}


static int decode ( struct iaxc_audio_codec *c, 
    int *inlen, char *in, int *outlen, short *out ) {

    struct state * decstate = (struct state *) c->decstate;
    int ret =0;
    int bits_left = 0;
    int bits_read = 0;
    int start_bits = 0;

    if(*inlen == 0) {
	//return 0;
	//fprintf(stderr, "Speex Interpolate\n");
	speex_decode_int(decstate->state, NULL, out);
	*outlen -= 160;
	return 0;
    }

    /* XXX if the input contains more than we can read, we lose here */
    speex_bits_read_from(&decstate->bits, in, *inlen);
    *inlen = 0; 

    start_bits = speex_bits_remaining(&decstate->bits);

    while(speex_bits_remaining(&decstate->bits) && (*outlen >= 160)) 
    {
        ret = speex_decode_int(decstate->state, &decstate->bits, out);
// * from speex/speex.h, speex_decode returns:
// * @return return status (0 for no error, -1 for end of stream, -2 other)
        if (ret == 0) {
        /* one frame of output */
            *outlen -= 160;
            out += 160;
        } else if (ret == -1) {
	/* at end of stream, or just a terminator */
            bits_left = speex_bits_remaining(&decstate->bits);
            if (bits_left < 5 )
            {
		/* end of stream */
                break;
            } 
            else {
		/* read past terminator to end of byte boundary */
                int bye_bits = 0;
                bits_read = start_bits - bits_left ;
                bye_bits = 8 - bits_read % 8;
                if (bye_bits < 8) 
      {
            //fprintf(stderr, "advancing to end of byte.  extra bits=>%d, total read => %d, bits remaining after advance => %d\n",bye_bits, bits_read + bye_bits, bits_left - bye_bits );
                    speex_bits_advance(&decstate->bits, bye_bits);
                }
            }
        } else {
	  /* maybe there's not a whole frame somehow? */
            fprintf(stderr, "decode_int returned non-zero => %d\n",ret);
	  break;
      }
    }
    return 0;
}

static int encode ( struct iaxc_audio_codec *c, 
    int *inlen, short *in, int *outlen, char *out ) {

    int bytes;
  //int bitrate;
    struct state * encstate = (struct state *) c->encstate;

    /* need to encode minimum of 160 samples */

#ifdef SPEEX_TERMINATOR
//fprintf(stderr, "SPEEX_TERMINATOR = 1\n");
    /* if mininum_outgoing_framesize (*inlen) is set at >160, 
       then pack multiple sets together in one packet on the wire, 
       separate with terminator */
    while(*inlen >= 160) 
    {
    /* reset and encode*/
        speex_bits_reset(&encstate->bits);
        speex_encode_int(encstate->state, in, &encstate->bits);

    /* add terminator */
        speex_bits_pack(&encstate->bits, 15, 5);

    /* write to output */
        /* can an error happen here?  no bytes? */
        bytes = speex_bits_write(&encstate->bits, out, *outlen);
//fprintf(stderr, "encode wrote %d bytes, outlen was %d\n", bytes, *outlen);
    /* advance pointers to input and output */
        *inlen -= 160;
        in += 160;
        *outlen -= bytes;
        out += bytes;
     } 
#else
//fprintf(stderr, "SPEEX_TERMINATOR = 0\n");
/*  only add terminator at end of bits */
    speex_bits_reset(&encstate->bits);

    /* need to encode minimum of 160 samples */
    while(*inlen >= 160) {
      speex_encode_int(encstate->state, in, &encstate->bits);
      *inlen -= 160;
      in += 160;
    } 

    /* add terminator */
    speex_bits_pack(&encstate->bits, 15, 5);
   
    bytes = speex_bits_write(&encstate->bits, out, *outlen);

    /* can an error happen here?  no bytes? */
    *outlen -= bytes;
#endif
    return 0;
}

struct iaxc_audio_codec *iaxc_audio_codec_speex_new(struct iaxc_speex_settings *set) {
  
  struct state * encstate;
  struct state * decstate;
  struct iaxc_audio_codec *c = calloc(sizeof(struct iaxc_audio_codec),1);

  if(!c) return c;

  strcpy(c->name,"speex");
  c->format = IAXC_FORMAT_SPEEX;
  c->encode = encode;
  c->decode = decode;
  c->destroy = destroy;

  c->minimum_frame_size = 160;

  c->encstate = calloc(sizeof(struct state),1);
  c->decstate = calloc(sizeof(struct state),1);

  /* leaks a bit on no-memory */
  if(!(c->encstate && c->decstate)) 
      return NULL;

  encstate = (struct state *) c->encstate;
  decstate = (struct state *) c->decstate;

  encstate->state = speex_encoder_init(&speex_nb_mode);
  decstate->state = speex_decoder_init(&speex_nb_mode);
  speex_bits_init(&encstate->bits);
  speex_bits_init(&decstate->bits);
  speex_bits_reset(&encstate->bits);
  speex_bits_reset(&decstate->bits);

  speex_decoder_ctl(decstate->state, SPEEX_SET_ENH, &set->decode_enhance);

  speex_encoder_ctl(encstate->state, SPEEX_SET_COMPLEXITY, &set->complexity);

  if(set->quality >= 0) {
    if(set->vbr) {
      speex_encoder_ctl(encstate->state, SPEEX_SET_VBR_QUALITY, &set->quality);
    } else {
      int quality = set->quality;
      speex_encoder_ctl(encstate->state, SPEEX_SET_QUALITY, &quality);
    }
  }
  if(set->bitrate >= 0) 
    speex_encoder_ctl(encstate->state, SPEEX_SET_BITRATE, &set->bitrate);
  if(set->vbr) 
    speex_encoder_ctl(encstate->state, SPEEX_SET_VBR, &set->vbr);
  if(set->abr)
    speex_encoder_ctl(encstate->state, SPEEX_SET_ABR, &set->abr);


  if(!(encstate->state && decstate->state)) 
      return NULL;

  return c;
}

