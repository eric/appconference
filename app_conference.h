/*
 * app_conference
 *
 * A channel independent conference application for Asterisk
 *
 * Copyright (C) 2002, 2003 Junghanns.NET GmbH
 *
 * Klaus-Peter Junghanns <kapejod@ns1.jnetdns.de>
 *
 * This program may be modified and distributed under the 
 * terms of the GNU Public License.
 */
 
#ifndef _ASTERISK_CONF_H
#define _ASTERISK_CONF_H

/* 160 samples 16bit signed linear */
#define AST_CONF_BLOCK_SAMPLES 160

/* wait max 100 ms for audio */
/* this value may not be longer than the time a ringbuffer can take (~1s) */
#define AST_CONF_LATENCY 100

/* output audio frames must be at least 20 ms long */
#define AST_CONF_MIN_MS 20

struct ast_conf_audiobuffer {
    /* lock */
    pthread_mutex_t lock;

    /* channel */
    struct ast_channel *chan;

    struct ast_onering *ring;

    int ringfails;
        
    /* next */
    struct ast_conf_audiobuffer *next;
};

struct ast_conf_member {
    /* lock */
    pthread_mutex_t lock;

    /* channel */
    struct ast_channel *chan;

    int samplesperframe;

    /* highest priority gets the channel */
    int priority;

    /*  Member Type
	L = ListenOnly 
	M = Moderator
	S = Standard (Listen/Talk)
    */
    char type;
    
    /* lock */
    pthread_mutex_t bufferlock;

    /* audiobuffers */
    struct ast_conf_audiobuffer *bufferlist;

//    struct ast_ringbuffer *echo;

    struct ast_smoother *smoother;
    
    /* next member */
    struct ast_conf_member *next;    
};

struct ast_conference {
    /* lock */
    pthread_mutex_t lock;

    /* lock */
    pthread_mutex_t memberlock;

    /* name (e.g. room1)  */
    char name[80];

    /* member in this conference */
    int membercount;

    /* members in this conference */
    struct ast_conf_member *memberlist;    

    /* next conference */
    struct ast_conference *next;
};
#endif
