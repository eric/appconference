/*
 * iaxclient: a portable telephony toolkit
 *
 * Written by Cyril VELTER <cyril.velter@metadys.com>
 *
 * This program is free software, distributed under the terms of
 * the GNU Lesser (Library) General Public License
 */

#include "codec_alaw.h"
#include "iaxclient_lib.h"

struct state {
    INTERPOLATE_DECLS;
};

static inline short int alawdecode (unsigned char alaw)
{
	int value;
	int segment;
	
	/* Mask value */
	alaw ^= 0x55;

	/* Extract and scale value */
	value = (alaw & 0x0f) << 4;
	
	/* Extract segment number */
	segment = (alaw & 0x70) >> 4;

	/* Compute value */
	switch (segment) {
		case 0:
			break;
		case 1:
			value += 0x100;
			break;
		default:
			value += 0x100;
			value <<= segment - 1;
	}

	/* Extract sign */
	return (alaw & 0x80) ? value : -value;
}

static inline unsigned char alawencode (short int linear)
{
	int mask = 0x55;
	int segment;
	unsigned char alaw;
	
	static int segments[8] = {0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF};
	
	if (linear >= 0)
	{
	  /* Sign (7th) bit = 1 */
	  mask |= 0x80;
	}
	else
	{
	  /* Sign (7th) bit = 0 */
	  linear = -linear;
	}
	
	/* Find the segment */
	for (segment = 0;segment < 8;segment++)
	 	if (linear <= segments[segment])
			break;

	/* Combine the sign, segment, and quantization bits. */

	if (segment < 8)		
	{
		if (segment < 2)
			alaw = (linear >> 4) & 0x0F;
		else
			alaw = (linear >> (segment + 3)) & 0x0F;
			
		return ((alaw | (segment << 4)) ^ mask);
	}
	else
		/* out of range, return maximum value. */
		return (0x7F ^ mask);
}

static int decode ( struct iaxc_audio_codec *c, 
    int *inlen, char *in, int *outlen, short *out ) {
    struct state *state = c->decstate;
    int i;
    short sample;

    if(*inlen == 0) {
        for(i=0;i<INTERPOLATE_BUFSIZ;i++) {

            INTERPOLATE_GET(state, sample);

            *(out++) = sample;
            (*outlen)--;

            if((*outlen < 0)) break;
        }
        return 0;
    }


    while ((*inlen > 0) && (*outlen > 0)) {
	sample = alawdecode((unsigned char)*(in++));
	*(out++) = sample;
	(*inlen)--; (*outlen)--;
	INTERPOLATE_PUT(state,sample);
    }

    return 0;
}

static int encode ( struct iaxc_audio_codec *c, 
    int *inlen, short *in, int *outlen, char *out ) {

    while ((*inlen > 0) && (*outlen > 0)) {
	*(out++) = alawencode(*(in++));
	(*inlen)--; (*outlen)--;
    }

    return 0;
}

static void destroy ( struct iaxc_audio_codec *c) {
    free(c);
}

struct iaxc_audio_codec *iaxc_audio_codec_alaw_new() {

  struct iaxc_audio_codec *c = calloc(sizeof(struct iaxc_audio_codec),1);
  
  if(!c) return c;

  strcpy(c->name,"alaw");
  c->format = IAXC_FORMAT_ALAW;
  c->encode = encode;
  c->decode = decode;
  c->destroy = destroy;

  /* really, we can use less, but don't want to */
  c->minimum_frame_size = 160;

  /* decoder state, used for interpolation */
  c->decstate = calloc(sizeof(struct state),1);

  return c;
}

