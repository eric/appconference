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

    /* XXX if the input contains more than we can read, we lose here */
    speex_bits_read_from(&decstate->bits, in, *inlen);
    *inlen = 0; 

    while(speex_bits_remaining(&decstate->bits) && (*outlen >= 160)) 
    {
      if(speex_decode_int(decstate->state, &decstate->bits, out))
      {
	  /* maybe there's not a whole frame somehow? */
	  break;
      }
      /* one frame of output */
      *outlen -= 160;
      out += 160;
    }

    return 0;
}

static int encode ( struct iaxc_audio_codec *c, 
    int *inlen, short *in, int *outlen, char *out ) {

    int bytes;
    struct state * encstate = (struct state *) c->encstate;

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
    return 0;
}

struct iaxc_audio_codec *iaxc_audio_codec_speex_new() {
  
  struct state * encstate;
  struct state * decstate;
  struct iaxc_audio_codec *c = calloc(sizeof(struct iaxc_audio_codec),1);

  
  if(!c) return c;

  strcpy(c->name,"speex");
  c->format = IAXC_FORMAT_SPEEX;
  c->encode = encode;
  c->decode = decode;
  c->destroy = destroy;

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

  if(!(encstate->state && decstate->state)) 
      return NULL;

  return c;
}

