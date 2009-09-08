
/*
 * app_conference
 *
 * A channel independent conference application for Asterisk
 *
 * Copyright (C) 2002, 2003 Junghanns.NET GmbH
 * Copyright (C) 2003, 2004 HorizonLive.com, Inc.
 * Copyright (C) 2005, 2006 HorizonWimba, Inc.
 * Copyright (C) 2007 Wimba, Inc.
 *
 * Klaus-Peter Junghanns <kapejod@ns1.jnetdns.de>
 *
 * Video Conferencing support added by
 * Neil Stratford <neils@vipadia.com>
 * Copyright (C) 2005, 2005 Vipadia Limited
 *
 * VAD driven video conferencing, text message support
 * and miscellaneous enhancements added by
 * Mihai Balea <mihai at hates dot ms>
 *
 * This program may be modified and distributed under the
 * terms of the GNU General Public License. You should have received
 * a copy of the GNU General Public License along with this
 * program; if not, write to the Free Software Foundation, Inc.
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "frame.h"
#include "packer.h"

//
// ast_packer, adapted from ast_smoother
// pack multiple frames together into one packet on the wire.
//

#define PACKER_SIZE  8000
#define PACKER_QUEUE 10 // store at most 10 complete packets in the queue

struct ast_packer {
	int framesize; // number of frames per packet on the wire.
	int size;
	int packet_index;
	int format;
	int readdata;
	int optimizablestream;
	int flags;
	float samplesperbyte;
	struct ast_frame f;
	struct timeval delivery;
	char data[PACKER_SIZE];
	char framedata[PACKER_SIZE + AST_FRIENDLY_OFFSET];
	int samples;
	int sample_queue[PACKER_QUEUE];
	int len_queue[PACKER_QUEUE];
	struct ast_frame *opt;
	int len;
};

void ast_packer_reset(struct ast_packer *s, int framesize)
{
	memset(s, 0, sizeof(struct ast_packer));
	s->framesize = framesize;
	s->packet_index=0;
	s->len=0;
}

struct ast_packer *ast_packer_new(int framesize)
{
	struct ast_packer *s;
	if (framesize < 1)
		return 0;
	s = ast_malloc(sizeof(struct ast_packer));
	if (s)
		ast_packer_reset(s, framesize);
	return s;
}

int ast_packer_get_flags(struct ast_packer *s)
{
	return s->flags;
}

void ast_packer_set_flags(struct ast_packer *s, int flags)
{
	s->flags = flags;
}

int ast_packer_feed(struct ast_packer *s, const struct ast_frame *f)
{
	if (f->frametype != AST_FRAME_VOICE) {
		ast_log(LOG_WARNING, "Huh?  Can't pack a non-voice frame!\n");
		return -1;
	}
	if (!s->format) {
		s->format = f->subclass;
		s->samples=0;
	} else if (s->format != f->subclass) {
		ast_log(LOG_WARNING, "Packer was working on %d format frames, now trying to feed %d?\n", s->format, f->subclass);
		return -1;
	}
	if (s->len + f->datalen > PACKER_SIZE) {
		ast_log(LOG_WARNING, "Out of packer space\n");
		return -1;
	}
	if (s->packet_index >= PACKER_QUEUE ){
		ast_log(LOG_WARNING, "Out of packer queue space\n");
		return -1;
	}

	memcpy(s->data + s->len, f->data, f->datalen);
	/* If either side is empty, reset the delivery time */
	if (!s->len || (!f->delivery.tv_sec && !f->delivery.tv_usec) ||
			(!s->delivery.tv_sec && !s->delivery.tv_usec))
		s->delivery = f->delivery;
	s->len += f->datalen;
//packer stuff
	s->len_queue[s->packet_index]    += f->datalen;
	s->sample_queue[s->packet_index] += f->samples;
	s->samples += f->samples;

	if (s->samples > s->framesize )
		++s->packet_index;

	return 0;
}

struct ast_frame *ast_packer_read(struct ast_packer *s)
{
	struct ast_frame *opt;
	int len;
	/* IF we have an optimization frame, send it */
	if (s->opt) {
		opt = s->opt;
		s->opt = 0;
		return opt;
	}

	/* Make sure we have enough data */
	if (s->samples < s->framesize ){
			return 0;
	}
	len = s->len_queue[0];
	if (len > s->len)
		len = s->len;
	/* Make frame */
	s->f.frametype = AST_FRAME_VOICE;
	s->f.subclass = s->format;
	s->f.data = s->framedata + AST_FRIENDLY_OFFSET;
	s->f.offset = AST_FRIENDLY_OFFSET;
	s->f.datalen = len;
	s->f.samples = s->sample_queue[0];
	s->f.delivery = s->delivery;
	/* Fill Data */
	memcpy(s->f.data, s->data, len);
	s->len -= len;
	/* Move remaining data to the front if applicable */
	if (s->len) {
		/* In principle this should all be fine because if we are sending
		   G.729 VAD, the next timestamp will take over anyawy */
		memmove(s->data, s->data + len, s->len);
		if (s->delivery.tv_sec || s->delivery.tv_usec) {
			/* If we have delivery time, increment it, otherwise, leave it at 0 */
			s->delivery.tv_sec +=  s->sample_queue[0] / 8000.0;
			s->delivery.tv_usec += (((int)(s->sample_queue[0])) % 8000) * 125;
			if (s->delivery.tv_usec > 1000000) {
				s->delivery.tv_usec -= 1000000;
				s->delivery.tv_sec += 1;
			}
		}
	}
	int j;
	s->samples -= s->sample_queue[0];
	if( s->packet_index > 0 ){
		for (j=0; j<s->packet_index -1 ; j++){
			s->len_queue[j]=s->len_queue[j+1];
			s->sample_queue[j]=s->sample_queue[j+1];
		}
		s->len_queue[s->packet_index]=0;
		s->sample_queue[s->packet_index]=0;
		s->packet_index--;
	} else {
		s->len_queue[0]=0;
		s->sample_queue[0]=0;
	}


	/* Return frame */
	return &s->f;
}

void ast_packer_free(struct ast_packer *s)
{
	ast_free(s);
}

