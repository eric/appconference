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
#include <pthread.h>
#include "app_conference.h"
#include "onering.h"


static pthread_mutex_t conflock = AST_MUTEX_INITIALIZER;
static struct ast_conference *conflist = NULL;

static char *tdesc = "Channel independent Conference Application";
static char *app = "Conference";
static char *synopsis = "Channel independent Conference";
static char *descrip = 
"  Conference():  returns 0\n"
"if the user exits with the '#' key, or -1 if the user hangs up.\n";

STANDARD_LOCAL_USER;
LOCAL_USER_DECL;

void remove_conf(struct ast_conference *conf) {
	struct ast_conference *confl,*conftmp;
	ast_pthread_mutex_lock(&conflock);

	confl = conflist;
	conftmp = NULL;
	while (confl != NULL) {
	    if (confl == conf) {
		if (conftmp == NULL) {
		    conflist = conflist->next;
		} else {
		    conftmp->next = confl->next;
		}
		ast_log(LOG_NOTICE,"removed conference %s\n",conf->name);
		free(conf);
		ast_pthread_mutex_unlock(&conflock);
		return;
	    }
	    conftmp = confl;
	    confl = confl->next;
	}
	ast_pthread_mutex_unlock(&conflock);
}

struct ast_conference *find_conf(char *name) {
	struct ast_conference *conf;
	
	ast_pthread_mutex_lock(&conflock);
	if (conflist == NULL) {
	    ast_pthread_mutex_unlock(&conflock);
	    return NULL;
	}
	
	conf = conflist;
	while (conf != NULL) {
	    if (conf != NULL) 
	    if (!strncasecmp((char *)&(conf->name),name,80)) {
		ast_pthread_mutex_unlock(&conflock);
		return conf;
	    }
	    conf = conf->next;
	}
	ast_pthread_mutex_unlock(&conflock);
	return NULL;
}

struct ast_conference *create_conf(char *name) {
	struct ast_conference *conf;
		
	conf = malloc(sizeof(struct ast_conference));
	if (conf == NULL) {
	    ast_log(LOG_ERROR,"unable to malloc ast_conference!\n");
	    return NULL;
	}
	conf->next = NULL;
	conf->memberlist = NULL;
	conf->membercount = 0;	
        strncpy((char *)&(conf->name),name,sizeof(conf->name)-1);
	pthread_mutex_init(&conf->lock,NULL);
	pthread_mutex_init(&conf->memberlock,NULL);
	
	ast_pthread_mutex_lock(&conflock);
	// prepend to conflist
	conf->next = conflist;
	conflist = conf;
	ast_pthread_mutex_unlock(&conflock);
	ast_log(LOG_NOTICE,"created conference %s\n",name);
	return conf;
}


/* must be called with member's bufferlock */
int add_buffer(struct ast_conf_member *member, struct ast_channel *chan)
{	
	struct ast_conf_audiobuffer *newbuffer;

	newbuffer = malloc(sizeof(struct ast_conf_audiobuffer));
	if (newbuffer == NULL) {
	    ast_log(LOG_ERROR,"unable to malloc ast_conf_audiobuffer!\n");
	    return -1;
	}
	newbuffer->ring = ast_onering_new();
	newbuffer->chan = chan;
	newbuffer->next = member->bufferlist;
	pthread_mutex_init(&newbuffer->lock,NULL);
	
	member->bufferlist = newbuffer;
	return 0;
}

void add_member(struct ast_conf_member *member, struct ast_conference *conf) {
	struct ast_conf_member *memberlist;

	if (conf == NULL) {
	    ast_log(LOG_ERROR,"trying to add member to NULL conference!\n");
	    return;
	}

	ast_pthread_mutex_lock(&conf->lock);
	ast_pthread_mutex_lock(&conf->memberlock);


	memberlist = conf->memberlist;
	while (memberlist != NULL) {
	    ast_pthread_mutex_lock(&memberlist->lock);
//    ast_log(LOG_NOTICE,"memberlist = %#x\n",memberlist);
    	    // for each member, create a new audiobuffer and prepend it

	// XXX add us to all bufferlists!!! , if we are supposed to speak
	    if (member->type != 'L') {
		if(add_buffer(member,memberlist->chan) < 0) {
		    ast_pthread_mutex_unlock(&memberlist->lock);
		    ast_pthread_mutex_unlock(&conf->memberlock);
		    ast_pthread_mutex_unlock(&conf->lock);
		    return;
		}
		ast_log(LOG_NOTICE,"added audiobuffer for member (type=%c, priority=%d) to memberlist (type=%c, priority=%d)\n",member->type,member->priority,memberlist->type,memberlist->priority);		
	    }

	    if (memberlist->type != 'L') {
		// and add everybody else (expcept us) to our bufferlist, if he is supposed to speak
		if(add_buffer(memberlist,member->chan) < 0) {
	    	    ast_log(LOG_ERROR,"unable to malloc ast_conf_audiobuffer!\n");
		    ast_pthread_mutex_unlock(&memberlist->lock);
		    ast_pthread_mutex_unlock(&conf->memberlock);
		    ast_pthread_mutex_unlock(&conf->lock);
		    return;
		}
		ast_log(LOG_NOTICE,"added audiobuffer for member (type=%c, priority=%d) to member (type=%c, priority=%d)\n",memberlist->type,memberlist->priority,member->type,member->priority);		
	    }

	    ast_pthread_mutex_unlock(&memberlist->lock);
	    memberlist = memberlist->next;
	}


// XXX
	member->next = conf->memberlist;
	conf->memberlist = member;
	conf->membercount++;
// ---

	ast_pthread_mutex_unlock(&conf->memberlock);
	ast_pthread_mutex_unlock(&conf->lock);
	ast_log(LOG_NOTICE,"member added to conference %s\n",conf->name);
}

struct ast_conf_member *create_member(struct ast_channel *chan, char type, int prio) {
	struct ast_conf_member *member;

	member = malloc(sizeof(struct ast_conf_member));
	if (member == NULL) {
	    ast_log(LOG_ERROR,"unable to malloc ast_conf_member!\n");
	    return NULL;
	}
	member->priority = prio;
	member->type = type;
	member->chan = chan;
	member->samplesperframe = 160; /* assume 160 samples per frame == 20 ms */
	pthread_mutex_init(&member->bufferlock,NULL);
	member->bufferlist = NULL;
	member->next = NULL;
	// XXX add us to all bufferlists!!!
	ast_log(LOG_NOTICE,"created member (type=%c, priority=%d)\n",member->type,member->priority);
	return member;
}

void remove_bufferlist(struct ast_conf_audiobuffer *bufferlist) {
	struct ast_conf_audiobuffer *bufferl;
	struct ast_conf_audiobuffer *buffertmp;

	if (bufferlist == NULL) {
	    return;
	}
	buffertmp = NULL;
	bufferl = bufferlist;
	while (bufferl != NULL) {
	    buffertmp = bufferl;
	    bufferl = bufferl->next;
	    if (buffertmp != NULL) {
		ast_onering_free(buffertmp->ring);
	        free(buffertmp);
	    }
	}
}

int remove_member(struct ast_conf_member *member,struct ast_conference *conf) {
	struct ast_conf_member *memberl;
	struct ast_conf_member *membertmp;
	struct ast_conf_audiobuffer *bufferl;
	struct ast_conf_audiobuffer *buffertmp;
	
		ast_log(LOG_NOTICE,"removing member from conference %s\n",conf->name);
	ast_pthread_mutex_lock(&conf->memberlock);
	memberl = conf->memberlist;
	membertmp = NULL;
	buffertmp = NULL;

	// XXX fixme this is inefficient

	while (memberl != NULL) {
	    ast_pthread_mutex_lock(&memberl->lock);
	    bufferl = memberl->bufferlist;
	    buffertmp = NULL;
	    while (bufferl != NULL) {
		if (bufferl->chan == member->chan) {
		    if (buffertmp == NULL) {
			// head
			memberl->bufferlist = bufferl->next;
		    } else {
			buffertmp->next = bufferl->next;
		    }
		    ast_log(LOG_NOTICE,"cut us out of (type=%c,prio=%d)\n",memberl->type,memberl->priority);
		}
		buffertmp = bufferl;
		bufferl = bufferl->next;
	    }
	    ast_pthread_mutex_unlock(&memberl->lock);
	    memberl = memberl->next;
	}

	memberl = conf->memberlist;
	membertmp = NULL;
	while (memberl != NULL) {
	    ast_pthread_mutex_lock(&memberl->lock);
	    if (memberl == member) {
		if (memberl->bufferlist != NULL) {
		    ast_pthread_mutex_lock(&memberl->bufferlock);
		    remove_bufferlist(memberl->bufferlist);
		    ast_pthread_mutex_unlock(&memberl->bufferlock);
		}
		ast_pthread_mutex_unlock(&memberl->lock);
		if (membertmp == NULL) { 
		    // removing head
		    conf->memberlist = memberl->next;
		} else {
		    membertmp->next = memberl->next;
		} 
		free(memberl);
		ast_log(LOG_NOTICE,"removed member from conference %s\n",conf->name);
		conf->membercount--;
		ast_log(LOG_NOTICE,"%d member(s) remaining in conference %s.\n",conf->membercount,conf->name);
		if (conf->membercount == 0) {
		    remove_conf(conf);
		} else {
		    ast_pthread_mutex_unlock(&conf->memberlock);
		    ast_pthread_mutex_unlock(&conf->lock); 
		}
		return 0;
	    }
	    ast_pthread_mutex_unlock(&memberl->lock);
	    membertmp = memberl;
	    memberl = memberl->next;

	}
	
	ast_pthread_mutex_unlock(&conf->memberlock);
	return -1;
}

void kill_conf(struct ast_conference *conf) {
	struct ast_conf_member *memberl,*memberln;
	ast_pthread_mutex_lock(&conf->lock);

	memberl = conf->memberlist;
	while (memberl != NULL) {
	    memberln = memberl->next;
	    remove_member(memberl,conf);
	    memberl = memberln;
	}
	ast_pthread_mutex_unlock(&conf->lock);
}

void write_audio(struct ast_frame *f, struct ast_conference *conference, struct ast_conf_member *member) {
	struct ast_conf_member *memberl;
	struct ast_conf_audiobuffer *bufferl;
	//here comes the good stuff:
	// write the audio into "our" buffer of every confmember, except ours
	ast_pthread_mutex_lock(&(conference->lock));
	ast_pthread_mutex_lock(&(conference->memberlock));
	memberl = conference->memberlist;
	while (memberl != NULL) {
//	ast_log(LOG_NOTICE,"member (type=%c)\n",memberl->type);
	    if (memberl->chan != member->chan) {
	    ast_pthread_mutex_lock(&(member->lock));
	    ast_pthread_mutex_lock(&(member->bufferlock));
		bufferl = memberl->bufferlist;
		while (bufferl != NULL) {
		    if (bufferl->chan == member->chan) {
	    ast_pthread_mutex_lock(&(bufferl->lock));
			if (ast_onering_write(bufferl->ring,f) != 0) {
//			    ast_log(LOG_ERROR,"failed to write audio\n");
//			ast_log(LOG_NOTICE,"written %d bytes from chan %#x to buffer %#x\n",f->datalen,member->chan,bufferl->ring);
			} else {
//			    ast_log(LOG_NOTICE,"+\n");
			}
	    ast_pthread_mutex_unlock(&(bufferl->lock));
		    }
		    bufferl = bufferl->next;
		}
	    ast_pthread_mutex_unlock(&(member->bufferlock));
	    ast_pthread_mutex_unlock(&(member->lock));
	    }
	    memberl = memberl->next;
	}
	ast_pthread_mutex_unlock(&(conference->memberlock));
	ast_pthread_mutex_unlock(&(conference->lock));
}

void mix_slin(char *dst, char *src, int samples) {
    int i=0;
//    printf("mixing\n");
    for (i=0;i<samples;i++) {
	    ((short *)dst)[i] += ((short *)src)[i];
    }
//    printf("mixed\n");
}

struct ast_frame *read_audio(struct ast_conference *conference, struct ast_conf_member *member, int samples) {
	struct ast_conf_audiobuffer *bufferl;
	struct ast_frame *f,*fout;
	char *databuf;
	int res=-1;
	
	databuf = malloc(samples * 2);
	memset(databuf,0x0,samples * 2);
	// sum it up ..sum it up...
	ast_pthread_mutex_lock(&(conference->lock));
	ast_pthread_mutex_lock(&(conference->memberlock));
	ast_pthread_mutex_lock(&(member->lock));
	ast_pthread_mutex_lock(&(member->bufferlock));
	bufferl = member->bufferlist;
	while (bufferl != NULL) {
	    ast_pthread_mutex_lock(&(bufferl->lock));
		if (bufferl->ring != NULL) {
		    f = ast_onering_read(bufferl->ring,samples);
		    if (f != NULL) {
    // ast_log(LOG_NOTICE,"%d samples\n",f->samples);
			mix_slin(databuf,(char *)&(f->data), samples);
			res = 0;
			ast_frfree(f);
		    } else {
			// remember that this member didnt give us data
			// bufferl->fails++;
			//ast_log(LOG_NOTICE,"No audio available for conference member in conference %s\n", conference->name);
			// was BUG: break;
		    }
		} else {
		    ast_log(LOG_NOTICE,"bufferlist has no ring\n");
		}
	    ast_pthread_mutex_unlock(&(bufferl->lock));
	    bufferl = bufferl->next;
	}

	ast_pthread_mutex_unlock(&(member->bufferlock));
	ast_pthread_mutex_unlock(&(member->lock));
	ast_pthread_mutex_unlock(&(conference->memberlock));
	ast_pthread_mutex_unlock(&(conference->lock));

	if (res == 0) {
	    fout = malloc(sizeof(struct ast_frame));
	    fout->frametype = AST_FRAME_VOICE;
	    fout->subclass = AST_FORMAT_SLINEAR;
	    fout->samples = samples;
	    fout->datalen = samples * 2;
	    fout->offset = AST_FRIENDLY_OFFSET;
	    fout->mallocd = 0;
	    fout->data = databuf;
	    return fout;
	} else {
	    fout = malloc(sizeof(struct ast_frame));
	    fout->frametype = AST_FRAME_VOICE;
	    fout->subclass = AST_FORMAT_SLINEAR;
	    fout->samples = samples;
	    fout->datalen = samples * 2;
	    fout->offset = AST_FRIENDLY_OFFSET;
	    fout->mallocd = 0;
	    fout->data = databuf;
	    memset(databuf,0,samples * 2);
	    return fout;
	}
}

static int send_audio(struct ast_conference *conference, struct ast_conf_member *member, int ms) {
	struct ast_frame *cf;
	cf = read_audio(conference,member,ms*8);
	if (cf != NULL) {
	    ast_write(member->chan,cf);
	    free(cf->data);
	    free(cf);
	    return 0;
	} else {
	    /* kaboom */
//	    ast_log(LOG_NOTICE,"silence is golden...\n");
//	    return -1;
//	    nf.frametype == AST_FRAME_NULL;
//	    ast_write(member->chan,cf);
	    return 0;	    
	}
}

#ifdef CONF_HANDLE_DTMF
static int handleDTMF(struct ast_frame *f,struct ast_conf_member *member, struct ast_conference *conference) {
    if (f->subclass == '#') {
    	return 0;
    } else 
    if (f->subclass == '*') {
	if (member->type == 'M') {
	    // shutdown of conference requested! good time to play a message XXX
	    ast_log(LOG_NOTICE,"moderator requested shutdown of conference %s\n",conference->name);
	    kill_conf(conference);
	    member = NULL;
	    return 1;
	} else {
	    // only moderators can shut down a conference
	    return -1;
	}
    }
    return -1;
}
#endif

static int conference_exec(struct ast_channel *chan, void *data)
{
	int res=-1;
	struct localuser *u;
	struct ast_frame *f;
	struct ast_conf_member *member;
	struct ast_conference *conference;
	char argstr[80];
	char *stringp,*confID,*type,*prio;
	int ms=20,dms;
	int rest=0;
	struct timeval time1, time2; /* <--- got lots of time ;-) */

	LOCAL_USER_ADD(u);
	
	/* parse the params */
	strncpy(argstr,data,sizeof(argstr)-1);
	stringp=argstr;
	confID = strsep(&stringp,"/");
	type = strsep(&stringp,"/");
	prio = strsep(&stringp,"/");
	/* XXX could need some checking ... */

	/* find the conference, or create it */
	conference = find_conf(confID);
	if (conference == NULL) {
	    conference = create_conf(confID);
	}
	if (conference == NULL) {
	    return -1;
	}
	
	member = create_member(chan,(char)type[0],atoi(prio));
	if (member == NULL) {
	    ast_pthread_mutex_lock(&conference->lock);
	    remove_conf(conference);
	    return -1;
	} else {
	    add_member(member,conference);
	}
	
	if (ast_set_write_format(chan, AST_FORMAT_SLINEAR) < 0) {
	    ast_log(LOG_ERROR,"Unable to set write format to signed linear!\n");
	    ast_pthread_mutex_lock(&conference->lock);
	    remove_member(member,conference);
	    return -1;
	}
	if (ast_set_read_format(chan, AST_FORMAT_SLINEAR) < 0) {
	    ast_log(LOG_ERROR,"Unable to set read format to signed linear!\n");
	    ast_pthread_mutex_lock(&conference->lock);
	    remove_member(member,conference);
	    return -1;
	}
	
	for (;;) {
		if (ast_check_hangup(chan) == 1) {
		    break;
		}
		ms = AST_CONF_LATENCY;
		gettimeofday(&time1,NULL);
		rest = ast_waitfor(chan, ms);
		gettimeofday(&time2,NULL);
		dms = (time2.tv_sec - time1.tv_sec) * 1000;
		dms += (time2.tv_usec - time1.tv_usec) / 1000;

		if (rest > 0) {
		    // ok, we got a frame from the channel
		    // ast_log(LOG_NOTICE,"dms1 = %d rest = %d\n",dms,rest);
		    f = ast_read(chan);
		    if (!f)
			break;
		    if (f->frametype == AST_FRAME_NULL) {
			// chan_modem_i4l sometimes spits out NULL frames
			continue;
		    }
		    if (f->frametype == AST_FRAME_VOICE) { 
			if (member->type != 'L') {
			    write_audio(f,conference,member);
			}
		    }
#ifdef CONF_HANDLE_DTMF
		     else if (f->frametype == AST_FRAME_DTMF) {
			res = handleDMTF(f)
			if (res == 0) { break };
			if (res == 1) { member = NULL; break };
		    }
#endif
		    if (f) {
			ast_frfree(f);
		    }
		}
		send_audio(conference,member,dms);
	} // for
	
	if (member != NULL) {
	    ast_pthread_mutex_lock(&conference->lock);
	    remove_member(member,conference);
	    // after this the conf could be gone
	    if (conference != NULL) {
		ast_pthread_mutex_unlock(&conference->lock);
	    }
	}
	LOCAL_USER_REMOVE(u);
	return res;
}

int unload_module(void)
{
	STANDARD_HANGUP_LOCALUSERS;
	return ast_unregister_application(app);
}

int load_module(void)
{
        pthread_mutex_init(&conflock,NULL);
	return ast_register_application(app, conference_exec, synopsis, descrip);
}

char *description(void)
{
	return tdesc;
}

int usecount(void)
{
	int res;
	STANDARD_USECOUNT(res);
	return res;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}
