/*
 * jitterbuf: an application-independent jitterbuffer
 *
 * Copyrights:
 * Copyright (C) 2004, Horizon Wimba, Inc.
 *
 * Contributors:
 * Steve Kann <stevek@stevek.com>
 *
 * This program is free software, distributed under the terms of
 * the GNU Lesser (Library) General Public License
 */

#include "jitterbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

//#define jb_warn(...) fprintf(stderr, __VA_ARGS__)
#define jb_warn(...) 
#define jb_err(...)  fprintf(stderr, __VA_ARGS__)
//#define jb_dbg(...)  fprintf(stderr, __VA_ARGS__)
#define jb_dbg(...)  

static void jb_warninfo(jitterbuf *jb);


int jb_reset(jitterbuf *jb) {
    memset(jb,0,sizeof(jitterbuf));

    /* initialize length */
    jb->info.current = jb->info.target = 100; 
    jb->info.silence = 1; 
    jb->info.last_voice_ms = 20;
}

jitterbuf * jb_new() {
    jitterbuf *jb;


    jb = malloc(sizeof(jitterbuf));
    if(!jb) return NULL;

    jb_reset(jb);

    jb_dbg("jb_new() = %x\n", jb);
    return jb;
}

void jb_destroy(jitterbuf *jb) {
    jb_frame *frame; 
    jb_dbg("jb_destroy(%x)\n", jb);

    /* free all the frames on the "free list" */
    frame = jb->free; frame;
    while(frame != NULL) {
      jb_frame *next = frame->next;
      free(frame);
      frame = next;
    }

    /* free ourselves! */ 
    free(jb);
}



/* simple history manipulation */
/* maybe later we can make the history buckets variable size, or something? */
/* drop parameter determines whether we will drop outliers to minimize
 * delay */
static inline long maxdiff(long *arr, int n, int drop) {
   long max=LONG_MIN, max2=LONG_MIN, min = LONG_MAX;

    if(n<2) return 0;

    while(n-- > 0) {
	if(arr[n] > max) {
	  max2 = max;
	  max = arr[n];
	} else if (arr[n] > max2) {
	  max2 = arr[n];
	}
	if(arr[n] < min) {
	  min = arr[n];
	}
    }

    if(drop)
      return max2-min;
    else
      return max-min;
}

static int longcmp(const void *a, const void *b) {
    return *(long *)a - *(long *)b;
}

static void history_put(jitterbuf *jb, long ts, long now) {

    /* don't add special/negative times to history */
    if(ts <= 0) return;

    /* rotate long-term history as needed */
    while( (now - jb->hist_ts) > JB_HISTORY_SHORTTM) 
    {
	jb->hist_ts += JB_HISTORY_SHORTTM;

	/* rotate long-term history */
	memmove(&(jb->hist_longmax[1]), &(jb->hist_longmax[0]), (JB_HISTORY_LONGSZ-1) * sizeof(jb->hist_longmax[0]));

	memmove(&(jb->hist_longmin[1]), &(jb->hist_longmin[0]), (JB_HISTORY_LONGSZ-1) * sizeof(jb->hist_longmin[0]));

	/* move current short-term history to long-term */
	if(jb->hist_shortcur > 2) {
	    qsort(jb->hist_short,jb->hist_shortcur,sizeof(long),longcmp);
	    /* intentionally drop PCT from max */
	    jb->hist_longmax[0] = jb->hist_short[jb->hist_shortcur-1-(JB_HISTORY_DROPPCT*jb->hist_shortcur/100)];
	    jb->hist_longmin[0] = jb->hist_short[0];
	    if(jb->hist_longmax[0] < jb->hist_longmin[0]) fprintf(stderr, "ERROR!!!!!!!!");
	    jb_warn("history: rotating, short-term was %ld %ld (%ld)\n", 
		jb->hist_longmax[0], jb->hist_longmin[0], 
		jb->hist_longmax[0]-jb->hist_longmin[0]);
	    jb_warninfo(jb);
	  //jb_warnqueue(jb);

	    /* clear short-term */
	    jb->hist_shortcur = 0;
	} else {
	    /* XXX what should we do here? */
	    /* just keep the old values for long-term? */
	}

    }

    /* add this entry, if it's in the window */
    if(jb->hist_shortcur < JB_HISTORY_SHORTSZ)
	jb->hist_short[jb->hist_shortcur++] = now - ts;

}

static long history_get(jitterbuf *jb, long *minptr) {
    int i,n;
    long max = LONG_MIN;
    long min = LONG_MAX;

    if(jb->hist_shortcur > 2) {
      qsort(jb->hist_short,jb->hist_shortcur,sizeof(long),longcmp);
      min = jb->hist_short[0];
      max = jb->hist_short[jb->hist_shortcur-1-(JB_HISTORY_DROPPCT*jb->hist_shortcur/100)];
    }

    n = jb->hist_ts/JB_HISTORY_SHORTTM;
    if(n > JB_HISTORY_LONGSZ) n = JB_HISTORY_LONGSZ;
 
    for(i=0;i<n;i++) {
      if(jb->hist_longmax[i] > max) max = jb->hist_longmax[i];
      if(jb->hist_longmin[i] < min) min = jb->hist_longmin[i];
    }

    if(minptr) *minptr = min;
    return max-min;
}

static void queue_put(jitterbuf *jb, void *data, int type, long ms, long ts) {
    jb_frame *frame;
    jb_frame *p;

    frame = jb->free;
    if(frame) {
	jb->free = frame->next;
    } else {
	frame = malloc(sizeof(jb_frame));
    }

    if(!frame) {
	jb_err("cannot allocate frame\n");
	return;
    }

    jb->info.frames_cur++;

    frame->data = data;
    frame->ts = ts;
    frame->ms = ms;
    frame->type = type;

    /* 
     * frames are a circular list, jb-frames points to to the lowest ts, 
     * jb->frames->prev points to the highest ts
     */

    if(!jb->frames) {  /* queue is empty */
	jb->frames = frame;
	frame->next = frame;
	frame->prev = frame;
    } else if(ts < jb->frames->ts) { 
	frame->next = jb->frames;
	frame->prev = jb->frames->prev;

	frame->next->prev = frame;
	frame->prev->next = frame;

	jb->frames = frame;
    } else { 
	p = jb->frames;

	/* frame is out of order */
	if(ts < p->prev->ts) jb->info.frames_ooo++;

	while(ts < p->prev->ts && p->prev != jb->frames) 
	    p = p->prev;

	frame->next = p;
	frame->prev = p->prev;

	frame->next->prev = frame;
	frame->prev->next = frame;
    }
}

static long queue_next(jitterbuf *jb) {
    if(jb->frames) return jb->frames->ts;
    else return -1;
}

static long queue_last(jitterbuf *jb) {
    if(jb->frames) return jb->frames->prev->ts;
    else return -1;
}

jb_frame *queue_get(jitterbuf *jb, long ts) {
    jb_frame *frame;
    frame = jb->frames;

    if(!frame)
	return NULL;

    //jb_warn("queue_get: ASK %ld FIRST %ld\n", ts, frame->ts);

    if(ts > frame->ts) {
	/* remove this frame */
	frame->prev->next = frame->next;
	frame->next->prev = frame->prev;

	if(frame->next == frame)
	  jb->frames = NULL;
	else
	  jb->frames = frame->next;


	/* insert onto "free" single-linked list */
	frame->next = jb->free;
	jb->free = frame;

	jb->info.frames_cur--;

	/* we return the frame pointer, even though it's on free list, 
	 * but caller must copy data */
	return frame;
    } 

    return NULL;
}

/* some diagnostics */
static void jb_dbginfo(jitterbuf *jb) {
}

static void jb_warninfo(jitterbuf *jb) {
    jb_warn("\njb info: fin=%ld fout=%ld flate=%ld flost=%ld fdrop=%ld fcur=%ld\n",
	    jb->info.frames_in, jb->info.frames_out, jb->info.frames_late, jb->info.frames_lost, jb->info.frames_dropped, jb->info.frames_cur);
	
    jb_warn("	jitter=%ld current=%ld target=%ld min=%ld sil=%d len=%d len/fcur=%ld\n",
	    jb->info.jitter, jb->info.current, jb->info.target, jb->info.min, jb->info.silence, jb->info.current - jb->info.min, 
	    jb->info.frames_cur ? (jb->info.current - jb->info.min)/jb->info.frames_cur : -8);
    if(jb->info.frames_in > 0) 
	jb_warn("jb info: Loss PCT = %ld%%, Late PCT = %ld%%\n",
	    jb->info.frames_lost * 100/(jb->info.frames_in + jb->info.frames_lost), 
	    jb->info.frames_late * 100/jb->info.frames_in);
	jb_warn("jb info: queue %d -> %d.  last_ts %d (queue len: %d) last_ms %d\n",
	    queue_next(jb), 
	    queue_last(jb),
	    jb->info.last_voice_ts, 
	    queue_last(jb) - queue_next(jb),
	    jb->info.last_voice_ms);
}

static void jb_chkqueue(jitterbuf *jb) {
    int i=0;
    jb_frame *p = jb->frames;

    if(!p) {
      return;
    }

    do {
	if(p->next == NULL)  {
	  jb_err("Queue is BROKEN at item [%d]", i);	
	}
	i++;
	p=p->next;
    } while (p->next != jb->frames);
}

static void jb_warnqueue(jitterbuf *jb) {
    int i=0;
    jb_frame *p = jb->frames;

    jb_warn("queue: ");

    if(!p) {
      jb_warn("EMPTY\n");
      return;
    }

    do {
	jb_warn("[%d]=%ld ", i++, p->ts);
	p=p->next;
    } while (p->next != jb->frames);

    jb_warn("\n");
}

static void jb_adjust(jitterbuf *jb, int now) {
#if 0
    long diff;
    long adjustment;

    jb->info.jitter = history_get(jb, NULL);
    
    diff = (jb->info.jitter) - jb->info.length;

    if(jb->info.silence) {
	if(diff > 0)
	{
	    /* just grow as we want, we'll only be expanding the silence a bit */
	    adjustment = diff;
	} else { /* we can shrink quickly here, as long as we have silence in the queue */
	    /* find out the gap (negative) between what we last sent, and the next frame in the queue
	     * it might not be voice, but then we'll adjust again later */
	    adjustment = (jb->info.last_ts ) - jb_next(jb); 

	    /* of course, clamp our adjustment at our goal */
	    if(adjustment < diff) adjustment = diff;
	}
    } else {
	if(diff < 0) {
	    /* shrink at about a 1 in 10 rate.  We'll drop 10% of frames, which is pretty unnoticable,
	     * but we get the latency down quickly */
	    adjustment = (jb->info.last_adjustment - now)/10;
	    if(adjustment < diff) adjustment = diff;
	} else {
	    /* adjust at a rate such that we get the full desired adjustment before the end of the buffer */
	    /* XXX: Is this a reasonable way to grow?? */
	    adjustment = diff * (now - jb->info.last_adjustment) / (jb->info.length + now - jb->info.last_adjustment);	
	}
    }

    jb->info.length += adjustment;
    jb->info.last_adjustment = now;
#endif
}


int jb_put(jitterbuf *jb, void *data, int type, long ms, long ts, long now) {
    long adj;

    jb_dbg("jb_put(%x,%x,%ld,%ld,%ld)\n", jb, data, ms, ts, now);

    jb->info.frames_in++;

    if(type == JB_TYPE_VOICE) {
      /* presently, I'm only adding VOICE frames to history and drift calculations; mostly because with the
       * IAX integrations, I'm sending retransmitted control frames with their awkward timestamps through */
      history_put(jb,ts,now);
    }

    queue_put(jb,data,type,ms,ts);

    jb_dbginfo(jb);
    return JB_OK;
}

/* this is the adjustment to be applied to the next outgoing frame timestamp */
static long jb_getadjustment(jitterbuf *jb) { 
  /* XXX fudge: try fudging by one voice frame */
  //return (jb->info.length - jb->info.drift + 30); 
  return 0;
}


static int _jb_get(jitterbuf *jb, jb_frame *frameout, long now) {
    jb_frame *frame;
    long diff;

    if((now - jb_next(jb)) > 2 * jb->info.last_voice_ms) jb_warn("SCHED: %ld", (now - jb_next(jb)));
    /* get jitter info */
    jb->info.jitter = history_get(jb, &jb->info.min);


    /* target */
    jb->info.target = jb->info.jitter + jb->info.min + 2 * jb->info.last_voice_ms; 
	//now - jb->info.last_voice_ts;

    diff = jb->info.target - jb->info.current;

//    jb_warn("diff = %d lms=%d last = %d now = %d\n", diff, 
//	jb->info.last_voice_ms, jb->info.last_adjustment, now);

    /* move up last_voice_ts; it is now the expected voice ts */
    jb->info.last_voice_ts += jb->info.last_voice_ms;

    /* let's work on non-silent case first */
    if(!jb->info.silence) { 
	  /* we want to grow */
      if( (diff > 0) && 
	  /* we haven't grown in a frames' length */
	  (((jb->info.last_adjustment + jb->info.last_voice_ms ) < now) || 
	   /* we need to grow more than the "length" we have left */
	   (diff > queue_last(jb)  - queue_next(jb)) ) ) {
	      jb->info.current += jb->info.last_voice_ms;
	      jb->info.last_adjustment = now;
	      jb_warn("G");
	      return JB_INTERP;
      }

      frame = queue_get(jb, jb->info.last_voice_ts - jb->info.current);

      /* not a voice frame; just return it. */
      if(frame && frame->type != JB_TYPE_VOICE) {
	/* rewind last_voice_ts, since this isn't voice */
	jb->info.last_voice_ts -= jb->info.last_voice_ms;

	if(frame->type == JB_TYPE_SILENCE) 
	  jb->info.silence = 1;

	*frameout = *frame;
	jb->info.frames_out++;
	jb_warn("o");
	return JB_OK;
      }


      /* voice frame is late */
      if(frame && frame->ts + jb->info.current < jb->info.last_voice_ts - jb->info.last_voice_ms ) {
	*frameout = *frame;
	/* rewind last_voice, since we're just dumping */
	jb->info.last_voice_ts -= jb->info.last_voice_ms;
	jb->info.frames_out++;
	jb->info.frames_late++;
	jb->info.frames_lost--;
	jb_warn("l");
	//jb_warn("\nlate: wanted=%ld, this=%ld, next=%ld\n", jb->info.last_voice_ts - jb->info.current, frame->ts, queue_next(jb));
	//jb_warninfo(jb);
	return JB_DROP;
      }

      /* keep track of frame sizes, to allow for variable sized-frames */
      if(frame && frame->ms > 0) {
	jb->info.last_voice_ms = frame->ms;
      }

      /* we want to shrink; shrink at 1 frame / 500ms */
      if(diff < -2 * jb->info.last_voice_ms && 
		((!frame && jb->info.last_adjustment + 80 < now) || 
		 (jb->info.last_adjustment + 500 < now))) {

	/* don't increment last_ts ?? */
	jb->info.last_voice_ts -= jb->info.last_voice_ms;
	jb->info.current -= jb->info.last_voice_ms;
	jb->info.last_adjustment = now;

	if(frame)  {
	  *frameout = *frame;
	  jb->info.frames_out++;
	  jb->info.frames_dropped++;
	  jb_warn("s");
	  return JB_DROP;
	} else {
	  jb_warn("S");
	  return JB_NOFRAME;
	}
      }

      /* lost frame */
      if(!frame) {
	  /* this is a bit of a hack for now, but if we're close to
	   * target, and we find a missing frame, it makes sense to
	   * grow, because the frame might just be a bit late;
	   * otherwise, we presently get into a pattern where we return
	   * INTERP for the lost frame, then it shows up next, and we
	   * throw it away because it's late */
	  /* I've recently only been able to replicate this using
	   * iaxclient talking to app_echo on asterisk.  In this case,
	   * my outgoing packets go through asterisk's (old)
	   * jitterbuffer, and then might get an unusual increasing delay 
	   * there if it decides to grow?? */
	  /* Update: that might have been a different bug, that has been fixed..
	   * But, this still seemed like a good idea, except that it ended up making a single actual
	   * lost frame get interpolated two or more times, when there was "room" to grow, so it might
	   * be a bit of a bad idea overall */
	  /*if(diff > -1 * jb->info.last_voice_ms) { 
	      jb->info.current += jb->info.last_voice_ms;
	      jb->info.last_adjustment = now;
	      jb_warn("g");
	      return JB_INTERP;
	  } */
	  jb->info.frames_lost++;
	  jb_warn("L");
	  return JB_INTERP;
      }

      /* normal case; return the frame, increment stuff */
      *frameout = *frame;
      jb->info.frames_out++;
      jb_warn("v");
      return JB_OK;
  } else {     
      /* TODO: after we get the non-silent case down, we'll make the
       * silent case -- basically, we'll just grow and shrink faster
       * here, plus handle last_voice_ts a bit differently */
      
      /* to disable silent special case altogether, just uncomment this: */
       /* jb->info.silence = 0; */

       frame = queue_get(jb, now - jb->info.current);
       if(!frame) {
	  return JB_NOFRAME;
       }
       if(frame && frame->type == JB_TYPE_VOICE) {
	  jb->info.silence = 0;
	  jb->info.last_voice_ts = frame->ts - jb->info.current;
	  jb->info.last_voice_ms = frame->ms;
	  *frameout = *frame;
	  return JB_OK;
       }
       /* normal case; in silent mode, got a non-voice frame */
       *frameout = *frame;
       return JB_OK;
  }
}

long jb_next(jitterbuf *jb) {
    if(jb->info.silence) {
      long next = queue_next(jb);
      if(next > 0) return next + jb->info.current;
      else return LONG_MAX;
    } else {
      return jb->info.last_voice_ts + jb->info.last_voice_ms;
    }
}

int jb_get(jitterbuf *jb, jb_frame *frameout, long now) {
    int ret = _jb_get(jb,frameout,now);
    static int lastts=0;
    int thists = ((ret == JB_OK) || (ret == JB_DROP)) ? frameout->ts : 0;
//    jb_warn("jb_get(%x,%x,%ld) = %d (%d)\n", jb, frameout, now, ret, thists);
    if(thists && thists < lastts) jb_warn("XXXX timestamp roll-back!!!\n");
    lastts = thists;
    return ret;
}

int jb_getall(jitterbuf *jb, jb_frame *frameout) {
    jb_frame *frame;
    frame = queue_get(jb, 0);

    if(!frame) {
      return JB_NOFRAME;
    }

    *frameout = *frame;
    return JB_OK;
}


int jb_getinfo(jitterbuf *jb, jb_info *stats) {

    jb->info.jitter = history_get(jb, NULL);

    *stats = jb->info;

  return JB_OK;
}

int jb_setinfo(jitterbuf *jb, jb_info *settings) {
  return JB_OK;
}


