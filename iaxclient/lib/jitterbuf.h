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

#ifndef _JITTERBUF_H_
#define _JITTERBUF_H_

#ifdef __cplusplus
extern "C" {
#endif

/* configuration constants */
#define JB_HISTORY_LONGSZ	10
#define JB_HISTORY_SHORTSZ	150
#define JB_HISTORY_SHORTTM	2500
#define JB_HISTORY_DROPPCT	2

/* return codes */
#define JB_OK		0
#define JB_EMPTY	1
#define JB_NOFRAME	2
#define JB_INTERP	3
#define JB_DROP		4

/* frame types */
#define JB_TYPE_CONTROL	0
#define JB_TYPE_VOICE	1
#define JB_TYPE_VIDEO	2  /* reserved */
#define JB_TYPE_SILENCE	3

typedef struct jb_info {
	/* statistics */
	long frames_in;  	/* number of frames input to the jitterbuffer.*/
	long frames_out;  	/* number of frames output from the jitterbuffer.*/
	long frames_late; 	/* number of frames which were too late, and dropped.*/
	long frames_lost; 	/* number of missing frames.*/
	long frames_dropped; 	/* number of frames dropped (shrinkage) */
	long frames_ooo; 	/* number of frames received out-of-order */
	long frames_cur; 	/* number of frames presently in jb, awaiting delivery.*/
	long jitter; 		/* jitter measured within current history interval*/
	long min;		/* minimum lateness within current history interval */
	long current; 		/* the present jitterbuffer adjustment */
	long target; 		/* the target jitterbuffer adjustment */
	long last_voice_ts;	/* the last ts that was read from the jb - in receiver's time */
	long last_voice_ms;	/* the duration of the last voice frame */
	long silence;		/* we are presently playing out silence */
	long last_adjustment;   /* the time of the last adjustment */
} jb_info;

typedef struct jb_frame {
	void *data;		/* the frame data */
	long ts;	/* the relative delivery time expected */
	long ms;	/* the time covered by this frame, in sec/8000 */
	int  type;	/* the type of frame */
	struct jb_frame *next, *prev;
} jb_frame;

typedef struct jitterbuf {
	jb_info info;

	/* history */
	long hist_longmax[JB_HISTORY_LONGSZ];	/* history buckets */
	long hist_longmin[JB_HISTORY_LONGSZ];	/* history buckets */
	long hist_short[JB_HISTORY_SHORTSZ];   /* short-term history */
	long hist_ts;				/* effective start time of short-term history */
	int  hist_shortcur;			/* current index into short-term history */

	jb_frame *frames; 		/* queued frames */
	jb_frame *free; 		/* free frames (avoid malloc?) */
} jitterbuf;


/* new jitterbuf */
jitterbuf *		jb_new();

/* destroy jitterbuf */
void			jb_destroy(jitterbuf *jb);

/* reset jitterbuf */
/* NOTE:  The jitterbuffer should be empty before you call this, otherwise
 * you will leak queued frames, and some internal structures */
int			jb_reset(jitterbuf *jb);

/* queue a frame data=frame data, timings (in ms): ms=length of frame (for voice), ts=ts (sender's time) 
 * now=now (in receiver's time)*/
int 			jb_put(jitterbuf *jb, void *data, int type, long ms, long ts, long now);

/* get a frame for time now (receiver's time)  return value is one of
 * JB_OK:  You've got frame!
 * JB_DROP: Here's an audio frame you should just drop.  Ask me again for this time..
 * JB_NOFRAME: There's no frame scheduled for this time.
 * JB_INTERP: Please interpolate an audio frame for this time (either we need to grow, or there was a lost frame 
 * JB_EMPTY: The jb is empty.
 */
int			jb_get(jitterbuf *jb, jb_frame *frame, long now);

/* when is the next frame due out, in receiver's time (0=EMPTY) 
 * This value may change as frames are added (esp non-audio frames) */
long			jb_next(jitterbuf *jb);

/* get jitterbuf info: only "statistics" may be valid */
int			jb_getinfo(jitterbuf *jb, jb_info *stats);

/* set jitterbuf info: only "settings" may be honored */
int			jb_setinfo(jitterbuf *jb, jb_info *settings);


#ifdef __cplusplus
}
#endif


#endif
