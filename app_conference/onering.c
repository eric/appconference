#include <asterisk/lock.h>
#include <asterisk/file.h>
#include <asterisk/logger.h>
#include <asterisk/channel.h>
#include <asterisk/pbx.h>
#include <asterisk/module.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "onering.h"


struct ast_onering *ast_onering_new(void)
{
	struct ast_onering *s;
	s = malloc(sizeof(struct ast_onering));
	if (s) {
		memset(s, 0, sizeof(struct ast_onering));
	}
	s->len = 0;
	memset(s->data,0x0,sizeof(s->data));
	return s; 
}

int ast_onering_write(struct ast_onering *s, struct ast_frame *f)
{
	int d = 0;
	if (f->frametype != AST_FRAME_VOICE) {
		return -1;
	}
	if (s->len + f->datalen > ONERING_SIZE) {
		ast_log(LOG_WARNING, "Out of ringbuffer space in s=%#x s->len = %d f->datalen = %d\n",(int)s,s->len,f->datalen);
	    d = (s->len + f->datalen) - ONERING_SIZE;
	    memmove(s->data , s->data + d, ONERING_SIZE - d);
	    s->len -= d;
	} else {
//		ast_log(LOG_WARNING, "written space s->len = %d in %#x s->size = %d f->datalen = %d\n",s->len,(int)s,s->size,f->datalen);
	}
	if (s->len > ONERING_SIZE/2) {
	     ast_log(LOG_NOTICE, "s->len = %d, f->datalen = %d f->offset = %d\n",s->len,f->datalen,f->offset);
	}
// ast_log(LOG_NOTICE, "s->len = %d, f->datalen = %d f->offset = %d\n",s->len,f->datalen,f->offset);
	memcpy(s->data + s->len, f->data, f->datalen);
	s->len += f->datalen;
	return 0;
}

struct ast_frame *ast_onering_read(struct ast_onering *s,int samples)
{
	struct ast_frame *f;
	char *databuf;
	/* Make sure we have enough data */
	if (s->len < (samples * 2)) {
	//	ast_log(LOG_NOTICE,"s->len = %d, samples = %d\n",s->len,samples);
		return NULL;
	}
	databuf = malloc((samples * 2) + AST_FRIENDLY_OFFSET);
	if (databuf == NULL) {
	    ast_log(LOG_NOTICE,"unable to malloc\n");
	    return NULL;
	}
	memset(databuf,0x0,(samples * 2) + AST_FRIENDLY_OFFSET);
	f = malloc(sizeof(struct ast_frame));
	if (f == NULL) {
	    free(databuf);
	    ast_log(LOG_NOTICE,"unable to malloc\n");
	    return NULL;
	}
	memset(f,0x0,sizeof(struct ast_frame));
	/* Make frame */
	f->frametype = AST_FRAME_VOICE;
	f->subclass = AST_FORMAT_SLINEAR;
	f->data = databuf + AST_FRIENDLY_OFFSET;
	f->offset = AST_FRIENDLY_OFFSET;
	f->mallocd =  AST_MALLOCD_HDR | AST_MALLOCD_DATA;
	f->datalen = samples * 2;
	f->samples = samples;
	f->src = NULL;
	/* Fill Data */
	memcpy(f->data, s->data, samples * 2);
	s->len -= samples * 2;
	/* Move remaining data to the front if applicable */
	if (s->len) 
		memmove(s->data, s->data + (samples * 2), s->len);
	/* Return frame */
	return f;
}


void ast_onering_free(struct ast_onering *s)
{
    if (s != NULL) {
	free(s);
    }
}
