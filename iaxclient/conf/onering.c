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

struct ast_onering *ast_onering_new()
{
	struct ast_onering *s;
	s = malloc(sizeof(struct ast_onering));
	if (s) {
	    memset(s, 0, sizeof(struct ast_onering));
	    s->z1 = 0;
	    s->z2 = 0;
	}
	return s;
}

int ast_onering_write(struct ast_onering *s, struct ast_frame *f)
{
    int d = 0, maxlen = 0, count = 0;
    int space = abs(s->z1 - s->z2);
    if (s->z1 >= s->z2) {
	// n.w.
	space = (s->z2 - 1)+ (ONERING_SAMPLES - s->z1);  
    } else {
	// w.
	space = (s->z2 - s->z1) - 1;
    }
    count = f->samples;
    if (space < count) {
//	printf("only %d samples left (%d samples to be written)\n",space,f->samples);
	// move z2
	d = count - space;
//	printf("moving z2=%d by %d samples\n",s->z2,d);
	s->z2 = (s->z2 + d) & ONERING_MASK; 
//	printf("z2=%d\n",s->z2); 
    }
    if (s->z1 + count <= ONERING_SAMPLES) {
	maxlen = count;
    } else {
	maxlen = ONERING_SAMPLES - s->z1;
    }
    memcpy((char *)&(s->data[s->z1]),f->data,maxlen*2);
    s->z1 += maxlen;
    count -= maxlen;
    if (count > 0) {
	memcpy((char *)&(s->data[0]),f->data,count*2);
	s->z1 = count;
    }
//	printf("written=%d\n",count + maxlen); 
}

struct ast_frame *ast_onering_read(struct ast_onering *s, int max)
{
    int d = 0, maxlen = 0, count = 0;
    int len = ONERING_SAMPLES - abs(s->z1 - s->z2);

    printf("s=%#x z1=%d z2=%d\n",s,s->z1,s->z2);
    if (s->z1 == s->z2) {
	// empty ring
	return NULL;
    }
    if (s->z1 > s->z2) {
	len = s->z1 - s->z2;
    } else {
	len = s->z1 + (ONERING_SAMPLES - s->z2) + 1;

    }

    if ((max > 0) && (len > max)){
	len = max;
    }
    
    count = len;
    if (s->z2 + count <= ONERING_SAMPLES) {
	maxlen = count;
    } else {
	maxlen = ONERING_SAMPLES - s->z2;
    }
    memcpy(s->framedata,(char *)&(s->data[s->z2]),maxlen*2);
    s->z2 += maxlen;
    count -= maxlen;
    if (count > 0) {
	memcpy((char *)&(s->framedata[maxlen*2]),s->data,count * 2);
	s->z2 = count;
    }
    s->f.frametype = AST_FRAME_VOICE;
    s->f.subclass = AST_FORMAT_SLINEAR;
    s->f.data = s->framedata + AST_FRIENDLY_OFFSET;
    s->f.offset = AST_FRIENDLY_OFFSET;
    s->f.datalen = len * 2;
    s->f.samples = len;
    return &s->f;
}

void ast_onering_free(struct ast_onering *s)
{
    if (s != NULL) {
	free(s);
    }
}


int main(int argc, char *argv[]) {
    struct ast_frame fr,*tf;
    struct ast_onering *r;

    fr.samples = 50;
    
    
    r = ast_onering_new();
    printf("z1=%d z2=%d\n",r->z1,r->z2);

    ast_onering_write(r,&fr);
    printf("\nz1=%d z2=%d\n",r->z1,r->z2);
    tf = ast_onering_read(r,10);
    printf("\nz1=%d z2=%d\n",r->z1,r->z2);
    printf("tf->samples=%d\n",tf->samples);
    tf = ast_onering_read(r,10);
    printf("\nz1=%d z2=%d\n",r->z1,r->z2);
    printf("tf->samples=%d\n",tf->samples);
    tf = ast_onering_read(r,10);
    printf("\nz1=%d z2=%d\n",r->z1,r->z2);
    printf("tf->samples=%d\n",tf->samples);
    tf = ast_onering_read(r,10);
    printf("\ntf->samples=%d\n",tf->samples);
    printf("z1=%d z2=%d\n",r->z1,r->z2);

    ast_onering_write(r,&fr);
    printf("\nz1=%d z2=%d\n",r->z1,r->z2);
    tf = ast_onering_read(r,0);
    printf("\nz1=%d z2=%d\n",r->z1,r->z2);
    printf("tf->samples=%d\n",tf->samples);

    fr.samples = 2048;
    ast_onering_write(r,&fr);
    printf("\nz1=%d z2=%d\n",r->z1,r->z2);
    ast_onering_write(r,&fr);
    printf("\nz1=%d z2=%d\n",r->z1,r->z2);

    tf = ast_onering_read(r,0);
    printf("\nz1=%d z2=%d\n",r->z1,r->z2);
    printf("tf->samples=%d\n",tf->samples);

    ast_onering_free(r);

    return 0;
}